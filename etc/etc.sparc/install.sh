#!/bin/sh
# $Id: install.sh,v 1.1.1.1 1995/10/18 08:38:02 deraadt Exp $
umask 0
cat ./bin.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./etc.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./sbin.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.bin.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.games.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.include.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.lib.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.libexec.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.misc.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.sbin.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./usr.share.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./var.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cat ./dev.tar.gz | gzip -d | (cd /mnt; tar xvpf -)
cp ./netbsd.scsi3 /mnt/netbsd
chmod 640 /mnt/netbsd; chown root.kmem /mnt/netbsd
cd /mnt/dev; ./MAKEDEV all
mv /mnt/etc/fstab.sd /mnt/etc/fstab
