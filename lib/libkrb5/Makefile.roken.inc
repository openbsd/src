# $OpenBSD: Makefile.roken.inc,v 1.2 2004/06/04 20:27:59 biorn Exp $

.PATH:	${KRB5SRC}/lib/roken

SRCS+=	strlwr.c \
	strupr.c \
	roken_net_write.c \
	roken_net_read.c \
	getarg.c \
	base64.c \
	issuid.c \
	parse_time.c \
	parse_units.c \
	strsep_copy.c \
	get_window_size.c \
	roken_gethostby.c \
	resolve.c \
	get_default_username.c \
	k_getpwnam.c \
	k_getpwuid.c

# There is a net_write.c and net_read.c both in lib/roken and lib/krb5.
roken_net_write.c:      ${KRB5SRC}/lib/roken/net_write.c
	ln -sf ${KRB5SRC}/lib/roken/net_write.c roken_net_write.c
roken_net_read.c:       ${KRB5SRC}/lib/roken/net_read.c
	ln -sf ${KRB5SRC}/lib/roken/net_read.c roken_net_read.c

CLEANFILES+=roken_net_{read,write}.c
