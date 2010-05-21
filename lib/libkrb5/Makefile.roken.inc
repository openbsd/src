# $OpenBSD: Makefile.roken.inc,v 1.5 2010/05/21 17:21:54 millert Exp $

.PATH:	${KRB5SRC}/lib/roken

SRCS+=	base64.c \
	get_default_username.c \
	get_window_size.c \
	getarg.c \
	hex.c \
	issuid.c \
	k_getpwnam.c \
	k_getpwuid.c \
	parse_time.c \
	parse_units.c \
	resolve.c \
	roken_gethostby.c \
	roken_net_read.c \
	roken_net_write.c \
	rtbl.c \
	simple_exec.c \
	strcollect.c \
	strlwr.c \
	strsep_copy.c \
	strupr.c

MAN+=	getarg.3
MLINKS+=getarg.3 arg_printusage.3

MAN+=	parse_time.3
MLINKS+=parse_time.3 print_time_table.3
MLINKS+=parse_time.3 unparse_time.3
MLINKS+=parse_time.3 unparse_time_approx.3

MAN+=	rtbl.3
MLINKS+=rtbl.3 rtbl_create.3
MLINKS+=rtbl.3 rtbl_destroy.3
MLINKS+=rtbl.3 rtbl_set_flags.3
MLINKS+=rtbl.3 rtbl_get_flags.3
MLINKS+=rtbl.3 rtbl_set_prefix.3
MLINKS+=rtbl.3 rtbl_set_separator.3
MLINKS+=rtbl.3 rtbl_set_column_prefix.3
MLINKS+=rtbl.3 rtbl_set_column_affix_by_id.3
MLINKS+=rtbl.3 rtbl_add_column.3
MLINKS+=rtbl.3 rtbl_add_column_by_id.3
MLINKS+=rtbl.3 rtbl_add_column_entry.3
MLINKS+=rtbl.3 rtbl_add_column_entry_by_id.3
MLINKS+=rtbl.3 rtbl_new_row.3
MLINKS+=rtbl.3 rtbl_format.3

# There is a net_write.c and net_read.c both in lib/roken and lib/krb5.
roken_net_write.c:      ${KRB5SRC}/lib/roken/net_write.c
	ln -sf ${KRB5SRC}/lib/roken/net_write.c roken_net_write.c
roken_net_read.c:       ${KRB5SRC}/lib/roken/net_read.c
	ln -sf ${KRB5SRC}/lib/roken/net_read.c roken_net_read.c

CLEANFILES+=roken_net_{read,write}.c
