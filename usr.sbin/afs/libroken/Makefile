#	$OpenBSD: Makefile,v 1.3 1999/12/01 19:54:59 deraadt Exp $
LIB	= roken
# roken stuff
SRCS	= base64.c emalloc.c estrdup.c getarg.c k_getpwuid.c k_getpwnam.c \
	  signal.o tm2time.c verify.c inaddr2str.c mini_inetd.c \
	  get_window_size.c warnerr.c snprintf.c resolve.c parse_units.c \
	  strnlen.c strupr.c strlwr.c
# util stuff
SRCS	+= atom.c date_rfc822.c efile.c eefile.c fnameutil.c freelist.c \
	   hash.c heap.c ip.c list.c log.c mmaptime.c strmatch.c \
	   strsplit.c strutil.c timeval.c
# sl stuff
SRCS	+= sl.c
CFLAGS	+= -DHAVE_CONFIG_H -I${.CURDIR}/../src/lib/roken \
	   -I${.CURDIR}/../src/util -I${.CURDIR}/../src/include \
	   -I${.CURDIR}/../src/lib/sl

NOPROFILE=
NOPIC=

.PATH: ${.CURDIR}/../src/lib/roken
.PATH: ${.CURDIR}/../src/util
.PATH: ${.CURDIR}/../src/lib/sl

.include <bsd.lib.mk>