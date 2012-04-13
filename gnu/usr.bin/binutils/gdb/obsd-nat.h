/* Native-dependent code for OpenBSD.

   Copyright 2012 Free Software Foundation, Inc.

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

#ifndef OBSD_NAT_H
#define OBSD_NAT_H

extern char *obsd_pid_to_str (ptid_t ptid);
extern void obsd_find_new_threads (void);
extern ptid_t obsd_wait (ptid_t ptid, struct target_waitstatus *ourstatus);

#endif /* obsd-nat.h */
