#	from: @(#)Makefile.inc	5.21 (Berkeley) 5/24/91
#	$Id: GNUmakefile.inc,v 1.1.1.1 1998/07/21 13:19:02 peter Exp $

# gen sources
VPATH:= ${VPATH}:${srcdir}/gen

SRCS:= ttyname.c directory.c popen.c time.c ctime.c difftime.c syslog.c \
	eprintf.c getpwent.c getpwnamuid.c pwd_internal.c \
        getcwd.c getwd.c isatty.c $(SRCS)
