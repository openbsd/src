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
* 								    *
\*******************************************************************/

struct TM_Elem {
    struct TM_Elem	*Next;		/* filled by package */
    struct TM_Elem	*Prev;		/* filled by package */
    struct timeval	TotalTime;	/* filled in by caller -- modified by package */
    struct timeval	TimeLeft;	/* filled by package */
    char		*BackPointer;	/* filled by caller, not interpreted by package */
};

#ifndef _TIMER_IMPL_
extern void Tm_Insert();
#define TM_Remove(list, elem) lwp_remque(elem)
extern int TM_Rescan();
extern struct TM_Elem *TM_GetExpired();
extern struct TM_Elem *TM_GetEarliest();
#endif

int TM_Init(struct TM_Elem **list);
int TM_Rescan(struct TM_Elem *tlist);
struct TM_Elem *TM_GetExpired(struct TM_Elem *tlist);
struct TM_Elem *TM_GetEarliest(struct TM_Elem *tlist);

typedef unsigned char bool; /* XXX - this is not the correct place */

bool TM_eql(struct timeval *, struct timeval *);
int TM_Final(struct TM_Elem **);
void TM_Insert(struct TM_Elem *, struct TM_Elem *);

#define FOR_ALL_ELTS(var, list, body)\
	{\
	    struct TM_Elem *_LIST_, *var, *_NEXT_;\
	    _LIST_ = (list);\
	    for (var = _LIST_ -> Next; var != _LIST_; var = _NEXT_) {\
		_NEXT_ = var -> Next;\
		body\
	    }\
	}

/* ---------------------- */

/*
 * FT - prototypes.
 * This is completly wrong place to place this.
 * But fasttime doesn't have any own prototypes.
 * (fasttime should be shot anyway)
 */

int FT_Init(int, int);
int FT_GetTimeOfDay(struct timeval * tv, struct timezone * tz);
int FT_AGetTimeOfDay(struct timeval *, struct timezone *);
int TM_GetTimeOfDay(struct timeval * tv, struct timezone * tz);
unsigned int FT_ApproxTime(void);
