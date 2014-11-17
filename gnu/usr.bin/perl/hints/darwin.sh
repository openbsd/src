##
# Darwin (Mac OS) hints
# Wilfredo Sanchez <wsanchez@wsanchez.net>
##

##
# Paths
##

# Configure hasn't figured out the version number yet.  Bummer.
perl_revision=`awk '/define[ 	]+PERL_REVISION/ {print $3}' $src/patchlevel.h`
perl_version=`awk '/define[ 	]+PERL_VERSION/ {print $3}' $src/patchlevel.h`
perl_subversion=`awk '/define[ 	]+PERL_SUBVERSION/ {print $3}' $src/patchlevel.h`
version="${perl_revision}.${perl_version}.${perl_subversion}"

# Pretend that Darwin doesn't know about those system calls in Tiger
# (10.4/darwin 8) and earlier [perl #24122]
case "$osvers" in
[1-8].*)
    d_setregid='undef'
    d_setreuid='undef'
    d_setrgid='undef'
    d_setruid='undef'
    ;;
esac

# This was previously used in all but causes three cases
# (no -Ddprefix=, -Dprefix=/usr, -Dprefix=/some/thing/else)
# but that caused too much grief.
# vendorlib="/System/Library/Perl/${version}"; # Apple-supplied modules

# BSD paths
case "$prefix" in
'')	# Default install; use non-system directories
	prefix='/usr/local';
	siteprefix='/usr/local';
	;;
'/usr')	# We are building/replacing the built-in perl
	prefix='/';
	installprefix='/';
	bin='/usr/bin';
	siteprefix='/usr/local';
	# We don't want /usr/bin/HEAD issues.
	sitebin='/usr/local/bin';
	sitescript='/usr/local/bin';
	installusrbinperl='define'; # You knew what you were doing.
	privlib="/System/Library/Perl/${version}";
	sitelib="/Library/Perl/${version}";
	vendorprefix='/';
	usevendorprefix='define';
	vendorbin='/usr/bin';
	vendorscript='/usr/bin';
	vendorlib="/Network/Library/Perl/${version}";
	# 4BSD uses ${prefix}/share/man, not ${prefix}/man.
	man1dir='/usr/share/man/man1';
	man3dir='/usr/share/man/man3';
	# But users' installs shouldn't touch the system man pages.
	# Transient obsoleted style.
	siteman1='/usr/local/share/man/man1';
	siteman3='/usr/local/share/man/man3';
	# New style.
	siteman1dir='/usr/local/share/man/man1';
	siteman3dir='/usr/local/share/man/man3';
	;;
  *)	# Anything else; use non-system directories, use Configure defaults
	;;
esac

##
# Tool chain settings
##

# Since we can build fat, the archname doesn't need the processor type
archname='darwin';

# nm isn't known to work after Snow Leopard and XCode 4; testing with OS X 10.5
# and Xcode 3 shows a working nm, but pretending it doesn't work produces no
# problems.
usenm='false';

case "$optimize" in
'')
#    Optimizing for size also mean less resident memory usage on the part
# of Perl.  Apple asserts that this is a more important optimization than
# saving on CPU cycles.  Given that memory speed has not increased at
# pace with CPU speed over time (on any platform), this is probably a
# reasonable assertion.
if [ -z "${optimize}" ]; then
  case "`${cc:-gcc} -v 2>&1`" in
    *"gcc version 3."*) optimize='-Os' ;;
    *) optimize='-O3' ;;
  esac
else
  optimize='-O3'
fi
;;
esac

# -fno-common because common symbols are not allowed in MH_DYLIB
# -DPERL_DARWIN: apparently the __APPLE__ is not sanctioned by Apple
# as the way to differentiate Mac OS X.  (The official line is that
# *no* cpp symbol does differentiate Mac OS X.)
ccflags="${ccflags} -fno-common -DPERL_DARWIN"

# At least on Darwin 1.3.x:
#
# # define INT32_MIN -2147483648
# int main () {
#  double a = INT32_MIN;
#  printf ("INT32_MIN=%g\n", a);
#  return 0;
# }
# will output:
# INT32_MIN=2.14748e+09
# Note that the INT32_MIN has become positive.
# INT32_MIN is set in /usr/include/stdint.h by:
# #define INT32_MIN        -2147483648
# which seems to break the gcc.  Defining INT32_MIN as (-2147483647-1)
# seems to work.  INT64_MIN seems to be similarly broken.
# -- Nicholas Clark, Ken Williams, and Edward Moy
#
# This seems to have been fixed since at least Mac OS X 10.1.3,
# stdint.h defining INT32_MIN as (-INT32_MAX-1)
# -- Edward Moy
#
case "$(grep '^#define INT32_MIN' /usr/include/stdint.h)" in
  *-2147483648) ccflags="${ccflags} -DINT32_MIN_BROKEN -DINT64_MIN_BROKEN" ;;
esac

# Avoid Apple's cpp precompiler, better for extensions
if [ "X`echo | ${cc} -no-cpp-precomp -E - 2>&1 >/dev/null`" = "X" ]; then
    cppflags="${cppflags} -no-cpp-precomp"

    # This is necessary because perl's build system doesn't
    # apply cppflags to cc compile lines as it should.
    ccflags="${ccflags} ${cppflags}"
fi

# Known optimizer problems.
case "`cc -v 2>&1`" in
  *"3.1 20020105"*) toke_cflags='optimize=""' ;;
esac

# Shared library extension is .dylib.
# Bundle extension is .bundle.
so='dylib';
dlext='bundle';
usedl='define';

# 10.4 can use dlopen.
# 10.4 broke poll().
case "$osvers" in
[1-7].*)
    dlsrc='dl_dyld.xs';
    ;;
*)
    dlsrc='dl_dlopen.xs';
    d_poll='undef';
    i_poll='undef';
    ;;
esac

case "$ccdlflags" in		# If passed in from command line, presume user knows best
'')
   cccdlflags=' '; # space, not empty, because otherwise we get -fpic
;;
esac

# Allow the user to override ld, but modify it as necessary below
case "$ld" in
    '') ld='cc';;
esac

# Perl bundles do not expect two-level namespace, added in Darwin 1.4.
# But starting from perl 5.8.1/Darwin 7 the default is the two-level.
case "$osvers" in
1.[0-3].*)
   lddlflags="${ldflags} -bundle -undefined suppress"
   ;;
1.*)
   ldflags="${ldflags} -flat_namespace"
   lddlflags="${ldflags} -bundle -undefined suppress"
   ;;
[2-6].*)
   ldflags="${ldflags} -flat_namespace"
   lddlflags="${ldflags} -bundle -undefined suppress"
   ;;
*) 
   lddlflags="${ldflags} -bundle -undefined dynamic_lookup"
   case "$ld" in
       *MACOSX_DEVELOPMENT_TARGET*) ;;
       *) ld="env MACOSX_DEPLOYMENT_TARGET=10.3 ${ld}" ;;
   esac
   ;;
esac
ldlibpthname='DYLD_LIBRARY_PATH';

# useshrplib=true results in much slower startup times.
# 'false' is the default value.  Use Configure -Duseshrplib to override.

cat > UU/archname.cbu <<'EOCBU'
# This script UU/archname.cbu will get 'called-back' by Configure 
# after it has otherwise determined the architecture name.
case "$ldflags" in
*"-flat_namespace"*) ;; # Backward compat, be flat.
# If we are using two-level namespace, we will munge the archname to show it.
*) archname="${archname}-2level" ;;
esac
EOCBU

# 64-bit addressing support. Currently strictly experimental. DFD 2005-06-06
case "$use64bitall" in
$define|true|[yY]*)
case "$osvers" in
[1-7].*)
     cat <<EOM >&4



*** 64-bit addressing is not supported for Mac OS X versions
*** below 10.4 ("Tiger") or Darwin versions below 8. Please try
*** again without -Duse64bitall. (-Duse64bitint will work, however.)

EOM
     exit 1
  ;;
*)
    case "$osvers" in
    8.*)
        cat <<EOM >&4



*** Perl 64-bit addressing support is experimental for Mac OS X
*** 10.4 ("Tiger") and Darwin version 8. System V IPC is disabled
*** due to problems with the 64-bit versions of msgctl, semctl,
*** and shmctl. You should also expect the following test failures:
***
***    ext/threads-shared/t/wait (threaded builds only)

EOM

        [ "$d_msgctl" ] || d_msgctl='undef'
        [ "$d_semctl" ] || d_semctl='undef'
        [ "$d_shmctl" ] || d_shmctl='undef'
    ;;
    esac

    case `uname -p` in 
    powerpc) arch=ppc64 ;;
    i386) arch=x86_64 ;;
    *) cat <<EOM >&4

*** Don't recognize processor, can't specify 64 bit compilation.

EOM
    ;;
    esac
    for var in ccflags cppflags ld ldflags
    do
       eval $var="\$${var}\ -arch\ $arch"
    done

    ;;
esac
;;
esac

##
# System libraries
##

# vfork works
usevfork='true';

# malloc wrap works
case "$usemallocwrap" in
'') usemallocwrap='define' ;;
esac

# our malloc works (but allow users to override)
case "$usemymalloc" in
'') usemymalloc='n' ;;
esac
# However sbrk() returns -1 (failure) somewhere in lib/unicore/mktables at
# around 14M, so we need to use system malloc() as our sbrk()
malloc_cflags='ccflags="-DUSE_PERL_SBRK -DPERL_SBRK_VIA_MALLOC $ccflags"'

# Locales aren't feeling well.
LC_ALL=C; export LC_ALL;
LANG=C; export LANG;

#
# The libraries are not threadsafe as of OS X 10.1.
#
# Fix when Apple fixes libc.
#
case "$usethreads$useithreads" in
  *define*)
  case "$osvers" in
    [12345].*)     cat <<EOM >&4



*** Warning, there might be problems with your libraries with
*** regards to threading.  The test ext/threads/t/libc.t is likely
*** to fail.

EOM
    ;;
    *) usereentrant='define';;
  esac

esac

# Fink can install a GDBM library that claims to have the ODBM interfaces
# but Perl dynaloader cannot for some reason use that library.  We don't
# really need ODBM_FIle, though, so let's just hint ODBM away.
i_dbm=undef;

# Configure doesn't detect ranlib on Tiger properly.
# NeilW says this should be acceptable on all darwin versions.
ranlib='ranlib'

##
# Build process
##

# Case-insensitive filesystems don't get along with Makefile and
# makefile in the same place.  Since Darwin uses GNU make, this dodges
# the problem.
firstmakefile=GNUmakefile;
