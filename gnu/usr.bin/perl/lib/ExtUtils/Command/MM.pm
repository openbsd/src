package ExtUtils::Command::MM;

use strict;

require 5.005_03;
require Exporter;
use vars qw($VERSION @ISA @EXPORT);
@ISA = qw(Exporter);

@EXPORT = qw(test_harness);
$VERSION = '0.01';

=head1 NAME

ExtUtils::Command::MM - Commands for the MM's to use in Makefiles

=head1 SYNOPSIS

  perl -MExtUtils::Command::MM -e "function" files...


=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>  The interface is not stable.

ExtUtils::Command::MM encapsulates code which would otherwise have to
be done with large "one" liners.

They all read their input from @ARGV unless otherwise noted.

Any $(FOO) used in the examples are make variables, not Perl.

=over 4

=item B<test_harness>

  test_harness($verbose, @test_libs);

Runs the tests on @ARGV via Test::Harness passing through the $verbose
flag.  Any @test_libs will be unshifted onto the test's @INC.

@test_libs are run in alphabetical order.

=cut

sub test_harness {
    require Test::Harness;
    require File::Spec;

    $Test::Harness::verbose = shift;

    local @INC = @INC;
    unshift @INC, map { File::Spec->rel2abs($_) } @_;
    Test::Harness::runtests(sort { lc $a cmp lc $b } @ARGV);
}

=back

=cut

1;
