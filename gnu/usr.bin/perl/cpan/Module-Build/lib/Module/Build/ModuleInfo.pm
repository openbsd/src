# -*- mode: cperl; tab-width: 8; indent-tabs-mode: nil; basic-offset: 2 -*-
# vim:ts=8:sw=2:et:sta:sts=2
package Module::Build::ModuleInfo;

use strict;
use vars qw($VERSION);
$VERSION = '0.4205';
$VERSION = eval $VERSION;

require Module::Metadata;
our @ISA = qw/Module::Metadata/;

1;

__END__

=for :stopwords ModuleInfo

=head1 NAME

Module::Build::ModuleInfo - DEPRECATED

=head1 DESCRIPTION

This module has been extracted into a separate distribution and renamed
L<Module::Metadata>.  This module is kept as a subclass wrapper for
compatibility.

=head1 SEE ALSO

perl(1), L<Module::Build>, L<Module::Metadata>

=cut

