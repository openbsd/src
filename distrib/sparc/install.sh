#!/bin/sh
# $Id: install.sh,v 1.1.1.1 1995/10/18 08:37:49 deraadt Exp $
umask 0

TAR="base.tar.gz comp.tar.gz etc.tar.gz games.tar.gz man.tar.gz
     misc.tar.gz secr.tar.gz text.tar.gz"

for i in $TAR
do
    if [ -f $i ]; then
	echo -n $i...
	cat $i | gzip -d | (cd /mnt; gtar xvpf -)
    else
	echo skipping $i because you do not want it
    fi
done
echo
cp netbsd.id3_scsi /mnt/netbsd
chmod 640 /mnt/netbsd; chown root.kmem /mnt/netbsd
cd /mnt/dev; ./MAKEDEV all
mv /mnt/etc/fstab.sd /mnt/etc/fstab
