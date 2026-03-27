#	$OpenBSD: Makefile,v 1.1 2026/03/27 05:06:33 dgl Exp $

PROG=	open
LDFLAGS=	-static

run-no-pledge: ${PROG}
	./${PROG} "[NO PLEDGE]" /etc/myname
REGRESS_TARGETS+=	run-no-pledge

run-unknown-file: ${PROG}
	if ./${PROG} "stdio" /etc/myname; \
	    then false; else [[ $$? == 134 ]]; fi
REGRESS_TARGETS+=	run-unknown-file

run-hosts: ${PROG}
	./${PROG} "stdio dns fattr" /etc/hosts
REGRESS_TARGETS+=	run-hosts

run-hosts-no-dns: ${PROG}
	if ./${PROG} "stdio" /etc/hosts; \
	    then false; else [[ $$? == 134 ]]; fi
REGRESS_TARGETS+=	run-hosts-no-dns

run-hosts-no-dns-error: ${PROG}
	if ./${PROG} "stdio error" /etc/hosts; \
	    then false; else [[ $$? == 2 ]]; fi
REGRESS_TARGETS+=	run-hosts-no-dns-error

run-hosts-no-dns-but-rpath: ${PROG}
	./${PROG} "stdio rpath fattr" /etc/hosts
REGRESS_TARGETS+=	run-hosts-no-dns-but-rpath

run-devnull: ${PROG}
	./${PROG} "stdio" /dev/null
REGRESS_TARGETS+=	run-devnull

run-empty-promises: ${PROG}
	if ./${PROG} "" /dev/null; \
	    then false; else [[ $$? == 134 ]]; fi
REGRESS_TARGETS+=	run-empty-promises

run-zoneinfo: ${PROG}
	./${PROG} "stdio fattr" /usr/share/zoneinfo/Asia/Ulaanbaatar
REGRESS_TARGETS+=	run-zoneinfo

run-zoneinfo-dotdot: ${PROG}
	if ./${PROG} "stdio" /usr/share/zoneinfo/Asia/..; \
	    then false; else [[ $$? == 134 ]]; fi
REGRESS_TARGETS+=	run-zoneinfo-dotdot

.include <bsd.regress.mk>
