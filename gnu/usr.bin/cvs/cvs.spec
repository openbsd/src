Summary: Concurrent Versions System
Name: cvs
Version: @VERSION@
Release: 1
Copyright: GPL
Group: Development/Version Control
Source: ftp.cyclic.com:/pub/cvs-@VERSION@.tar.gz
Buildroot: /

%description
CVS is a version control system, which allows you to keep old versions
of files (usually source code), keep a log of who, when, and why
changes occurred, etc., like RCS or SCCS.  It handles multiple
developers, multiple directories, triggers to enable/log/control
various operations, and can work over a wide area network.  The
following tasks are not included; they can be done in conjunction with
CVS but will tend to require some script-writing and software other
than CVS: bug-tracking, build management (that is, make and make-like
tools), and automated testing.

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
