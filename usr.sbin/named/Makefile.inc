#	$OpenBSD: Makefile.inc,v 1.7 1998/05/23 19:24:45 millert Exp $

BIND_DIR=	${.CURDIR}/..

VER!=		awk -F' *= *' '$$1 == "VER" { print $$2 ; exit }' \
		${BIND_DIR}/Makefile

PIDDIR=		/var/run
CHROOTDIR=	/var/named
PS=		ps
IOT=		ABRT

CONFIG?=	-DUSE_OPTIONS_H
INCLUDE?=	-I${.CURDIR} -I${BIND_DIR}

# It's in libc now.
LIBRESOLV=
