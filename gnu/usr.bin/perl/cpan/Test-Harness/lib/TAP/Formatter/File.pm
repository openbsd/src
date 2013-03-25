package TAP::Formatter::File;

use strict;
use TAP::Formatter::Base ();
use TAP::Formatter::File::Session;
use POSIX qw(strftime);

use vars qw($VERSION @ISA);

@ISA = qw(TAP::Formatter::Base);

=head1 NAME

TAP::Formatter::File - Harness output delegate for file output

=head1 VERSION

Version 3.23

=cut

$VERSION = '3.23';

=head1 DESCRIPTION

This provides file orientated output formatting for TAP::Harness.

=head1 SYNOPSIS

 use TAP::Formatter::File;
 my $harness = TAP::Formatter::File->new( \%args );

=head2 C<< open_test >>

See L<TAP::Formatter::base>

=cut

sub open_test {
    my ( $self, $test, $parser ) = @_;

    my $session = TAP::Formatter::File::Session->new(
        {   name      => $test,
            formatter => $self,
            parser    => $parser,
        }
    );

    $session->header;

    return $session;
}

sub _should_show_count {
    return 0;
}

1;
