# $OpenBSD: Makefile.inc,v 1.7 2003/05/11 03:53:07 hin Exp $

KRB5DIR	= ${.CURDIR}/${KRBDEPTH}
KRB5SRC	= ${.CURDIR}/${KRBDEPTH}/src

KRB4DIR	= ${KRB5DIR}/../kerberosIV
KRB4SRC	= ${KRB4DIR}/src

CFLAGS	+= -DHAVE_CONFIG_H -DBINDIR=\"/usr/bin\" -DSBINDIR=\"/usr/sbin\"\
	   -DLIBEXECDIR=\"/usr/libexec\" -DSYSCONFDIR=\"/etc/kerberosV\" \
	   -I${DESTDIR}/usr/include/kerberosV \
	   -I${DESTDIR}/usr/include/kerberosIV \
	   -I${KRB5DIR}/include -I${KRB5SRC}/lib/roken\
	   -I${KRB5SRC}/include -I${KRB4SRC}/lib/sl 
CFLAGS+=	-Wall
.include <bsd.own.mk>				# for 'NOPIC' definition
.if !defined(NOPIC)
CFLAGS	+= -DHAVE_DLOPEN
.endif

LIBROKEN= 	-L${KRBDEPTH}/lib/roken -lroken
LIBROKEN_PIC= 	-L${KRBDEPTH}/lib/roken -lroken_pic
LIBSL=		-L${KRBDEPTH}/../kerberosIV/lib/sl -lsl

.PATH:	${KRB5SRC}/man

COMPILE_ET	= /usr/bin/compile_et

COM_ERR=	-lcom_err_pic
