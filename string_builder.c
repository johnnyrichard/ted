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
#include "string_builder.h"

#include <string.h>
#include <stdlib.h>

void
string_builder_append(string_builder_t *sb, const char *s, int len)
{
  char *snew = realloc(sb->data, sb->len + len);

  if (snew == NULL) {
    return;
  }

  memcpy(&snew[sb->len], s, len);
  sb->data = snew;
  sb->len += len;
}

void
string_builder_destroy(string_builder_t *sb)
{
  free(sb->data);
}

