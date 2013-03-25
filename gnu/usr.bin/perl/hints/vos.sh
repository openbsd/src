# This is a hints file for Stratus OpenVOS, using the POSIX environment
# found in VOS 17.1.0 and higher.
#
# OpenVOS POSIX is based on POSIX.1-1996 and contains elements of
# POSIX.1-2001.  It ships with gcc as the standard compiler.
#
# Paul Green (Paul.Green@stratus.com)

# C compiler and default options.
cc=gcc
ccflags="-D_XOPEN_SOURCE=700 -D_VOS_EXTENDED_NAMES"
ccdlflags="-Wl,-rpath,$shrpdir"
cccdlflags="-fPIC"

# Make command.
make="/system/gnu_library/bin/gmake"
# indented to not put it into config.sh
  _make="/system/gnu_library/bin/gmake"

# Check for the minimum acceptable release of OpenVOS (17.1.0).
if test `uname -r | sed -e 's/OpenVOS Release //' -e 's/VOS Release //'` \< "17.1.0"; then
cat >&4 <<EOF
***
*** This version of Perl 5 must be built on OpenVOS Release 17.1.0 or later.
***
EOF
exit 1
fi

# Always X86
archname=`uname -m`

# Executable suffix.
# No, this is not a typo.  The ".pm" really is the native
# executable suffix in VOS.  Talk about cosmic resonance.
_exe=".pm"

# Object library paths.
loclibpth="$loclibpth /system/posix_object_library"
loclibpth="$loclibpth /system/c_object_library"
loclibpth="$loclibpth /system/object_library"
glibpth="$loclibpth"

# Include library paths
locincpth=""
usrinc="/system/include_library"

# Where to install perl5.
prefix=/system/ported/perl5

# Linker is gcc.
ld="gcc"
lddlflags="-shared"

# Shared libraries!
so="so"

# Build libperl.so
useshrplib="true"

# Don't use nm. The VOS copy of libc.a is empty.
usenm="n"

# Make the default be no large file support.
uselargefiles="n"

# Don't use malloc that comes with perl.
usemymalloc="n"

# Make bison the default compiler-compiler.
yacc="/system/gnu_library/bin/bison"

# VOS doesn't have (or need) a pager, but perl needs one.
pager="/system/gnu_library/bin/cat.pm"

# VOS has a bug that causes _exit() to flush all files.
# This confuses the tests.  Make 'em happy here.
fflushNULL=define

# VOS has a link() function but it is a dummy.
d_link="undef"

# VOS does not have truncate() but we supply one in vos.c
d_truncate="define"
archobjs="vos.o"

# Help gmake find vos.c
test -h vos.c || ln -s vos/vos.c vos.c

# Tell Configure where to find the hosts file.
hostcat="cat /system/stcp/hosts"
