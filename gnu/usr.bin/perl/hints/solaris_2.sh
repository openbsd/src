# hints/solaris_2.sh
# Last modified: Mon Jan 29 12:52:28 2001
# Lupe Christoph <lupe@lupe-christoph.de>
# Based on version by:
# Andy Dougherty  <doughera@lafayette.edu>
# Which was based on input from lots of folks, especially
# Dean Roehrich <roehrich@ironwood-fddi.cray.com>
# Additional input from Alan Burlison, Jarkko Hietaniemi,
# and Richard Soderberg.
#
# See README.solaris for additional information.
#
# For consistency with gcc, we do not adopt Sun Marketing's
# removal of the '2.' prefix from the Solaris version number.
# (Configure tries to detect an old fixincludes and needs
# this information.)

# If perl fails tests that involve dynamic loading of extensions, and
# you are using gcc, be sure that you are NOT using GNU as and ld.  One
# way to do that is to invoke Configure with
#
#     sh Configure -Dcc='gcc -B/usr/ccs/bin/'
#
#  (Note that the trailing slash is *required*.)
#  gcc will occasionally emit warnings about "unused prefix", but
#  these ought to be harmless.  See below for more details.

# See man vfork.
usevfork=${usevfork:-false}

# Solaris has secure SUID scripts
d_suidsafe=${d_suidsafe:-define}

# Several people reported problems with perl's malloc, especially
# when use64bitall is defined or when using gcc.
#     http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2001-01/msg01318.html
#     http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2001-01/msg00465.html
usemymalloc=${usemymalloc:-false}

# Avoid all libraries in /usr/ucblib.
# /lib is just a symlink to /usr/lib
set `echo $glibpth | sed -e 's@/usr/ucblib@@' -e 's@ /lib @ @'`
glibpth="$*"

# Remove unwanted libraries.  -lucb contains incompatible routines.
# -lld and -lsec don't do anything useful. -lcrypt does not
# really provide anything we need over -lc, so we drop it, too.
# -lmalloc can cause a problem with GNU CC & Solaris.  Specifically,
# libmalloc.a may allocate memory that is only 4 byte aligned, but
# GNU CC on the Sparc assumes that doubles are 8 byte aligned.
# Thanks to  Hallvard B. Furuseth <h.b.furuseth@usit.uio.no>
set `echo " $libswanted " | sed -e 's@ ld @ @' -e 's@ malloc @ @' -e 's@ ucb @ @' -e 's@ sec @ @' -e 's@ crypt @ @'`
libswanted="$*"

# Look for architecture name.  We want to suggest a useful default.
case "$archname" in
'')
    if test -f /usr/bin/arch; then
        archname=`/usr/bin/arch`
    	archname="${archname}-${osname}"
    elif test -f /usr/ucb/arch; then
        archname=`/usr/ucb/arch`
    	archname="${archname}-${osname}"
    fi
    ;;
esac

cat > UU/workshoplibpth.cbu << 'EOCBU'
# This script UU/workshoplibpth.cbu will get 'called-back'
# by other CBUs this script creates.
case "$workshoplibpth_done" in
    '')	if test `uname -p` = "sparc"; then
	case "$use64bitall" in
	    "$define"|true|[yY]*)
		# add SPARC-specific 64 bit libraries
		loclibpth="$loclibpth /usr/lib/sparcv9"
		if test -n "$workshoplibs"; then
		    loclibpth=`echo $loclibpth | sed -e "s% $workshoplibs%%" `
		    for lib in $workshoplibs; do
			# Logically, it should be sparcv9.
			# But the reality fights back, it's v9.
			loclibpth="$loclibpth $lib/sparcv9 $lib/v9"
		    done
		fi
	    ;;
	*)  loclibpth="$loclibpth $workshoplibs"
	    ;;
	esac
	else
	    loclibpth="$loclibpth $workshoplibs"
	fi
	workshoplibpth_done="$define"
	;;
esac
EOCBU

case "$cc" in
'')	if test -f /opt/SUNWspro/bin/cc; then
		cc=/opt/SUNWspro/bin/cc
		cat <<EOF >&4	

You specified no cc but you seem to have the Workshop compiler
($cc) installed, using that.
If you want something else, specify that in the command line,
e.g. Configure -Dcc=gcc

EOF
	fi
	;;
esac

######################################################
# General sanity testing.  See below for excerpts from the Solaris FAQ.
#
# From roehrich@ironwood-fddi.cray.com Wed Sep 27 12:51:46 1995
# Date: Thu, 7 Sep 1995 16:31:40 -0500
# From: Dean Roehrich <roehrich@ironwood-fddi.cray.com>
# To: perl5-porters@africa.nicoh.com
# Subject: Re: On perl5/solaris/gcc
#
# Here's another draft of the perl5/solaris/gcc sanity-checker.

case `type ${cc:-cc}` in
*/usr/ucb/cc*) cat <<END >&4

NOTE:  Some people have reported problems with /usr/ucb/cc.
If you have difficulties, please make sure the directory
containing your C compiler is before /usr/ucb in your PATH.

END
;;
esac


# Check that /dev/fd is mounted.  If it is not mounted, let the
# user know that suid scripts may not work.
df /dev/fd 2>&1 > /dev/null
case $? in
0) ;;
*)
	cat <<END >&4

NOTE: Your system does not have /dev/fd mounted.  If you want to
be able to use set-uid scripts you must ask your system administrator
to mount /dev/fd.

END
	;;
esac


# See if libucb can be found in /usr/lib.  If it is, warn the user
# that this may cause problems while building Perl extensions.
/usr/bin/ls /usr/lib/libucb* >/dev/null 2>&1
case $? in
0)
	cat <<END >&4

NOTE: libucb has been found in /usr/lib.  libucb should reside in
/usr/ucblib.  You may have trouble while building Perl extensions.

END
;;
esac

# Use shell built-in 'type' command instead of /usr/bin/which to
# avoid possible csh start-up problems and also to use the same shell
# we'll be using to Configure and make perl.
# The path name is the last field in the output, but the type command
# has an annoying array of possible outputs, e.g.:
#	make is hashed (/opt/gnu/bin/make)
# 	cc is /usr/ucb/cc
#	foo not found
# use a command like type make | awk '{print $NF}' | sed 's/[()]//g'

# See if make(1) is GNU make(1).
# If it is, make sure the setgid bit is not set.
make -v > make.vers 2>&1
if grep GNU make.vers > /dev/null 2>&1; then
    tmp=`type make | awk '{print $NF}' | sed 's/[()]//g'`
    case "`/usr/bin/ls -lL $tmp`" in
    ??????s*)
	    cat <<END >&2

NOTE: Your PATH points to GNU make, and your GNU make has the set-group-id
bit set.  You must either rearrange your PATH to put /usr/ccs/bin before the
GNU utilities or you must ask your system administrator to disable the
set-group-id bit on GNU make.

END
	    ;;
    esac
fi
rm -f make.vers

cat > UU/cc.cbu <<'EOCBU'
# This script UU/cc.cbu will get 'called-back' by Configure after it
# has prompted the user for the C compiler to use.

# If the C compiler is gcc:
#   - check the fixed-includes
#   - check as(1) and ld(1), they should not be GNU
#	(GNU as and ld 2.8.1 and later are reportedly ok, however.)
# If the C compiler is not gcc:
#   - Check if it is the Workshop/Forte compiler.
#     If it is, prepare for 64 bit and long doubles.
#   - check as(1) and ld(1), they should not be GNU
#	(GNU as and ld 2.8.1 and later are reportedly ok, however.)
#
# Watch out in case they have not set $cc.

# Perl compiled with some combinations of GNU as and ld may not
# be able to perform dynamic loading of extensions.  If you have a
# problem with dynamic loading, be sure that you are using the Solaris
# /usr/ccs/bin/as and /usr/ccs/bin/ld.  You can do that with
#  		sh Configure -Dcc='gcc -B/usr/ccs/bin/'
# (note the trailing slash is required).
# Combinations that are known to work with the following hints:
#
#  gcc-2.7.2, GNU as 2.7, GNU ld 2.7
#  egcs-1.0.3, GNU as 2.9.1 and GNU ld 2.9.1
#	--Andy Dougherty  <doughera@lafayette.edu>
#	Tue Apr 13 17:19:43 EDT 1999

# Get gcc to share its secrets.
echo 'main() { return 0; }' > try.c
	# Indent to avoid propagation to config.sh
	verbose=`${cc:-cc} -v -o try try.c 2>&1`

if echo "$verbose" | grep '^Reading specs from' >/dev/null 2>&1; then
	#
	# Using gcc.
	#

	# See if as(1) is GNU as(1).  GNU as(1) might not work for this job.
	if echo "$verbose" | grep ' /usr/ccs/bin/as ' >/dev/null 2>&1; then
	    :
	else
	    cat <<END >&2

NOTE: You are using GNU as(1).  GNU as(1) might not build Perl.  If you
have trouble, you can use /usr/ccs/bin/as by including -B/usr/ccs/bin/
in your ${cc:-cc} command.  (Note that the trailing "/" is required.)

END
	    # Apparently not needed, at least for as 2.7 and later.
	    # cc="${cc:-cc} -B/usr/ccs/bin/"
	fi

	# See if ld(1) is GNU ld(1).  GNU ld(1) might not work for this job.
	# Recompute $verbose since we may have just changed $cc.
	verbose=`${cc:-cc} -v -o try try.c 2>&1 | grep ld 2>&1`

	if echo "$verbose" | grep ' /usr/ccs/bin/ld ' >/dev/null 2>&1; then
	    # Ok, gcc directly calls the Solaris /usr/ccs/bin/ld.
	    :
	elif echo "$verbose" | grep "ld: Software Generation Utilities" >/dev/null 2>&1; then
	    # Hmm.  gcc doesn't call /usr/ccs/bin/ld directly, but it
	    # does appear to be using it eventually.  egcs-1.0.3's ld
	    # wrapper does this.
	    # All Solaris versions of ld I've seen contain the magic
	    # string used in the grep.
	    :
	else
	    # No evidence yet of /usr/ccs/bin/ld.  Some versions
	    # of egcs's ld wrapper call /usr/ccs/bin/ld in turn but
	    # apparently don't reveal that unless you pass in -V.
	    # (This may all depend on local configurations too.)

	    # Recompute verbose with -Wl,-v to find GNU ld if present
	    verbose=`${cc:-cc} -v -Wl,-v -o try try.c 2>&1 | grep ld 2>&1`

	    myld=`echo $verbose| grep ld | awk '/\/ld/ {print $1}'`
	    # This assumes that gcc's output will not change, and that
	    # /full/path/to/ld will be the first word of the output.
	    # Thus myld is something like /opt/gnu/sparc-sun-solaris2.5/bin/ld

	    # Allow that $myld may be '', due to changes in gcc's output 
	    if ${myld:-ld} -V 2>&1 |
		grep "ld: Software Generation Utilities" >/dev/null 2>&1; then
		# Ok, /usr/ccs/bin/ld eventually does get called.
		:
	    else
		echo "Found GNU ld='$myld'" >&4
		cat <<END >&2

NOTE: You are using GNU ld(1).  GNU ld(1) might not build Perl.  If you
have trouble, you can use /usr/ccs/bin/ld by including -B/usr/ccs/bin/
in your ${cc:-cc} command.  (Note that the trailing "/" is required.)

I will try to use GNU ld by passing in the -Wl,-E flag, but if that
doesn't work, you should use -B/usr/ccs/bin/ instead.

END
		ccdlflags="$ccdlflags -Wl,-E"
		lddlflags="$lddlflags -Wl,-E -G"
	    fi
	fi

else
	#
	# Not using gcc.
	#

	ccversion="`${cc:-cc} -V 2>&1|sed -n -e '1s/^cc: //p'`"
	case "$ccversion" in
	*WorkShop*) ccname=workshop ;;
	*) ccversion='' ;;
	esac

	case "$ccname" in
	workshop)
		cat >try.c <<EOM
#include <sunmath.h>
int main() { return(0); }
EOM
		workshoplibs=`cc -### try.c -lsunmath -o try 2>&1|sed -n '/ -Y /s%.* -Y "P,\(.*\)".*%\1%p'|tr ':' '\n'|grep '/SUNWspro/'`
		. ./workshoplibpth.cbu
	;;
	esac

	# See if as(1) is GNU as(1).  GNU might not work for this job.
	case `as --version < /dev/null 2>&1` in
	*GNU*)
		cat <<END >&2

NOTE: You are using GNU as(1).  GNU as(1) might not build Perl.
You must arrange to use /usr/ccs/bin/as, perhaps by adding /usr/ccs/bin
to the beginning of your PATH.

END
		;;
	esac

	# See if ld(1) is GNU ld(1).  GNU ld(1) might not work for this job.
	# ld --version doesn't properly report itself as a GNU tool,
	# as of ld version 2.6, so we need to be more strict. TWP 9/5/96
	# Sun's ld always emits the "Software Generation Utilities" string.
	if ld -V 2>&1 | grep "ld: Software Generation Utilities" >/dev/null 2>&1; then
	    # Ok, ld is /usr/ccs/bin/ld.
	    :
	else
	    cat <<END >&2

NOTE: You are apparently using GNU ld(1).  GNU ld(1) might not build Perl.
You should arrange to use /usr/ccs/bin/ld, perhaps by adding /usr/ccs/bin
to the beginning of your PATH.

END
	fi

fi

# as --version or ld --version might dump core.
rm -f try try.c
rm -f core

# XXX
EOCBU

cat > UU/usethreads.cbu <<'EOCBU'
# This script UU/usethreads.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use threads.
case "$usethreads" in
$define|true|[yY]*)
        ccflags="-D_REENTRANT $ccflags"

        # sched_yield is in -lposix4 up to Solaris 2.6, in -lrt starting with Solaris 2.7
	case `uname -r` in
	5.[0-6] | 5.5.1) sched_yield_lib="posix4" ;;
	*) sched_yield_lib="rt";
	esac
        set `echo X "$libswanted "| sed -e "s/ c / $sched_yield_lib pthread c /"`
        shift
        libswanted="$*"

        # On Solaris 2.6 x86 there is a bug with sigsetjmp() and siglongjmp()
        # when linked with the threads library, such that whatever positive
        # value you pass to siglongjmp(), sigsetjmp() returns 1.
        # Thanks to Simon Parsons <S.Parsons@ftel.co.uk> for this report.
        # Sun BugID is 4117946, "sigsetjmp always returns 1 when called by
        # siglongjmp in a MT program". As of 19980622, there is no patch
        # available.
        cat >try.c <<'EOM'
	/* Test for sig(set|long)jmp bug. */
	#include <setjmp.h>

	main()
	{
	    sigjmp_buf env;
	    int ret;

	    ret = sigsetjmp(env, 1);
	    if (ret) { return ret == 2; }
	    siglongjmp(env, 2);
	}
EOM
        if test "`arch`" = i86pc -a `uname -r` = 5.6 && \
           ${cc:-cc} try.c -lpthread >/dev/null 2>&1 && ./a.out; then
 	    d_sigsetjmp=$undef
	    cat << 'EOM' >&2

You will see a *** WHOA THERE!!! ***  message from Configure for
d_sigsetjmp.  Keep the recommended value.  See hints/solaris_2.sh
for more information.

EOM
        fi

	# These prototypes should be visible since we using
	# -D_REENTRANT, but that does not seem to work.
	# It does seem to work for getnetbyaddr_r, weirdly enough,
	# and other _r functions. (Solaris 8)

	d_ctermid_r_proto="$define"
	d_gethostbyaddr_r_proto="$define"
	d_gethostbyname_r_proto="$define"
	d_getnetbyname_r_proto="$define"
	d_getprotobyname_r_proto="$define"
	d_getprotobynumber_r_proto="$define"
	d_getservbyname_r_proto="$define"
	d_getservbyport_r_proto="$define"

	# Ditto. (Solaris 7)
	d_readdir_r_proto="$define"
	d_readdir64_r_proto="$define"
	d_tmpnam_r_proto="$define"
	d_ttyname_r_proto="$define"

	;;
esac
EOCBU

cat > UU/uselargefiles.cbu <<'EOCBU'
# This script UU/uselargefiles.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use large files.
case "$uselargefiles" in
''|$define|true|[yY]*)

# Keep these in the left margin.
ccflags_uselargefiles="`getconf LFS_CFLAGS 2>/dev/null`"
ldflags_uselargefiles="`getconf LFS_LDFLAGS 2>/dev/null`"
libswanted_uselargefiles="`getconf LFS_LIBS 2>/dev/null|sed -e 's@^-l@@' -e 's@ -l@ @g`"

    ccflags="$ccflags $ccflags_uselargefiles"
    ldflags="$ldflags $ldflags_uselargefiles"
    libswanted="$libswanted $libswanted_uselargefiles"
    ;;
esac
EOCBU

# This is truly a mess.
case "$usemorebits" in
"$define"|true|[yY]*)
	use64bitint="$define"
	uselongdouble="$define"
	;;
esac

if test `uname -p` = "sparc"; then
    cat > UU/use64bitint.cbu <<'EOCBU'
# This script UU/use64bitint.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use 64 bit integers.
case "$use64bitint" in
"$define"|true|[yY]*)
	    case "`uname -r`" in
	    5.[0-4])
		cat >&4 <<EOM
Solaris `uname -r|sed -e 's/^5\./2./'` does not support 64-bit integers.
You should upgrade to at least Solaris 2.5.
EOM
		exit 1
		;;
	    esac
	    ;;
esac
# gcc-2.8.1 on Solaris 8 with -Duse64bitint fails op/pat.t test 822
# if we compile regexec.c with -O.  Turn off optimization for that one
# file.  See hints/README.hints , especially 
# =head2 Propagating variables to config.sh, method 3.
#  A. Dougherty  May 24, 2002
case "$use64bitint" in
"$define")
    case "${gccversion}-${optimize}" in
    2.8*-O*)
	# Honor a command-line override (rather unlikely)
	case "$regexec_cflags" in
	'') echo "Disabling optimization on regexec.c for gcc $gccversion" >&4
	    regexec_cflags='optimize='
	    echo "regexec_cflags='optimize=\"\"'" >> config.sh 
	    ;;
	esac
	;;
    esac
    ;;
esac
EOCBU

    cat > UU/use64bitall.cbu <<'EOCBU'
# This script UU/use64bitall.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to be maximally 64 bitty.
case "$use64bitall-$use64bitall_done" in
"$define-"|true-|[yY]*-)
	    case "`uname -r`" in
	    5.[0-6])
		cat >&4 <<EOM
Solaris `uname -r|sed -e 's/^5\./2./'` does not support 64-bit pointers.
You should upgrade to at least Solaris 2.7.
EOM
		exit 1
		;;
	    esac
	    libc='/usr/lib/sparcv9/libc.so'
	    if test ! -f $libc; then
		cat >&4 <<EOM

I do not see the 64-bit libc, $libc.
Cannot continue, aborting.

EOM
		exit 1
	    fi
	    . ./workshoplibpth.cbu
	    case "$cc -v 2>/dev/null" in
	    *gcc*)
		echo 'main() { return 0; }' > try.c
		case "`${cc:-cc} -mcpu=v9 -m64 -S try.c 2>&1 | grep 'm64 is not supported by this configuration'`" in
		*"m64 is not supported"*)
		    cat >&4 <<EOM

Full 64-bit build is not supported by this gcc configuration.
Check http://gcc.gnu.org/ for the latest news of availability
of gcc for 64-bit Sparc.

Cannot continue, aborting.

EOM
		    exit 1
		    ;;
		esac
		ccflags="$ccflags -mcpu=v9 -m64"
		if test X`getconf XBS5_LP64_OFF64_CFLAGS 2>/dev/null` != X; then
		    ccflags="$ccflags -Wa,`getconf XBS5_LP64_OFF64_CFLAGS 2>/dev/null`"
		fi
		# no changes to ld flags, as (according to man ld):
		#
   		# There is no specific option that tells ld to link 64-bit
		# objects; the class of the first object that gets processed
		# by ld determines whether it is to perform a 32-bit or a
		# 64-bit link edit.
		;;
	    *)
		ccflags="$ccflags `getconf XBS5_LP64_OFF64_CFLAGS 2>/dev/null`"
		ldflags="$ldflags `getconf XBS5_LP64_OFF64_LDFLAGS 2>/dev/null`"
		lddlflags="$lddlflags -G `getconf XBS5_LP64_OFF64_LDFLAGS 2>/dev/null`"
		;;
	    esac
	    libscheck='case "`/usr/bin/file $xxx`" in
*64-bit*|*SPARCV9*) ;;
*) xxx=/no/64-bit$xxx ;;
esac'

	    use64bitall_done=yes
	    ;;
esac
EOCBU

    # Actually, we want to run this already now, if so requested,
    # because we need to fix up things right now.
    case "$use64bitall" in
    "$define"|true|[yY]*)
	# CBUs expect to be run in UU
	cd UU; . ./use64bitall.cbu; cd ..
	;;
    esac
fi

cat > UU/uselongdouble.cbu <<'EOCBU'
# This script UU/uselongdouble.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use long doubles.
case "$uselongdouble" in
"$define"|true|[yY]*)
	if test -f /opt/SUNWspro/lib/libsunmath.so; then
		# Unfortunately libpth has already been set and
		# searched, so we need to add in everything manually.
		libpth="$libpth /opt/SUNWspro/lib"
		libs="$libs -lsunmath"
		ldflags="$ldflags -L/opt/SUNWspro/lib -R/opt/SUNWspro/lib"
		d_sqrtl=define
	else
		cat >&4 <<EOM

The Sun Workshop math library is not installed; therefore I do not
know how to do long doubles, sorry.  I'm disabling the use of long
doubles.
EOM
		uselongdouble="$undef"
	fi
	;;
esac
EOCBU

rm -f try.c try.o try a.out
