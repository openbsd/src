### rxdef stuff
## No, cb.ss.c is _not_ a mistake. The _client_ is the callback server.
SRCS		+= vldb.cs.c fs.cs.c cb.ss.c pts.cs.c \
		   volumeserver.cs.c ubik.cs.c

CLEANFILES 	+= vldb.ss.c vldb.cs.c vldb.ydr.c vldb.ss.h vldb.cs.h vldb.h \
		   fs.ss.c fs.cs.c fs.ydr.c fs.cs.h fs.ss.h fs.h \
		   cb.ss.c cb.cs.c cb.ydr.c cb.cs.h cb.ss.h cb.h \
		   pts.ss.c pts.cs.c pts.ydr.c pts.cs.h pts.ss.h pts.h \
		   volumeserver.ss.c volumeserver.cs.c volumeserver.ydr.c \
		   volumeserver.ss.h volumeserver.cs.h volumeserver.h \
		   ubik.ss.c ubik.cs.c ubik.ydr.c ubik.cs.h ubik.ss.h ubik.h

CFLAGS		+= -I${.CURDIR}/../src/rxdef

YDR		= ../ydr/ydr
YDRFLAGS	= -I${.CURDIR}
.PATH: ${.CURDIR}/../src/rxdef

vldb.cs.c vldb.cs.h vldb.ss.c vldb.ss.h vldb.ydr.c vldb.h: vldb.xg
		$(YDR) $(YDRFLAGS) ${.CURDIR}/../src/rxdef/vldb.xg

fs.cs.c fs.cs.h fs.ss.c fs.ss.h fs.ydr.c fs.h: fs.xg
		$(YDR) $(YDRFLAGS) ${.CURDIR}/../src/rxdef/fs.xg

cb.cs.c cb.cs.h cb.ss.c cb.ss.h cb.ydr.c cb.h: cb.xg
		$(YDR) $(YDRFLAGS) ${.CURDIR}/../src/rxdef/cb.xg

pts.cs.c pts.cs.h pts.ss.c pts.ss.h pts.ydr.c pts.h: pts.xg
		$(YDR) $(YDRFLAGS) ${.CURDIR}/../src/rxdef/pts.xg

volumeserver.cs.c volumeserver.cs.h volumeserver.ss.c volumeserver.ss.h volumeserver.ydr.c volumeserver.h: volumeserver.xg
		$(YDR) $(YDRFLAGS) ${.CURDIR}/../src/rxdef/volumeserver.xg

ubik.cs.c ubik.cs.h ubik.ss.c ubik.ss.h ubik.ydr.c ubik.h: ubik.xg
		$(YDR) $(YDRFLAGS) ${.CURDIR}/../src/rxdef/ubik.xg

