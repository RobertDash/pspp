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

#ifndef FORMAT_GUESSER_H
#define FORMAT_GUESSER_H 1

struct fmt_spec;
struct substring;

struct fmt_guesser *fmt_guesser_create (void);
void fmt_guesser_destroy (struct fmt_guesser *);
void fmt_guesser_clear (struct fmt_guesser *);
void fmt_guesser_add (struct fmt_guesser *, struct substring);
void fmt_guesser_guess (struct fmt_guesser *, struct fmt_spec *);

#endif /* format-guesser.h */