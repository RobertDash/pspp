/* PSPPIRE - a graphical user interface for PSPP.
   Copyright (C) 2009  Free Software Foundation

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


#ifndef __PSPPIRE_VAR_VIEW_H__
#define __PSPPIRE_VAR_VIEW_H__


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

#define PSPPIRE_VAR_VIEW_TYPE            (psppire_var_view_get_type ())
#define PSPPIRE_VAR_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PSPPIRE_VAR_VIEW_TYPE, PsppireVarView))
#define PSPPIRE_VAR_VIEW_CLASS(class)    (G_TYPE_CHECK_CLASS_CAST ((class), \
    PSPPIRE_VAR_VIEW_TYPE, PsppireVarViewClass))
#define PSPPIRE_IS_VAR_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
    PSPPIRE_VAR_VIEW_TYPE))
#define PSPPIRE_IS_VAR_VIEW_CLASS(class) (G_TYPE_CHECK_CLASS_TYPE ((class), \
    PSPPIRE_VAR_VIEW_TYPE))


typedef struct _PsppireVarView       PsppireVarView;
typedef struct _PsppireVarViewClass  PsppireVarViewClass;

struct variable;

struct _PsppireVarView
{
  GtkTreeView parent;

  GtkListStore *list;
  
  gint *nums;
};

struct _PsppireVarViewClass
{
  GtkTreeViewClass parent_class;

};

GType      psppire_var_view_get_type        (void);

gint psppire_var_view_append_names (PsppireVarView *vv, gint column, GString *string);

gboolean psppire_var_view_get_iter_first (PsppireVarView *vv, GtkTreeIter *iter);

gboolean psppire_var_view_get_iter_next (PsppireVarView *vv, GtkTreeIter *iter);

const struct variable * psppire_var_view_get_variable (PsppireVarView *vv, gint column, GtkTreeIter *iter);



G_END_DECLS

#endif /* __PSPPIRE_VAR_VIEW_H__ */