#ifndef LYSIGNAL_H
#define LYSIGNAL_H

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef VMS
    extern void VMSsignal(int sig, void (*func) ());

#ifdef signal
#undef signal
#endif				/* signal */
#define signal(a,b) VMSsignal(a,b)	/* use LYCurses.c routines for interrupts */
#endif				/* VMS */

#ifdef HAVE_SIGACTION
    typedef void LYSigHandlerFunc_t (int);

/* implementation in LYUtils.c */
    extern void LYExtSignal(int sig, LYSigHandlerFunc_t *handler);

#else
#define LYExtSignal(sig,h) signal(sig, h)
#endif

#ifdef __cplusplus
}
#endif
#endif				/* LYSIGNAL_H */
