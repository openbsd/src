# $OpenBSD: Makefile.inc,v 1.2 2023/11/05 11:22:59 espie Exp $

.PATH:	${.CURDIR}/lst.lib
CFLAGS += -I${.CURDIR}/lst.lib
HOSTCFLAGS += -I${.CURDIR}/lst.lib

SRCS+=	lstAddNew.c lstAppend.c lstConcat.c lstConcatDestroy.c \
	lstDeQueue.c lstDestroy.c lstDupl.c lstFindFrom.c lstForEachFrom.c \
	lstInsert.c lstMember.c lstRemove.c lstSucc.c
