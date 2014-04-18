# $OpenBSD: Makefile,v 1.9 2014/04/18 20:01:31 beck Exp $

LIB=	roken
INCS=	roken.h \
	roken-common.h \
	base64.h \
	getarg.h \
	hex.h \
	parse_bytes.h \
	parse_time.h \
	parse_units.h \
	resolve.h \
	rtbl.h \
	xdbm.h

SRCS=	base64.c \
	copyhostent.c \
	ecalloc.c \
	emalloc.c \
	erealloc.c \
	estrdup.c \
	strlwr.c \
	strsep_copy.c \
	strupr.c \
	bswap.c \
	cloexec.c \
	concat.c \
	ct.c \
	doxygen.c \
	dumpdata.c \
	environment.c \
	eread.c \
	esetenv.c \
	ewrite.c \
	freehostent.c \
	get_default_username.c \
	get_window_size.c \
	getaddrinfo_hostspec.c \
	getarg.c \
	getnameinfo_verified.c \
	h_errno.c \
	hex.c \
	hostent_find_fqdn.c \
	issuid.c \
	k_getpwnam.c \
	k_getpwuid.c \
	mini_inetd.c \
	net_read.c \
	net_write.c \
	parse_bytes.c \
	parse_time.c \
	parse_units.c \
	qsort.c \
	rand.c \
	realloc.c \
	resolve.c \
	roken.h \
	roken_gethostby.c \
	rtbl.c \
	setprogname.c \
	simple_exec.c \
	snprintf.c \
	socket.c \
	strcollect.c \
	strerror_r.c \
	strpool.c \
	timeval.c \
	tm2time.c \
	unvis.c \
	verify.c \
	vis.c \
	warnerr.c \
	write_pid.c \
	xfree.c

MPAGES=		getarg.3 parse_time.3 rtbl.3 ecalloc.3
MSRC=		${KRB5DIR}/lib/roken

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I.
CPPFLAGS+=	-DBUILD_ROKEN_LIB

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/roken ${KRB5DIR}/include ${.CURDIR}/../../include
