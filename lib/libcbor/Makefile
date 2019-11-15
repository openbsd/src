# $OpenBSD: Makefile,v 1.2 2019/11/15 03:19:39 deraadt Exp $

.PATH: ${.CURDIR}/src ${.CURDIR}/src/cbor ${.CURDIR}/src/cbor/internal

CFLAGS+= -I${.CURDIR}/src -DHAVE_ENDIAN_H -std=c99

LIB=	cbor
SRCS=	cbor.c
#SRCS+=	allocators.c

WARNINGS=yes
CDIAGFLAGS+=	-Wall -Wextra -Wno-unused-parameter
#CDIAGFLAGS+=	-Werror

# cbor/
SRCS+= arrays.c bytestrings.c callbacks.c common.c encoding.c floats_ctrls.c
SRCS+= ints.c maps.c serialization.c streaming.c tags.c strings.c 

# cbor/internal
SRCS+= builder_callbacks.c encoders.c loaders.c memory_utils.c
SRCS+= stack.c unicode.c

HDRS=	cbor.h
HDRS+=	cbor/arrays.h cbor/bytestrings.h cbor/callbacks.h cbor/common.h
HDRS+=	cbor/configuration.h cbor/data.h cbor/encoding.h cbor/floats_ctrls.h
HDRS+=	cbor/ints.h cbor/maps.h cbor/serialization.h cbor/streaming.h
HDRS+=	cbor/strings.h cbor/tags.h

NOMAN=

includes:
	@for i in $(HDRS); do \
		j="test -d ${DESTDIR}/usr/include/`dirname $$i` || \
		    ${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		    ${DESTDIR}/usr/include/`dirname $$i`"; \
		echo $$j; \
		eval "$$j"; \
		j="cmp -s src/$$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		    -m 444 ${.CURDIR}/src/$$i ${DESTDIR}/usr/include/$$i"; \
		echo $$j; \
		eval "$$j"; \
	done

.include <bsd.lib.mk>
