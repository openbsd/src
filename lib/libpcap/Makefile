#	$OpenBSD: Makefile,v 1.17 2005/11/24 20:49:20 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1996/05/10 21:54:24 cgd Exp $

LIB=	pcap
WANTLINT=
MAN=	pcap.3
MLINKS=	pcap.3 pcap_open_live.3 pcap.3 pcap_open_offline.3 \
        pcap.3 pcap_dump_open.3 pcap.3 pcap_lookupdev.3 pcap.3 \
        pcap_lookupnet.3 pcap.3 pcap_dispatch.3 pcap.3 pcap_loop.3 \
        pcap.3 pcap_dump.3 pcap.3 pcap_inject.3 pcap.3 pcap_compile.3 \
        pcap.3 pcap_setfilter.3 pcap.3 pcap_next.3 pcap.3 pcap_datalink.3 \
        pcap.3 pcap_snapshot.3 pcap.3 pcap_is_swapped.3 pcap.3 \
        pcap_major_version.3 pcap.3 pcap_minor_version.3 pcap.3 \
        pcap_stats.3 pcap.3 pcap_file.3 pcap.3 pcap_fileno.3 pcap.3 \
        pcap_perror.3 pcap.3 pcap_geterr.3 pcap.3 pcap_strerror.3 \
        pcap.3 pcap_close.3 pcap.3 pcap_dump_close.3 \
	pcap.3 pcap_freecode.3 pcap.3 pcap_breakloop.3 \
	pcap.3 pcap_findalldevs.3 pcap.3 pcap_freealldevs.3 \
	pcap.3 pcap_getnonblock.3 pcap.3 pcap_setnonblock.3 \
	pcap.3 pcap_set_datalink.3 pcap.3 pcap_list_datalinks.3 \
	pcap.3 pcap_open_dead.3 pcap.3 pcap_lib_version.3 \
	pcap.3 pcap_datalink_val_to_name.3 \
	pcap.3 pcap_datalink_val_to_description.3 \
	pcap.3 pcap_datalink_name_to_val.3

DEFS=	-DHAVE_SYS_IOCCOM_H -DHAVE_SYS_SOCKIO_H -DHAVE_ETHER_HOSTTON \
	-DHAVE_STRERROR -DHAVE_SOCKADDR_SA_LEN -DLBL_ALIGN -DHAVE_IFADDRS_H \
	-DINET6

CFLAGS+=-I. -I${.CURDIR} -Dyylval=pcap_yylval ${DEFS}

HDRS=	pcap.h pcap-int.h pcap-namedb.h
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
