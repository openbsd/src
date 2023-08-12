#!/bin/sh -ex
# $OpenBSD: test.sh,v 1.1 2023/08/12 13:43:22 gnezdo Exp $

res=0

test() {
  if eval "$@"; then
    echo "passed"
  else
    echo "FAILED"
    res=1
  fi
}

test "ldd empty 2>&1 | grep 'incomplete ELF header'"
test "ldd short 2>&1 | grep 'incomplete program header'"

exit $res
