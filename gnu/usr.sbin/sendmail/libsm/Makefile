#	$OpenBSD: Makefile,v 1.5 2004/12/16 00:21:30 millert Exp $

LIB=	sm
SRCS=	assert.c debug.c errstring.c exc.c heap.c match.c rpool.c strdup.c \
	strl.c clrerr.c fclose.c feof.c ferror.c fflush.c fget.c \
	fpos.c findfp.c flags.c fopen.c fprintf.c fpurge.c fput.c fread.c \
	fscanf.c fseek.c fvwrite.c fwalk.c fwrite.c get.c makebuf.c put.c \
	refill.c rewind.c setvbuf.c smstdio.c snprintf.c sscanf.c stdio.c \
	strio.c ungetc.c vasprintf.c vfprintf.c vfscanf.c vprintf.c \
	vsnprintf.c wbuf.c wsetup.c string.c stringf.c \
	xtrap.c strto.c test.c path.c strcasecmp.c strrevcmp.c signal.c \
	clock.c config.c shm.c mbdb.c strexit.c cf.c ldap.c niprop.c
ENVDEF=	-DNOT_SENDMAIL

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
