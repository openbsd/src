#
# The lines starting with #b that follow are the uwin.sh
# file from Joe Buehler.  Some lines are, themselves,
# commented out.  If an uncommented line disappears
# altogether, it means it didn't seem to be needed any more,
# to get a proper build on the following machine.
#    UWIN-NT korn-7200 3.19-5.0 2195 i686
# But maybe they'll be useful to others on different machines.

#b # hint file for U/WIN (UNIX for Windows 95/NT)
#b #
#b # created for U/WIN version 1.55
#b # running under Windows NT 4.0 SP 3
#b # using MSVC++ 5.0 for the compiler
#b #
#b # created by Joe Buehler (jbuehler@hekimian.com)
#b #
#b # for information about U/WIN see www.gtlinc.com
#b #
#b 
#b #ccflags=-D_BSDCOMPAT
#b # confusion in Configure over preprocessor
#b cppstdin=`pwd`/cppstdin
#b cpprun=`pwd`/cppstdin
#b # pwd.h confuses Configure
#b d_pwcomment=undef
#b d_pwgecos=define
#b # work around case-insensitive file names
#b firstmakefile=GNUmakefile
#b # avoid compilation error
#b i_utime=undef
#b # compile/link flags
#b ldflags=-g
#b optimize=-g
#b static_ext="B Data/Dumper Digest/MD5 Errno Fcntl Filter::Util::Call IO IPC/SysV MIME::Base64 Opcode PerlIO::scalar POSIX SDBM_File Socket Storable Unicode::Collate Unicode::Normalize attributes re"
#b #static_ext=none
#b # dynamic loading needs work
#b usedl=undef
#b # perl malloc will not work
#b usemymalloc=n
#b # cannot use nm
#b usenm=undef
#b # vfork() is buggy (as of 1.55 anyway)
#b usevfork=false

# __UWIN__ added so it could be used in ext/POSIX/POSIX.xs
# to protect against either tzname definition.  According to Dave Korn

#dgk gcc on uwin also predefined _UWIN as does the digital mars compiler.
#dgk 
#dgk Only ncc does not define _UWIN and this is intentional.  ncc is used
#dgk to build binaries that do not require the uwin runtime.
#dgk This could be used for building a native win32 perl using unix
#dgk makefiles.  However, in this case you don't wan't _UWIN defined.
#dgk 
#dgk I have used _UWIN everywhere else in any uwin specific changes.
#dgk and _WIN32 on windows specific changes, and _MSVC on any compiler
#dgk Visual C specific changes.  We also define _WINIX for any unix
#dgk on windows implementation so that _UWIN or __cygwin__ imply _WINIX.

# I left __UWIN__ as is, since I had already filed a patch,
# and it might be useful to distinguish perl-specific tweaks
# from generic uwin ones.

ccflags="$ccflags -D__UWIN__"

# This from Dave Korn
#dgk Windows splits shared libraries into two parts; the part used
#dgk for linking and the part that is used for running.
#dgk Given a library foo, then the part you link with is named
#dgk	foo.lib
#dgk and is in the lib directory.  The part that you run with
#dgk is named
#dgk	foo.dll or foo#.dll
#dgk and is in the bin directory.  This way when you set you PATH
#dgk variable, it automatically does the library search.
#dgk
#dgk Static libraries use libfoo.a.
#dgk By the way if you specify -lfoo, then it will first look for foo.lib
#dgk and then libfoo.a.  If you specify +lfoo, it will only look for
#dgk static versions of the library.

# So we use .lib as the extension, and put -lm in, because it is a .a
# This probably accounts for the comment about dynamic libraries
# needing work, and indeed, the build failed if I didn't undef it.

lib_ext=".lib"
libs="-lm"
so=dll
# dynamic loading still needs work
usedl=undef

# confusion in Configure over preprocessor
cppstdin=`pwd`/cppstdin
cpprun=`pwd`/cppstdin

# lest it default to .exe, and then there's no perl in the test directory,
# t, just a perl.exe, and make test promptly dies.  _exe gets set to .exe
# by Configure (on 5/23/2003) if exe_ext is merely null, so clean it out, too.
exe_ext=''
_exe=''

# work around case-insensitive file names
firstmakefile=GNUmakefile
# compile/link flags
ldflags=-g
optimize=-g

# Original, with :: separators, cause make to choke.
# No longer seems to be necessary at all.
# static_ext="B Data/Dumper Digest/MD5 Errno Fcntl Filter/Util/Call IO IPC/SysV MIME/Base64 Opcode PerlIO/scalar POSIX SDBM_File Socket Storable Unicode/Collate Unicode/Normalize attributes re"

# perl malloc will not work
usemymalloc=n
# cannot use nm
usenm=undef
# vfork() is buggy (as of 1.55 anyway)
usevfork=false

# Some other comments:
# If you see something like

#          got: '/E/users/jpl/src/cmd/perl/t'
#     expected: '/e/users/jpl/src/cmd/perl/t'
#     Failed test (../dist/Cwd/t/cwd.t at line 88)

# when running tests under harness, try the simple expedient of
# changing to directory
#     /E/users/jpl/src/cmd/perl/t   # note the leading capital /E
# before running the tests.  UWIN is a bit schizophrenic about case.
# It likes to return an uppercase "disk" letter for the leading directory,
# but your home directory may well have that in lower case.
# In most cases, they are entirely interchangeable, but the perl tests
# don't ignore case.  If they fail, change to the directory they expect.
