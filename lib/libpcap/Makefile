#	$NetBSD: Makefile,v 1.1 1995/03/06 11:38:04 mycroft Exp $

LIB=	pcap
MAN=	pcap.3

CFLAGS+=-I. -I${.CURDIR}

HDRS=	pcap.h pcap-namedb.h
SRCS=	pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c \
	savefile.c bpf_filter.c bpf_image.c grammar.y scanner.l \
	pcap-bpf.c
.PATH:	${.CURDIR}/../../sys/net

CLEANFILES+= grammar.c scanner.c y.tab.h

beforeinstall:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		{ j="install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
		echo $$j; $$j; } \
	done

.include <bsd.lib.mk>
