package Test2::Compare::OrderedSubset;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/inref items/;

use Carp qw/croak/;
use Scalar::Util qw/reftype/;

sub init {
    my $self = shift;

    if(my $ref = $self->{+INREF}) {
        croak "Cannot specify both 'inref' and 'items'" if $self->{+ITEMS};
        croak "'inref' must be an array reference, got '$ref'" unless reftype($ref) eq 'ARRAY';
        $self->{+ITEMS} = [@{$self->{+INREF}}];
    }

    $self->{+ITEMS} ||= [];

    $self->SUPER::init();
}

sub name { '<ORDERED SUBSET>' }

sub verify {
    my $self = shift;
    my %params = @_;

    return 0 unless $params{exists};
    defined( my $got = $params{got} ) || return 0;
    return 0 unless ref($got);
    return 0 unless reftype($got) eq 'ARRAY';
    return 1;
}

sub add_item {
    my $self = shift;
    my $check = pop;

    push @{$self->{+ITEMS}} => $check;
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my @deltas;
    my $state = 0;
    my $items = $self->{+ITEMS};

    my $idx = 0;

    for my $item (@$items) {
        my $check = $convert->($item);

        my $i = $idx;
        my $found;
        while($i < @$got) {
            my $val = $got->[$i++];
            next if $check->run(
                id      => [ARRAY => $i],
                convert => $convert,
                seen    => $seen,
                exists  => 1,
                got     => $val,
            );

            $idx = $i;
            $found++;
            last;
        }

        next if $found;

        push @deltas => Test2::Compare::Delta->new(
            verified => 0,
            id       => ['ARRAY', '?'],
            check    => $check,
            dne      => 'got',
        );
    }

    return @deltas;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::OrderedSubset - Internal representation of an ordered subset.

=head1 DESCRIPTION

This module is used to ensure an array has all the expected items int he
expected order. It ignores any unexpected items mixed into the array. It only
cares that all the expected values are present, and in order, everything else
is noise.

=head1 METHODS

=over 4

=item $ref = $arr->inref()

If the instance was constructed from an actual array, this will have the
reference to that array.

=item $arrayref = $arr->items()

=item $arr->set_items($arrayref)

All the expected items, in order.

=item $name = $arr->name()

Always returns the string C<< "<ORDERED SUBSET>" >>.

=item $bool = $arr->verify(got => $got, exists => $bool)

Check if C<$got> is an array reference or not.

=item $arr->add_item($item)

Add an item to the list of values to check.

=item @deltas = $arr->deltas(got => $got, convert => \&convert, seen => \%seen)

Find the differences between the expected array values and those in the C<$got>
arrayref.

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
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

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
