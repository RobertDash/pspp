/* PSPPIRE - a graphical user interface for PSPP.
   Copyright (C) 2007  Free Software Foundation

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <config.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include "t-test-one-sample.h"
#include "psppire-dict.h"
#include "psppire-var-store.h"
#include "helper.h"
#include <gtksheet/gtksheet.h>
#include "data-editor.h"
#include "psppire-dialog.h"
#include "dialog-common.h"
#include "dict-display.h"
#include "widget-io.h"

#include "t-test-options.h"
#include <language/syntax-string-source.h>
#include "syntax-editor.h"

#include <gettext.h>
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid


struct tt_one_sample_dialog
{
  PsppireDict *dict;
  GtkWidget *vars_treeview;
  GtkWidget *test_value_entry;
  struct tt_options_dialog *opt;
};


static gchar *
generate_syntax (const struct tt_one_sample_dialog *d)
{
  gchar *text;

  GString *str = g_string_new ("T-TEST ");

  g_string_append_printf (str, "/TESTVAL=%s",
			  gtk_entry_get_text (GTK_ENTRY (d->test_value_entry)));

  g_string_append (str, "\n\t/VARIABLES=");

  append_variable_names (str, d->dict, GTK_TREE_VIEW (d->vars_treeview));

  tt_options_dialog_append_syntax (d->opt, str);

  g_string_append (str, ".\n");

  text = str->str;

  g_string_free (str, FALSE);

  return text;
}



static void
refresh (const struct tt_one_sample_dialog *d)
{
  GtkTreeModel *model =
    gtk_tree_view_get_model (GTK_TREE_VIEW (d->vars_treeview));

  gtk_entry_set_text (GTK_ENTRY (d->test_value_entry), "");

  gtk_list_store_clear (GTK_LIST_STORE (model));
}



static gboolean
dialog_state_valid (gpointer data)
{
  gchar *s = NULL;
  const gchar *text;
  struct tt_one_sample_dialog *tt_d = data;

  GtkTreeModel *vars =
    gtk_tree_view_get_model (GTK_TREE_VIEW (tt_d->vars_treeview));

  GtkTreeIter notused;

  text = gtk_entry_get_text (GTK_ENTRY (tt_d->test_value_entry));

  if ( 0 == strcmp ("", text))
    return FALSE;

  /* Check to see if the entry is numeric */
  g_strtod (text, &s);

  if (s - text != strlen (text))
    return FALSE;


  if ( 0 == gtk_tree_model_get_iter_first (vars, &notused))
    return FALSE;

  return TRUE;
}


/* Pops up the dialog box */
void
t_test_one_sample_dialog (GObject *o, gpointer data)
{
  struct tt_one_sample_dialog tt_d;
  gint response;
  struct data_editor *de = data;

  PsppireVarStore *vs;

  GladeXML *xml = XML_NEW ("t-test.glade");

  GtkSheet *var_sheet =
    GTK_SHEET (get_widget_assert (de->xml, "variable_sheet"));

  GtkWidget *dict_view =
    get_widget_assert (xml, "one-sample-t-test-treeview2");

  GtkWidget *options_button =
    get_widget_assert (xml, "button1");

  GtkWidget *selector = get_widget_assert (xml, "psppire-selector1");

  GtkWidget *dialog = get_widget_assert (xml, "t-test-one-sample-dialog");

  vs = PSPPIRE_VAR_STORE (gtk_sheet_get_model (var_sheet));

  tt_d.dict = vs->dict;
  tt_d.vars_treeview = get_widget_assert (xml, "one-sample-t-test-treeview1");
  tt_d.test_value_entry = get_widget_assert (xml, "test-value-entry");
  tt_d.opt = tt_options_dialog_create (xml, de->parent.window);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), de->parent.window);

  attach_dictionary_to_treeview (GTK_TREE_VIEW (dict_view),
				 vs->dict,
				 GTK_SELECTION_MULTIPLE,
				 var_is_numeric);

  set_dest_model (GTK_TREE_VIEW (tt_d.vars_treeview), vs->dict);


  psppire_selector_set_subjects (PSPPIRE_SELECTOR (selector),
				 dict_view, tt_d.vars_treeview,
				 insert_source_row_into_tree_view,
				 NULL);


  g_signal_connect_swapped (dialog, "refresh",
			    G_CALLBACK (refresh),  &tt_d);


  g_signal_connect_swapped (options_button, "clicked",
			    G_CALLBACK (tt_options_dialog_run), tt_d.opt);

  psppire_dialog_set_valid_predicate (PSPPIRE_DIALOG (dialog),
				      dialog_state_valid, &tt_d);

  response = psppire_dialog_run (PSPPIRE_DIALOG (dialog));

  switch (response)
    {
    case GTK_RESPONSE_OK:
      {
	gchar *syntax = generate_syntax (&tt_d);
	struct getl_interface *sss = create_syntax_string_source (syntax);
	execute_syntax (sss);

	g_free (syntax);
      }
      break;
    case PSPPIRE_RESPONSE_PASTE:
      {
	gchar *syntax = generate_syntax (&tt_d);

	struct syntax_editor *se =
	  (struct syntax_editor *) window_create (WINDOW_SYNTAX, NULL);

	gtk_text_buffer_insert_at_cursor (se->buffer, syntax, -1);

	g_free (syntax);
      }
      break;
    default:
      break;
    }


  g_object_unref (xml);
  tt_options_dialog_destroy (tt_d.opt);
}

