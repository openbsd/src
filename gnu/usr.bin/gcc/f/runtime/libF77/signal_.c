#include "f2c.h"

#ifndef RETSIGTYPE
/* we shouldn't rely on this... */
#ifdef KR_headers
#define RETSIGTYPE int
#else
#define RETSIGTYPE void
#endif
#endif
typedef RETSIGTYPE (*sig_type)();

#ifdef KR_headers
extern sig_type signal();

ftnint signal_(sigp, proc) integer *sigp; sig_type proc;
#else
#include <signal.h>
typedef int (*sig_proc)(int);

ftnint signal_(integer *sigp, sig_proc proc)
#endif
{
	int sig;
	sig = (int)*sigp;

	return (ftnint)signal(sig, (sig_type)proc);
	return 0;
	}
