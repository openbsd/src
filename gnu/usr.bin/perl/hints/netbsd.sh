# hints/netbsd.sh
#
# talk to mrg@eterna.com.au if you want to change this file.
#
# netbsd keeps dynamic loading dl*() functions in /usr/lib/crt0.o,
# so Configure doesn't find them (unless you abandon the nm scan).
# this should be *just* 0.9 below as netbsd 0.9a was the first to
# introduce shared libraries.
case "$osvers" in
0.9|0.8*)
	usedl="$undef"
	;;
*)	d_dlopen=$define
	d_dlerror=$define
# we use -fPIC here because -fpic is *NOT* enough for some of the
# extensions like Tk on some netbsd platforms (the sparc is one)
	cccdlflags="-DPIC -fPIC $cccdlflags"
	lddlflags="-Bforcearchive -Bshareable $lddlflags"
# netbsd has these but they don't really work as advertised.  if they
# are defined, then there isn't a way to make perl call setuid() or
# setgid().  if they aren't, then ($<, $>) = ($u, $u); will work (same
# for $(/$)).  this is because you can not change the real userid of
# a process under 4.4BSD.
	d_setregid="$undef"
	d_setreuid="$undef"
	d_setrgid="$undef"
	d_setruid="$undef"
	;;
esac

# Avoid telldir prototype conflict in pp_sys.c  (NetBSD uses const DIR *)
# Configure should test for this.  Volunteers?
pp_sys_cflags='ccflags="$ccflags -DHAS_TELLDIR_PROTOTYPE"'

case "$archname" in
'')
    archname=`uname -m`-${osname}
    ;;
esac
