#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/14 21:53:31 art Exp $
LIB	= roken
# roken stuff
SRCS	= base64.c getarg.c k_getpwuid.c k_getpwnam.c signal.o tm2time.c \
	  verify.c inaddr2str.c mini_inetd.c get_window_size.c warnerr.c \
	  snprintf.c resolve.c parse_units.c strnlen.c strtok_r.c strupr.c
# util stuff
SRCS	+= atom.c date_rfc822.c efile.c fnameutil.c freelist.c hash.c ip.c \
	   list.c log.c mem.c mmaptime.c prio.c strmatch.c strsplit.c \
	   strutil.c timeprio.c timeval.c
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