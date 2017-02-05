/*
 * Tom Bertrand
 * 2/2/2017
 * COE 1550
 * Misurda T, Th 11:00
 * Project 1
 * Driver program demonstrating graphics library functions
 */

#include "library.h"

void display_menu();
void rectangle();
void text();

int main(int argc, char** argv)
{
	char key = 'x';

	init_graphics();
	display_menu();

	while(key != '3')
	{
		key = getkey();
		if (key == '1')
		{
			rectangle();
			display_menu();
		}
		else if (key == '2')
		{
			text();
			display_menu();
		}
	}

	exit_graphics();

	return 0;
}

/* Display main menu */
void display_menu()
{
	clear_screen();
	draw_text(0, 0, "Graphics Library Driver Program", 0xFFFF);
	draw_text(10, 18, "Press 1 to draw a rectangle", 0xFFFF);
	draw_text(10, 32, "Press 2 to draw text", 0xFFFF);
	draw_text(10, 46, "Press 3 to exit", 0xFFFF);
}

/* Draws rectangle and allows user to move it around with WASD */
void rectangle()
{
	char key = 'x';
	int x = 200, y = 200;

	clear_screen();
	draw_text(10, 0, "Use W,A,S,D to move rectangle around", 0xFFFF);
	draw_text(10, 14, "Press q to return to menu", 0xFFFF);

	draw_rect(x, y, 150, 50, 0xFF00);

	while(key != 'q')
	{
		key = getkey();
		if(key == 'w')
		{
			draw_rect(x, y, 150, 50, 0);
			sleep_ms(20);
			y-=10;
			draw_rect(x, y, 150, 50, 0xFF00);
			sleep_ms(20);
		}
		else if(key == 's')
		{
			draw_rect(x, y, 150, 50, 0);
			sleep_ms(20);
			y+=10;
			draw_rect(x, y, 150, 50, 0xFF00);
			sleep_ms(20);
		}
		else if(key == 'a')
		{
			draw_rect(x, y, 150, 50, 0);
			sleep_ms(20);
			x-=10;
			draw_rect(x, y, 150, 50, 0xFF00);
			sleep_ms(20);
		}
		else if(key == 'd')
		{
			draw_rect(x, y, 150, 50, 0);
			sleep_ms(20);
			x+=10;
			draw_rect(x, y, 150, 50, 0xFF00);
			sleep_ms(20);
		}
	}
}

/* Allows user to enter a string and see it displayed on the screen */
void text()
{
	char key = 'x';
	char string[50];
	int i=0;
	clear_screen();
	draw_text(10, 0, "Type text and press enter to see it displayed on the screen", 0xFFFF);
	draw_text(10, 14, "(Max 50 characters)", 0xFFFF);
	draw_text(10, 28, "When finished viewing text, press q to return to menu", 0xFFFF);

	/* Get string text until enter is pressed or 50 characters are entered */
	while(key != '\n' || i == 50)
	{
		key = getkey();
		if (key != 0)
		{
			string[i] = key;
			i++;
		}
	}

	sleep_ms(20);
	draw_text(20, 200, &string[0], 0xF000);

	while(key != 'q')
	{
		key = getkey();
	}
}