/*	$OpenBSD: rx_trace.c,v 1.1.1.1 1998/09/14 21:53:17 art Exp $	*/
#include "rx_locl.h"

RCSID("$KTH: rx_trace.c,v 1.4 1998/02/22 19:55:04 joda Exp $");

#ifdef RXTRACEON
char rxi_tracename[80] = "/tmp/rxcalltrace";

#else
char rxi_tracename[80] = "\0Change This pathname (and preceding NUL) to initiate tracing";

#endif
int rxi_logfd = 0;
char rxi_tracebuf[4096];
unsigned long rxi_tracepos = 0;

struct rx_trace {
    unsigned long cid;
    unsigned short call;
    unsigned short qlen;
    unsigned long now;
    unsigned long waittime;
    unsigned long servicetime;
    unsigned long event;
};

struct rx_trace rxtinfo;

#ifdef RXDEBUG
void 
rxi_flushtrace(void)
{
    write(rxi_logfd, rxi_tracebuf, rxi_tracepos);
    rxi_tracepos = 0;
}

void 
rxi_calltrace(unsigned int event, struct rx_call *call)
{
    struct clock now;

    if (!rxi_tracename[0])
	return;

    if (!rxi_logfd) {
	rxi_logfd = open(rxi_tracename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if (!rxi_logfd)
	    rxi_tracename[0] = '\0';
    }
    clock_GetTime(&now);

    rxtinfo.event = event;
    rxtinfo.now = now.sec * 1000 + now.usec / 1000;
    rxtinfo.cid = call->conn->cid;
    rxtinfo.call = *(call->callNumber);
    rxtinfo.qlen = rx_nWaiting;
    rxtinfo.servicetime = 0;
    rxtinfo.waittime = 0;

    switch (event) {
    case RX_CALL_END:
	clock_Sub(&now, &(call->traceStart));
	rxtinfo.servicetime = now.sec * 10000 + now.usec / 100;
	if (call->traceWait.sec) {
	    now = call->traceStart;
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	call->traceWait.sec = call->traceWait.usec =
	    call->traceStart.sec = call->traceStart.usec = 0;
	break;

    case RX_CALL_START:
	call->traceStart = now;
	if (call->traceWait.sec) {
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	break;

    case RX_TRACE_DROP:
	if (call->traceWait.sec) {
	    clock_Sub(&now, &(call->traceWait));
	    rxtinfo.waittime = now.sec * 10000 + now.usec / 100;
	} else
	    rxtinfo.waittime = 0;
	break;

    case RX_CALL_ARRIVAL:
	call->traceWait = now;
    default:
	break;
    }

    memcpy(rxi_tracebuf + rxi_tracepos, &rxtinfo, sizeof(struct rx_trace));
    rxi_tracepos += sizeof(struct rx_trace);
    if (rxi_tracepos >= (4096 - sizeof(struct rx_trace)))
	rxi_flushtrace();
}
#endif

#ifdef DUMPTRACE

void
main(int argc, char **argv)
{
    struct rx_trace ip;
    int err = 0;

    setlinebuf(stdout);
    argv++;
    argc--;
    while (argc && **argv == '-') {
	if (strcmp(*argv, "-trace") == 0) {
	    strcpy(rxi_tracename, *(++argv));
	    argc--;
	} else {
	    err++;
	    break;
	}
	argv++, argc--;
    }
    if (err || argc != 0) {
	printf("usage: dumptrace [-trace pathname]");
	exit(1);
    }
    rxi_logfd = open(rxi_tracename, O_RDONLY);
    if (!rxi_logfd) {
	perror("");
	exit(errno);
    }
    while (read(rxi_logfd, &ip, sizeof(struct rx_trace))) {
	printf("%9u ", ip.now);
	switch (ip.event) {
	case RX_CALL_END:
	    putchar('E');
	    break;
	case RX_CALL_START:
	    putchar('S');
	    break;
	case RX_CALL_ARRIVAL:
	    putchar('A');
	    break;
	case RX_TRACE_DROP:
	    putchar('D');
	    break;
	default:
	    putchar('U');
	    break;
	}
	printf(" %3u %7u %7u      %x.%x\n",
	       ip.qlen, ip.servicetime, ip.waittime, ip.cid, ip.call);
    }
}

#endif				       /* DUMPTRACE */

