#	$OpenBSD: Makefile.inc,v 1.4 1997/03/13 08:57:57 downsj Exp $
#	$NetBSD: Makefile.inc,v 1.1 1996/02/02 15:25:35 mrg Exp $
# from: $Id: Makefile.inc,v 8.3 1995/12/31 23:28:00 vixie Exp 

BIND_DIR=	${.CURDIR}/..

VER!=		awk -F' *= *' '$$1 == "VER" { print $$2 ; exit }' \
		${BIND_DIR}/Makefile

PIDDIR=		/var/run
PS=		ps
IOT=		ABRT

CONFIG?=	-DUSE_OPTIONS_H
INCLUDE?=	-I${.CURDIR} -I${BIND_DIR} -I${BIND_DIR}/include

# we need to use our version of libresolv.
.if exists(${BIND_DIR}/libresolv/${__objdir})
LIBRESOLV=	${BIND_DIR}/libresolv/${__objdir}/libresolv.a
.else
LIBRESOLV=	${BIND_DIR}/libresolv/libresolv.a
.endif
