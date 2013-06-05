#	$OpenBSD: Makefile.inc,v 1.2 2013/06/05 01:09:09 jasper Exp $

NOMAN=
S=		${.CURDIR}/../../../..
BOOTDIR=	${S}/arch/octeon/stand/boot

CPPFLAGS+=	${SAABI} -mno-abicalls -D_NO_ABICALLS -Werror \
		-nostdinc -D__octeon__ -msoft-float \
		-I${S} -I${S}/lib/libsa \
		-I${.OBJDIR} -I${BOOTDIR}

LDSCRIPT=	${.CURDIR}/../../conf/ld.script
LDFLAGS+=	${SALDFLAGS} -T ${LDSCRIPT} -Ttext=0xffffffff81000000 -e __start

AFLAGS+=	${SAABI}

.PATH:		${BOOTDIR}
SRCS=		start.S
SRCS+=		conf.c clock.c devopen.c exec.c machdep.c uart.c

#### MI boot code
.PATH:		${S}/stand/boot
SRCS+=		boot.c cmd.c vars.c

#### libkern
.PATH:		${S}/lib/libkern/arch/mips64 ${S}/lib/libkern
SRCS+=		strlcat.c strlcpy.c strlen.c
SRCS+=		udivdi3.c qdivrem.c

.if !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h mips64 ] || ln -s ${.CURDIR}/../../../mips64/include mips64)
CLEANFILES+= machine mips64
.endif

${PROG}: ${OBJS} ${LIBSA}
	${LD} ${LDFLAGS} -o ${PROG} ${OBJS} ${LIBSA}
