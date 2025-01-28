package Test2::Compare::Hash;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/inref meta ending items order for_each_key for_each_val/;

use Carp qw/croak confess/;
use Scalar::Util qw/reftype/;

sub init {
    my $self = shift;

    if( defined( my $ref = $self->{+INREF} ) ) {
        croak "Cannot specify both 'inref' and 'items'" if $self->{+ITEMS};
        croak "Cannot specify both 'inref' and 'order'" if $self->{+ORDER};
        $self->{+ITEMS} = {%$ref};
        $self->{+ORDER} = [sort keys %$ref];
    }
    else {
        # Clone the ref to be safe
        $self->{+ITEMS} = $self->{+ITEMS} ? {%{$self->{+ITEMS}}} : {};
        if ($self->{+ORDER}) {
            my @all = keys %{$self->{+ITEMS}};
            my %have = map { $_ => 1 } @{$self->{+ORDER}};
            my @missing = grep { !$have{$_} } @all;
            croak "Keys are missing from the 'order' array: " . join(', ', sort @missing)
                if @missing;
        }
        else {
            $self->{+ORDER} = [sort keys %{$self->{+ITEMS}}];
        }
    }

    $self->{+FOR_EACH_KEY} ||= [];
    $self->{+FOR_EACH_VAL} ||= [];

    $self->SUPER::init();
}

sub name { '<HASH>' }

sub meta_class  { 'Test2::Compare::Meta' }

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;
    return 0 unless defined $got;
    return 0 unless ref($got);
    return 0 unless reftype($got) eq 'HASH';
    return 1;
}

sub add_prop {
    my $self = shift;
    $self->{+META} = $self->meta_class->new unless defined $self->{+META};
    $self->{+META}->add_prop(@_);
}

sub add_field {
    my $self = shift;
    my ($name, $check) = @_;

    croak "field name is required"
        unless defined $name;

    croak "field '$name' has already been specified"
        if exists $self->{+ITEMS}->{$name};

    push @{$self->{+ORDER}} => $name;
    $self->{+ITEMS}->{$name} = $check;
}

sub add_for_each_key {
    my $self = shift;
    push @{$self->{+FOR_EACH_KEY}} => @_;
}

sub add_for_each_val {
    my $self = shift;
    push @{$self->{+FOR_EACH_VAL}} => @_;
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my @deltas;
    my $items = $self->{+ITEMS};
    my $each_key = $self->{+FOR_EACH_KEY};
    my $each_val = $self->{+FOR_EACH_VAL};

    # Make a copy that we can munge as needed.
    my %fields = %$got;

    my $meta     = $self->{+META};
    push @deltas => $meta->deltas(%params) if defined $meta;

    for my $key (@{$self->{+ORDER}}) {
        my $check  = $convert->($items->{$key});
        my $exists = exists $fields{$key};
        my $val    = delete $fields{$key};

        if ($exists) {
            for my $kcheck (@$each_key) {
                $kcheck = $convert->($kcheck);

                push @deltas => $kcheck->run(
                    id      => [HASHKEY => $key],
                    convert => $convert,
                    seen    => $seen,
                    exists  => $exists,
                    got     => $key,
                );
            }

            for my $vcheck (@$each_val) {
                $vcheck = $convert->($vcheck);

                push @deltas => $vcheck->run(
                    id      => [HASH => $key],
                    convert => $convert,
                    seen    => $seen,
                    exists  => $exists,
                    got     => $val,
                );
            }
        }

        push @deltas => $check->run(
            id      => [HASH => $key],
            convert => $convert,
            seen    => $seen,
            exists  => $exists,
            $exists ? (got => $val) : (),
        );
    }

    if (keys %fields) {
        for my $key (sort keys %fields) {
            my $val = $fields{$key};

            for my $kcheck (@$each_key) {
                $kcheck = $convert->($kcheck);

                push @deltas => $kcheck->run(
                    id      => [HASHKEY => $key],
                    convert => $convert,
                    seen    => $seen,
                    got     => $key,
                    exists  => 1,
                );
            }

            for my $vcheck (@$each_val) {
                $vcheck = $convert->($vcheck);

                push @deltas => $vcheck->run(
                    id      => [HASH => $key],
                    convert => $convert,
                    seen    => $seen,
                    got     => $val,
                    exists  => 1,
                );
            }

            # if items are left over, and ending is true, we have a problem!
            if ($self->{+ENDING}) {
                push @deltas => $self->delta_class->new(
                    dne      => 'check',
                    verified => undef,
                    id       => [HASH => $key],
                    got      => $val,
                    check    => undef,

                    $self->{+ENDING} eq 'implicit' ? (note => 'implicit end') : (),
                );
            }
        }
    }

    return @deltas;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Hash - Representation of a hash in a deep comparison.

=head1 DESCRIPTION

In deep comparisons this class is used to represent a hash.

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
