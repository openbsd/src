#	$OpenBSD: Makefile,v 1.5 2001/01/31 06:53:42 art Exp $
LIB	= roken
# roken stuff
SRCS	= copy_basename.c base64.c emalloc.c eefile.c efile.c estrdup.c \
	  getarg.c k_getpwuid.c k_getpwnam.c signal.c tm2time.c verify.c \
	  inaddr2str.c mini_inetd.c get_window_size.c warnerr.c snprintf.c \
	  parse_units.c strnlen.c strupr.c strlwr.c strmatch.c \
	  erealloc.c str2inaddr.c

# util stuff
SRCS	+= hash.c heap.c list.c log.c mmaptime.c

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
