PROG	= vos
MAN	= vos.8
BINDIR	= /usr/sbin
SRCS	= vos.c arlalib.c fs_lib.c vos_common.c vos_createentry.c \
	  vos_createvolume.c vos_dump.c vos_endtrans.c vos_examine.c \
	  vos_listpart.c vos_listvldb.c vos_listvol.c vos_partinfo.c \
	  vos_status.c vos_syncsite.c

LDADD	+= -lkafs
DPADD	+= ${LIBKAFS}

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/appl

.include <bsd.prog.mk>
