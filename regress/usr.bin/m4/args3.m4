dnl invalid number of parameters for doifelse
dnl do not segfault
ifelse(A, "s",,,)
