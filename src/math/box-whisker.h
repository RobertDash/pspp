/* PSPP - a program for statistical analysis.
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

#ifndef __MATH_BOX_WHISKER_H__
#define __MATH_BOX_WHISKER_H__

#include <stddef.h>
#include <libpspp/ll.h>
#include <libpspp/str.h>
#include "order-stats.h"

/* This module calculates the statistics typically displayed by box-plots.
   However, there's no reason not to use it for other purposes too.
 */
struct tukey_hinges;


struct outlier
{
  double value;
  struct string label;
  bool extreme;
  struct ll ll;
};


struct box_whisker
{
  struct order_stats parent;

  double hinges[3];
  double whiskers[2];

  struct ll_list outliers;

  double step;

  size_t casenumber_idx;
  const struct variable *id_var;
};

struct statistic * box_whisker_create (const struct tukey_hinges *,
					 const struct variable *, size_t);

void box_whisker_whiskers (const struct box_whisker *bw, double whiskers[2]);

void box_whisker_hinges (const struct box_whisker *bw, double hinges[2]);

const struct ll_list * box_whisker_outliers (const struct box_whisker *bw);


#endif