#!/bin/sh
# $Id: install.sh,v 1.2 1995/11/08 07:12:53 deraadt Exp $
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
cp ./bsd.scsi3 /mnt/bsd
chmod 640 /mnt/bsd; chown root.kmem /mnt/bsd
cd /mnt/dev; ./MAKEDEV all
mv /mnt/etc/fstab.sd /mnt/etc/fstab
