.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root
BINMODE=4555
BINDIR=	/usr/bin
LDADD=	-lkrb -lcrypto -ldes -lgmp -lutil -lz
MAN=	ssh.1

SRCS=	ssh.c sshconnect.c log-client.c readconf.c hostfile.c readpass.c \
	tildexpand.c uidswap.c clientloop.c canohost.c rsa.c randoms.c \
	ssh_md5.c buffer.c packet.c xmalloc.c ttymodes.c channels.c bufaux.c \
	authfd.c authfile.c crc32.c rsaglue.c match.c mpaux.c minfd.c cipher.c \
	compress.c

gen_minfd: gen_minfd.c
minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

########################

# Creates /etc/ssh_host_key
generate-host-key:
	-@if test -f $(install_prefix)$(HOST_KEY_FILE); \
          then echo "You already have a host key in $(install_prefix)$(HOST_KEY_FILE)."; \
	  else \
	    umask 022; echo "Generating 1024 bit host key."; \
	    ./ssh-keygen -b 1024 -f $(install_prefix)$(HOST_KEY_FILE) -N ''; \
	  fi

# Ssh is much to large and hairy to be installed suid root by
# default. Disabled for now/bg. If you really need rhosts
# authentication do a manual chmod u+s $(install_prefix)$(bindir)/ssh.
#
# Ssh is preferably installed suid root.  It can also be used non-root,
# but then it cannot connect from a privileged socket, and rhosts
# authentication will be disabled.
#
# Sshd is not suid root, but should preferably be run as root
# (otherwise it can only log in as the user it runs as, and must be
# bound to a non-privileged port).  Also, password authentication may
# not be available if non-root and using shadow passwords.
fuck-install: $(PROGRAMS) make-dirs generate-host-key
	$(INSTALL_PROGRAM) -o root -m 0755 ssh $(install_prefix)$(bindir)/ssh
	-if test "`echo ssh | sed '$(transform)'`" '!=' ssh; then \
	  rm -f $(install_prefix)$(bindir)/`echo ssh | sed '$(transform)'`; \
	  $(LN_S) ssh \
	    $(install_prefix)$(bindir)/`echo ssh | sed '$(transform)'`; fi
	rm -f $(install_prefix)$(bindir)/slogin
	$(LN_S) ssh $(install_prefix)$(bindir)/slogin
	-if test "`echo slogin | sed '$(transform)'`" '!=' slogin; then \
	  rm -f $(install_prefix)$(bindir)/`echo slogin | sed '$(transform)'`;\
	  $(LN_S) ssh \
	    $(install_prefix)$(bindir)/`echo slogin | sed '$(transform)'`; fi
	-for p in $(NORMAL_PROGRAMS); do \
	  $(INSTALL_PROGRAM) -m 0755 $$p $(install_prefix)$(bindir)/$$p; \
	  if test "`echo $$p | sed '$(transform)'`" '!=' $$p; then \
	    rm -f $(install_prefix)$(bindir)/`echo $$p | sed '$(transform)'`; \
	    $(LN_S) $$p \
	      $(install_prefix)$(bindir)/`echo $$p | sed '$(transform)'`; fi; \
	done
# Remove $(etcdir)/sshd_pid as it is now $(bindir)/sshd.pid
	-rm -f $(install_prefix)$(etcdir)/sshd_pid
# Remove $(bindir)/sshd to avoid confusion since it is now in $(sbindir)
	-rm -f $(install_prefix)$(bindir)/sshd
	-rm -f $(install_prefix)$(bindir)/`echo sshd | sed '$(transform)'`
	-for p in $(SBIN_PROGRAMS); do \
	  $(INSTALL_PROGRAM) -m 0755 $$p $(install_prefix)$(sbindir)/$$p; \
	  if test "`echo $$p | sed '$(transform)'`" '!=' $$p; then \
	    rm -f $(install_prefix)$(sbindir)/`echo $$p | sed '$(transform)'`;\
	    $(LN_S) $$p \
	      $(install_prefix)$(sbindir)/`echo $$p | sed '$(transform)'`; fi;\
	done
	-for p in $(MAN1PAGES); do \
	  $(INSTALL_DATA) -m 0644 $(srcdir)/$$p $(install_prefix)$(man1dir)/$$p ; \
	  if test "`echo $$p | sed '$(transform)'`" '!=' $$p; then \
	    rm -f $(install_prefix)$(man1dir)/`echo $$p | sed '$(transform)'`;\
	    $(LN_S) $$p \
	      $(install_prefix)$(man1dir)/`echo $$p | sed '$(transform)'`; \
	  fi; \
	done
	rm -f $(install_prefix)$(man1dir)/slogin.1
	$(LN_S) ssh.1 $(install_prefix)$(man1dir)/slogin.1
	if test "`echo slogin.1 | sed '$(transform)'`" '!=' slogin.1; then \
	  rm -f $(install_prefix)$(man1dir)/`echo slogin.1 | sed '$(transform)'`;\
	  $(LN_S) ssh.1 \
	    $(install_prefix)$(man1dir)/`echo slogin.1 | sed '$(transform)'`; \
	fi
	-for p in $(MAN1GENERATED); do \
	  $(INSTALL_DATA) -m 0644 $$p $(install_prefix)$(man1dir)/$$p ; \
	  if test "`echo $$p | sed '$(transform)'`" '!=' $$p; then \
	    rm -f $(install_prefix)$(man1dir)/`echo $$p | sed '$(transform)'`;\
	    $(LN_S) $$p \
	      $(install_prefix)$(man1dir)/`echo $$p | sed '$(transform)'`; \
	  fi; \
	done
	-for p in $(MAN8GENERATED); do \
	  $(INSTALL_DATA) -m 0644 $$p $(install_prefix)$(man8dir)/$$p; \
	  if test "`echo $$p | sed '$(transform)'`" '!=' $$p; then \
	    rm -f $(install_prefix)$(man8dir)/`echo $$p | sed '$(transform)'`;\
	    $(LN_S) $$p \
	      $(install_prefix)$(man8dir)/`echo $$p | sed '$(transform)'`; fi;\
	done
	-if test '!' -f $(install_prefix)$(HOST_CONFIG_FILE); then \
	  $(INSTALL_DATA) -m 0644 $(srcdir)/host_config.sample \
	    $(install_prefix)$(HOST_CONFIG_FILE); fi
	-if test '!' -f $(install_prefix)$(SERVER_CONFIG_FILE); then \
	  cat $(srcdir)/server_config.sample | \
	  sed "s#_ETCDIR_#$(etcdir)#g" >/tmp/ssh_inst.$$; \
	  $(INSTALL_DATA) -m 0644 /tmp/ssh_inst.$$ \
	    $(install_prefix)$(SERVER_CONFIG_FILE); \
	  rm -f /tmp/ssh_inst.$$; fi

.include<bsd.prog.mk>
