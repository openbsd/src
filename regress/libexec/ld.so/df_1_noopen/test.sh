#!/bin/sh -ex
# $OpenBSD: test.sh,v 1.1 2012/06/15 20:50:06 matthew Exp $

export LD_LIBRARY_PATH=.
export LD_TRACE_LOADED_OBJECTS_FMT1='lib%o.so\n'
export LD_TRACE_LOADED_OBJECTS_FMT2='%o\n'

res=0

test() {
  if "$@"; then
    echo "passed"
  else
    echo "FAILED"
    res=1
  fi
}

for i in 1 2 3; do
  test ldd lib${i}.so
  test ./dlopen -lib${i}.so

  for j in 1 2 3; do
    test env LD_PRELOAD=lib${j}.so ./dlopen +lib${i}.so
    test ./dlopen${j} +lib${i}.so
  done
done

exit $res
