PROG	= vos
MAN	= vos.8
BINDIR	= /usr/sbin
SRCS	= vos.c vos_common.c vos_createentry.c vos_createvolume.c \
	  vos_dump.c vos_endtrans.c vos_examine.c vos_listpart.c \
	  vos_listvldb.c vos_listvol.c vos_lock.c vos_partinfo.c \
	  vos_status.c vos_syncsite.c vos_unlock.c vos_vldbexamine.c

LDADD	+= -lkafs
DPADD	+= ${LIBKAFS}

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/appl/vos

SRCS	+= arlalib.c fs_lib.c
CFLAGS	+= -I${.CURDIR}/../src/appl/lib
.PATH: ${.CURDIR}/../src/appl/lib
.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>
