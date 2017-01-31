/*
 * CS 1550: Graphics library skeleton code for Qemu VM
 * WARNING: This code is the minimal implementation of the project 1.
 *          It is not intended to serve as a reference implementation.
 *          Following project guidelines, a complete implementation
 *          for this project will contain ~300 lines or more.
 * (c) Mohammad H. Mofrad, 2017
 */

#include "library.h"

int fid;
color_t *address;

/* Screen size vars */
struct fb_var_screeninfo screen_info;
struct fb_fix_screeninfo fixed_info;
struct termios terminal_info;
size_t size;

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
		printf("%d", size);

		address = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0);
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

void draw_line(color_t c)
{
    /* Print a single line */
    color_t off_p = 0;
    for(off_p =0; off_p < size; off_p++)
    {
        *(address + off_p) = RMASK(c) | GMASK(c) | BMASK(c);
        /* 
          printf("Address(0x%08x), Color(0x%04x) B(0x%04x), G(0x%04x), R(0x%04x) \n",
                (address + off_p), *(address + off_p), BMASK(c), GMASK(c), RMASK(c));
        */      
    }
}

void sleep_s(unsigned seconds)
{
   sleep(seconds); /* This is in seconds and not milliseconds */
}

void clear_screen() 
{
    write(STDOUT_FILENO, "\033[2J", 8);  /* Clear Screen */
}

void exit_graphics() 
{
	/* Turn off ICANON and ECHO bits */
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
