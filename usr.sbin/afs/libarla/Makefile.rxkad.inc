### rxkad stuff
SRCS	+= rxk_locl.c rxk_clnt.c rxk_serv.c rxk_crpt.c rxk_info.c \
	   osi_alloc.c compat.c

CFLAGS	+= -I${.CURDIR}/../src/rxkad

.PATH: ${.CURDIR}/../src/rxkad
