#	$OpenBSD: Makefile,v 1.8 2013/11/13 22:37:00 deraadt Exp $

LIB=	wrap
SRCS=	hosts_access.c options.c shell_cmd.c rfc931.c eval.c \
	hosts_ctl.c refuse.c percent_x.c clean_exit.c \
	socket.c update.c misc.c diag.c percent_m.c
HDRS=	tcpd.h

# Configuration options for libwrap.
CFLAGS+=-DPROCESS_OPTIONS -DFACILITY=LOG_AUTH -DSEVERITY=LOG_INFO \
	-DRFC931_TIMEOUT=10 -DHOSTS_ACCESS -DALWAYS_HOSTNAME \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DNETGROUP -DSYS_ERRLIST_DEFINED -D_TCPD_PRIVATE
CFLAGS+=-DINET6

MAN=	hosts_access.3 hosts_access.5 hosts_options.5 rfc1413.3
MLINKS+=hosts_access.5 hosts.allow.5
MLINKS+=hosts_access.5 hosts.deny.5
MLINKS+=hosts_access.3 hosts_ctl.3
MLINKS+=hosts_access.3 request_init.3
MLINKS+=hosts_access.3 request_set.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
