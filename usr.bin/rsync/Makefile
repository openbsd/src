SRCS= blocks.c child.c client.c downloader.c fargs.c flist.c hash.c \
      io.c log.c md4.c mkpath.c receiver.c sender.c server.c session.c \
      socket.c symlinks.c uploader.c main.c

PROG=	rsync
xMAN=	rsync.1

LDADD+= -lm
DPADD+= ${LIBM}

CFLAGS+=-g -W -Wall -Wextra -Wno-unused-parameter

.include <bsd.prog.mk>
