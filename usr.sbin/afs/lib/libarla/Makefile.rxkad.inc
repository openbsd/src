### rxkad stuff
SRCS	+= rxk_locl.c \
	   rxk_clnt.c \
	   rxk_crpt.c \
	   osi_alloc.c

CFLAGS	+= -I${AFSSRC}/rxkad

.PATH: ${AFSSRC}/rxkad
