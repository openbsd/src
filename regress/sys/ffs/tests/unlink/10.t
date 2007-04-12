#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/10.t,v 1.1 2007/01/17 01:42:12 pjd Exp $

desc="unlink returns EPERM if the parent directory of the named file has its immutable or append-only flag set"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755

expect 0 create ${n0}/${n1} 0644
expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM unlink ${n0}/${n1}
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 create ${n0}/${n1} 0644
expect 0 chflags ${n0} UF_IMMUTABLE
expect EPERM unlink ${n0}/${n1}
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 create ${n0}/${n1} 0644
expect 0 chflags ${n0} SF_APPEND
expect EPERM unlink ${n0}/${n1}
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 create ${n0}/${n1} 0644
expect 0 chflags ${n0} UF_APPEND
expect EPERM unlink ${n0}/${n1}
expect 0 chflags ${n0} none
expect 0 unlink ${n0}/${n1}

expect 0 rmdir ${n0}
