#	$OpenBSD: Unveil.pm,v 1.1 2019/07/09 20:41:54 afresh1 Exp $	#
package OpenBSD::Unveil;

use 5.028;
use strict;
use warnings;

use Carp;

use parent 'Exporter';
our %EXPORT_TAGS = ( 'all' => [qw( unveil )] );
our @EXPORT_OK   = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT      = qw( unveil );                           ## no critic 'export'

our $VERSION = '0.02';

require XSLoader;
XSLoader::load( 'OpenBSD::Unveil', $VERSION );

sub unveil
{       ## no critic 'unpack'
	croak("Usage: OpenBSD::Unveil::unveil([path, permissions])")
	    unless @_ == 0 || @_ == 2; ## no critic 'postfix'
	return _unveil(@_);
}

1;

## no critic 'pod sections'
__END__

=head1 NAME

OpenBSD::Unveil - Perl interface to OpenBSD unveil(2)

=head1 SYNOPSIS

  use OpenBSD::Unveil;

  my $file = "/usr/share/dict/words";
  unveil( $file, "r" ) || die "Unable to unveil: $!";
  unveil() || die "Unable to lock unveil: $!";
  open my $fh, '<', $file or die "Unable to open $file: $!";

  print grep { /unveil/i } readline($fh);
  close $fh;


=head1 DESCRIPTION

This module provides a perl interface to OpenBSD's L<unveil(2)> L<syscall(2)>.

=head1 EXPORT

Exports L</unveil> by default.

=head1 FUNCTIONS

=head2 unveil

Perl interface to L<unveil(2)>.

	unveil($paths, $permissions)
	unveil() # to lock

Returns true on success, returns false and sets $! on failure.
Throws an exception on incorrect number of parameters.

=head1 SEE ALSO

L<unveil(2)>

L<http://man.openbsd.org/unveil.2>

=head1 AUTHOR

Andrew Fresh, E<lt>afresh1@OpenBSD.orgE<gt>

=head1 LICENSE AND COPYRIGHT

Copyright (C) 2019 by Andrew Fresh E<lt>afresh1@OpenBSD.orgE<gt>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

=cut
