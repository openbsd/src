%#ifndef _AFSUUID_COMMON_
%#define _AFSUUID_COMMON_

/* $arla: afsuuid.h,v 1.1 2002/04/16 18:44:07 lha Exp $ */

#ifndef AFSUUID_GENERATE
#define AFSUUID_GENERATE  __attribute__((__nogenerate__))
#endif

struct afsUUID {
     u_long time_low;
     u_short time_mid;
     u_short time_hi_and_version;
     char clock_seq_hi_and_reserved;
     char clock_seq_low;
     char node[6];
} AFSUUID_GENERATE;

%#endif /* _AFSUUID_COMMON_ */
