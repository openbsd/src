#	$OpenBSD: Makefile.inc,v 1.1 2020/02/07 09:58:52 florian Exp $

.PATH: ${.CURDIR}/lib/dns

SRCS+=	byaddr.c callbacks.c compress.c dns_log.c dns_result.c dns_time.c
SRCS+=	dst_api.c dst_parse.c dst_result.c masterdump.c
SRCS+=	hmac_link.c key.c lib.c name.c openssl_link.c message.c
SRCS+=	rcode.c rdata.c rdatalist.c
SRCS+=	rdataset.c tsig.c ttl.c

