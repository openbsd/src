### rxkad stuff
SRCS	+= rxk_locl.c \
	   rxk_clnt.c \
	   rxk_crpt.c \
	   osi_alloc.c

CFLAGS	+= -I${AFSSRC}/rxkad -I${DESTDIR}/usr/include/kerberosIV

.PATH: ${AFSSRC}/rxkad
