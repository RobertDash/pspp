/* PSPPIRE - a graphical user interface for PSPP.
   Copyright (C) 2007 Free Software Foundation, Inc.

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

#include <config.h>
#include <gtk/gtk.h>
#include "window-manager.h"
#include "output-viewer.h"
#include "helper.h"
#include "about.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glade/glade.h>
#include <ctype.h>

struct output_viewer
{
  struct editor_window parent;
  GtkTextBuffer *buffer;  /* The buffer which contains the text */
  GtkWidget *textview ;
  FILE *fp;               /* The file it's viewing */
};


static void
cancel_urgency (GtkWindow *window,  gpointer data)
{
  gtk_window_set_urgency_hint (window, FALSE);
}


static struct output_viewer *the_output_viewer = NULL;


/* Callback for the "delete" action (clicking the x on the top right
   hand corner of the window) */
static gboolean
on_delete (GtkWidget *w, GdkEvent *event, gpointer user_data)
{
  struct output_viewer *ov = user_data;

  g_free (ov);

  the_output_viewer = NULL;

  unlink (OUTPUT_FILE_NAME);

  return FALSE;
}


/*
  Create a new output viewer
*/
struct output_viewer *
new_output_viewer (void)
{
  GladeXML *xml = XML_NEW ("output-viewer.glade");

  struct output_viewer *ov ;
  struct editor_window *e;

  connect_help (xml);

  ov = g_malloc (sizeof (*ov));

  e = (struct editor_window *)ov;


  e->window = GTK_WINDOW (get_widget_assert (xml, "output-viewer-window"));
  ov->textview = get_widget_assert (xml, "output-viewer-textview");
  ov->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (ov->textview));

  g_signal_connect (e->window,
		    "focus-in-event",
		    G_CALLBACK (cancel_urgency),
		    NULL);

  {
    /* Output uses ascii characters for tabular material.
       So we need a monospaced font otherwise it'll look silly */
    PangoFontDescription *font_desc =
      pango_font_description_from_string ("monospace");

    gtk_widget_modify_font (ov->textview, font_desc);
    pango_font_description_free (font_desc);
  }

  ov->fp = NULL;

  g_signal_connect (get_widget_assert (xml,"help_about"),
		    "activate",
		    G_CALLBACK (about_new),
		    e->window);

  g_signal_connect (get_widget_assert (xml,"help_reference"),
		    "activate",
		    G_CALLBACK (reference_manual),
		    NULL);

  g_signal_connect (get_widget_assert (xml,"windows_minimise-all"),
		    "activate",
		    G_CALLBACK (minimise_all_windows),
		    NULL);

  g_object_unref (xml);


  g_signal_connect (e->window, "delete-event",
		    G_CALLBACK (on_delete), ov);

  return ov;
}


void
reload_the_viewer (void)
{
  struct stat buf;

  /* If there is no output, then don't do anything */
  if (0 != stat (OUTPUT_FILE_NAME, &buf))
    return ;

  if ( NULL == the_output_viewer )
    {
      the_output_viewer =
	(struct output_viewer *) window_create (WINDOW_OUTPUT, NULL);
    }

  reload_viewer (the_output_viewer);
}


void
reload_viewer (struct output_viewer *ov)
{
  GtkTextIter end_iter;
  char line[OUTPUT_LINE_WIDTH];
  GtkTextMark *mark ;
  gboolean chars_inserted = FALSE;


  if ( ov->fp == NULL)
    {
      ov->fp = fopen (OUTPUT_FILE_NAME, "r");
      if ( ov->fp == NULL)
	{
	  g_print ("Cannot open %s\n", OUTPUT_FILE_NAME);
	  return;
	}
    }

  gtk_text_buffer_get_end_iter (ov->buffer, &end_iter);

  mark = gtk_text_buffer_create_mark (ov->buffer, NULL, &end_iter, TRUE);

  /* Read in the next lot of text */
  while (fgets (line, OUTPUT_LINE_WIDTH, ov->fp) != NULL)
    {
      chars_inserted = TRUE;
      gtk_text_buffer_insert (ov->buffer, &end_iter, line, -1);
    }

  /* Scroll to where the start of this lot of text begins */
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (ov->textview),
				mark,
				0.1, TRUE, 0.0, 0.0);


  if ( chars_inserted )
    gtk_window_set_urgency_hint ( ((struct editor_window *)ov)->window, TRUE);
}


