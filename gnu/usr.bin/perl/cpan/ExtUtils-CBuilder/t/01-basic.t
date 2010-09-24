#! perl -w

use strict;
use Test::More;
BEGIN { 
  if ($^O eq 'VMS') {
    # So we can get the return value of system()
    require vmsish;
    import vmsish;
  }
}
use ExtUtils::CBuilder;
use File::Spec;

# TEST doesn't like extraneous output
my $quiet = $ENV{PERL_CORE} && !$ENV{HARNESS_ACTIVE};
my ($source_file, $object_file, $lib_file);

my $b = ExtUtils::CBuilder->new(quiet => $quiet);

# test plan
if ( ! $b->have_compiler ) {
  plan skip_all => "no compiler available for testing";
}
else {
  plan tests => 10;
}

ok $b, "created EU::CB object";

ok $b->have_compiler, "have_compiler";

$source_file = File::Spec->catfile('t', 'compilet.c');
{
  local *FH;
  open FH, "> $source_file" or die "Can't create $source_file: $!";
  print FH "int boot_compilet(void) { return 1; }\n";
  close FH;
}
ok -e $source_file, "source file '$source_file' created";

$object_file = $b->object_file($source_file);
ok 1;

is $object_file, $b->compile(source => $source_file);

$lib_file = $b->lib_file($object_file);
ok 1;

my ($lib, @temps) = $b->link(objects => $object_file,
                             module_name => 'compilet');
$lib =~ tr/"'//d;
is $lib_file, $lib;

for ($source_file, $object_file, $lib_file) {
  tr/"'//d;
  1 while unlink;
}

if ($^O eq 'VMS') {
   1 while unlink 'COMPILET.LIS';
   1 while unlink 'COMPILET.OPT';
}

my @words = $b->split_like_shell(' foo bar');

SKIP: {
  skip "MSWindows", 3 if $^O =~ m/MSWin/;
  is( @words, 2 );
  is( $words[0], 'foo' );
  is( $words[1], 'bar' );
}
