/* bucomm.h -- binutils common include file.
   Copyright (C) 1992, 93, 94 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _BUCOMM_H
#define _BUCOMM_H

#include "ansidecl.h"
#include <stdio.h>
#include <sys/types.h>

#include "config.h"

#ifdef USE_BINARY_FOPEN
#include "fopen-bin.h"
#else
#include "fopen-same.h"
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#else
extern char *strchr ();
extern char *strrchr ();
extern char *strstr ();
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef O_RDWR
#define O_RDWR 2
#endif

/* bucomm.c */
void bfd_nonfatal PARAMS ((CONST char *));

void bfd_fatal PARAMS ((CONST char *));

void fatal PARAMS ((CONST char *, ...));

void list_matching_formats PARAMS ((char **p));

void list_supported_targets PARAMS ((const char *, FILE *));

void print_arelt_descr PARAMS ((FILE *file, bfd *abfd, boolean verbose));

char *make_tempname PARAMS ((char *));

bfd_vma parse_vma PARAMS ((const char *, const char *));

extern char *program_name;

/* filemode.c */
void mode_string PARAMS ((unsigned long mode, char *buf));

/* libiberty */
PTR xmalloc PARAMS ((size_t));

PTR xrealloc PARAMS ((char *, size_t));

#endif /* _BUCOMM_H */
