#	$OpenBSD: Makefile.inc,v 1.6 1996/11/11 05:43:08 tholo Exp $

CFLAGS+=-I${.CURDIR}/../include
BINDIR?=/usr/sbin

.if defined(OBJMACHINE)
__objdir=	obj.${MACHINE}
.else
__objdir=	obj
.endif

.if exists(${.CURDIR}/../mk_cmds/${__objdir})
MK_CMDS=${.CURDIR}/../mk_cmds/${__objdir}/mk_cmds
.else
MK_CMDS=${.CURDIR}/../mk_cmds/mk_cmds
.endif

.if (${MACHINE_ARCH} != "mips")
COM_ERR=	-lcom_err_pic
.else
COM_ERR=	-lcom_err
.endif
