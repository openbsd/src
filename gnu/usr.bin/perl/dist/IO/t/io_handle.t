#!./perl -w

use strict;
require($ENV{PERL_CORE} ? "../../t/test.pl" : "./t/test.pl");

plan(tests => 2);

use_ok("IO::Handle");
my $io = IO::Handle->new;

can_ok($io, qw(fdopen close opened fileno getc eof print printf say truncate read),
            qw(sysread write syswrite stat autoflush binmode fcntl ioctl));
