dnl $OpenBSD: quotes.m4,v 1.1 2001/09/29 15:49:18 espie Exp $
dnl Checking the way changequote() is supposed to work
dnl in both normal and gnu emulation mode
changequote()dnl
`No quotes left'
changequote([,])dnl
[Quotes]changequote
`No quotes left'
changequote([,])dnl
changequote(,)dnl
`No quotes left'
changequote([,])dnl
dnl same thing with comments, so first:
define([comment], [COMMENT])dnl
# this is a comment
changecom(>>)dnl
# this is a comment
changecom
# this is a comment
changecom(,)dnl
# this is a comment
