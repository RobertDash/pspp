/* PSPP - computes sample statistics.
   Copyright (C) 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA. */

/* This is a test program for the routines defined in
   range-set.c.  This test program aims to be as comprehensive as
   possible.  With -DNDEBUG, "gcov -b" should report 100%
   coverage of lines and branches in range-set.c routines.
   (Without -DNDEBUG, branches caused by failed assertions will
   not be taken.)  "valgrind --leak-check=yes
   --show-reachable=yes" should give a clean report, both with
   and without -DNDEBUG. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libpspp/range-set.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpspp/compiler.h>
#include <libpspp/pool.h>

#include "xalloc.h"

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

/* A contiguous region. */
struct region 
  {
    unsigned long int start;    /* Start of region. */
    unsigned long int end;      /* One past the end. */
  };

/* Number of bits in an unsigned int. */
#define UINT_BIT (CHAR_BIT * sizeof (unsigned int))

/* Returns the number of contiguous 1-bits in X starting from bit
   0.
   This implementation is designed to be obviously correct, not
   to be efficient. */
static int
count_one_bits (unsigned long int x) 
{
  int count = 0;
  while (x & 1) 
    {
      count++;
      x >>= 1;
    }
  return count;
}

/* Searches the bits in PATTERN from right to left starting from
   bit OFFSET for one or more 1-bits.  If any are found, sets
   *START to the bit index of the first and *WIDTH to the number
   of contiguous 1-bits and returns true.  Otherwise, returns
   false.
   This implementation is designed to be obviously correct, not
   to be efficient. */
static bool
next_region (unsigned int pattern, unsigned int offset,
             unsigned long int *start, unsigned long int *width) 
{
  unsigned int i;

  assert (offset <= UINT_BIT);
  for (i = offset; i < UINT_BIT; i++)
    if (pattern & (1u << i)) 
      {
        *start = i;
        *width = count_one_bits (pattern >> i);
        return true;
      }
  return false;
}

/* Prints the regions in RS to stdout. */
static void UNUSED
print_regions (const struct range_set *rs)
{
  const struct range_set_node *node;

  printf ("result:");
  for (node = range_set_first (rs); node != NULL;
       node = range_set_next (rs, node)) 
    printf (" (%lu,%lu)",
            range_set_node_get_start (node), range_set_node_get_end (node));
  printf ("\n");
}

/* Checks that the regions in RS match the bits in PATTERN. */
static void
check_pattern (const struct range_set *rs, unsigned int pattern) 
{
  const struct range_set_node *node;
  unsigned long int start, width;
  int i;
  
  for (node = rand () % 2 ? range_set_first (rs) : range_set_next (rs, NULL),
         start = width = 0;
       next_region (pattern, start + width, &start, &width);
       node = range_set_next (rs, node)) 
    {
      check (node != NULL);
      check (range_set_node_get_start (node) == start);
      check (range_set_node_get_end (node) == start + width);
      check (range_set_node_get_width (node) == width);
    }
  check (node == NULL);

  for (i = 0; i < UINT_BIT; i++)
    check (range_set_contains (rs, i) == ((pattern & (1u << i)) != 0));
  check (!range_set_contains (rs,
                              UINT_BIT + rand () % (ULONG_MAX - UINT_BIT)));

  check (range_set_is_empty (rs) == (pattern == 0));
}

/* Creates and returns a range set that contains regions for the
   bits set in PATTERN. */
static struct range_set *
make_pattern (unsigned int pattern) 
{
  unsigned long int start = 0;
  unsigned long int width = 0;
  struct range_set *rs = range_set_create_pool (NULL);
  while (next_region (pattern, start + width, &start, &width))
    range_set_insert (rs, start, width);
  check_pattern (rs, pattern);
  return rs;
}

/* Returns an unsigned int with bits OFS...OFS+CNT (exclusive)
   set to 1, other bits set to 0. */
static unsigned int
bit_range (unsigned int ofs, unsigned int cnt) 
{
  assert (ofs < UINT_BIT);
  assert (cnt <= UINT_BIT);
  assert (ofs + cnt <= UINT_BIT);

  return cnt < UINT_BIT ? ((1u << cnt) - 1) << ofs : UINT_MAX;
}

/* Tests inserting all possible patterns into all possible range
   sets (up to a small maximum number of bits). */
static void
test_insert (void) 
{
  const int positions = 9;
  unsigned int init_pat;
  int i, j;
  
  for (init_pat = 0; init_pat < (1u << positions); init_pat++) 
    for (i = 0; i < positions + 1; i++)
      for (j = i; j <= positions + 1; j++)
        {
          struct range_set *rs;
          unsigned int final_pat;

          rs = make_pattern (init_pat);
          range_set_insert (rs, i, j - i);
          final_pat = init_pat | bit_range (i, j - i);
          check_pattern (rs, final_pat);
          range_set_destroy (rs);
        }
}

/* Tests deleting all possible patterns from all possible range
   sets (up to a small maximum number of bits). */
static void
test_delete (void) 
{
  const int positions = 9;
  unsigned int init_pat;
  int i, j;
  
  for (init_pat = 0; init_pat < (1u << positions); init_pat++) 
    for (i = 0; i < positions + 1; i++)
      for (j = i; j <= positions + 1; j++)
        {
          struct range_set *rs;
          unsigned int final_pat;

          rs = make_pattern (init_pat);
          range_set_delete (rs, i, j - i);
          final_pat = init_pat & ~bit_range (i, j - i);
          check_pattern (rs, final_pat);
          range_set_destroy (rs);
        }
}

/* Tests all possible allocation in all possible range sets (up
   to a small maximum number of bits). */
static void
test_allocate (void)
{
  const int positions = 9;
  unsigned int init_pat;
  int request;
  
  for (init_pat = 0; init_pat < (1u << positions); init_pat++) 
    for (request = 1; request <= positions + 1; request++)
      {
        struct range_set *rs;
        unsigned long int start, width, expect_start, expect_width;
        bool success, expect_success;
        unsigned int final_pat;
        int i;

        /* Figure out expected results. */
        expect_success = false;
        expect_start = expect_width = 0;
        final_pat = init_pat;
        for (i = 0; i < positions; i++)
          if (init_pat & (1u << i))
            {
              expect_success = true;
              expect_start = i;
              expect_width = count_one_bits (init_pat >> i);
              if (expect_width > request)
                expect_width = request;
              final_pat &= ~bit_range (expect_start, expect_width);
              break;
            }

        /* Test. */
        rs = make_pattern (init_pat);
        success = range_set_allocate (rs, request, &start, &width);
        check_pattern (rs, final_pat);
        range_set_destroy (rs);

        /* Check results. */
        check (success == expect_success);
        if (expect_success) 
          {
            check (start == expect_start);
            check (width == expect_width);
          }
      }
}

/* Tests freeing a range set through a pool. */
static void
test_pool (void) 
{
  struct pool *pool;
  struct range_set *rs;

  /* Destroy the range set, then the pool.
     Makes sure that this doesn't cause a double-free. */
  pool = pool_create ();
  rs = range_set_create_pool (pool);
  range_set_insert (rs, 1, 10);
  range_set_destroy (rs);
  pool_destroy (pool);
  
  /* Just destroy the pool.
     Makes sure that this doesn't cause a leak. */
  pool = pool_create ();
  rs = range_set_create_pool (pool);
  range_set_insert (rs, 1, 10);
  pool_destroy (pool);
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
  run_test (test_insert, "insert");
  run_test (test_delete, "delete");
  run_test (test_allocate, "allocate");
  run_test (test_pool, "pool");
  putchar ('\n');

  return 0;
}