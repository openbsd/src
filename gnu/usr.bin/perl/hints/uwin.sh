#
# hint file for U/WIN (UNIX for Windows 95/NT)
#
# created for U/WIN version 1.55
# running under Windows NT 4.0 SP 3
# using MSVC++ 5.0 for the compiler
#
# created by Joe Buehler (jbuehler@hekimian.com)
#
# for information about U/WIN see www.gtlinc.com
#

#ccflags=-D_BSDCOMPAT
# confusion in Configure over preprocessor
cppstdin=`pwd`/cppstdin
cpprun=`pwd`/cppstdin
# pwd.h confuses Configure
d_pwcomment=undef
d_pwgecos=define
# work around case-insensitive file names
firstmakefile=GNUmakefile
# avoid compilation error
i_utime=undef
# compile/link flags
ldflags=-g
optimize=-g
static_ext="B Data/Dumper Digest/MD5 Errno Fcntl Filter::Util::Call IO IPC/SysV MIME::Base64 Opcode PerlIO::scalar POSIX SDBM_File Socket Storable Unicode::Normalize attrs re"
#static_ext=none
# dynamic loading needs work
usedl=undef
# perl malloc will not work
usemymalloc=n
# cannot use nm
usenm=undef
# vfork() is buggy (as of 1.55 anyway)
usevfork=false
