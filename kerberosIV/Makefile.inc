#	$OpenBSD: Makefile.inc,v 1.7 1996/11/15 09:21:25 downsj Exp $

CFLAGS+=-I${.CURDIR}/../include
BINDIR?=/usr/sbin

.if defined(OBJMACHINE)
__objdir=	obj.${MACHINE}
.else
__objdir=	obj
.endif

.if (${MACHINE_ARCH} != "mips")
COM_ERR=	-lcom_err_pic
.else
COM_ERR=	-lcom_err
.endif
