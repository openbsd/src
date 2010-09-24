# $Id: dgux.sh,v 1.9 2001-05-07 00:06:00-05 Takis Exp $

# This is a hints file for DGUX, which is EMC's Data General's Unix.  It 
# was originally developed with version 5.4.3.10 of the OS, and then was
# later updated running under version 4.11.2 (running on m88k hardware).
# The gross features should work with versions going back to 2.nil but
# some tweaking will probably be necessary.
#
# DGUX is an SVR4 derivative.  It ships with gcc as the standard
# compiler.  Since version 3.0 it has shipped with Perl 4.036
# installed in /usr/bin, which is kind of neat.  Be careful when you
# install that you don't overwrite the system version, though (by
# answering yes to the question about installing perl as /usr/bin/perl),
# as it would suck to try to get support if the vendor learned that you
# were physically replacing the system binaries.
#
# -Roderick Schertler <roderick@argon.org>

# The standard system compiler is gcc, but invoking it as cc changes its
# behavior.  I have to pick one name or the other so I can get the
# dynamic loading switches right (they vary depending on this).  I'm
# picking gcc because there's no way to get at the optimization options
# and so on when you call it cc.

##########################################
# Modified by Takis Psarogiannakopoulos
# Universirty of Cambridge
# Centre for Mathematical Sciences
# Department of Pure Mathematics
# Wilberforce road
# Cambridge CB3 0WB , UK
# e-mail <takis@XFree86.Org>
# Use GCC-2.95.2/3 rev (DG/UX) for threads
# This compiler supports the -pthread switch
# to link correctly DG/UX 's -lthread.
# March 2002
###########################################

cc=gcc
ccflags="-DDGUX -D_DGUX_SOURCE"
# Debug build. If using GNU as,ld use the flag -gstabs+
# ccflags="-g -mstandard -DDGUX -D_DGUX_SOURCE -DDEBUGGING"
# Dummy ; always compile with -O2 on GCC 2.95.2/3 rev (DG/UX)
# even if you debugging the program!
optimize="-mno-legend -O2"

archname="ix86-dgux"
libpth="/usr/lib"

#####################################
# <takis@XFree86.Org>
# Change this if you want.
# prefix =/usr/local
#####################################

prefix=/usr/local
perlpath="$prefix/bin/perl512"
startperl="#! $prefix/bin/perl512"
privlib="$prefix/lib/perl512"
man1dir="$prefix/man/man1"
man3dir="$prefix/man/man3"

sitearch="$prefix/lib/perl512/$archname"
sitelib="$prefix/lib/perl512"

#Do not overwrite by default /usr/bin/perl of DG/UX
installusrbinperl="$undef"

# Configure may fail to find lstat()
# function in <sys/stat.h>.
d_lstat='define'

# Internal (perl) malloc is causing serious problems and
# test failures in DG/UX. Most notable Embed.t 
# So for perl-5.7.3 and on do NOT use. 
# I have no time to investigate more.
# <takis@XFree86.Org>

case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

case "$uselongdouble" in
'') uselongdouble='y' ;;
esac

#usevfork=true
usevfork=false

# DG has this thing set up with symlinks which point to different places
# depending on environment variables (see elink(5)) and the compiler and
# related tools use them to access different development environments
# (COFF, ELF, m88k BCS and so on), see sde(5).  The upshot, however, is
# that when a normal program tries to access one of these elinks it sees
# no such file (like stat()ting a mis-directed symlink).  Setting
# $plibpth to explicitly include the place to which the elinks point
# allows Configure to find libraries which vary based on the development
# environment.
#
# Starting with version 4.10 (the first time the OS supported Intel
# hardware) all libraries are accessed with this mechanism.
#
# The default $TARGET_BINARY_INTERFACE changed with version 4.10.  The
# system now comes with a link named /usr/sde/default which points to
# the proper entry, but older versions lacked this and used m88kdgux
# directly.

: && sde_path=${SDE_PATH:-/usr}/sde	# hide from Configure
while : # dummy loop
do
    if [ -n "$TARGET_BINARY_INTERFACE" ]
	then set X "$TARGET_BINARY_INTERFACE"
	else set X default dg m88k_dg ix86_dg m88kdgux m88kdguxelf
    fi
    shift
    default_sde=$1
    for sde
    do
	[ -d "$sde_path/$sde" ] && break 2
    done
    cat <<END >&2

NOTE:  I can't figure out what SDE is used by default on this machine (I
didn't find a likely directory under $sde_path).  This is bad news.  If
this is a R4.10 or newer system I'm not going to be able to find any of
your libraries, if this system is R3.10 or older I won't be able to find
the math library.  You should re-run Configure with the environment
variable TARGET_BINARY_INTERFACE set to the proper value for this
machine, see sde(5) and the notes in hints/dgux.sh.

END
    sde=$default_sde
    break
done

plibpth="$plibpth $sde_path/$sde/usr/lib"
unset sde_path default_sde sde

#####################################
# <takis@XFree86.Org>
#####################################

libperl="libperl512.so"

# Many functions (eg, gethostent(), killpg(), getpriority(), setruid()
# dbm_*(), and plenty more) are defined in -ldgc.  Usually you don't
# need to know this (it seems that libdgc.so is searched automatically
# by ld), but Configure needs to check it otherwise it will report all
# those functions as missing.

#####################################
# <takis@XFree86.Org>
#####################################

# libswanted="dgc gdbm $libswanted"
#libswanted="dbm posix $libswanted"
# Do *NOT* add there the malloc native 
# DG/UX library!
libswanted="dbm posix resolv socket nsl dl m"

#####################################
# <takis@XFree86.Org>
#####################################

mydomain='.localhost'
cf_by=`(whoami) 2>/dev/null`
cf_email="$cf_by@localhost"

# Dynamic loading works using the dlopen() functions.  Note that dlfcn.h
# used to be broken, it declared _dl*() rather than dl*().  This was the
# case up to 3.10, it has been fixed in 4.11.  I'm not sure if it was
# fixed in 4.10.  If you have the older header just ignore the warnings
# (since pointers and integers have the same format on m88k).

# usedl=true
usedl=false

# For cc rather than gcc the flags would be `-K PIC' for compiling and
# -G for loading.  I haven't tested this.

#####################################
# <takis@XFree86.Org>
# Use -fPIC instead -fpic 
#####################################

cccdlflags=-fPIC
#We must use gcc
ld="gcc"
lddlflags="-shared"

############################################################################
# DGUX Posix 4A Draft 10 Thread support
# <takis@XFree86.Org>
# use Configure -Dusethreads to enable
############################################################################

cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
	ccflags="$ccflags"
	# DG/UX has this for sure! Main Configure fails to
	# detect it but it is needed!
	d_pthread_atfork='define'
	shift
	# DG/UX's sched_yield is in -lrte
	# Do *NOT* add there the malloc native 
	# DG/UX library!
	libswanted="dbm posix resolv socket nsl dl m rte"
	archname="ix86-dgux-thread"
	sitearch="$prefix/lib/perl512/$archname"
	sitelib="$prefix/lib/perl512"
  case "$cc" in
	*gcc*)
	   #### Use GCC -2.95.2/3 rev (DG/UX) and -pthread
	   #### Otherwise take out the switch -pthread 
	   #### And add manually the -D_POSIX4A_DRAFT10_SOURCE flag.
	   ld="gcc"
	   ccflags="$ccflags -D_POSIX4A_DRAFT10_SOURCE"
	   # Debug build : use -DS flag on command line perl
	   # ccflags="$ccflags -g -mstandard -DDEBUGGING -D_POSIX4A_DRAFT10_SOURCE -pthread"
	   cccdlflags='-fPIC'
	   lddlflags="-shared"
	   #### Use GCC -2.95.2/3 rev (DG/UX) and -pthread
	   #### Otherwise take out the switch -pthread
	   #### And add manually the -lthread library.
	   ldflags="$ldflags -pthread"
	;;

	*)
	   echo "Not supported DG/UX cc and threads !"
	;;
  esac
esac
EOCBU

# "./Configure -d" can't figure this out easily
d_suidsafe='define'

###################################################
