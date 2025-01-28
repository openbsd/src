package Test2::Compare::Scalar;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/item/;

use Carp qw/croak confess/;
use Scalar::Util qw/reftype blessed/;

sub init {
    my $self = shift;
    croak "'item' is a required attribute"
        unless defined $self->{+ITEM};

    $self->SUPER::init();
}

sub name     { '<SCALAR>' }
sub operator { '${...}' }

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;
    return 0 unless defined $got;
    return 0 unless ref($got);
    return 0 unless reftype($got) eq 'SCALAR' || reftype($got) eq 'VSTRING';
    return 1;
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my $item = $self->{+ITEM};
    my $check = $convert->($item);

    return (
        $check->run(
            id      => ['SCALAR' => '$*'],
            got     => $$got,
            convert => $convert,
            seen    => $seen,
            exists  => 1,
        ),
    );
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Scalar - Representation of a Scalar Ref in deep
comparisons

=head1 DESCRIPTION

This is used in deep comparisons to represent a scalar reference.

=head1 SYNOPSIS

    my $sr = Test2::Compare::Scalar->new(item => 'foo');

    is([\'foo'], $sr, "pass");
    is([\'bar'], $sr, "fail, different value");
    is(['foo'],  $sr, "fail, not a ref");

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
