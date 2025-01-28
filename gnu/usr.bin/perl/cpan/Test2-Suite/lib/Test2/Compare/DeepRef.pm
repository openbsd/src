package Test2::Compare::DeepRef;
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

sub name { '<REF>' }

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

    return 1;
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my $in = $self->{+INPUT};
    my $in_type = rtype($in);
    my $got_type = rtype($got);
    
    my $check = $convert->($$in);

    return $check->run(
        id      => ['DEREF' => '$*'],
        convert => $convert,
        seen    => $seen,
        got     => $$got,
        exists  => 1,
    );
}



1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::DeepRef - Ref comparison

=head1 DESCRIPTION

Used to compare two refs in a deep comparison.

=head1 SYNOPSIS


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
