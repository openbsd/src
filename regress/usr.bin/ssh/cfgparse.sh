#	$OpenBSD: cfgparse.sh,v 1.1 2015/04/23 05:01:19 dtucker Exp $
#	Placed in the Public Domain.

tid="config parse"

verbose "reparse default config"
($SUDO ${SSHD} -T -f /dev/null >$OBJ/sshd_config.1 &&
 $SUDO ${SSHD} -T -f $OBJ/sshd_config.1 >$OBJ/sshd_config.2 &&
 diff $OBJ/sshd_config.1 $OBJ/sshd_config.2) || fail "reparse default config"

verbose "reparse regress config"
($SUDO ${SSHD} -T -f $OBJ/sshd_config >$OBJ/sshd_config.1 &&
 $SUDO ${SSHD} -T -f $OBJ/sshd_config.1 >$OBJ/sshd_config.2 &&
 diff $OBJ/sshd_config.1 $OBJ/sshd_config.2) || fail "reparse regress config"

rm -f $OBJ/sshd_config.[12]
