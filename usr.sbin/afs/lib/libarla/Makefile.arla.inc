SRCS	+= arlalib.c \
	   fs_lib.c \
	   ka-procs.c \
	   tokens.c \
	   print_version.c

CFLAGS	+= -I${AFSSRC}/appl/lib
.PATH:	${AFSSRC}/appl/lib
