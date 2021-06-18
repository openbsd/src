# $OpenBSD: Makefile,v 1.2 2021/06/18 14:09:31 mortimer Exp $

.include <bsd.own.mk>

.if ${COMPILER_VERSION} == "clang"

# Only amd64 has unwind info by default in llvm11. llvm12 adds more arches.
CLANG_VERSION!=clang --version | head -1 | grep -E -o 'version [[:digit:]]+' | cut -d' ' -f 2
CLANG_GE_12!=if [ ${CLANG_VERSION} -ge 12 ] ; then echo 1 ; fi
.if ${MACHINE} == "amd64" || ${CLANG_GE_12} == "1"

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

.else # Clang arch but no unwind info
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
	# No unwind info on this arch
	@echo SKIPPED
.endif

.else # Not clang arch
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
	# backtrace() is a noop on non-unwind arches.
	@echo SKIPPED
.endif

.include <bsd.regress.mk>
