#	$OpenBSD: Makefile,v 1.28 2008/05/02 09:18:26 jmc Exp $

LIB=	event
WANTLINT=
SRCS=	buffer.c evbuffer.c evdns.c event.c event_tagging.c kqueue.c \
	log.c poll.c select.c signal.c
HDRS=	event.h evdns.h
MAN=	evdns.3 event.3
MLINKS=	event.3 event_init.3 event.3 event_dispatch.3 event.3 event_loop.3 \
	event.3 event_loopexit.3 event.3 event_set.3 \
	event.3 event_base_dispatch.3 event.3 event_base_loop.3 \
	event.3 event_base_loopexit.3 event.3 event_base_set.3 \
	event.3 event_base_free.3 event.3 event_add.3 event.3 event_del.3 \
	event.3 event_once.3 event.3 event_base_once.3 \
	event.3 event_pending.3 event.3 event_initialized.3 \
	event.3 event_priority_init.3 event.3 event_priority_set.3 \
	event.3 evtimer_set.3 event.3 evtimer_add.3 \
	event.3 evtimer_del.3 event.3 evtimer_pending.3 \
	event.3 evtimer_initialized.3 event.3 signal_set.3 \
	event.3 signal_add.3 event.3 signal_del.3 event.3 signal_pending.3 \
	event.3 signal_initialized.3 event.3 bufferevent_new.3 \
	event.3 bufferevent_free.3 event.3 bufferevent_write.3 \
	event.3 bufferevent_write_buffer.3 event.3 bufferevent_read.3 \
	event.3 bufferevent_enable.3 event.3 bufferevent_disable.3 \
	event.3 bufferevent_settimeout.3 event.3 bufferevent_base_set.3 \
	event.3 evbuffer_new.3 event.3 evbuffer_free.3 event.3 evbuffer_add.3 \
	event.3 evbuffer_add_buffer.3 event.3 evbuffer_add_printf.3 \
	event.3 evbuffer_add_vprintf.3 event.3 evbuffer_drain.3 \
	event.3 evbuffer_write.3 event.3 evbuffer_read.3 \
	event.3 evbuffer_find.3 event.3 evbuffer_readline.3 \
	evdns.3 evdns_init.3 evdns.3 evdns_shutdown.3 \
	evdns.3 evdns_err_to_string.3 evdns.3 evdns_nameserver_add.3 \
	evdns.3 evdns_count_nameservers.3 \
	evdns.3 evdns_clear_nameservers_and_suspend.3 \
	evdns.3 evdns_resume.3 evdns.3 evdns_nameserver_ip_add.3 \
	evdns.3 evdns_resolve_ipv4.3 evdns.3 evdns_resolve_reverse.3 \
	evdns.3 evdns_resolv_conf_parse.3 evdns.3 evdns_search_clear.3 \
	evdns.3 evdns_search_add.3 evdns.3 evdns_search_ndots_set.3 \
	evdns.3 evdns_set_log_fn.3

CFLAGS+= -I${.CURDIR} -DNDEBUG \
	-DDNS_USE_ARC4RANDOM_FOR_ID \
	-DHAVE_CLOCK_GETTIME \
	-DHAVE_CLOCK_MONOTONIC \
	-DHAVE_FCNTL_H \
	-DHAVE_GETADDRINFO \
	-DHAVE_GETNAMEINFO \
	-DHAVE_POLL \
	-DHAVE_SELECT \
	-DHAVE_SETFD \
	-DHAVE_STDARG_H \
	-DHAVE_STDINT_H \
	-DHAVE_STRSEP \
	-DHAVE_STRTOK_R \
	-DHAVE_STRUCT_IN6_ADDR \
	-DHAVE_SYS_IOCTL_H \
	-DHAVE_SYS_TIME_H \
	-DHAVE_UNISTD_H \
	-DHAVE_UNISTD_H \
	-DHAVE_VASPRINTF \
	-DHAVE_WORKING_KQUEUE

CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wcast-qual
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wcast-align
CDIAGFLAGS+=	-Wbad-function-cast

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
