
#ifndef LYSIGNAL_H
#define LYSIGNAL_H

#include <signal.h>

#ifdef VMS
extern void *VMSsignal PARAMS((int sig, void (*func)()));
#ifdef signal
#undef signal
#endif /* signal */
#define signal(a,b) VMSsignal(a,b) /* use LYCurses.c routines for interrupts */
#endif /* VMS */

#endif /* LYSIGNAL_H */
