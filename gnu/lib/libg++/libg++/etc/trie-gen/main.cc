/* Driver routine for the minimal-prefix trie generator.

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

#include <_G_config.h>
#include <iostream.h>
#include "options.h"
#include "trie.h"

int
main (int argc, char **argv)
{
  option (argc, argv);

  Trie  trie;
  char *buf;

  for (int i = 0; cin.gets (&buf); i++)
    trie.insert (buf, cin.gcount ());

  trie.output ();
}
