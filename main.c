#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <time.h>
#include <threads.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
static Display* display;
static Window window;
static XVisualInfo vinfo;
static Colormap colormap;
static XftDraw* xft_drawable;
static char time_string[256] = {0};
static int width = 100;
static int height = 50;

void get_primary_monitor_offset(Display *display, int screen, int *x_offset, int *y_offset) {
    XRRScreenResources *resources = XRRGetScreenResources(display, RootWindow(display, screen));
    if (resources == NULL) {
        fprintf(stderr, "Error: Failed to get screen resources\n");
        return;
    }
    RROutput primary_output = XRRGetOutputPrimary(display, RootWindow(display, screen));
    if (primary_output == None) {
        fprintf(stderr, "Error: Failed to get primary output\n");
        XRRFreeScreenResources(resources);
        return;
    }
    XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, resources, XRRGetOutputInfo(display, resources, primary_output)->crtc);
    if (crtc_info == NULL) {
        fprintf(stderr, "Error: Failed to get crtc info\n");
        XRRFreeScreenResources(resources);
        return;
    }
    *x_offset = crtc_info->x;
    *y_offset = crtc_info->y;
    XRRFreeCrtcInfo(crtc_info);
    XRRFreeScreenResources(resources);
}

void get_monitor_offset(Display *display, int screen, int monitor, int *x_offset, int *y_offset) {
    XRRScreenResources *resources = XRRGetScreenResources(display, RootWindow(display, screen));
    if (resources == NULL) {
        fprintf(stderr, "Error: Failed to get screen resources\n");
        return;
    }
    if (monitor < 0 || monitor >= resources->noutput) {
        fprintf(stderr, "Error: Invalid monitor number\n");
        XRRFreeScreenResources(resources);
        return;
    }
    XRROutputInfo *output_info = XRRGetOutputInfo(display, resources, resources->outputs[monitor]);
    if (output_info == NULL) {
        fprintf(stderr, "Error: Failed to get output info\n");
        XRRFreeScreenResources(resources);
        return;
    }
    if (output_info->crtc == 0) {
        fprintf(stderr, "Error: Monitor is not active\n");
        XRRFreeOutputInfo(output_info);
        XRRFreeScreenResources(resources);
        return;
    }
    XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, resources, output_info->crtc);
    if (crtc_info == NULL) {
        fprintf(stderr, "Error: Failed to get crtc info\n");
        XRRFreeOutputInfo(output_info);
        XRRFreeScreenResources(resources);
        return;
    }
    *x_offset = crtc_info->x;
    *y_offset = crtc_info->y;
    XRRFreeCrtcInfo(crtc_info);
    XRRFreeOutputInfo(output_info);
    XRRFreeScreenResources(resources);
}

static void draw_text_center(XftColor font_color,XftColor stroke_color, XftFont* font,XftFont* stroke, const char* text){
    {
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, stroke, (XftChar8*) text, strlen(text), &extents);
        int x = (width - extents.width) / 2;
        int y = (height + font->ascent) / 2;

        XftDrawStringUtf8(xft_drawable, &stroke_color, stroke, x, y, (XftChar8*) text, strlen(text));
    }
    {
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font, (XftChar8*) text, strlen(text), &extents);
        int x = (width - extents.width) / 2;
        int y = (height + font->ascent) / 2;

        XftDrawStringUtf8(xft_drawable, &font_color, font, x, y, (XftChar8*) text, strlen(text));
    }
}

void draw_text_with_stroke(XftFont *font, const char *text, XftColor text_color, XftColor stroke_color, int stroke_width) {
    // Set up XftDraw for stroke
    XftDrawSetClip(xft_drawable, None);

        int x;
    int y;
    {
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font, (XftChar8*) text, strlen(text), &extents);
        x = (width - extents.width) / 2;
        y = (height) / 2;
    }

    // Draw the stroke
    for (int i = -stroke_width; i <= stroke_width; i++) {
        for (int j = -stroke_width; j <= stroke_width; j++) {
            if (i == 0 && j == 0) continue;
            XftDrawStringUtf8(xft_drawable, &stroke_color, font, x + i, y + j, (const FcChar8 *)text, strlen(text));
        }
    }

    // Draw the text
    XftDrawStringUtf8(xft_drawable, &text_color, font, x, y, (const FcChar8 *)text, strlen(text));
}


_Noreturn static void clock_thread(){
    while (1){
        time_t temp;
        struct tm *timeptr;

        temp = time(NULL);
        timeptr = localtime(&temp);

        strftime(time_string,sizeof(time_string),"%H:%M", timeptr);

        XEvent event;
        event.type = Expose;

        XSendEvent(display,window,False,ExposureMask,&event);
        XFlush(display);

        sleep(1);
    }
}

int main() {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        exit(1);
    }
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    int x_offset, y_offset;
    get_monitor_offset(display,screen,0,&x_offset, &y_offset);

    // create a completely transparent colormap
    XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo);
    colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    XSetWindowAttributes attributes;
    attributes.colormap = colormap;
    attributes.border_pixel = 0;
    attributes.background_pixel = 0;

    // create a window without top level decorations
    window = XCreateWindow(display, root, x_offset, y_offset, width, height, 0, vinfo.depth, InputOutput, vinfo.visual,
                           CWColormap | CWBorderPixel | CWBackPixel, &attributes);

    XSetWindowAttributes windowAttributes;
    windowAttributes.override_redirect = True; // this removes decoration
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &windowAttributes);

    XSelectInput(display, window, ExposureMask | EnterWindowMask | LeaveWindowMask);
    XMapWindow(display, window);

    // set the window to be completely transparent
    XRenderColor color;
    memset(&color, 0, sizeof(color));
    color.alpha = 0;
    Picture picture = XRenderCreatePicture(display, window, XRenderFindVisualFormat(display, vinfo.visual), 0, NULL);
    XRenderPictureAttributes pa;
    pa.subwindow_mode = IncludeInferiors;
    XRenderChangePicture(display, picture, CPSubwindowMode, &pa);
    XRenderFillRectangle(display, PictOpSrc, picture, &color, 0, 0, width, height);

    // set the window to be always on top
    Atom atom = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(display, window, XInternAtom(display, "_NET_WM_STATE", False), XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&atom, 1);


    XftFont *font;
    XftColor font_color;
    XftColor stroke_color;

    {
        FcPattern* pattern = XftNameParse("mono");
        XftPatternAddDouble(pattern, XFT_SIZE, (double) 20);
        FcResult result;
        pattern = XftFontMatch(display, screen, pattern, &result);
        font = XftFontOpenPattern(display, pattern);
    }


    XftColorAllocName(display, vinfo.visual, colormap, "green", &font_color);
    XftColorAllocName(display, vinfo.visual, colormap, "black", &stroke_color);

    xft_drawable = XftDrawCreate(display, window, vinfo.visual, colormap);


    pthread_t thread;

    thrd_create(&thread,clock_thread,NULL);

    XRectangle rect;
    XserverRegion region = XFixesCreateRegion(display, &rect, 1);
    XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(display, region);

    // enter the event loop
    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            // redraw the window if necessary
            XRenderFillRectangle(display, PictOpSrc, picture, &color, 0, 0, width, height);
            {
                draw_text_with_stroke(font, time_string, font_color, stroke_color, 2);
            }

        }
    }

    XRenderFreePicture(display, picture);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}

#pragma clang diagnostic pop