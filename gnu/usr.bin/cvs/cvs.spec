Summary: Concurrent Versions System
Name: cvs
Version: @VERSION@
Release: 1
Copyright: GPL
Group: Development/Version Control
Source: ftp://download.cyclic.com/pub/cvs-@VERSION@/cvs-@VERSION@.tar.gz
Prefix: /usr

%description
CVS is a version control system, which allows you to keep old versions
of files (usually source code), keep a log of who, when, and why
changes occurred, etc., like RCS or SCCS.  Unlike the simpler systems,
CVS does not just operate on one file at a time or one directory at a
time, but operates on hierarchical collections of directories
consisting of version controlled files.  CVS helps to manage releases
and to control the concurrent editing of source files among multiple
authors.  CVS allows triggers to enable/log/control various
operations and works well over a wide area network.

%prep
%setup

%build
./configure --prefix=$RPM_BUILD_ROOT/usr
make CFLAGS="$RPM_OPT_FLAGS -DRCSBIN_DFLT=\\\"/usr/bin\\\"" LDFLAGS=-s 

%install
make installdirs
make install
rm -f $RPM_BUILD_ROOT/usr/info/cvs*
make install-info
gzip -9nf $RPM_BUILD_ROOT/usr/info/cvs*

%files
%doc BUGS COPYING COPYING.LIB FAQ HACKING
%doc INSTALL MINOR-BUGS NEWS PROJECTS README TESTS TODO
/usr/bin/cvs
/usr/bin/cvsbug
/usr/bin/rcs2log
/usr/man/man1/cvs.1
/usr/man/man5/cvs.5
/usr/man/man8/cvsbug.8
/usr/info/cvs*
/usr/lib/cvs
