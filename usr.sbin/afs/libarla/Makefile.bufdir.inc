### bufdir stuff
SRCS	+= fdir.c fbuf.c
CFLAGS	+= -I${.CURDIR}/../src/lib/bufdir
.PATH: ${.CURDIR}/../src/lib/bufdir
