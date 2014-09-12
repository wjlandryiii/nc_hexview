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
#define ADDRESS_COLUMN 0
#define HEX_COLUMN 12
#define ASCII_COLUMN 63

#define I_COL(x) ((x) % NBYTES_PER_LINE)
#define I_ROW(x) ((x) / NBYTES_PER_LINE)

struct file_contents {
	int fd;
	size_t size;
	uint8_t *bytes;
	size_t curs_offset;
	int window_offset;
};

struct file_contents contents;

int load_file(char *filename);
void unload_file(void);
void draw_window(void);
int chloop(void);

int main(int argc, char *argv[]){
	if(argc < 2){
		goto usage;
	} else if(load_file(argv[1])){
		printf("error opening file: %s\n", argv[1]);
		goto fail;
	} else if(initscr() == NULL){
		printf("err on initscr()\n");
		goto fail_file;
	} else {
		refresh();
		keypad(stdscr, TRUE);
		noecho();
		curs_set(0);
		draw_window();
		chloop();	
		endwin();
		unload_file();
		return 0;
	}

fail_file:
	unload_file();
fail:
	return -1;
usage:
	printf("%s: [file name]\n", argv[0]);
	return 0;
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


void draw_line(int row, size_t line_start){
	int i;
	int nbytes;
	uint8_t b;

	move(row, 0);
	printw("%08X", line_start);

	nbytes = contents.size - line_start;
	if(nbytes > NBYTES_PER_LINE){
		nbytes = NBYTES_PER_LINE;
	}

	for(i = 0; i < nbytes; i++){
		if(contents.curs_offset == line_start + i){
			attron(A_REVERSE);
		}
		b = contents.bytes[line_start + i];
		mvprintw(row, HEX_COLUMN + i * 3, "%02X", b);
		mvaddch(row, ASCII_COLUMN + i, isprint(b) ? b : '.');
		if(contents.curs_offset == line_start + i){
			attroff(A_REVERSE);
		}
	}
}

void draw_status_line(int row){
	mvprintw(row, 0, "Offset: 0x%08X", contents.curs_offset);
}

void scroll_screen_to_cursor_if_necessary(int nrows){
	int start_row = I_ROW(contents.window_offset);
	int end_row = start_row + nrows;
	int curs_row = I_ROW(contents.curs_offset);
	int new_offset = contents.window_offset;

	if(curs_row < start_row){
		new_offset = curs_row * NBYTES_PER_LINE;
	} else if(end_row <= curs_row){
		new_offset = (curs_row - nrows + 1) * NBYTES_PER_LINE;
	}
	contents.window_offset = new_offset;
}

void draw_window(void){
	int row;
	int line_offset;
	int nrows;
	int status_row;
	int nhex_rows;
	int hex_first_row;
	int hex_last_row;

	nrows = getmaxy(stdscr);
	hex_first_row = 0;
	hex_last_row = nrows - 1;
	nhex_rows = hex_last_row - hex_first_row;
	status_row = nrows - 1;

	scroll_screen_to_cursor_if_necessary(nhex_rows);

	clear();
	line_offset = contents.window_offset;
	for(row = hex_first_row; row < hex_last_row; row++){
		draw_line(row, line_offset);
		line_offset += NBYTES_PER_LINE;
	}
	draw_status_line(status_row);
	refresh();
};


void move_cursor_up(){
	if(contents.curs_offset / NBYTES_PER_LINE == 0){
		beep();
	} else {
		contents.curs_offset -= NBYTES_PER_LINE;
		draw_window();
	}
}


void move_cursor_down(){
	int cur_row = I_ROW(contents.curs_offset);
	int last_row = I_ROW(contents.size - 1);

	if(cur_row >= last_row){
		beep();
	} else if(cur_row + 1 > last_row){
		beep();
	} else if(contents.curs_offset + NBYTES_PER_LINE >= contents.size){
		beep();
	} else {
		contents.curs_offset += NBYTES_PER_LINE;
		draw_window();
	}
}


void move_cursor_left(){
	if(I_COL(contents.curs_offset) == 0){
		beep();
	} else {
		contents.curs_offset -= 1;
		draw_window();
	}
}


void move_cursor_right(){
	if(contents.curs_offset == contents.size - 1){
		beep();
	} else if(I_COL(contents.curs_offset) == NBYTES_PER_LINE - 1){
		beep();
	} else {
		contents.curs_offset += 1;
		draw_window();
	}
}


void scroll_up(void){
	if(I_ROW(contents.curs_offset) == 0){
		beep();
	} else {
		if(contents.curs_offset < NBYTES_PER_LINE * 16){
			contents.curs_offset = I_COL(contents.curs_offset);
		} else {
			contents.curs_offset -= NBYTES_PER_LINE * 16;
		}
		draw_window();
	}
}


void scroll_down(void){
	int cur_row = I_ROW(contents.curs_offset);
	int last_row = I_ROW(contents.size - 1);

	if(cur_row >= last_row){
		beep();
	} else {
		if(cur_row + 16 >= last_row){
			contents.curs_offset = contents.size - 1;
		} else {
			contents.curs_offset += NBYTES_PER_LINE * 16;
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
