#!./perl
use strict;
require './test.pl';

$^I = $^O eq 'VMS' ? '_bak' : '.bak';

plan( tests => 2 );

my @tfiles     = ('.a','.b','.c');
my @tfiles_bak = (".a$^I", ".b$^I", ".c$^I");

END { unlink_all('.a','.b','.c',".a$^I", ".b$^I", ".c$^I"); }

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

