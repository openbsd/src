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


#include "misc.h"
#include "lf.h"
#include "table.h"

#include "filter.h"

#include "ld-decode.h"
#include "ld-cache.h"
#include "ld-insn.h"

#include "igen.h"

#include "gen-semantics.h"
#include "gen-idecode.h"
#include "gen-icache.h"



static void
print_icache_function_header(lf *file,
			     const char *basename,
			     insn_bits *expanded_bits,
			     int is_function_definition)
{
  lf_printf(file, "\n");
  lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "EXTERN_ICACHE", " ");
  print_function_name(file,
		      basename,
		      expanded_bits,
		      function_name_prefix_icache);
  lf_printf(file, "\n(%s)", ICACHE_FUNCTION_FORMAL); 
  if (!is_function_definition)
    lf_printf(file, ";");
  lf_printf(file, "\n");
}


void
print_icache_declaration(insn_table *entry,
			 lf *file,
			 void *data,
			 insn *instruction,
			 int depth)
{
  if (generate_expanded_instructions) {
    ASSERT(entry->nr_insn == 1);
    print_icache_function_header(file,
				 entry->insns->file_entry->fields[insn_name],
				 entry->expanded_bits,
				 0/* is not function definition */);
  }
  else {
    print_icache_function_header(file,
				 instruction->file_entry->fields[insn_name],
				 NULL,
				 0/* is not function definition */);
  }
}



static void
print_icache_extraction(lf *file,
			insn *instruction,
			char *field_name,
			char *field_type,
			char *field_expression,
			const char *file_name,
			int line_nr,
			insn_field *cur_field,
			insn_bits *bits,
			int use_defines,
			int get_value_from_cache,
			int put_value_in_cache)
{
  ASSERT(field_name != NULL);
  if (use_defines && put_value_in_cache) {
    /* We've finished with the value - destory it */
    lf_indent_suppress(file);
    lf_printf(file, "#undef %s\n", field_name);
    return;
  }
  else if (use_defines && get_value_from_cache) {
      lf_indent_suppress(file);
      lf_printf(file, "#define %s ", field_name);
  }
  else {
    if (file_name != NULL)
      lf_print__external_reference(file, line_nr, file_name);
    lf_printf(file, "%s const %s __attribute__((__unused__)) = ",
	      field_type == NULL ? "unsigned" : field_type,
	      field_name);
  }

  if (bits != NULL
      && ((bits->opcode->is_boolean
	   && bits->value == 0)
	  || !bits->opcode->is_boolean)
      && strcmp(field_name, cur_field->val_string) == 0) {
    /* The field has been made constant (as a result of expanding
       instructions or similar) - define a constant variable with the
       corresponding value. */
    ASSERT(bits->field == cur_field);
    ASSERT(field_type == NULL);
    if (bits->opcode->is_boolean)
      lf_printf(file, "%d", bits->opcode->boolean_constant);
    else if (bits->opcode->last < bits->field->last)
      lf_printf(file, "%d",
		bits->value << (bits->field->last - bits->opcode->last));
    else
      lf_printf(file, "%d", bits->value);
  }
  else {
    /* put the field in the local variable, possibly also enter it
       into the cache */
    /* getting it from the cache */
    if (get_value_from_cache || put_value_in_cache) {
      lf_printf(file, "cache_entry->crack.%s.%s",
		instruction->file_entry->fields[insn_form],
		field_name);
      if (put_value_in_cache) /* also put it in the cache? */
	lf_printf(file, " = ");
    }
    if (!get_value_from_cache) {
      if (cur_field != NULL && strcmp(field_name, cur_field->val_string) == 0)
	lf_printf(file, "EXTRACTED32(instruction, %d, %d)",
		  i2target(hi_bit_nr, cur_field->first),
		  i2target(hi_bit_nr, cur_field->last));
      else if (field_expression != NULL)
	lf_printf(file, "%s", field_expression);
      else
	lf_printf(file, "eval_%s", field_name);
    }
  }

  if (use_defines && get_value_from_cache)
    lf_printf(file, "\n");
  else
    lf_printf(file, ";\n");
}


void
print_icache_body(lf *file,
		  insn *instruction,
		  insn_bits *expanded_bits,
		  cache_table *cache_rules,
		  int use_defines,
		  int get_value_from_cache,
		  int put_value_in_cache)
{
  insn_field *cur_field;
  
  /* extract instruction fields */
  lf_printf(file, "/* extraction: %s defines=%d get-value=%d put-value=%d */\n",
	    instruction->file_entry->fields[insn_format],
	    use_defines, get_value_from_cache, put_value_in_cache);
  
  for (cur_field = instruction->fields->first;
       cur_field->first < insn_bit_size;
       cur_field = cur_field->next) {
    if (cur_field->is_string) {
      insn_bits *bits;
      int found_rule = 0;
      /* find any corresponding value */
      for (bits = expanded_bits;
	   bits != NULL;
	   bits = bits->last) {
	if (bits->field == cur_field)
	  break;
      }
      /* try the cache rule table for what to do */
      if (get_value_from_cache || put_value_in_cache) {      
	cache_table *cache_rule;
	for (cache_rule = cache_rules;
	     cache_rule != NULL;
	     cache_rule = cache_rule->next) {
	  if (strcmp(cur_field->val_string, cache_rule->old_name) == 0) {
	    found_rule = 1;
	    if (cache_rule->type == compute_value
		&& put_value_in_cache
		&& !use_defines)
	      print_icache_extraction(file,
				      instruction,
				      cache_rule->new_name,
				      cache_rule->type_def,
				      cache_rule->expression,
				      cache_rule->file_entry->file_name,
				      cache_rule->file_entry->line_nr,
				      cur_field,
				      bits,
				      0 /*use_defines*/,
				      0 /*get-value-from-cache*/,
				      0 /*put-value-in-cache*/);
	    else if (cache_rule->type == cache_value)
	      print_icache_extraction(file,
				      instruction,
				      cache_rule->new_name,
				      cache_rule->type_def,
				      cache_rule->expression,
				      cache_rule->file_entry->file_name,
				      cache_rule->file_entry->line_nr,
				      cur_field,
				      bits,
				      use_defines,
				      get_value_from_cache,
				      put_value_in_cache);
	  }
	}
      }
      if (found_rule == 0)
	print_icache_extraction(file,
				instruction,
				cur_field->val_string,
				0,
				0,
				instruction->file_entry->file_name,
				instruction->file_entry->line_nr,
				cur_field,
				bits,
				use_defines,
				get_value_from_cache,
				put_value_in_cache);
      /* if any (XXX == 0), output a corresponding test */
      if (instruction->file_entry->annex != NULL) {
	char *field_name = cur_field->val_string;
	char *is_0_ptr = instruction->file_entry->annex;
	int field_len = strlen(field_name);
	if (strlen(is_0_ptr) >= (strlen("_is_0") + field_len)) {
	  is_0_ptr += field_len;
	  while ((is_0_ptr = strstr(is_0_ptr, "_is_0")) != NULL) {
	    if (strncmp(is_0_ptr - field_len, field_name, field_len) == 0
		&& !isalpha(is_0_ptr[ - field_len - 1])) {
	      if (!use_defines || (use_defines && get_value_from_cache)) {
		if (use_defines) {
		  lf_indent_suppress(file);
		  lf_printf(file, "#define %s_is_0 ", field_name);
		}
		else {
		  table_entry_print_cpp_line_nr(file, instruction->file_entry);
		  lf_printf(file, "const unsigned %s_is_0 __attribute__((__unused__)) = ",
			    field_name);
		}
		if (bits != NULL)
		  lf_printf(file, "(%d == 0)", bits->value);
		else
		  lf_printf(file, "(%s == 0)", field_name);
		if (use_defines)
		  lf_printf(file, "\n");
		else
		  lf_printf(file, ";\n");
	      }
	      else if (use_defines && put_value_in_cache) {
		lf_indent_suppress(file);
		lf_printf(file, "#undef %s_is_0\n", field_name);
	      }
	      break;
	    }
	    is_0_ptr += strlen("_is_0");
	  }
	}
      }
      /* any thing else ... */
    }
  }

  if ((code & generate_with_insn_in_icache)) {
    print_icache_extraction(file,
			    instruction,
			    "insn",
			    "instruction_word",
			    "instruction",
			    NULL, 0,
			    NULL, NULL,
			    use_defines,
			    get_value_from_cache,
			    put_value_in_cache);
  }

  lf_print__internal_reference(file);
}



typedef struct _icache_tree icache_tree;
struct _icache_tree {
  char *name;
  icache_tree *next;
  icache_tree *children;
};

static icache_tree *
icache_tree_insert(icache_tree *tree,
		   char *name)
{
  icache_tree *new_tree;
  /* find it */
  icache_tree **ptr_to_cur_tree = &tree->children;
  icache_tree *cur_tree = *ptr_to_cur_tree;
  while (cur_tree != NULL
	 && strcmp(cur_tree->name, name) < 0) {
    ptr_to_cur_tree = &cur_tree->next;
    cur_tree = *ptr_to_cur_tree;
  }
  ASSERT(cur_tree == NULL
	 || strcmp(cur_tree->name, name) >= 0);
  /* already in the tree */
  if (cur_tree != NULL
      && strcmp(cur_tree->name, name) == 0)
    return cur_tree;
  /* missing, insert it */
  ASSERT(cur_tree == NULL
	 || strcmp(cur_tree->name, name) > 0);
  new_tree = ZALLOC(icache_tree);
  new_tree->name = name;
  new_tree->next = cur_tree;
  *ptr_to_cur_tree = new_tree;
  return new_tree;
}


static icache_tree *
insn_table_cache_fields(insn_table *table)
{
  icache_tree *tree = ZALLOC(icache_tree);
  insn *instruction;
  for (instruction = table->insns;
       instruction != NULL;
       instruction = instruction->next) {
    insn_field *field;
    icache_tree *form =
      icache_tree_insert(tree,
			 instruction->file_entry->fields[insn_form]);
    for (field = instruction->fields->first;
	 field != NULL;
	 field = field->next) {
      if (field->is_string)
	icache_tree_insert(form, field->val_string);
    }
  }
  return tree;
}



extern void
print_icache_struct(insn_table *instructions,
		    cache_table *cache_rules,
		    lf *file)
{
  icache_tree *tree = insn_table_cache_fields(instructions);
  
  lf_printf(file, "#define WITH_IDECODE_CACHE_SIZE %d\n",
	    (code & generate_with_icache) ? icache_size : 0);
  lf_printf(file, "\n");
  
  /* create an instruction cache if being used */
  if ((code & generate_with_icache)) {
    icache_tree *form;
    lf_printf(file, "typedef struct _idecode_cache {\n");
    lf_printf(file, "  unsigned_word address;\n");
    lf_printf(file, "  void *semantic;\n");
    lf_printf(file, "  union {\n");
    for (form = tree->children;
	 form != NULL;
	 form = form->next) {
      icache_tree *field;
      lf_printf(file, "    struct {\n");
      if (code & generate_with_insn_in_icache)
	lf_printf(file, "      instruction_word insn;\n");
      for (field = form->children;
	   field != NULL;
	   field = field->next) {
	cache_table *cache_rule;
	int found_rule = 0;
	for (cache_rule = cache_rules;
	     cache_rule != NULL;
	     cache_rule = cache_rule->next) {
	  if (strcmp(field->name, cache_rule->old_name) == 0) {
	    found_rule = 1;
	    if (cache_rule->new_name != NULL)
	      lf_printf(file, "      %s %s; /* %s */\n",
			(cache_rule->type_def == NULL
			 ? "unsigned" 
			 : cache_rule->type_def),
			cache_rule->new_name,
			cache_rule->old_name);
	  }
	}
	if (!found_rule)
	  lf_printf(file, "      unsigned %s;\n", field->name);
      }
      lf_printf(file, "    } %s;\n", form->name);
    }
    lf_printf(file, "  } crack;\n");
    lf_printf(file, "} idecode_cache;\n");
  }
  else {
    /* alernativly, since no cache, #define the fields to be
       extractions from the instruction variable.  Emit a dummy
       definition for idecode_cache to allow model_issue to not
       be #ifdefed at the call level */
    cache_table *cache_rule;
    lf_printf(file, "\n");
    lf_printf(file, "typedef void idecode_cache;\n");
    lf_printf(file, "\n");
    for (cache_rule = cache_rules;
	 cache_rule != NULL;
	 cache_rule = cache_rule->next) {
      if (cache_rule->expression != NULL
	  && strlen(cache_rule->expression) > 0)
	lf_printf(file, "#define %s %s\n",
		  cache_rule->new_name, cache_rule->expression);
    }
  }
}



static void
print_icache_function(lf *file,
		      insn *instruction,
		      insn_bits *expanded_bits,
		      opcode_field *opcodes,
		      cache_table *cache_rules)
{
  int indent;

  /* generate code to enter decoded instruction into the icache */
  lf_printf(file, "\n");
  lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "EXTERN_ICACHE", "\n");
  indent = print_function_name(file,
			       instruction->file_entry->fields[insn_name],
			       expanded_bits,
			       function_name_prefix_icache);
  lf_indent(file, +indent);
  lf_printf(file, "(%s)\n", ICACHE_FUNCTION_FORMAL);
  lf_indent(file, -indent);
  
  /* function header */
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  
  print_my_defines(file, expanded_bits, instruction->file_entry);
  print_itrace(file, instruction->file_entry, 1/*putting-value-in-cache*/);
  
  print_idecode_validate(file, instruction, opcodes);
  
  lf_printf(file, "\n");
  lf_printf(file, "{\n");
  lf_indent(file, +2);
  if ((code & generate_with_semantic_icache))
    lf_printf(file, "unsigned_word nia;\n");
  print_icache_body(file,
		    instruction,
		    expanded_bits,
		    cache_rules,
		    0/*use_defines*/,
		    0/*get_value_from_cache*/,
		    1/*put_value_in_cache*/);
  
  lf_printf(file, "cache_entry->address = cia;\n");
  lf_printf(file, "cache_entry->semantic = ");
  print_function_name(file,
		      instruction->file_entry->fields[insn_name],
		      expanded_bits,
		      function_name_prefix_semantics);
  lf_printf(file, ";\n");
  lf_printf(file, "\n");

  if ((code & generate_with_semantic_icache)) {
    lf_printf(file, "/* semantic routine */\n");
    print_semantic_body(file,
			instruction,
			expanded_bits,
			opcodes);
    lf_printf(file, "return nia;\n");
  }
  
  if (!(code & generate_with_semantic_icache)) {
    lf_printf(file, "/* return the function proper */\n");
    lf_printf(file, "return ");
    print_function_name(file,
			instruction->file_entry->fields[insn_name],
			expanded_bits,
			function_name_prefix_semantics);
    lf_printf(file, ";\n");
  }
  
  lf_indent(file, -2);
  lf_printf(file, "}\n");
  lf_indent(file, -2);
  lf_printf(file, "}\n");
}


void
print_icache_definition(insn_table *entry,
			lf *file,
			void *data,
			insn *instruction,
			int depth)
{
  cache_table *cache_rules = (cache_table*)data;
  if (generate_expanded_instructions) {
    ASSERT(entry->nr_insn == 1
	   && entry->opcode == NULL
	   && entry->parent != NULL
	   && entry->parent->opcode != NULL);
    ASSERT(entry->nr_insn == 1
	   && entry->opcode == NULL
	   && entry->parent != NULL
	   && entry->parent->opcode != NULL
	   && entry->parent->opcode_rule != NULL);
    print_icache_function(file,
			  entry->insns,
			  entry->expanded_bits,
			  entry->opcode,
			  cache_rules);
  }
  else {
    print_icache_function(file,
			  instruction,
			  NULL,
			  NULL,
			  cache_rules);
  }
}



void
print_icache_internal_function_declaration(insn_table *table,
					   lf *file,
					   void *data,
					   table_entry *function)
{
  ASSERT((code & generate_with_icache) != 0);
  if (it_is("internal", function->fields[insn_flags])) {
    lf_printf(file, "\n");
    lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "INLINE_ICACHE",
			   "\n");
    print_function_name(file,
			function->fields[insn_name],
			NULL,
			function_name_prefix_icache);
    lf_printf(file, "\n(%s);\n", ICACHE_FUNCTION_FORMAL);
  }
}


void
print_icache_internal_function_definition(insn_table *table,
					  lf *file,
					  void *data,
					  table_entry *function)
{
  ASSERT((code & generate_with_icache) != 0);
  if (it_is("internal", function->fields[insn_flags])) {
    lf_printf(file, "\n");
    lf_print_function_type(file, ICACHE_FUNCTION_TYPE, "INLINE_ICACHE",
			   "\n");
    print_function_name(file,
			function->fields[insn_name],
			NULL,
			function_name_prefix_icache);
    lf_printf(file, "\n(%s)\n", ICACHE_FUNCTION_FORMAL);
    lf_printf(file, "{\n");
    lf_indent(file, +2);
    lf_printf(file, "/* semantic routine */\n");
    table_entry_print_cpp_line_nr(file, function);
    if ((code & generate_with_semantic_icache)) {
      lf_print__c_code(file, function->annex);
      lf_printf(file, "error(\"Internal function must longjump\\n\");\n");
      lf_printf(file, "return 0;\n");
    }
    else {
      lf_printf(file, "return ");
      print_function_name(file,
			  function->fields[insn_name],
			  NULL,
			  function_name_prefix_semantics);
      lf_printf(file, ";\n");
    }
    
    lf_print__internal_reference(file);
    lf_indent(file, -2);
    lf_printf(file, "}\n");
  }
}
