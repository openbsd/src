package Module::Build::Version;
use strict;
use vars qw($VERSION);
$VERSION = '0.87'; ### XXX sync with version of version.pm below

use version 0.87;
our @ISA = qw(version);

1;

=head1 NAME

Module::Build::Version - DEPRECATED

=head1 DESCRIPTION

Module::Build now lists L<version> as a C<configure_requires> dependency
and no longer installs a copy.

=cut

