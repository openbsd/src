Summary: A version control system.
Name: cvs
Version: 1.11.1p1
Release: 1
Copyright: GPL
Group: Development/Tools
Source: ftp://ftp.cvshome.com/pub/cvs-%{version}/cvs-%{version}.tar.gz
URL: http://CVSHome.org
Prereq: /sbin/install-info
Prefix: %{_prefix}
Buildroot: %{_tmppath}/%{name}-root

%description
CVS (Concurrent Version System) is a version control system which can
record the history of your files (usually, but not always, source
code). CVS only stores the differences between versions, instead of
every version of every file you've ever created. CVS also keeps a log
of who, when and why changes occurred.

CVS is very helpful for managing releases and controlling the
concurrent editing of source files among multiple authors. Instead of
providing version control for a collection of files in a single
directory, CVS provides version control for a hierarchical collection
of directories consisting of revision controlled files.  These
directories and files can then be combined together to form a software
release.

Install the cvs package if you need to use a version control system.

%define gssapi %(test -r /usr/kerberos/lib/libkrb5.a && echo 1 || echo 0)
%if %gssapi
%package krb5
Summary: A version control system with Kerberos authentication.
Group: Development/Tools
Requires: cvs krb5-libs
BuildRequires: krb5-devel

%description krb5
CVS (Concurrent Version System) is a version control system which can
record the history of your files (usually, but not always, source
code). CVS only stores the differences between versions, instead of
every version of every file you've ever created. CVS also keeps a log
of who, when and why changes occurred.

CVS is very helpful for managing releases and controlling the
concurrent editing of source files among multiple authors. Instead of
providing version control for a collection of files in a single
directory, CVS provides version control for a hierarchical collection
of directories consisting of revision controlled files.  These
directories and files can then be combined together to form a software
release.

This package contains only a Kerberized version of the CVS binary.  You should
install the base cvs package to get the standard CVS distribution.

Install the Kerberos version of the cvs package if you need to use a version
control system with Kerberos authentication and encryption.

%define _kerberosdir %{prefix}/kerberos
%define _kerberosbindir %{_kerberosdir}/bin
%endif

%prep
%setup -q

%build
# The Kerberized binary
%if %gssapi
%configure --with-gssapi=/usr/kerberos --enable-encryption

make LDFLAGS=-s
mv src/cvs src/cvs.krb5

rm config.cache
%endif

# The standard package
%configure --without-gssapi

make LDFLAGS=-s
make -C doc info

%install
rm -rf $RPM_BUILD_ROOT

%makeinstall
# The Kerberized binary
%if %gssapi
./install-sh src/cvs.krb5 $RPM_BUILD_ROOT%{_kerberosbindir}/cvs
%endif

rm -f $RPM_BUILD_ROOT${_infodir}/dir
gzip -9nf $RPM_BUILD_ROOT%{_infodir}/cvs*
strip $RPM_BUILD_ROOT%{_bindir}/cvs
%if %gssapi
strip $RPM_BUILD_ROOT%{_kerberosbindir}/cvs
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%post
    /sbin/install-info --info-dir=%{_infodir} %{_infodir}/cvs.info.gz
    /sbin/install-info --info-dir=%{_infodir} %{_infodir}/cvsclient.info.gz
%preun
if [ $1 = 0 ]; then
    # uninstall the info reference in the dir file
    /sbin/install-info --delete --info-dir=%{_infodir} %{_infodir}/cvs.info.gz
    /sbin/install-info --delete --info-dir=%{_infodir} %{_infodir}/cvsclient.info.gz
fi

%files
%defattr(-,root,root)
%doc BUGS FAQ MINOR-BUGS NEWS PROJECTS TODO README
%doc doc/RCSFILES doc/*.ps
%{_bindir}
%{_infodir}/cvs*.gz
%{_mandir}

%if %gssapi
%files krb5
%defattr(-,root,root)
%{_kerberosdir}
%endif

%changelog
* Thu Apr 26 2001 Derek Price <dprice@collab.net>
- avoid picking up %{_infodir}/dir.
- remove krb5-configs from requirements since RedHat doesn't use it anymore.

* Wed Nov 29 2000 Derek Price <dprice@openavenue.com>
- Use _infodir consistently for info pages and _bindir for binaries.
- use more succinct file list

* Wed Oct 18 2000 Derek Price <dprice@openavenue.com>
- Make the Kerberos binary a subpackage.
- fix the info & man pages too

* Wed Sep 27 2000 Derek Price <dprice@openavenue.com>
- updated for cvs 1.11

* Wed Mar  1 2000 Nalin Dahyabhai <nalin@redhat.com>
- make kerberos support conditional at build-time

* Wed Mar  1 2000 Bill Nottingham <notting@redhat.com>
- integrate kerberos support into main tree

* Mon Feb 14 2000 Nalin Dahyabhai <nalin@redhat.com>
- build with gssapi auth (--with-gssapi, --with-encryption)
- apply patch to update libs to krb5 1.1.1

* Fri Feb 04 2000 Cristian Gafton <gafton@redhat.com>
- fix the damn info pages too while we're at it.
- fix description
- man pages are compressed
- make sure %post and %preun work okay

* Sun Jan 9 2000  Jim Kingdon <http://bugzilla.redhat.com/bugzilla>
- update to 1.10.7.

* Wed Jul 14 1999 Jim Kingdon <http://developer.redhat.com>
- add the patch to make 1.10.6 usable
  (http://www.cyclic.com/cvs/dev-known.html).

* Tue Jun  1 1999 Jeff Johnson <jbj@redhat.com>
- update to 1.10.6.

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 2)

* Mon Feb 22 1999 Jeff Johnson <jbj@redhat.com>
- updated text in spec file.

* Mon Feb 22 1999 Jeff Johnson <jbj@redhat.com>
- update to 1.10.5.

* Tue Feb  2 1999 Jeff Johnson <jbj@redhat.com>
- update to 1.10.4.

* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- update to 1.10.3.

* Mon Sep 28 1998 Jeff Johnson <jbj@redhat.com>
- update to 1.10.2.

* Wed Sep 23 1998 Jeff Johnson <jbj@redhat.com>
- remove trailing characters from rcs2log mktemp args

* Thu Sep 10 1998 Jeff Johnson <jbj@redhat.com>
- update to 1.10.1

* Mon Aug 31 1998 Jeff Johnson <jbj@redhat.com>
- fix race conditions in cvsbug/rcs2log

* Sun Aug 16 1998 Jeff Johnson <jbj@redhat.com>
- update to 1.10.

* Wed Aug 12 1998 Jeff Johnson <jbj@redhat.com>
- update to 1.9.30.

* Mon Jun 08 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr

* Mon Jun  8 1998 Jeff Johnson <jbj@redhat.com>
- build root
- update to 1.9.28

* Mon Apr 27 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr

* Wed Oct 29 1997 Otto Hammersmith <otto@redhat.com>
- added install-info stuff
- added changelog section
