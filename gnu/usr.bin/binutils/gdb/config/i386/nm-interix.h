/* Native-dependent definitions for Intel 386 running Interix, for GDB.
   Copyright 1986, 1987, 1989, 1992, 1996 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef NM_INTERIX_H
#define NM_INTERIX_H

/* Be shared lib aware.  */
#include "solib.h"

/* submodes of USE_PROC_FS.  */
#define UNIXWARE

/* It's ALMOST coff; bfd does the same thing. Mostly used in coffread.c.  */
#define COFF_IMAGE_WITH_PE

/* Turn on our own child_pid_to_exec_file.  */
#define CHILD_PID_TO_EXEC_FILE

#endif /* NM_INTERIX_H */
