#	$OpenBSD: Makefile,v 1.3 1999/09/26 22:29:50 deraadt Exp $

.include <bsd.own.mk>

SUBDIR= ssh sshd ssh-add ssh-keygen ssh-agent scp

generate-host-key:
	-@if [ -f ${DESTDIR}/etc/ssh_host_key ]; then \
		echo "Host key exists in ${DESTDIR}/etc/ssh_host_key."; \
	else \
		umask 022; echo "Generating 1024 bit host key."; \
		ssh-keygen -b 1024 -f ${DESTDIR}/etc/ssh_host_key -N ''; \
	fi

distribution:
	install -C -o root -g wheel -m 0644 ${.CURDIR}/etc/ssh_config /etc
	install -C -o root -g wheel -m 0644 ${.CURDIR}/etc/sshd_config /etc

.include <bsd.subdir.mk>
