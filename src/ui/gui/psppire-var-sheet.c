
/* PSPPIRE - a graphical user interface for PSPP.
   Copyright (C) 2008 Free Software Foundation, Inc.

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
#include "psppire-var-sheet.h"

#include <glade/glade.h>
#include "helper.h"
#include <gtksheet/gsheet-hetero-column.h>
#include "customentry.h"
#include <data/variable.h>
#include "psppire-var-store.h"

#include <gettext.h>
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid


static void psppire_var_sheet_class_init  (PsppireVarSheetClass *klass);
static void psppire_var_sheet_init        (PsppireVarSheet      *vs);


GType
psppire_var_sheet_get_type (void)
{
  static GType vs_type = 0;

  if (!vs_type)
    {
      static const GTypeInfo vs_info =
      {
	sizeof (PsppireVarSheetClass),
	NULL, /* base_init */
        NULL, /* base_finalize */
	(GClassInitFunc) psppire_var_sheet_class_init,
        NULL, /* class_finalize */
	NULL, /* class_data */
        sizeof (PsppireVarSheet),
	0,
	(GInstanceInitFunc) psppire_var_sheet_init,
      };

      vs_type = g_type_register_static (GTK_TYPE_SHEET, "PsppireVarSheet",
					&vs_info, 0);
    }

  return vs_type;
}

static GObjectClass * parent_class = NULL;

static void
psppire_var_sheet_dispose (GObject *obj)
{
  PsppireVarSheet *vs = (PsppireVarSheet *)obj;

  if (vs->dispose_has_run)
    return;

  /* Make sure dispose does not run twice. */
  vs->dispose_has_run = TRUE;

  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
psppire_var_sheet_finalize (GObject *obj)
{
   /* Chain up to the parent class */
   G_OBJECT_CLASS (parent_class)->finalize (obj);
}


struct column_parameters
{
  gchar label[20];
  gint width ;
};

static const struct column_parameters column_def[] = {
  { N_("Name"),    80},
  { N_("Type"),    100},
  { N_("Width"),   57},
  { N_("Decimals"),91},
  { N_("Label"),   95},
  { N_("Values"),  103},
  { N_("Missing"), 95},
  { N_("Columns"), 80},
  { N_("Align"),   69},
  { N_("Measure"), 99},
};


#define n_ALIGNMENTS 3

const gchar *const alignments[n_ALIGNMENTS + 1]={
  N_("Left"),
  N_("Right"),
  N_("Center"),
  0
};

const gchar *const measures[n_MEASURES + 1]={
  N_("Nominal"),
  N_("Ordinal"),
  N_("Scale"),
  0
};



/* Create a list store from an array of strings */
static GtkListStore *
create_label_list (const gchar *const *labels)
{
  const gchar *s;
  gint i = 0;
  GtkTreeIter iter;

  GtkListStore *list_store;
  list_store = gtk_list_store_new (1, G_TYPE_STRING);


  while ( (s = labels[i++]))
    {
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter,
			  0, gettext (s),
			  -1);
    }

  return list_store;
}




static void
psppire_var_sheet_class_init (PsppireVarSheetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = psppire_var_sheet_dispose;
  object_class->finalize = psppire_var_sheet_finalize;


  klass->measure_list = create_label_list (measures);
  klass->alignment_list = create_label_list (alignments);
}



/* Callback for when the alignment combo box
   item is selected */
static void
change_alignment (GtkComboBox *cb,
		  struct variable *var)
{
  gint active_item = gtk_combo_box_get_active (cb);

  if ( active_item < 0 ) return ;

  var_set_alignment (var, active_item);
}



/* Callback for when the measure combo box
   item is selected */
static void
change_measure (GtkComboBox *cb,
		struct variable *var)
{
  gint active_item = gtk_combo_box_get_active (cb);

  if ( active_item < 0 ) return ;

  var_set_measure (var, active_item);
}



static gboolean
traverse_cell_callback (GtkSheet *sheet,
			gint row, gint column,
			gint *new_row, gint *new_column
			)
{
  PsppireVarStore *var_store = PSPPIRE_VAR_STORE (gtk_sheet_get_model (sheet));

  gint n_vars = psppire_var_store_get_var_cnt (var_store);

  if ( row == n_vars && *new_row >= n_vars)
    {
      GtkEntry *entry = GTK_ENTRY (gtk_sheet_get_entry (sheet));

      const gchar *name = gtk_entry_get_text (entry);

      if (! psppire_dict_check_name (var_store->dict, name, TRUE))
	return FALSE;

      psppire_dict_insert_variable (var_store->dict, row, name);

      return TRUE;
    }

  /* If the destination cell is outside the current  variables, then
     automatically create variables for the new rows.
  */
  if ( (*new_row > n_vars) ||
       (*new_row == n_vars && *new_column != PSPPIRE_VAR_STORE_COL_NAME) )
    {
      gint i;
      for ( i = n_vars ; i <= *new_row; ++i )
	psppire_dict_insert_variable (var_store->dict, i, NULL);
    }

  return TRUE;
}




/*
   Callback whenever the pointer leaves a cell on the var sheet.
*/
static gboolean
var_sheet_cell_entry_leave (GtkSheet * sheet, gint row, gint column,
			    gpointer data)
{
  gtk_sheet_change_entry (sheet, GTK_TYPE_ENTRY);
  return TRUE;
}


/*
   Callback whenever the pointer enters a cell on the var sheet.
*/
static gboolean
var_sheet_cell_entry_enter (PsppireVarSheet *vs, gint row, gint column,
			    gpointer data)
{
  GtkSheetCellAttr attributes;
  PsppireVarStore *var_store ;
  PsppireVarSheetClass *vs_class =
    PSPPIRE_VAR_SHEET_CLASS(G_OBJECT_GET_CLASS (vs));

  struct variable *var ;
  GtkSheet *sheet = GTK_SHEET (vs);

  g_return_val_if_fail (sheet != NULL, FALSE);

  var_store = PSPPIRE_VAR_STORE (gtk_sheet_get_model (sheet));

  g_assert (var_store);

  if ( row >= psppire_var_store_get_var_cnt (var_store))
    return TRUE;

  gtk_sheet_get_attributes (sheet, row, column, &attributes);


  var = psppire_var_store_get_var (var_store, row);

  switch (column)
    {
    case PSPPIRE_VAR_STORE_COL_ALIGN:
      {
	static GtkListStore *list_store = NULL;
	GtkComboBoxEntry *cbe;
	gtk_sheet_change_entry (sheet, GTK_TYPE_COMBO_BOX_ENTRY);
	cbe =
	  GTK_COMBO_BOX_ENTRY (gtk_sheet_get_entry (sheet)->parent);


	if ( ! list_store) list_store = create_label_list (alignments);

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbe),
				GTK_TREE_MODEL (vs_class->alignment_list));

	gtk_combo_box_entry_set_text_column (cbe, 0);

	g_signal_connect (G_OBJECT (cbe),"changed",
			 G_CALLBACK (change_alignment), var);
      }
      break;

    case PSPPIRE_VAR_STORE_COL_MEASURE:
      {
	GtkComboBoxEntry *cbe;
	gtk_sheet_change_entry (sheet, GTK_TYPE_COMBO_BOX_ENTRY);
	cbe =
	  GTK_COMBO_BOX_ENTRY (gtk_sheet_get_entry (sheet)->parent);



	gtk_combo_box_set_model (GTK_COMBO_BOX (cbe),
				GTK_TREE_MODEL (vs_class->measure_list));

	gtk_combo_box_entry_set_text_column (cbe, 0);

	g_signal_connect (G_OBJECT (cbe),"changed",
			  G_CALLBACK (change_measure), var);
      }
      break;

    case PSPPIRE_VAR_STORE_COL_VALUES:
      {
	PsppireCustomEntry *customEntry;

	gtk_sheet_change_entry (sheet, PSPPIRE_CUSTOM_ENTRY_TYPE);

	customEntry =
	  PSPPIRE_CUSTOM_ENTRY (gtk_sheet_get_entry (sheet));

	if ( var_is_long_string (var))
	  g_object_set (customEntry,
			"editable", FALSE,
			NULL);

	val_labs_dialog_set_target_variable (vs->val_labs_dialog, var);

	g_signal_connect_swapped (customEntry,
				  "clicked",
				  G_CALLBACK (val_labs_dialog_show),
				  vs->val_labs_dialog);
      }
      break;

    case PSPPIRE_VAR_STORE_COL_MISSING:
      {
	PsppireCustomEntry *customEntry;

	gtk_sheet_change_entry (sheet, PSPPIRE_CUSTOM_ENTRY_TYPE);

	customEntry =
	  PSPPIRE_CUSTOM_ENTRY (gtk_sheet_get_entry (sheet));

	if ( var_is_long_string (var))
	  g_object_set (customEntry,
			"editable", FALSE,
			NULL);


	vs->missing_val_dialog->pv =
	  psppire_var_store_get_var (var_store, row);

	g_signal_connect_swapped (customEntry,
				  "clicked",
				  G_CALLBACK (missing_val_dialog_show),
				  vs->missing_val_dialog);
      }
      break;

    case PSPPIRE_VAR_STORE_COL_TYPE:
      {
	PsppireCustomEntry *customEntry;

	gtk_sheet_change_entry (sheet, PSPPIRE_CUSTOM_ENTRY_TYPE);

	customEntry =
	  PSPPIRE_CUSTOM_ENTRY (gtk_sheet_get_entry (sheet));


	/* Popup the Variable Type dialog box */
	vs->var_type_dialog->pv = var;

	g_signal_connect_swapped (customEntry,
				 "clicked",
				 G_CALLBACK (var_type_dialog_show),
				  vs->var_type_dialog);
      }
      break;

    case PSPPIRE_VAR_STORE_COL_WIDTH:
    case PSPPIRE_VAR_STORE_COL_DECIMALS:
    case PSPPIRE_VAR_STORE_COL_COLUMNS:
      {
	if ( attributes.is_editable)
	  {
	    gint r_min, r_max;

	    const gchar *s = gtk_sheet_cell_get_text (sheet, row, column);

	    if (s)
	      {
		GtkSpinButton *spinButton ;
		const gint current_value  = g_strtod (s, NULL);
		GtkObject *adj ;

		const struct fmt_spec *fmt = var_get_write_format (var);
		switch (column)
		  {
		  case PSPPIRE_VAR_STORE_COL_WIDTH:
		    r_min = MAX (fmt->d + 1, fmt_min_output_width (fmt->type));
		    r_max = fmt_max_output_width (fmt->type);
		    break;
		  case PSPPIRE_VAR_STORE_COL_DECIMALS:
		    r_min = 0 ;
		    r_max = fmt_max_output_decimals (fmt->type, fmt->w);
		    break;
		  case PSPPIRE_VAR_STORE_COL_COLUMNS:
		    r_min = 1;
		    r_max = 255 ; /* Is this a sensible value ? */
		    break;
		  default:
		    g_assert_not_reached ();
		  }

		adj = gtk_adjustment_new (current_value,
					 r_min, r_max,
					 1.0, 1.0, 1.0 /* steps */
					 );

		gtk_sheet_change_entry (sheet, GTK_TYPE_SPIN_BUTTON);

		spinButton =
		  GTK_SPIN_BUTTON (gtk_sheet_get_entry (sheet));

		gtk_spin_button_set_adjustment (spinButton, GTK_ADJUSTMENT (adj));
		gtk_spin_button_set_digits (spinButton, 0);
	      }
	  }
      }
      break;

    default:
      gtk_sheet_change_entry (sheet, GTK_TYPE_ENTRY);
      break;
    }


  return TRUE;
}




static void
psppire_var_sheet_init (PsppireVarSheet *vs)
{
  gint i;
  GObject *geo = g_sheet_hetero_column_new (75, PSPPIRE_VAR_STORE_n_COLS);
  GladeXML *xml = XML_NEW ("data-editor.glade");

  vs->val_labs_dialog = val_labs_dialog_create (xml);
  vs->missing_val_dialog = missing_val_dialog_create (xml);
  vs->var_type_dialog = var_type_dialog_create (xml);

  g_object_unref (xml);

  vs->dispose_has_run = FALSE;

  for (i = 0 ; i < PSPPIRE_VAR_STORE_n_COLS ; ++i )
    {
      g_sheet_hetero_column_set_button_label (G_SHEET_HETERO_COLUMN (geo), i,
					      gettext (column_def[i].label));

      g_sheet_hetero_column_set_width (G_SHEET_HETERO_COLUMN (geo), i,
				       column_def[i].width);
    }

  g_object_set (vs, "column-geometry", geo, NULL);


  g_signal_connect (vs, "activate",
		    G_CALLBACK (var_sheet_cell_entry_enter),
		    NULL);

  g_signal_connect (vs, "deactivate",
		    G_CALLBACK (var_sheet_cell_entry_leave),
		    NULL);

  g_signal_connect (vs, "traverse",
		    G_CALLBACK (traverse_cell_callback), NULL);
}


GtkWidget*
psppire_var_sheet_new (void)
{
  return GTK_WIDGET (g_object_new (psppire_var_sheet_get_type (), NULL));
}