/*  This file is part of the program psim.

    Copyright (C) 1994,1995,1996, Andrew Cagney <cagney@highland.com.au>

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


#include "misc.h"
#include "lf.h"
#include "table.h"
#include "ld-cache.h"

#ifndef NULL
#define NULL 0
#endif


enum {
  ca_type,
  ca_old_name,
  ca_new_name,
  ca_type_def,
  ca_expression,
  nr_cache_rule_fields,
};

static const name_map cache_type_map[] = {
  { "cache", cache_value },
  { "compute", compute_value },
  { NULL, 0 },
};


cache_table *
load_cache_table(char *file_name,
		 int hi_bit_nr)
{
  table *file = table_open(file_name, nr_cache_rule_fields, 0);
  table_entry *entry;
  cache_table *table = NULL;
  cache_table **curr_rule = &table;
  while ((entry = table_entry_read(file)) != NULL) {
    cache_table *new_rule = ZALLOC(cache_table);
    new_rule->type = name2i(entry->fields[ca_type], cache_type_map);
    new_rule->old_name = entry->fields[ca_old_name];
    new_rule->new_name = entry->fields[ca_new_name];
    new_rule->type_def = (strlen(entry->fields[ca_type_def])
			  ? entry->fields[ca_type_def]
			  : NULL);
    new_rule->expression = (strlen(entry->fields[ca_expression]) > 0
			    ? entry->fields[ca_expression]
			    : NULL);
    new_rule->file_entry = entry;
    *curr_rule = new_rule;
    curr_rule = &new_rule->next;
  }
  return table;
}



#ifdef MAIN

static void
dump_cache_rule(cache_table* rule,
		int indent)
{
  dumpf(indent, "((cache_table*)0x%x\n", rule);
  dumpf(indent, " (type %s)\n", i2name(rule->type, cache_type_map));
  dumpf(indent, " (old_name \"%s\")\n", rule->old_name);
  dumpf(indent, " (new_name \"%s\")\n", rule->new_name);
  dumpf(indent, " (type-def \"%s\")\n", rule->type_def);
  dumpf(indent, " (expression \"%s\")\n", rule->expression);
  dumpf(indent, " (next 0x%x)\n", rule->next);
  dumpf(indent, " )\n");
}


static void
dump_cache_rules(cache_table* rule,
		 int indent)
{
  while (rule) {
    dump_cache_rule(rule, indent);
    rule = rule->next;
  }
}


int
main(int argc, char **argv)
{
  cache_table *rules;
  if (argc != 3)
    error("Usage: cache <cache-file> <hi-bit-nr>\n");
  rules = load_cache_table(argv[1], a2i(argv[2]));
  dump_cache_rules(rules, 0);
  return 0;
}
#endif
