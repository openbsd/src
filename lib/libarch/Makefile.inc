#	$OpenBSD: Makefile.inc,v 1.1 1996/03/19 23:14:52 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.1 1996/02/21 02:43:57 jtk Exp $
#
# This Makefile includes boiler-plate stuff included by each subdir's Makefile.

.if exists(${.CURDIR}/../${MACHINE_ARCH}) && (${MACHINE} != ${MACHINE_ARCH})
.PATH:	${.CURDIR}/../${MACHINE_ARCH}
.include "${.CURDIR}/../${MACHINE_ARCH}/Makefile.inc"
AINC+= -I${.CURDIR}/../${MACHINE_ARCH}
.endif

OBJS+= ${ASM}
POBJS+=	${ASM:.o=.po}
CLEANFILES+= ${ASM} ${POBJS}

LIB=    ${MACHINE}
