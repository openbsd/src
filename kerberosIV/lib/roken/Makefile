# $OpenBSD: Makefile,v 1.2 2000/03/01 08:52:45 deraadt Exp $

LIB	= roken

NOPIC=
NOPROFILE=

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/lib/roken

CFLAGS	+= -I.

SRCS=	base64.c		\
	concat.c		\
	emalloc.c		\
	eread.c			\
	erealloc.c		\
	estrdup.c		\
	ewrite.c		\
	get_default_username.c	\
	get_window_size.c	\
	getarg.c		\
	inaddr2str.c		\
	issuid.c		\
	k_getpwnam.c		\
	k_getpwuid.c		\
	mini_inetd.c		\
	net_read.c		\
	net_write.c		\
	parse_time.c		\
	parse_units.c		\
	resolve.c		\
	roken_gethostby.c	\
	simple_exec.c		\
	tm2time.c		\
	verify.c		\
	strlwr.c		\
	strndup.c		\
	strnlen.c		\
	strtok_r.c		\
	strupr.c		\
	print_version.c		\
	socket.c		\
	warnerr.c

# These files have been omitted from kth-rkb:
# putenv.c, setenv.c, unsetenv.c, daemon.c, flock.c, getegid.c, getgid.c,
# gethostname.c, gettimeofday.c, hstrerror.c, innetgr.c, mkstemp.c,
# strcasecmp.c, strdup.c, strncasecmp.c, strsep.c, swab.c, vsyslog.c,
# getuid.c

.include <bsd.lib.mk>
