/*      $OpenBSD: wdcdbg.h,v 1.1 2000/10/28 18:08:46 csapuntz Exp $      */

#define WDCDEBUG_PROBE

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#define DEBUG_STATUSX 0x20
#define DEBUG_SDRIVE 0x40
#define DEBUG_DETACH 0x80

extern int wdcdebug_mask; /* init'ed in wdc.c */

#ifdef WDCDEBUG
extern int wdcdebug_mask; /* init'ed in wdc.c */
#define WDCDEBUG_PRINT(args, level) \
        if (wdcdebug_mask & (level)) \
		printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif

#if defined(WDCDEBUG) || defined(WDCDEBUG_PROBE)
#define WDCDEBUG_PRINT_PROBE(args)  if (wdcdebug_mask & DEBUG_PROBE) printf args
#else
#define WDCDEBUG_PRINT_PROBE(args)
#endif
