# gcc -O3 (and -O2) get overly excited over B.c in OpenBSD 3.3/sparc 64
$self->{OPTIMIZE} = '-O1' if $Config{ARCH} eq 'sparc64';
