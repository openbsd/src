#	$NetBSD: Makefile,v 1.47 1995/11/23 02:20:59 jtc Exp $
#	@(#)Makefile	8.2 (Berkeley) 2/3/94
#
# All library objects contain sccsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_SCCS and -DSYSLIBC_SCCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_SCCS from CFLAGS.
#
# The NLS (message catalog) functions are always in libc.  To choose that
# strerror(), perror(), strsignal(), psignal(), etc. actually call the NLS
# functions, put -DNLS on the CFLAGS line below.
#
# The YP functions are always in libc. To choose that getpwent() and friends
# actually call the YP functions, put -DYP on the CFLAGS line below.

LIB=c
CFLAGS+=-DNLS -DYP -DLIBC_SCCS -DSYSLIBC_SCCS -I${.CURDIR}/include
AINC=	-I${.CURDIR}/arch/${MACHINE_ARCH}
.if defined(DESTDIR)
AINC+=	-nostdinc -idirafter ${DESTDIR}/usr/include
.endif
CLEANFILES+=tags

.if exists (${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc)
.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}
.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"
.endif

.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/compat-43/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/gmon/Makefile.inc"
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/nls/Makefile.inc"
.if (${MACHINE_ARCH} != "alpha")
.include "${.CURDIR}/quad/Makefile.inc"
.endif
.include "${.CURDIR}/regex/Makefile.inc"
.include "${.CURDIR}/rpc/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/string/Makefile.inc"
.include "${.CURDIR}/termios/Makefile.inc"
.include "${.CURDIR}/time/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/yp/Makefile.inc"

NLS=	C.msg Pig.msg de.msg es.msg fr.msg

LIBKERN=	${.CURDIR}/../../sys/lib/libkern

KSRCS=	bcmp.c bzero.c ffs.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c \
	strncpy.c htonl.c htons.c ntohl.c ntohs.c
.if (${MACHINE_ARCH} != "alpha")
KSRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
KINCLUDES+=	quad/quad.h
.endif

copy-to-libkern:	copy-to-libkern-machind copy-to-libkern-machdep

copy-to-libkern-machind: ${KSRCS}
	cp -p ${.ALLSRC} ${LIBKERN}
.if defined(KINCLUDES) && !empty(KINCLUDES)
	(cd ${.CURDIR} ; cp -p ${KINCLUDES} ${LIBKERN})
.endif

copy-to-libkern-machdep: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	cp -p ${.ALLSRC} ${LIBKERN}/arch/${MACHINE_ARCH}
.endif
.if defined(KMINCLUDES) && !empty(KMINCLUDES)
	(cd ${.CURDIR} ; cp -p ${KMINCLUDES} ${LIBKERN}/arch/${MACHINE_ARCH})
.endif

rm-from-libkern:
	for i in ${KSRCS}; do rm -f ${LIBKERN}/$$i; done
.if defined(KMSRCS) && !empty(KMSRCS)
	for i in ${KMSRCS}; do rm -f ${LIBKERN}/arch/${MACHINE_ARCH}/$$i; done
.endif

all: tags
tags: ${SRCS}
	ctags ${.ALLSRC:M*.c}
	egrep "^ENTRY(.*)|^FUNC(.*)|^SYSCALL(.*)" /dev/null ${.ALLSRC:M*.S} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> tags; sort -o tags tags

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 tags \
		${DESTDIR}/var/db/libc.tags

.include <bsd.lib.mk>
