#	$OpenBSD: Makefile.inc,v 1.6 1997/09/21 11:43:59 deraadt Exp $

BIND_DIR=	${.CURDIR}/..

VER!=		awk -F' *= *' '$$1 == "VER" { print $$2 ; exit }' \
		${BIND_DIR}/Makefile

PIDDIR=		/var/run
PS=		ps
IOT=		ABRT

CONFIG?=	-DUSE_OPTIONS_H
INCLUDE?=	-I${.CURDIR} -I${BIND_DIR}

# It's in libc now.
LIBRESOLV=
