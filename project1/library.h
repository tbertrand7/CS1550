/*
 * Tom Bertrand
 * 2/2/2017
 * COE 1550
 * Misurda T, Th 11:00
 * Project 1
 * Graphics Library header file
 */


#ifndef _GRAPHICS_INCLUDED_H
#define _GRAPHICS_INCLUDED_H

#include <linux/fb.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>

typedef unsigned short color_t;    // |15 11|10  5|4  0|
                                   // |red  |green|blue|
                                   //   5   + 6   + 5  =  16 bits
// Macros to extract RGB colors 
#define BMASK(c) (c & 0x001F) // Blue mask
#define GMASK(c) (c & 0x07E0) // Green mask
#define RMASK(c) (c & 0xF800) // Red mask

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void draw_char(int x, int y, char c, color_t color);

#endif
