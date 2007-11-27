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

CFLAGS		+= -I${AFSSRC}/rxdef -I${AFSSRC}/arlad

YDRFLAGS	= -I${.CURDIR}
.PATH: ${AFSSRC}/rxdef

.for f in vldb fs cb pts volumeserver ubik bos ka

stamp_$f: $f.xg
		$(YDR) $(YDRFLAGS) ${AFSSRC}/rxdef/$f.xg
		touch $@

$f.cs.c $f.cs.h $f.ss.c $f.ss.h $f.ydr.c $f.h: stamp_$f

CLEANFILES+=$f.cs.c $f.cs.h $f.ss.c $f.ss.h $f.ydr.c $f.h stamp_$f
.endfor
