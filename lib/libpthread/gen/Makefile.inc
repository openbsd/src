#	from: @(#)Makefile.inc	5.21 (Berkeley) 5/24/91
#	$Id: Makefile.inc,v 1.2 1998/07/21 16:28:06 peter Exp $

.PATH: ${.CURDIR}/gen

SRCS+= ttyname.c isatty.c directory.c popen.c time.c ctime.c difftime.c \
    syslog.c eprintf.c getpwent.c getpwnamuid.c pwd_internal.c getcwd.c \
    getwd.c
