
#ifndef LYSIGNAL_H
#define LYSIGNAL_H

#include <signal.h>

#ifdef VMS
extern void VMSsignal PARAMS((int sig, void (*func)()));
#ifdef signal
#undef signal
#endif /* signal */
#define signal(a,b) VMSsignal(a,b) /* use LYCurses.c routines for interrupts */
#endif /* VMS */

#if HAVE_SIGACTION
typedef void LYSigHandlerFunc_t PARAMS((int));
/* implementation in LYUtils.c */
extern void LYExtSignal PARAMS((int sig, LYSigHandlerFunc_t * handler));
#else
#define LYExtSignal(sig,h) signal(sig, h)
#endif

#endif /* LYSIGNAL_H */
