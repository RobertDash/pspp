/* PSPP - a program for statistical analysis.
   Copyright (C) 2009, 2010, 2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef OUTPUT_RENDER_H
#define OUTPUT_RENDER_H 1

#include <stdbool.h>
#include <stddef.h>
#include "output/table-provider.h"

struct table;

enum render_line_style
  {
    RENDER_LINE_NONE,           /* No line. */
    RENDER_LINE_SINGLE,         /* Single line. */
    RENDER_LINE_DOUBLE,         /* Double line. */
    RENDER_N_LINES
  };

struct render_params
  {
    /* Measures CELL's width.  Stores in *MIN_WIDTH the minimum width required
       to avoid splitting a single word across multiple lines (normally, this
       is the width of the longest word in the cell) and in *MAX_WIDTH the
       minimum width required to avoid line breaks other than at new-lines. */
    void (*measure_cell_width) (void *aux, const struct table_cell *cell,
                                int *min_width, int *max_width);

    /* Returns the height required to render CELL given a width of WIDTH. */
    int (*measure_cell_height) (void *aux, const struct table_cell *cell,
                                int width);

    /* Draws a generalized intersection of lines in the rectangle whose
       top-left corner is (BB[TABLE_HORZ][0], BB[TABLE_VERT][0]) and whose
       bottom-right corner is (BB[TABLE_HORZ][1], BB[TABLE_VERT][1]).

       STYLES is interpreted this way:

       STYLES[TABLE_HORZ][0]: style of line from top of BB to its center.
       STYLES[TABLE_HORZ][1]: style of line from bottom of BB to its center.
       STYLES[TABLE_VERT][0]: style of line from left of BB to its center.
       STYLES[TABLE_VERT][1]: style of line from right of BB to its center. */
    void (*draw_line) (void *aux, int bb[TABLE_N_AXES][2],
                       enum render_line_style styles[TABLE_N_AXES][2]);

    /* Draws CELL within bounding box BB.  CLIP is the same as BB (the common
       case) or a subregion enclosed by BB.  In the latter case only the part
       of the cell that lies within CLIP should actually be drawn, although BB
       should used to determine the layout of the cell. */
    void (*draw_cell) (void *aux, const struct table_cell *cell,
                       int bb[TABLE_N_AXES][2], int clip[TABLE_N_AXES][2]);

    /* Auxiliary data passed to each of the above functions. */
    void *aux;

    /* Page size to try to fit the rendering into.  Some tables will, of
       course, overflow this size. */
    int size[TABLE_N_AXES];

    /* Nominal size of a character in the most common font:
       font_size[TABLE_HORZ]: Em width.
       font_size[TABLE_VERT]: Line spacing. */
    int font_size[TABLE_N_AXES];

    /* Width of different kinds of lines. */
    int line_widths[TABLE_N_AXES][RENDER_N_LINES];
  };

/* A "page" of content that is ready to be rendered.

   A page's size is not limited to the size passed in as part of render_params.
   Use render_break (see below) to break a too-big render_page into smaller
   render_pages that will fit in the available space. */
struct render_page *render_page_create (const struct render_params *,
                                        const struct table *);

struct render_page *render_page_ref (const struct render_page *);
void render_page_unref (struct render_page *);

int render_page_get_size (const struct render_page *, enum table_axis);
void render_page_draw (const struct render_page *);
void render_page_draw_region (const struct render_page *,
                              int x, int y, int w, int h);

/* An iterator for breaking render_pages into smaller chunks. */
struct render_break
  {
    struct render_page *page;   /* Page being broken up. */
    enum table_axis axis;       /* Axis along which 'page' is being broken. */
    int cell;                   /* Next cell. */
    int pixel;                  /* Pixel offset within 'cell' (usually 0). */
    int hw;                     /* Width of headers of 'page' along 'axis'. */
  };

void render_break_init (struct render_break *, struct render_page *,
                        enum table_axis);
void render_break_init_empty (struct render_break *);
void render_break_destroy (struct render_break *);

bool render_break_has_next (const struct render_break *);
int render_break_next_size (const struct render_break *);
struct render_page *render_break_next (struct render_break *, int size);

#endif /* output/render.h */
