/*
 * $Id: vjcomp.h,v 1.1.1.1 1997/11/23 20:27:37 brian Exp $
 */

extern void VjInit(int);
extern void SendPppFrame(struct mbuf *);
extern struct mbuf *VjCompInput(struct mbuf *, int);
