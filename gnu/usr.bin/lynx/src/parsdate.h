/* $LynxId: parsdate.h,v 1.2 2010/10/31 17:56:22 tom Exp $ */
#ifndef PARSDATE_H
#define PARSDATE_H

#ifdef __cplusplus
extern "C" {
#endif
#include <LYUtils.h>
#define ARRAY_SIZE(array)       ((int) (sizeof(array) / sizeof(array[0])))
    typedef struct _TIMEINFO {
	time_t time;
	long usec;
	long tzone;
    } TIMEINFO;

    extern time_t parsedate(char *p, TIMEINFO * now);

#ifdef __cplusplus
}
#endif
#endif				/* PARSDATE_H */
