# $OpenBSD: Makefile,v 1.1 2000/02/25 16:35:26 hin Exp $

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
	parse_units.h		\
	resolve.c		\
	roken_gethostby.c	\
	signal.c		\
	simple_exec.c		\
	snprintf.c		\
	tm2time.c		\
	verify.c		\
	warnerr.c		\
	xdbm.h			\
	chown.c			\
	err.c			\
	err.h			\
	errx.c			\
	fchown.c		\
	fnmatch.c		\
	fnmatch.h		\
	getdtablesize.c		\
	getopt.c		\
	glob.h			\
	inet_aton.c		\
	initgroups.c		\
	iruserok.c		\
	lstat.c			\
	memmove.c		\
	readv.c			\
	recvmsg.c		\
	sendmsg.c		\
	setegid.c		\
	seteuid.c		\
	strerror.c		\
	strftime.c		\
	strlwr.c		\
	strndup.c		\
	strnlen.c		\
	strtok_r.c		\
	strupr.c		\
	verr.c			\
	verrx.c			\
	vwarn.c			\
	vwarnx.c		\
	warn.c			\
	warnx.c			\
	writev.c		\
	print_version.c		\
	socket.c

# These files have been omitted from kth-rkb:
# putenv.c, setenv.c, unsetenv.c, daemon.c, flock.c, getegid.c, getgid.c,
# gethostname.c, gettimeofday.c, hstrerror.c, innetgr.c, mkstemp.c,
# strcasecmp.c, strdup.c, strncasecmp.c, strsep.c, swab.c, vsyslog.c,
# getuid.c

.include <bsd.lib.mk>