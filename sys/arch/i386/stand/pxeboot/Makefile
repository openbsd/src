#	$OpenBSD: Makefile,v 1.1 2004/03/19 13:48:19 tom Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	pxeboot.8

.if ${MACHINE} == "i386"
PROG=	pxeboot
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic
INSTALL_STRIP=

LDFLAGS+=-Ttext $(LINKADDR) -N -x -noinhibit-exec
CLEANFILES+=	crt0.o
SRCS=	srt0.S

SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c devopen.c net.c open.c
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

.PATH:	${S}/stand/boot

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) $(LDADD)
	@$(SIZE) ${PROG}
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
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
