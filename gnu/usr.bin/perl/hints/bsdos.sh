# hints/bsdos.sh
#
# hints file for BSD/OS 2.x (adapted from bsd386.sh)
# Original by Neil Bowers <neilb@khoros.unm.edu>
#     Tue Oct  4 12:01:34 EDT 1994
# Updated by Tony Sanders <sanders@bsdi.com>
#     Mon Nov 27 17:25:51 CST 1995
#
# You can override the compiler and loader on the Configure command line:
#     ./Configure -Dcc=shlicc2 -Dld=shlicc2

# filename extension for shared library objects
so='o'

# Don't use this for Perl 5.002, which needs parallel sig_name and sig_num lists
#sig_name='ZERO HUP INT QUIT ILL TRAP IOT EMT FPE KILL BUS SEGV SYS PIPE ALRM TERM URG STOP TSTP CONT CHLD TTIN TTOU IO XCPU XFSZ VTALRM PROF WINCH INFO USR1 USR2 '
signal_t='void'
d_voidsig='define'
d_dosuid='define'

# we don't want to use -lnm, since exp() is busted (in 1.1 anyway)
set `echo X "$libswanted "| sed -e 's/ nm / /'`
shift
libswanted="$*"

# BSD/OS X libraries are in their own tree
glibpth="$glibpth /usr/X11/lib"
ldflags="$ldflags -L/usr/X11/lib"

# Avoid telldir prototype conflict in pp_sys.c
pp_sys_cflags='ccflags="$ccflags -DHAS_TELLDIR_PROTOTYPE"'

case "$bsdos_distribution" in
defined)
	d_portable='no'
	prefix='/usr/contrib'
	man3dir='/usr/contrib/man/man3'
	;;
esac

case "$osvers" in
1.0*)
	# Avoid problems with HUGE_VAL in POSIX in 1.0's cc.
	POSIX_cflags='ccflags="$ccflags -UHUGE_VAL"' 
	;;
1.1*)
	# Use gcc2
	case "$cc" in
	'')	cc='gcc2' ;;
	esac
	;;
2.0*)
	# default to GCC 2.X w/shared libraries
	case "$cc" in
	'')	cc='shlicc2' ;;
	esac

	# default ld to shared library linker
	case "$ld" in
	'')	ld='shlicc2' ;;
	esac

	# setre?[ug]id() have been replaced by the _POSIX_SAVED_IDS stuff
	# in 4.4BSD-based systems (including BSD/OS 2.0 and later).
	# See http://www.bsdi.com/bsdi-man?setuid(2)
	d_setregid='undef'
	d_setreuid='undef'
	d_setrgid='undef'
	d_setruid='undef'
	;;
2.1*)
	# Use 2.1's shlicc2 for dynamic linking
	# Since cc -o is linking, use it for compiling too.
	# I'm not sure whether Configure is careful about
	# distinguishing between the two.

	case "$cc" in
	'')	cc='shlicc2'
		cccdlflags=' ' ;; # Avoid the dreaded -fpic
	esac

	# Link with shared libraries in 2.1
	# Turns out that shlicc2 will automatically use the
	# shared libs, so don't explicitly specify -lc_s.2.1.*
	case "$ld" in
	'')	ld='shlicc2'
		lddlflags='-r' ;; # this one is necessary
	esac

	# setre?[ug]id() have been replaced by the _POSIX_SAVED_IDS  stuff
	# in 4.4BSD-based systems (including BSD/OS 2.0 and later).
	# See http://www.bsdi.com/bsdi-man?setuid(2)
	# This stuff may or may not be right, but it works.
	d_setregid='undef'
	d_setreuid='undef'
	d_setrgid='undef'
	d_setruid='undef'
	;;
esac
