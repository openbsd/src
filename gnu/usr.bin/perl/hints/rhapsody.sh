##
# Rhapsody (Mac OS X Server) hints
# Wilfredo Sanchez <wsanchez@mit.edu>
##

##
# Paths
##

# BSD paths
case "$prefix" in
'')	
	prefix='/usr/local'; # Built-in perl uses /usr
	siteprefix='/usr/local';
	vendorprefix='/usr/local'; usevendorprefix='define';

	# 4BSD uses ${prefix}/share/man, not ${prefix}/man.
	# Don't put man pages in ${prefix}/lib; that's goofy.
	man1dir="${prefix}/share/man/man1";
	man3dir="${prefix}/share/man/man3";

	# Where to put modules.
	# Built-in perl uses /System/Library/Perl
	privlib='/Local/Library/Perl';
	sitelib='/Local/Library/Perl';
	vendorlib='/Network/Library/Perl';
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

# XXX Unclear why we require -pipe and -fno-common here.
ccflags="${ccflags} -pipe -fno-common"

# cpp-precomp is problematic.
cppflags='-traditional-cpp';

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

# malloc works
usemymalloc='n';

# Case-insensitive filesystems don't get along with Makefile and
# makefile in the same place.  Since Darwin uses GNU make, this dodges
# the problem.
firstmakefile=GNUmakefile;
