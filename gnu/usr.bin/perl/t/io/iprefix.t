#!./perl
use strict;
require './test.pl';

$^I = 'bak*';

# Modified from the original inplace.t to test adding prefixes

plan( tests => 2 );

my @tfiles     = ('.a','.b','.c');
my @tfiles_bak = ('bak.a', 'bak.b', 'bak.c');

END { unlink_all('.a','.b','.c', 'bak.a', 'bak.b', 'bak.c'); }

for my $file (@tfiles) {
    runperl( prog => 'print qq(foo\n);', 
             args => ['>', $file] );
}

@ARGV = @tfiles;

while (<>) {
    s/foo/bar/;
}
continue {
    print;
}

is ( runperl( prog => 'print<>;', args => \@tfiles ),
     "bar\nbar\nbar\n", 
     "file contents properly replaced" );

is ( runperl( prog => 'print<>;', args => \@tfiles_bak ), 
     "foo\nfoo\nfoo\n", 
     "backup file contents stay the same" );

