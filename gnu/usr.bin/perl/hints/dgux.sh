# $Id: dgux.sh,v 1.4 1996/01/18 03:40:38 roderick Exp $

# This is a hints file for DGUX, which is Data General's Unix.  It was
# developed using version 5.4.3.10 of the OS.  I think the gross
# features should work with versions 5.4.2 through 5.4.4.11 with perhaps
# minor tweaking, but I don't have any older or newer versions installed
# at the moment with which to test it.
#
# DGUX is a SVR4 derivative.  It ships with gcc as the standard
# compiler.  Since version 5.4.3.0 it has shipped with Perl 4.036
# installed in /usr/bin, which is kind of neat.  Be careful when you
# install that you don't overwrite the system version, though (by
# answering yes to the question about installing perl as /usr/bin/perl),
# as it would suck to try to get support if the vendor learned that you
# were physically replacing the system binaries.
#
# Be aware that if you opt to use dynamic loading you'll need to set
# your $LD_LIBRARY_PATH to include the source directory when you build,
# test and install the software.
#
# -Roderick Schertler <roderick@gate.net>


# Here are the things from some old DGUX hints files which are different
# from what's in here now.  I don't know the exact reasons that most of
# these settings were in the hints files, presumably they can be chalked
# up to old Configure inadequacies and changes in the OS headers and the
# like.  These settings might make a good place to start looking if you
# have problems.
#
# This was specified the the 4.036 hints file.  That hints file didn't
# say what version of the OS it was developed using.
#
#     cppstdin='/lib/cpp'
#
# The 4.036 and 5.001 hints files both contained these.  The 5.001 hints
# file said it was developed with version 5.4.2.01 of DGUX.
#
#     gidtype='gid_t'
#     groupstype='gid_t'
#     uidtype='uid_t'
#     d_index='define'
#     cc='gcc'
#
# These were peculiar to the 5.001 hints file.
#
#     ccflags='-D_POSIX_SOURCE -D_DGUX_SOURCE'
#
#     # an ugly hack, since the Configure test for "gcc -P -" hangs.
#     # can't just use 'cppstdin', since our DG has a broken cppstdin :-(
#     cppstdin=`cd ..; pwd`/cppstdin
#     cpprun=`cd ..; pwd`/cppstdin
#
# One last note:  The 5.001 hints file said "you don't want to use
# /usr/ucb/cc" in the place at which it set cc to gcc.  That in
# particular baffles me, as I used to have 5.4.2.01 loaded and my memory
# is telling me that even then /usr/ucb was a symlink to /usr/bin.


# The standard system compiler is gcc, but invoking it as cc changes its
# behavior.  I have to pick one name or the other so I can get the
# dynamic loading switches right (they vary depending on this).  I'm
# picking gcc because there's no way to get at the optimization options
# and so on when you call it cc.
case $cc in
    '')
	cc=gcc
	case $optimize in
	    '') optimize=-O2;;
	esac
	;;
esac

usevfork=true

# DG has this thing set up with symlinks which point to different places
# depending on environment variables (see elink(5)) and the compiler and
# related tools use them to access different development environments
# (COFF, ELF, m88k BCS and so on), see sde(5).  The upshot, however, is
# that when a normal program tries to access one of these elinks it sees
# no such file (like stat()ting a mis-directed symlink).  Setting
# $plibpth to explicitly include the place to which the elinks point
# allows Configure to find libraries which vary based on the development
# environment.
plibpth="$plibpth \
    ${SDE_PATH:-/usr}/sde/${TARGET_BINARY_INTERFACE:-m88kdgux}/usr/lib"

# Many functions (eg, gethostent(), killpg(), getpriority(), setruid()
# dbm_*(), and plenty more) are defined in -ldgc.  Usually you don't
# need to know this (it seems that libdgc.so is searched automatically
# by ld), but Configure needs to check it otherwise it will report all
# those functions as missing.
libswanted="dgc $libswanted"

# Dynamic loading works using the dlopen() functions.  Note that dlfcn.h
# is broken, it declares _dl*() rather than dl*().  (This is in my
# I'd-open-a-ticket-about-this-if-it-weren't-going-to-be-such-a-hassle
# file.)  You can ignore the warnings caused by the missing
# declarations, they're harmless.
usedl=true
# For cc rather than gcc the flags would be `-K PIC' for compiling and
# -G for loading.  I haven't tested this.
cccdlflags=-fpic
lddlflags=-shared
# The Perl library has to be built as a shared library so that dynamic
# loading will work (otherwise code loaded with dlopen() won't be able
# to reference symbols in the main part of perl).  Note that since
# Configure doesn't normally prompt about $d_shrplib this will cause a
# `Whoa there!'.  This is normal, just keep the recommended value.  A
# consequence of all this is that you've got to include the source
# directory in your LD_LIBRARY_PATH when you're building and testing
# perl.
d_shrplib=define

# The system has a function called dg_flock() which is an flock()
# emulation built using fcntl() locking.  Perl currently comes with an
# flock() emulation which uses lockf(), it should eventually also
# include an fcntl() emulation of its own.  Until that happens I
# recommend using DG's emulation (and ignoring the `WHOA THERE!' this
# causes), it provides semantics closer to the original than the lockf()
# emulation.
ccflags="$ccflags -Dflock=dg_flock"
d_flock=define
