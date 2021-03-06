/* PSPP - a program for statistical analysis.
   Copyright (C) 2009, 2010 Free Software Foundation, Inc.

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

#include <errno.h>
#include <stdlib.h>

#include "data/file-name.h"
#include "libpspp/assertion.h"
#include "libpspp/compiler.h"
#include "libpspp/message.h"
#include "libpspp/string-map.h"
#include "output/text-item.h"
#include "output/driver-provider.h"
#include "output/options.h"
#include "output/message-item.h"
#include "output/table-item.h"
#include "output/table-provider.h"

#include "gl/error.h"
#include "gl/xalloc.h"
#include "gl/xvasprintf.h"

#include "gettext.h"
#define _(msgid) gettext (msgid)

/* Comma-separated value output driver. */
struct csv_driver
  {
    struct output_driver driver;

    char *separator;            /* Field separator (usually comma or tab). */
    int quote;                  /* Quote character (usually ' or ") or 0. */
    char *quote_set;            /* Characters that force quoting. */
    bool captions;              /* Print table captions? */

    char *file_name;            /* Output file name. */
    char *command_name;         /* Current command. */
    FILE *file;                 /* Output file. */
    int n_items;                /* Number of items output so far. */
  };

static const struct output_driver_class csv_driver_class;

static struct csv_driver *
csv_driver_cast (struct output_driver *driver)
{
  assert (driver->class == &csv_driver_class);
  return UP_CAST (driver, struct csv_driver, driver);
}

static struct driver_option *
opt (struct output_driver *d, struct string_map *options, const char *key,
     const char *default_value)
{
  return driver_option_get (d, options, key, default_value);
}

static struct output_driver *
csv_create (const char *file_name, enum settings_output_devices device_type,
            struct string_map *o)
{
  struct output_driver *d;
  struct csv_driver *csv;
  char *quote;

  csv = xzalloc (sizeof *csv);
  d = &csv->driver;
  output_driver_init (&csv->driver, &csv_driver_class, file_name, device_type);

  csv->separator = parse_string (opt (d, o, "separator", ","));
  quote = parse_string (opt (d, o, "quote", "\""));
  csv->quote = quote[0];
  free (quote);
  csv->quote_set = xasprintf ("\n\r\t%s%c", csv->separator, csv->quote);
  csv->captions = parse_boolean (opt (d, o, "captions", "true"));
  csv->file_name = xstrdup (file_name);
  csv->file = fn_open (csv->file_name, "w");
  csv->n_items = 0;

  if (csv->file == NULL)
    {
      error (0, errno, _("error opening output file `%s'"), csv->file_name);
      output_driver_destroy (d);
      return NULL;
    }

  return d;
}

static void
csv_destroy (struct output_driver *driver)
{
  struct csv_driver *csv = csv_driver_cast (driver);

  if (csv->file != NULL)
    fn_close (csv->file_name, csv->file);

  free (csv->separator);
  free (csv->quote_set);
  free (csv->file_name);
  free (csv);
}

static void
csv_flush (struct output_driver *driver)
{
  struct csv_driver *csv = csv_driver_cast (driver);
  if (csv->file != NULL)
    fflush (csv->file);
}

static void
csv_output_field (struct csv_driver *csv, const char *field)
{
  while (*field == ' ')
    field++;

  if (csv->quote && field[strcspn (field, csv->quote_set)])
    {
      const char *p;

      putc (csv->quote, csv->file);
      for (p = field; *p != '\0'; p++)
        {
          if (*p == csv->quote)
            putc (csv->quote, csv->file);
          putc (*p, csv->file);
        }
      putc (csv->quote, csv->file);
    }
  else
    fputs (field, csv->file);
}

static void
csv_output_field_format (struct csv_driver *csv, const char *format, ...)
  PRINTF_FORMAT (2, 3);

static void
csv_output_field_format (struct csv_driver *csv, const char *format, ...)
{
  va_list args;
  char *s;

  va_start (args, format);
  s = xvasprintf (format, args);
  va_end (args);

  csv_output_field (csv, s);
  free (s);
}

static void
csv_put_separator (struct csv_driver *csv)
{
  if (csv->n_items++ > 0)
    putc ('\n', csv->file);
}

static void
csv_submit (struct output_driver *driver,
            const struct output_item *output_item)
{
  struct csv_driver *csv = csv_driver_cast (driver);

  output_driver_track_current_command (output_item, &csv->command_name);

  if (is_table_item (output_item))
    {
      struct table_item *table_item = to_table_item (output_item);
      const char *caption = table_item_get_caption (table_item);
      const struct table *t = table_item_get_table (table_item);
      int x, y;

      csv_put_separator (csv);

      if (csv->captions && caption != NULL)
        {
          csv_output_field_format (csv, "Table: %s", caption);
          putc ('\n', csv->file);
        }

      for (y = 0; y < table_nr (t); y++)
        {
          for (x = 0; x < table_nc (t); x++)
            {
              struct table_cell cell;

              table_get_cell (t, x, y, &cell);

              if (x > 0)
                fputs (csv->separator, csv->file);

              if (x != cell.d[TABLE_HORZ][0] || y != cell.d[TABLE_VERT][0])
                csv_output_field (csv, "");
              else
                csv_output_field (csv, cell.contents);

              table_cell_free (&cell);
            }
          putc ('\n', csv->file);
        }
    }
  else if (is_text_item (output_item))
    {
      const struct text_item *text_item = to_text_item (output_item);
      enum text_item_type type = text_item_get_type (text_item);
      const char *text = text_item_get_text (text_item);

      if (type == TEXT_ITEM_COMMAND_OPEN || type == TEXT_ITEM_COMMAND_CLOSE
          || type == TEXT_ITEM_SYNTAX)
        return;

      csv_put_separator (csv);
      switch (type)
        {
        case TEXT_ITEM_TITLE:
          csv_output_field_format (csv, "Title: %s", text);
          break;

        case TEXT_ITEM_SUBTITLE:
          csv_output_field_format (csv, "Subtitle: %s", text);
          break;

        default:
          csv_output_field (csv, text);
          break;
        }
      putc ('\n', csv->file);
    }
  else if (is_message_item (output_item))
    {
      const struct message_item *message_item = to_message_item (output_item);
      const struct msg *msg = message_item_get_msg (message_item);
      char *s = msg_to_string (msg, csv->command_name);
      csv_put_separator (csv);
      csv_output_field (csv, s);
      free (s);
      putc ('\n', csv->file);
    }
}

struct output_driver_factory csv_driver_factory = { "csv", csv_create };

static const struct output_driver_class csv_driver_class =
  {
    "csv",
    csv_destroy,
    csv_submit,
    csv_flush,
  };
