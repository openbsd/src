# instbin.mk - generated from instbin.conf by crunchgen 0.2

STRIP=strip
LIBS=-L/rel/usr/lib  -ledit -lutil -lcurses -ll -lm -lkvm -lz -lkrb -ldes
CRUNCHED_OBJS= dd.lo mount_cd9660.lo df.lo dhclient.lo mount.lo mount_ext2fs.lo sync.lo restore.lo newfs_msdos.lo stty.lo ln.lo disklabel.lo pax.lo ping.lo cat.lo ifconfig.lo ls.lo less.lo mount_nfs.lo fdisk.lo grep.lo umount.lo mount_msdos.lo rsh.lo fsck.lo scsi.lo mknod.lo route.lo ftp.lo mount_ffs.lo reboot.lo ed.lo cp.lo gzip.lo chmod.lo fsck_ffs.lo sort.lo init.lo newfs.lo mount_kernfs.lo tip.lo rm.lo mt.lo mkdir.lo sed.lo ksh.lo sleep.lo mv.lo expr.lo test.lo hostname.lo mg.lo
SUBMAKE_TARGETS= dd_make mount_cd9660_make df_make dhclient_make mount_make mount_ext2fs_make sync_make restore_make newfs_msdos_make stty_make ln_make disklabel_make pax_make ping_make cat_make ifconfig_make ls_make less_make mount_nfs_make fdisk_make grep_make umount_make mount_msdos_make rsh_make fsck_make scsi_make mknod_make route_make ftp_make mount_ffs_make reboot_make ed_make cp_make gzip_make chmod_make fsck_ffs_make sort_make init_make newfs_make mount_kernfs_make tip_make rm_make mt_make mkdir_make sed_make ksh_make sleep_make mv_make expr_make test_make hostname_make mg_make

instbin: instbin.o $(CRUNCHED_OBJS)
	$(CC) -static -o instbin instbin.o $(CRUNCHED_OBJS) $(LIBS)
	$(STRIP) instbin
all: objs exe
objs: $(SUBMAKE_TARGETS)
exe: instbin
clean:
	rm -f instbin *.lo *.o *_stub.c

# -------- dd

dd_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd
dd_OBJS= args.o conv.o conv_tab.o dd.o misc.o position.o
dd_make:
	(cd $(dd_SRCDIR); make -f Makefile $(dd_OBJS))

dd_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd/args.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd/conv.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd/conv_tab.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd/dd.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd/misc.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/dd/position.o
dd_stub.c:
	echo "int _crunched_dd_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >dd_stub.c
dd.lo: dd_stub.o $(dd_OBJPATHS)
	${LD} -dc -r -o dd.lo dd_stub.o $(dd_OBJPATHS)
	crunchide -k _crunched_dd_stub dd.lo

# -------- mount_cd9660

mount_cd9660_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_cd9660
mount_cd9660_OBJS= mount_cd9660.o getmntopts.o
mount_cd9660_make:
	(cd $(mount_cd9660_SRCDIR); make -f Makefile $(mount_cd9660_OBJS))

mount_cd9660_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_cd9660/mount_cd9660.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_cd9660/getmntopts.o
mount_cd9660_stub.c:
	echo "int _crunched_mount_cd9660_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_cd9660_stub.c
mount_cd9660.lo: mount_cd9660_stub.o $(mount_cd9660_OBJPATHS)
	${LD} -dc -r -o mount_cd9660.lo mount_cd9660_stub.o $(mount_cd9660_OBJPATHS)
	crunchide -k _crunched_mount_cd9660_stub mount_cd9660.lo

# -------- df

df_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/df
df_OBJS= df.o ffs_df.o lfs_df.o ext2fs_df.o
df_make:
	(cd $(df_SRCDIR); make -f Makefile $(df_OBJS))

df_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/df/df.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/df/ffs_df.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/df/lfs_df.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/df/ext2fs_df.o
df_stub.c:
	echo "int _crunched_df_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >df_stub.c
df.lo: df_stub.o $(df_OBJPATHS)
	${LD} -dc -r -o df.lo df_stub.o $(df_OBJPATHS)
	crunchide -k _crunched_df_stub df.lo

# -------- dhclient

dhclient_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient
dhclient_OBJS= dhclient.o clparse.o raw.o parse.o nit.o icmp.o dispatch.o conflex.o upf.o bpf.o socket.o packet.o memory.o print.o options.o inet.o convert.o sysconf.o tree.o tables.o hash.o alloc.o errwarn.o inet_addr.o dns.o resolv.o
dhclient_make:
	(cd $(dhclient_SRCDIR); make -f Makefile $(dhclient_OBJS))

dhclient_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/dhclient.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/clparse.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/raw.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/parse.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/nit.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/icmp.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/dispatch.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/conflex.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/upf.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/bpf.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/socket.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/packet.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/memory.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/print.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/options.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/inet.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/convert.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/sysconf.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/tree.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/tables.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/hash.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/alloc.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/errwarn.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/inet_addr.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/dns.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/dhclient/resolv.o
dhclient_stub.c:
	echo "int _crunched_dhclient_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >dhclient_stub.c
dhclient.lo: dhclient_stub.o $(dhclient_OBJPATHS)
	${LD} -dc -r -o dhclient.lo dhclient_stub.o $(dhclient_OBJPATHS)
	crunchide -k _crunched_dhclient_stub dhclient.lo

# -------- mount

mount_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount
mount_OBJS= mount.o getmntopts.o
mount_make:
	(cd $(mount_SRCDIR); make -f Makefile $(mount_OBJS))

mount_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount/mount.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount/getmntopts.o
mount_stub.c:
	echo "int _crunched_mount_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_stub.c
mount.lo: mount_stub.o $(mount_OBJPATHS)
	${LD} -dc -r -o mount.lo mount_stub.o $(mount_OBJPATHS)
	crunchide -k _crunched_mount_stub mount.lo

# -------- mount_ext2fs

mount_ext2fs_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_ext2fs
mount_ext2fs_OBJS= mount_ext2fs.o getmntopts.o
mount_ext2fs_make:
	(cd $(mount_ext2fs_SRCDIR); make -f Makefile $(mount_ext2fs_OBJS))

mount_ext2fs_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_ext2fs/mount_ext2fs.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_ext2fs/getmntopts.o
mount_ext2fs_stub.c:
	echo "int _crunched_mount_ext2fs_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_ext2fs_stub.c
mount_ext2fs.lo: mount_ext2fs_stub.o $(mount_ext2fs_OBJPATHS)
	${LD} -dc -r -o mount_ext2fs.lo mount_ext2fs_stub.o $(mount_ext2fs_OBJPATHS)
	crunchide -k _crunched_mount_ext2fs_stub mount_ext2fs.lo

# -------- sync

sync_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/sync
sync_OBJS= sync.o
sync_make:
	(cd $(sync_SRCDIR); make -f Makefile $(sync_OBJS))

sync_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/sync/sync.o
sync_stub.c:
	echo "int _crunched_sync_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >sync_stub.c
sync.lo: sync_stub.o $(sync_OBJPATHS)
	${LD} -dc -r -o sync.lo sync_stub.o $(sync_OBJPATHS)
	crunchide -k _crunched_sync_stub sync.lo

# -------- restore

restore_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore
restore_OBJS= main.o interactive.o restore.o dirs.o symtab.o tape.o utilities.o dumprmt.o
restore_make:
	(cd $(restore_SRCDIR); make -f Makefile $(restore_OBJS))

restore_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/interactive.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/restore.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/dirs.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/symtab.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/tape.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/utilities.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/restore/dumprmt.o
restore_stub.c:
	echo "int _crunched_restore_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >restore_stub.c
restore.lo: restore_stub.o $(restore_OBJPATHS)
	${LD} -dc -r -o restore.lo restore_stub.o $(restore_OBJPATHS)
	crunchide -k _crunched_restore_stub restore.lo

# -------- newfs_msdos

newfs_msdos_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs_msdos
newfs_msdos_OBJS= newfs_msdos.o
newfs_msdos_make:
	(cd $(newfs_msdos_SRCDIR); make -f Makefile $(newfs_msdos_OBJS))

newfs_msdos_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs_msdos/newfs_msdos.o
newfs_msdos_stub.c:
	echo "int _crunched_newfs_msdos_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >newfs_msdos_stub.c
newfs_msdos.lo: newfs_msdos_stub.o $(newfs_msdos_OBJPATHS)
	${LD} -dc -r -o newfs_msdos.lo newfs_msdos_stub.o $(newfs_msdos_OBJPATHS)
	crunchide -k _crunched_newfs_msdos_stub newfs_msdos.lo

# -------- stty

stty_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty
stty_OBJS= cchar.o gfmt.o key.o modes.o print.o stty.o
stty_make:
	(cd $(stty_SRCDIR); make -f Makefile $(stty_OBJS))

stty_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty/cchar.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty/gfmt.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty/key.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty/modes.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty/print.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/stty/stty.o
stty_stub.c:
	echo "int _crunched_stty_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >stty_stub.c
stty.lo: stty_stub.o $(stty_OBJPATHS)
	${LD} -dc -r -o stty.lo stty_stub.o $(stty_OBJPATHS)
	crunchide -k _crunched_stty_stub stty.lo

# -------- ln

ln_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/ln
ln_OBJS= ln.o
ln_make:
	(cd $(ln_SRCDIR); make -f Makefile $(ln_OBJS))

ln_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ln/ln.o
ln_stub.c:
	echo "int _crunched_ln_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ln_stub.c
ln.lo: ln_stub.o $(ln_OBJPATHS)
	${LD} -dc -r -o ln.lo ln_stub.o $(ln_OBJPATHS)
	crunchide -k _crunched_ln_stub ln.lo

# -------- disklabel

disklabel_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/disklabel
disklabel_OBJS= disklabel.o dkcksum.o editor.o manual.o
disklabel_make:
	(cd $(disklabel_SRCDIR); make -f Makefile $(disklabel_OBJS))

disklabel_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/disklabel/disklabel.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/disklabel/dkcksum.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/disklabel/editor.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/disklabel/manual.o
disklabel_stub.c:
	echo "int _crunched_disklabel_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >disklabel_stub.c
disklabel.lo: disklabel_stub.o $(disklabel_OBJPATHS)
	${LD} -dc -r -o disklabel.lo disklabel_stub.o $(disklabel_OBJPATHS)
	crunchide -k _crunched_disklabel_stub disklabel.lo

# -------- pax

pax_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax
pax_OBJS= ar_io.o ar_subs.o buf_subs.o cache.o cpio.o file_subs.o ftree.o gen_subs.o getoldopt.o options.o pat_rep.o pax.o sel_subs.o tables.o tar.o tty_subs.o
pax_make:
	(cd $(pax_SRCDIR); make -f Makefile $(pax_OBJS))

pax_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/ar_io.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/ar_subs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/buf_subs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/cache.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/cpio.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/file_subs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/ftree.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/gen_subs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/getoldopt.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/options.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/pat_rep.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/pax.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/sel_subs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/tables.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/tar.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/pax/tty_subs.o
pax_stub.c:
	echo "int _crunched_pax_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >pax_stub.c
pax.lo: pax_stub.o $(pax_OBJPATHS)
	${LD} -dc -r -o pax.lo pax_stub.o $(pax_OBJPATHS)
	crunchide -k _crunched_pax_stub pax.lo

# -------- ping

ping_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/ping
ping_OBJS= ping.o
ping_make:
	(cd $(ping_SRCDIR); make -f Makefile $(ping_OBJS))

ping_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/ping/ping.o
ping_stub.c:
	echo "int _crunched_ping_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ping_stub.c
ping.lo: ping_stub.o $(ping_OBJPATHS)
	${LD} -dc -r -o ping.lo ping_stub.o $(ping_OBJPATHS)
	crunchide -k _crunched_ping_stub ping.lo

# -------- cat

cat_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/cat
cat_OBJS= cat.o
cat_make:
	(cd $(cat_SRCDIR); make -f Makefile $(cat_OBJS))

cat_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/cat/cat.o
cat_stub.c:
	echo "int _crunched_cat_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >cat_stub.c
cat.lo: cat_stub.o $(cat_OBJPATHS)
	${LD} -dc -r -o cat.lo cat_stub.o $(cat_OBJPATHS)
	crunchide -k _crunched_cat_stub cat.lo

# -------- ifconfig

ifconfig_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/ifconfig
ifconfig_OBJS= ifconfig.o
ifconfig_make:
	(cd $(ifconfig_SRCDIR); make -f Makefile $(ifconfig_OBJS))

ifconfig_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/ifconfig/ifconfig.o
ifconfig_stub.c:
	echo "int _crunched_ifconfig_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ifconfig_stub.c
ifconfig.lo: ifconfig_stub.o $(ifconfig_OBJPATHS)
	${LD} -dc -r -o ifconfig.lo ifconfig_stub.o $(ifconfig_OBJPATHS)
	crunchide -k _crunched_ifconfig_stub ifconfig.lo

# -------- ls

ls_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/ls
ls_OBJS= cmp.o ls.o main.o print.o util.o
ls_make:
	(cd $(ls_SRCDIR); make -f Makefile $(ls_OBJS))

ls_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ls/cmp.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ls/ls.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ls/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ls/print.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ls/util.o
ls_stub.c:
	echo "int _crunched_ls_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ls_stub.c
ls.lo: ls_stub.o $(ls_OBJPATHS)
	${LD} -dc -r -o ls.lo ls_stub.o $(ls_OBJPATHS)
	crunchide -k _crunched_ls_stub ls.lo

# -------- less

less_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less
less_OBJS= main.o screen.o brac.o ch.o charset.o cmdbuf.o command.o decode.o edit.o filename.o forwback.o help.o ifile.o input.o jump.o line.o linenum.o lsystem.o mark.o optfunc.o option.o opttbl.o os.o output.o position.o prompt.o search.o signal.o tags.o ttyin.o version.o
less_make:
	(cd $(less_SRCDIR); make -f Makefile.bsd-wrapper $(less_OBJS))

less_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/screen.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/brac.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/ch.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/charset.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/cmdbuf.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/command.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/decode.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/edit.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/filename.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/forwback.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/help.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/ifile.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/input.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/jump.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/line.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/linenum.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/lsystem.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/mark.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/optfunc.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/option.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/opttbl.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/os.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/output.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/position.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/prompt.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/search.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/signal.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/tags.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/ttyin.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/less/version.o
less_stub.c:
	echo "int _crunched_less_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >less_stub.c
less.lo: less_stub.o $(less_OBJPATHS)
	${LD} -dc -r -o less.lo less_stub.o $(less_OBJPATHS)
	crunchide -k _crunched_less_stub less.lo

# -------- mount_nfs

mount_nfs_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_nfs
mount_nfs_OBJS= mount_nfs.o getmntopts.o
mount_nfs_make:
	(cd $(mount_nfs_SRCDIR); make -f Makefile $(mount_nfs_OBJS))

mount_nfs_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_nfs/mount_nfs.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_nfs/getmntopts.o
mount_nfs_stub.c:
	echo "int _crunched_mount_nfs_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_nfs_stub.c
mount_nfs.lo: mount_nfs_stub.o $(mount_nfs_OBJPATHS)
	${LD} -dc -r -o mount_nfs.lo mount_nfs_stub.o $(mount_nfs_OBJPATHS)
	crunchide -k _crunched_mount_nfs_stub mount_nfs.lo

# -------- fdisk

fdisk_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk
fdisk_OBJS= fdisk.o user.o misc.o disk.o mbr.o part.o cmd.o manual.o
fdisk_make:
	(cd $(fdisk_SRCDIR); make -f Makefile $(fdisk_OBJS))

fdisk_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/fdisk.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/user.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/misc.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/disk.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/mbr.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/part.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/cmd.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fdisk/manual.o
fdisk_stub.c:
	echo "int _crunched_fdisk_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >fdisk_stub.c
fdisk.lo: fdisk_stub.o $(fdisk_OBJPATHS)
	${LD} -dc -r -o fdisk.lo fdisk_stub.o $(fdisk_OBJPATHS)
	crunchide -k _crunched_fdisk_stub fdisk.lo

# -------- grep

grep_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep
grep_OBJS= dfa.o grep.o getopt.o getopt1.o kwset.o obstack.o regex.o savedir.o search.o stpcpy.o
grep_make:
	(cd $(grep_SRCDIR); make -f Makefile $(grep_OBJS))

grep_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/dfa.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/grep.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/getopt.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/getopt1.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/kwset.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/obstack.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/regex.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/savedir.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/search.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/grep/stpcpy.o
grep_stub.c:
	echo "int _crunched_grep_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >grep_stub.c
grep.lo: grep_stub.o $(grep_OBJPATHS)
	${LD} -dc -r -o grep.lo grep_stub.o $(grep_OBJPATHS)
	crunchide -k _crunched_grep_stub grep.lo

# -------- umount

umount_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/umount
umount_OBJS= umount.o
umount_make:
	(cd $(umount_SRCDIR); make -f Makefile $(umount_OBJS))

umount_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/umount/umount.o
umount_stub.c:
	echo "int _crunched_umount_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >umount_stub.c
umount.lo: umount_stub.o $(umount_OBJPATHS)
	${LD} -dc -r -o umount.lo umount_stub.o $(umount_OBJPATHS)
	crunchide -k _crunched_umount_stub umount.lo

# -------- mount_msdos

mount_msdos_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_msdos
mount_msdos_OBJS= mount_msdos.o getmntopts.o
mount_msdos_make:
	(cd $(mount_msdos_SRCDIR); make -f Makefile $(mount_msdos_OBJS))

mount_msdos_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_msdos/mount_msdos.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_msdos/getmntopts.o
mount_msdos_stub.c:
	echo "int _crunched_mount_msdos_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_msdos_stub.c
mount_msdos.lo: mount_msdos_stub.o $(mount_msdos_OBJPATHS)
	${LD} -dc -r -o mount_msdos.lo mount_msdos_stub.o $(mount_msdos_OBJPATHS)
	crunchide -k _crunched_mount_msdos_stub mount_msdos.lo

# -------- rsh

rsh_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/rsh
rsh_OBJS= rsh.o
rsh_make:
	(cd $(rsh_SRCDIR); make -f Makefile $(rsh_OBJS))

rsh_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/rsh/rsh.o
rsh_stub.c:
	echo "int _crunched_rsh_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >rsh_stub.c
rsh.lo: rsh_stub.o $(rsh_OBJPATHS)
	${LD} -dc -r -o rsh.lo rsh_stub.o $(rsh_OBJPATHS)
	crunchide -k _crunched_rsh_stub rsh.lo

# -------- fsck

fsck_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck
fsck_OBJS= fsck.o fsutil.o preen.o
fsck_make:
	(cd $(fsck_SRCDIR); make -f Makefile $(fsck_OBJS))

fsck_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck/fsck.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck/fsutil.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck/preen.o
fsck_stub.c:
	echo "int _crunched_fsck_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >fsck_stub.c
fsck.lo: fsck_stub.o $(fsck_OBJPATHS)
	${LD} -dc -r -o fsck.lo fsck_stub.o $(fsck_OBJPATHS)
	crunchide -k _crunched_fsck_stub fsck.lo

# -------- scsi

scsi_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/scsi
scsi_OBJS= scsi.o
scsi_make:
	(cd $(scsi_SRCDIR); make -f Makefile $(scsi_OBJS))

scsi_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/scsi/scsi.o
scsi_stub.c:
	echo "int _crunched_scsi_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >scsi_stub.c
scsi.lo: scsi_stub.o $(scsi_OBJPATHS)
	${LD} -dc -r -o scsi.lo scsi_stub.o $(scsi_OBJPATHS)
	crunchide -k _crunched_scsi_stub scsi.lo

# -------- mknod

mknod_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mknod
mknod_OBJS= mknod.o
mknod_make:
	(cd $(mknod_SRCDIR); make -f Makefile $(mknod_OBJS))

mknod_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mknod/mknod.o
mknod_stub.c:
	echo "int _crunched_mknod_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mknod_stub.c
mknod.lo: mknod_stub.o $(mknod_OBJPATHS)
	${LD} -dc -r -o mknod.lo mknod_stub.o $(mknod_OBJPATHS)
	crunchide -k _crunched_mknod_stub mknod.lo

# -------- route

route_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/route
route_OBJS= route.o show.o keywords.o ccitt_addr.o
route_make:
	(cd $(route_SRCDIR); make -f Makefile $(route_OBJS))

route_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/route/route.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/route/show.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/route/keywords.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/route/ccitt_addr.o
route_stub.c:
	echo "int _crunched_route_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >route_stub.c
route.lo: route_stub.o $(route_OBJPATHS)
	${LD} -dc -r -o route.lo route_stub.o $(route_OBJPATHS)
	crunchide -k _crunched_route_stub route.lo

# -------- ftp

ftp_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp
ftp_OBJS= cmds.o cmdtab.o complete.o domacro.o fetch.o ftp.o main.o ruserpass.o stringlist.o util.o
ftp_make:
	(cd $(ftp_SRCDIR); make -f Makefile $(ftp_OBJS))

ftp_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/cmds.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/cmdtab.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/complete.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/domacro.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/fetch.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/ftp.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/ruserpass.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/stringlist.o /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/ftp/util.o
ftp_stub.c:
	echo "int _crunched_ftp_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ftp_stub.c
ftp.lo: ftp_stub.o $(ftp_OBJPATHS)
	${LD} -dc -r -o ftp.lo ftp_stub.o $(ftp_OBJPATHS)
	crunchide -k _crunched_ftp_stub ftp.lo

# -------- mount_ffs

mount_ffs_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_ffs
mount_ffs_OBJS= mount_ffs.o getmntopts.o
mount_ffs_make:
	(cd $(mount_ffs_SRCDIR); make -f Makefile $(mount_ffs_OBJS))

mount_ffs_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_ffs/mount_ffs.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_ffs/getmntopts.o
mount_ffs_stub.c:
	echo "int _crunched_mount_ffs_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_ffs_stub.c
mount_ffs.lo: mount_ffs_stub.o $(mount_ffs_OBJPATHS)
	${LD} -dc -r -o mount_ffs.lo mount_ffs_stub.o $(mount_ffs_OBJPATHS)
	crunchide -k _crunched_mount_ffs_stub mount_ffs.lo

# -------- reboot

reboot_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/reboot
reboot_OBJS= reboot.o
reboot_make:
	(cd $(reboot_SRCDIR); make -f Makefile $(reboot_OBJS))

reboot_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/reboot/reboot.o
reboot_stub.c:
	echo "int _crunched_reboot_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >reboot_stub.c
reboot.lo: reboot_stub.o $(reboot_OBJPATHS)
	${LD} -dc -r -o reboot.lo reboot_stub.o $(reboot_OBJPATHS)
	crunchide -k _crunched_reboot_stub reboot.lo

# -------- ed

ed_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed
ed_OBJS= buf.o cbc.o glbl.o io.o main.o re.o sub.o undo.o
ed_make:
	(cd $(ed_SRCDIR); make -f Makefile $(ed_OBJS))

ed_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/buf.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/cbc.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/glbl.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/io.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/re.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/sub.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ed/undo.o
ed_stub.c:
	echo "int _crunched_ed_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ed_stub.c
ed.lo: ed_stub.o $(ed_OBJPATHS)
	${LD} -dc -r -o ed.lo ed_stub.o $(ed_OBJPATHS)
	crunchide -k _crunched_ed_stub ed.lo

# -------- cp

cp_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/cp
cp_OBJS= cp.o utils.o
cp_make:
	(cd $(cp_SRCDIR); make -f Makefile $(cp_OBJS))

cp_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/cp/cp.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/cp/utils.o
cp_stub.c:
	echo "int _crunched_cp_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >cp_stub.c
cp.lo: cp_stub.o $(cp_OBJPATHS)
	${LD} -dc -r -o cp.lo cp_stub.o $(cp_OBJPATHS)
	crunchide -k _crunched_cp_stub cp.lo

# -------- gzip

gzip_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip
gzip_OBJS= gzip.o zip.o deflate.o trees.o bits.o unzip.o inflate.o util.o crypt.o lzw.o unlzw.o unlzh.o unpack.o getopt.o match.o
gzip_make:
	(cd $(gzip_SRCDIR); make -f Makefile $(gzip_OBJS))

gzip_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/gzip.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/zip.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/deflate.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/trees.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/bits.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/unzip.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/inflate.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/util.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/crypt.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/lzw.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/unlzw.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/unlzh.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/unpack.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/getopt.o /usr/src/distrib/mvmeppc/ramdisk/../../../gnu/usr.bin/gzip/match.o
gzip_stub.c:
	echo "int _crunched_gzip_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >gzip_stub.c
gzip.lo: gzip_stub.o $(gzip_OBJPATHS)
	${LD} -dc -r -o gzip.lo gzip_stub.o $(gzip_OBJPATHS)
	crunchide -k _crunched_gzip_stub gzip.lo

# -------- chmod

chmod_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/chmod
chmod_OBJS= chmod.o
chmod_make:
	(cd $(chmod_SRCDIR); make -f Makefile $(chmod_OBJS))

chmod_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/chmod/chmod.o
chmod_stub.c:
	echo "int _crunched_chmod_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >chmod_stub.c
chmod.lo: chmod_stub.o $(chmod_OBJPATHS)
	${LD} -dc -r -o chmod.lo chmod_stub.o $(chmod_OBJPATHS)
	crunchide -k _crunched_chmod_stub chmod.lo

# -------- fsck_ffs

fsck_ffs_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs
fsck_ffs_OBJS= dir.o inode.o main.o pass1.o pass1b.o pass2.o pass3.o pass4.o pass5.o fsutil.o setup.o utilities.o ffs_subr.o ffs_tables.o
fsck_ffs_make:
	(cd $(fsck_ffs_SRCDIR); make -f Makefile $(fsck_ffs_OBJS))

fsck_ffs_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/dir.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/inode.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/pass1.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/pass1b.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/pass2.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/pass3.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/pass4.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/pass5.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/fsutil.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/setup.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/utilities.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/ffs_subr.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/fsck_ffs/ffs_tables.o
fsck_ffs_stub.c:
	echo "int _crunched_fsck_ffs_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >fsck_ffs_stub.c
fsck_ffs.lo: fsck_ffs_stub.o $(fsck_ffs_OBJPATHS)
	${LD} -dc -r -o fsck_ffs.lo fsck_ffs_stub.o $(fsck_ffs_OBJPATHS)
	crunchide -k _crunched_fsck_ffs_stub fsck_ffs.lo

# -------- sort

sort_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort
sort_OBJS= append.o fields.o files.o fsort.o init.o msort.o sort.o tmp.o
sort_make:
	(cd $(sort_SRCDIR); make -f Makefile $(sort_OBJS))

sort_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/append.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/fields.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/files.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/fsort.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/init.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/msort.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/sort.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sort/tmp.o
sort_stub.c:
	echo "int _crunched_sort_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >sort_stub.c
sort.lo: sort_stub.o $(sort_OBJPATHS)
	${LD} -dc -r -o sort.lo sort_stub.o $(sort_OBJPATHS)
	crunchide -k _crunched_sort_stub sort.lo

# -------- init

init_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/init
init_OBJS= init.o
init_make:
	(cd $(init_SRCDIR); make -f Makefile $(init_OBJS))

init_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../distrib/special/init/init.o
init_stub.c:
	echo "int _crunched_init_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >init_stub.c
init.lo: init_stub.o $(init_OBJPATHS)
	${LD} -dc -r -o init.lo init_stub.o $(init_OBJPATHS)
	crunchide -k _crunched_init_stub init.lo

# -------- newfs

newfs_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs
newfs_OBJS= dkcksum.o getmntopts.o newfs.o mkfs.o
newfs_make:
	(cd $(newfs_SRCDIR); make -f Makefile $(newfs_OBJS))

newfs_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs/dkcksum.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs/getmntopts.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs/newfs.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/newfs/mkfs.o
newfs_stub.c:
	echo "int _crunched_newfs_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >newfs_stub.c
newfs.lo: newfs_stub.o $(newfs_OBJPATHS)
	${LD} -dc -r -o newfs.lo newfs_stub.o $(newfs_OBJPATHS)
	crunchide -k _crunched_newfs_stub newfs.lo

# -------- mount_kernfs

mount_kernfs_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_kernfs
mount_kernfs_OBJS= mount_kernfs.o getmntopts.o
mount_kernfs_make:
	(cd $(mount_kernfs_SRCDIR); make -f Makefile $(mount_kernfs_OBJS))

mount_kernfs_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_kernfs/mount_kernfs.o /usr/src/distrib/mvmeppc/ramdisk/../../../sbin/mount_kernfs/getmntopts.o
mount_kernfs_stub.c:
	echo "int _crunched_mount_kernfs_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mount_kernfs_stub.c
mount_kernfs.lo: mount_kernfs_stub.o $(mount_kernfs_OBJPATHS)
	${LD} -dc -r -o mount_kernfs.lo mount_kernfs_stub.o $(mount_kernfs_OBJPATHS)
	crunchide -k _crunched_mount_kernfs_stub mount_kernfs.lo

# -------- tip

tip_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip
tip_OBJS= acu.o acutab.o cmds.o cmdtab.o cu.o hunt.o log.o partab.o remote.o tip.o tipout.o uucplock.o value.o vars.o biz22.o courier.o df.o dn11.o hayes.o t3000.o v3451.o v831.o ventel.o
tip_make:
	(cd $(tip_SRCDIR); make -f Makefile $(tip_OBJS))

tip_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/acu.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/acutab.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/cmds.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/cmdtab.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/cu.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/hunt.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/log.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/partab.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/remote.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/tip.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/tipout.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/uucplock.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/value.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/vars.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/biz22.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/courier.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/df.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/dn11.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/hayes.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/t3000.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/v3451.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/v831.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/tip/ventel.o
tip_stub.c:
	echo "int _crunched_tip_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >tip_stub.c
tip.lo: tip_stub.o $(tip_OBJPATHS)
	${LD} -dc -r -o tip.lo tip_stub.o $(tip_OBJPATHS)
	crunchide -k _crunched_tip_stub tip.lo

# -------- rm

rm_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/rm
rm_OBJS= rm.o
rm_make:
	(cd $(rm_SRCDIR); make -f Makefile $(rm_OBJS))

rm_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/rm/rm.o
rm_stub.c:
	echo "int _crunched_rm_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >rm_stub.c
rm.lo: rm_stub.o $(rm_OBJPATHS)
	${LD} -dc -r -o rm.lo rm_stub.o $(rm_OBJPATHS)
	crunchide -k _crunched_rm_stub rm.lo

# -------- mt

mt_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/mt
mt_OBJS= mt.o mtrmt.o
mt_make:
	(cd $(mt_SRCDIR); make -f Makefile $(mt_OBJS))

mt_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/mt/mt.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/mt/mtrmt.o
mt_stub.c:
	echo "int _crunched_mt_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mt_stub.c
mt.lo: mt_stub.o $(mt_OBJPATHS)
	${LD} -dc -r -o mt.lo mt_stub.o $(mt_OBJPATHS)
	crunchide -k _crunched_mt_stub mt.lo

# -------- mkdir

mkdir_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/mkdir
mkdir_OBJS= mkdir.o
mkdir_make:
	(cd $(mkdir_SRCDIR); make -f Makefile $(mkdir_OBJS))

mkdir_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/mkdir/mkdir.o
mkdir_stub.c:
	echo "int _crunched_mkdir_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mkdir_stub.c
mkdir.lo: mkdir_stub.o $(mkdir_OBJPATHS)
	${LD} -dc -r -o mkdir.lo mkdir_stub.o $(mkdir_OBJPATHS)
	crunchide -k _crunched_mkdir_stub mkdir.lo

# -------- sed

sed_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sed
sed_OBJS= compile.o main.o misc.o process.o
sed_make:
	(cd $(sed_SRCDIR); make -f Makefile $(sed_OBJS))

sed_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sed/compile.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sed/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sed/misc.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/sed/process.o
sed_stub.c:
	echo "int _crunched_sed_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >sed_stub.c
sed.lo: sed_stub.o $(sed_OBJPATHS)
	${LD} -dc -r -o sed.lo sed_stub.o $(sed_OBJPATHS)
	crunchide -k _crunched_sed_stub sed.lo

# -------- ksh

ksh_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh
ksh_OBJS= alloc.o c_ksh.o c_sh.o c_test.o c_ulimit.o edit.o emacs.o eval.o exec.o expr.o history.o io.o jobs.o lex.o mail.o main.o misc.o missing.o path.o shf.o syn.o table.o trap.o tree.o tty.o var.o version.o vi.o
ksh_make:
	(cd $(ksh_SRCDIR); make -f Makefile $(ksh_OBJS))

ksh_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/alloc.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/c_ksh.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/c_sh.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/c_test.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/c_ulimit.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/edit.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/emacs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/eval.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/exec.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/expr.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/history.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/io.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/jobs.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/lex.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/mail.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/misc.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/missing.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/path.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/shf.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/syn.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/table.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/trap.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/tree.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/tty.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/var.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/version.o /usr/src/distrib/mvmeppc/ramdisk/../../../bin/ksh/vi.o
ksh_stub.c:
	echo "int _crunched_ksh_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >ksh_stub.c
ksh.lo: ksh_stub.o $(ksh_OBJPATHS)
	${LD} -dc -r -o ksh.lo ksh_stub.o $(ksh_OBJPATHS)
	crunchide -k _crunched_ksh_stub ksh.lo

# -------- sleep

sleep_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/sleep
sleep_OBJS= sleep.o
sleep_make:
	(cd $(sleep_SRCDIR); make -f Makefile $(sleep_OBJS))

sleep_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/sleep/sleep.o
sleep_stub.c:
	echo "int _crunched_sleep_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >sleep_stub.c
sleep.lo: sleep_stub.o $(sleep_OBJPATHS)
	${LD} -dc -r -o sleep.lo sleep_stub.o $(sleep_OBJPATHS)
	crunchide -k _crunched_sleep_stub sleep.lo

# -------- mv

mv_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/mv
mv_OBJS= mv.o
mv_make:
	(cd $(mv_SRCDIR); make -f Makefile $(mv_OBJS))

mv_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/mv/mv.o
mv_stub.c:
	echo "int _crunched_mv_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mv_stub.c
mv.lo: mv_stub.o $(mv_OBJPATHS)
	${LD} -dc -r -o mv.lo mv_stub.o $(mv_OBJPATHS)
	crunchide -k _crunched_mv_stub mv.lo

# -------- expr

expr_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/expr
expr_OBJS= expr.o
expr_make:
	(cd $(expr_SRCDIR); make -f Makefile $(expr_OBJS))

expr_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/expr/expr.o
expr_stub.c:
	echo "int _crunched_expr_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >expr_stub.c
expr.lo: expr_stub.o $(expr_OBJPATHS)
	${LD} -dc -r -o expr.lo expr_stub.o $(expr_OBJPATHS)
	crunchide -k _crunched_expr_stub expr.lo

# -------- test

test_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/test
test_OBJS= test.o
test_make:
	(cd $(test_SRCDIR); make -f Makefile $(test_OBJS))

test_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/test/test.o
test_stub.c:
	echo "int _crunched_test_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >test_stub.c
test.lo: test_stub.o $(test_OBJPATHS)
	${LD} -dc -r -o test.lo test_stub.o $(test_OBJPATHS)
	crunchide -k _crunched_test_stub test.lo

# -------- hostname

hostname_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../bin/hostname
hostname_OBJS= hostname.o
hostname_make:
	(cd $(hostname_SRCDIR); make -f Makefile $(hostname_OBJS))

hostname_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../bin/hostname/hostname.o
hostname_stub.c:
	echo "int _crunched_hostname_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >hostname_stub.c
hostname.lo: hostname_stub.o $(hostname_OBJPATHS)
	${LD} -dc -r -o hostname.lo hostname_stub.o $(hostname_OBJPATHS)
	crunchide -k _crunched_hostname_stub hostname.lo

# -------- mg

mg_SRCDIR=/usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg
mg_OBJS= cinfo.o fileio.o spawn.o ttyio.o tty.o ttykbd.o basic.o dir.o dired.o file.o line.o match.o paragraph.o random.o region.o search.o version.o window.o word.o buffer.o display.o echo.o extend.o help.o kbd.o keymap.o macro.o main.o modes.o re_search.o funmap.o grep.o
mg_make:
	(cd $(mg_SRCDIR); make -f Makefile $(mg_OBJS))

mg_OBJPATHS= /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/cinfo.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/fileio.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/spawn.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/ttyio.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/tty.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/ttykbd.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/basic.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/dir.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/dired.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/file.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/line.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/match.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/paragraph.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/random.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/region.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/search.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/version.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/window.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/word.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/buffer.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/display.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/echo.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/extend.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/help.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/kbd.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/keymap.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/macro.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/main.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/modes.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/re_search.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/funmap.o /usr/src/distrib/mvmeppc/ramdisk/../../../usr.bin/mg/grep.o
mg_stub.c:
	echo "int _crunched_mg_stub(int argc, char **argv, char **envp){return main(argc,argv,envp);}" >mg_stub.c
mg.lo: mg_stub.o $(mg_OBJPATHS)
	${LD} -dc -r -o mg.lo mg_stub.o $(mg_OBJPATHS)
	crunchide -k _crunched_mg_stub mg.lo

# ========
