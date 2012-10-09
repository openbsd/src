#	$OpenBSD: Makefile,v 1.12 2012/10/09 11:43:22 jsing Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	cdboot.8

.if ${MACHINE} == "i386"
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	cdboot
SRCS=	srt0.S
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -nopie
INSTALL_STRIP=

CLEANFILES+=	crt0.o ${PROG}.whole

SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c
LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

.PATH:	${S}/lib/libkern/arch/i386 ${S}/lib/libkern
SRCS+=	strlcpy.c moddi3.c divdi3.c qdivrem.c

.PATH:	${S}/stand/boot

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) $(LDADD)
	@$(SIZE) ${PROG}
	cp ${PROG} ${PROG}.whole
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.else
NOPROG=
.endif

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CPPFLAGS+=-DLINKADDR=${LINKADDR}
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
CFLAGS+=-DOSREV=\"${OSREV}\" -DMACHINE=\"${MACHINE}\"
CFLAGS+=-DKERNEL=\"/${OSREV}/${MACHINE}/bsd.rd\"
CFLAGS+=-fno-pie
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
AFLAGS+=-fno-pie
