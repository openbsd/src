/* This may look like C code, but it is really -*- C++ -*- */

/* Data and member functions for generating a minimal-prefix trie.

   Copyright (C) 1989 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)
   
   This file is part of GNU TRIE-GEN.
   
   GNU TRIE-GEN is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.
   
   GNU TRIE-GEN is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GNU trie-gen; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "compact.h"

class Trie
{
public:
  /* N is a guess on the total number of keys. */
                 Trie (int n = 1000): matrix (n), total_size (n) {
		     keys = new char *[n]; current_size = 0;
		     max_key_len = 0; max_row = 1; }
  void           insert (char *str, int len);
  void           output (void);
  void           sort (void);

private:
  Compact_Matrix matrix;           /* Dynamic table encoding the trie DFA. */
  char         **keys;             /* Dynamic resizable table to store input keys. */
  int            total_size;       /* Total size of the keyword table. */
  int            current_size;     /* Current size of the keyword table. */
  int		 max_row;	   /* Largest row in the trie so far. */
  int            max_key_len;	   /* Length of the longest keyword. */

  void           resize (int new_size);
  void           build (int hi, int lo = 0, int col = 0);
  static int     cmp (char **s1, char **s2);
};

