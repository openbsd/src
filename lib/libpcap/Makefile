#	$OpenBSD: Makefile,v 1.5 1996/06/18 16:26:20 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1996/05/10 21:54:24 cgd Exp $

LIB=	pcap
MAN=	pcap.3

CFLAGS+=-I. -I${.CURDIR} -Dyylval=pcap_yylval

HDRS=	pcap.h pcap-namedb.h
SRCS=	pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c \
	savefile.c bpf_filter.c bpf_image.c grammar.y scanner.l \
	pcap-bpf.c
.PATH:	${.CURDIR}/../../sys/net

CLEANFILES+= grammar.c scanner.c grammar.tab.h

grammar.c:	grammar.y
	${YACC.y} -ppcap_yy -b ${.TARGET:R} ${.IMPSRC}
	mv ${.TARGET:R}.tab.c ${.TARGET}

scanner.c:	scanner.l grammar.c
	${LEX.l} -Ppcap_yy -o${.TARGET} ${.IMPSRC}

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
