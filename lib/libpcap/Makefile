#	$OpenBSD: Makefile,v 1.10 1999/07/20 04:49:54 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1996/05/10 21:54:24 cgd Exp $

LIB=	pcap
MAN=	pcap.3

DEFS=	-DHAVE_SYS_IOCCOM_H -DHAVE_SYS_SOCKIO_H -DHAVE_ETHER_HOSTTON \
	-DHAVE_STRERROR -DHAVE_SOCKADDR_SA_LEN -DLBL_ALIGN

CFLAGS+=-I. -I${.CURDIR} -Dyylval=pcap_yylval ${DEFS}

HDRS=	pcap.h pcap-int.h pcap-namedb.h
SRCS=	pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c \
	savefile.c bpf_filter.c bpf_image.c grammar.y scanner.l \
	pcap-bpf.c version.c
.PATH:	${.CURDIR}/../../sys/net

CLEANFILES+= grammar.c scanner.c grammar.tab.h version.c

grammar.c: grammar.y
	${YACC.y} -ppcap_yy -b ${.TARGET:R} ${.IMPSRC}
	mv ${.TARGET:R}.tab.c ${.TARGET}

scanner.c: scanner.l grammar.c
	${LEX.l} -Ppcap_yy -o${.TARGET} ${.IMPSRC}

version.c: ${.CURDIR}/VERSION
	@rm -f $@
	sed 's/.*/char pcap_version[] = "&";/' ${.CURDIR}/VERSION > $@

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
