#	$OpenBSD: Makefile,v 1.8 2010/07/10 17:09:15 deraadt Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	cdboot.8

.if ${MACHINE} == "i386"
PROG=	cdboot
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic
INSTALL_STRIP=

LDFLAGS+=-Ttext $(LINKADDR) -N -x -noinhibit-exec
CLEANFILES+=	crt0.o ${PROG}.whole
SRCS=	srt0.S

SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

.PATH:	${S}/lib/libkern/arch/i386 ${S}/lib/libkern
SRCS+=	strlcpy.c moddi3.c divdi3.c qdivrem.c

.PATH:	${S}/stand/boot

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) $(LDADD)
	@$(SIZE) ${PROG}
	cp ${PROG} ${PROG}.whole
	if [ -x ${.OBJDIR}/${PROG} ]; then \
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
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
