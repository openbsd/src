/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include "rx_locl.h"

RCSID("$arla: rx_dumptrace.c,v 1.3 2003/04/09 02:33:05 lha Exp $");

int
main(int argc, char **argv)
{
    struct rx_trace ip;
    int err = 0;
    int fd;

    setlinebuf(stdout);
    argv++;
    argc--;
    while (argc && **argv == '-') {
	if (strcmp(*argv, "-trace") == 0) {
	    strlcpy(rxi_tracename, *(++argv), sizeof(rxi_tracename));
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
    fd = open(rxi_tracename, O_RDONLY);
    if (fd < 0) {
	perror("");
	exit(errno);
    }
    while (read(fd, &ip, sizeof(struct rx_trace))) {
	printf("%9u ", (unsigned)ip.now);
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
	printf(" %3u %7u %7u      %lx.%x\n",
	       (unsigned)ip.qlen, 
	       (unsigned)ip.servicetime,
	       (unsigned)ip.waittime,
	       ip.cid,
	       ip.call);
    }
    return 0;
}
