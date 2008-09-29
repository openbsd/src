#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';
}

our $pragma_name = "feature";
require "../t/lib/common.pl";
