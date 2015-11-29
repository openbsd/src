#	$OpenBSD: Pledge.pm,v 1.1 2015/11/29 19:01:27 afresh1 Exp $	#
package OpenBSD::Pledge;

use 5.020002;
use strict;
use warnings;

use parent 'Exporter';
our %EXPORT_TAGS = ( 'all' => [qw( pledge pledgenames )] );
our @EXPORT_OK   = ( @{ $EXPORT_TAGS{'all'} } );
our @EXPORT      = qw( pledge );                           ## no critic 'export'

our $VERSION = '0.01';

require XSLoader;
XSLoader::load( 'OpenBSD::Pledge', $VERSION );

sub pledge
{
	my (@promises) = @_;

	my $paths;
	$paths = pop @promises if @promises and ref $promises[-1] eq 'ARRAY';

	my %seen;
	my $promises = join q{ },
	    sort grep { !$seen{$_}++ } ( 'stdio', @promises );

	return _pledge( $promises, $paths );
}

1;

## no critic 'pod sections'
__END__

=head1 NAME

OpenBSD::Pledge - Perl interface to OpenBSD pledge(2)

=head1 SYNOPSIS

  use OpenBSD::Pledge;
  my $file = "/usr/share/dict/words";
  pledge(qw( rpath ), [$file]) || die "Unable to pledge: $!";

  open my $fh, '<', $file or die "Unable to open $file: $!\n";
  while ( readline($fh) ) {
    print if /pledge/i;
  }
  close $fh;

=head1 DESCRIPTION

This module provides a perl interface to OpenBSD's L<pledge(2)> L<syscall(2)>.

Once you promise that your program will only use certain syscalls
the kernel will kill the program if it attempts to call any other
interfaces.

=head2 EXPORT

Exports L</pledge> by default.

C<:all> will also export L</pledgenames>

=head1 METHODS

=head2 pledge(@promises, [\@paths])

With L<pledge(2)> you can promise what abilities your program will need.
You can pledge multiple times with more restrictive promises,
but abilities can never be regained.

This interface always promises C<stdio> because L<perl(1)> itself uses some of
the provided system calls.

You can supply an optional array reference of paths to be used as a whitelist,
all other paths will appear not to exist.
You may only limit the paths once.

Returns true on success, returns false and sets C<$!> on failure.

=head2 pledgenames

Returns a list of the possible promises you can pass to L</pledge>.

=head1 BUGS AND LIMITATIONS

Perl is particularly fond of C<stdio> so that promise is always added by
L</pledge>.

=head1 SEE ALSO

L<pledge(2)>

L<http://www.openbsd.org/cgi-bin/man.cgi/OpenBSD-current/man2/pledge.2>

=head1 AUTHOR

Andrew Fresh, E<lt>afresh1@OpenBSD.orgE<gt>

=head1 LICENSE AND COPYRIGHT

Copyright (C) 2015 by Andrew Fresh E<lt>afresh1@OpenBSD.orgE<gt>

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
