/* $arla: /afs/stacken.kth.se/src/SourceRepository/arla/lwp/preempt.h,v 1.3 2002/06/02 11:59:54 lha Exp $ */
/* $arla: /afs/stacken.kth.se/src/SourceRepository/arla/lwp/preempt.h,v $ */

#if !defined(lint) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsidpreempt = "$arla: /afs/stacken.kth.se/src/SourceRepository/arla/lwp/preempt.h,v 1.3 2002/06/02 11:59:54 lha Exp $";
#endif

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 	Bradley White and M. Satyanarayanan			    *
\*******************************************************************/


#define PRE_PreemptMe()		lwp_cpptr->level = 0
#define PRE_BeginCritical()	lwp_cpptr->level++
#define PRE_EndCritical()	lwp_cpptr->level--

#define DEFAULTSLICE	10

int PRE_InitPreempt(struct timeval *);
int PRE_EndPreempt(void);

extern char PRE_Block;

