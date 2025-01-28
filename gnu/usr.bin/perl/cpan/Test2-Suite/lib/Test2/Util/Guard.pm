package Test2::Util::Guard;

use strict;
use warnings;

use Carp qw(confess);

our $VERSION = '0.000162';

sub new {
    confess "Can't create a Test2::Util::Guard in void context" unless (defined wantarray);

    my $class = shift;
    my $handler = shift() || die 'Test2::Util::Guard::new: no handler supplied';
    my $ref = ref $handler || '';

    die "Test2::Util::new: invalid handler - expected CODE ref, got: '$ref'"
        unless ref($handler) eq 'CODE';

    bless [ 0, $handler ], ref $class || $class;
}

sub dismiss {
    my $self = shift;
    my $dismiss = @_ ? shift : 1;

    $self->[0] = $dismiss;
}

sub DESTROY {
    my $self = shift;
    my ($dismiss, $handler) = @$self;

    $handler->() unless ($dismiss);
}

1;

__END__

=pod

=head1 NAME

Test2::Util::Guard - Inline copy of L<Scope::Guard>

=head1 SEE ALSO

See L<Scope::Guard>

=head1 ORIGINAL AUTHOR

=over 4

=item chocolateboy <chocolate@cpan.org>

=back

=head2 INLINE AND MODIFICATION AUTHOR

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright (c) 2005-2015, chocolateboy.

Modified copy is Copyright 2023 Chad Granum E<lt>exodist7@gmail.comE<gt>.

This module is free software. It may be used, redistributed and/or modified under the same terms
as Perl itself.

=cut
