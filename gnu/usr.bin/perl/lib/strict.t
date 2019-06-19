#!./perl

chdir 't' if -d 't';
@INC = ( '.', '../lib' );

our $local_tests = 6;
require "../t/lib/common.pl";

eval qq(use strict 'garbage');
like($@, qr/^Unknown 'strict' tag\(s\) 'garbage'/);

eval qq(no strict 'garbage');
like($@, qr/^Unknown 'strict' tag\(s\) 'garbage'/);

eval qq(use strict qw(foo bar));
like($@, qr/^Unknown 'strict' tag\(s\) 'foo bar'/);

eval qq(no strict qw(foo bar));
like($@, qr/^Unknown 'strict' tag\(s\) 'foo bar'/);

eval 'use v5.12; use v5.10; ${"c"}';
is($@, '', 'use v5.10 disables implicit strict refs');

eval 'use strict; use v5.10; ${"c"}';
like($@,
    qr/^Can't use string \("c"\) as a SCALAR ref while "strict refs" in use/,
    "use v5.10 doesn't disable explicit strict ref");
