#!/bin/sh

trap 'rm -f *.*_tmp' EXIT

ltypes="bullet dash enum hyphen item tag diag hang ohang inset column"
atypes="compact width offset"
for list in $ltypes; do
  for arg in $atypes; do
    [ "X$arg" = "Xcompact" ] && arg2="" || arg2="Ds"
    ${MANDOC} ${MANDOCOPTS} > ENOTYPE.out_tmp 2> ENOTYPE.err_tmp << __EOF__;
.Dd \$Mdocdate: October 28 2009 $
.Dt ENOTYPE 1
.Os
.Sh NAME
.Nm ENOTYPE
.Nd list argument before list type
.Sh DESCRIPTION
.Bl -${arg} ${arg2} -${list}
.El
__EOF__
    if [ $? -ne 0 ]; then
      echo ".Bl -$arg -$list failed"
      cat ENOTYPE.err_tmp
      exit 1
    fi
    if ! grep -q 'list arguments preceding type' ENOTYPE.err_tmp; then
      echo ".Bl -$arg -$list did not throw ENOTYPE"
      exit 1
    fi
  done
done

exit 0
