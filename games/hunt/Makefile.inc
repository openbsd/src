#	$NetBSD: Makefile.inc,v 1.3 1997/10/22 05:05:21 lukem Exp $
#
#  Hunt
#  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
#  San Francisco, California

#
# Game parameter flags are:
#	RANDOM	Include doors which disperse shots randomly
#	REFLECT	Include diagonal walls that reflect shots
#	MONITOR	Include code for watching the game from the sidelines
#	OOZE	Include slime shots
#	FLY	Make people fly when walls regenerate under them
#	VOLCANO	Include occasional large slime explosions
#	DRONE	Include occasional drone shots
#	BOOTS	Include boots (which makes you immune to slime)
#	OTTO	Reserved for CGL automatic player
#
GAME_PARAM=	-DRANDOM -DREFLECT -DMONITOR -DOOZE -DFLY -DVOLCANO -DBOOTS \
		-DOTTO

#
# System parameter flags are:
#	DEBUG		Don't trust everything in the code
#	INTERNET	Use the Internet domain IPC instead of UNIX domain
#	LOG		Use syslog error-logging in driver (needs SYSLOG_42 or
#			or SYSLOG_43)
#	OLDIPC		Use 4.1a internet system calls (must also define
#			INTERNET)
#	TERMINFO	Use terminfo instead of termcap
#	SYSLOG_42	Use 4.2 BSD syslog(3)
#	SYSLOG_43	Use 4.2 BSD syslog(3)
#	LOG		Use syslog(3) for logging errors
#	BSD_RELEASE	Which version of BSD distribution
#			42 is 4.2BSD (implies SYSLOG_42)
#			43 is 4.3BSD (implies BROADCAST, SYSLOG_43)
#	USE_CURSES	Use curses for everything (automatic w/TERMINFO)
#	SIGNAL_TYPE	void or int
#	HPUX		do special HPUX only hacks
#
DEFS_BSD43=	-DINTERNET -DLOG -DBSD_RELEASE=43 -DSIGNAL_TYPE=int
DEFS_BSD44=	-DINTERNET -DLOG -DBSD_RELEASE=44 -DSIGNAL_TYPE=void
DEFS_SUN=	-DINTERNET -DLOG -DBSD_RELEASE=42 -DBROADCAST -DSIGNAL_TYPE=int
DEFS_SUN4_0=	-DINTERNET -DLOG -DSYSLOG_43 -DBROADCAST -DSIGNAL_TYPE=void
DEFS_ULTRIX=	-DINTERNET -DLOG -DBSD_RELEASE=42 -DSIGNAL_TYPE=void
DEFS_HPUX=	-DHPUX -DINTERNET -DLOG -DSYSLOG_43 -DBROADCAST -DTERMINFO -DSIGNAL_TYPE=void
DEFS_SGI=	-DINTERNET -DLOG -DBSD_RELEASE=43 -DTERMINFO -DSIGNAL_TYPE=void
DEFS_NEXT=	$(DEFS_43) -bsd -traditional -Dconst= -DSIGNAL_TYPE=int
DEFS_OSF1=	-DINTERNET -DLOG -DBSD_RELEASE=43 -DSIGNAL_TYPE=void -D_BSD

#
# The following flags are used for system specific compilation arguments.
# Change them to include the appropriate arguments.  For example, on SGI
# machines, they should look like
# SYSCFLAGS=	-I/usr/include/bsd
# SYSLIBS=	-lbsd
#
SYSCFLAGS=
SYSLIBS=

#
# Generic definitions
#
DEFS=		$(GAME_PARAM) $(DEFS_BSD44)
CPPFLAGS+=	$(SYSCFLAGS) $(DEFS) -DHUNTD=\"/usr/games/huntd\"

.include "../Makefile.inc"
