LIB=	event
SRCS=	event.c select.c kqueue.c
HDRS=	event.h
MAN=	event.3
MLINKS=	event.3 event_init.3 event.3 event_dispatch.3 event.3 event_loop.3 \
        event.3 event_set.3 event.3 event_add.3 event.3 \
        event_del.3 event.3 event_pending.3 event.3 event_initialized.3 \
        event.3 timeout_set.3 event.3 timeout_add.3 event.3 \
        timeout_del.3 event.3 timeout_pending.3 event.3 timeout_initialized.3 \
        event.3 signal_set.3 event.3 signal_add.3 event.3 signal_del.3 \
	event.3 signal_pending.3 event.3 signal_initalized.3
CFLAGS+= -I${.CURDIR}
NOPIC=

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
