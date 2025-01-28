package Test2::Compare::Ref;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/input/;

use Test2::Util::Ref qw/render_ref rtype/;
use Scalar::Util qw/refaddr/;
use Carp qw/croak/;

sub init {
    my $self = shift;

    croak "'input' is a required attribute"
        unless $self->{+INPUT};

    croak "'input' must be a reference, got '" . $self->{+INPUT} . "'"
        unless ref $self->{+INPUT};

    $self->SUPER::init();
}

sub operator { '==' }

sub name { render_ref($_[0]->{+INPUT}) }

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;

    my $in = $self->{+INPUT};
    return 0 unless ref $in;
    return 0 unless ref $got;

    my $in_type = rtype($in);
    my $got_type = rtype($got);

    return 0 unless $in_type eq $got_type;

    # Don't let overloading mess with us.
    return refaddr($in) == refaddr($got);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Ref - Ref comparison

=head1 DESCRIPTION

Used to compare two refs in a deep comparison.

=head1 SYNOPSIS

    my $ref = {};
    my $check = Test2::Compare::Ref->new(input => $ref);

    # Passes
    is( [$ref], [$check], "The array contains the exact ref we want" );

    # Fails, they both may be empty hashes, but we are looking for a specific
    # reference.
    is( [{}], [$check], "This will fail");

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
