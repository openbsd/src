package Test2::Compare::Array;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/inref meta ending items order for_each/;

use Carp qw/croak confess/;
use Scalar::Util qw/reftype looks_like_number/;

sub init {
    my $self = shift;

    if( defined( my $ref = $self->{+INREF}) ) {
        croak "Cannot specify both 'inref' and 'items'" if $self->{+ITEMS};
        croak "Cannot specify both 'inref' and 'order'" if $self->{+ORDER};
        croak "'inref' must be an array reference, got '$ref'" unless reftype($ref) eq 'ARRAY';
        my $order = $self->{+ORDER} = [];
        my $items = $self->{+ITEMS} = {};
        for (my $i = 0; $i < @$ref; $i++) {
            push @$order => $i;
            $items->{$i} = $ref->[$i];
        }
    }
    else {
        $self->{+ITEMS} ||= {};
        croak "All indexes listed in the 'items' hashref must be numeric"
            if grep { !looks_like_number($_) } keys %{$self->{+ITEMS}};

        $self->{+ORDER} ||= [sort { $a <=> $b } keys %{$self->{+ITEMS}}];
        croak "All indexes listed in the 'order' arrayref must be numeric"
            if grep { !(looks_like_number($_) || (ref($_) && reftype($_) eq 'CODE')) } @{$self->{+ORDER}};
    }

    $self->{+FOR_EACH} ||= [];

    $self->SUPER::init();
}

sub name { '<ARRAY>' }

sub meta_class  { 'Test2::Compare::Meta' }

sub verify {
    my $self = shift;
    my %params = @_;

    return 0 unless $params{exists};
    my $got = $params{got};
    return 0 unless defined $got;
    return 0 unless ref($got);
    return 0 unless reftype($got) eq 'ARRAY';
    return 1;
}

sub add_prop {
    my $self = shift;
    $self->{+META} = $self->meta_class->new unless defined $self->{+META};
    $self->{+META}->add_prop(@_);
}

sub top_index {
    my $self = shift;
    my @order = @{$self->{+ORDER}};

    while(@order) {
        my $idx = pop @order;
        next if ref $idx;
        return $idx;
    }

    return undef; # No indexes
}

sub add_item {
    my $self = shift;
    my $check = pop;
    my ($idx) = @_;

    my $top = $self->top_index;

    croak "elements must be added in order!"
        if $top && $idx && $idx <= $top;

    $idx = defined($top) ? $top + 1 : 0
        unless defined($idx);

    push @{$self->{+ORDER}} => $idx;
    $self->{+ITEMS}->{$idx} = $check;
}

sub add_filter {
    my $self = shift;
    my ($code) = @_;
    croak "A single coderef is required"
        unless @_ == 1 && $code && ref $code && reftype($code) eq 'CODE';

    push @{$self->{+ORDER}} => $code;
}

sub add_for_each {
    my $self = shift;
    push @{$self->{+FOR_EACH}} => @_;
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my @deltas;
    my $state = 0;
    my @order = @{$self->{+ORDER}};
    my $items = $self->{+ITEMS};
    my $for_each = $self->{+FOR_EACH};

    my $meta     = $self->{+META};
    push @deltas => $meta->deltas(%params) if defined $meta;

    # Make a copy that we can munge as needed.
    my @list = @$got;

    while (@order) {
        my $idx = shift @order;
        my $overflow = 0;
        my $val;

        # We have a filter, not an index
        if (ref($idx)) {
            @list = $idx->(@list);
            next;
        }

        confess "Internal Error: Stacks are out of sync (state > idx)"
            if $state > $idx + 1;

        while ($state <= $idx) {
            $overflow = !@list;
            $val = shift @list;

            # check-all goes here so we hit each item, even unspecified ones.
            for my $check (@$for_each) {
                last if $overflow; # avoid doing 'for each' checks beyond array bounds
                $check = $convert->($check);
                push @deltas => $check->run(
                    id      => [ARRAY => $state],
                    convert => $convert,
                    seen    => $seen,
                    exists  => !$overflow,
                    $overflow ? () : (got => $val),
                );
            }

            $state++;
        }

        confess "Internal Error: Stacks are out of sync (state != idx + 1)"
            unless $state == $idx + 1;

        my $check = $convert->($items->{$idx});

        push @deltas => $check->run(
            id      => [ARRAY => $idx],
            convert => $convert,
            seen    => $seen,
            exists  => !$overflow,
            $overflow ? () : (got => $val),
        );
    }

    while (@list && (@$for_each || $self->{+ENDING})) {
        my $item = shift @list;

        for my $check (@$for_each) {
            $check = $convert->($check);
            push @deltas => $check->run(
                id      => [ARRAY => $state],
                convert => $convert,
                seen    => $seen,
                got     => $item,
                exists  => 1,
            );
        }

        # if items are left over, and ending is true, we have a problem!
        if ($self->{+ENDING}) {
            push @deltas => $self->delta_class->new(
                dne      => 'check',
                verified => undef,
                id       => [ARRAY => $state],
                got      => $item,
                check    => undef,

                $self->{+ENDING} eq 'implicit' ? (note => 'implicit end') : (),
            );
        }

        $state++;
    }

    return @deltas;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Array - Internal representation of an array comparison.

=head1 DESCRIPTION

This module is an internal representation of an array for comparison purposes.

=head1 METHODS

=over 4

=item $ref = $arr->inref()

If the instance was constructed from an actual array, this will return the
reference to that array.

=item $bool = $arr->ending

=item $arr->set_ending($bool)

Set this to true if you would like to fail when the array being validated has
more items than the check. That is, if you check indexes 0-3 but the array has
values for indexes 0-4, it will fail and list that last item in the array as
unexpected. If set to false then it is assumed you do not care about extra
items.

=item $hashref = $arr->items()

Returns the hashref of C<< key => val >> pairs to be checked in the
array.

=item $arr->set_items($hashref)

Accepts a hashref to permit indexes to be skipped if desired.

B<Note:> that there is no validation when using C<set_items>, it is better to
use the C<add_item> interface.

=item $arrayref = $arr->order()

Returns an arrayref of all indexes that will be checked, in order.

=item $arr->set_order($arrayref)

Sets the order in which indexes will be checked.

B<Note:> that there is no validation when using C<set_order>, it is better to
use the C<add_item> interface.

=item $name = $arr->name()

Always returns the string C<< "<ARRAY>" >>.

=item $bool = $arr->verify(got => $got, exists => $bool)

Check if C<$got> is an array reference or not.

=item $idx = $arr->top_index()

Returns the topmost index which is checked. This will return undef if there
are no items, or C<0> if there is only 1 item.

=item $arr->add_item($item)

Push an item onto the list of values to be checked.

=item $arr->add_item($idx => $item)

Add an item to the list of values to be checked at the specified index.

=item $arr->add_filter(sub { ... })

Add a filter sub. The filter receives all remaining values of the array being
checked, and should return the values that should still be checked. The filter
will be run between the last item added and the next item added.

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
