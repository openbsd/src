# $OpenBSD: Makefile.inc,v 1.16 2002/06/11 15:23:28 hin Exp $

KRBDIR	= ${.CURDIR}/${KRBDEPTH}
KRBSRC	= ${.CURDIR}/${KRBDEPTH}/src

KRB5DIR	= ${KRBDIR}/../kerberosV
KRB5SRC	= ${KRB5DIR}/src

CFLAGS	+= -DHAVE_CONFIG_H -DBINDIR=\"/usr/bin\" -DSBINDIR=\"/usr/sbin\" \
	   -I${DESTDIR}/usr/include/kerberosV \
	   -I${DESTDIR}/usr/include/kerberosIV \
	   -I${KRBSRC}/include  -I${KRBSRC}/lib/sl \
	   -I${KRB5SRC}/lib/roken -I${KRB5DIR}/include

CFLAGS+=-Wall

LIBROKEN= -L${KRBDEPTH}/../kerberosV/lib/roken -lroken
LIBSL	= -L${KRBDEPTH}/lib/sl -lsl

.PATH:	${KRBSRC}/man

COMPILE_ET	= /usr/bin/compile_et

COM_ERR=	-lcom_err_pic
