/*
 * doschk - check filenames for DOS (and SYSV) compatibility
 *
 * Copyright (C) 1993 DJ Delorie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 *
 * This program is intended as a utility to help software developers
 * ensure that their source file names are distinguishable on MS-DOS and
 * 14-character SYSV platforms.  To perform this task, doschk reads a
 * list of filenames and produces a report of all the conflicts that
 * would arise if the files were transferred to a MS-DOS or SYSV
 * platform.  It also reports any file names that would conflict with
 * MS-DOS device names.
 *
 * To use this program, you must feed it a list of filenames in this
 * format:
 *
 *         dir
 *         dir/file1.ext
 *         dir/file2.exe
 *         dir/dir2
 *         dir/dir2/file3.ext
 *
 * If the list does not include the directory-only lines (like dir/dir2)
 * then their names will not be checked for uniqueness, else they will
 * be.  Typical uses of this program are like these:
 *
 *         find . -print | doschk
 *         tar tf file.tar | doschk
 *
 * If this program produces no output, then all your files are MS-DOS
 * compatible.  Any output messages are designed to be self-explanatory
 * and indicate cases where the files will not transfer to MS-DOS without
 * problems.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

extern char *malloc ();

typedef struct ENT
{
  struct ENT *next;
  char *dos_name;
  char *full_name;
  char *path;
  int tagged;
} ENT;

/* 
 * List of filenames in MSDOG that are special devices. 
 * Not all of these are problems on all MSLOSS systems, but most would not
 * work on most of them.
 */
static char *dos_special_names[] = 
{
  "NUL", 
  "CON", 
  "PRN", 
  "AUX", 
  "COM1", 
  "COM2", 
  "COM3",
  "COM4",
  "LPT1",
  "LPT2",
  "LPT3",
  "MS$MOUSE",
  "EMMXXXX0",
  "XMSXXXX0",
  "SMARTAAR",
  "SETVERXX",
  NULL
} ;

ENT *eroot = 0;

int first_inv = 1;
int first_msg = 1;

/****************************************************************\
 *  Utility routines						*
\****************************************************************/

void
invalid_msg ()
{
  if (first_inv)
    {
      if (first_msg)
	first_msg = 0;
      else
	putchar ('\n');
      printf ("The following files are not valid DOS file names:\n");
      first_inv = 0;
    }
}

char *
xmalloc (size)
     int size;
{
  char *s;

  if (size == 0) 
    return NULL; 

  s = (char *) malloc (size);

  if (s == NULL)
    {
      fprintf (stderr, "Virtual memory exhausted.\n");
      exit (1);
    }

  return s;
}

ENT *
alloc_ent ()
{
  ENT *rv = (ENT *) xmalloc (sizeof (ENT));
  memset (rv, 0, sizeof (ENT));
  return rv;
}

void
fill_ent (ent, path)
     ENT *ent;
     char *path;
{
  char *first = path;
  char *null = path + strlen (path);
  char *last_slash = strrchr (path, '/');
  char *cp, *dp;
  int dots_seen, chars_seen;

  if (last_slash + 1 == null)
    {
      *--null = '\0';
      last_slash = strrchr (path, '/');
    }

  if (!last_slash)
    {
      last_slash = first - 1;
    }

  if (null - last_slash < 13)
    ent->dos_name = (char *) xmalloc (null - last_slash);
  else
    ent->dos_name = (char *) xmalloc (13);
  ent->full_name = (char *) xmalloc (null - last_slash);
  ent->path = (char *) xmalloc (last_slash - first + 1);

  strcpy (ent->full_name, last_slash + 1);
  if (last_slash > first)
    {
      strncpy (ent->path, first, last_slash - first);
      ent->path[last_slash - first] = '\0';
    }
  else
    ent->path = "\0";

  cp = last_slash + 1;
  dp = ent->dos_name;
  dots_seen = 0;
  chars_seen = 0;
  while (1)
    {
      if (!*cp)
	break;
      switch (*cp)
	{
	case '.':
	  if (cp == last_slash + 1 && strcmp (last_slash + 1, "."))
	    {
	      invalid_msg ();
	      printf ("%s - file name cannot start with dot\n", path);
	      *dp = 0;
	      break;
	    }
	  if (dots_seen == 1)
	    {
              /* If trailing dot, it will be ignored by MSDOG, so don't */
              /* actually complain. */
              if (*(cp + 1) != NULL)
                {
                  invalid_msg ();
                  printf ("%s - too many dots\n", path);
                }
	      *dp = '\0';
	      break;
	    }
	  *dp++ = '.';
	  chars_seen = 0;
	  dots_seen++;
	  break;
	case '"':
	case '*':
	case '+':
	case ',':
	case ';':
	case '<':
	case '=':
	case '>':
	case '?':
	case '[':
	case '\\':
	case ']':
	case '|':
        case ':':
	  invalid_msg ();
	  printf ("%s - invalid character `%c'\n", path, *cp);
	  *dp++ = '?';
	  chars_seen++;
	  break;
	default:
	  if (dots_seen)
	    {
	      if (chars_seen >= 3)
		break;
	    }
	  else if (chars_seen >= 8)
	    break;
	  if ((*cp <= ' ') || (*cp >= 0x7f))
	    {
	      invalid_msg ();
	      printf ("%s - invalid character `%c'\n", path, *cp);
	      *dp++ = '?';
	      chars_seen++;
	      break;
	    }
	  if (islower (*cp))
	    *dp++ = toupper (*cp);
	  else
	    *dp++ = *cp;
	  chars_seen++;
	  break;
	}
      cp++;
    }
  *dp++ = '\0';
}

int
compare_ent_dosname (e1, e2)
     ENT **e1;
     ENT **e2;
{
  int r = strcmp ((*e1)->dos_name, (*e2)->dos_name);
  if (r == 0)
    r = strcmp ((*e1)->path, (*e2)->path);
  if (r == 0)
    r = strcmp ((*e1)->full_name, (*e2)->full_name);
  return r;
}

int
compare_ent_fullname (e1, e2)
     ENT **e1;
     ENT **e2;
{
  int r = strncmp ((*e1)->full_name, (*e2)->full_name, 14);
  if (r == 0)
    r = strcmp ((*e1)->path, (*e2)->path);
  if (r == 0)
    r = strcmp ((*e1)->full_name, (*e2)->full_name);
  return r;
}

char *
mpath (ent)
     ENT *ent;
{
  static char buf[1024];  /* fixed sizes for buffers are bad! */
  if (ent->path && ent->path[0])
    sprintf (buf, "%s/%s", ent->path, ent->full_name);
  else
    return ent->full_name;
  return buf;
}

/****************************************************************\
 *  List handling routines					*
\****************************************************************/

void
add_ent (ent)
     ENT *ent;
{
  ent->next = eroot;
  eroot = ent;
}

void
handle_input (line)
     char *line;
{
  ENT *ent = alloc_ent ();
  fill_ent (ent, line);
  add_ent (ent);
}

void
display_problems ()
{
  ENT **elist, *ent;
  int ecount, i, first, first_err;
  char **dos_dev_name;

  for (ecount = 0, ent = eroot; ent; ent = ent->next, ecount++);
  elist = (ENT **) xmalloc (sizeof (ENT *) * ecount);
  for (ecount = 0, ent = eroot; ent; ent = ent->next, ecount++)
    elist[ecount] = ent;

  qsort (elist, ecount, sizeof (ENT *), compare_ent_dosname);

  first_err = 1;
  for (i = 0; i < ecount; i++)
    {
      int elist_len = strlen (elist[i]->dos_name);

      dos_dev_name = dos_special_names; 
      while (*dos_dev_name)
        {
          if ((strcmp (elist[i]->dos_name, *dos_dev_name) == 0)
              || ((*(elist[i]->dos_name + elist_len - 1) == '.')
                  && (strncmp (elist[i]->dos_name, *dos_dev_name, elist_len - 2) == 0)))
            {
              if (first_err)
                {
                  if (first_msg)
                    first_msg = 0;
                  else
                    putchar ('\n');
                  printf ("The following resolve to special DOS device names:\n");
                  first_err = 0;
                }
              printf ("%-14s : %s\n", elist[i]->dos_name, mpath (elist[i]));
              break;
            }
          dos_dev_name++;
        }
    }
  
  first = 1;
  first_err = 1;
  for (i = 0; i < ecount - 1; i++)
    {
      int elist1_len = strlen (elist[i + 1]->dos_name);

      if (((strcmp (elist[i]->dos_name, elist[i + 1]->dos_name) == 0)
           && (strcmp (elist[i]->path, elist[i + 1]->path) == 0))
          || ((*(elist[i + 1]->dos_name + elist1_len - 1) == '.')
              && (strncmp (elist[i]->dos_name, elist[i + 1]->dos_name, elist1_len - 2) == 0)))
	{
	  if (first_err)
	    {
	      if (first_msg)
		first_msg = 0;
	      else
		putchar ('\n');
	      printf ("The following resolve to the same DOS file names:\n");
	      first_err = 0;
	    }
	  if (first)
	    {
	      printf ("%-14s : %s\n", elist[i]->dos_name, mpath (elist[i]));
	      first = 0;
	    }
	  printf ("\t\t %s\n", mpath (elist[i + 1]));
	}
      else
	first = 1;
    }

  qsort (elist, ecount, sizeof (ENT *), compare_ent_fullname);

  first = 1;
  first_err = 1;
  for (i = 0; i < ecount - 1; i++)
    {
      if ((strncmp (elist[i]->full_name, elist[i + 1]->full_name, 14) == 0) &&
	  (strcmp (elist[i]->path, elist[i + 1]->path) == 0))
	{
	  if (first_err)
	    {
	      if (first_msg)
		first_msg = 0;
	      else
		putchar ('\n');
	      printf ("The following resolve to the same SysV file names:\n");
	      first_err = 0;
	    }
	  if (first)
	    {
	      printf ("%.14s : %s\n", elist[i]->full_name, mpath (elist[i]));
	      first = 0;
	      elist[i]->tagged = 1;
	    }
	  printf ("\t\t %s\n", mpath (elist[i + 1]));
	  elist[i + 1]->tagged = 1;
	}
      else
	first = 1;
    }

  first_err = 1;
  for (i = 0; i < ecount; i++)
    {
      if ((strlen (elist[i]->full_name) > 14) && !elist[i]->tagged)
	{
	  if (first_err)
	    {
	      if (first_msg)
		first_msg = 0;
	      else
		putchar ('\n');
	      printf ("The following file names are too long for SysV:\n");
	      first_err = 0;
	    }
	  printf ("%.14s : %s\n", elist[i]->full_name, mpath (elist[i]));
	}
    }
}

/****************************************************************\
 *  Main entry point						*
\****************************************************************/

main (argc, argv)
     int argc;
     char **argv;
{
  FILE *input = stdin;
  if (argc > 1)
    {
      input = fopen (argv[1], "r");
      if (!input)
	{
	  perror (argv[1]);
	  exit (1);
	}
    }
  while (1)
    {
      char line[500];
      char *lp;
      fgets (line, 500, input);
      if (feof (input))
	break;
      lp = line + strlen (line);
      while ((lp != line) && (*lp <= ' '))
	lp--;
      lp[1] = 0;
      handle_input (line);
    }
  display_problems ();
  return 0;
}

/* End of file */
