#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.4 1999/05/24 16:54:03 millert Exp $

LIB=	kdb
CFLAGS+=-I${.CURDIR}
SRCS=	copykey.c krb_cache.c krb_dbm.c krb_kdb_utils.c krb_lib.c print_princ.c
CFLAGS+=	-DNDBM -DHAVE_NEW_DB

.include <bsd.lib.mk>
