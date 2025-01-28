package Test2::Compare::Meta;
use strict;
use warnings;

use Test2::Compare::Delta();
use Test2::Compare::Isa();

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/items/;

use Carp qw/croak confess/;
use Scalar::Util qw/reftype blessed/;

sub init {
    my $self = shift;
    $self->{+ITEMS} ||= [];
    $self->SUPER::init();
}

sub name { '<META CHECKS>' }

sub verify {
    my $self = shift;
    my %params = @_;
    return $params{exists} ? 1 : 0;
}

sub add_prop {
    my $self = shift;
    my ($name, $check) = @_;

    croak "prop name is required"
        unless defined $name;

    croak "check is required"
        unless defined $check;

    my $meth = "get_prop_$name";
    croak "'$name' is not a known property"
        unless $self->can($meth);

    if ($name eq 'isa') {
        if (blessed($check) && $check->isa('Test2::Compare::Wildcard')) {
            # Carry forward file and lines that are set in Test2::Tools::Compare::prop.
            $check = Test2::Compare::Isa->new(
                input => $check->expect,
                file  => $check->file,
                lines => $check->lines,
            );
        } else {
            $check = Test2::Compare::Isa->new(input => $check);
        }
    }

    push @{$self->{+ITEMS}} => [$meth, $check, $name];
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my @deltas;
    my $items = $self->{+ITEMS};

    for my $set (@$items) {
        my ($meth, $check, $name) = @$set;

        $check = $convert->($check);

        my $val = $self->$meth($got);

        push @deltas => $check->run(
            id      => [META => $name],
            got     => $val,
            convert => $convert,
            seen    => $seen,
        );
    }

    return @deltas;
}

sub get_prop_blessed { blessed($_[1]) }

sub get_prop_reftype { reftype($_[1]) }

sub get_prop_isa { $_[1] }

sub get_prop_this { $_[1] }

sub get_prop_size {
    my $self = shift;
    my ($it) = @_;

    my $type = reftype($it) || '';

    return scalar @$it      if $type eq 'ARRAY';
    return scalar keys %$it if $type eq 'HASH';
    return undef;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Meta - Check library for meta-checks

=head1 DESCRIPTION

Sometimes in a deep comparison you want to run extra checks against an item
down the chain. This library allows you to write a check that verifies several
attributes of an item.

=head1 DEFINED CHECKS

=over 4

=item blessed

Lets you check that an item is blessed, and that it is blessed into the
expected class.

=item reftype

Lets you check the reftype of the item.

=item isa

Lets you check if the item is an instance of the expected class.

=item this

Lets you check the item itself.

=item size

Lets you check the size of the item. For an arrayref this is the number of
elements. For a hashref this is the number of keys. For everything else this is
undef.

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
