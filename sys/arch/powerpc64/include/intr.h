#define IPL_NONE	0
#define IPL_SOFTCLOCK	2
#define IPL_SOFTNET	3
#define IPL_SOFTTTY	4
#define IPL_BIO		5
#define IPL_NET		6
#define IPL_TTY		7
#define IPL_VM		8
#define IPL_AUDIO	9
#define IPL_CLOCK	10
#define IPL_STATCLOCK	IPL_CLOCK
#define IPL_HIGH	15
#define IPL_MPFLOOR	IPL_TTY

#define spl0()		9
#define splsoftclock()	0
#define splsoftnet()	0
#define splsofttty()	0
#define splbio()	0
#define splnet()	0
#define spltty()	0
#define splvm()		0
#define splclock()	0
#define splstatclock()	0
#define splsched()	0
#define splhigh()	0
#define splraise(s)	0
#define splx(s)		(void)s

#define splassert(x)

#include <machine/softintr.h>
