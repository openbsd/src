/* Low level Unix child interface to ttrace, for GDB when running under HP-UX.

   Copyright 2003, 2004 Free Software Foundation, Inc.

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

#ifndef INFTTRACE_H
#define INFTTRACE_H

enum target_waitkind;

extern int parent_attach_all (int, PTRACE_ARG3_TYPE, int);
extern pid_t hppa_switched_threads (pid_t gdb_pid);
extern int hpux_has_forked (int pid, int *childpid);
extern int hpux_has_vforked (int pid, int *childpid);
extern int hpux_has_execd (int pid, char **execd_pathname);
extern int hpux_has_syscall_event (int pid, enum target_waitkind *kind,
				   int *syscall_id);

#endif
