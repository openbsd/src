/*
 * $Id: async.h,v 1.1.1.1 1997/11/23 20:27:33 brian Exp $
 */

extern void AsyncInit(void);
extern void SetLinkParams(struct lcpstate *);
extern void AsyncOutput(int, struct mbuf *, int);
extern void AsyncInput(u_char *, int);
