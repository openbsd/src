# $Id: vos.sh,v 1.0 2001-12-11 09:30:00-05 Green Exp $

# This is a hints file for Stratus VOS, using the POSIX environment
# in VOS 14.4.0 and higher.
#
# VOS POSIX is based on POSIX.1-1996.  It ships with gcc as the standard
# compiler.
#
# Paul Green (Paul.Green@stratus.com)

# C compiler and default options.
cc=gcc
ccflags="-D_SVID_SOURCE -D_POSIX_C_SOURCE=199509L"

# Make command.
make="/system/gnu_library/bin/gmake"
# indented to not put it into config.sh
  _make="/system/gnu_library/bin/gmake"

# Architecture name
archname="hppa1.1"

# Executable suffix.
# No, this is not a typo.  The ".pm" really is the native
# executable suffix in VOS.  Talk about cosmic resonance.
_exe=".pm"

# Object library paths.
loclibpth="/system/stcp/object_library"
loclibpth="$loclibpth /system/stcp/object_library/common"
loclibpth="$loclibpth /system/stcp/object_library/net"
loclibpth="$loclibpth /system/stcp/object_library/socket"
loclibpth="$loclibpth /system/posix_object_library/sysv"
loclibpth="$loclibpth /system/posix_object_library"
loclibpth="$loclibpth /system/c_object_library"
loclibpth="$loclibpth /system/object_library"
glibpth="$loclibpth"

# Include library paths
locincpth="/system/stcp/include_library"
locincpth="$locincpth /system/include_library/sysv"
usrinc="/system/include_library"

# Where to install perl5.
prefix=/system/ported/perl5

# Linker is gcc.
ld="gcc"

# No shared libraries.
so="none"

# Don't use nm.
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

# VOS returns a constant 1 for st_nlink when stat'ing a
# directory. Therefore, we must set this variable to stop
# File::Find using the link count to determine whether there are
# subdirectories to be searched.
dont_use_nlink=define

# Tell Configure where to find the hosts file.
hostcat="cat /system/stcp/hosts"

# VOS does not have socketpair() but we supply one in vos.c
d_sockpair="define"

# Once we have the compiler flags defined, Configure will
# execute the following call-back script. See hints/README.hints
# for details.
cat > UU/cc.cbu <<'EOCBU'
# This script UU/cc.cbu will get 'called-back' by Configure after it
# has prompted the user for the C compiler to use.

# Compile and run the a test case to see if bug gnu_g++-220 is
# present. If so, lower the optimization level when compiling
# pp_pack.c.  This works around a bug in unpack.

echo " "
echo "Testing whether bug gnu_g++-220 is fixed in your compiler..."

# Try compiling the test case.
if $cc -o t001 -O $ccflags $ldflags ../hints/t001.c; then
	gccbug=`$run ./t001`
	if [ "X$gccversion" = "X" ]; then
		# Done too late in Configure if hinted
		gccversion=`$cc --version | sed 's/.*(GCC) *//'`
	fi
	case "$gccbug" in
	*fails*)	cat >&4 <<EOF
This C compiler ($gccversion) is known to have optimizer
problems when compiling pp_pack.c.  The Stratus bug number
for this problem is gnu_g++-220.

Disabling optimization for pp_pack.c.
EOF
			case "$pp_pack_cflags" in
			'')	pp_pack_cflags='optimize='
				echo "pp_pack_cflags='optimize=\"\"'" >> config.sh ;;
			*)  echo "You specified pp_pack_cflags yourself, so we'll go with your value." >&4 ;;
			esac
		;;
	*)	echo "Your compiler is ok." >&4
		;;
	esac
else
	echo " "
	echo "*** WHOA THERE!!! ***" >&4
	echo "    Your C compiler \"$cc\" doesn't seem to be working!" >&4
	case "$knowitall" in
	'')
		echo "    You'd better start hunting for one and let me know about it." >&4
		exit 1
		;;
	esac
fi

$rm -f t001$_o t001$_exe t001.kp
EOCBU


# VOS 14.7 has minimal support for dynamic linking. Too minimal for perl.
usedl="undef"
