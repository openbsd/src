#	$OpenBSD: Makefile,v 1.4 2003/02/20 03:20:09 deraadt Exp $

LIB=	event
SRCS=	event.c select.c kqueue.c
HDRS=	event.h
MAN=	event.3
MLINKS=	event.3 event_init.3 event.3 event_dispatch.3 event.3 event_loop.3 \
        event.3 event_set.3 event.3 event_add.3 event.3 \
        event_del.3 event.3 event_pending.3 event.3 event_initialized.3 \
        event.3 evtimer_set.3 event.3 evtimer_add.3 event.3 \
        evtimer_del.3 event.3 evtimer_pending.3 event.3 evtimer_initialized.3 \
        event.3 signal_set.3 event.3 signal_add.3 event.3 signal_del.3 \
	event.3 signal_pending.3 event.3 signal_initialized.3
CFLAGS+= -I${.CURDIR}
NOPIC=

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
