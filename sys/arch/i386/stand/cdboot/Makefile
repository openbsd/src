#	$OpenBSD: Makefile,v 1.5 2007/05/29 00:03:10 deraadt Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	cdboot.8

.if ${MACHINE} == "i386"
PROG=	cdboot
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic
INSTALL_STRIP=

LDFLAGS+=-Ttext $(LINKADDR) -N -x -noinhibit-exec
CLEANFILES+=	crt0.o
SRCS=	srt0.S

SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ} ${LIBKERN}
DPADD=	${LIBSA} ${LIBZ} ${LIBKERN}

.PATH:	${S}/stand/boot

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) $(LDADD)
	@$(SIZE) ${PROG}
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
