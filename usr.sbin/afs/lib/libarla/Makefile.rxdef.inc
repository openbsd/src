### rxdef stuff
includes:	fs.h vldb.h cb.h pts.h bos.h volumeserver.h ubik.h bos.h ka.h

## No, cb.ss.c is _not_ a mistake. The _client_ is the callback server.
SRCS	+= vldb.cs.c \
	   fs.cs.c \
	   cb.ss.c \
	   pts.cs.c \
	   volumeserver.cs.c \
	   ubik.cs.c \
	   bos.cs.c \
	   ka.cs.c

CLEANFILES 	+= vldb.ss.c vldb.cs.c vldb.ydr.c vldb.ss.h vldb.cs.h vldb.h \
		   fs.ss.c fs.cs.c fs.ydr.c fs.cs.h fs.ss.h fs.h \
		   cb.ss.c cb.cs.c cb.ydr.c cb.cs.h cb.ss.h cb.h \
		   pts.ss.c pts.cs.c pts.ydr.c pts.cs.h pts.ss.h pts.h \
		   volumeserver.ss.c volumeserver.cs.c volumeserver.ydr.c \
		   volumeserver.ss.h volumeserver.cs.h volumeserver.h \
		   ubik.ss.c ubik.cs.c ubik.ydr.c ubik.cs.h ubik.ss.h ubik.h \
		   bos.ss.c bos.cs.c bos.ydr.c bos.cs.h bos.ss.h bos.h \
		   vldb.td.c fs.td.c cb.td.c pts.td.c volumeserver.td.c \
		   ubik.td.c bos.td.c ka.cs.c ka.cs.h ka.h ka.ss.c ka.ss.h \
		   ka.td.c ka.ydr.c

CFLAGS		+= -I${AFSSRC}/rxdef -I${AFSSRC}/arlad

YDRFLAGS	= -I${.CURDIR}
.PATH: ${AFSSRC}/rxdef

vldb.cs.c vldb.cs.h vldb.ss.c vldb.ss.h vldb.ydr.c vldb.h: vldb.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/vldb.xg

fs.cs.c fs.cs.h fs.ss.c fs.ss.h fs.ydr.c fs.h: fs.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/fs.xg

cb.cs.c cb.cs.h cb.ss.c cb.ss.h cb.ydr.c cb.h: cb.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/cb.xg

pts.cs.c pts.cs.h pts.ss.c pts.ss.h pts.ydr.c pts.h: pts.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/pts.xg

volumeserver.cs.c volumeserver.cs.h volumeserver.ss.c volumeserver.ss.h volumeserver.ydr.c volumeserver.h: volumeserver.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/volumeserver.xg

ubik.cs.c ubik.cs.h ubik.ss.c ubik.ss.h ubik.ydr.c ubik.h: ubik.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/ubik.xg

bos.cs.c bos.cs.h bos.ss.c bos.ss.h bos.ydr.c bos.h: bos.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/bos.xg

ka.cs.c ka.cs.h ka.ss.c ka.ss.h ka.ydr.c ka.h: ka.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/ka.xg

