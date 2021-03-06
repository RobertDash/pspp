/* PSPP - a program for statistical analysis.
   Copyright (C) 2006, 2010, 2011 Free Software Foundation, Inc.

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

#include "data/dataset-reader.h"

#include <stdlib.h>

#include "data/case.h"
#include "data/casereader.h"
#include "data/dataset.h"
#include "data/dictionary.h"
#include "data/file-handle-def.h"
#include "libpspp/assertion.h"
#include "libpspp/message.h"

#include "gl/xalloc.h"

#include "gettext.h"
#define _(msgid) gettext (msgid)

/* Opens FH, which must have referent type FH_REF_DATASET, and returns a
   dataset_reader for it, or a null pointer on failure.  Stores a copy of the
   dictionary for the dataset file into *DICT.  The caller takes ownership of
   the casereader and the dictionary.  */
struct casereader *
dataset_reader_open (struct file_handle *fh, struct dictionary **dict)
{
  struct dataset *ds;

  /* We don't bother doing fh_lock or fh_ref on the file handle,
     as there's no advantage in this case, and doing these would
     require us to keep track of the "struct file_handle" and
     "struct fh_lock" and undo our work later. */
  assert (fh_get_referent (fh) == FH_REF_DATASET);

  ds = fh_get_dataset (fh);
  if (ds == NULL || !dataset_has_source (ds))
    {
      msg (SE, _("Cannot read from dataset %s because no dictionary or data "
                 "has been written to it yet."),
           fh_get_name (fh));
      return NULL;
    }

  *dict = dict_clone (dataset_dict (ds));
  return casereader_clone (dataset_source (ds));
}
