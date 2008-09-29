# hints/dragonfly.sh
#
# This file is mostly copied from hints/freebsd.sh with the OS version
# information taken out and only the FreeBSD-4 information intact.
# Please check with Todd Willey <xtoddx@gmail.com> before making
# modifications to this file. See http://www.dragonflybsd.org/main/

case "$osvers" in
*)  usevfork='true'
    case "$usemymalloc" in
	"") usemymalloc='n'
	    ;;
    esac
    libswanted=`echo $libswanted | sed 's/ malloc / /'`
    ;;
esac

# Dynamic Loading flags have not changed much, so they are separated
# out here to avoid duplicating them everywhere.
case "$osvers" in
*)  objformat=`/usr/bin/objformat`
    if [ x$objformat = xelf ]; then
	libpth="/usr/lib /usr/local/lib"
	glibpth="/usr/lib /usr/local/lib"
	ldflags="-Wl,-E "
	lddlflags="-shared "
    else
	if [ -e /usr/lib/aout ]; then
	    libpth="/usr/lib/aout /usr/local/lib /usr/lib"
	    glibpth="/usr/lib/aout /usr/local/lib /usr/lib"
	fi
	lddlflags='-Bshareable'
    fi
    cccdlflags='-DPIC -fPIC'
    ;;
esac

case "$osvers" in
*)  ccflags="${ccflags} -DHAS_FPSETMASK -DHAS_FLOATINGPOINT_H"
    if /usr/bin/file -L /usr/lib/libc.so | /usr/bin/grep -vq "not stripped" ; then
	usenm=false
    fi
    ;;
esac

cat <<'EOM' >&4

Some users have reported that Configure halts when testing for
the O_NONBLOCK symbol with a syntax error.  This is apparently a
sh error.  Rerunning Configure with ksh apparently fixes the
problem.  Try
       ksh Configure [your options]

EOM

# From: Anton Berezin <tobez@plab.ku.dk>
# To: perl5-porters@perl.org
# Subject: [PATCH 5.005_54] Configure - hints/freebsd.sh signal handler type
# Date: 30 Nov 1998 19:46:24 +0100
# Message-ID: <864srhhvcv.fsf@lion.plab.ku.dk>

signal_t='void'
d_voidsig='define'

# This script UU/usethreads.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
    lc_r=`/sbin/ldconfig -r|grep ':-lc_r'|awk '{print $NF}'|sed -n '$p'`
    case "$osvers" in
    *)  if [ ! -r "$lc_r" ]; then
	    cat <<EOM >&4
POSIX threads should be supported by FreeBSD $osvers --
but your system is missing the shared libc_r.
(/sbin/ldconfig -r doesn't find any).

Consider using the latest STABLE release.
EOM
	    exit 1
	fi
	case "$osvers" in
	    *)  ldflags="-pthread $ldflags"
		;;
	esac
	# Both in 4.x and 5.x gethostbyaddr_r exists but
	# it is "Temporary function, not threadsafe"...
	# Presumably earlier it didn't even exist.
	d_gethostbyaddr_r="undef"
	d_gethostbyaddr_r_proto="0"
	;;
    esac

    set `echo X "$libswanted "| sed -e 's/ c / c_r /'`
    shift
    libswanted="$*"
    # Configure will probably pick the wrong libc to use for nm scan.
    # The safest quick-fix is just to not use nm at all...
    usenm=false

    unset lc_r

    # Even with the malloc mutexes the Perl malloc does not
    # seem to be threadsafe in FreeBSD?
    case "$usemymalloc" in
    '')  usemymalloc=n ;;
    esac
esac
EOCBU

# malloc wrap works
case "$usemallocwrap" in
'') usemallocwrap='define' ;;
esac
