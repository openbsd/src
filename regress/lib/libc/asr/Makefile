#	$OpenBSD: Makefile,v 1.3 2013/04/01 10:18:28 eric Exp $

#
# Note on building and running the regress tests:
#
# The regress suite builds two sets of static executables: one linked
# against the current libc, and one linked against a libc with the resolver
# replaced. The idea is to compare the output of all programs in both cases.
#
# So before building the regression test programs, you need to have two
# static libc: the system libc.a in /usr/lib/ which is assumed to be built
# with asr, and a pre-asr libc.a in the "./lib/" directory (you can build
# one by commenting out the ".include "${LIBCSRCDIR}/asr/Makefile.inc"
# line from the /usr/src/lib/libc/Makefile.in and rebuild the libc).
# 
# When done, run "make && make install". This builds and installs the two sets
# of programs in REGRESSDIR. They will run chrooted there to allow testing with
# various /etc environment without messing up the local machine config files.
#
# When you are sure everything is in place, run "make regress". This will
# create two files: $REGRESSDIR/output.log with the output of all tests,
# and $REGRESSDIR/regress.log with the diffs for those that "failed".
#
# Note that the "install" and "regress" targets need to be run as root.
#

LIBCDIRSTD?=	${.CURDIR}/lib
LIBCDIRASR?=	/usr/lib

REGRESSDIR?=	/tmp/regress
REGRESS?=	regress.sh

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
	RUNDIR=${REGRESSDIR} sh ${.CURDIR}/${REGRESS}
