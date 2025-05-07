SYSTEM_LIBDIR=/usr/lib
SYSTEM_INCLUDEDIR=/usr/include

SYSPATH=/usr/lib/pkgconfig
PKGPATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig
X11PATH=/usr/X11R6/lib/pkgconfig:/usr/X11R6/share/pkgconfig
PKG_DEFAULT_PATH=${SYSPATH}:${PKGPATH}:${X11PATH}

.PATH.c: ${.CURDIR}/libpkgconf 
.PATH.c: ${.CURDIR}/cli

PROG = pkg-config
SRCS = argvsplit.c	\
	audit.c		\
	bsdstubs.c	\
	buffer.c	\
	cache.c		\
	client.c	\
	dependency.c	\
	fileio.c	\
	fragment.c	\
	parser.c	\
	path.c		\
	personality.c	\
	pkg.c		\
	queue.c		\
	tuple.c		\
	getopt_long.c	\
	main.c

CPPFLAGS = -DPKGCONF_LITE 
CPPFLAGS += -I${.CURDIR} -I${.CURDIR}/libpkgconf -I${.CURDIR}/cli 
CPPFLAGS += -DSYSTEM_LIBDIR=\"${SYSTEM_LIBDIR}\" 
CPPFLAGS += -DSYSTEM_INCLUDEDIR=\"${SYSTEM_INCLUDEDIR}\" 
CPPFLAGS += -DPKG_DEFAULT_PATH=\"${PKG_DEFAULT_PATH}\"

pkg-config.1:	man/pkgconf.1
	cp ${.CURDIR}/man/pkgconf.1 $@

CLEANFILES = pkg-config.1

.include <bsd.prog.mk>
