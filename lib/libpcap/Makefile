#	$OpenBSD: Makefile,v 1.3 1996/05/06 21:44:25 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1996/04/29 20:32:24 jtc Exp $

LIB=	pcap
MAN=	pcap.3

CFLAGS+=-I. -I${.CURDIR}

HDRS=	pcap.h pcap-namedb.h
SRCS=	pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c \
	savefile.c bpf_filter.c bpf_image.c grammar.y scanner.l \
	pcap-bpf.c
.PATH:	${.CURDIR}/../../sys/net

CLEANFILES+= grammar.c scanner.c y.tab.h

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		{ j="install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
		echo $$j; $$j; } \
	done

.include <bsd.lib.mk>
