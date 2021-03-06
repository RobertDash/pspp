/* PSPPIRE - a graphical user interface for PSPP.
   Copyright (C) 2011, 2012  Free Software Foundation

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

#include "count-dialog.h"

#include <gtk/gtk.h>
#include "builder-wrapper.h"
#include "psppire-dialog.h"
#include "psppire-selector.h"
#include "psppire-val-chooser.h"
#include "psppire-var-view.h"
#include "psppire-acr.h"
#include "dialog-common.h"


#include <ui/syntax-gen.h>
#include "executor.h"
#include "helper.h"

struct cnt_dialog
{
  PsppireDict *dict;

  GtkWidget *dialog;

  GtkListStore *value_list;
  GtkWidget *chooser;

  GtkWidget *target;
  GtkWidget *label;
  GtkWidget *variable_treeview;
};

/* Callback which gets called when a new row is selected
   in the acr's variable treeview.
   We use if to set the togglebuttons and entries to correspond to the
   selected row.
*/
static void
on_acr_selection_change (GtkTreeSelection *selection, gpointer data)
{
  GtkTreeIter iter;
  struct old_value *ov = NULL;
  GtkTreeModel *model = NULL;
  struct cnt_dialog *cnt = data;
  GValue ov_value = {0};

  if ( ! gtk_tree_selection_get_selected (selection, &model, &iter) )
    return;

  gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter,
			    0, &ov_value);

  ov = g_value_get_boxed (&ov_value);
  psppire_val_chooser_set_status (PSPPIRE_VAL_CHOOSER (cnt->chooser), ov);
}



static char * generate_syntax (const struct cnt_dialog *cnt);

static void values_dialog (struct cnt_dialog *cd);

static void
refresh (PsppireDialog *dialog, struct cnt_dialog *cnt)
{
  GtkTreeModel *vars =
    gtk_tree_view_get_model (GTK_TREE_VIEW (cnt->variable_treeview));

  gtk_list_store_clear (GTK_LIST_STORE (vars));

  gtk_entry_set_text (GTK_ENTRY (cnt->target), "");
  gtk_entry_set_text (GTK_ENTRY (cnt->label), "");
  gtk_list_store_clear (GTK_LIST_STORE (cnt->value_list));
}

static gboolean
dialog_state_valid (gpointer data)
{
  GtkTreeIter iter;
  struct cnt_dialog *cnt = data;

  if (! cnt->value_list)
    return FALSE;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (cnt->value_list),	&iter) )
    return FALSE;

  if (!gtk_tree_model_get_iter_first (gtk_tree_view_get_model (GTK_TREE_VIEW (cnt->variable_treeview)), &iter))
    return FALSE;

  if (0 == strcmp ("", gtk_entry_get_text (GTK_ENTRY (cnt->target))))
    return FALSE;

  return TRUE;
}

void count_dialog (PsppireDataWindow *de)
{
  gint response;
  PsppireVarStore *vs = NULL;
  struct cnt_dialog cnt;

  GtkBuilder *builder = builder_new ("count.ui");

  GtkWidget *selector = get_widget_assert (builder, "count-selector1");

  GtkWidget *dict_view = get_widget_assert (builder, "dict-view");
  GtkWidget *button = get_widget_assert (builder, "button1");

  cnt.target = get_widget_assert (builder, "entry1");
  cnt.label = get_widget_assert (builder, "entry2");
  cnt.variable_treeview = get_widget_assert (builder, "treeview2");

  g_signal_connect_swapped (button, "clicked", G_CALLBACK (values_dialog), &cnt);

  cnt.value_list = gtk_list_store_new (1,old_value_get_type ());

  cnt.dialog =  get_widget_assert (builder, "count-dialog");

  g_signal_connect (cnt.dialog, "refresh", G_CALLBACK (refresh),  &cnt);


  g_object_get (de->data_editor, "var-store", &vs, NULL);

  g_object_get (vs, "dictionary", &cnt.dict, NULL);

  gtk_window_set_transient_for (GTK_WINDOW (cnt.dialog), GTK_WINDOW (de));

  g_object_set (dict_view, "model", cnt.dict, NULL);

  psppire_selector_set_allow (PSPPIRE_SELECTOR (selector),  numeric_only);

  psppire_dialog_set_valid_predicate (PSPPIRE_DIALOG (cnt.dialog),
				      dialog_state_valid, &cnt);

  response = psppire_dialog_run (PSPPIRE_DIALOG (cnt.dialog));

  switch (response)
    {
    case GTK_RESPONSE_OK:
      g_free (execute_syntax_string (de, generate_syntax (&cnt)));
      break;
    case PSPPIRE_RESPONSE_PASTE:
      g_free (paste_syntax_to_window (generate_syntax (&cnt)));
      break;
    default:
      break;
    }


  g_object_unref (builder);
}

/* A function to set a value in a column in the ACR */
static gboolean
set_value (gint col, GValue  *val, gpointer data)
{
  struct cnt_dialog *cnt = data;
  PsppireValChooser *vc = PSPPIRE_VAL_CHOOSER (cnt->chooser);
  struct old_value ov;
	
  g_assert (col == 0);

  psppire_val_chooser_get_status (vc, &ov);

  g_value_init (val, old_value_get_type ());
  g_value_set_boxed (val, &ov);

  return TRUE;
}


static void
values_dialog (struct cnt_dialog *cd)
{
  gint response;
  GtkListStore *local_store = clone_list_store (cd->value_list);
  GtkBuilder *builder = builder_new ("count.ui");

  GtkWidget *dialog = get_widget_assert (builder, "values-dialog");

  GtkWidget *acr = get_widget_assert (builder, "acr");
  cd->chooser = get_widget_assert (builder, "value-chooser");

  psppire_acr_set_enabled (PSPPIRE_ACR (acr), TRUE);

  psppire_acr_set_model (PSPPIRE_ACR (acr), local_store);
  psppire_acr_set_get_value_func (PSPPIRE_ACR (acr), set_value, cd);

  {
    GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (PSPPIRE_ACR(acr)->tv));
    g_signal_connect (sel, "changed",
		    G_CALLBACK (on_acr_selection_change), cd);
  }

  response = psppire_dialog_run (PSPPIRE_DIALOG (dialog));

  if ( response == PSPPIRE_RESPONSE_CONTINUE )
    {
      g_object_unref (cd->value_list);
      cd->value_list = local_store;
    }
  else
    {
      g_object_unref (local_store);
    }

  psppire_dialog_notify_change (PSPPIRE_DIALOG (cd->dialog));

  g_object_unref (builder);
}



static char *
generate_syntax (const struct cnt_dialog *cnt)
{
  gchar *text = NULL;
  const gchar *s = NULL;
  gboolean ok;
  GtkTreeIter iter;
  GString *str = g_string_sized_new (100);

  g_string_append (str, "\nCOUNT ");

  g_string_append (str, gtk_entry_get_text (GTK_ENTRY (cnt->target)));

  g_string_append (str, " =");

  psppire_var_view_append_names (PSPPIRE_VAR_VIEW (cnt->variable_treeview), 0, str);

  g_string_append (str, "(");
  for (ok = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (cnt->value_list),
					   &iter);
       ok;
       ok = gtk_tree_model_iter_next (GTK_TREE_MODEL (cnt->value_list), &iter))
    {
      GValue a_value = {0};
      struct old_value *ov;

      gtk_tree_model_get_value (GTK_TREE_MODEL (cnt->value_list), &iter,
				0, &a_value);

      ov = g_value_get_boxed (&a_value);

      g_string_append (str, " ");
      old_value_append_syntax (str, ov);
    }
  g_string_append (str, ").");


  s = gtk_entry_get_text (GTK_ENTRY (cnt->label));
  if (0 != strcmp (s, ""))
  {
    struct string ds;
    ds_init_empty (&ds);
    g_string_append (str, "\nVARIABLE LABELS ");

    g_string_append (str, gtk_entry_get_text (GTK_ENTRY (cnt->target)));

    g_string_append (str, " ");

    syntax_gen_string (&ds, ss_cstr (s));

    g_string_append (str, ds_cstr (&ds));

    g_string_append (str, ".");
    ds_destroy (&ds);
  }

  g_string_append (str, "\nEXECUTE.\n");

  text = str->str;

  g_string_free (str, FALSE);

  return text;
}
