dnl $OpenBSD: fibo.m4,v 1.1 2000/07/01 00:49:07 espie Exp $
define(`copy', `$1')dnl
define(`fibo',dnl
`ifelse($1,0,`a',dnl
$1,1,`b',dnl
`copy(fibo(decr($1)))`'copy(fibo(decr(decr($1))))')')dnl
fibo(13)
