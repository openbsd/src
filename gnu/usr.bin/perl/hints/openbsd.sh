# hints/openbsd.sh
#
# hints file for OpenBSD; Todd Miller <millert@openbsd.org>
# Edited to allow Configure command-line overrides by
#  Andy Dougherty <doughera@lafayette.edu>
#
# To build with distribution paths, use:
#	./Configure -des -Dopenbsd_distribution=defined
#

# OpenBSD has a better malloc than perl...
test "$usemymalloc" || usemymalloc='n'

# malloc wrap works
case "$usemallocwrap" in
'') usemallocwrap='define' ;;
esac

# Currently, vfork(2) is not a real win over fork(2).
usevfork="$undef"

# In OpenBSD < 3.3, the setre?[ug]id() are emulated using the
# _POSIX_SAVED_IDS functionality which does not have the same
# semantics as 4.3BSD.  Starting with OpenBSD 3.3, the original
# semantics have been restored.
case "$osvers" in
[0-2].*|3.[0-2])
	d_setregid=$undef
	d_setreuid=$undef
	d_setrgid=$undef
	d_setruid=$undef
esac

# OpenBSD 5.5 on has 64 bit time_t
case "$osvers" in
[0-4].*|5.[0-4]) ;;
*)
	cppflags="$cppflags -DBIG_TIME"
	;;
esac

#
# Not all platforms support dynamic loading...
# For the case of "$openbsd_distribution", the hints file
# needs to know whether we are using dynamic loading so that
# it can set the libperl name appropriately.
# Allow command line overrides.
#
ARCH=`arch | sed 's/^OpenBSD.//'`
case "${ARCH}-${osvers}" in
alpha-2.[0-8]|mips-2.[0-8]|powerpc-2.[0-7]|m88k-[2-4].*|m88k-5.[0-2]|hppa-3.[0-5]|vax-*)
	test -z "$usedl" && usedl=$undef
	;;
*)
	test -z "$usedl" && usedl=$define
	# We use -fPIC here because -fpic is *NOT* enough for some of the
	# extensions like Tk on some OpenBSD platforms (ie: sparc)
	PICFLAG=-fPIC
	if [ -e /usr/share/mk/bsd.own.mk ]; then
		PICFLAG=`make -f /usr/share/mk/bsd.own.mk -V PICFLAG`
	fi
	cccdlflags="-DPIC ${PICFLAG} $cccdlflags"
	case "$osvers" in
	[01].*|2.[0-7]|2.[0-7].*)
		lddlflags="-Bshareable $lddlflags"
		;;
	2.[8-9]|3.0)
		ld=${cc:-cc}
		lddlflags="-shared -fPIC $lddlflags"
		;;
	*) # from 3.1 onwards
		ld=${cc:-cc}
		lddlflags="-shared ${PICFLAG} $lddlflags"
		libswanted=`echo $libswanted | sed 's/ dl / /'`
		;;
	esac

	# We need to force ld to export symbols on ELF platforms.
	# Without this, dlopen() is crippled.
	ELF=`${cc:-cc} -dM -E - </dev/null | grep __ELF__`
	test -n "$ELF" && ldflags="-Wl,-E $ldflags"
	;;
esac

#
# Tweaks for various versions of OpenBSD
#
case "$osvers" in
2.5)
	# OpenBSD 2.5 has broken odbm support
	i_dbm=$undef
	;;
esac

# OpenBSD doesn't need libcrypt but many folks keep a stub lib
# around for old NetBSD binaries.
libswanted=`echo $libswanted | sed 's/ crypt / /'`

# OpenBSD hasn't ever needed linking to libutil
libswanted=`echo $libswanted | sed 's/ util / /'`

# Configure can't figure this out non-interactively
d_suidsafe=$define

# cc is gcc so we can do better than -O
# Allow a command-line override, such as -Doptimize=-g
case "${ARCH}-${osvers}" in
hppa-3.3|m88k-2.*|m88k-3.[0-3])
   test "$optimize" || optimize='-O0'
   ;;
m88k-3.4)
   test "$optimize" || optimize='-O1'
   ;;
*)
   test "$optimize" || optimize='-O2'
   ;;
esac

#
# Unaligned access on alpha with -ftree-ter
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=59679
# More details
# https://rt.perl.org/Public/Bug/Display.html?id=120888
#
case "${ARCH}-${osvers}" in
    alpha-*)
    ccflags="-fno-tree-ter $ccflags"
    ;;
esac

# Special per-arch specific ccflags
case "${ARCH}-${osvers}" in
    vax-*)
    ccflags="-DUSE_PERL_ATOF=0 $ccflags"
    ;;
esac

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
	# any openbsd version dependencies with pthreads?
	ccflags="-pthread $ccflags"
	ldflags="-pthread $ldflags"
	case "$osvers" in
	[0-2].*|3.[0-2])
		# Change from -lc to -lc_r
		set `echo "X $libswanted " | sed 's/ c / c_r /'`
		shift
		libswanted="$*"
	;;
	esac
	case "$osvers" in
	[012].*|3.[0-6])
        	# Broken up to OpenBSD 3.6, fixed in OpenBSD 3.7
		d_getservbyname_r=$undef ;;
	esac
	;;
*)
	libswanted=`echo $libswanted | sed 's/ pthread / /'`
esac
EOCBU

# When building in the OpenBSD tree we use different paths
# This is only part of the story, the rest comes from config.over
case "$openbsd_distribution" in
''|$undef|false) ;;
*)
	# We put things in /usr, not /usr/local
	prefix='/usr'
	prefixexp='/usr'
	sysman='/usr/share/man/man1'
	libpth='/usr/lib'
	glibpth='/usr/lib'
	# Local things, however, do go in /usr/local
	siteprefix='/usr/local'
	siteprefixexp='/usr/local'
	# Ports installs non-std libs in /usr/local/lib so look there too
	locincpth=''
	loclibpth=''
	# Link perl with shared libperl
	if [ "$usedl" = "$define" -a -r $src/shlib_version ]; then
		useshrplib=true
		libperl=`. $src/shlib_version; echo libperl.so.${major}.${minor}`
	fi
	;;
esac

# openbsd has a problem regarding newlocale()
# https://marc.info/?l=openbsd-bugs&m=155364568608759&w=2
# which is being fixed.  In the meantime, forbid POSIX 2008 locales
d_newlocale="$undef"

# OpenBSD's locale support is not that complete yet
ccflags="-DNO_LOCALE_NUMERIC -DNO_LOCALE_COLLATE $ccflags"

# Seems that OpenBSD returns bogus values in _Thread_local variables in code in
# shared objects, so we need to disable it. See GH #19109
d_thread_local=undef

# end
