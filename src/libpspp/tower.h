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

/* "Tower" data structure, implemented as an augmented binary
   tree.

   Imagine a tall stack of books on a table; actually, call it a
   "tower" of books because "stack" is already taken.  If you're
   careful enough and strong enough, you can pull individual
   books out of the stack, as well as insert new books between
   existing ones or at the bottom or top of the stack.

   At any given time, you can refer to a book in the tower by
   measuring the book's height above the tower in some unit,
   e.g. mm.  This isn't necessarily equivalent to the number of
   books in the tower below the book in question, like an array
   index, because the books in the stack aren't necessarily all
   the same thickness: some might be as thin as K&R and others as
   thick as _Introduction to Algorithms_.

   This is the analogy behind this data structure.  Each node in
   the data structure has a "thickness", which is actually called
   the node's "height" because "thickness" is just too awkward a
   name.  The primary way to look up nodes is by a height from
   the bottom of the tower; any height within a node retrieves
   that node, not just the distance to the bottom of the node.
   You can insert a new node between any two existing nodes, or
   at either end, which shifts up the height of all the nodes
   above it.  You can also delete any node, which shifts down the
   height of all the nodes above it. */

#ifndef LIBPSPP_TOWER_H
#define LIBPSPP_TOWER_H

#include <stdbool.h>
#include <libpspp/abt.h>

/* Returns the data structure corresponding to the given NODE,
   assuming that NODE is embedded as the given MEMBER name in
   data type STRUCT. */
#define tower_data(NODE, STRUCT, MEMBER)                            \
        ((STRUCT *) ((char *) (NODE) - offsetof (STRUCT, MEMBER)))

/* A node within a tower. */
struct tower_node 
  {
    struct abt_node abt_node;         /* ABT node. */
    unsigned long int subtree_height; /* Node's plus descendants' heights. */
    unsigned long int height;         /* Height. */
  };

/* Returns the height of a tower node. */
static inline unsigned long
tower_node_get_height (const struct tower_node *node) 
{
  return node->height;
}

/* A tower. */
struct tower 
  {
    struct abt abt;              /* Tree. */
  };

void tower_init (struct tower *);

bool tower_is_empty (const struct tower *);
unsigned long int tower_height (const struct tower *);

void tower_insert (struct tower *, unsigned long int height,
                   struct tower_node *new, struct tower_node *under);
struct tower_node *tower_delete (struct tower *, struct tower_node *);
void tower_resize (struct tower *, struct tower_node *,
                   unsigned long int new_height);
void tower_splice (struct tower *dst, struct tower_node *under,
                   struct tower *src,
                   struct tower_node *first, struct tower_node *last);

struct tower_node *tower_lookup (const struct tower *,
                                 unsigned long int level,
                                 unsigned long int *node_start);
struct tower_node *tower_first (const struct tower *);
struct tower_node *tower_next (const struct tower *,
                               const struct tower_node *);

#endif /* libpspp/tower.h */