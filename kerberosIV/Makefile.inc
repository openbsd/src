# $OpenBSD: Makefile.inc,v 1.14 2001/07/08 17:56:52 brad Exp $

KRBDIR	= ${.CURDIR}/${KRBDEPTH}
KRBSRC	= ${.CURDIR}/${KRBDEPTH}/src

CFLAGS	+= -DHAVE_CONFIG_H -DBINDIR=\"/usr/bin\" -DSBINDIR=\"/usr/sbin\" \
	   -I${DESTDIR}/usr/include/kerberosIV \
	   -I${KRBSRC}/include -I${KRBSRC}/lib/roken \
	   -I${KRBDIR}/lib/roken -I${KRBSRC}/lib/sl 
CFLAGS+=-Wall

LIBROKEN= -L${KRBDEPTH}/lib/roken -lroken
LIBSL	= -L${KRBDEPTH}/lib/sl -lsl

.PATH:	${KRBSRC}/man

COMPILE_ET	= /usr/bin/compile_et

COM_ERR=	-lcom_err_pic
