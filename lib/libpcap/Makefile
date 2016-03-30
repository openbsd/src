#	$OpenBSD: Makefile,v 1.25 2016/03/30 06:38:42 jmc Exp $
#	$NetBSD: Makefile,v 1.3 1996/05/10 21:54:24 cgd Exp $

LIB=	pcap
MAN=	pcap.3 pcap-filter.3

DEFS=	-DHAVE_SYS_IOCCOM_H -DHAVE_SYS_SOCKIO_H -DHAVE_ETHER_HOSTTON \
	-DHAVE_STRERROR -DHAVE_SOCKADDR_SA_LEN -DLBL_ALIGN -DHAVE_IFADDRS_H \
	-DINET6 -DHAVE_BSD_IEEE80211

CFLAGS+=-I. -I${.CURDIR} -Dyylval=pcap_yylval ${DEFS}

HDRS=	pcap.h pcap-namedb.h
SRCS=	pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c \
	savefile.c bpf_filter.c bpf_image.c grammar.y scanner.l \
	pcap-bpf.c version.c fad-getad.c
.PATH:	${.CURDIR}/../../sys/net

CLEANFILES+= grammar.c scanner.c grammar.tab.h version.c

grammar.c: grammar.y
	${YACC.y} -ppcap_yy -b ${.TARGET:R} ${.ALLSRC}
	mv ${.TARGET:R}.tab.c ${.TARGET}

scanner.c: scanner.l grammar.c
	${LEX.l} -Ppcap_yy -o${.TARGET} ${.CURDIR}/scanner.l

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
