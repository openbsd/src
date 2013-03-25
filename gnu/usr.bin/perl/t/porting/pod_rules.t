#!./perl

BEGIN {
    chdir '..' unless -d 't';
    unshift @INC, 'lib';
}

use strict;
require 't/test.pl';

my $result = runperl(switches => ['-f', '-Ilib'], 
                     progfile => 'Porting/pod_rules.pl',
                     args     => ['--tap']);

print $result;
