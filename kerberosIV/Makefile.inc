# $OpenBSD: Makefile.inc,v 1.13 2001/06/28 04:53:44 hin Exp $

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

.if (${MACHINE_ARCH} != "mips")
COM_ERR=	-lcom_err_pic
.else
COM_ERR=	-lcom_err
.endif
