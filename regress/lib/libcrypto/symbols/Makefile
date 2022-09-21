#	$OpenBSD: Makefile,v 1.3 2022/09/21 15:24:45 tb Exp $

TESTS = \
	symbols

PROG = ${TESTS}

REGRESS_TARGETS= all_tests

.include <bsd.own.mk>					# for BSDSRCDIR

# libcrypto's Makefile stomps on globals, so only include it when needed.
.ifmake include_headers.c
. include "${BSDSRCDIR}/lib/libcrypto/Makefile"		# for HDRS

include_headers.c: ${BSDSRCDIR}/lib/libcrypto/Symbols.list
. for h in ${HDRS:T}
	@echo "#include <openssl/$h>" >> include_headers.c.tmp
. endfor
	mv include_headers.c.tmp include_headers.c
.endif

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}
LDFLAGS+=	-lcrypto
LDFLAGS+=	-Wl,--no-allow-shlib-undefined
CFLAGS+=	-Wno-deprecated-declarations

CLEANFILES+= include_headers.c symbols.c symbols.c.tmp

symbols.c: symbols.awk ../../../../lib/libcrypto/Symbols.list
	make -f ${.CURDIR}/Makefile include_headers.c
	awk -f ${.CURDIR}/symbols.awk \
		< ${BSDSRCDIR}/lib/libcrypto/Symbols.list > symbols.c.tmp
	mv symbols.c.tmp symbols.c

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test 2>/dev/null; \
	done

.include <bsd.regress.mk>
