/* spu -- A program to make lots of random C code.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Stan Shebs.

This file is part of SPU.

SPU is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This is a random program generator. */

#include <stdio.h>

#include "ansidecl.h"

/* The limits on these could be eliminated, but that would be
   some work.  As they stand, the limits are enough to generate
   some truly enormous programs...  */

#define MAXMACROS 50000
#define MAXMARGS 6
#define MAXSTRUCTS 1000
#define MAXSLOTS 20
#define MAXFUNCTIONS 50000
#define MAXFARGS 6

struct macro_desc {
  char *name;
  int numargs;
  char *args[MAXMARGS];
};

struct slot_desc {
  char *name;
};

struct struct_desc {
  char *name;
  int numslots;
  struct slot_desc slots[MAXSLOTS];
};

/* (should add unions as type of struct) */

struct type_desc {
  char *name;
};

struct function_desc {
  char *name;
  int numargs;
  char *args[MAXFARGS];
};

struct file_desc {
  char *name;
};

void
display_usage PARAMS ((void));

void
init_xrandom PARAMS ((int seed));

int
xrandom PARAMS ((int n));

char *
copy_string PARAMS ((char *str));

char *
xmalloc PARAMS ((int n));

char *
gen_new_macro_name PARAMS ((int n));

char *
gen_random_name PARAMS ((char *root));

char *
gen_random_local_name PARAMS ((int n, char **others));

void
write_struct PARAMS ((FILE *fp, int n));

void
create_structs PARAMS ((void));

void
create_macros PARAMS ((void));

void
create_functions PARAMS ((void));

void
write_header_file PARAMS ((int n));

void
write_source_file PARAMS ((int n));

void
write_macro PARAMS ((FILE *fp, int n));

void
write_function_decl PARAMS ((FILE *fp, int n));

void
write_function PARAMS ((FILE *fp, int n));

void
write_statement PARAMS ((FILE *fp, int depth, int max_depth));

void
write_expression PARAMS ((FILE *fp, int depth, int max_depth));

void
write_makefile PARAMS ((void));

/* The default values are set low for testing purposes.
   Real values can get much larger. */

int numfiles = 5;
int numheaderfiles = 1;

char *file_base_name = "file";

int nummacros = 1000;

int numstructs = 20;
int numslots = MAXSLOTS;

int numfunctions = 100;

int function_length = 20;

int num_functions_per_file;

/* The amount of commenting in the source. */

int commenting = 0;

struct macro_desc macros[MAXMACROS];

struct struct_desc structs[MAXSTRUCTS];

struct function_desc functions[MAXFUNCTIONS];

int num_computer_terms;

/* Likely words to appear in names of things. */

char *computerese[] = {
  "make",
  "create",
  "alloc",
  "modify",
  "delete",
  "new",
  "add",
  "list",
  "array",
  "queue",
  "object",
  "of",
  "by",
  "point",
  "line",
  "rectangle",
  "shape",
  "area",
  "window",
  "null",
  NULL
};

/* Return a word that commonly appears in programs. */

char *
computer_word ()
{
  if (num_computer_terms == 0)
    {
      int i;

      for (i = 0; computerese[i] != NULL; ++i) ;
      num_computer_terms = i;
    }
  return computerese[xrandom (num_computer_terms)];
}

main (argc, argv)
     int argc;
     char **argv;
{
  int i, num;
  char *arg;
  FILE *fp;

  /* Parse all the arguments. */
  /* (should check on numeric values) */
  for (i = 1; i < argc; ++i)
    {
      arg = argv[i];
      if (strcmp(arg, "--comments") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  commenting = num;
	}
      else if (strcmp(arg, "--files") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  numfiles = num;
	}
      else if (strcmp(arg, "--functions") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  numfunctions = num;
	}
      else if (strcmp(arg, "--function-length") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  function_length = num;
	}
      else if (strcmp(arg, "--function-depth") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  /* (should use this!) */
	}
      else if (strcmp(arg, "--header-files") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  numheaderfiles = num;
	}
      else if (strcmp(arg, "--help") == 0)
	{
	  display_usage ();
	  exit (0);
	}
      else if (strcmp(arg, "--macros") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  nummacros = num;
	}
      else if (strcmp(arg, "--slots") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  numslots = num;
	}
      else if (strcmp(arg, "--structs") == 0)
	{
	  num = strtol (argv[++i], NULL, 10);
	  numstructs = num;
	}
      else if (strcmp(arg, "--version") == 0)
	{
	  fprintf (stderr, "SPU program generator version 0.1\n");
	  exit (0);
	}
      else
	{
	  fprintf (stderr, "Usage: \"%s\" not valid, ignored\n", arg);
	  display_usage ();
	}
    }
  init_xrandom (-1);
  /* Create the definitions of objects internally. */
  create_macros ();
  create_structs ();
  create_functions ();
  num_functions_per_file = numfunctions / numfiles;
  /* Write out a bunch of files. */
  printf ("Writing %d header files...\n", numheaderfiles);
  for (i = 0; i < numheaderfiles; ++i)
    write_header_file (i);
  printf ("Writing %d files...\n", numfiles);
  for (i = 0; i < numfiles; ++i)
    write_source_file (i);
  /* Write out a makefile. */
  write_makefile ();
  /* Succeed if we actually wrote out a whole program correctly. */
  exit (0);
}

void
display_usage ()
{
  fprintf (stderr, "Usage: spu [ ... options ... ]\n");
  fprintf (stderr, "           --comments <n>\n");
  fprintf (stderr, "           --files <n>\n");
  fprintf (stderr, "           --functions <n>\n");
  fprintf (stderr, "           --function-length <n>\n");
  fprintf (stderr, "           --function-depth <n>\n");
  fprintf (stderr, "           --help\n");
  fprintf (stderr, "           --macros <n>\n");
  fprintf (stderr, "           --slots <n>\n");
  fprintf (stderr, "           --structs <n>\n");
  fprintf (stderr, "           --version\n");
}

int
create_type (str)
char *str;
{
  int i;

  return 1;
}

int
random_type ()
{
  return 1;
}

char *
name_from_type (n)
     int n;
{
  return "int";
}

/* Generate a macro name that is unique if the given n is unique. */

char *
gen_new_macro_name (n)
     int n;
{
  int i = 0;
  char namebuf[100];

  ++n;
  namebuf[i] = '\0';
  strcat (namebuf, computer_word ());
  i = strlen (namebuf);
  namebuf[i++] = '_';
  namebuf[i++] = 'M';
  while (n > 0)
    {
      namebuf[i++] = 'a' + (n % 26);
      n /= 26;
    }
  namebuf[i] = '\0';
  return copy_string (namebuf);
}

/* Create basic definitions of macros. */

void
create_macros()
{
  int i, j, numargs;

  printf ("Creating %d macros...\n", nummacros);
  for (i = 0; i < nummacros; ++i)
    {
      macros[i].name = gen_new_macro_name (i);
      numargs = xrandom (MAXMARGS + 1);
      for (j = 0; j < numargs; ++j)
	{
	  macros[i].args[j] = gen_random_local_name(j, NULL);
	}
      macros[i].numargs = numargs;
    }
}

/* Generate a unique structure name, based on the number n. */

char *
gen_new_struct_name (n)
     int n;
{
  int i = 0;
  char namebuf[100];

  ++n;
  namebuf[i++] = 's';
  namebuf[i++] = '_';
  while (n > 0)
    {
      namebuf[i++] = 'a' + (n % 26);
      n /= 26;
    }
  namebuf[i] = '\0';
  if (xrandom (4) == 0)
    strcat (namebuf, "_struct");
  return copy_string (namebuf);
}

char *
gen_random_slot_name (n)
     int n;
{
  char namebuf[100];

  /* (should have more variety) */
  sprintf (namebuf, "slot%d", n);
  return copy_string (namebuf);
}

/* Create definitions of the desired number of structures. */

void
create_structs()
{
  int i, j;

  printf ("Creating %d structs...\n", numstructs);
  for (i = 0; i < numstructs; ++i)
    {
      structs[i].name = gen_new_struct_name(i);
      for (j = 0; j < 20; ++j)
	{
	  structs[i].slots[j].name = gen_random_slot_name (j);
	}
      structs[i].numslots = j;
    }
}

/* Generate a function name that is unique if n is unique. */

char *
gen_new_function_name (n)
     int n;
{
  int i = 0;
  char namebuf[100];

  ++n;
  namebuf[i] = '\0';
  /* Start with a random computer term.  */
  if (xrandom (5) == 0)
    {
      strcat (namebuf, computer_word ());
      i = strlen (namebuf);
      namebuf[i++] = '_';
    }
  namebuf[i++] = 'f';
  /* Note that if we just add an 'f', there is a small chance of getting
     the name "for", which make the compiler unhappy.  */
  namebuf[i++] = 'n';
  /* Convert the number n itself into a string, maybe with some underscores
     thrown in for flavor.  */
  while (n > 0)
    {
      if (xrandom(4) == 0) namebuf[i++] = '_';
      namebuf[i++] = 'a' + (n % 26);
      n /= 26;
    }
  namebuf[i] = '\0';
  /* Maybe add some more computerese on the end. */
  if (xrandom (4) != 0)
    {
      namebuf[i++] = '_';
      namebuf[i] = '\0';
      strcat (namebuf, computer_word ());
    }
  return copy_string (namebuf);
}

/* Create a number of functions with random numbers of arguments. */

/* (should gen with random arg types also) */

void
create_functions()
{
  int i, j, numargs;

  printf ("Creating %d functions...\n", numfunctions);
  for (i = 0; i < numfunctions; ++i)
    {
      functions[i].name = gen_new_function_name(i);
      numargs = xrandom (MAXFARGS + 1);
      for (j = 0; j < numargs; ++j)
	{
	  functions[i].args[j] = gen_random_local_name(j, NULL);
	}
      functions[i].numargs = numargs;
    }
}

void
write_header_file (n)
     int n;
{
  int i;
  char tmpbuf[100];
  FILE *fp;

  sprintf (tmpbuf, "%s%d.h", file_base_name, n);
  fp = fopen (tmpbuf, "w");
  if (fp)
    {
      if (commenting > 0)
	fprintf (fp, "/* header */\n");
      if (1)
	{
	  printf ("Writing %d structs...\n", numstructs);
	  for (i = 0; i < numstructs; ++i)
	    {
	      write_struct (fp, i);
	    }
	}
      if (1)
	{
	  printf ("Writing %d macros...\n", nummacros);
	  for (i = 0; i < nummacros; ++i)
	    {
	      write_macro (fp, i);
	    }
	}
      if (1)
	{
	  printf ("Writing %d function decls...\n", numfunctions);
	  for (i = 0; i < numfunctions; ++i)
	    {
	      write_function_decl (fp, i);
	    }
	}
      fclose (fp);
    }
}

/* Write out the definition of a structure. */

void
write_struct (fp, i)
     FILE *fp;
     int i;
{
  int j;

  if (i == 0) printf ("  (Each struct contains %d slots)\n", numslots);
  fprintf (fp, "struct %s {\n", structs[i].name);
  for (j = 0; j < structs[i].numslots; ++j)
    {
      fprintf (fp, "  %s %s;\n",
	       name_from_type (random_type ()), structs[i].slots[j].name);
    }
  fprintf (fp, "};\n\n");
}

void
write_macro (fp, n)
     FILE *fp;
     int n;
{
  int i, j;

  fprintf (fp, "#define %s", macros[n].name);
  if (1)
    {
      fprintf (fp, "(");
      for (j = 0; j < macros[n].numargs; ++j)
	{
	  if (j > 0) fprintf (fp, ",");
	  fprintf (fp, "%s", macros[n].args[j]);
	}
      fprintf (fp, ")");
    }
  /* Generate a macro body. */
  switch (xrandom(2))
    {
    case 0:
      fprintf (fp, "\\\n");
      fprintf (fp, "(");
      if (macros[n].numargs > 0)
	{
	  for (i = 0; i < macros[n].numargs; ++i)
	    {
	      if (i > 0) fprintf (fp, ",");
	      fprintf (fp, " \\\n");
	      fprintf (fp, "  (%s)", macros[n].args[i]);
	      if (xrandom (2) == 0)
		{
		  fprintf (fp, ",");
		  fprintf (fp, " \\\n");
		  fprintf (fp, " ((int) (%s))", macros[n].args[i]);
		}
	    }
	  fprintf (fp, "\\\n");
	}
      else
	{
	  fprintf (fp, " (1)");
	}
      fprintf (fp, ")");
      break;
    default:
      fprintf (fp, " (1)");
      break;
    }
  fprintf (fp, "\n\n");
}

void
write_function_decl (fp, n)
     FILE *fp;
     int n;
{
  fprintf (fp, "int %s (", functions[n].name);
  fprintf (fp, ");\n");
}

/* Write a complete source file. */

void
write_source_file (n)
     int n;
{
  char tmpbuf[100];
  int j, k;
  FILE *fp;

  sprintf (tmpbuf, "%s%d.c", file_base_name, n);
  fp = fopen (tmpbuf, "w");
  if (fp)
    {
      if (numheaderfiles > 0)
	{
	  for (j = 0; j < numheaderfiles; ++j)
	    {
	      fprintf(fp, "#include \"%s%d.h\"\n", file_base_name, j);
	    }
	  fprintf(fp, "\n");
	}

      if (n == 0) printf ("  (Each file contains %d functions)\n",
			  num_functions_per_file);

      /* Put out a "main", but only in the first C file. */
      if (n == 0)
	{
	  fprintf (fp, "main ()\n");
	  fprintf (fp, "{\n");
	  if (1 /* use stdio */)
	    {
	      fprintf (fp, "  printf (\"hello world\\n\");\n");
	      /* (should issue calls to other functions?) */
	    }
	  fprintf (fp, "}\n\n");
	}

      for (j = 0; j < num_functions_per_file; ++j)
	{
	  write_function (fp, n * num_functions_per_file + j);
	}
    }
  fclose (fp);
}

void
write_function (fp, n)
     FILE *fp;
     int n;
{
  int k;

  fprintf(fp, "%s ()\n", functions[n].name);
  fprintf(fp, "{\n");
  /* Generate a plausible function body. */
  for (k = 0; k < function_length; ++k)
    {
      write_statement (fp, 0, xrandom(2) + 1);
    }
  fprintf (fp, "}\n\n");
}

void
write_statement (fp, depth, max_depth)
     FILE *fp;
     int depth, max_depth;
{
  int n, j;

  /* Always do non-recursive statements if going too deep. */
  if (depth >= max_depth || xrandom(2) == 0)
    {
      switch (xrandom(2))
	{
	default:
	  write_expression (fp, 0, xrandom(4) + 1);
	  fprintf (fp, ";\n");
	  break;
	}
    }
  else
    {
      switch (xrandom(2))
	{
	default:
	  fprintf (fp, "if (");
	  write_expression (fp, 0, xrandom(2) + 1);
	  fprintf (fp, ")\n    {\n");
	  write_statement(fp, depth + 1, max_depth);
	  fprintf (fp, "    }\n");
	  break;
	}
    }
}

/* Write a single expression. */

void
write_expression (fp, depth, max_depth)
     FILE *fp;
     int depth, max_depth;
{
  int n, j;

  /* Always do non-recursive statements if going too deep. */
  if (depth >= max_depth || xrandom(2) == 0)
    {
      switch (xrandom(10))
	{
	case 7:
	  fprintf (fp, "%d", xrandom (1000));
	  break;
	default:
	  fprintf (fp, "%d", xrandom (127));
	  break;
	}
    }
  else
    {
      switch (xrandom(10))
	{
	case 0:
	case 5:
	case 7:
	  n = xrandom (numfunctions);
	  fprintf(fp, "  %s (", functions[n].name);
	  for (j = 0; j < functions[n].numargs; ++j)
	    {
	      if (j > 0) fprintf (fp, ", ");
	      write_expression(fp, depth + 1, max_depth);
	    }
	  fprintf(fp, ")");
	  break;
	case 1:
	case 6:
	case 8:
	  n = xrandom (nummacros);
	  fprintf(fp, "  %s(", macros[n].name);
	  for (j = 0; j < macros[n].numargs; ++j)
	    {
	      if (j > 0) fprintf (fp, ", ");
	      write_expression(fp, depth + 1, max_depth);
	    }
	  fprintf(fp, ")");
	  break;
	case 2:
	  write_expression (fp, depth + 1, max_depth);
	  fprintf (fp, " + ");
	  write_expression (fp, depth + 1, max_depth);
	  break;
	case 3:
	  write_expression (fp, depth + 1, max_depth);
	  fprintf (fp, " - ");
	  write_expression (fp, depth + 1, max_depth);
	  break;
	case 4:
	  write_expression (fp, depth + 1, max_depth);
	  fprintf (fp, " * ");
	  write_expression (fp, depth + 1, max_depth);
	  break;
	default:
	  fprintf (fp, "%d", xrandom (127));
	  break;
	}
    }
}

/* Write out a makefile that will compile the program just generated. */

void
write_makefile ()
{
  char tmpbuf[100];
  int i, j;
  FILE *fp;

  sprintf (tmpbuf, "%s.mk", file_base_name);
  fp = fopen (tmpbuf, "w");
  if (fp)
    {
      fprintf (fp, "CC = cc\n\n");
      /* Write dependencies and action line for the executable.  */
      fprintf (fp, "%s:	", file_base_name);
      for (i = 0; i < numfiles; ++i)
	fprintf (fp, " %s%d.o", file_base_name, i);
      fprintf (fp, "\n");
      fprintf (fp, "\t$(CC) -o %s.out", file_base_name);
      for (i = 0; i < numfiles; ++i)
	fprintf (fp, " %s%d.o", file_base_name, i);
      fprintf (fp, "\n\n");
      /* Write dependencies for individual files. */
      for (i = 0; i < numfiles; ++i)
	{
	  fprintf (fp, " %s%d.o:	%s%d.c",
		   file_base_name, i, file_base_name, i);
	  for (j = 0; j < numheaderfiles; ++j)
	    fprintf (fp, " %s%d.h", file_base_name, j);
	  fprintf (fp, "\n");
	}
      fclose (fp);
    }
}


/* Utility/general functions. */

char *
gen_random_name (root)
char *root;
{
  char namebuf[100];

  if (root == NULL) root = "n";
  sprintf (namebuf, "%s_%d", root, xrandom (10000));
  return copy_string (namebuf);
}

/* Generate a local variable name. */

char *
gen_random_local_name (numothers, others)
int numothers;
char **others;
{
  char namebuf[100];

  sprintf (namebuf, "arg%d", numothers + 1);
  return copy_string (namebuf);
}

#include <time.h>

/* Random number handling is important but terrible/nonexistent
   in some systems.  Do it ourselves.  Also, this will give repeatable
   results across multiple platforms.  */

/* The random state *must* be at least 32 bits.  */

unsigned long initrandstate = 0;

unsigned long randstate = 0;

/* Seed can come from elsewhere, for repeatability.  Otherwise, it comes
   from the current time, scaled down to where 32-bit arithmetic won't
   overflow.  */

void
init_xrandom (seed)
     int seed;
{
  time_t tm;
    	
  if (seed > 0)
    {
      /* If the random state is already set, changes are somewhat
	 suspicious.  */
      if (randstate > 0)
	{
	  fprintf (stderr, "Randstate being changed from %lu to %d\n",
		   randstate, seed);
	}
      randstate = seed;
    }
  else
    {
      time (&tm);
      randstate = tm;
    }
  /* Whatever its source, put the randstate into known range (0 - 99999).  */
  randstate = abs (randstate);
  randstate %= 100000L;
  /* This is kept around for the sake of error reporting. */
  initrandstate = randstate;
}

/* Numbers lifted from Numerical Recipes, p. 198.  */
/* Arithmetic must be 32-bit.  */

int
xrandom (m)
int m;
{
    randstate = (8121 * randstate + 28411) % 134456L;
    return ((m * randstate) / 134456L);
}

char *
xmalloc (amt)
     int amt;
{
  char *value = (char *) malloc (amt);

  if (value == NULL)
    {
      /* This is pretty serious, have to get out quickly.  */
      fprintf (stderr, "Memory exhausted!!\n");
      exit (1);
    }
  /* Save callers from having to clear things themselves.  */
  bzero (value, amt);
  return value;
}

/* Copy a string to newly-allocated space.  The new space is never freed. */

char *
copy_string (str)
     char *str;
{
  int len = strlen (str);
  char *rslt;
  
  rslt = xmalloc (len + 1);
  strcpy (rslt, str);
  return rslt;
}
