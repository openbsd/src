# $OpenBSD: Makefile,v 1.6 2026/10/23 20:05:33 bluhm Exp $

SRCS=		runtests.c
SRCS+=		acc_tests.c alloc_tests.c
SRCS+=		basic_tests.c
SRCS+=		chardata.c common.c
SRCS+=		dummy.c
SRCS+=		handlers.c hash_tests.c
SRCS+=		memcheck.c minicheck.c misc_tests.c
SRCS+=		ns_tests.c nsalloc_tests.c
SRCS+=		props_tests.c
SRCS+=		structdata.c
SRCS+=		xcs_tests.c

# _INTERNAL_trim_to_complete_utf8_characters is not exported in dynamic library
LDFLAGS=	-static

.include <bsd.regress.mk>
