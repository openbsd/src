# $OpenBSD: Makefile,v 1.1 2015/06/15 09:23:12 florian Exp $

REGRESS_TARGETS=	chmod_abs_0000 chmod_abs_0644
REGRESS_TARGETS+=	chmod_sym_0644 chmod_sym_0000
REGRESS_TARGETS+=	chmod_R_0755 chmod_RP_0755 chmod_RH_0755 chmod_RL_0755
REGRESS_TARGETS+=	chmod_error

REGRESS_TARGETS+=	chgrp_id chgrp_name
REGRESS_TARGETS+=	chgrp_symlink chgrp_h_symlink
REGRESS_TARGETS+=	chgrp_R chgrp_RP chgrp_RH chgrp_RL
REGRESS_TARGETS+=	chgrp_error

REGRESS_TARGETS+=	chown_id chown_name
REGRESS_TARGETS+=	chown_symlink chown_h_symlink
REGRESS_TARGETS+=	chown_R chown_RP chown_RH chown_RL
REGRESS_TARGETS+=	chown_error

REGRESS_TARGETS+=	chflags chflags_no
REGRESS_TARGETS+=	chflags_R chflags_RP chflags_RH chflags_RL
REGRESS_TARGETS+=	chflags_error

prepfile: clean
	@/bin/mkdir -p regress
	@/usr/bin/touch regress/file
	@/bin/chmod 000 regress/file
	@[ `/usr/bin/stat -f '%p' regress/file` == '100000' ]

prepdir: clean
	@/bin/mkdir -p regress/dir
	@/usr/bin/touch regress/dir/file1
	@/usr/bin/touch regress/dir/file2
	@/bin/chmod 000 regress/dir/file1
	@/bin/chmod 000 regress/dir/file2
	@/usr/bin/touch regress/link_target_file
	@/bin/chmod 0600 regress/link_target_file
	@[ `/usr/bin/stat -f '%p' regress/dir/file1` == '100000' ]
	@[ `/usr/bin/stat -f '%p' regress/dir/file2` == '100000' ]
	@ln -s ${.OBJDIR}/regress/link_target_file \
	    ${.OBJDIR}/regress/dir/linked_file
	@ln -s ${.OBJDIR}/regress/dir ${.OBJDIR}/regress/linked_dir
	@/bin/chmod 000 regress/dir
	@[ `/usr/bin/stat -f '%p' regress/dir` == '40000' ]
	@[ `/usr/bin/stat -f '%p' regress/link_target_file` == '100600' ]

chown_prepfile: prepfile
	@[ `id -u` != 0 ] || /sbin/chown root:wheel regress/file
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/file` == 'root' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/file` == \
	    'wheel' ]

chown_prepsymlink: chown_prepfile
	@[ `id -u` != 0 ] || ln -s ${.OBJDIR}/regress/file \
	    ${.OBJDIR}/regress/symlink
	@[ `id -u` != 0 ] || /sbin/chown -h root:wheel regress/symlink
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/symlink` == \
	    'root' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/symlink` == \
	    'wheel' ]

chown_prepdir: prepdir
	@[ `id -u` != 0 ] || /bin/chmod -R 0755 regress
	@[ `id -u` != 0 ] || /sbin/chown -RL root:wheel regress

chflags_prepfile: prepfile
	@/usr/bin/chflags dump regress/file
	@[ `/usr/bin/stat -f '%f' regress/file` == '0' ]

chflags_prepdir: prepdir
	@/bin/chmod -R 0755 regress
	@/usr/bin/chflags -RL dump regress

chmod_abs_0000: prepfile
	@echo chmod abs 0000
	@/bin/chmod 0644 regress/file
	@/bin/chmod 0000 regress/file
	@[ `/usr/bin/stat -f '%p' regress/file` == '100000' ]

chmod_abs_0644: prepfile
	@echo chmod abs 0644
	@/bin/chmod 0644 regress/file
	@[ `/usr/bin/stat -f '%p' regress/file` == '100644' ]

chmod_sym_0644: prepfile
	@echo chmod sym 0644
	@/bin/chmod u=rw,go=r regress/file
	@[ `/usr/bin/stat -f '%p' regress/file` == '100644' ]

chmod_sym_0000: prepfile
	@echo chmod sym 0000
	@/bin/chmod 0644 regress/file
	@/bin/chmod a-rwx regress/file
	@[ `/usr/bin/stat -f '%p' regress/file` == '100000' ]

chmod_R_0755: prepdir
	@echo chmod -R 0755
	@/bin/chmod -R 0755 regress/dir
	@[ `/usr/bin/stat -f '%p' regress/dir/file1` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir/file2` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir` == '40755' ]
	@[ `/usr/bin/stat -f '%p' regress/link_target_file` == '100600' ]

chmod_RP_0755: prepdir
	@echo chmod -RP 0755
	@/bin/chmod -RP 0755 regress/dir
	@[ `/usr/bin/stat -f '%p' regress/dir/file1` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir/file2` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir` == '40755' ]
	@[ `/usr/bin/stat -f '%p' regress/link_target_file` == '100600' ]

chmod_RH_0755: prepdir
	@echo chmod -RH 0755
	@/bin/chmod -RH 0755 regress/linked_dir
	@[ `/usr/bin/stat -f '%p' regress/dir/file1` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir/file2` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir` == '40755' ]
	@[ `/usr/bin/stat -f '%p' regress/link_target_file` == '100600' ]

chmod_RL_0755: prepdir
	@echo chmod -RL 0755
	@/bin/chmod -RL 0755 regress/linked_dir
	@[ `/usr/bin/stat -f '%p' regress/dir/file1` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir/file2` == '100755' ]
	@[ `/usr/bin/stat -f '%p' regress/dir` == '40755' ]
	@[ `/usr/bin/stat -f '%p' regress/link_target_file` == '100755' ]

chmod_error:
	@echo chmod error
	@/bin/chmod foo bar 2> ${.OBJDIR}/chmod.error || /usr/bin/true
	@cmp -s ${.CURDIR}/chmod.error ${.OBJDIR}/chmod.error

chgrp_id: chown_prepfile
	@echo chgrp 32767
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp 32767 regress/file
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%g' regress/file` == 32767 ]

chgrp_name: chown_prepfile
	@echo chgrp nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp nobody regress/file
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/file` == \
	    'nobody' ]

chgrp_symlink: chown_prepsymlink
	@echo chgrp symlink
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp nobody regress/symlink
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/file` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/symlink` == \
	    'wheel' ]

chgrp_h_symlink: chown_prepsymlink
	@echo chgrp -h symlink
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp -h nobody regress/symlink
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/file` == \
	    'wheel' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/symlink` == \
	    'nobody' ]

chgrp_R: chown_prepdir
	@echo chown -R nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp -R nobody regress/dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' \
	    regress/link_target_file` == 'wheel' ]

chgrp_RP: chown_prepdir
	@echo chgrp -RP nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp -RP nobody regress/dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' \
	    regress/link_target_file` == 'wheel' ]

chgrp_RH: chown_prepdir
	@echo chgrp -RH nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp -RH nobody regress/linked_dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' \
	    regress/link_target_file` == 'wheel' ]

chgrp_RL: chown_prepdir
	@echo chgrp -RL nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /bin/chgrp -RL nobody regress/linked_dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Sg' \
	    regress/link_target_file` == 'nobody' ]

chgrp_error:
	@echo chgrp error
	@/bin/chgrp __hopefully_not_existing_group bar 2> \
	    ${.OBJDIR}/chgrp.error || /usr/bin/true
	@cmp -s ${.CURDIR}/chgrp.error ${.OBJDIR}/chgrp.error

chown_id: chown_prepfile
	@echo chown 32767
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown 32767 regress/file
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%u' regress/file` == 32767 ]

chown_name: chown_prepfile
	@echo chown nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown nobody regress/file
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/file` == \
	    'nobody' ]

chown_symlink: chown_prepsymlink
	@echo chown symlink
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown nobody regress/symlink
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/file` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/symlink` == \
	    'root' ]

chown_h_symlink: chown_prepsymlink
	@echo chown -h symlink
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown -h nobody regress/symlink
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/file` == \
	    'root' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/symlink` == \
	    'nobody' ]

chown_R: chown_prepdir
	@echo chown -R nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown -R nobody regress/dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' \
	    regress/link_target_file` == 'root' ]

chown_RP: chown_prepdir
	@echo chown -RP nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown -RP nobody regress/dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' \
	    regress/link_target_file` == 'root' ]

chown_RH: chown_prepdir
	@echo chown -RH nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown -RH nobody regress/linked_dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' \
	    regress/link_target_file` == 'root' ]

chown_RL: chown_prepdir
	@echo chown -RL nobody
	@[ `id -u` != 0 ] && echo skipped, not root || /usr/bin/true
	@[ `id -u` != 0 ] || /sbin/chown -RL nobody regress/linked_dir
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file1` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir/file2` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' regress/dir` == \
	    'nobody' ]
	@[ `id -u` != 0 ] || [ `/usr/bin/stat -f '%Su' \
	    regress/link_target_file` == 'nobody' ]

chown_error:
	@echo chown error
	@/sbin/chown __hopefully_not_existing_user bar 2> \
	    ${.OBJDIR}/chown.error || /usr/bin/true
	@cmp -s ${.CURDIR}/chown.error ${.OBJDIR}/chown.error

chflags: chflags_prepfile
	@echo chflags nodump
	@/usr/bin/chflags nodump regress/file
	@[ `/usr/bin/stat -f '%f' regress/file` == '1' ]

chflags_no: chflags_prepfile
	@echo chflags dump
	@/usr/bin/chflags nodump regress/file
	@[ `/usr/bin/stat -f '%f' regress/file` == '1' ]
	@/usr/bin/chflags dump regress/file
	@[ `/usr/bin/stat -f '%f' regress/file` == '0' ]

chflags_R: chflags_prepdir
	@echo chflags -R nodump
	@/usr/bin/chflags -R nodump regress/dir
	@[ `/usr/bin/stat -f '%f' regress/dir/file1` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir/file2` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/link_target_file` == '0' ]

chflags_RP: chflags_prepdir
	@echo chflags -RP nodump
	@/usr/bin/chflags -RP nodump regress/dir
	@[ `/usr/bin/stat -f '%f' regress/dir/file1` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir/file2` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/link_target_file` == '0' ]

chflags_RH: chflags_prepdir
	@echo chflags -RH nodump
	@/usr/bin/chflags -RH nodump regress/linked_dir
	@[ `/usr/bin/stat -f '%f' regress/dir/file1` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir/file2` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/link_target_file` == '0' ]

chflags_RL: chflags_prepdir
	@echo chflags -RL nodump
	@/usr/bin/chflags -RL nodump regress/linked_dir
	@[ `/usr/bin/stat -f '%f' regress/dir/file1` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir/file2` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/dir` == '1' ]
	@[ `/usr/bin/stat -f '%f' regress/link_target_file` == '1' ]

chflags_error:
	@echo chflags error
	@/usr/bin/chflags foo bar 2> ${.OBJDIR}/chflags.error || /usr/bin/true
	@cmp -s ${.CURDIR}/chflags.error ${.OBJDIR}/chflags.error

clean:
	@chmod -R 0700 regress 2>/dev/null || /usr/bin/true
	@/bin/rm -rf regress

.PHONY: ${REGRESS_TARGETS} prepfile prepdir chown_prepfile chown_prepsymlink \
	    chown_prepdir chflags_prepfile chflags_prepdir

.include <bsd.regress.mk>
