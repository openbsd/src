/*	$OpenBSD: rx_trace.h,v 1.1.1.1 1998/09/14 21:53:17 art Exp $	*/
/* $Header: /home/cvs/src/usr.sbin/afs/src/rx/Attic/rx_trace.h,v 1.1.1.1 1998/09/14 21:53:17 art Exp $ */

#ifndef	_RX_TRACE
#define _RX_TRACE

#ifndef	RXDEBUG
#define rxi_calltrace(a,b)
#define rxi_flushtrace()
#else
void rxi_calltrace(unsigned int, struct rx_call*);
void rxi_flushtrace(void);

#define RX_CALL_ARRIVAL 0
#define RX_CALL_START 1
#define RX_CALL_END 2
#define RX_TRACE_DROP 3

#endif				       /* RXDEBUG */

#endif				       /* _RX_TRACE */
