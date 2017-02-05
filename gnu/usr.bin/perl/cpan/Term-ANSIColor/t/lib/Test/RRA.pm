# Helper functions for test programs written in Perl.
#
# This module provides a collection of helper functions used by test programs
# written in Perl.  This is a general collection of functions that can be used
# by both C packages with Automake and by stand-alone Perl modules.  See
# Test::RRA::Automake for additional functions specifically for C Automake
# distributions.
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2013, 2014
#     The Board of Trustees of the Leland Stanford Junior University
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

package Test::RRA;

use 5.006;
use strict;
use warnings;

use Exporter;
use Test::More;

# For Perl 5.006 compatibility.
## no critic (ClassHierarchies::ProhibitExplicitISA)

# Declare variables that should be set in BEGIN for robustness.
our (@EXPORT_OK, @ISA, $VERSION);

# Set $VERSION and everything export-related in a BEGIN block for robustness
# against circular module loading (not that we load any modules, but
# consistency is good).
BEGIN {
    @ISA       = qw(Exporter);
    @EXPORT_OK = qw(skip_unless_author skip_unless_automated use_prereq);

    # This version should match the corresponding rra-c-util release, but with
    # two digits for the minor version, including a leading zero if necessary,
    # so that it will sort properly.
    $VERSION = '5.04';
}

# Skip this test unless author tests are requested.  Takes a short description
# of what tests this script would perform, which is used in the skip message.
# Calls plan skip_all, which will terminate the program.
#
# $description - Short description of the tests
#
# Returns: undef
sub skip_unless_author {
    my ($description) = @_;
    if (!$ENV{AUTHOR_TESTING}) {
        plan skip_all => "$description only run for author";
    }
    return;
}

# Skip this test unless doing automated testing or release testing.  This is
# used for tests that should be run by CPAN smoke testing or during releases,
# but not for manual installs by end users.  Takes a short description of what
# tests this script would perform, which is used in the skip message.  Calls
# plan skip_all, which will terminate the program.
#
# $description - Short description of the tests
#
# Returns: undef
sub skip_unless_automated {
    my ($description) = @_;
    for my $env (qw(AUTOMATED_TESTING RELEASE_TESTING AUTHOR_TESTING)) {
        return if $ENV{$env};
    }
    plan skip_all => "$description normally skipped";
    return;
}

# Attempt to load a module and skip the test if the module could not be
# loaded.  If the module could be loaded, call its import function manually.
# If the module could not be loaded, calls plan skip_all, which will terminate
# the program.
#
# The special logic here is based on Test::More and is required to get the
# imports to happen in the caller's namespace.
#
# $module  - Name of the module to load
# @imports - Any arguments to import, possibly including a version
#
# Returns: undef
sub use_prereq {
    my ($module, @imports) = @_;

    # If the first import looks like a version, pass it as a bare string.
    my $version = q{};
    if (@imports >= 1 && $imports[0] =~ m{ \A \d+ (?: [.][\d_]+ )* \z }xms) {
        $version = shift(@imports);
    }

    # Get caller information to put imports in the correct package.
    my ($package) = caller;

    # Do the import with eval, and try to isolate it from the surrounding
    # context as much as possible.  Based heavily on Test::More::_eval.
    ## no critic (BuiltinFunctions::ProhibitStringyEval)
    ## no critic (ValuesAndExpressions::ProhibitImplicitNewlines)
    my ($result, $error, $sigdie);
    {
        local $@            = undef;
        local $!            = undef;
        local $SIG{__DIE__} = undef;
        $result = eval qq{
            package $package;
            use $module $version \@imports;
            1;
        };
        $error = $@;
        $sigdie = $SIG{__DIE__} || undef;
    }

    # If the use failed for any reason, skip the test.
    if (!$result || $error) {
        my $name = length($version) > 0 ? "$module $version" : $module;
        plan skip_all => "$name required for test";
    }

    # If the module set $SIG{__DIE__}, we cleared that via local.  Restore it.
    ## no critic (Variables::RequireLocalizedPunctuationVars)
    if (defined($sigdie)) {
        $SIG{__DIE__} = $sigdie;
    }
    return;
}

1;
__END__

=for stopwords
Allbery Allbery's DESC bareword sublicense MERCHANTABILITY NONINFRINGEMENT
rra-c-util

=head1 NAME

Test::RRA - Support functions for Perl tests

=head1 SYNOPSIS

    use Test::RRA
      qw(skip_unless_author skip_unless_automated use_prereq);

    # Skip this test unless author tests are requested.
    skip_unless_author('Coding style tests');

    # Skip this test unless doing automated or release testing.
    skip_unless_automated('POD syntax tests');

    # Load modules, skipping the test if they're not available.
    use_prereq('Perl6::Slurp', 'slurp');
    use_prereq('Test::Script::Run', '0.04');

=head1 DESCRIPTION

This module collects utility functions that are useful for Perl test
scripts.  It assumes Russ Allbery's Perl module layout and test
conventions and will only be useful for other people if they use the
same conventions.

=head1 FUNCTIONS

None of these functions are imported by default.  The ones used by a
script should be explicitly imported.

=over 4

=item skip_unless_author(DESC)

Checks whether AUTHOR_TESTING is set in the environment and skips the
whole test (by calling C<plan skip_all> from Test::More) if it is not.
DESC is a description of the tests being skipped.  A space and C<only run
for author> will be appended to it and used as the skip reason.

=item skip_unless_automated(DESC)

Checks whether AUTHOR_TESTING, AUTOMATED_TESTING, or RELEASE_TESTING are
set in the environment and skips the whole test (by calling C<plan
skip_all> from Test::More) if they are not.  This should be used by tests
that should not run during end-user installs of the module, but which
should run as part of CPAN smoke testing and release testing.

DESC is a description of the tests being skipped.  A space and C<normally
skipped> will be appended to it and used as the skip reason.

=item use_prereq(MODULE[, VERSION][, IMPORT ...])

Attempts to load MODULE with the given VERSION and import arguments.  If
this fails for any reason, the test will be skipped (by calling C<plan
skip_all> from Test::More) with a skip reason saying that MODULE is
required for the test.

VERSION will be passed to C<use> as a version bareword if it looks like a
version number.  The remaining IMPORT arguments will be passed as the
value of an array.

=back

=head1 AUTHOR

Russ Allbery <eagle@eyrie.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2013, 2014 The Board of Trustees of the Leland Stanford Junior
University

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=head1 SEE ALSO

Test::More(3), Test::RRA::Automake(3), Test::RRA::Config(3)

This module is maintained in the rra-c-util package.  The current version
is available from L<http://www.eyrie.org/~eagle/software/rra-c-util/>.

The functions to control when tests are run use environment variables
defined by the L<Lancaster
Consensus|https://github.com/Perl-Toolchain-Gang/toolchain-site/blob/master/lancaster-consensus.md>.

=cut
