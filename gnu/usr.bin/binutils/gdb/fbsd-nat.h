/* Native-dependent code for FreeBSD.

   Copyright 2004 Free Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef FBSD_NAT_H
#define FBSD_NAT_H

/* Return a the name of file that can be opened to get the symbols for
   the child process identified by PID.  */

extern char *fbsd_pid_to_exec_file (int pid);

/* Iterate over all the memory regions in the current inferior,
   calling FUNC for each memory region.  OBFD is passed as the last
   argument to FUNC.  */

extern int fbsd_find_memory_regions (int (*func) (CORE_ADDR, unsigned long,
						  int, int, int, void *),
				     void *obfd);

/* Create appropriate note sections for a corefile, returning them in
   allocated memory.  */

extern char *fbsd_make_corefile_notes (bfd *obfd, int *note_size);

#endif /* fbsd-nat.h */
