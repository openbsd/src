##
# Rhapsody (Mac OS X Server) hints
# Wilfredo Sanchez <wsanchez@wsanchez.net>
##

##
# Paths
##

# Configure hasn't figured out the version number yet.  Bummer.
perl_revision=`awk '/define[   ]+PERL_REVISION/ {print $3}' $src/patchlevel.h`
perl_version=`awk '/define[    ]+PERL_VERSION/ {print $3}' $src/patchlevel.h`
perl_subversion=`awk '/define[         ]+PERL_SUBVERSION/ {print $3}' $src/patchlevel.h`
version="${perl_revision}.${perl_version}.${perl_subversion}"

# BSD paths
case "$prefix" in
  '')
    # Default install; use non-system directories
    prefix='/usr/local'; # Built-in perl uses /usr
    siteprefix='/usr/local';
    vendorprefix='/usr'; usevendorprefix='define';

    # Where to put modules.
    sitelib="/Local/Library/Perl/${version}"; # FIXME: Want "/Network/Perl/${version}" also
    vendorlib="/System/Library/Perl/${version}"; # Apple-supplied modules
    ;;

  '/usr')
    # We are building/replacing the built-in perl
    siteprefix='/usr/local';
    vendorprefix='/usr/local'; usevendorprefix='define';

    # Where to put modules.
    sitelib="/Local/Library/Perl/${version}"; # FIXME: Want "/Network/Perl/${version}" also
    vendorlib="/System/Library/Perl/${version}"; # Apple-supplied modules
    ;;
esac

##
# Tool chain settings
##

# Since we can build fat, the archname doesn't need the processor type
archname='rhapsody';

# nm works.
usenm='true';
  
# Libc is in libsystem.
libc='/System/Library/Frameworks/System.framework/System';

# Optimize.
optimize='-O3';

# -pipe: makes compilation go faster.
# -fno-common because common symbols are not allowed in MH_DYLIB
ccflags="${ccflags} -pipe -fno-common"

# Unverified whether this is necessary on Rhapsody, but the test shouldn't hurt.
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
case "$(grep '^#define INT32_MIN' /usr/include/stdint.h)" in
  *-2147483648) ccflags="${ccflags} -DINT32_MIN_BROKEN -DINT64_MIN_BROKEN" ;;
esac

# cpp-precomp is problematic.
cppflags='${cppflags} -traditional-cpp';

# This is necessary because perl's build system doesn't
# apply cppflags to cc compile lines as it should.
ccflags="${ccflags} ${cppflags}"

# Shared library extension is .dylib.
# Bundle extension is .bundle.
ld='cc';
so='dylib';
dlext='bundle';
dlsrc='dl_dyld.xs';
usedl='define';
cccdlflags='';
lddlflags="${ldflags} -bundle -undefined suppress";
ldlibpthname='DYLD_LIBRARY_PATH';
useshrplib='true';

##
# System libraries
##
  
# vfork works
usevfork='true';

# our malloc works (but allow users to override)
case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

#
# The libraries are not threadsafe in Rhapsody
#
# Fix when Apple fixes libc.
#
case "$usethreads$useithreads" in
  *define*)
    cat <<EOM >&4



*** Warning, there might be problems with your libraries with
*** regards to threading.  The test ext/threads/t/libc.t is likely
*** to fail.

EOM
    ;;
esac

##
# Build process
##

# Case-insensitive filesystems don't get along with Makefile and
# makefile in the same place.  Since Darwin uses GNU make, this dodges
# the problem.
firstmakefile=GNUmakefile;
