# $OpenBSD: Makefile.inc,v 1.10 2000/03/27 12:53:41 maja Exp $

KRBVERSION = "krb4-1.0"

KRBDIR	= ${.CURDIR}/${KRBDEPTH}
KRBSRC	= ${.CURDIR}/${KRBDEPTH}/src

CFLAGS	+= -DHAVE_CONFIG_H -DBINDIR=\"/usr/bin\" \
	   -I${DESTDIR}/usr/include/kerberosIV \
	   -I${KRBSRC}/include -I${KRBSRC}/lib/roken \
	   -I${KRBDIR}/lib/roken -I${KRBSRC}/lib/sl 

LIBROKEN= -L${KRBDEPTH}/lib/roken -lroken
LIBSL	= -L${KRBDEPTH}/lib/sl -lsl

.PATH:	${KRBSRC}/man

COMPILE_ET	= /usr/bin/compile_et

.if (${MACHINE_ARCH} != "mips")
COM_ERR=	-lcom_err_pic
.else
COM_ERR=	-lcom_err
.endif
