/*	$OpenBSD: rf_etimer.h,v 1.1 1999/01/11 14:29:20 niklas Exp $	*/
/*	$NetBSD: rf_etimer.h,v 1.1 1998/11/13 04:20:29 oster Exp $	*/
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

/* rf_etimer.h -- header file for code related to accurate timing
 * This code currently assumes that the elapsed time between START_TIMER
 * and START_TIMER is less than the period of the cycle counter.  This
 * means the events you want to time must be less than:
 *        clock speed               max time
 *        ----------                --------
 *           175 MHz                 24 sec
 *           150 MHz                 28 sec
 *           125 MHz                 34 sec
 *
 *
 * :  
 * Log: rf_etimer.h,v 
 * Revision 1.32  1996/08/13 18:11:09  jimz
 * want MACH&&!__osf__, not just MACH for mach timing (MACH defined under OSF/1)
 *
 * Revision 1.31  1996/08/12  20:11:38  jimz
 * use read_real_time() on AIX4+
 *
 * Revision 1.30  1996/08/09  18:48:12  jimz
 * for now, use gettimeofday() on MACH
 * (should eventually use better clock stuff)
 *
 * Revision 1.29  1996/08/07  21:09:08  jimz
 * add IRIX as a gettimeofday system
 *
 * Revision 1.28  1996/08/06  22:25:23  jimz
 * add LINUX_I386
 *
 * Revision 1.27  1996/07/30  04:45:53  jimz
 * add ultrix stuff
 *
 * Revision 1.26  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.25  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.24  1996/07/27  18:40:24  jimz
 * cleanup sweep
 *
 * Revision 1.23  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.22  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.21  1996/07/17  21:00:58  jimz
 * clean up timer interface, tracing
 *
 * Revision 1.20  1996/07/17  14:26:28  jimz
 * rf_scc -> rf_rpcc
 *
 * Revision 1.19  1996/06/14  21:24:48  jimz
 * move out ConfigureEtimer
 *
 * Revision 1.18  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.17  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.16  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.15  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.14  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.13  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.12  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.11  1995/12/01  18:10:40  root
 * added copyright info
 *
 * Revision 1.10  1995/09/29  14:27:32  wvcii
 * removed printfs from ConfigureEtimer()
 *
 * Revision 1.9  95/09/19  22:57:31  jimz
 * added kernel version of ConfigureEtimer
 * 
 * Revision 1.8  1995/09/14  13:03:04  amiri
 * set default CPU speed to 125Mhz to avoid divide by zero problems.
 *
 * Revision 1.7  1995/09/11  19:04:36  wvcii
 * timer autoconfigs using pdl routine to check cpu speed
 * value may still be overridden via config debug var timerTicksPerSec
 *
 */


#ifndef _RF__RF_TIMER_H_
#define _RF__RF_TIMER_H_

#include "rf_options.h"

#ifdef _KERNEL
#define KERNEL
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)

#ifdef KERNEL
extern unsigned int rpcc(void);
#define rf_read_cycle_counter rpcc
#else /* KERNEL */
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
/* XXX does this function even exist anywhere??? GO */
extern unsigned int rf_rpcc();
#endif
#define rf_read_cycle_counter rf_rpcc
#endif /* KERNEL */

#define RF_DEF_TIMER_MAX_VAL            0xFFFFFFFF

typedef struct RF_EtimerVal_s {
  unsigned  ccnt; /* cycle count */
} RF_EtimerVal_t;

struct RF_Etimer_s {
  RF_EtimerVal_t  st;
  RF_EtimerVal_t  et;
  unsigned long   ticks; /* elapsed time in ticks */
};

extern long rf_timer_max_val;
extern long rf_timer_ticks_per_second;
extern unsigned long rf_timer_ticks_per_usec;

#define RF_ETIMER_TICKS2US(_tcks_)  ( (_tcks_) / rf_timer_ticks_per_usec )
#define RF_ETIMER_START(_t_)  { (_t_).st.ccnt = rf_read_cycle_counter(); }
#define RF_ETIMER_STOP(_t_)   { (_t_).et.ccnt = rf_read_cycle_counter(); }
#define RF_ETIMER_EVAL(_t_) { \
	if ((_t_).st.ccnt < (_t_).et.ccnt) \
		(_t_).ticks = (_t_).et.ccnt - (_t_).st.ccnt; \
	else \
		(_t_).ticks = rf_timer_max_val - ((_t_).st.ccnt - (_t_).et.ccnt); \
}

#define RF_ETIMER_VAL_TICKS(_t_)  ((_t_).ticks)
#define RF_ETIMER_VAL_US(_t_)      (RF_ETIMER_TICKS2US((_t_).ticks))
#define RF_ETIMER_VAL_MS(_t_)      (RF_ETIMER_TICKS2US((_t_).ticks)/1000)

#endif /* __NetBSD__ || __OpenBSD__ */


#if defined(__alpha) && !defined(__NetBSD__) && !defined(__OpenBSD__)

#ifdef KERNEL
extern unsigned int rpcc();
#define rf_read_cycle_counter rpcc
#else /* KERNEL */
extern unsigned int rf_rpcc();
#define rf_read_cycle_counter rf_rpcc
#endif /* KERNEL */

#define RF_DEF_TIMER_MAX_VAL            0xFFFFFFFF

typedef struct RF_EtimerVal_s {
  unsigned  ccnt; /* cycle count */
} RF_EtimerVal_t;

struct RF_Etimer_s {
  RF_EtimerVal_t  st;
  RF_EtimerVal_t  et;
  unsigned long   ticks; /* elapsed time in ticks */
};

extern long rf_timer_max_val;
extern long rf_timer_ticks_per_second;
extern unsigned long rf_timer_ticks_per_usec;

#define RF_ETIMER_TICKS2US(_tcks_)  ( (_tcks_) / rf_timer_ticks_per_usec )
#define RF_ETIMER_START(_t_)  { (_t_).st.ccnt = rf_read_cycle_counter(); }
#define RF_ETIMER_STOP(_t_)   { (_t_).et.ccnt = rf_read_cycle_counter(); }
#define RF_ETIMER_EVAL(_t_) { \
	if ((_t_).st.ccnt < (_t_).et.ccnt) \
		(_t_).ticks = (_t_).et.ccnt - (_t_).st.ccnt; \
	else \
		(_t_).ticks = rf_timer_max_val - ((_t_).st.ccnt - (_t_).et.ccnt); \
}

#define RF_ETIMER_VAL_TICKS(_t_)  ((_t_).ticks)
#define RF_ETIMER_VAL_US(_t_)      (RF_ETIMER_TICKS2US((_t_).ticks))
#define RF_ETIMER_VAL_MS(_t_)      (RF_ETIMER_TICKS2US((_t_).ticks)/1000)

#endif /* __alpha */

#ifdef _IBMR2

extern void rf_rtclock(unsigned int *secs, unsigned int *nsecs);

#define RF_MSEC_PER_SEC 1000
#define RF_USEC_PER_SEC 1000000
#define RF_NSEC_PER_SEC 1000000000

typedef struct RF_EtimerVal_s {
  unsigned int secs;
  unsigned int nsecs;
} RF_EtimerVal_t;

struct RF_Etimer_s {
  RF_EtimerVal_t  start;
  RF_EtimerVal_t  end;
  RF_EtimerVal_t  elapsed;
};

#if RF_AIXVERS >= 4

#include <sys/time.h>

#define RF_ETIMER_START(_t_) { \
  timebasestruct_t tb; \
  tb.flag = 1; \
  read_real_time(&tb, TIMEBASE_SZ); \
  (_t_).start.secs = tb.tb_high; \
  (_t_).start.nsecs = tb.tb_low; \
}

#define RF_ETIMER_STOP(_t_) { \
  timebasestruct_t tb; \
  tb.flag = 1; \
  read_real_time(&tb, TIMEBASE_SZ); \
  (_t_).end.secs = tb.tb_high; \
  (_t_).end.nsecs = tb.tb_low; \
}

#else /* RF_AIXVERS >= 4 */

#define RF_ETIMER_START(_t_) { \
  rf_rtclock(&((_t_).start.secs), &((_t_).start.nsecs)); \
}

#define RF_ETIMER_STOP(_t_) { \
  rf_rtclock(&((_t_).end.secs), &((_t_).end.nsecs)); \
}

#endif /* RF_AIXVERS >= 4 */

#define RF_ETIMER_EVAL(_t_) { \
  if ((_t_).end.nsecs >= (_t_).start.nsecs) { \
    (_t_).elapsed.nsecs = (_t_).end.nsecs - (_t_).start.nsecs; \
    (_t_).elapsed.secs = (_t_).end.secs - (_t_).start.nsecs; \
  } \
  else { \
    (_t_).elapsed.nsecs = RF_NSEC_PER_SEC + (_t_).end.nsecs; \
    (_t_).elapsed.nsecs -= (_t_).start.nsecs; \
    (_t_).elapsed.secs = (_t_).end.secs - (_t_).start.secs + 1; \
  } \
}

#define RF_ETIMER_VAL_US(_t_) (((_t_).elapsed.secs*RF_USEC_PER_SEC)+((_t_).elapsed.nsecs/1000))
#define RF_ETIMER_VAL_MS(_t_) (((_t_).elapsed.secs*RF_MSEC_PER_SEC)+((_t_).elapsed.nsecs/1000000))

#endif /* _IBMR2 */

/*
 * XXX investigate better timing for these
 */
#if defined(hpux) || defined(sun) || defined(NETBSD_I386) || defined(OPENBSD_I386) || defined(ultrix) || defined(LINUX_I386) || defined(IRIX) || (defined(MACH) && !defined(__osf__))
#include <sys/time.h>

#define RF_USEC_PER_SEC 1000000

struct RF_Etimer_s {
  struct timeval  start;
  struct timeval  end;
  struct timeval  elapsed;
};
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#define RF_ETIMER_START(_t_) { \
  gettimeofday(&((_t_).start), NULL); \
}

#define RF_ETIMER_STOP(_t_) { \
  gettimeofday(&((_t_).end), NULL); \
}

#else
#define RF_ETIMER_START(_t_) { \
}
/* XXX these just drop off the end of the world... */
#define RF_ETIMER_STOP(_t_) { \
}
#endif 

#define RF_ETIMER_EVAL(_t_) { \
  if ((_t_).end.tv_usec >= (_t_).start.tv_usec) { \
    (_t_).elapsed.tv_usec = (_t_).end.tv_usec - (_t_).start.tv_usec; \
    (_t_).elapsed.tv_sec = (_t_).end.tv_sec - (_t_).start.tv_usec; \
  } \
  else { \
    (_t_).elapsed.tv_usec = RF_USEC_PER_SEC + (_t_).end.tv_usec; \
    (_t_).elapsed.tv_usec -= (_t_).start.tv_usec; \
    (_t_).elapsed.tv_sec = (_t_).end.tv_sec - (_t_).start.tv_sec + 1; \
  } \
}

#define RF_ETIMER_VAL_US(_t_) (((_t_).elapsed.tv_sec*RF_USEC_PER_SEC)+(_t_).elapsed.tv_usec)
#define RF_ETIMER_VAL_MS(_t_) (((_t_).elapsed.tv_sec*RF_MSEC_PER_SEC)+((_t_).elapsed.tv_usec/1000))

#endif /* hpux || sun || NETBSD_I386 || OPENBSD_I386 || ultrix || LINUX_I386 || IRIX || (MACH && !__osf__) */

#endif /* !_RF__RF_TIMER_H_ */
