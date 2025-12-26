#! perl -w

use strict;
use Test::More;
BEGIN {
  if ($^O eq 'VMS') {
    # So we can get the return value of system()
    require vmsish;
    vmsish->import;
  }
}
use ExtUtils::CBuilder;
use File::Spec;

# TEST does not like extraneous output
my $quiet = $ENV{PERL_CORE} && !$ENV{HARNESS_ACTIVE};
my ($source_file, $object_file, $lib_file);

my $b = ExtUtils::CBuilder->new(quiet => $quiet);

# test plan
if ( ! $b->have_cplusplus ) {
  plan skip_all => "no compiler available for testing";
}
else {
  plan tests => 7;
}

ok $b->have_cplusplus, "have_cplusplus";

$source_file = File::Spec->catfile('t', 'cplust.cc');
{
  open my $FH, '>', $source_file or die "Can't create $source_file: $!";
  print $FH q<namespace Bogus { extern "C" int boot_cplust() { return 1; } };> . "\n";
  close $FH;
}
ok -e $source_file, "source file '$source_file' created";

$object_file = $b->object_file($source_file);
ok 1;

is $object_file, $b->compile(source => $source_file, 'C++' => 1);

$lib_file = $b->lib_file($object_file, module_name => 'cplust');
ok 1;

my ($lib, @temps) = $b->link(objects => $object_file,
                             module_name => 'cplust');
$lib =~ tr/"'//d;
$_ = File::Spec->rel2abs($_) for $lib_file, $lib;
is $lib_file, $lib;

for ($source_file, $object_file, $lib_file) {
  tr/"'//d;
  1 while unlink;
}

if ($^O eq 'VMS') {
   1 while unlink 'CPLUST.LIS';
   1 while unlink 'CPLUST.OPT';
}

{
    # GH #23355
    local $ENV{CC};
    delete $ENV{CC};
    local $ENV{CXX};
    delete $ENV{CXX};
    # GH #23146
    my $fake_cc = File::Spec->rel2abs(File::Spec->catfile(qw(some directory what doesnt exist), 'cc'));
    my $cb = ExtUtils::CBuilder->new(
        quiet => $quiet,
        config => {
            cc => $fake_cc,
        },
    );

    is $cb->{config}{cxx}, $fake_cc, "did not search PATH for C++ compiler when given absolute path to C compiler";
}

