/* Target machine sub-description for QNX Neutrino version 6.
   This is included by other tm-*.h files to specify nto specific
   stuff. 

   Copyright 2003 Free Software Foundation, Inc.

   This code was donated by QNX Software Systems Ltd.

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

#ifndef _TM_QNXNTO_H
#define _TM_QNXNTO_H

#include "tm-sysv4.h"

/* Setup the valid realtime signal range.  */
#define REALTIME_LO 41
#define REALTIME_HI 56

#endif /* _TM_QNXNTO_H */
