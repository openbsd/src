# $OpenBSD: test.sh,v 1.1 2003/07/01 05:51:31 niklas Exp $

msg=`LD_LIBRARY_PATH=lib $1`
case $2 in
%ERROR%)
  test $? -ne 0;;
*)
  test X"$msg" = X"$2"
esac
exit $?
