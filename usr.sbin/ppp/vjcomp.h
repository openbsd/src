/*
 * $Id: vjcomp.h,v 1.2 1997/12/06 12:09:06 brian Exp $
 */

extern void VjInit(int);
extern void SendPppFrame(struct mbuf *);
extern struct mbuf *VjCompInput(struct mbuf *, int);
extern const char *vj2asc(u_long);
