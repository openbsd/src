package ExtUtils::MM_Any;

use strict;
use vars qw($VERSION @ISA);
$VERSION = 0.04;

use Config;
use File::Spec;


=head1 NAME

ExtUtils::MM_Any - Platform agnostic MM methods

=head1 SYNOPSIS

  FOR INTERNAL USE ONLY!

  package ExtUtils::MM_SomeOS;

  # Temporarily, you have to subclass both.  Put MM_Any first.
  require ExtUtils::MM_Any;
  require ExtUtils::MM_Unix;
  @ISA = qw(ExtUtils::MM_Any ExtUtils::Unix);

=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>

ExtUtils::MM_Any is a superclass for the ExtUtils::MM_* set of
modules.  It contains methods which are either inherently
cross-platform or are written in a cross-platform manner.

Subclass off of ExtUtils::MM_Any I<and> ExtUtils::MM_Unix.  This is a
temporary solution.

B<THIS MAY BE TEMPORARY!>

=head1 Inherently Cross-Platform Methods

These are methods which are by their nature cross-platform and should
always be cross-platform.

=head2 File::Spec wrappers  B<DEPRECATED>

The following methods are deprecated wrappers around File::Spec
functions.  They exist from before File::Spec did and in fact are from
which File::Spec sprang.

They are all deprecated.  Please use File::Spec directly.

=over 4

=item canonpath

=cut

sub canonpath {
    shift;
    return File::Spec->canonpath(@_);;
}

=item catdir

=cut

sub catdir {
    shift;
    return File::Spec->catdir(@_);
}

=item catfile

=cut

sub catfile {
    shift;
    return File::Spec->catfile(@_);
}

=item curdir

=cut

my $Curdir = File::Spec->curdir;
sub curdir {
    return $Curdir;
}

=item file_name_is_absolute

=cut

sub file_name_is_absolute {
    shift;
    return File::Spec->file_name_is_absolute(@_);
}

=item path

=cut

sub path {
    return File::Spec->path();
}

=item rootdir

=cut

my $Rootdir = File::Spec->rootdir;
sub rootdir {
    return $Rootdir;
}

=item updir

=cut

my $Updir = File::Spec->updir;
sub updir {
    return $Updir;
}

=back

=head1 Thought To Be Cross-Platform Methods

These are methods which are thought to be cross-platform by virtue of
having been written in a way to avoid incompatibilities.

=over 4

=item test_via_harness

  my $command = $mm->test_via_harness($perl, $tests);

Returns a $command line which runs the given set of $tests with
Test::Harness and the given $perl.

Used on the t/*.t files.

=cut

sub test_via_harness {
    my($self, $perl, $tests) = @_;

    return qq{\t$perl "-MExtUtils::Command::MM" }.
           qq{"-e" "test_harness(\$(TEST_VERBOSE), '\$(INST_LIB)', '\$(INST_ARCHLIB)')" $tests\n};
}

=item test_via_script

  my $command = $mm->test_via_script($perl, $script);

Returns a $command line which just runs a single test without
Test::Harness.  No checks are done on the results, they're just
printed.

Used for test.pl, since they don't always follow Test::Harness
formatting.

=cut

sub test_via_script {
    my($self, $perl, $script) = @_;
    return qq{\t$perl "-I\$(INST_LIB)" "-I\$(INST_ARCHLIB)" $script\n};
}

=back

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> with code from ExtUtils::MM_Unix
and ExtUtils::MM_Win32.


=cut

1;
