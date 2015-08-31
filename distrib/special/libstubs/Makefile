#	$OpenBSD: Makefile,v 1.15 2015/08/31 04:08:31 deraadt Exp $

.include <bsd.own.mk>
LIB=    stubs
SRCS=	db.c setlocale.c sha1.c sha2.c getgrent.c getpwent.c \
	ethers.c \
	mbrtowc_sb.c sscanf.c vfprintf.c vfscanf.c
SRCS+=	asr.c			\
	asr_utils.c		\
	getaddrinfo.c		\
	getaddrinfo_async.c	\
	gethostnamadr.c		\
	gethostnamadr_async.c	\
	getnameinfo.c		\
	getnameinfo_async.c	\
	getnetnamadr.c		\
	getnetnamadr_async.c	\
	getrrsetbyname.c	\
	getrrsetbyname_async.c	\
	res_debug.c		\
	res_init.c		\
	res_mkquery.c		\
	res_query.c		\
	res_search_async.c	\
	res_send.c		\
	res_send_async.c	\
	sethostent.c

NOPIC=	Yes
NOPROFILE=Yes

CFLAGS+=-D__DBINTERFACE_PRIVATE -DSHA2_SMALL -UYP -DNO_LOG_BAD_DNS_RESPONSES \
	-DFLOATING_POINT -DASRNODEBUG \
	-I${LIBCSRCDIR}/include -I${LIBCSRCDIR}/hidden \
	-I${LIBCSRCDIR}/asr -I${LIBCSRCDIR}/locale -I${LIBCSRCDIR}/gdtoa \
	-I${LIBCSRCDIR}/arch/${MACHINE_CPU}/gdtoa -I${LIBCSRCDIR}/stdio  
CFLAGS+=

LIBCSRCDIR=${.CURDIR}/../../../lib/libc

.PATH: ${.CURDIR}/../../../lib/libc/hash
.PATH: ${.CURDIR}/../../../lib/libc/gen
.PATH: ${.CURDIR}/../../../lib/libc/asr
.PATH: ${.CURDIR}/../../../lib/libc/net
.PATH: ${.CURDIR}/../../../lib/libc/stdio

.include <bsd.lib.mk>
