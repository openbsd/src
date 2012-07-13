#	$OpenBSD: Makefile,v 1.1.1.1 2012/07/13 17:49:53 eric Exp $

#
# Note on building and running the regress tests:
#
# The regress suite builds two sets of static executables: one linked
# against the current libc, and one linked against a libc with the resolver
# replaced. The idea is to compare the output of all programs in both cases.
#
# So before building the regression test programs, you need to have the two
# libc.a ready.  By default, it is assumed that the asr-enabled libc was just
# built from src but not installed, so the current libc.a is expected to be
# found in /usr/lib, and the new one in /usr/obj/lib/libc. If your setting
# is different, just set LIBCDIRSTD and LIBCDIRASR accordingly.
# 
# When done, run "make && make install". This builds and installs the two sets
# of programs in REGRESSDIR. They will run chrooted there to allow testing with
# various /etc environment without messing up the local machine config files.
#
# When you are sure everything is in place, run "make regress". This will
# create two files: $REGRESSDIR/output.log with the output of all run tests,
# and $REGRESSDIR/regress.log with the diffs for those that "failed".
#
# Note that "install" and "regress" targets need to be run as root.
#

LIBCDIRSTD?=	/usr/lib
LIBCDIRASR?=	/usr/obj/lib/libc

REGRESSDIR?=	/tmp/regress

all: build

build:
	cd bin && EXT=.asr LDFLAGS=-L${LIBCDIRASR} make
	cd bin && EXT=.std LDFLAGS=-L${LIBCDIRSTD} make

clean:
	cd bin && EXT=.std make clean
	cd bin && EXT=.asr make clean

install:
	mkdir -p ${REGRESSDIR}/etc
	mkdir -p ${REGRESSDIR}/bin
	cd bin && BINDIR=${REGRESSDIR}/bin EXT=.std make install
	cd bin && BINDIR=${REGRESSDIR}/bin EXT=.asr make install

uninstall:
	rm -rf ${REGRESSDIR}

regress:
	RUNDIR=${REGRESSDIR} sh ${.CURDIR}/regress.sh
