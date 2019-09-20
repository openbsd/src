#	$OpenBSD: Makefile,v 1.2 2019/09/20 12:45:43 bluhm Exp $

# The regression test binary must be run on a wxallowed filesystem
# to verify correct behaviour (it should crash)

.if ! (make(clean) || make(cleandir) || make(obj))

MOUNT_OBJ !!= mount | grep ^$$(df -P . | tail -1 | awk '{ print $$1 }')

.if "${MOUNT_OBJ:M*wxallowed*}" == ""
regress:
	echo '${MOUNT_OBJ}'
	echo object directory needs mount option wxallowed
	echo SKIPPED
.endif

.endif

PROG=		wx_syscall
LDFLAGS=	-z wxneeded

run-regress-wx_syscall: wx_syscall
	! ./wx_syscall

.include <bsd.regress.mk>
