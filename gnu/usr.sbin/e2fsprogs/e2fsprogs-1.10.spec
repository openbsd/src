Summary: Tools for the second extended (ext2) filesystem 
Name: e2fsprogs
Version: 1.10
Release: 0
Copyright: GPL
Group: Utilities/System
Source: tsx-11.mit.edu:/pub/linux/packages/ext2fs/e2fsprogs-1.10.tar.gz
BuildRoot: /tmp/e2fsprogs-root

%description
This package includes a number of utilities for creating, checking,
and repairing ext2 filesystems.

%package devel
Summary: e2fs static libs and headers
Group: Development/Libraries

%description devel 
Libraries and header files needed to develop ext2 filesystem-specific
programs.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --enable-elf-shlibs

make libs progs docs

%install
export PATH=/sbin:$PATH
make install DESTDIR="$RPM_BUILD_ROOT"
make install-libs DESTDIR="$RPM_BUILD_ROOT"

mv $RPM_BUILD_ROOT/usr/sbin/debugfs $RPM_BUILD_ROOT/sbin/debugfs

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%doc README RELEASE-NOTES
%attr(-, root, root) /sbin/e2fsck
%attr(-, root, root) /sbin/fsck.ext2
%attr(-, root, root) /sbin/debugfs
%attr(-, root, root) /sbin/mke2fs
%attr(-, root, root) /sbin/badblocks
%attr(-, root, root) /sbin/tune2fs
%attr(-, root, root) /sbin/dumpe2fs
%attr(-, root, root) /sbin/fsck
%attr(-, root, root) /usr/sbin/mklost+found
%attr(-, root, root) /sbin/mkfs.ext2

%attr(-, root, root) /lib/libe2p.so.2.3
%attr(-, root, root) /lib/libext2fs.so.2.3
%attr(-, root, root) /lib/libss.so.2.0
%attr(-, root, root) /lib/libcom_err.so.2.0
%attr(-, root, root) /lib/libuuid.so.1.1

%attr(-, root, root) /usr/bin/chattr
%attr(-, root, root) /usr/bin/lsattr
%attr(-, root, root) /usr/man/man8/e2fsck.8
%attr(-, root, root) /usr/man/man8/debugfs.8
%attr(-, root, root) /usr/man/man8/tune2fs.8
%attr(-, root, root) /usr/man/man8/mklost+found.8
%attr(-, root, root) /usr/man/man8/mke2fs.8
%attr(-, root, root) /usr/man/man8/dumpe2fs.8
%attr(-, root, root) /usr/man/man8/badblocks.8
%attr(-, root, root) /usr/man/man8/fsck.8
%attr(-, root, root) /usr/man/man1/chattr.1
%attr(-, root, root) /usr/man/man1/lsattr.1

%files devel
%attr(-, root, root) /usr/info/libext2fs.info*
%attr(-, root, root) /usr/lib/libe2p.a
%attr(-, root, root) /usr/lib/libext2fs.a
%attr(-, root, root) /usr/lib/libss.a
%attr(-, root, root) /usr/lib/libcom_err.a
%attr(-, root, root) /usr/lib/libuuid.a
%attr(-, root, root) /usr/include/ss
%attr(-, root, root) /usr/include/ext2fs
%attr(-, root, root) /usr/include/et
%attr(-, root, root) /usr/include/uuid
%attr(-, root, root) /usr/lib/libe2p.so
%attr(-, root, root) /usr/lib/libext2fs.so
%attr(-, root, root) /usr/lib/libss.so
%attr(-, root, root) /usr/lib/libcom_err.so
%attr(-, root, root) /usr/lib/libuuid.so

