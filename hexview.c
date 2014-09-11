/*
 * Copyright 2014 Joseph Landry
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ncurses.h>
#include <inttypes.h>

#define NBYTES_PER_LINE 16

struct file_contents {
	int fd;
	size_t size;
	uint8_t *bytes;
};

struct file_contents contents;

int load_file(char *filename);
void unload_file(void);
void draw_window(void);
int chloop(void);

int main(int argc, char *argv[]){
	WINDOW *w;
	
	if(load_file("/bin/cat")){
		return -1;
	} else if((w = initscr()) == NULL){
		printf("err on initscr()\n");
		exit(-1);
	} else {
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
		unload_file();
		return 0;
	}

fail_file:
	unload_file();
fail:
	return -1;
}


int load_file(char *filename){
	int fd;
	off_t size;
	void *p;

	if((fd = open(filename, O_RDONLY)) == -1){
		printf("Couldn't open file: %s\n", filename);
		goto fail;
	} else if((size = lseek(fd, 0, SEEK_END)) == -1){
		printf("Couldn't seek to end of file\n");
		goto fail_file;
	} else if(lseek(fd, 0, SEEK_SET) == -1){
		printf("Couldn't seek to begining of file\n");
		goto fail_file;
	} else {
		p = mmap(NULL, size, PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);
		if(p == MAP_FAILED){
			printf("Couldn't map file to memory\n");
			goto fail_file;
		} else {
			contents.fd = fd;
			contents.size = size;
			contents.bytes = p;
			return 0;
		}
	}

fail_file:
	close(fd);
fail:
	return -1;
}

void unload_file(void){
	munmap(contents.bytes, contents.size);
	contents.bytes = NULL;
	close(contents.fd);
	contents.fd = -1;
}

int cur_i = 0;
int file_offset = 0;

void draw_line(int row, size_t foffset){
	int i;
	int end;
	int ch;

	move(row, 0);
	printw("%08X", foffset);

	end = contents.size - foffset;
	if(end > NBYTES_PER_LINE){
		end = NBYTES_PER_LINE;
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

	first_row = file_offset / NBYTES_PER_LINE;
	c_row = cur_i / NBYTES_PER_LINE;
	last_row = first_row + nrows;

	if(c_row < first_row){
		first_row = c_row;
		file_offset = first_row * NBYTES_PER_LINE;
		last_row = first_row + nrows;
	} else if(c_row > last_row - 1){
		last_row = c_row;
		first_row = last_row - nrows + 1;
		file_offset = first_row * NBYTES_PER_LINE;
	}

	clear();
	for(row = 0; row < nrows; row++){
		foffset = file_offset + row * NBYTES_PER_LINE;
		draw_line(row, foffset);
	}
	refresh();
};


void move_cursor_up(){
	if(cur_i / NBYTES_PER_LINE == 0){
		beep();
	} else {
		cur_i -= NBYTES_PER_LINE;
		draw_window();
	}
}


void move_cursor_down(){
	if(cur_i / NBYTES_PER_LINE == contents.size / NBYTES_PER_LINE){
		beep();
	} else {
		if(cur_i + NBYTES_PER_LINE >= contents.size){
			cur_i = contents.size - 1;
		} else {
			cur_i += NBYTES_PER_LINE;
		}
		draw_window();
	}
}


void move_cursor_left(){
	if(cur_i % NBYTES_PER_LINE == 0){
		beep();
	} else {
		cur_i -= 1;
		draw_window();
	}
}


void move_cursor_right(){
	if(cur_i == contents.size - 1){
		beep();
	} else if(cur_i % NBYTES_PER_LINE == NBYTES_PER_LINE - 1){
		beep();
	} else {
		cur_i += 1;
		draw_window();
	}
}


void scroll_up(void){
	if(cur_i / NBYTES_PER_LINE == 0){
		beep();
	} else {
		if(cur_i - NBYTES_PER_LINE * 16 < 0){
			cur_i = cur_i % NBYTES_PER_LINE;
		} else {
			cur_i -= NBYTES_PER_LINE * 16;
		}
		draw_window();
	}
}


void scroll_down(void){
	int cur_row = cur_i / NBYTES_PER_LINE;
	int last_row = (contents.size - 1) / NBYTES_PER_LINE;

	if(cur_row == last_row){
		beep();
	} else {
		if(cur_row + 16 > last_row){
			cur_i = contents.size - 1;
		} else {
			cur_i += NBYTES_PER_LINE * 16;
		}
		draw_window();
	}
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
