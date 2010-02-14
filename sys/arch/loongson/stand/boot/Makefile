#	$OpenBSD: Makefile,v 1.1 2010/02/14 22:39:33 miod Exp $

.include	"${.CURDIR}/../Makefile.inc"

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "loongson"
PROG=		boot

S=		${.CURDIR}/../../../..

CPPFLAGS+=	${SAABI} -mno-abicalls -D_NO_ABICALLS \
		-nostdinc -D__loongson__ \
		-I${S} -I${S}/lib/libsa \
		-I${.OBJDIR} -I${.CURDIR}

LDSCRIPT=	${.CURDIR}/ld.script
LDFLAGS+=	${SALDFLAGS} -T ${LDSCRIPT} -e __start

AFLAGS+=	${SAABI}

SRCS=		start.S
SRCS+=		conf.c cons.c dev.c devopen.c exec.c machdep.c
.PATH:		${S}/arch/loongson/loongson
SRCS+=		pmon.c pmon32.S

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
	${LD} ${LDFLAGS} -o ${PROG} ${OBJS} -L${LIBSADIR} ${LIBSA}
.else
NOPROG=
.endif

.include <bsd.prog.mk>
