#	$OpenBSD: Makefile,v 1.7 2003/09/23 08:52:04 markus Exp $

LIB=	event
SRCS=	event.c select.c poll.c signal.c kqueue.c
HDRS=	event.h
MAN=	event.3
MLINKS=	event.3 event_init.3 event.3 event_dispatch.3 event.3 event_loop.3 \
        event.3 event_set.3 event.3 event_add.3 event.3 \
        event_del.3 event.3 event_pending.3 event.3 event_initialized.3 \
        event.3 evtimer_set.3 event.3 evtimer_add.3 event.3 \
        evtimer_del.3 event.3 evtimer_pending.3 event.3 evtimer_initialized.3 \
        event.3 signal_set.3 event.3 signal_add.3 event.3 signal_del.3 \
	event.3 signal_pending.3 event.3 signal_initialized.3

CFLAGS+= -I${.CURDIR} \
	-DHAVE_WORKING_KQUEUE \
	-DHAVE_SELECT \
	-DHAVE_POLL \
	-DHAVE_SYS_TIME_H \

NOPIC=

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
