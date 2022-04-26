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
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "string_builder.h"

#define CTRL_KEY(k) ((k) & 0x1F)
#define TED_VERSION "0.0.1"

enum editor_key {
  ARROW_LEFT  = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

typedef struct editor_config {
  int cx, cy;
  int screen_rows;
  int screen_cols;
  struct termios orig_termios;
} editor_config_t;

editor_config_t E;


void editor_init(editor_config_t *ec);
int editor_read_key();
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
void editor_move_cursor(editor_config_t *ec, int key);
void editor_process_key(editor_config_t *ec);
void editor_draw_rows(editor_config_t *ec, string_builder_t *sb);
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
    editor_process_key(&E);
  }

  return EXIT_SUCCESS;
}

void
editor_init(editor_config_t *ec)
{
  ec->cx = 0;
  ec->cy = 0;
  if (get_window_size(&ec->screen_rows, &ec->screen_cols) == -1) {
    die("get_window_size");
  }
}

int
editor_read_key()
{
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
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
editor_move_cursor(editor_config_t *ec, int key)
{
  switch (key) {
    case ARROW_LEFT:
      if (ec->cx != 0) {
        ec->cx--;
      }
      break;
    case ARROW_RIGHT:
      if (ec->cx != ec->screen_cols - 1) {
        ec->cx++;
      }
      break;
    case ARROW_UP:
      if (ec->cy != 0) {
        ec->cy--;
      }
      break;
    case ARROW_DOWN:
      if (ec->cy != ec->screen_rows - 1) {
        ec->cy++;
      }
      break;
  }
}

void
editor_process_key(editor_config_t *ec)
{
  int c = editor_read_key();

  switch (c) {
    case CTRL_KEY('q'):
      editor_clear_screen();
      exit(EXIT_SUCCESS);
      break;
    case HOME_KEY:
      ec->cx = 0;
      break;
    case END_KEY:
      ec->cx = ec->screen_cols - 1;
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = ec->screen_rows;
        while (times--) {
          editor_move_cursor(ec, c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editor_move_cursor(ec, c);
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
editor_draw_rows(editor_config_t *ec, string_builder_t *sb)
{
  for (int y = 0; y < ec->screen_rows; ++y) {
    if (y == ec->screen_rows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome), "Ted editor --- version %s", TED_VERSION);
      if (welcomelen > ec->screen_cols) {
        welcomelen = ec->screen_cols;
      }

      int padding = (ec->screen_cols - welcomelen) / 2;
      if (padding) {
        string_builder_append(sb, "~", 1);
        padding--;
      }

      while (padding--) {
        string_builder_append(sb, " ", 1);
      }

      string_builder_append(sb, welcome, welcomelen);
    } else {
      string_builder_append(sb, "~", 1);
    }

    string_builder_append(sb, "\x1b[K", 3);
    if (y < ec->screen_rows - 1) {
      string_builder_append(sb, "\r\n", 2);
    }
  }
}

void
editor_refresh_screen(editor_config_t *ec)
{
  string_builder_t sb = STRING_BUILDER_INIT;

  string_builder_append(&sb, "\x1b[?25l", 6);
  string_builder_append(&sb, "\x1b[H", 3);

  editor_draw_rows(ec, &sb);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", ec->cy + 1, ec->cx + 1);
  string_builder_append(&sb, buf, strlen(buf));

  string_builder_append(&sb, "\x1b[?25h", 6);

  write(STDOUT_FILENO, sb.data, sb.len);
  string_builder_destroy(&sb);
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
