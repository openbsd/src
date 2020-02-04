#	$OpenBSD: Makefile.inc,v 1.2 2020/02/04 18:42:51 florian Exp $

.PATH: ${.CURDIR}/lib/dns

SRCS+=	byaddr.c callbacks.c compress.c dns_log.c dns_result.c dns_time.c
SRCS+=	ds.c dst_api.c dst_parse.c dst_result.c masterdump.c
SRCS+=	hmac_link.c key.c keydata.c lib.c name.c openssl_link.c message.c
SRCS+=	ncache.c nsec.c nsec3.c openssldh_link.c openssldsa_link.c
SRCS+=	opensslecdsa_link.c opensslrsa_link.c rcode.c rdata.c rdatalist.c
SRCS+=	rdataset.c soa.c tsig.c ttl.c

