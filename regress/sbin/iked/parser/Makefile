#	$OpenBSD: Makefile,v 1.1 2017/05/29 20:59:28 markus Exp $

WARNINGS=Yes

.include <bsd.own.mk>
.include <bsd.obj.mk>

.PATH:          ${.CURDIR}/../../../../sbin/iked \
		${.CURDIR}/../../../../sbin/iked/${__objdir}

PROG=		test_parser
SRCS=		tests.c common.c test_parser_fuzz.c ikev2_pld.c \
		imsg_util.c log.c util.c
SRCS+=		ikev2_map.c eap_map.c

LDADD=		-lutil -lcrypto
DPADD+=		${LIBUTIL} ${LIBCRYPTO}
CFLAGS+=	-I${.CURDIR}/../../../../sbin/iked
CFLAGS+=	-I${.CURDIR}/../../../../sbin/iked/${__objdir}
CFLAGS+=	-Wno-missing-field-initializers

CFLAGS+=	-I${.CURDIR}/../test_helper

.if exists(${.CURDIR}/../test_helper/${__objdir})
LDADD+=-L${.CURDIR}/../test_helper/${__objdir} -ltest_helper
DPADD+=${.CURDIR}/../test_helper/${__objdir}/libtest_helper.a
.else
LDADD+=-L${.CURDIR}/../test_helper -ltest_helper
DPADD+=${.CURDIR}/../test_helper/libtest_helper.a
.endif

REGRESS_TARGETS=stamp-regress-${PROG}

stamp-regress-${PROG}: ${PROG}
	./${PROG} && touch stamp-regress-${PROG}

.include <bsd.regress.mk>
