/* PSPP - a program for statistical analysis.
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

/* FIXME: error checking. */
/* FIXME: merge pattern should be improved, this one causes a
   performance regression. */
#include <config.h>

#include <math/merge.h>

#include <data/case-ordering.h>
#include <data/case.h>
#include <data/casereader.h>
#include <data/casewriter.h>
#include <libpspp/array.h>
#include <libpspp/assertion.h>
#include <libpspp/taint.h>

#include "xalloc.h"

#define MAX_MERGE_ORDER 7

struct merge_input
  {
    struct casereader *reader;
    struct ccase c;
  };

struct merge
  {
    struct case_ordering *ordering;
    struct merge_input inputs[MAX_MERGE_ORDER];
    size_t input_cnt;
  };

static void do_merge (struct merge *m);

struct merge *
merge_create (const struct case_ordering *ordering)
{
  struct merge *m = xmalloc (sizeof *m);
  m->ordering = case_ordering_clone (ordering);
  m->input_cnt = 0;
  return m;
}

void
merge_destroy (struct merge *m)
{
  if (m != NULL)
    {
      size_t i;

      case_ordering_destroy (m->ordering);
      for (i = 0; i < m->input_cnt; i++)
        casereader_destroy (m->inputs[i].reader);
      free (m);
    }
}

void
merge_append (struct merge *m, struct casereader *r)
{
  r = casereader_rename (r);
  m->inputs[m->input_cnt++].reader = r;
  if (m->input_cnt >= MAX_MERGE_ORDER)
    do_merge (m);
}

struct casereader *
merge_make_reader (struct merge *m)
{
  struct casereader *r;

  if (m->input_cnt > 1)
    do_merge (m);

  if (m->input_cnt == 1)
    {
      r = m->inputs[0].reader;
      m->input_cnt = 0;
    }
  else if (m->input_cnt == 0)
    {
      size_t value_cnt = case_ordering_get_value_cnt (m->ordering);
      struct casewriter *writer = mem_writer_create (value_cnt);
      r = casewriter_make_reader (writer);
    }
  else
    NOT_REACHED ();

  return r;
}

static bool
read_input_case (struct merge *m, size_t idx)
{
  struct merge_input *i = &m->inputs[idx];

  if (casereader_read (i->reader, &i->c))
    return true;
  else
    {
      casereader_destroy (i->reader);
      remove_element (m->inputs, m->input_cnt, sizeof *m->inputs, idx);
      m->input_cnt--;
      return false;
    }
}

static void
do_merge (struct merge *m)
{
  struct casewriter *w;
  size_t i;

  assert (m->input_cnt > 1);

  w = tmpfile_writer_create (case_ordering_get_value_cnt (m->ordering));
  for (i = 0; i < m->input_cnt; i++)
    taint_propagate (casereader_get_taint (m->inputs[i].reader),
                     casewriter_get_taint (w));

  for (i = 0; i < m->input_cnt; )
    if (read_input_case (m, i))
      i++;
  while (m->input_cnt > 0)
    {
      size_t min;

      min = 0;
      for (i = 1; i < m->input_cnt; i++)
        if (case_ordering_compare_cases (&m->inputs[i].c, &m->inputs[min].c,
                                         m->ordering) < 0)
          min = i;

      casewriter_write (w, &m->inputs[min].c);
      read_input_case (m, min);
    }

  m->input_cnt = 1;
  m->inputs[0].reader = casewriter_make_reader (w);
}
