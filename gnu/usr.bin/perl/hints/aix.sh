# hints/aix.sh

# AIX 3 and AIX 4 are split off to aix_3.sh and aix_4.sh
#    early Feb 2004 by H.Merijn Brand
# please read comments in there for historic questions.
#    many now stripped here

# Contact dfavor@corridor.com for any of the following:
#
#    - AIX 43x and above support
#    - gcc + threads support
#    - socks support
#
# Notes:
#
#    - shared libperl support is tricky. if ever libperl.a ends up
#      in /usr/local/lib/* it can override any subsequent builds of
#      that same perl release. to make sure you know where the shared
#      libperl.a is coming from do a 'dump -Hv perl' and check all the
#      library search paths in the loader header.
#
#      it would be nice to warn the user if a libperl.a exists that is
#      going to override the current build, but that would be complex.
#
#      better yet, a solid fix for this situation should be developed.
#

# Configure finds setrgid and setruid, but they're useless.  The man
# pages state:
#    setrgid: The EPERM error code is always returned.
#    setruid: The EPERM error code is always returned. Processes cannot
#	      reset only their real user IDs.
d_setrgid='undef'
d_setruid='undef'

alignbytes=8

case "$usemymalloc" in
    '')  usemymalloc='n' ;;
    esac

# malloc wrap works, but not in vac-5, see later
case "$usemallocwrap" in
    '') usemallocwrap='define' ;;
    esac

# Intuiting the existence of system calls under AIX is difficult,
# at best; the safest technique is to find them empirically.

case "$usenativedlopen" in
    '') usenativedlopen='true' ;;
    esac

so="a"
# AIX itself uses .o (libc.o) but we prefer compatibility
# with the rest of the world and with rest of the scripting
# languages (Tcl, Python) and related systems (SWIG).
# Stephanie Beals <bealzy@us.ibm.com>
dlext="so"

# Take possible hint from the environment.  If 32-bit is set in the
# environment, we can override it later.  If set for 64, the
# 'sizeof' test sees a native 64-bit architecture and never looks back.
case "$OBJECT_MODE" in
    32) cat >&4 <<EOF

You have OBJECT_MODE=32 set in the environment.
I take this as a hint you do not want to
build for a 64-bit address space. You will be
given the opportunity to change this later.
EOF
	;;
    64) cat >&4 <<EOF

You have OBJECT_MODE=64 set in the environment.
This forces a full 64-bit build.  If that is
not what you intended, please terminate this
program, unset it and restart.
EOF
	;;
    esac

# uname -m output is too specific and not appropriate here
case "$archname" in
    '') archname="$osname" ;;
    esac

cc=${cc:-cc}

ccflags="$ccflags -D_ALL_SOURCE -D_ANSI_C_SOURCE -D_POSIX_SOURCE"
case "$cc" in
    *gcc*) ;;
    *) ccflags="$ccflags -qmaxmem=-1 -qnoansialias" ;;
    esac
nm_opt='-B'

# These functions don't work like Perl expects them to.
d_setregid='undef'
d_setreuid='undef'

# Changes for dynamic linking by Wayne Scott <wscott@ichips.intel.com>
#
# Tell perl which symbols to export for dynamic linking.
cccdlflags='none'	# All AIX code is position independent
   cc_type=xlc		# do not export to config.sh
case "$cc" in
    *gcc*)
	cc_type=gcc
	ccdlflags='-Xlinker'
	if [ "X$gccversion" = "X" ]; then
	    # Done too late in Configure if hinted
	    gccversion=`$cc --version | sed 's/.*(GCC) *//'`
	    fi
	;;

    *)  ccversion=`lslpp -L | grep 'C for AIX Compiler$' | grep -v '\.msg\.[A-Za-z_]*\.' | head -1 | awk '{print $1,$2}'`
	case "$ccversion" in
	    '') ccversion=`lslpp -L | grep 'IBM C and C++ Compilers LUM$'` ;;

	    *.*.*.*.*.*.*)	# Ahhrgg, more than one C compiler installed
		first_cc_path=`which ${cc:-cc}`
		case "$first_cc_path" in
		    *vac*)
			cc_type=vac ;;

		    /usr/bin/cc)		# Check the symlink
			if [ -h $first_cc_path ] ; then
			    ls -l $first_cc_path > reflect
			    if grep -i vac reflect >/dev/null 2>&1 ; then
				cc_type=vac
				fi
			    rm -f reflect
			    fi
			;;
		    esac
		ccversion=`lslpp -L | grep 'C for AIX Compiler$' | grep -i $cc_type | head -1`
		;;

	    vac*.*.*.*)
		cc_type=vac
		;;
	    esac

	ccversion=`echo "$ccversion" | awk '{print $2}'`
	# Redbooks state AIX-5 only supports vac-5.0.2.0 and up
	case "$ccversion" in
	    5*) usemallocwrap='n' ;; # panic in miniperl
	    esac
	;;
    esac

# the required -bE:$installarchlib/CORE/perl.exp is added by
# libperl.U (Configure) later.

# The first 3 options would not be needed if dynamic libs. could be linked
# with the compiler instead of ld.
# -bI:$(PERL_INC)/perl.exp  Read the exported symbols from the perl binary
# -bE:$(BASEEXT).exp	    Export these symbols.  This file contains only one
#			    symbol: boot_$(EXP)	 can it be auto-generated?
lddlflags="$lddlflags -bhalt:4 -bM:SRE -bI:\$(PERL_INC)/perl.exp -bE:\$(BASEEXT).exp -bnoentry -lc"

case "$use64bitall" in
    $define|true|[yY]*) use64bitint="$define" ;;
    esac

case "$usemorebits" in
    $define|true|[yY]*) use64bitint="$define"; uselongdouble="$define" ;;
    esac

case $cc_type in
    vac|xlc)
	case "$uselongdouble" in
	    $define|true|[yY]*)
		ccflags="$ccflags -qlongdouble"
		libswanted="c128 $libswanted"
		lddlflags=`echo "$lddlflags " | sed -e 's/ -lc / -lc128 -lc /'`
		;;
	    esac
	;;
    esac

case "$cc" in
    *gcc*) ;;

    cc*|xlc*) # cc should've been set by line 116 or so if empty.
	if test ! -x /usr/bin/$cc -a -x /usr/vac/bin/$cc; then
	    case ":$PATH:" in
		*:/usr/vac/bin:*) ;;
		*)  if test ! -x /QOpenSys/usr/bin/$cc; then
			# The /QOpenSys/usr/bin/$cc saves us if we are
			# building natively in OS/400 PASE.
			cat >&4 <<EOF

***
*** You either implicitly or explicitly specified an IBM C compiler,
*** but you do not seem to have one in /usr/bin, but you seem to have
*** the VAC installed in /usr/vac, but you do not have the /usr/vac/bin
*** in your PATH.  I suggest adding that and retrying Configure.
***
EOF
			exit 1
			fi
		    ;;
		esac
	    fi
	;;
    esac

case "$ldlibpthname" in
    '') ldlibpthname=LIBPATH ;;
    esac

# This script UU/usethreads.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
    $define|true|[yY]*)
	d_drand48_r='undef'
	d_endgrent_r='undef'
	d_endpwent_r='undef'
	d_getgrent_r='undef'
	d_getpwent_r='undef'
	d_random_r='undef'
	d_setgrent_r='undef'
	d_setpwent_r='undef'
	d_srand48_r='undef'
	d_strerror_r='undef'

	ccflags="$ccflags -DNEED_PTHREAD_INIT"
	case "$cc" in
	    *gcc*) ccflags="-D_THREAD_SAFE $ccflags" ;;

	    cc_r) ;;
	    '') cc=cc_r ;;

	    *)


	    # No | alternation in aix sed. :-(
	    newcc=`echo $cc | sed -e 's/cc$/cc_r/' -e 's/xl[cC]$/cc_r/' -e 's/xl[cC]_r$/cc_r/'`
	    case "$newcc" in
		$cc) # No change
		;;

		*cc_r)
		echo >&4 "Switching cc to cc_r because of POSIX threads."
		# xlc_r has been known to produce buggy code in AIX 4.3.2.
		# (e.g. pragma/overload core dumps)	 Let's suspect xlC_r, too.
		# --jhi@iki.fi
		cc="$newcc"
		;;

		*)
		cat >&4 <<EOM
*** For pthreads you should use the AIX C compiler cc_r.
*** (now your compiler was set to '$cc')
*** Cannot continue, aborting.
EOM
		exit 1
		;;
	    esac
	esac

	# Insert pthreads to libswanted, before any libc or libC.
	set `echo X "$libswanted "| sed -e 's/ \([cC]\) / pthreads \1 /'`
	shift
	libswanted="$*"
	# Insert pthreads to lddlflags, before any libc or libC.
	set `echo X "$lddlflags " | sed -e 's/ \(-l[cC]\) / -lpthreads \1 /'`
	shift
	lddlflags="$*"
	;;
esac
EOCBU

# This script UU/uselargefiles.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use large files.
cat > UU/uselargefiles.cbu <<'EOCBU'
case "$uselargefiles" in
    ''|$define|true|[yY]*)
	# Configure should take care of use64bitint and use64bitall being
	# defined before uselargefiles.cbu is consulted.
	if test X"$use64bitint:$quadtype" = X"$define:long" -o X"$use64bitall" = Xdefine; then
# Keep these at the left margin.
ccflags_uselargefiles="`getconf XBS5_LP64_OFF64_CFLAGS 2>/dev/null`"
ldflags_uselargefiles="`getconf XBS5_LP64_OFF64_LDFLAGS 2>/dev/null`"
	else
# Keep these at the left margin.
ccflags_uselargefiles="`getconf XBS5_ILP32_OFFBIG_CFLAGS 2>/dev/null`"
ldflags_uselargefiles="`getconf XBS5_ILP32_OFFBIG_LDFLAGS 2>/dev/null`"
	    fi
	if test X"$use64bitint:$quadtype" = X"$define:long" -o X"$use64bitall" = Xdefine; then
# Keep this at the left margin.
libswanted_uselargefiles="`getconf XBS5_LP64_OFF64_LIBS 2>/dev/null|sed -e 's@^-l@@' -e 's@ -l@ @g`"
	else
# Keep this at the left margin.
libswanted_uselargefiles="`getconf XBS5_ILP32_OFFBIG_LIBS 2>/dev/null|sed -e 's@^-l@@' -e 's@ -l@ @g`"
	    fi

	case "$ccflags_uselargefiles$ldflags_uselargefiles$libs_uselargefiles" in
	    '') ;;
	    *)  ccflags="$ccflags $ccflags_uselargefiles"
		ldflags="$ldflags $ldflags_uselargefiles"
		libswanted="$libswanted $libswanted_uselargefiles"
		;;
	    esac

	case "$gccversion" in
	    '') ;;
	    *)  # Remove xlc-specific -qflags.
		ccflags="`echo $ccflags | sed -e 's@ -q[^ ]*@ @g' -e 's@^-q[^ ]* @@g'`"
		ldflags="`echo $ldflags | sed -e 's@ -q[^ ]*@ @g' -e 's@^-q[^ ]* @@g'`"
		# Move xlc-specific -bflags.
		ccflags="`echo $ccflags | sed -e 's@ -b@ -Wl,-b@g'`"
		ldflags="`echo ' '$ldflags | sed -e 's@ -b@ -Wl,-b@g'`"
		lddlflags="`echo ' '$lddlflags | sed -e 's@ -b@ -Wl,-b@g'`"
		ld='gcc'
		echo >&4 "(using ccflags   $ccflags)"
		echo >&4 "(using ldflags   $ldflags)"
		echo >&4 "(using lddlflags $lddlflags)"
		;;
	    esac
	;;
    esac
EOCBU

cat > UU/use64bitall.cbu <<'EOCBU'
# This script UU/use64bitall.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to be maximally 64-bitty.
case "$use64bitall" in
    $define|true|[yY]*)
	echo " "
	echo "Checking the CPU width of your hardware..." >&4
	$cat >size.c <<EOCP
#include <stdio.h>
#include <sys/systemcfg.h>
int main (void)
{
    printf ("%d\n", _system_configuration.width);
    return (0);
    }
EOCP
	set size
	if eval $compile_ok; then
	    qacpuwidth=`./size`
	    echo "You are running on $qacpuwidth bit hardware."
	else
	    dflt="32"
	    echo " "
	    echo "(I can't seem to compile the test program.  Guessing...)"
	    rp="What is the width of your CPU (in bits)?"
	    . ./myread
	    qacpuwidth="$ans"
	    fi
	$rm -f size.c size

	case "$qacpuwidth" in
	    32*)
		cat >&4 <<EOM
Bzzzt! At present, you can only perform a
full 64-bit build on a 64-bit machine.
EOM
		exit 1
		;;
	    esac
	qacflags="`getconf XBS5_LP64_OFF64_CFLAGS 2>/dev/null`"
	qaldflags="`getconf XBS5_LP64_OFF64_LDFLAGS 2>/dev/null`"
	# See jhi's comments above regarding this re-eval.  I've
	# seen similar weirdness in the form of:
	#
# 1506-173 (W) Option lm is not valid.  Enter xlc for list of valid options.
	#
	# error messages from 'cc -E' invocation. Again, the offending
	# string is simply not detectable by any means.  Since it doesn't
	# do any harm, I didn't pursue it. -- sh
	qaldflags="`echo $qaldflags`"
	qalibs="`getconf XBS5_LP64_OFF64_LIBS 2>/dev/null|sed -e 's@^-l@@' -e 's@ -l@ @g`"
	# -q32 and -b32 may have been set by uselargefiles or user.
	# Remove them.
	ccflags="`echo $ccflags | sed -e 's@-q32@@'`"
	ldflags="`echo $ldflags | sed -e 's@-b32@@'`"
	# Tell archiver to use large format.  Unless we remove 'ar'
	# from 'trylist', the Configure script will just reset it to 'ar'
	# immediately prior to writing config.sh.  This took me hours
	# to figure out.
	trylist="`echo $trylist | sed -e 's@^ar @@' -e 's@ ar @ @g' -e 's@ ar$@@'`"
	ar="ar -X64"
	nm_opt="-X64 $nm_opt"
	# Note: Placing the 'qacflags' variable into the 'ldflags' string
	# is NOT a typo.  ldflags is passed to the C compiler for final
	# linking, and it wants -q64 (-b64 is for ld only!).
	case "$qacflags$qaldflags$qalibs" in
	    '') ;;
	    *)  ccflags="$ccflags $qacflags"
		ldflags="$ldflags $qacflags"
		lddlflags="$qaldflags $lddlflags"
		libswanted="$libswanted $qalibs"
		;;
	    esac
	case "$ccflags" in
	    *-DUSE_64_BIT_ALL*) ;;
	    *) ccflags="$ccflags -DUSE_64_BIT_ALL";;
	    esac
	case "$archname64" in
	    ''|64*) archname64=64all ;;
	    esac
	longsize="8"
	qacflags=''
	qaldflags=''
	qalibs=''
	qacpuwidth=''
	;;
    esac
EOCBU

if test $usenativedlopen = 'true' ; then
    ccflags="$ccflags -DUSE_NATIVE_DLOPEN"
    case "$cc" in
	*gcc*) ldflags="$ldflags -Wl,-brtl" ;;
	*)     ldflags="$ldflags -brtl" ;;
	esac
elif test -f /lib/libC.a -a X"`$cc -v 2>&1 | grep gcc`" = X; then
    # If the C++ libraries, libC and libC_r, are available we will
    # prefer them over the vanilla libc, because the libC contain
    # loadAndInit() and terminateAndUnload() which work correctly
    # with C++ statics while libc load() and unload() do not. See
    # ext/DynaLoader/dl_aix.xs. The C-to-C_r switch is done by
    # usethreads.cbu, if needed.

    # Cify libswanted.
    set `echo X "$libswanted "| sed -e 's/ c / C c /'`
    shift
    libswanted="$*"
    # Cify lddlflags.
    set `echo X "$lddlflags "| sed -e 's/ -lc / -lC -lc /'`
    shift
    lddlflags="$*"
    fi

case "$PASE" in
    define)
	case "$prefix" in
	    '') prefix=/QOpenSys/perl ;;
	    esac
	cat >&4 <<EOF

***
*** You seem to be compiling in AIX for the OS/400 PASE environment.
*** I'm not going to use the AIX bind, nsl, and possible util libraries, then.
*** I'm also not going to install perl as /usr/bin/perl.
*** Perl will be installed under $prefix.
*** For instructions how to install this build from AIX to PASE,
*** see the file README.os400.  Accept the "aix" for the question
*** about "Operating system name".
***
EOF
	set `echo " $libswanted " | sed -e 's@ bind @ @' -e 's@ nsl @ @' -e 's@ util @ @'`
	shift
	libswanted="$*"
	installusrbinperl="$undef"

	# V5R1 doesn't have this (V5R2 does), without knowing
	# which one we have it's safer to be pessimistic.
	# Cwd will work fine even without fchdir(), but if
	# V5R1 tries to use code compiled assuming fchdir(),
	# lots of grief will issue forth from Cwd.
	case "$d_fchdir" in
	    '') d_fchdir="$undef" ;;
	    esac
	;;
    esac

# EOF
