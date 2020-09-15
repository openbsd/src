#	$OpenBSD: Makefile.inc,v 1.4 2020/09/15 08:15:17 florian Exp $

.PATH: ${.CURDIR}/lib/dns

SRCS+=	compress.c dns_log.c dns_result.c dns_time.c
SRCS+=	dst_api.c dst_result.c masterdump.c
SRCS+=	hmac_link.c key.c name.c openssl_link.c message.c
SRCS+=	rcode.c rdata.c rdatalist.c
SRCS+=	rdataset.c tsig.c ttl.c

