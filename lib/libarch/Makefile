#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:41:19 deraadt Exp $

.if exists(${.CURDIR}/${MACHINE})
.PATH:	${.CURDIR}/${MACHINE}
.include "${.CURDIR}/${MACHINE}/Makefile.inc"
AINC+= -I${.CURDIR}/${MACHINE}
.endif

.if exists(${.CURDIR}/${MACHINE_ARCH}) && (${MACHINE} != ${MACHINE_ARCH})
.PATH:	${.CURDIR}/${MACHINE_ARCH}
.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
AINC+= -I${.CURDIR}/${MACHINE_ARCH}
.endif

OBJS+= ${ASM}
POBJS+=	${ASM:.o=.po}
CLEANFILES+= ${ASM} ${POBJS}


.if exists(${.CURDIR}/${MACHINE}) || exists(${.CURDIR}/${MACHINE_ARCH})
# then there's a lib for this machine/machine architecture
LIB=	${MACHINE}
.include <bsd.lib.mk>
.else
# there's NOT, so get the targets right
all:
clean:
cleandir:
depend:
install:
lint:
obj:
.endif
