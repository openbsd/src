#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:46:24 deraadt Exp $

CFLAGS += -I${.CURDIR}/../mkhits -I${.CURDIR}/.. -I.

NOMAN = I mean it!

SRCS = mkastosc.c dohits.c asc_ebc.c ebc_disp.c
PROG = mkastosc

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../mkhits ${.CURDIR}/../../api
