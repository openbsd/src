/*	$OpenBSD: rf_debugprint.c,v 1.1 1999/01/11 14:29:13 niklas Exp $	*/
/*	$NetBSD: rf_debugprint.c,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* 
 * Code to do debug printfs. Calls to rf_debug_printf cause the corresponding
 * information to be printed to a circular buffer rather than the screen.
 * The point is to try and minimize the timing variations induced by the
 * printfs, and to capture only the printf's immediately preceding a failure.
 */

/* :  
 * Log: rf_debugprint.c,v 
 * Revision 1.13  1996/08/07 21:08:31  jimz
 * remove bogus ; from mutex decl
 *
 * Revision 1.12  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.11  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.10  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.9  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.8  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.7  1996/05/20  16:16:06  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.6  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.5  1995/12/01  16:00:45  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_debugprint.h"
#include "rf_general.h"
#include "rf_options.h"

#include <sys/param.h>

struct RF_Entry_s {
   char *cstring;
   void *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
};

/* space for 1k lines */
#define BUFSHIFT 10
#define BUFSIZE  (1<<BUFSHIFT)
#define BUFMASK  (BUFSIZE-1)

static struct RF_Entry_s rf_debugprint_buf[BUFSIZE];
static int rf_debugprint_index = 0;
RF_DECLARE_STATIC_MUTEX(rf_debug_print_mutex)

int rf_ConfigureDebugPrint(listp)
  RF_ShutdownList_t  **listp;
{
  int rc;

  rc = rf_create_managed_mutex(listp, &rf_debug_print_mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(rc);
  }
  rf_clear_debug_print_buffer();
  return(0);
}

void rf_clear_debug_print_buffer()
{
  int i;

  for (i=0; i<BUFSIZE; i++)
    rf_debugprint_buf[i].cstring = NULL;
  rf_debugprint_index = 0;
}

void rf_debug_printf(s,a1,a2,a3,a4,a5,a6,a7,a8)
char *s;
void *a1,*a2,*a3,*a4,*a5,*a6,*a7,*a8;
{
  int idx;

  if (rf_debugPrintUseBuffer) {

    RF_LOCK_MUTEX(rf_debug_print_mutex);
    idx = rf_debugprint_index;
    rf_debugprint_index = (rf_debugprint_index+1) & BUFMASK;
    RF_UNLOCK_MUTEX(rf_debug_print_mutex);

    rf_debugprint_buf[idx].cstring = s;
    rf_debugprint_buf[idx].a1 = a1;
    rf_debugprint_buf[idx].a2 = a2;
    rf_debugprint_buf[idx].a3 = a3;
    rf_debugprint_buf[idx].a4 = a4;
    rf_debugprint_buf[idx].a5 = a5;
    rf_debugprint_buf[idx].a6 = a6;
    rf_debugprint_buf[idx].a7 = a7;
    rf_debugprint_buf[idx].a8 = a8;
  }
  else {
    printf(s,a1,a2,a3,a4,a5,a6,a7,a8);
  }
}

void rf_print_debug_buffer()
{
  rf_spill_debug_buffer(NULL);
}

void rf_spill_debug_buffer(fname)
  char  *fname;
{
  int i;
#ifndef KERNEL
  FILE *fp;
#endif /* !KERNEL */

  if (!rf_debugPrintUseBuffer)
    return;

  RF_LOCK_MUTEX(rf_debug_print_mutex);
#ifndef KERNEL
  fp = (fname) ? fopen(fname,"w") : stdout;
  if (!fp) {printf("Unable to open file %s for writing\n",fname); return;}
  for (i=rf_debugprint_index+1; i != rf_debugprint_index; i = (i+1)&BUFMASK) if (rf_debugprint_buf[i].cstring)
    fprintf(fp,rf_debugprint_buf[i].cstring,rf_debugprint_buf[i].a1,rf_debugprint_buf[i].a2,rf_debugprint_buf[i].a3,
	    rf_debugprint_buf[i].a4,rf_debugprint_buf[i].a5,rf_debugprint_buf[i].a6,rf_debugprint_buf[i].a7,rf_debugprint_buf[i].a8);
  fprintf(fp,rf_debugprint_buf[i].cstring,rf_debugprint_buf[i].a1,rf_debugprint_buf[i].a2,rf_debugprint_buf[i].a3,
	  rf_debugprint_buf[i].a4,rf_debugprint_buf[i].a5,rf_debugprint_buf[i].a6,rf_debugprint_buf[i].a7,rf_debugprint_buf[i].a8);
  fclose(fp);
#else /* !KERNEL */
  for (i=rf_debugprint_index+1; i != rf_debugprint_index; i = (i+1)&BUFMASK) if (rf_debugprint_buf[i].cstring)
    printf(rf_debugprint_buf[i].cstring,rf_debugprint_buf[i].a1,rf_debugprint_buf[i].a2,rf_debugprint_buf[i].a3,
	   rf_debugprint_buf[i].a4,rf_debugprint_buf[i].a5,rf_debugprint_buf[i].a6,rf_debugprint_buf[i].a7,rf_debugprint_buf[i].a8);
  printf(rf_debugprint_buf[i].cstring,rf_debugprint_buf[i].a1,rf_debugprint_buf[i].a2,rf_debugprint_buf[i].a3,
	 rf_debugprint_buf[i].a4,rf_debugprint_buf[i].a5,rf_debugprint_buf[i].a6,rf_debugprint_buf[i].a7,rf_debugprint_buf[i].a8);
#endif /* !KERNEL */
  RF_UNLOCK_MUTEX(rf_debug_print_mutex);
}
