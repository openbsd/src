#	from @(#)Makefile.inc	8.1 (Berkeley) 6/1/93
#	$Id: Makefile.inc,v 1.1.1.1 1995/12/14 06:52:32 tholo Exp $

CFLAGS+=-I${.CURDIR}/../include
BINDIR?=/usr/sbin

.if exists(${.CURDIR}/../mk_cmds/obj)
MK_CMDS=${.CURDIR}/../mk_cmds/obj/mk_cmds
.else
MK_CMDS=${.CURDIR}/../mk_cmds/mk_cmds
.endif

.if exists(${.CURDIR}/../compile_et/obj)
COMPILE_ET=${.CURDIR}/../compile_et/obj/compile_et
.else
COMPILE_ET=${.CURDIR}/../compile_et/compile_et
.endif

.if exists(${.CURDIR}/../com_err/obj)
COM_ERR=-L${.CURDIR}/../com_err/obj -lcom_err_pic
.else
COM_ERR=-L${.CURDIR}/../com_err -lcom_err_pic
.endif
