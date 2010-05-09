# $FreeBSD: src/gnu/lib/libobjc/Makefile,v 1.34.2.1 2009/08/03 08:13:06 kensmith Exp $
# $OpenBSD: Makefile,v 1.1 2010/05/09 13:26:56 robert Exp $

OBJCDIR=${.CURDIR}/../../../gcc/libobjc
GCCDIR=	${.CURDIR}/../../../gcc/gcc
GCCLIB=	${.CURDIR}/../../../gcc

.PATH: ${OBJCDIR}/objc ${OBJCDIR}

LIB=	objc

SRCS=	archive.c class.c encoding.c gc.c hash.c init.c linking.m misc.c \
	nil_method.c NXConstStr.m Object.m objects.c Protocol.m sarray.c \
	selector.c sendmsg.c thr.c thr-objc.c exception.c

INCS=	encoding.h hash.h objc-api.h objc-decls.h objc-list.h objc.h runtime.h \
	sarray.h thr.h typedstream.h NXConstStr.h Object.h Protocol.h

CFLAGS+= -DHAVE_GTHR_DEFAULT -DIN_GCC -DIN_TARGET_LIBS
CFLAGS+= -I. -I${.CURDIR}/../cc_tools
CFLAGS+= -I${OBJCDIR}/objc -I${OBJCDIR}
CFLAGS+= -I${GCCDIR}/config -I${GCCDIR}
CFLAGS+= -I${GCCLIB}/include
CFLAGS+= -fexceptions -frandom-seed=RepeatabilityConsideredGood
OBJCFLAGS= -fgnu-runtime ${CFLAGS}

GENHDRS= runtime-info.h

runtime-info.h:
	`${CC} --print-prog-name=cc1obj` -print-objc-runtime-info \
	    < /dev/null > ${.TARGET}

.for H in tconfig.h tm.h config.h options.h gthr-default.h unwind.h
$H: ${.CURDIR}/../cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}
GENHDRS+=	$H
.endfor

CLEANFILES+=	${GENHDRS} cs-* optionlist
SRCS+=		${GENHDRS}

.if !empty(INCS)
includes: ${INCS}
	${INSTALL} -C -o ${BINOWN} -g ${LOCALEGRP} -m ${NONBINMODE} ${.ALLSRC} \
		${DESTDIR}/usr/include/objc
.endif

${OBJS}: ${GENHDRS}

.include <bsd.lib.mk>

.SUFFIXES: .m

# ObjC
OBJC?=		cc
OBJCFLAGS?=	${CFLAGS} ${CPPFLAGS} -Wno-import
COMPILE.m?=	${OBJC} ${OBJCFLAGS} -c
LINK.m?=	${OBJC} ${OBJCFLAGS} ${LDFLAGS}

.m:
	${LINK.m} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.m.o:
	${COMPILE.m} ${.IMPSRC}
.m.a:
	${COMPILE.m} ${.IMPSRC}
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o
.m.po:
	${COMPILE.m} -p ${.IMPSRC} -o ${.TARGET}.o
	${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	rm -f $*.o
.m.so:
	${COMPILE.m} ${PICFLAG} -DPIC ${.IMPSRC} -o ${.TARGET}.o
	${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	rm -f $*.o
