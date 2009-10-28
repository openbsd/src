#!/bin/sh

trap 'rm -f *.*_tmp' EXIT

ltypes="bullet dash enum hyphen item tag diag hang ohang inset column"

for first in $ltypes; do
  for second in $ltypes; do
    ${MANDOC} > EMULTILIST.out_tmp 2> EMULTILIST.err_tmp << __EOF__;
.Dd \$Mdocdate: October 28 2009 $
.Dt EMULTILIST 1
.Os
.Sh NAME
.Nm EMULTILIST
.Nd list of multiple types
.Sh DESCRIPTION
.Bl -${first} -${second}
.El
__EOF__
    if [ $? -eq 0 ]; then
      echo ".Bl -$first -$second failed to fail"
      exit 1
    fi
    if [ -s EMULTILIST.out_tmp ]; then
      echo ".Bl -$first -$second produced output"
      cat EMULTILIST.out_tmp
      exit 1
    fi
    if ! grep -q 'too many list types' EMULTILIST.err_tmp; then
      echo ".Bl -$first -$second did not throw EMULTILIST"
      exit 1
    fi
    if grep -qv 'too many list types' EMULTILIST.err_tmp; then
      echo ".Bl -$first -$second returned unexpected error"
      cat EMULTILIST.err_tmp
    fi
  done
done

exit 0
