# $OpenBSD: Makefile,v 1.9 2002/06/09 19:01:59 hin Exp $

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
	getnameinfo_verified.c	\
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
	simple_exec.c		\
	socket.c		\
	warnerr.c		\
	write_pid.c

install:


.include <bsd.lib.mk>
