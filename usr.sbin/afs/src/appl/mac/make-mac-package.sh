#!/bin/sh

name=`basename $0`

if [ $# = 0 ]; then
    echo "$name version"
    exit
fi

DESTDIR=`pwd`/arlapackage

mkdir $DESTDIR
mkdir $DESTDIR/usr
mkdir $DESTDIR/usr/arla
mkdir $DESTDIR/Applications
mkdir $DESTDIR/Applications/Arla
mkdir -p $DESTDIR/Library/StartupItems/Arla

cp -Rp /usr/arla/bin $DESTDIR/usr/arla
cp -Rp /usr/arla/cache $DESTDIR/usr/arla
cp -Rp /usr/arla/etc $DESTDIR/usr/arla
cp -Rp /usr/arla/include $DESTDIR/usr/arla
cp -Rp /usr/arla/info $DESTDIR/usr/arla
cp -Rp /usr/arla/lib $DESTDIR/usr/arla
cp -Rp /usr/arla/libexec $DESTDIR/usr/arla
cp -Rp /usr/arla/man $DESTDIR/usr/arla
cp -Rp /usr/arla/sbin $DESTDIR/usr/arla
cp -Rp "/usr/arla/Arla Configuration.app" $DESTDIR/Applications/Arla

cat > $DESTDIR/Library/StartupItems/Arla/Arla <<'EOF'
#!/bin/sh
if [ -f /usr/arla/etc/startatboot ]; then
    if [ `cat /usr/arla/etc/startatboot` = "yes" ]; then
	test -d /afs || mkdir /afs
	/sbin/kmodload /usr/arla/bin/xfs_mod.o
	/usr/arla/libexec/arlad -D
	/usr/arla/sbin/mount_xfs /dev/xfs0 /afs
	/usr/sbin/disktool -r
    fi
fi
EOF
chmod +x $DESTDIR/Library/StartupItems/Arla/Arla

cat > $DESTDIR/Library/StartupItems/Arla/StartupParameters.plist <<EOF
{
  Description = "Arla AFS Client";
  Provides = ("AFS");
  Requires = ("Network");
  OrderPreference = "None";
  Messages =
  {
    start = "Starting Arla AFS Client";
    stop = "Stopping Arla AFS Client";
  }
}
EOF

cat > arla.info <<"EOF"
Title Arla
Version $1
Description
DefaultLocation /
Diskname (null)
DeleteWarning 
NeedsAuthorization YES
DisableStop NO
UseUserMask NO
Application NO
Relocatable NO
Required NO
InstallOnly NO
RequiresReboot NO
InstallFat NO
EOF

/usr/bin/package $DESTDIR arla.info
tar cf arla-$1.pkg.tar arla.pkg
gzip arla-$1.pkg.tar
