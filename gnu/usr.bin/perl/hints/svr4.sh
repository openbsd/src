# svr4 hints, System V Release 4.x
# Last modified 1995/01/28 by Tye McQueen, tye@metronet.com
# Use Configure -Dcc=gcc to use gcc.
case "$cc" in
'') cc='/bin/cc'
    test -f $cc || cc='/usr/ccs/bin/cc'
    ;;
esac
# We include support for using libraries in /usr/ucblib, but the setting
# of libswanted excludes some libraries found there.  You may want to
# prevent "ucb" from being removed from libswanted and see if perl will
# build on your system.
ldflags='-L/usr/ccs/lib -L/usr/ucblib'
ccflags='-I/usr/include -I/usr/ucbinclude'
# Don't use problematic libraries:
libswanted=`echo " $libswanted " | sed -e 's/ malloc / /'` # -e 's/ ucb / /'`
# libmalloc.a - Probably using Perl's malloc() anyway.
# libucb.a - Remove it if you have problems ld'ing.  We include it because
#   it is needed for ODBM_File and NDBM_File extensions.
if [ -r /usr/ucblib/libucb.a ]; then	# If using BSD-compat. library:
    d_gconvert='undef'	# Unusuable under UnixWare 1.1 [use gcvt() instead]
    # Use the "native" counterparts, not the BSD emulation stuff:
    d_bcmp='undef' d_bcopy='undef' d_bzero='undef' d_safebcpy='undef'
    d_index='undef' d_killpg='undef' d_getprior='undef' d_setprior='undef'
    d_setlinebuf='undef' d_setregid='undef' d_setreuid='undef'
fi
d_suidsafe='define'	# "./Configure -d" can't figure this out easilly
usevfork='false'

# Configure may fail to find lstat() since it's a static/inline
# function in <sys/stat.h> on Unisys U6000 SVR4, and possibly
# other SVR4 derivatives.
d_lstat=define

cat <<'EOM' >&4

If you wish to use dynamic linking, you must use 
	LD_LIBRARY_PATH=`pwd`; export LD_LIBRARY_PATH
or
	setenv LD_LIBRARY_PATH `pwd`
before running make.

EOM
