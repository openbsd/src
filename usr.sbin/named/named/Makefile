#	$OpenBSD: Makefile,v 1.5 2002/11/14 06:56:57 millert Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/02 15:28:00 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:45 vixie Exp 

.PATH: ${.CURDIR}/../man

PROG=	named
SRCS=	version.c db_dump.c db_glue.c db_load.c db_lookup.c db_reload.c \
	db_save.c db_secure.c db_update.c ns_forw.c \
	ns_init.c ns_main.c ns_maint.c ns_ncache.c ns_req.c ns_resp.c \
	ns_sort.c ns_stats.c ns_udp.c ns_validate.c storage.c tree.c
MAN=	named.8
CLEANFILES+= version.c

CFLAGS+=${INCLUDE} ${CONFIG}

version.c: Version.c ${.CURDIR}/../Makefile
	(u=root d=`pwd` h=localhost t=`date +%c`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${.CURDIR}/Version.c > version.c)

.include <bsd.prog.mk>
.include "../../Makefile.inc"
