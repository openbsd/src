#	$OpenBSD: Makefile,v 1.1.1.1 2001/06/27 18:23:19 kjc Exp $
#	$NetBSD: Makefile,v 1.2 2000/12/16 18:57:35 thorpej Exp $

BINDIR=/usr/bin

PROG=	altqstat
SRCS=	altqstat.c quip_client.c qdisc_conf.c qdisc_cbq.c qdisc_hfsc.c \
	qdisc_cdnr.c qdisc_wfq.c qdisc_fifoq.c qdisc_red.c qdisc_rio.c \
	qdisc_blue.c qdisc_priq.c

CPPFLAGS+= -DALTQ -I${.CURDIR}/../libaltq

DPADD+=	${LIBCURSES}
LDADD+=	-lcurses

.include <bsd.prog.mk>
