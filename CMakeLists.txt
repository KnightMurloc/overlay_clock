cmake_minimum_required(VERSION 3.16)
project(overlay_clock C)

set(CMAKE_C_STANDARD 11)

include_directories(/usr/include/freetype2)

add_executable(overlay_clock main.c)
target_link_libraries(overlay_clock X11 Xrender Xcomposite Xext fontconfig Xft Xrandr Xfixes)