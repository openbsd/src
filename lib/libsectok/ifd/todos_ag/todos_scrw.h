/*
 * $Id: todos_scrw.h,v 1.1 2001/05/22 15:35:57 rees Exp $
 */

/* SCPERF - performance evaluation */
#ifdef SCPERF
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#endif /* SCPERF */

/* open flags */
#define SCODSR		0x1	/* wait for dsr */
#define SCODCD		0x2	/* wait for dcd */
#define SCOHUP		0x4	/* send signal on card removal */
#define SCOXCTS		0x8	/* wait for no cts (todos reader) */
#define SCOXDTR		0x10	/* invert dtr (todos reader) */
#define SCOINVRT	0x20	/* inverse convention */

/* Reset flags */
#define SCRV	0x1	/* be verbose */
#define SCRLEN	0x2	/* determine length by examing atr */
#define SCRTODOS 0x4	/* Todos reader */
#define SCRFORCE 0x8	/* Talk to card even if atr is bad */

/* error codes */
#define SCEOK		0
#define SCENOTTY	1	/* no such tty */
#define SCENOMEM	2	/* malloc (or similar) failed */
#define SCTIMEO		3	/* time out */
#define SCESLAG		4	/* slag (no atr) */
#define SCENOSUPP	5	/* card type not supported */
#define SCENOCARD	6	/* no card in reader */
#define SCENOIMPL	7
#define SCEDRVR 	8
#define SCECOMM 	9
#define SCECLOSED	10
#define SCEUNKNOWN	11

/* Extra tags for things they forgot to put in the ifd interface */
#define SCTAG_IFD_ATRLEN 0x6601
#define SCTAG_IFD_CARDPRESENT 0x301
#define TAG_OPEN_FLAGS  0x800
#define TAG_RESET_FLAGS 0x801

extern struct scparam {
    int t, etu, cwt, bwt, n;
} scparam[];

extern unsigned char scinvert[];

/* forward declarations */

int todos_scopen(int ttyn, int flags, int *ep);
int todos_scsetflags(int ttyn, int flags, int mask);
int todos_scrw(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p);
int todos_sccardpresent(int ttyn);
int todos_scdsr(int ttyn);
int todos_scclose(int ttyn);
int todos_scxreset(int ttyn, int flags, unsigned char *atr, int *ep);
int todos_scdtr(int ttyn, int cmd);
int todos_scgetc(int ttyn, unsigned char *cp, int ms);
int todos_scputc(int ttyn, int ic);
int todos_scputblk(int ttyn, unsigned char *bp, int n);
void todos_scsleep(int ms);
void todos_scdrain(int ttyn);
int todos_scioT1(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p);
int todos_scioT1Iblk(int ttyn, int ilen, unsigned char *ibuf, unsigned char *obuf);
int todos_scioT1pkt(int ttyn, unsigned char *ibuf, unsigned char *obuf);
int todos_get_atr(int ttyn, int flags, unsigned char *atr, struct scparam *param);

/* SCPERF - performance evaluation */
#ifdef SCPERF
#ifdef SCPERF_FIRST_APPEARANCE

#define MAX_EVENTS 1024

struct timeval perf_tv[MAX_EVENTS];
char *perf_buf[MAX_EVENTS];
int perf_num = 0; 

void print_time ()
{
    int i;

    for (i = 0 ; i < perf_num ; i ++ ) {
	printf ("%d.%06d: %s\n",
		perf_tv[i].tv_sec, perf_tv[i].tv_usec, perf_buf[i]);
    }
    return; 
}

#define SetTime(x) \
  gettimeofday(&(perf_tv[perf_num]), NULL); \
  perf_buf[perf_num] = x; \
  perf_num++; \
  if (perf_num >= MAX_EVENTS) {\
    fprintf (stderr, "SetTime overflow %d\n", MAX_EVENTS); \
    exit (1); \
  }

#else /* !SCPERF_FIRST_APPEARANCE */
extern struct timeval perf_tv[];
extern char *perf_buf[];
extern int perf_num;

#define MAX_EVENTS 1024

#define SetTime(x) \
  gettimeofday(&(perf_tv[perf_num]), NULL); \
  perf_buf[perf_num] = x; \
  perf_num++; \
  if (perf_num >= MAX_EVENTS) {\
    fprintf (stderr, "SetTime overflow %d\n", MAX_EVENTS); \
    exit (1); \
  }
#endif /* SCPERF_FIRST_APPEARANCE */

#else /* !SCPERF */
#define SetTime(x)
#define print_time() ; 
#endif /* SCPERF */

/* macros */
#ifdef SCFS
#define ADEBMISC        0x00000001	/* misc debugging */
#define MESSAGE1(x) arla_warnx (ADEBMISC,x)
#define MESSAGE2(x,y) arla_warnx (ADEBMISC,x,y)
#define MESSAGE3(x,y,z) arla_warnx (ADEBMISC,x,y,z)
#define MESSAGE4(x,y,z,u) arla_warnx (ADEBMISC,x,y,z,u)
#define MESSAGE5(x,y,z,u,v) arla_warnx (ADEBMISC,x,y,z,u,v)
#define MESSAGE6(x,y,z,u,v,w) arla_warnx (ADEBMISC,x,y,z,u,v,w)
#else 
#define MESSAGE1(x) fprintf(stderr,x)
#define MESSAGE2(x,y) fprintf(stderr,x,y)
#define MESSAGE3(x,y,z) fprintf(stderr,x,y,z)
#define MESSAGE4(x,y,z,u) fprintf(stderr,x,y,z,u)
#define MESSAGE5(x,y,z,u,v) fprintf(stderr,x,y,z,u,v)
#define MESSAGE6(x,y,z,u,v,w) fprintf(stderr,x,y,z,u,v,w)
#endif /* SCFS */

/*
copyright 1997
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/
