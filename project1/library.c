/*
 * CS 1550: Graphics library skeleton code for Qemu VM
 * WARNING: This code is the minimal implementation of the project 1.
 *          It is not intended to serve as a reference implementation.
 *          Following project guidelines, a complete implementation
 *          for this project will contain ~300 lines or more.
 * (c) Mohammad H. Mofrad, 2017
 */

#include "library.h"
#include "iso_font.h"

int fid;
color_t *address;

/* Screen size vars */
struct fb_var_screeninfo screen_info;
struct fb_fix_screeninfo fixed_info;
struct termios terminal_info;
size_t size;

/* Initialize the graphics library  */
void init_graphics()
{
	/* Open fb file descriptor */
	fid = open("/dev/fb0", O_RDWR);
	if(fid == -1)
    {
        perror("Error opening /dev/fb0");
        exit(1);
    }

    /* Get screen size and bits per pixel using iotcl */
    if (!ioctl(fid, FBIOGET_VSCREENINFO, &screen_info) &&
    		!ioctl(fid, FBIOGET_FSCREENINFO, &fixed_info))
    {
		size = screen_info.yres_virtual * fixed_info.line_length;

		address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0);
		if(address == (void *) -1)
		{
			perror("Error mapping memory");
			exit(1);
		}

		/* Turn off ICANON and ECHO bits */
		ioctl(STDIN_FILENO, TCGETS, &terminal_info);
		terminal_info.c_lflag &= ~ICANON;
		terminal_info.c_lflag &= ~ECHO;
		ioctl(STDIN_FILENO, TCSETS, &terminal_info);
    }
    else
    {
    	perror("Error retrieving screen size");
    	exit(1);
    }
}

/* Perform cleanup actions on program exit */
void exit_graphics() 
{
	/* Turn on ICANON and ECHO bits */
	ioctl(STDIN_FILENO, TCGETS, &terminal_info);
	terminal_info.c_lflag |= ICANON;
	terminal_info.c_lflag |= ECHO;
	ioctl(STDIN_FILENO, TCSETS, &terminal_info);

	/* Remove the memory mapping */
    if(munmap(address, size) == -1)
    {
        perror("Error unmapping memory");
        exit(1);
    }

    /* Close the fb file descriptor */
    if(!close(fid))
    {
        exit(0);
    }
    else
    {
        perror("Error closing /dev/fb0");
        exit(1);
    }
}

/* Clears screen using an ANSI escape code */
void clear_screen()
{
	write(STDOUT_FILENO, "\033[2J", 4);  /* Clear Screen */
}

/* Read in character input from user without blocking */
char getkey()
{
	char input = 0;
	int retval;
	struct timeval tv;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	retval = select(1, &rfds, NULL, NULL, &tv);

	if (retval == -1)
	{
		perror("Error with select()");
		return(1);
	}
	else if (retval)
	{
		read(STDIN_FILENO, &input, sizeof(input));
	}

	return input;
}

/* Sleep for ms milliseconds using nanosleep */
void sleep_ms(long ms)
{
	struct timespec t;

	/* Calc seconds and nanoseconds based on ms */
	if (ms > 999)
	{
		t.tv_sec = (int)(ms/1000);
		t.tv_nsec = (ms - ((long)t.tv_sec*1000)) * 1000000;
	}
	else
	{
		t.tv_sec = 0;
		t.tv_nsec = ms * 1000000;
	}

	nanosleep(&t,NULL); //Call nanosleep
}

/* Draw a pixel of color at position x,y */
void draw_pixel(int x, int y, color_t color)
{
	if ((x < screen_info.xres_virtual && x >= 0) &&
			(y < screen_info.yres_virtual && y >= 0)) //Check if pixel is in screen range
	{
		int location = (y*screen_info.xres_virtual)+x; //Calculate offset using y and x
		*(address + location) = RMASK(color) | GMASK(color) | BMASK(color); //Set pixel color
	}
}

/* Draws rectangle starting at x1,y1 with specified width, height, color */
void draw_rect(int x1, int y1, int width, int height, color_t c)
{
	color_t y_pos=0, x_pos=0;

	for (y_pos=y1; y_pos < (y1+height); y_pos++) //Loop through each row
	{
		for (x_pos=x1; x_pos < (x1+width); x_pos++) //Draw line for each row
		{
			draw_pixel(x_pos,y_pos,c);
		}
	}
}

void draw_text(int x, int y, const char *text, color_t c)
{

}

void draw_char(int x, int y, char c, color_t c)
{
	
}
