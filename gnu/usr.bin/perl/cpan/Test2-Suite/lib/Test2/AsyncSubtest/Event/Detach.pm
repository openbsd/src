package Test2::AsyncSubtest::Event::Detach;
use strict;
use warnings;

our $VERSION = '0.000162';

use base 'Test2::Event';
use Test2::Util::HashBase qw/id/;

sub no_display { 1 }

sub callback {
    my $self = shift;
    my ($hub) = @_;

    my $id = $self->{+ID};
    my $ids = $hub->ast_ids;

    unless (defined $ids->{$id}) {
        require Test2::Event::Exception;
        my $trace = $self->trace;
        $hub->send(
            Test2::Event::Exception->new(
                trace => $trace,
                error => "Invalid AsyncSubtest detach ID: $id at " . $trace->debug . "\n",
            )
        );
        return;
    }

    unless (delete $ids->{$id}) {
        require Test2::Event::Exception;
        my $trace = $self->trace;
        $hub->send(
            Test2::Event::Exception->new(
                trace => $trace,
                error => "AsyncSubtest ID $id is not attached at " . $trace->debug . "\n",
            )
        );
        return;
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::AsyncSubtest::Event::Detach - Event to detach a subtest from the parent.

=head1 DESCRIPTION

Used internally by L<Test2::AsyncSubtest>. No user serviceable parts inside.

=head1 SOURCE

The source code repository for Test2-AsyncSubtest can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist7@gmail.comE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
