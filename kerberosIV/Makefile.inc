#	$OpenBSD: Makefile.inc,v 1.3 1996/07/16 07:49:08 pefo Exp $

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

.if exists(${.CURDIR}/../compile_et/${__objdir})
COMPILE_ET=${.CURDIR}/../compile_et/${__objdir}/compile_et
.else
COMPILE_ET=${.CURDIR}/../compile_et/compile_et
.endif

.if (${MACHINE_ARCH} != "mips")
COM_ERR_PIC=	-lcom_err_pic
.else
COM_ERR_PIC=	-lcom_err
.endif

.if exists(${.CURDIR}/../com_err/${__objdir})
COM_ERR=-L${.CURDIR}/../com_err/${__objdir} ${COM_ERR_PIC}
.else
COM_ERR=-L${.CURDIR}/../com_err ${COM_ERR_PIC}
.endif
