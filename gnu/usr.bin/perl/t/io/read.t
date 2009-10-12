#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
eval 'use Errno';
die $@ if $@ and !$ENV{PERL_CORE_MINITEST};

plan tests => 2;

my $tmpfile = tempfile();

open(A,"+>$tmpfile");
print A "_";
seek(A,0,0);

my $b = "abcd"; 
$b = "";

read(A,$b,1,4);

close(A);

is($b,"\000\000\000\000_"); # otherwise probably "\000bcd_"

SKIP: {
    skip "no EBADF", 1 if (!exists &Errno::EBADF);

    $! = 0;
    no warnings 'unopened';
    read(B,$b,1);
    ok($! == &Errno::EBADF);
}
