/*	$OpenBSD: cpus.h,v 1.1.1.1 2004/04/21 15:23:57 aoyama Exp $ */
/* 
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 *
 * HISTORY
 */
/* 
  Versions Idents for 88k family chips 
 */

#ifndef __MACHINE_CPUS_H__
#define __MACHINE_CPUS_H__

/*
 * cpu Processor Identification Register (PID).
 */
#ifndef _LOCORE
union cpupid {
   unsigned cpupid;
   struct {
      unsigned
      /*empty*/:16,
      arc:8,
      version:7,
      master:1;
   } m88100;
   struct {
      unsigned
      id:8,
      type:3,
      version:5,
      /*empty*/:16;
   } m88200;
};
#endif /* _LOCORE */

#define M88100_ID 0
#define M88200_ID 5
#define M88204_ID 6

#endif /* __MACHINE_CPUS_H__ */
