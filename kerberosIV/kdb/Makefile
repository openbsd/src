#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.3 1997/12/02 20:56:48 deraadt Exp $

LIB=	kdb
CFLAGS+=-I${.CURDIR}
SRCS=	copykey.c krb_cache.c krb_dbm.c krb_kdb_utils.c krb_lib.c print_princ.c
COPTS+=	-DNDBM -DHAVE_NEW_DB

.include <bsd.lib.mk>
