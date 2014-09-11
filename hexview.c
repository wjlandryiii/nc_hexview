/*
 * Copyright 2014 Joseph Landry
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <ncurses.h>
#include <inttypes.h>

struct file_contents {
	size_t start;
	size_t end;
	uint8_t *bytes;
};

struct file_contents contents;

int load_file(void){
	FILE *f;
	size_t size;

	if((f = fopen("tests/shellcode.bin", "rb")) == NULL){
		printf("couldn't open file\n");
		exit(-1);
	}
	if(fseek(f, 0, SEEK_END)){
		printf("couldn't fseek()\n");
		exit(-1);
	}
	size = ftell(f);
	if(fseek(f, 0, SEEK_SET)){
		printf("Couldn't fseek()\n");
		exit(-1);
	}
	if(ftell(f) != 0){
		printf("diddn't seek to begining of file\n");
		exit(-1);
	}

	contents.start = 0;
	contents.end = size;
	if((contents.bytes = malloc(size)) == NULL){
		printf("couldn't malloc()\n");
		exit(-1);
	}
	if(fread(contents.bytes, size, 1, f) != 1){
		printf("couldn't fread()\n");
		exit(-1);
	}
	fclose(f);
	return 0;
}


int cur_i = 0;
int file_offset = 0;

void draw_line(int row, size_t foffset){
	int i;
	int end;
	int ch;

	move(row, 0);
	printw("%08X", foffset);

	end = contents.end - foffset;
	if(end > 16){
		end = 16;
	}

	for(i = 0; i < end; i++){
		if(foffset + i == cur_i){
			attron(A_REVERSE);
		}
		ch = contents.bytes[foffset + i];
		move(row, i*3 + 12);
		printw("%02X", ch);
		mvaddch(row, 63 + i, isprint(ch) ? ch : '.');
		if(foffset + i == cur_i){
			attroff(A_REVERSE);
		}
	}
}

void draw_window(void){
	int row;
	int foffset;
	int nrows;
	int ncolumns;
	int c_row;
	int first_row;
	int last_row;

	nrows = getmaxy(stdscr);
	ncolumns = getmaxx(stdscr);

	first_row = file_offset / 16;
	c_row = cur_i / 16;
	last_row = first_row + nrows;

	if(c_row < first_row){
		first_row = c_row;
		file_offset = first_row * 16;
		last_row = first_row + nrows;
	} else if(c_row > last_row - 1){
		last_row = c_row;
		first_row = last_row - nrows + 1;
		file_offset = first_row * 16;
	}

	clear();
	for(row = 0; row < nrows; row++){
		foffset = file_offset + row * 16;
		draw_line(row, foffset);
	}
	refresh();
};


void move_cursor_up(){
	if(cur_i / 16 == 0){
		beep();
	} else {
		cur_i -= 16;
		draw_window();
	}
}

void move_cursor_down(){
	if(cur_i / 16 == contents.end / 16){
		beep();
	} else {
		if(cur_i + 16 >= contents.end){
			cur_i = contents.end - 1;
		} else {
			cur_i += 16;
		}
		draw_window();
	}
}

void move_cursor_left(){
	if(cur_i % 16 == 0){
		beep();
	} else {
		cur_i -= 1;
		draw_window();
	}
}

void move_cursor_right(){
	if(cur_i == contents.end - 1){
		beep();
	} else if(cur_i % 16 == 15){
		beep();
	} else {
		cur_i += 1;
		draw_window();
	}
}

void scroll_up(void){
	if(cur_i / 16 == 0){
		beep();
	} else {
		if(cur_i - 16 * 16 < 0){
			cur_i = cur_i % 16;
		} else {
			cur_i -= 16 * 16;
		}
		draw_window();
	}
}

void scroll_down(void){
	if(cur_i / 16 == (contents.end - 1)/16){
		beep();
	} else {
		if(cur_i / 16 + 16 > (contents.end - 1) / 16){
			cur_i = contents.end - 1;
		} else {
			cur_i += 16 * 16;
		}
		draw_window();
	}
}


int chloop(void);
int main(int argc, char *argv[]){
	WINDOW *w;
	
	load_file();

	if((w = initscr()) == NULL){
		printf("err on initscr()\n");
		exit(-1);
	}
	keypad(stdscr, TRUE);
	noecho();
	refresh();
	curs_set(0);

	draw_window();

	chloop();	
	if(endwin() == ERR){
		printf("err on endwin()\n");
		exit(-1);
	}
	return 0;
}

int chloop(void){
	int ch = 0;

	while((ch = getch()) != KEY_F(1) && ch != 'q'){
		switch(ch){
		case KEY_LEFT:
		case 'h':
			  move_cursor_left();
			  break;
		case KEY_RIGHT:
		case 'l':
			  move_cursor_right();
			  break;
		case KEY_UP:
		case 'k':
			  move_cursor_up();
			  break;
		case KEY_DOWN:
		case 'j':
			  move_cursor_down();
			  break;
		case 0x15: /* CTRL+U */
			  scroll_up();
			  break;
		case 0x4: /* CTRL+D */
			  scroll_down();
			  break;
		case KEY_RESIZE:
			  draw_window();
			  if((ch = getch()) != ERR){
				  printw("unexpected non error?");
			  }
			  break;
		default:
			move(0, 0);
			printw("unknown ch: __%08x__", ch);
		}
	}
	return 0;
}
