# hints/netbsd.sh
#
# Please check with packages@netbsd.org before making modifications
# to this file.

case "$archname" in
'')
    archname=`uname -m`-${osname}
    ;;
esac

# NetBSD keeps dynamic loading dl*() functions in /usr/lib/crt0.o,
# so Configure doesn't find them (unless you abandon the nm scan).
# Also, NetBSD 0.9a was the first release to introduce shared
# libraries.
#
case "$osvers" in
0.9|0.8*)
	usedl="$undef"
	;;
*)
	case `uname -m` in
	pmax)
		# NetBSD 1.3 and 1.3.1 on pmax shipped an `old' ld.so,
		# which will not work.
		case "$osvers" in
		1.3|1.3.1)
			d_dlopen=$undef
			;;
		esac
		;;
	esac
	if test -f /usr/libexec/ld.elf_so; then
		# ELF
		d_dlopen=$define
		d_dlerror=$define
		cccdlflags="-DPIC -fPIC $cccdlflags"
		lddlflags="--whole-archive -shared $lddlflags"
		rpathflag="-Wl,-rpath,"
		#
		# Include the whole libgcc.a into the perl executable so
		# that certain symbols needed by loadable modules built as
		# C++ objects (__eh_alloc, __pure_virtual, etc.) will always
		# be defined.
		#
		# XXX This should be obsoleted by gcc-3.0.
		#
		ccdlflags="-Wl,-whole-archive -lgcc -Wl,-no-whole-archive \
			-Wl,-E $ccdlflags"
	elif test -f /usr/libexec/ld.so; then
		# a.out
		d_dlopen=$define
		d_dlerror=$define
		cccdlflags="-DPIC -fPIC $cccdlflags"
		lddlflags="-Bshareable $lddlflags"
		rpathflag="-R"
	else
		d_dlopen=$undef
		rpathflag=
	fi
	;;
esac

# netbsd had these but they don't really work as advertised, in the
# versions listed below.  if they are defined, then there isn't a
# way to make perl call setuid() or setgid().  if they aren't, then
# ($<, $>) = ($u, $u); will work (same for $(/$)).  this is because
# you can not change the real userid of a process under 4.4BSD.
# netbsd fixed this in 1.3.2.
case "$osvers" in
0.9*|1.[012]*|1.3|1.3.1)
	d_setregid="$undef"
	d_setreuid="$undef"
	;;
esac

# These are obsolete in any netbsd.
d_setrgid="$undef"
d_setruid="$undef"

# there's no problem with vfork.
usevfork=true

# This is there but in machine/ieeefp_h.
ieeefp_h="define"

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads. 
cat > UU/usethreads.cbu <<'EOCBU' 
case "$usethreads" in 
$define|true|[yY]*) 
	lpthread=
	for xxx in pthread; do
		for yyy in $loclibpth $plibpth $glibpth dummy; do
			zzz=$yyy/lib$xxx.a
			if test -f "$zzz"; then
				lpthread=$xxx
				break;
			fi
			zzz=$yyy/lib$xxx.so
			if test -f "$zzz"; then
				lpthread=$xxx
				break;
			fi
			zzz=`ls $yyy/lib$xxx.so.* 2>/dev/null`
			if test "X$zzz" != X; then
				lpthread=$xxx
				break;
			fi
		done
		if test "X$lpthread" != X; then
			break;
		fi
	done
	if test "X$lpthread" != X; then
		# Add -lpthread. 
		libswanted="$libswanted $lpthread" 
		# There is no libc_r as of NetBSD 1.5.2, so no c -> c_r.
		# This will be revisited when NetBSD gains a native pthreads
		# implementation.
        else 
		echo "$0: No POSIX threads library (-lpthread) found.  " \
		     "You may want to install GNU pth.  Aborting." >&4 
		exit 1 
        fi
	unset lpthread
        ;; 
esac 
EOCBU

# Set sensible defaults for NetBSD: look for local software in
# /usr/pkg (NetBSD Packages Collection) and in /usr/local.
#
loclibpth="/usr/pkg/lib /usr/local/lib"
locincpth="/usr/pkg/include /usr/local/include"
case "$rpathflag" in
'')
	ldflags=
	;;
*)
	ldflags=
	for yyy in $loclibpth; do
		ldflags="$ldflags $rpathflag$yyy"
	done
	;;
esac
