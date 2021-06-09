# $OpenBSD: Makefile,v 1.1 2021/06/09 19:39:15 mortimer Exp $

.include <bsd.own.mk>

.if ${COMPILER_VERSION} == "clang"
PROG=	t_backtrace

.PATH: ${SRCDIR} ${BSDSRCDIR}/regress/lib/libc/sys/
SRCS+= t_backtrace.c atf-c.c

.if empty(CFLAGS:M-std=*)
CFLAGS+=  -std=gnu99
.endif

CFLAGS+=	-I${BSDSRCDIR}/regress/lib/libc/sys/
LDFLAGS+=	-Wl,--export-dynamic
LDADD+=	-lexecinfo
DPADD+=	${LIBEXECINFO}

# Only needed while this lib is not installed
CFLAGS+= -I${BSDSRCDIR}/lib/libexecinfo/
LDFLAGS+= -L${BSDOBJDIR}/lib/libexecinfo/

run-regress-${PROG}: ${PROG}
	./${PROG} -r 1

.else # Not clang arch
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
	# backtrace() is a noop on non-unwind arches.
	@echo SKIPPED
.endif

.include <bsd.regress.mk>
