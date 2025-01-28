package Test2::Workflow::Task::Group;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;

use Test2::Workflow::Task::Action;

use base 'Test2::Workflow::Task';
use Test2::Util::HashBase qw/before after primary rand variant/;

sub init {
    my $self = shift;

    if (my $take = delete $self->{take}) {
        $self->{$_} = delete $take->{$_} for ISO, ASYNC, TODO, SKIP;
        $self->{$_} = $take->{$_} for FLAT, SCAFFOLD, NAME, CODE, FRAME;
        $take->{+FLAT}     = 1;
        $take->{+SCAFFOLD} = 1;
    }

    {
        local $Carp::CarpLevel = $Carp::CarpLevel + 1;
        $self->SUPER::init();
    }

    $self->{+BEFORE}  ||= [];
    $self->{+AFTER}   ||= [];
    $self->{+PRIMARY} ||= [];
}

sub filter {
    my $self = shift;
    my ($filter) = @_;

    return if $self->{+IS_ROOT};

    my $result = $self->SUPER::filter($filter);

    my $child_ok = 0;
    for my $c (@{$self->{+PRIMARY}}) {
        next if $c->{+SCAFFOLD};
        # A child matches the filter, so we should not be filtered, but also
        # should not satisfy the filter.
        my $res = $c->filter($filter);

        # A child satisfies the filter
        $child_ok++ if !$res || $res->{satisfied};
        last if $child_ok;
    }

    # If the filter says we are ok
    unless($result) {
        # If we are a variant then allow everything under us to be run
        return {satisfied => 1} if $self->{+VARIANT} || !$child_ok;

        # Normal group
        return;
    }

    return if $child_ok;

    return $result;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Workflow::Task::Group - Encapsulation of a group (describe).

=head1 SOURCE

The source code repository for Test2-Workflow can be found at
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

