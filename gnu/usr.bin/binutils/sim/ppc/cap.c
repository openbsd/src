/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _CAP_C_
#define _CAP_C_

#include "cap.h"

typedef struct _cap_mapping cap_mapping;
struct _cap_mapping {
  unsigned32 external;
  void *internal;
  cap_mapping *next;
};

struct _cap {
  int nr_mappings;
  cap_mapping *mappings;
};

INLINE_CAP\
(cap *)
cap_create(const char *key)
{
  return ZALLOC(cap);
}

INLINE_CAP\
(void)
cap_init(cap *map)
{
  cap_mapping *current_mapping = map->mappings;
  while (current_mapping != NULL) {
    cap_mapping *tbd = current_mapping;
    current_mapping = tbd->next;
    zfree(tbd);
  }
  map->nr_mappings = 0;
  map->mappings = (cap_mapping*)0;
}

INLINE_CAP\
(void *)
cap_internal(cap *db,
	     signed32 external)
{
  cap_mapping *current_map = db->mappings;
  while (current_map != NULL) {
    if (current_map->external == external)
      return current_map->internal;
    current_map = current_map->next;
  }
  return (void*)0;
}

INLINE_CAP\
(signed32)
cap_external(cap *db,
	     void *internal)
{
  cap_mapping *current_map = db->mappings;
  while (current_map != NULL) {
    if (current_map->internal == internal)
      return current_map->external;
    current_map = current_map->next;
  }
  current_map = ZALLOC(cap_mapping);
  current_map->next = db->mappings;
  current_map->internal = internal;
  db->nr_mappings += 1;
  current_map->external = db->nr_mappings;
  db->mappings = current_map;
  return current_map->external;
}

#endif
