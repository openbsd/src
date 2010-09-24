#! perl -w

use File::Spec;
my $perl;
BEGIN {
  $perl = File::Spec->rel2abs($^X);
}

use strict;
use Test::More;
BEGIN { 
  if ($^O eq 'VMS') {
    # So we can get the return value of system()
    require vmsish;
    import vmsish;
  }
}

plan tests => 6;

require_ok "ExtUtils::CBuilder";

my $b = eval { ExtUtils::CBuilder->new(quiet => 1) };
ok( $b, "got CBuilder object" ) or diag $@;

my $bogus_path = 'djaadjfkadjkfajdf';
my $run_perl = "$perl -e1 --";
# test missing compiler
$b->{config}{cc} = $bogus_path;
$b->{config}{ld} = $bogus_path;

$b->{have_compiler} = undef;
is( $b->have_compiler, 0, "have_compiler: fake missing cc" );
$b->{have_compiler} = undef;
is( $b->have_cplusplus, 0, "have_cplusplus: fake missing c++" );

# test found compiler
$b->{config}{cc} = $run_perl;
$b->{config}{ld} = $run_perl;
$b->{have_compiler} = undef;
is( $b->have_compiler, 1, "have_compiler: fake present cc" );
$b->{have_compiler} = undef;
is( $b->have_cplusplus, 1, "have_cpp_compiler: fake present c++" );

# test missing cpp compiler
