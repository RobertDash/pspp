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



#include <config.h>

#include <libpspp/message.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)
#define N_(msgid) (msgid)


#if !GNM_SUPPORT

struct casereader *
gnumeric_open_reader (struct gnumeric_read_info *gri, struct dictionary **dict)
{
  msg (ME, _("Support for Gnumeric files was not compiled into this installation of PSPP"));

  return NULL;
}

#else

#include <data/casereader-provider.h>
#include <errno.h>
#include <libpspp/str.h>
#include <libpspp/i18n.h>
#include <data/dictionary.h>
#include <data/variable.h>
#include <xalloc.h>

#include <errno.h>
#include <libxml/xmlreader.h>
#include <zlib.h>
#include <stdbool.h>

#include <data/case.h>
#include <data/value.h>

#include "gnumeric-reader.h"
#include <data/identifier.h>
#include <assert.h>


static void gnm_file_casereader_destroy (struct casereader *, void *);

static bool gnm_file_casereader_read (struct casereader *, void *,
				      struct ccase *);

static struct casereader_class gnm_file_casereader_class =
  {
    gnm_file_casereader_read,
    gnm_file_casereader_destroy,
    NULL,
    NULL,
  };

/* Convert a string, which is an integer encoded in base26
   IE, A=0, B=1, ... Z=25 to the integer it represents.
   ... except that in this scheme, digits with an exponent
   greater than 1 are implicitly incremented by 1, so
   AA  = 0 + 1*26, AB = 1 + 1*26,
   ABC = 2 + 2*26 + 1*26^2 ....
*/
static int
pseudo_base26 (const char *str)
{
  int i;
  int multiplier = 1;
  int result = 0;
  int len = strlen (str);

  for ( i = len - 1 ; i >= 0; --i)
    {
      int mantissa = (str[i] - 'A');

      if ( mantissa < 0 || mantissa > 25 )
	return -1;

      if ( i != len - 1)
	mantissa++;

      result += mantissa * multiplier;

      multiplier *= 26;
    }

  return result;
}



/* Convert a cell reference in the form "A1:B2", to
   integers.  A1 means column zero, row zero.
   B1 means column 1 row 0. AA1 means column 26, row 0.
*/
static bool
convert_cell_ref (const char *ref,
		  int *col0, int *row0,
		  int *coli, int *rowi)
{
  char startcol[5];
  char stopcol [5];

  int startrow;
  int stoprow;

  int n = sscanf (ref, "%4[a-zA-Z]%d:%4[a-zA-Z]%d",
	      startcol, &startrow,
	      stopcol, &stoprow);
  if ( n != 4)
    return false;

  str_uppercase (startcol);
  *col0 = pseudo_base26 (startcol);
  str_uppercase (stopcol);
  *coli = pseudo_base26 (stopcol);
  *row0 = startrow - 1;
  *rowi = stoprow - 1 ;

  return true;
}


enum reader_state
  {
    STATE_INIT = 0,        /* Initial state */
    STATE_SHEET_START,     /* Found the start of a sheet */
    STATE_SHEET_NAME,      /* Found the sheet name */
    STATE_MAXROW,
    STATE_SHEET_FOUND,     /* Found the sheet that we actually want */
    STATE_CELLS_START,     /* Found the start of the cell array */
    STATE_CELL             /* Found a cell */
  };


struct gnumeric_reader
{
  xmlTextReaderPtr xtr;

  enum reader_state state;
  int row;
  int col;
  int node_type;
  int sheet_index;


  const xmlChar *target_sheet;
  int target_sheet_index;

  int start_row;
  int start_col;
  int stop_row;
  int stop_col;


  size_t value_cnt;
  struct dictionary *dict;
  struct ccase first_case;
  bool used_first_case;
};

static void process_node (struct gnumeric_reader *r);

#define _xml(X) (const xmlChar *)(X)

#define _xmlchar_to_int(X) atoi((const char *)X)

static void
gnm_file_casereader_destroy (struct casereader *reader UNUSED, void *r_)
{
  struct gnumeric_reader *r = r_;
  if ( r == NULL)
	return ;

  if ( r->xtr)
    xmlFreeTextReader (r->xtr);

  if ( ! r->used_first_case )
    case_destroy (&r->first_case);

  free (r);
}

static void
process_node (struct gnumeric_reader *r)
{
  xmlChar *name = xmlTextReaderName (r->xtr);
  if (name == NULL)
    name = xmlStrdup (_xml ("--"));


  r->node_type = xmlTextReaderNodeType (r->xtr);

  switch ( r->state)
    {
    case STATE_INIT:
      if (0 == xmlStrcasecmp (name, _xml("gnm:Sheet")) &&
	  XML_READER_TYPE_ELEMENT  == r->node_type)
	{
	  r->state = STATE_SHEET_START;
	}
      break;
    case STATE_SHEET_START:
      if (0 == xmlStrcasecmp (name, _xml("gnm:Name"))  &&
	  XML_READER_TYPE_ELEMENT  == r->node_type)
	{
	  r->state = STATE_SHEET_NAME;
	}
      else if (0 == xmlStrcasecmp (name, _xml("gnm:Name"))  &&
	       XML_READER_TYPE_END_ELEMENT  == r->node_type)
	{
	  r->state = STATE_INIT;
	}
      break;
    case STATE_SHEET_NAME:
      if (0 == xmlStrcasecmp (name, _xml("gnm:Name"))  &&
	  XML_READER_TYPE_END_ELEMENT  == r->node_type)
	{
	  r->state = STATE_SHEET_START;
	}
      else if (XML_READER_TYPE_TEXT == r->node_type)
	{
	  ++r->sheet_index;
	  if ( r->target_sheet != NULL)
	    {
	      xmlChar *value = xmlTextReaderValue (r->xtr);
	      if ( 0 == xmlStrcmp (value, r->target_sheet))
		r->state = STATE_SHEET_FOUND;
	      free (value);
	    }
	  else if (r->target_sheet_index == r->sheet_index)
	    {
	      r->state = STATE_SHEET_FOUND;
	    }
	}
      break;
    case STATE_SHEET_FOUND:
      if (0 == xmlStrcasecmp (name, _xml("gnm:Cells"))  &&
	  XML_READER_TYPE_ELEMENT  == r->node_type)
	{
	  if (! xmlTextReaderIsEmptyElement (r->xtr))
	    r->state = STATE_CELLS_START;
	}
      else if (0 == xmlStrcasecmp (name, _xml("gnm:MaxRow"))  &&
	  XML_READER_TYPE_ELEMENT  == r->node_type)
	{
	  r->state = STATE_MAXROW;
	}
      else if (0 == xmlStrcasecmp (name, _xml("gnm:Sheet"))  &&
	  XML_READER_TYPE_END_ELEMENT  == r->node_type)
	{
	  r->state = STATE_INIT;
	}
      break;
    case STATE_MAXROW:
      if (0 == xmlStrcasecmp (name, _xml("gnm:MaxRow"))  &&
	  XML_READER_TYPE_END_ELEMENT  == r->node_type)
	{
	  r->state = STATE_SHEET_FOUND;
	}
    case STATE_CELLS_START:
      if (0 == xmlStrcasecmp (name, _xml ("gnm:Cell"))  &&
	  XML_READER_TYPE_ELEMENT  == r->node_type)
	{
	  xmlChar *attr = NULL;
	  r->state = STATE_CELL;

	  attr = xmlTextReaderGetAttribute (r->xtr, _xml ("Col"));
	  r->col =  _xmlchar_to_int (attr);
	  free (attr);

	  attr = xmlTextReaderGetAttribute (r->xtr, _xml ("Row"));
	  r->row = _xmlchar_to_int (attr);
	  free (attr);
	}
      else if (0 == xmlStrcasecmp (name, _xml("gnm:Cells"))  &&
	       XML_READER_TYPE_END_ELEMENT  == r->node_type)
	r->state = STATE_SHEET_NAME;

      break;
    case STATE_CELL:
      if (0 == xmlStrcasecmp (name, _xml("gnm:Cell"))  &&
			      XML_READER_TYPE_END_ELEMENT  == r->node_type)
	r->state = STATE_CELLS_START;
      break;
    default:
      break;
    };

  xmlFree (name);
}



/*
  Change SUGGESTION until it's a valid name that can be added to DICT.
*/
static void
devise_name (const struct dictionary *dict, struct string *name, int *x)
{
  struct string basename;
  if ( ds_is_empty (name))
    ds_init_cstr (&basename, "var");
  else
    ds_init_string (&basename, name);
  do
    {
      ds_clear (name);
      ds_put_format (name, "%s%d", ds_cstr (&basename), ++(*x));
    }
  while (NULL != dict_lookup_var (dict, ds_cstr (name)) );

  ds_destroy (&basename);
}

/*
   Mutate NAME of a variable, which is gauranteed to be valid for the
   dictionary DICT.
*/
static void
munge_name (const struct dictionary *dict, struct string *name)
{
  int x = 0;

  if (! ds_is_empty (name))
    {
      /* Change all the invalid characters to valid ones */
      char *s;

      s = ds_data (name);

      if ( !lex_is_id1 (*s))
	*s = '@';

      s++;

      while (s < ds_data (name) + ds_length (name))
	{
	  if ( !lex_is_idn (*s))
	    *s = '_';
	  s++;
	}

      assert (var_is_valid_name (ds_cstr (name), false));
    }

  while (ds_is_empty (name) || NULL != dict_lookup_var (dict, ds_cstr (name)) )
    {
      devise_name (dict, name, &x);
    }
}


/*
   Sets the VAR of case C, to the value corresponding to the xml string XV
 */
static void
convert_xml_string_to_value (struct ccase *c, const struct variable *var,
			     const xmlChar *xv)
{
  char *text;
  int n_bytes = 0;
  union value *v = case_data_rw (c, var);

  text = recode_string (CONV_UTF8_TO_PSPP, (const char *) xv, -1);

  if ( text)
    n_bytes = MIN (var_get_width (var), strlen (text));

  if ( var_is_alpha (var))
    {
      memcpy (v->s, text, n_bytes);
    }
  else
    {
      char *endptr;
      errno = 0;
      v->f = strtod (text, &endptr);
      if ( errno != 0 || endptr == text)
	v->f = SYSMIS;
    }

  free (text);
}

struct var_spec
{
  char *name;
  int width;
  xmlChar *first_value;
};

struct casereader *
gnumeric_open_reader (struct gnumeric_read_info *gri, struct dictionary **dict)
{
  int ret;
  casenumber n_cases = CASENUMBER_MAX;
  int i;
  struct var_spec *var_spec = NULL;
  int n_var_specs = 0;

  struct gnumeric_reader *r = NULL;

  gzFile gz = gzopen (gri->file_name, "r");

  if ( NULL == gz)
    {
      msg (ME, _("Error opening \"%s\" for reading as a gnumeric file: %s."),
           gri->file_name, strerror (errno));

      goto error;
    }

  r = xzalloc (sizeof *r);

  r->xtr = xmlReaderForIO ((xmlInputReadCallback) gzread, gzclose, gz,
			   NULL, NULL, 0);

  if ( r->xtr == NULL)
    goto error;

  if ( gri->cell_range )
    {
      if ( ! convert_cell_ref (gri->cell_range,
			       &r->start_col, &r->start_row,
			       &r->stop_col, &r->stop_row))
	{
	  msg (SE, _("Invalid cell range \"%s\""),
	       gri->cell_range);
	  goto error;
	}
    }
  else
    {
      r->start_col = 0;
      r->start_row = 0;
      r->stop_col = -1;
      r->stop_row = -1;
    }

  r->state = STATE_INIT;
  r->target_sheet = BAD_CAST gri->sheet_name;
  r->target_sheet_index = gri->sheet_index;
  r->row = r->col = -1;
  r->sheet_index = 0;

  /* Advance to the start of the cells for the target sheet */
  while ( (r->state != STATE_CELL || r->row < r->start_row )
	  && 1 == (ret = xmlTextReaderRead (r->xtr)))
    {
      xmlChar *value ;
      process_node (r);
      value = xmlTextReaderValue (r->xtr);

      if ( r->state == STATE_MAXROW  && r->node_type == XML_READER_TYPE_TEXT)
	{
	  n_cases = 1 + _xmlchar_to_int (value) ;
	}
      free (value);
    }


  /* If a range has been given, then  use that to calculate the number
     of cases */
  if ( gri->cell_range)
    {
      n_cases = MIN (n_cases, r->stop_row - r->start_row + 1);
    }

  if ( gri->read_names )
    {
      r->start_row++;
      n_cases --;
    }

  /* Read in the first row of cells,
     including the headers if read_names was set */
  while (
	 (( r->state == STATE_CELLS_START && r->row <= r->start_row) || r->state == STATE_CELL )
	 && (ret = xmlTextReaderRead (r->xtr))
	 )
    {
      int idx;
      process_node (r);

      if ( r->row > r->start_row ) break;

      if ( r->col < r->start_col ||
	   (r->stop_col != -1 && r->col > r->stop_col))
	continue;

      idx = r->col - r->start_col;

      if ( idx  >= n_var_specs )
	{
	  n_var_specs =  idx + 1 ;
	  var_spec = realloc (var_spec, sizeof (*var_spec) * n_var_specs);
	  var_spec [idx].name = NULL;
	  var_spec [idx].width = -1;
	  var_spec [idx].first_value = NULL;
	}

      if ( r->node_type == XML_READER_TYPE_TEXT )
	{
	  char *text ;
	  xmlChar *value = xmlTextReaderValue (r->xtr);

	  text = recode_string (CONV_UTF8_TO_PSPP, (const char *) value, -1);

	  if ( r->row < r->start_row)
	    {
	      if ( gri->read_names )
		{
		  var_spec [idx].name = strdup (text);
		}
	    }
	  else
	    {
	      var_spec [idx].first_value = xmlStrdup (value);

	      if (-1 ==  var_spec [idx].width )
		var_spec [idx].width = (gri->asw == -1) ?
		  ROUND_UP (strlen(text), MAX_SHORT_STRING) : gri->asw;
	    }

	  free (value);
	  free (text);
	}
      else if ( r->node_type == XML_READER_TYPE_ELEMENT
		&& r->state == STATE_CELL)
	{
	  if ( r->row == r->start_row )
	    {
	      xmlChar *attr =
		xmlTextReaderGetAttribute (r->xtr, _xml ("ValueType"));

	      if ( NULL == attr || 60 !=  _xmlchar_to_int (attr))
		var_spec [idx].width = 0;

	      free (attr);
	    }
	}
    }


  /* Create the dictionary and populate it */
  *dict = r->dict = dict_create ();

  r->value_cnt = 0;

  for (i = 0 ; i < n_var_specs ; ++i )
    {
      struct string name;

      /* Probably no data exists for this variable, so allocate a default width */
      if ( var_spec[i].width == -1 )
	var_spec[i].width = MAX_SHORT_STRING;

      r->value_cnt += value_cnt_from_width (var_spec[i].width);

      if (var_spec[i].name)
	ds_init_cstr (&name, var_spec[i].name);
      else
	ds_init_empty (&name);

      munge_name (r->dict, &name);


      dict_create_var (r->dict, ds_cstr (&name), var_spec[i].width);

      ds_destroy (&name);
    }

  /* Create the first case, and cache it */
  r->used_first_case = false;

  if ( n_var_specs ==  0 )
    {
      msg (MW, _("Selected sheet or range of spreadsheet \"%s\" is empty."),
           gri->file_name);
      goto error;
    }

  case_create (&r->first_case, r->value_cnt);
  memset (case_data_rw_idx (&r->first_case, 0)->s,
	  ' ', MAX_SHORT_STRING * r->value_cnt);

  for ( i = 0 ; i < n_var_specs ; ++i )
    {
      const struct variable *var = dict_get_var (r->dict, i);

      convert_xml_string_to_value (&r->first_case, var,
				   var_spec[i].first_value);
    }

  for ( i = 0 ; i < n_var_specs ; ++i )
    {
      free (var_spec[i].first_value);
      free (var_spec[i].name);
    }

  free (var_spec);

  return casereader_create_sequential
    (NULL,
     r->value_cnt,
     n_cases,
     &gnm_file_casereader_class, r);


 error:
  for ( i = 0 ; i < n_var_specs ; ++i )
    {
      free (var_spec[i].first_value);
      free (var_spec[i].name);
    }

  free (var_spec);

  gnm_file_casereader_destroy (NULL, r);

  return NULL;
};


/* Reads one case from READER's file into C.  Returns true only
   if successful. */
static bool
gnm_file_casereader_read (struct casereader *reader UNUSED, void *r_,
                          struct ccase *c)
{
  int ret = 0;

  struct gnumeric_reader *r = r_;
  int current_row = r->row;

  if ( !r->used_first_case )
    {
      *c = r->first_case;
      r->used_first_case = true;
      return true;
    }

  case_create (c, r->value_cnt);

  memset (case_data_rw_idx (c, 0)->s, ' ', MAX_SHORT_STRING * r->value_cnt);

  while ((r->state == STATE_CELL || r->state == STATE_CELLS_START )
	 && r->row == current_row && (ret = xmlTextReaderRead (r->xtr)))
    {
      process_node (r);

      if ( r->col < r->start_col || (r->stop_col != -1 &&
				     r->col > r->stop_col))
	continue;

      if ( r->col - r->start_col >= r->value_cnt)
	continue;

      if ( r->stop_row != -1 && r->row > r->stop_row)
	break;

      if ( r->node_type == XML_READER_TYPE_TEXT )
	{
	  xmlChar *value = xmlTextReaderValue (r->xtr);

	  const int idx = r->col - r->start_col;

	  const struct variable *var = dict_get_var (r->dict, idx);

	  convert_xml_string_to_value (c, var, value);

	  free (value);
	}

    }

  return (ret == 1);
}


#endif /* GNM_SUPPORT */