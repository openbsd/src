### rx stuff
SRCS	+= rx_clock.c \
	   rx_event.c \
	   rx_user.c \
	   rx.c \
	   rx_globs.c \
	   rx_null.c \
	   rx_misc.c \
	   rx_pkt.c \
	   rx_rdwr.c \
	   rx_multi.c \
	   rx_trace.c

CFLAGS	+= -I${AFSSRC}/rx -I${AFSSRC}
.PATH: ${AFSSRC}/rx
