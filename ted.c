/*
 * Terminal Text Editor
 * Copyright (C) 2022 Johnny Richard
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1F)

typedef struct editor_config {
  int screen_rows;
  int screen_cols;
  struct termios orig_termios;
} editor_config_t;

editor_config_t E;


void editor_init(editor_config_t *ec);
char editor_read_key();
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
void editor_process_key();
void editor_draw_rows(editor_config_t *ec);
void editor_clear_screen();
void editor_refresh_screen(editor_config_t *ec);
void disable_raw_mode();
void enable_raw_mode();
void die(const char *s);

int
main()
{
  enable_raw_mode();

  editor_init(&E);

  while (true) {
    editor_refresh_screen(&E);
    editor_process_key();
  }

  return EXIT_SUCCESS;
}

void
editor_init(editor_config_t *ec)
{
  if (get_window_size(&ec->screen_rows, &ec->screen_rows) == -1) {
    die("get_window_size");
  }
}

char
editor_read_key()
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }
  return c;
}

int
get_cursor_position(int *rows, int *cols)
{
  char buf[32];
  uint32_t i = 0;

  if (write(STDIN_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if (buf[i] == 'R') {
      break;
    }
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
    return -1;
  }

  return 0;
}

int
get_window_size(int *rows, int *cols)
{
  struct winsize ws;

  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return -1;
    }
    return get_cursor_position(rows, cols);
  }

  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
}

void
editor_process_key()
{
  char c = editor_read_key();

  switch (c) {
    case CTRL_KEY('q'):
      editor_clear_screen();
      exit(EXIT_SUCCESS);
      break;
  }
}

void
editor_clear_screen()
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void
editor_draw_rows(editor_config_t *ec)
{
  for (int y = 0; y < ec->screen_rows; ++y) {
    write(STDOUT_FILENO, "~", 1);

    if (y < ec->screen_rows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void
editor_refresh_screen(editor_config_t *ec)
{
  editor_clear_screen();
  editor_draw_rows(ec);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void
disable_raw_mode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void
enable_raw_mode()
{
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }

  atexit(disable_raw_mode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag &= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

void
die(const char *s)
{
  editor_clear_screen();
  perror(s);
  exit(EXIT_FAILURE);
}
