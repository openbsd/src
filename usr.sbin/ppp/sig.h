/*
 * $Id: sig.h,v 1.1.1.1 1997/11/23 20:27:36 brian Exp $
 */

typedef void (*sig_type)(int);

/* Call this instead of signal() */
extern sig_type pending_signal(int, sig_type);

/* Call this when you want things to *actually* happen */
extern void handle_signals(void);
