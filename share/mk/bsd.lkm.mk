#	$OpenBSD: bsd.lkm.mk,v 1.14 1998/12/31 23:49:45 millert Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.include <bsd.own.mk>

.SUFFIXES: .out .o .c .cc .C .y .l .s .8 .7 .6 .5 .4 .3 .2 .1 .0

# XXX In order to at least diminish the brokenness of trusting /sys to point
# to the tree we're actually belonging to we check BSDSRCDIR.  On multi-tree
# machines /sys isn't always a link to the correct tree.
.if defined(BSDSRCDIR)
CFLAGS+=	${COPTS} -D_KERNEL -I${BSDSRCDIR}/sys -I${BSDSRCDIR}/sys/arch
.else
CFLAGS+=	${COPTS} -D_KERNEL -I/sys -I/sys/arch
.endif

LDFLAGS+= -r
.if defined(LKM)
SRCS?=	${LKM}.c
.if !empty(SRCS:N*.h:N*.sh)
OBJS+=	${SRCS:N*.h:N*.sh:R:S/$/.o/g}
LOBJS+=	${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}
.endif
COMBINED?=combined.o
.if !defined(POSTINSTALL)
POSTINSTALL= ${LKM}install
.endif

.if defined(OBJS) && !empty(OBJS)

${COMBINED}: ${OBJS} ${DPADD}
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

.endif	# defined(OBJS) && !empty(OBJS)

.if	!defined(MAN)
MAN=	${LKM}.1
.endif	# !defined(MAN)
.endif	# defined(LKM)

.MAIN: all
all: ${COMBINED} _SUBDIRUSE

.if !target(clean)
clean: _SUBDIRUSE
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${LKM} ${COMBINED} ${OBJS} ${LOBJS} ${CLEANFILES}
.endif

cleandir: _SUBDIRUSE clean

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

.if !target(realinstall)
realinstall:
.if defined(LKM)
	${INSTALL} ${INSTALL_COPY} -o ${LKMOWN} -g ${LKMGRP} -m ${LKMMODE} \
	    ${COMBINED} ${DESTDIR}${LKMDIR}/${LKM}.o
.if exists(${.CURDIR}/${POSTINSTALL})
	${INSTALL} ${INSTALL_COPY} -o ${LKMOWN} -g ${LKMGRP} -m 555 \
	    ${.CURDIR}/${POSTINSTALL} ${DESTDIR}${LKMDIR}
.endif
.endif
.endif


load:	${COMBINED}
	if [ -x ${.CURDIR}/${POSTINSTALL} ]; then \
		modload -d -o $(LKM) -e$(LKM) -p${.CURDIR}/${POSTINSTALL} $(COMBINED); \
	else \
		modload -d -o $(LKM) -e$(LKM) $(COMBINED); \
	fi

unload:
	modunload -n $(LKM)

install: maninstall _SUBDIRUSE
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}${LKMDIR}/$$1; \
		shift; \
		t=${DESTDIR}${LKMDIR}/$$1; \
		shift; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done; true
.endif

maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall
.endif

.if !target(lint)
lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	@${LINT} ${LINTFLAGS} ${LDFLAGS:M-L*} ${LOBJS} ${LDADD}
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

.if !defined(NONLS)
.include <bsd.nls.mk>
.endif

.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
