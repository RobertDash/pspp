/* PSPP - a program for statistical analysis.
   Copyright (C) 1997-9, 2000, 2009, 2010 Free Software Foundation, Inc.

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

#ifndef LIBPSPP_HASH_FUNCTIONS_H
#define LIBPSPP_HASH_FUNCTIONS_H 1

#include <stddef.h>

unsigned int hash_bytes (const void *, size_t, unsigned int basis);
unsigned int hash_string (const char *, unsigned int basis);
unsigned int hash_case_bytes (const void *, size_t, unsigned int basis);
unsigned int hash_case_string (const char *, unsigned int basis);
unsigned int hash_int (int, unsigned int basis);
unsigned int hash_double (double, unsigned int basis);
unsigned int hash_pointer (const void *, unsigned int basis);

#endif /* libpspp/hash-functions.h */
