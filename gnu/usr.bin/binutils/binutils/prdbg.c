/* prdbg.c -- Print out generic debugging information.
   Copyright 1995, 1996, 2002 Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@cygnus.com>.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This file prints out the generic debugging information, by
   supplying a set of routines to debug_write.  */

#include <stdio.h>
#include <assert.h>

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "debug.h"
#include "budbg.h"

/* This is the structure we use as a handle for these routines.  */

struct pr_handle
{
  /* File to print information to.  */
  FILE *f;
  /* Current indentation level.  */
  unsigned int indent;
  /* Type stack.  */
  struct pr_stack *stack;
  /* Parameter number we are about to output.  */
  int parameter;
};

/* The type stack.  */

struct pr_stack
{
  /* Next element on the stack.  */
  struct pr_stack *next;
  /* This element.  */
  char *type;
  /* Current visibility of fields if this is a class.  */
  enum debug_visibility visibility;
  /* Name of the current method we are handling.  */
  const char *method;
};

static void indent
  PARAMS ((struct pr_handle *));
static bfd_boolean push_type
  PARAMS ((struct pr_handle *, const char *));
static bfd_boolean prepend_type
  PARAMS ((struct pr_handle *, const char *));
static bfd_boolean append_type
  PARAMS ((struct pr_handle *, const char *));
static bfd_boolean substitute_type
  PARAMS ((struct pr_handle *, const char *));
static bfd_boolean indent_type
  PARAMS ((struct pr_handle *));
static char *pop_type
  PARAMS ((struct pr_handle *));
static void print_vma
  PARAMS ((bfd_vma, char *, bfd_boolean, bfd_boolean));
static bfd_boolean pr_fix_visibility
  PARAMS ((struct pr_handle *, enum debug_visibility));
static bfd_boolean pr_start_compilation_unit
  PARAMS ((PTR, const char *));
static bfd_boolean pr_start_source
  PARAMS ((PTR, const char *));
static bfd_boolean pr_empty_type
  PARAMS ((PTR));
static bfd_boolean pr_void_type
  PARAMS ((PTR));
static bfd_boolean pr_int_type
  PARAMS ((PTR, unsigned int, bfd_boolean));
static bfd_boolean pr_float_type
  PARAMS ((PTR, unsigned int));
static bfd_boolean pr_complex_type
  PARAMS ((PTR, unsigned int));
static bfd_boolean pr_bool_type
  PARAMS ((PTR, unsigned int));
static bfd_boolean pr_enum_type
  PARAMS ((PTR, const char *, const char **, bfd_signed_vma *));
static bfd_boolean pr_pointer_type
  PARAMS ((PTR));
static bfd_boolean pr_function_type
  PARAMS ((PTR, int, bfd_boolean));
static bfd_boolean pr_reference_type
  PARAMS ((PTR));
static bfd_boolean pr_range_type
  PARAMS ((PTR, bfd_signed_vma, bfd_signed_vma));
static bfd_boolean pr_array_type
  PARAMS ((PTR, bfd_signed_vma, bfd_signed_vma, bfd_boolean));
static bfd_boolean pr_set_type
  PARAMS ((PTR, bfd_boolean));
static bfd_boolean pr_offset_type
  PARAMS ((PTR));
static bfd_boolean pr_method_type
  PARAMS ((PTR, bfd_boolean, int, bfd_boolean));
static bfd_boolean pr_const_type
  PARAMS ((PTR));
static bfd_boolean pr_volatile_type
  PARAMS ((PTR));
static bfd_boolean pr_start_struct_type
  PARAMS ((PTR, const char *, unsigned int, bfd_boolean, unsigned int));
static bfd_boolean pr_struct_field
  PARAMS ((PTR, const char *, bfd_vma, bfd_vma, enum debug_visibility));
static bfd_boolean pr_end_struct_type
  PARAMS ((PTR));
static bfd_boolean pr_start_class_type
  PARAMS ((PTR, const char *, unsigned int, bfd_boolean, unsigned int,
	   bfd_boolean, bfd_boolean));
static bfd_boolean pr_class_static_member
  PARAMS ((PTR, const char *, const char *, enum debug_visibility));
static bfd_boolean pr_class_baseclass
  PARAMS ((PTR, bfd_vma, bfd_boolean, enum debug_visibility));
static bfd_boolean pr_class_start_method
  PARAMS ((PTR, const char *));
static bfd_boolean pr_class_method_variant
  PARAMS ((PTR, const char *, enum debug_visibility, bfd_boolean, bfd_boolean,
	   bfd_vma, bfd_boolean));
static bfd_boolean pr_class_static_method_variant
  PARAMS ((PTR, const char *, enum debug_visibility, bfd_boolean,
	   bfd_boolean));
static bfd_boolean pr_class_end_method
  PARAMS ((PTR));
static bfd_boolean pr_end_class_type
  PARAMS ((PTR));
static bfd_boolean pr_typedef_type
  PARAMS ((PTR, const char *));
static bfd_boolean pr_tag_type
  PARAMS ((PTR, const char *, unsigned int, enum debug_type_kind));
static bfd_boolean pr_typdef
  PARAMS ((PTR, const char *));
static bfd_boolean pr_tag
  PARAMS ((PTR, const char *));
static bfd_boolean pr_int_constant
  PARAMS ((PTR, const char *, bfd_vma));
static bfd_boolean pr_float_constant
  PARAMS ((PTR, const char *, double));
static bfd_boolean pr_typed_constant
  PARAMS ((PTR, const char *, bfd_vma));
static bfd_boolean pr_variable
  PARAMS ((PTR, const char *, enum debug_var_kind, bfd_vma));
static bfd_boolean pr_start_function
  PARAMS ((PTR, const char *, bfd_boolean));
static bfd_boolean pr_function_parameter
  PARAMS ((PTR, const char *, enum debug_parm_kind, bfd_vma));
static bfd_boolean pr_start_block
  PARAMS ((PTR, bfd_vma));
static bfd_boolean pr_end_block
  PARAMS ((PTR, bfd_vma));
static bfd_boolean pr_end_function
  PARAMS ((PTR));
static bfd_boolean pr_lineno
  PARAMS ((PTR, const char *, unsigned long, bfd_vma));

static const struct debug_write_fns pr_fns =
{
  pr_start_compilation_unit,
  pr_start_source,
  pr_empty_type,
  pr_void_type,
  pr_int_type,
  pr_float_type,
  pr_complex_type,
  pr_bool_type,
  pr_enum_type,
  pr_pointer_type,
  pr_function_type,
  pr_reference_type,
  pr_range_type,
  pr_array_type,
  pr_set_type,
  pr_offset_type,
  pr_method_type,
  pr_const_type,
  pr_volatile_type,
  pr_start_struct_type,
  pr_struct_field,
  pr_end_struct_type,
  pr_start_class_type,
  pr_class_static_member,
  pr_class_baseclass,
  pr_class_start_method,
  pr_class_method_variant,
  pr_class_static_method_variant,
  pr_class_end_method,
  pr_end_class_type,
  pr_typedef_type,
  pr_tag_type,
  pr_typdef,
  pr_tag,
  pr_int_constant,
  pr_float_constant,
  pr_typed_constant,
  pr_variable,
  pr_start_function,
  pr_function_parameter,
  pr_start_block,
  pr_end_block,
  pr_end_function,
  pr_lineno
};

/* Print out the generic debugging information recorded in dhandle.  */

bfd_boolean
print_debugging_info (f, dhandle)
     FILE *f;
     PTR dhandle;
{
  struct pr_handle info;

  info.f = f;
  info.indent = 0;
  info.stack = NULL;
  info.parameter = 0;

  return debug_write (dhandle, &pr_fns, (PTR) &info);
}

/* Indent to the current indentation level.  */

static void
indent (info)
     struct pr_handle *info;
{
  unsigned int i;

  for (i = 0; i < info->indent; i++)
    putc (' ', info->f);
}

/* Push a type on the type stack.  */

static bfd_boolean
push_type (info, type)
     struct pr_handle *info;
     const char *type;
{
  struct pr_stack *n;

  if (type == NULL)
    return FALSE;

  n = (struct pr_stack *) xmalloc (sizeof *n);
  memset (n, 0, sizeof *n);

  n->type = xstrdup (type);
  n->visibility = DEBUG_VISIBILITY_IGNORE;
  n->method = NULL;
  n->next = info->stack;
  info->stack = n;

  return TRUE;
}

/* Prepend a string onto the type on the top of the type stack.  */

static bfd_boolean
prepend_type (info, s)
     struct pr_handle *info;
     const char *s;
{
  char *n;

  assert (info->stack != NULL);

  n = (char *) xmalloc (strlen (s) + strlen (info->stack->type) + 1);
  sprintf (n, "%s%s", s, info->stack->type);
  free (info->stack->type);
  info->stack->type = n;

  return TRUE;
}

/* Append a string to the type on the top of the type stack.  */

static bfd_boolean
append_type (info, s)
     struct pr_handle *info;
     const char *s;
{
  unsigned int len;

  if (s == NULL)
    return FALSE;

  assert (info->stack != NULL);

  len = strlen (info->stack->type);
  info->stack->type = (char *) xrealloc (info->stack->type,
					 len + strlen (s) + 1);
  strcpy (info->stack->type + len, s);

  return TRUE;
}

/* We use an underscore to indicate where the name should go in a type
   string.  This function substitutes a string for the underscore.  If
   there is no underscore, the name follows the type.  */

static bfd_boolean
substitute_type (info, s)
     struct pr_handle *info;
     const char *s;
{
  char *u;

  assert (info->stack != NULL);

  u = strchr (info->stack->type, '|');
  if (u != NULL)
    {
      char *n;

      n = (char *) xmalloc (strlen (info->stack->type) + strlen (s));

      memcpy (n, info->stack->type, u - info->stack->type);
      strcpy (n + (u - info->stack->type), s);
      strcat (n, u + 1);

      free (info->stack->type);
      info->stack->type = n;

      return TRUE;
    }

  if (strchr (s, '|') != NULL
      && (strchr (info->stack->type, '{') != NULL
	  || strchr (info->stack->type, '(') != NULL))
    {
      if (! prepend_type (info, "(")
	  || ! append_type (info, ")"))
	return FALSE;
    }

  if (*s == '\0')
    return TRUE;

  return (append_type (info, " ")
	  && append_type (info, s));
}

/* Indent the type at the top of the stack by appending spaces.  */

static bfd_boolean
indent_type (info)
     struct pr_handle *info;
{
  unsigned int i;

  for (i = 0; i < info->indent; i++)
    {
      if (! append_type (info, " "))
	return FALSE;
    }

  return TRUE;
}

/* Pop a type from the type stack.  */

static char *
pop_type (info)
     struct pr_handle *info;
{
  struct pr_stack *o;
  char *ret;

  assert (info->stack != NULL);

  o = info->stack;
  info->stack = o->next;
  ret = o->type;
  free (o);

  return ret;
}

/* Print a VMA value into a string.  */

static void
print_vma (vma, buf, unsignedp, hexp)
     bfd_vma vma;
     char *buf;
     bfd_boolean unsignedp;
     bfd_boolean hexp;
{
  if (sizeof (vma) <= sizeof (unsigned long))
    {
      if (hexp)
	sprintf (buf, "0x%lx", (unsigned long) vma);
      else if (unsignedp)
	sprintf (buf, "%lu", (unsigned long) vma);
      else
	sprintf (buf, "%ld", (long) vma);
    }
  else
    {
      buf[0] = '0';
      buf[1] = 'x';
      sprintf_vma (buf + 2, vma);
    }
}

/* Start a new compilation unit.  */

static bfd_boolean
pr_start_compilation_unit (p, filename)
     PTR p;
     const char *filename;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->indent == 0);

  fprintf (info->f, "%s:\n", filename);

  return TRUE;
}

/* Start a source file within a compilation unit.  */

static bfd_boolean
pr_start_source (p, filename)
     PTR p;
     const char *filename;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->indent == 0);

  fprintf (info->f, " %s:\n", filename);

  return TRUE;
}

/* Push an empty type onto the type stack.  */

static bfd_boolean
pr_empty_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return push_type (info, "<undefined>");
}

/* Push a void type onto the type stack.  */

static bfd_boolean
pr_void_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return push_type (info, "void");
}

/* Push an integer type onto the type stack.  */

static bfd_boolean
pr_int_type (p, size, unsignedp)
     PTR p;
     unsigned int size;
     bfd_boolean unsignedp;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[10];

  sprintf (ab, "%sint%d", unsignedp ? "u" : "", size * 8);
  return push_type (info, ab);
}

/* Push a floating type onto the type stack.  */

static bfd_boolean
pr_float_type (p, size)
     PTR p;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[10];

  if (size == 4)
    return push_type (info, "float");
  else if (size == 8)
    return push_type (info, "double");

  sprintf (ab, "float%d", size * 8);
  return push_type (info, ab);
}

/* Push a complex type onto the type stack.  */

static bfd_boolean
pr_complex_type (p, size)
     PTR p;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;

  if (! pr_float_type (p, size))
    return FALSE;

  return prepend_type (info, "complex ");
}

/* Push a bfd_boolean type onto the type stack.  */

static bfd_boolean
pr_bool_type (p, size)
     PTR p;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[10];

  sprintf (ab, "bool%d", size * 8);

  return push_type (info, ab);
}

/* Push an enum type onto the type stack.  */

static bfd_boolean
pr_enum_type (p, tag, names, values)
     PTR p;
     const char *tag;
     const char **names;
     bfd_signed_vma *values;
{
  struct pr_handle *info = (struct pr_handle *) p;
  unsigned int i;
  bfd_signed_vma val;

  if (! push_type (info, "enum "))
    return FALSE;
  if (tag != NULL)
    {
      if (! append_type (info, tag)
	  || ! append_type (info, " "))
	return FALSE;
    }
  if (! append_type (info, "{ "))
    return FALSE;

  if (names == NULL)
    {
      if (! append_type (info, "/* undefined */"))
	return FALSE;
    }
  else
    {
      val = 0;
      for (i = 0; names[i] != NULL; i++)
	{
	  if (i > 0)
	    {
	      if (! append_type (info, ", "))
		return FALSE;
	    }

	  if (! append_type (info, names[i]))
	    return FALSE;

	  if (values[i] != val)
	    {
	      char ab[20];

	      print_vma (values[i], ab, FALSE, FALSE);
	      if (! append_type (info, " = ")
		  || ! append_type (info, ab))
		return FALSE;
	      val = values[i];
	    }

	  ++val;
	}
    }

  return append_type (info, " }");
}

/* Turn the top type on the stack into a pointer.  */

static bfd_boolean
pr_pointer_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *s;

  assert (info->stack != NULL);

  s = strchr (info->stack->type, '|');
  if (s != NULL && s[1] == '[')
    return substitute_type (info, "(*|)");
  return substitute_type (info, "*|");
}

/* Turn the top type on the stack into a function returning that type.  */

static bfd_boolean
pr_function_type (p, argcount, varargs)
     PTR p;
     int argcount;
     bfd_boolean varargs;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char **arg_types;
  unsigned int len;
  char *s;

  assert (info->stack != NULL);

  len = 10;

  if (argcount <= 0)
    {
      arg_types = NULL;
      len += 15;
    }
  else
    {
      int i;

      arg_types = (char **) xmalloc (argcount * sizeof *arg_types);
      for (i = argcount - 1; i >= 0; i--)
	{
	  if (! substitute_type (info, ""))
	    return FALSE;
	  arg_types[i] = pop_type (info);
	  if (arg_types[i] == NULL)
	    return FALSE;
	  len += strlen (arg_types[i]) + 2;
	}
      if (varargs)
	len += 5;
    }

  /* Now the return type is on the top of the stack.  */

  s = (char *) xmalloc (len);
  strcpy (s, "(|) (");

  if (argcount < 0)
    strcat (s, "/* unknown */");
  else
    {
      int i;

      for (i = 0; i < argcount; i++)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, arg_types[i]);
	}
      if (varargs)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, "...");
	}
      if (argcount > 0)
	free (arg_types);
    }

  strcat (s, ")");

  if (! substitute_type (info, s))
    return FALSE;

  free (s);

  return TRUE;
}

/* Turn the top type on the stack into a reference to that type.  */

static bfd_boolean
pr_reference_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->stack != NULL);

  return substitute_type (info, "&|");
}

/* Make a range type.  */

static bfd_boolean
pr_range_type (p, lower, upper)
     PTR p;
     bfd_signed_vma lower;
     bfd_signed_vma upper;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char abl[20], abu[20];

  assert (info->stack != NULL);

  if (! substitute_type (info, ""))
    return FALSE;

  print_vma (lower, abl, FALSE, FALSE);
  print_vma (upper, abu, FALSE, FALSE);

  return (prepend_type (info, "range (")
	  && append_type (info, "):")
	  && append_type (info, abl)
	  && append_type (info, ":")
	  && append_type (info, abu));
}

/* Make an array type.  */

static bfd_boolean
pr_array_type (p, lower, upper, stringp)
     PTR p;
     bfd_signed_vma lower;
     bfd_signed_vma upper;
     bfd_boolean stringp;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *range_type;
  char abl[20], abu[20], ab[50];

  range_type = pop_type (info);
  if (range_type == NULL)
    return FALSE;

  if (lower == 0)
    {
      if (upper == -1)
	sprintf (ab, "|[]");
      else
	{
	  print_vma (upper + 1, abu, FALSE, FALSE);
	  sprintf (ab, "|[%s]", abu);
	}
    }
  else
    {
      print_vma (lower, abl, FALSE, FALSE);
      print_vma (upper, abu, FALSE, FALSE);
      sprintf (ab, "|[%s:%s]", abl, abu);
    }

  if (! substitute_type (info, ab))
    return FALSE;

  if (strcmp (range_type, "int") != 0)
    {
      if (! append_type (info, ":")
	  || ! append_type (info, range_type))
	return FALSE;
    }

  if (stringp)
    {
      if (! append_type (info, " /* string */"))
	return FALSE;
    }

  return TRUE;
}

/* Make a set type.  */

static bfd_boolean
pr_set_type (p, bitstringp)
     PTR p;
     bfd_boolean bitstringp;
{
  struct pr_handle *info = (struct pr_handle *) p;

  if (! substitute_type (info, ""))
    return FALSE;

  if (! prepend_type (info, "set { ")
      || ! append_type (info, " }"))
    return FALSE;

  if (bitstringp)
    {
      if (! append_type (info, "/* bitstring */"))
	return FALSE;
    }

  return TRUE;
}

/* Make an offset type.  */

static bfd_boolean
pr_offset_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  if (! substitute_type (info, ""))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  return (substitute_type (info, "")
	  && prepend_type (info, " ")
	  && prepend_type (info, t)
	  && append_type (info, "::|"));
}

/* Make a method type.  */

static bfd_boolean
pr_method_type (p, domain, argcount, varargs)
     PTR p;
     bfd_boolean domain;
     int argcount;
     bfd_boolean varargs;
{
  struct pr_handle *info = (struct pr_handle *) p;
  unsigned int len;
  char *domain_type;
  char **arg_types;
  char *s;

  len = 10;

  if (! domain)
    domain_type = NULL;
  else
    {
      if (! substitute_type (info, ""))
	return FALSE;
      domain_type = pop_type (info);
      if (domain_type == NULL)
	return FALSE;
      if (strncmp (domain_type, "class ", sizeof "class " - 1) == 0
	  && strchr (domain_type + sizeof "class " - 1, ' ') == NULL)
	domain_type += sizeof "class " - 1;
      else if (strncmp (domain_type, "union class ",
			sizeof "union class ") == 0
	       && (strchr (domain_type + sizeof "union class " - 1, ' ')
		   == NULL))
	domain_type += sizeof "union class " - 1;
      len += strlen (domain_type);
    }

  if (argcount <= 0)
    {
      arg_types = NULL;
      len += 15;
    }
  else
    {
      int i;

      arg_types = (char **) xmalloc (argcount * sizeof *arg_types);
      for (i = argcount - 1; i >= 0; i--)
	{
	  if (! substitute_type (info, ""))
	    return FALSE;
	  arg_types[i] = pop_type (info);
	  if (arg_types[i] == NULL)
	    return FALSE;
	  len += strlen (arg_types[i]) + 2;
	}
      if (varargs)
	len += 5;
    }

  /* Now the return type is on the top of the stack.  */

  s = (char *) xmalloc (len);
  if (! domain)
    *s = '\0';
  else
    strcpy (s, domain_type);
  strcat (s, "::| (");

  if (argcount < 0)
    strcat (s, "/* unknown */");
  else
    {
      int i;

      for (i = 0; i < argcount; i++)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, arg_types[i]);
	}
      if (varargs)
	{
	  if (i > 0)
	    strcat (s, ", ");
	  strcat (s, "...");
	}
      if (argcount > 0)
	free (arg_types);
    }

  strcat (s, ")");

  if (! substitute_type (info, s))
    return FALSE;

  free (s);

  return TRUE;
}

/* Make a const qualified type.  */

static bfd_boolean
pr_const_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return substitute_type (info, "const |");
}

/* Make a volatile qualified type.  */

static bfd_boolean
pr_volatile_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return substitute_type (info, "volatile |");
}

/* Start accumulating a struct type.  */

static bfd_boolean
pr_start_struct_type (p, tag, id, structp, size)
     PTR p;
     const char *tag;
     unsigned int id;
     bfd_boolean structp;
     unsigned int size;
{
  struct pr_handle *info = (struct pr_handle *) p;

  info->indent += 2;

  if (! push_type (info, structp ? "struct " : "union "))
    return FALSE;
  if (tag != NULL)
    {
      if (! append_type (info, tag))
	return FALSE;
    }
  else
    {
      char idbuf[20];

      sprintf (idbuf, "%%anon%u", id);
      if (! append_type (info, idbuf))
	return FALSE;
    }

  if (! append_type (info, " {"))
    return FALSE;
  if (size != 0 || tag != NULL)
    {
      char ab[30];

      if (! append_type (info, " /*"))
	return FALSE;

      if (size != 0)
	{
	  sprintf (ab, " size %u", size);
	  if (! append_type (info, ab))
	    return FALSE;
	}
      if (tag != NULL)
	{
	  sprintf (ab, " id %u", id);
	  if (! append_type (info, ab))
	    return FALSE;
	}
      if (! append_type (info, " */"))
	return FALSE;
    }
  if (! append_type (info, "\n"))
    return FALSE;

  info->stack->visibility = DEBUG_VISIBILITY_PUBLIC;

  return indent_type (info);
}

/* Output the visibility of a field in a struct.  */

static bfd_boolean
pr_fix_visibility (info, visibility)
     struct pr_handle *info;
     enum debug_visibility visibility;
{
  const char *s = NULL;
  char *t;
  unsigned int len;

  assert (info->stack != NULL);

  if (info->stack->visibility == visibility)
    return TRUE;

  switch (visibility)
    {
    case DEBUG_VISIBILITY_PUBLIC:
      s = "public";
      break;
    case DEBUG_VISIBILITY_PRIVATE:
      s = "private";
      break;
    case DEBUG_VISIBILITY_PROTECTED:
      s = "protected";
      break;
    case DEBUG_VISIBILITY_IGNORE:
      s = "/* ignore */";
      break;
    default:
      abort ();
      return FALSE;
    }

  /* Trim off a trailing space in the struct string, to make the
     output look a bit better, then stick on the visibility string.  */

  t = info->stack->type;
  len = strlen (t);
  assert (t[len - 1] == ' ');
  t[len - 1] = '\0';

  if (! append_type (info, s)
      || ! append_type (info, ":\n")
      || ! indent_type (info))
    return FALSE;

  info->stack->visibility = visibility;

  return TRUE;
}

/* Add a field to a struct type.  */

static bfd_boolean
pr_struct_field (p, name, bitpos, bitsize, visibility)
     PTR p;
     const char *name;
     bfd_vma bitpos;
     bfd_vma bitsize;
     enum debug_visibility visibility;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];
  char *t;

  if (! substitute_type (info, name))
    return FALSE;

  if (! append_type (info, "; /* "))
    return FALSE;

  if (bitsize != 0)
    {
      print_vma (bitsize, ab, TRUE, FALSE);
      if (! append_type (info, "bitsize ")
	  || ! append_type (info, ab)
	  || ! append_type (info, ", "))
	return FALSE;
    }

  print_vma (bitpos, ab, TRUE, FALSE);
  if (! append_type (info, "bitpos ")
      || ! append_type (info, ab)
      || ! append_type (info, " */\n")
      || ! indent_type (info))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  if (! pr_fix_visibility (info, visibility))
    return FALSE;

  return append_type (info, t);
}

/* Finish a struct type.  */

static bfd_boolean
pr_end_struct_type (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *s;

  assert (info->stack != NULL);
  assert (info->indent >= 2);

  info->indent -= 2;

  /* Change the trailing indentation to have a close brace.  */
  s = info->stack->type + strlen (info->stack->type) - 2;
  assert (s[0] == ' ' && s[1] == ' ' && s[2] == '\0');

  *s++ = '}';
  *s = '\0';

  return TRUE;
}

/* Start a class type.  */

static bfd_boolean
pr_start_class_type (p, tag, id, structp, size, vptr, ownvptr)
     PTR p;
     const char *tag;
     unsigned int id;
     bfd_boolean structp;
     unsigned int size;
     bfd_boolean vptr;
     bfd_boolean ownvptr;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *tv = NULL;

  info->indent += 2;

  if (vptr && ! ownvptr)
    {
      tv = pop_type (info);
      if (tv == NULL)
	return FALSE;
    }

  if (! push_type (info, structp ? "class " : "union class "))
    return FALSE;
  if (tag != NULL)
    {
      if (! append_type (info, tag))
	return FALSE;
    }
  else
    {
      char idbuf[20];

      sprintf (idbuf, "%%anon%u", id);
      if (! append_type (info, idbuf))
	return FALSE;
    }

  if (! append_type (info, " {"))
    return FALSE;
  if (size != 0 || vptr || ownvptr || tag != NULL)
    {
      if (! append_type (info, " /*"))
	return FALSE;

      if (size != 0)
	{
	  char ab[20];

	  sprintf (ab, "%u", size);
	  if (! append_type (info, " size ")
	      || ! append_type (info, ab))
	    return FALSE;
	}

      if (vptr)
	{
	  if (! append_type (info, " vtable "))
	    return FALSE;
	  if (ownvptr)
	    {
	      if (! append_type (info, "self "))
		return FALSE;
	    }
	  else
	    {
	      if (! append_type (info, tv)
		  || ! append_type (info, " "))
		return FALSE;
	    }
	}

      if (tag != NULL)
	{
	  char ab[30];

	  sprintf (ab, " id %u", id);
	  if (! append_type (info, ab))
	    return FALSE;
	}

      if (! append_type (info, " */"))
	return FALSE;
    }

  info->stack->visibility = DEBUG_VISIBILITY_PRIVATE;

  return (append_type (info, "\n")
	  && indent_type (info));
}

/* Add a static member to a class.  */

static bfd_boolean
pr_class_static_member (p, name, physname, visibility)
     PTR p;
     const char *name;
     const char *physname;
     enum debug_visibility visibility;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  if (! substitute_type (info, name))
    return FALSE;

  if (! prepend_type (info, "static ")
      || ! append_type (info, "; /* ")
      || ! append_type (info, physname)
      || ! append_type (info, " */\n")
      || ! indent_type (info))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  if (! pr_fix_visibility (info, visibility))
    return FALSE;

  return append_type (info, t);
}

/* Add a base class to a class.  */

static bfd_boolean
pr_class_baseclass (p, bitpos, virtual, visibility)
     PTR p;
     bfd_vma bitpos;
     bfd_boolean virtual;
     enum debug_visibility visibility;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;
  const char *prefix;
  char ab[20];
  char *s, *l, *n;

  assert (info->stack != NULL && info->stack->next != NULL);

  if (! substitute_type (info, ""))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  if (strncmp (t, "class ", sizeof "class " - 1) == 0)
    t += sizeof "class " - 1;

  /* Push it back on to take advantage of the prepend_type and
     append_type routines.  */
  if (! push_type (info, t))
    return FALSE;

  if (virtual)
    {
      if (! prepend_type (info, "virtual "))
	return FALSE;
    }

  switch (visibility)
    {
    case DEBUG_VISIBILITY_PUBLIC:
      prefix = "public ";
      break;
    case DEBUG_VISIBILITY_PROTECTED:
      prefix = "protected ";
      break;
    case DEBUG_VISIBILITY_PRIVATE:
      prefix = "private ";
      break;
    default:
      prefix = "/* unknown visibility */ ";
      break;
    }

  if (! prepend_type (info, prefix))
    return FALSE;

  if (bitpos != 0)
    {
      print_vma (bitpos, ab, TRUE, FALSE);
      if (! append_type (info, " /* bitpos ")
	  || ! append_type (info, ab)
	  || ! append_type (info, " */"))
	return FALSE;
    }

  /* Now the top of the stack is something like "public A / * bitpos
     10 * /".  The next element on the stack is something like "class
     xx { / * size 8 * /\n...".  We want to substitute the top of the
     stack in before the {.  */
  s = strchr (info->stack->next->type, '{');
  assert (s != NULL);
  --s;

  /* If there is already a ':', then we already have a baseclass, and
     we must append this one after a comma.  */
  for (l = info->stack->next->type; l != s; l++)
    if (*l == ':')
      break;
  if (! prepend_type (info, l == s ? " : " : ", "))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  n = (char *) xmalloc (strlen (info->stack->type) + strlen (t) + 1);
  memcpy (n, info->stack->type, s - info->stack->type);
  strcpy (n + (s - info->stack->type), t);
  strcat (n, s);

  free (info->stack->type);
  info->stack->type = n;

  free (t);

  return TRUE;
}

/* Start adding a method to a class.  */

static bfd_boolean
pr_class_start_method (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;

  assert (info->stack != NULL);
  info->stack->method = name;
  return TRUE;
}

/* Add a variant to a method.  */

static bfd_boolean
pr_class_method_variant (p, physname, visibility, constp, volatilep, voffset,
			 context)
     PTR p;
     const char *physname;
     enum debug_visibility visibility;
     bfd_boolean constp;
     bfd_boolean volatilep;
     bfd_vma voffset;
     bfd_boolean context;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *method_type;
  char *context_type;

  assert (info->stack != NULL);
  assert (info->stack->next != NULL);

  /* Put the const and volatile qualifiers on the type.  */
  if (volatilep)
    {
      if (! append_type (info, " volatile"))
	return FALSE;
    }
  if (constp)
    {
      if (! append_type (info, " const"))
	return FALSE;
    }

  /* Stick the name of the method into its type.  */
  if (! substitute_type (info,
			 (context
			  ? info->stack->next->next->method
			  : info->stack->next->method)))
    return FALSE;

  /* Get the type.  */
  method_type = pop_type (info);
  if (method_type == NULL)
    return FALSE;

  /* Pull off the context type if there is one.  */
  if (! context)
    context_type = NULL;
  else
    {
      context_type = pop_type (info);
      if (context_type == NULL)
	return FALSE;
    }

  /* Now the top of the stack is the class.  */

  if (! pr_fix_visibility (info, visibility))
    return FALSE;

  if (! append_type (info, method_type)
      || ! append_type (info, " /* ")
      || ! append_type (info, physname)
      || ! append_type (info, " "))
    return FALSE;
  if (context || voffset != 0)
    {
      char ab[20];

      if (context)
	{
	  if (! append_type (info, "context ")
	      || ! append_type (info, context_type)
	      || ! append_type (info, " "))
	    return FALSE;
	}
      print_vma (voffset, ab, TRUE, FALSE);
      if (! append_type (info, "voffset ")
	  || ! append_type (info, ab))
	return FALSE;
    }

  return (append_type (info, " */;\n")
	  && indent_type (info));
}

/* Add a static variant to a method.  */

static bfd_boolean
pr_class_static_method_variant (p, physname, visibility, constp, volatilep)
     PTR p;
     const char *physname;
     enum debug_visibility visibility;
     bfd_boolean constp;
     bfd_boolean volatilep;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *method_type;

  assert (info->stack != NULL);
  assert (info->stack->next != NULL);
  assert (info->stack->next->method != NULL);

  /* Put the const and volatile qualifiers on the type.  */
  if (volatilep)
    {
      if (! append_type (info, " volatile"))
	return FALSE;
    }
  if (constp)
    {
      if (! append_type (info, " const"))
	return FALSE;
    }

  /* Mark it as static.  */
  if (! prepend_type (info, "static "))
    return FALSE;

  /* Stick the name of the method into its type.  */
  if (! substitute_type (info, info->stack->next->method))
    return FALSE;

  /* Get the type.  */
  method_type = pop_type (info);
  if (method_type == NULL)
    return FALSE;

  /* Now the top of the stack is the class.  */

  if (! pr_fix_visibility (info, visibility))
    return FALSE;

  return (append_type (info, method_type)
	  && append_type (info, " /* ")
	  && append_type (info, physname)
	  && append_type (info, " */;\n")
	  && indent_type (info));
}

/* Finish up a method.  */

static bfd_boolean
pr_class_end_method (p)
     PTR p;
{
  struct pr_handle *info = (struct pr_handle *) p;

  info->stack->method = NULL;
  return TRUE;
}

/* Finish up a class.  */

static bfd_boolean
pr_end_class_type (p)
     PTR p;
{
  return pr_end_struct_type (p);
}

/* Push a type on the stack using a typedef name.  */

static bfd_boolean
pr_typedef_type (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;

  return push_type (info, name);
}

/* Push a type on the stack using a tag name.  */

static bfd_boolean
pr_tag_type (p, name, id, kind)
     PTR p;
     const char *name;
     unsigned int id;
     enum debug_type_kind kind;
{
  struct pr_handle *info = (struct pr_handle *) p;
  const char *t, *tag;
  char idbuf[20];

  switch (kind)
    {
    case DEBUG_KIND_STRUCT:
      t = "struct ";
      break;
    case DEBUG_KIND_UNION:
      t = "union ";
      break;
    case DEBUG_KIND_ENUM:
      t = "enum ";
      break;
    case DEBUG_KIND_CLASS:
      t = "class ";
      break;
    case DEBUG_KIND_UNION_CLASS:
      t = "union class ";
      break;
    default:
      abort ();
      return FALSE;
    }

  if (! push_type (info, t))
    return FALSE;
  if (name != NULL)
    tag = name;
  else
    {
      sprintf (idbuf, "%%anon%u", id);
      tag = idbuf;
    }

  if (! append_type (info, tag))
    return FALSE;
  if (name != NULL && kind != DEBUG_KIND_ENUM)
    {
      sprintf (idbuf, " /* id %u */", id);
      if (! append_type (info, idbuf))
	return FALSE;
    }

  return TRUE;
}

/* Output a typedef.  */

static bfd_boolean
pr_typdef (p, name)
     PTR p;
     const char *name;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *s;

  if (! substitute_type (info, name))
    return FALSE;

  s = pop_type (info);
  if (s == NULL)
    return FALSE;

  indent (info);
  fprintf (info->f, "typedef %s;\n", s);

  free (s);

  return TRUE;
}

/* Output a tag.  The tag should already be in the string on the
   stack, so all we have to do here is print it out.  */

static bfd_boolean
pr_tag (p, name)
     PTR p;
     const char *name ATTRIBUTE_UNUSED;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  indent (info);
  fprintf (info->f, "%s;\n", t);

  free (t);

  return TRUE;
}

/* Output an integer constant.  */

static bfd_boolean
pr_int_constant (p, name, val)
     PTR p;
     const char *name;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];

  indent (info);
  print_vma (val, ab, FALSE, FALSE);
  fprintf (info->f, "const int %s = %s;\n", name, ab);
  return TRUE;
}

/* Output a floating point constant.  */

static bfd_boolean
pr_float_constant (p, name, val)
     PTR p;
     const char *name;
     double val;
{
  struct pr_handle *info = (struct pr_handle *) p;

  indent (info);
  fprintf (info->f, "const double %s = %g;\n", name, val);
  return TRUE;
}

/* Output a typed constant.  */

static bfd_boolean
pr_typed_constant (p, name, val)
     PTR p;
     const char *name;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;
  char ab[20];

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  indent (info);
  print_vma (val, ab, FALSE, FALSE);
  fprintf (info->f, "const %s %s = %s;\n", t, name, ab);

  free (t);

  return TRUE;
}

/* Output a variable.  */

static bfd_boolean
pr_variable (p, name, kind, val)
     PTR p;
     const char *name;
     enum debug_var_kind kind;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;
  char ab[20];

  if (! substitute_type (info, name))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  indent (info);
  switch (kind)
    {
    case DEBUG_STATIC:
    case DEBUG_LOCAL_STATIC:
      fprintf (info->f, "static ");
      break;
    case DEBUG_REGISTER:
      fprintf (info->f, "register ");
      break;
    default:
      break;
    }
  print_vma (val, ab, TRUE, TRUE);
  fprintf (info->f, "%s /* %s */;\n", t, ab);

  free (t);

  return TRUE;
}

/* Start outputting a function.  */

static bfd_boolean
pr_start_function (p, name, global)
     PTR p;
     const char *name;
     bfd_boolean global;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;

  if (! substitute_type (info, name))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  indent (info);
  if (! global)
    fprintf (info->f, "static ");
  fprintf (info->f, "%s (", t);

  info->parameter = 1;

  return TRUE;
}

/* Output a function parameter.  */

static bfd_boolean
pr_function_parameter (p, name, kind, val)
     PTR p;
     const char *name;
     enum debug_parm_kind kind;
     bfd_vma val;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char *t;
  char ab[20];

  if (kind == DEBUG_PARM_REFERENCE
      || kind == DEBUG_PARM_REF_REG)
    {
      if (! pr_reference_type (p))
	return FALSE;
    }

  if (! substitute_type (info, name))
    return FALSE;

  t = pop_type (info);
  if (t == NULL)
    return FALSE;

  if (info->parameter != 1)
    fprintf (info->f, ", ");

  if (kind == DEBUG_PARM_REG || kind == DEBUG_PARM_REF_REG)
    fprintf (info->f, "register ");

  print_vma (val, ab, TRUE, TRUE);
  fprintf (info->f, "%s /* %s */", t, ab);

  free (t);

  ++info->parameter;

  return TRUE;
}

/* Start writing out a block.  */

static bfd_boolean
pr_start_block (p, addr)
     PTR p;
     bfd_vma addr;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];

  if (info->parameter > 0)
    {
      fprintf (info->f, ")\n");
      info->parameter = 0;
    }

  indent (info);
  print_vma (addr, ab, TRUE, TRUE);
  fprintf (info->f, "{ /* %s */\n", ab);

  info->indent += 2;

  return TRUE;
}

/* Write out line number information.  */

static bfd_boolean
pr_lineno (p, filename, lineno, addr)
     PTR p;
     const char *filename;
     unsigned long lineno;
     bfd_vma addr;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];

  indent (info);
  print_vma (addr, ab, TRUE, TRUE);
  fprintf (info->f, "/* file %s line %lu addr %s */\n", filename, lineno, ab);

  return TRUE;
}

/* Finish writing out a block.  */

static bfd_boolean
pr_end_block (p, addr)
     PTR p;
     bfd_vma addr;
{
  struct pr_handle *info = (struct pr_handle *) p;
  char ab[20];

  info->indent -= 2;

  indent (info);
  print_vma (addr, ab, TRUE, TRUE);
  fprintf (info->f, "} /* %s */\n", ab);

  return TRUE;
}

/* Finish writing out a function.  */

static bfd_boolean
pr_end_function (p)
     PTR p ATTRIBUTE_UNUSED;
{
  return TRUE;
}
