/* PSPP - a program for statistical analysis.
   Copyright (C) 2007, 2008, 2009, 2010 Free Software Foundation, Inc.

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

/* This is a test program for the stringi_set_* routines defined in
   stringi-set.c.  This test program aims to be as comprehensive as possible.
   "gcov -a -b" should report almost complete coverage of lines, blocks and
   branches in stringi-set.c, except that one branch caused by hash collision
   is not exercised because our hash function has so few collisions.  "valgrind
   --leak-check=yes --show-reachable=yes" should give a clean report. */

#include <config.h>

#include "libpspp/stringi-set.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libpspp/compiler.h"
#include "libpspp/str.h"

/* Currently running test. */
static const char *test_name;

/* Exit with a failure code.
   (Place a breakpoint on this function while debugging.) */
static void
check_die (void)
{
  exit (EXIT_FAILURE);
}

/* If OK is not true, prints a message about failure on the
   current source file and the given LINE and terminates. */
static void
check_func (bool ok, int line)
{
  if (!ok)
    {
      printf ("Check failed in %s test at %s, line %d\n",
              test_name, __FILE__, line);
      check_die ();
    }
}

/* Verifies that EXPR evaluates to true.
   If not, prints a message citing the calling line number and
   terminates. */
#define check(EXPR) check_func ((EXPR), __LINE__)

/* Prints a message about memory exhaustion and exits with a
   failure code. */
static void
xalloc_die (void)
{
  printf ("virtual memory exhausted\n");
  exit (EXIT_FAILURE);
}

static void *xmalloc (size_t n) MALLOC_LIKE;
static void *xnmalloc (size_t n, size_t m) MALLOC_LIKE;
static void *xmemdup (const void *p, size_t n) MALLOC_LIKE;

/* Allocates and returns N bytes of memory. */
static void *
xmalloc (size_t n)
{
  if (n != 0)
    {
      void *p = malloc (n);
      if (p == NULL)
        xalloc_die ();

      return p;
    }
  else
    return NULL;
}

static void *
xmemdup (const void *p, size_t n)
{
  void *q = xmalloc (n);
  memcpy (q, p, n);
  return q;
}

/* Clone STRING.  */
static char *
xstrdup (const char *string)
{
  return xmemdup (string, strlen (string) + 1);
}

/* Allocates and returns N * M bytes of memory. */
static void *
xnmalloc (size_t n, size_t m)
{
  if ((size_t) -1 / m <= n)
    xalloc_die ();
  return xmalloc (n * m);
}

/* Support routines. */

enum { MAX_VALUE = 1024 };

static char *string_table[MAX_VALUE];

static const char *
make_string (int value)
{
  char **s;

  assert (value >= 0 && value < MAX_VALUE);
  s = &string_table[value];
  if (*s == NULL)
    {
      *s = xmalloc (16);
      str_format_26adic (value + 1, *s, 16);
    }
  return *s;
}

static void
free_strings (void)
{
  int i;

  for (i = 0; i < MAX_VALUE; i++)
    free (string_table[i]);
}

/* Swaps *A and *B. */
static void
swap (int *a, int *b)
{
  int t = *a;
  *a = *b;
  *b = t;
}

/* Reverses the order of the CNT integers starting at VALUES. */
static void
reverse (int *values, size_t cnt)
{
  size_t i = 0;
  size_t j = cnt;

  while (j > i)
    swap (&values[i++], &values[--j]);
}

/* Arranges the CNT elements in VALUES into the lexicographically
   next greater permutation.  Returns true if successful.
   If VALUES is already the lexicographically greatest
   permutation of its elements (i.e. ordered from greatest to
   smallest), arranges them into the lexicographically least
   permutation (i.e. ordered from smallest to largest) and
   returns false. */
static bool
next_permutation (int *values, size_t cnt)
{
  if (cnt > 0)
    {
      size_t i = cnt - 1;
      while (i != 0)
        {
          i--;
          if (values[i] < values[i + 1])
            {
              size_t j;
              for (j = cnt - 1; values[i] >= values[j]; j--)
                continue;
              swap (values + i, values + j);
              reverse (values + (i + 1), cnt - (i + 1));
              return true;
            }
        }

      reverse (values, cnt);
    }

  return false;
}

/* Returns N!. */
static unsigned int
factorial (unsigned int n)
{
  unsigned int value = 1;
  while (n > 1)
    value *= n--;
  return value;
}

/* Randomly shuffles the CNT elements in ARRAY, each of which is
   SIZE bytes in size. */
static void
random_shuffle (void *array_, size_t cnt, size_t size)
{
  char *array = array_;
  char *tmp = xmalloc (size);
  size_t i;

  for (i = 0; i < cnt; i++)
    {
      size_t j = rand () % (cnt - i) + i;
      if (i != j)
        {
          memcpy (tmp, array + j * size, size);
          memcpy (array + j * size, array + i * size, size);
          memcpy (array + i * size, tmp, size);
        }
    }

  free (tmp);
}

/* Checks that SET contains STRING. */
static void
check_set_contains (struct stringi_set *set, const char *string)
{
  struct stringi_set_node *node;

  check (stringi_set_contains (set, string));
  check (!stringi_set_insert (set, string));
  check (!stringi_set_insert_nocopy (set, xstrdup (string)));

  node = stringi_set_find_node (set, string);
  check (node != NULL);
  check (!strcasecmp (string, stringi_set_node_get_string (node)));
}

/* Checks that SET contains the CNT strings in DATA, that its structure is
   correct, and that certain operations on SET produce the expected results. */
static void
check_stringi_set (struct stringi_set *set, const int data[], size_t cnt)
{
  size_t i;

  check (stringi_set_is_empty (set) == (cnt == 0));
  check (stringi_set_count (set) == cnt);

  for (i = 0; i < cnt; i++)
    {
      const char *s;
      char copy[16];
      char *p;

      s = make_string (data[i]);
      check_set_contains (set, s);

      strcpy (copy, s);
      for (p = copy; *p != '\0'; p++)
        {
          assert (isupper (*p));
          *p = tolower (*p);
          check_set_contains (set, copy);
        }
    }

  check (!stringi_set_contains (set, "xxx"));
  check (stringi_set_find_node (set, "") == NULL);

  if (cnt == 0)
    {
      check (stringi_set_first (set) == NULL);
      free (stringi_set_get_array (set));
    }
  else
    {
      const struct stringi_set_node *node;
      char **array;
      int *data_copy;
      int left;

      array = stringi_set_get_array (set);
      data_copy = xmemdup (data, cnt * sizeof *data);
      left = cnt;
      for (node = stringi_set_first (set), i = 0; i < cnt;
           node = stringi_set_next (set, node), i++)
        {
          const char *s = stringi_set_node_get_string (node);
          size_t j;

          check (s == array[i]);

          for (j = 0; j < left; j++)
            if (!strcasecmp (s, make_string (data_copy[j])))
              {
                data_copy[j] = data_copy[--left];
                goto next;
              }
          check_die ();

        next: ;
        }
      check (node == NULL);
      free (data_copy);
      free (array);

      array = stringi_set_get_sorted_array (set);
      for (i = 0; i < cnt; i++)
        {
          if (i > 0)
            check (strcasecmp (array[i - 1], array[i]) < 0);
          check (stringi_set_contains (set, array[i]));
        }
      free (array);
    }
}

/* Inserts the CNT strings from 0 to CNT - 1 (inclusive) into a set in the
   order specified by INSERTIONS, then deletes them in the order specified by
   DELETIONS, checking the set's contents for correctness after each
   operation.  */
static void
test_insert_delete (const int insertions[],
                    const int deletions[],
                    size_t cnt)
{
  struct stringi_set set;
  size_t i;

  stringi_set_init (&set);
  check_stringi_set (&set, NULL, 0);
  for (i = 0; i < cnt; i++)
    {
      check (stringi_set_insert (&set, make_string (insertions[i])));
      check_stringi_set (&set, insertions, i + 1);
    }
  for (i = 0; i < cnt; i++)
    {
      check (stringi_set_delete (&set, make_string (deletions[i])));
      check_stringi_set (&set, deletions + i + 1, cnt - i - 1);
    }
  stringi_set_destroy (&set);
}

/* Inserts strings into a set in each possible order, then removes them in each
   possible order, up to a specified maximum size. */
static void
test_insert_any_remove_any (void)
{
  const int max_elems = 5;
  int cnt;

  for (cnt = 0; cnt <= max_elems; cnt++)
    {
      int *insertions, *deletions;
      unsigned int ins_perm_cnt;
      int i;

      insertions = xnmalloc (cnt, sizeof *insertions);
      deletions = xnmalloc (cnt, sizeof *deletions);
      for (i = 0; i < cnt; i++)
        insertions[i] = i;

      for (ins_perm_cnt = 0;
           ins_perm_cnt == 0 || next_permutation (insertions, cnt);
           ins_perm_cnt++)
        {
          unsigned int del_perm_cnt;
          int i;

          for (i = 0; i < cnt; i++)
            deletions[i] = i;

          for (del_perm_cnt = 0;
               del_perm_cnt == 0 || next_permutation (deletions, cnt);
               del_perm_cnt++)
            test_insert_delete (insertions, deletions, cnt);

          check (del_perm_cnt == factorial (cnt));
        }
      check (ins_perm_cnt == factorial (cnt));

      free (insertions);
      free (deletions);
    }
}

/* Inserts strings into a set in each possible order, then removes them in the
   same order, up to a specified maximum size. */
static void
test_insert_any_remove_same (void)
{
  const int max_elems = 7;
  int cnt;

  for (cnt = 0; cnt <= max_elems; cnt++)
    {
      int *values;
      unsigned int permutation_cnt;
      int i;

      values = xnmalloc (cnt, sizeof *values);
      for (i = 0; i < cnt; i++)
        values[i] = i;

      for (permutation_cnt = 0;
           permutation_cnt == 0 || next_permutation (values, cnt);
           permutation_cnt++)
        test_insert_delete (values, values, cnt);
      check (permutation_cnt == factorial (cnt));

      free (values);
    }
}

/* Inserts strings into a set in each possible order, then
   removes them in reverse order, up to a specified maximum
   size. */
static void
test_insert_any_remove_reverse (void)
{
  const int max_elems = 7;
  int cnt;

  for (cnt = 0; cnt <= max_elems; cnt++)
    {
      int *insertions, *deletions;
      unsigned int permutation_cnt;
      int i;

      insertions = xnmalloc (cnt, sizeof *insertions);
      deletions = xnmalloc (cnt, sizeof *deletions);
      for (i = 0; i < cnt; i++)
        insertions[i] = i;

      for (permutation_cnt = 0;
           permutation_cnt == 0 || next_permutation (insertions, cnt);
           permutation_cnt++)
        {
          memcpy (deletions, insertions, sizeof *insertions * cnt);
          reverse (deletions, cnt);

          test_insert_delete (insertions, deletions, cnt);
        }
      check (permutation_cnt == factorial (cnt));

      free (insertions);
      free (deletions);
    }
}

/* Inserts and removes strings in a set, in random order. */
static void
test_random_sequence (void)
{
  const int max_elems = 64;
  const int max_trials = 8;
  int cnt;

  for (cnt = 0; cnt <= max_elems; cnt += 2)
    {
      int *insertions, *deletions;
      int trial;
      int i;

      insertions = xnmalloc (cnt, sizeof *insertions);
      deletions = xnmalloc (cnt, sizeof *deletions);
      for (i = 0; i < cnt; i++)
        insertions[i] = i;
      for (i = 0; i < cnt; i++)
        deletions[i] = i;

      for (trial = 0; trial < max_trials; trial++)
        {
          random_shuffle (insertions, cnt, sizeof *insertions);
          random_shuffle (deletions, cnt, sizeof *deletions);

          test_insert_delete (insertions, deletions, cnt);
        }

      free (insertions);
      free (deletions);
    }
}

/* Inserts strings into a set in ascending order, then delete in ascending
   order. */
static void
test_insert_ordered (void)
{
  const int max_elems = 64;
  int *values;
  struct stringi_set set;
  int i;

  stringi_set_init (&set);
  values = xnmalloc (max_elems, sizeof *values);
  for (i = 0; i < max_elems; i++)
    {
      values[i] = i;
      stringi_set_insert_nocopy (&set, xstrdup (make_string (i)));
      check_stringi_set (&set, values, i + 1);
    }
  for (i = 0; i < max_elems; i++)
    {
      stringi_set_delete (&set, make_string (i));
      check_stringi_set (&set, values + i + 1, max_elems - i - 1);
    }
  stringi_set_destroy (&set);
  free (values);
}

static void
test_boolean_ops (void (*function)(struct stringi_set *a, struct stringi_set *b,
                                   unsigned int *a_pat, unsigned int *b_pat))
{
  enum { MAX_STRINGS = 7 };
  unsigned int a_pat, b_pat;

  for (a_pat = 0; a_pat < (1u << MAX_STRINGS); a_pat++)
    for (b_pat = 0; b_pat < (1u << MAX_STRINGS); b_pat++)
      {
        unsigned int new_a_pat = a_pat;
        unsigned int new_b_pat = b_pat;
        struct stringi_set a, b;
        int a_strings[MAX_STRINGS], b_strings[MAX_STRINGS];
        size_t i, n_a, n_b;

        stringi_set_init (&a);
        stringi_set_init (&b);
        for (i = 0; i < MAX_STRINGS; i++)
          {
            if (a_pat & (1u << i))
              stringi_set_insert (&a, make_string (i));
            if (b_pat & (1u << i))
              stringi_set_insert (&b, make_string (i));
          }

        function (&a, &b, &new_a_pat, &new_b_pat);

        n_a = n_b = 0;
        for (i = 0; i < MAX_STRINGS; i++)
          {
            if (new_a_pat & (1u << i))
              a_strings[n_a++] = i;
            if (new_b_pat & (1u << i))
              b_strings[n_b++] = i;
          }
        check_stringi_set (&a, a_strings, n_a);
        check_stringi_set (&b, b_strings, n_b);
        stringi_set_destroy (&a);
        stringi_set_destroy (&b);
      }
}

static void
union_cb (struct stringi_set *a, struct stringi_set *b,
          unsigned int *a_pat, unsigned int *b_pat)
{
  stringi_set_union (a, b);
  *a_pat |= *b_pat;
}

static void
test_union (void)
{
  test_boolean_ops (union_cb);
}

static void
union_and_intersection_cb (struct stringi_set *a, struct stringi_set *b,
                           unsigned int *a_pat, unsigned int *b_pat)
{
  unsigned int orig_a_pat = *a_pat;
  unsigned int orig_b_pat = *b_pat;

  stringi_set_union_and_intersection (a, b);
  *a_pat = orig_a_pat | orig_b_pat;
  *b_pat = orig_a_pat & orig_b_pat;
}

static void
test_union_and_intersection (void)
{
  test_boolean_ops (union_and_intersection_cb);
}

static void
intersect_cb (struct stringi_set *a, struct stringi_set *b,
              unsigned int *a_pat, unsigned int *b_pat)
{
  stringi_set_intersect (a, b);
  *a_pat &= *b_pat;
}

static void
test_intersect (void)
{
  test_boolean_ops (intersect_cb);
}

static void
subtract_cb (struct stringi_set *a, struct stringi_set *b,
              unsigned int *a_pat, unsigned int *b_pat)
{
  stringi_set_subtract (a, b);
  *a_pat &= ~*b_pat;
}

static void
test_subtract (void)
{
  test_boolean_ops (subtract_cb);
}

static void
swap_cb (struct stringi_set *a, struct stringi_set *b,
         unsigned int *a_pat, unsigned int *b_pat)
{
  unsigned int tmp;
  stringi_set_swap (a, b);
  tmp = *a_pat;
  *a_pat = *b_pat;
  *b_pat = tmp;
}

static void
test_swap (void)
{
  test_boolean_ops (swap_cb);
}

static void
clear_cb (struct stringi_set *a, struct stringi_set *b UNUSED,
         unsigned int *a_pat, unsigned int *b_pat UNUSED)
{
  stringi_set_clear (a);
  *a_pat = 0;
}

static void
test_clear (void)
{
  test_boolean_ops (clear_cb);
}

static void
clone_cb (struct stringi_set *a, struct stringi_set *b,
         unsigned int *a_pat, unsigned int *b_pat)
{
  stringi_set_destroy (a);
  stringi_set_clone (a, b);
  *a_pat = *b_pat;
}

static void
test_clone (void)
{
  test_boolean_ops (clone_cb);
}

static void
test_destroy_null (void)
{
  stringi_set_destroy (NULL);
}

/* Main program. */

/* Runs TEST_FUNCTION and prints a message about NAME. */
static void
run_test (void (*test_function) (void), const char *name)
{
  test_name = name;
  putchar ('.');
  fflush (stdout);
  test_function ();
}

int
main (void)
{
  run_test (test_insert_any_remove_any, "insert any order, delete any order");
  run_test (test_insert_any_remove_same,
            "insert any order, delete same order");
  run_test (test_insert_any_remove_reverse,
            "insert any order, delete reverse order");
  run_test (test_random_sequence, "insert and delete in random sequence");
  run_test (test_insert_ordered, "insert in ascending order");
  run_test (test_union, "union");
  run_test (test_union_and_intersection, "union and intersection");
  run_test (test_intersect, "intersect");
  run_test (test_subtract, "subtract");
  run_test (test_swap, "swap");
  run_test (test_clear, "clear");
  run_test (test_clone, "clone");
  run_test (test_destroy_null, "destroying null table");

  putchar ('\n');

  free_strings ();

  return 0;
}