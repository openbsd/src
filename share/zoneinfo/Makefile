#	$OpenBSD: Makefile,v 1.7 2005/09/29 00:43:03 deraadt Exp $
#	$NetBSD: Makefile,v 1.14 1995/04/22 12:10:17 cgd Exp $

# Change the line below for your time zone (after finding the zone you want in
# the time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -l rightzone

# This line has been moved to /usr/src/etc/Makefile
LOCALTIME=	US/Pacific

# If you want something other than Eastern United States time as a template
# for handling POSIX-style time zone environment variables,
# change the line below (after finding the zone you want in the
# time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -p rightzone

POSIXRULES=	US/Pacific

# Use an absolute path name for TZDIR unless you're just testing the software.

TZDIR=	${DESTDIR}/usr/share/zoneinfo

# If you always want time values interpreted as "seconds since the epoch
# (not counting leap seconds)", use
# 	REDO=		posix_only
# below.  If you always want right time values interpreted as "seconds since
# the epoch" (counting leap seconds)", use
#	REDO=		right_only
# below.  If you want both sets of data available, with leap seconds not
# counted normally, use
#	REDO=		posix_right
# below.  If you want both sets of data available, with leap seconds counted
# normally, use
#	REDO=		right_posix
# below.

REDO=		posix_right

# Since "." may not be in PATH...
YEARISTYPE=	${.CURDIR}/datfiles/yearistype.sh
YEARISTYPECOPY=	${.OBJDIR}/yearistypecopy

YDATA=		africa antarctica asia australasia \
		europe northamerica southamerica pacificnew etcetera factory \
		backward
NDATA=		systemv
SDATA=		solar87 solar88 solar89
TDATA=		$(YDATA) $(NDATA) $(SDATA)
TABDATA=	iso3166.tab zone.tab
DATA=		$(YDATA) $(NDATA) $(SDATA) $(TABDATA) leapseconds # yearistype.sh
USNO=		usno1988 usno1989 usno1989a usno1995 usno1997

ZIC=zic

all:	${YEARISTYPECOPY}

${YEARISTYPECOPY}:
	cp ${YEARISTYPE} yearistypecopy
	chmod u+x yearistypecopy

posix_only: ${TDATA} ${YEARISTYPECOPY}
	(cd ${.CURDIR}/datfiles; \
	    ${ZIC} -y ${YEARISTYPECOPY} -d ${TZDIR} -L /dev/null ${TDATA})

right_only: leapseconds ${TDATA} ${YEARISTYPECOPY}
	(cd ${.CURDIR}/datfiles; \
	    ${ZIC} -y ${YEARISTYPECOPY} -d ${TZDIR} -L leapseconds ${TDATA})

other_two: leapseconds ${TDATA} ${YEARISTYPECOPY}
	(cd ${.CURDIR}/datfiles; \
	    ${ZIC} -y ${YEARISTYPECOPY} -d ${TZDIR}/posix -L /dev/null ${TDATA})
	(cd ${.CURDIR}/datfiles; \
	    ${ZIC} -y ${YEARISTYPECOPY} -d ${TZDIR}/right -L leapseconds ${TDATA})

posix_right: posix_only other_two

right_posix: right_only other_two

realinstall: ${DATA} ${REDO} ${YEARISTYPECOPY}
	(cd ${.CURDIR}/datfiles; \
	    ${ZIC} -y ${YEARISTYPECOPY} -d ${TZDIR} -p ${POSIXRULES})
	chown -R ${BINOWN}:${BINGRP} ${TZDIR}
	find ${TZDIR} -type f -print0 | xargs -0r chmod a=r
	${INSTALL} -c -o root -g wheel -m 600 ${.CURDIR}/datfiles/iso3166.tab \
	    ${DESTDIR}/usr/share/misc


CLEANFILES+= yearistypecopy

.PATH:	${.CURDIR}/datfiles
.include <bsd.prog.mk>
