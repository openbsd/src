#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib/');
    }
    else {
        unshift @INC, 't/lib/';
    }
}
chdir 't';

use Test::More tests => 3;

mkdir('hints', 0777);
my $hint_file = "hints/$^O.pl";
open(HINT, ">$hint_file") || die "Can't write dummy hints file $hint_file: $!";
print HINT <<'CLOO';
$self->{CCFLAGS} = 'basset hounds got long ears';
CLOO
close HINT;

use TieOut;
use ExtUtils::MakeMaker;

my $out = tie *STDERR, 'TieOut';
my $mm = bless {}, 'ExtUtils::MakeMaker';
$mm->check_hints;
is( $mm->{CCFLAGS}, 'basset hounds got long ears' );
is( $out->read, "Processing hints file $hint_file\n" );

open(HINT, ">$hint_file") || die "Can't write dummy hints file $hint_file: $!";
print HINT <<'CLOO';
die "Argh!\n";
CLOO
close HINT;

$mm->check_hints;
is( $out->read, <<OUT, 'hint files produce errors' );
Processing hints file $hint_file
Argh!
OUT

END {
    use File::Path;
    rmtree ['hints'];
}
