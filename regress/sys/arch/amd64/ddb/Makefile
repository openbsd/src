# $OpenBSD: Makefile,v 1.1 2024/11/19 05:50:41 anton Exp $

.if "${MACHINE_ARCH}" == "amd64"

SRCS+=		disasm.c
SRCS+=		stubs.c
SRCS+=		db_disasm.c
SRCS+=		db_output.c
SRCS+=		db_sym.c
PROG=		disasm

CPPFLAGS+=	-D_KERNEL
CPPFLAGS+=	-I${.CURDIR}/../../../../../sys
WARNINGS=	yes

.PATH: ${.CURDIR}/../../../../../sys/arch/amd64/amd64
.PATH: ${.CURDIR}/../../../../../sys/ddb

.else

regress:
        @echo SKIPPED

.endif

.include <bsd.regress.mk>
