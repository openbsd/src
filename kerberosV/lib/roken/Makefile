# $OpenBSD: Makefile,v 1.8 2002/05/18 16:43:36 deraadt Exp $

LIB	= roken

NOPROFILE=

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

.PATH:	${KRB5SRC}/lib/roken
test:
	echo ${KRB4SRC} 
CFLAGS	+= -I.

SRCS=	base64.c		\
	concat.c		\
	emalloc.c		\
	eread.c			\
	erealloc.c		\
	esetenv.c		\
	estrdup.c		\
	ewrite.c		\
	getaddrinfo_hostspec.c	\
	get_default_username.c	\
	get_window_size.c	\
	getarg.c		\
	issuid.c		\
	k_getpwnam.c		\
	k_getpwuid.c		\
	mini_inetd.c		\
	net_read.c		\
	net_write.c		\
	parse_bytes.c		\
	parse_time.c		\
	parse_units.c		\
	resolve.c		\
	roken_gethostby.c	\
	rtbl.c			\
	tm2time.c		\
	verify.c		\
	strcollect.c		\
	strlwr.c		\
	strndup.c		\
	strnlen.c		\
	strsep_copy.c		\
	strupr.c		\
	print_version.c		\
	socket.c		\
	warnerr.c		\
	write_pid.c

install:


.include <bsd.lib.mk>
