/*
   PSPPIRE --- A Graphical User Interface for PSPP
   Copyright (C) 2007  Free Software Foundation

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA. */


#ifndef VAR_DISPLAY_H
#define VAR_DISPLAY 1

#include <glib.h>

struct variable;


gchar * name_to_string (const struct variable *var, GError **err);


gchar * missing_values_to_string (const struct variable *pv, GError **err);

gchar * measure_to_string (const struct variable *var, GError **err);

gchar * label_to_string (const struct variable *var, GError **err);


#endif