#! perl -w

BEGIN {
  if ($ENV{PERL_CORE}) {
    chdir 't' if -d 't';
    chdir '../lib/ExtUtils/CBuilder'
      or die "Can't chdir to lib/ExtUtils/CBuilder: $!";
    @INC = qw(../..);
  }
}

use strict;
use Test;
BEGIN { plan tests => 11 }

use ExtUtils::CBuilder;
use File::Spec;
ok 1;

# TEST doesn't like extraneous output
my $quiet = $ENV{PERL_CORE} && !$ENV{HARNESS_ACTIVE};

my $b = ExtUtils::CBuilder->new(quiet => $quiet);
ok $b;

ok $b->have_compiler;

my $source_file = File::Spec->catfile('t', 'compilet.c');
{
  local *FH;
  open FH, "> $source_file" or die "Can't create $source_file: $!";
  print FH "int boot_compilet(void) { return 1; }\n";
  close FH;
}
ok -e $source_file;

my $object_file = $b->object_file($source_file);
ok 1;

ok $object_file, $b->compile(source => $source_file);

my $lib_file = $b->lib_file($object_file);
ok 1;

my ($lib, @temps) = $b->link(objects => $object_file,
                             module_name => 'compilet');
$lib =~ tr/"'//d;
ok $lib_file, $lib;

for ($source_file, $object_file, $lib_file) {
  tr/"'//d;
  1 while unlink;
}

my @words = $b->split_like_shell(' foo bar');
ok @words, 2;
ok $words[0], 'foo';
ok $words[1], 'bar';
