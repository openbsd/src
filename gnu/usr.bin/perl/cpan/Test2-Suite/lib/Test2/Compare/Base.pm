package Test2::Compare::Base;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/confess croak/;
use Scalar::Util qw/blessed/;

use Test2::Util::Sub qw/sub_info/;
use Test2::Compare::Delta();

sub MAX_CYCLES() { 75 }

use Test2::Util::HashBase qw{builder _file _lines _info called};
use Test2::Util::Ref qw/render_ref/;

{
    no warnings 'once';
    *set_lines = \&set__lines;
    *set_file  = \&set__file;
}

sub clone {
    my $self = shift;
    my $class = blessed($self);

    # Shallow copy is good enough for all the current compare types.
    return bless({%$self}, $class);
}

sub init {
    my $self = shift;
    $self->{+_LINES} = delete $self->{lines} if exists $self->{lines};
    $self->{+_FILE}  = delete $self->{file}  if exists $self->{file};
}

sub file {
    my $self = shift;
    return $self->{+_FILE} if $self->{+_FILE};

    if ($self->{+BUILDER}) {
        $self->{+_INFO} ||= sub_info($self->{+BUILDER});
        return $self->{+_INFO}->{file};
    }
    elsif ($self->{+CALLED}) {
        return $self->{+CALLED}->[1];
    }

    return undef;
}

sub lines {
    my $self = shift;
    return $self->{+_LINES} if $self->{+_LINES};

    if ($self->{+BUILDER}) {
        $self->{+_INFO} ||= sub_info($self->{+BUILDER});
        return $self->{+_INFO}->{lines} if @{$self->{+_INFO}->{lines}};
    }
    if ($self->{+CALLED}) {
        return [$self->{+CALLED}->[2]];
    }
    return [];
}

sub delta_class { 'Test2::Compare::Delta' }

sub deltas { () }
sub got_lines { () }

sub stringify_got { 0 }

sub operator { '' }
sub verify   { confess "unimplemented" }
sub name     { confess "unimplemented" }

sub render {
    my $self = shift;
    return $self->name;
}

sub run {
    my $self = shift;
    my %params = @_;

    my $id      = $params{id};
    my $convert = $params{convert} or confess "no convert sub provided";
    my $seen    = $params{seen} ||= {};

    $params{exists} = exists $params{got} ? 1 : 0
        unless exists $params{exists};

    my $exists = $params{exists};
    my $got = $exists ? $params{got} : undef;

    my $gotname = render_ref($got);

    # Prevent infinite cycles
    if (defined($got) && ref $got) {
        die "Cycle detected in comparison, aborting"
            if $seen->{$gotname} && $seen->{$gotname} >= MAX_CYCLES;
        $seen->{$gotname}++;
    }

    my $ok = $self->verify(%params);
    my @deltas = $ok ? $self->deltas(%params) : ();

    $seen->{$gotname}-- if defined $got && ref $got;

    return if $ok && !@deltas;

    return $self->delta_class->new(
        verified => $ok,
        id       => $id,
        got      => $got,
        check    => $self,
        children => \@deltas,
        $exists ? () : (dne => 'got'),
    );
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Base - Base class for comparison classes.

=head1 DESCRIPTION

All comparison classes for Test2::Compare should inherit from this base class.

=head1 SYNOPSIS

    package Test2::Compare::MyCheck;
    use strict;
    use warnings;

    use base 'Test2::Compare::Base';
    use Test2::Util::HashBase qw/stuff/;

    sub name { 'STUFF' }

    sub operator {
        my $self = shift;
        my ($got) = @_;
        return 'eq';
    }

    sub verify {
        my $self = shift;
        my $params = @_;

        # Always check if $got exists! This method must return false if no
        # value at all was received.
        return 0 unless $params{exists};

        my $got = $params{got};

        # Returns true if both values match. This includes undef, 0, and other
        # false-y values!
        return $got eq $self->stuff;
    }

=head1 METHODS

Some of these must be overridden, others can be.

=over 4

=item $dclass = $check->delta_class

Returns the delta subclass that should be used. By default
L<Test2::Compare::Delta> is used.

=item @deltas = $check->deltas(id => $id, exists => $bool, got => $got, convert => \&convert, seen => \%seen)

Should return child deltas.

=item @lines = $check->got_lines($got)

This is your chance to provide line numbers for errors in the C<$got>
structure.

=item $op = $check->operator()

=item $op = $check->operator($got)

Returns the operator that was used to compare the check with the received data
in C<$got>. If there was no value for got then there will be no arguments,
undef will only be an argument if undef was seen in C<$got>. This is how you
can tell the difference between a missing value and an undefined one.

=item $bool = $check->verify(id => $id, exists => $bool, got => $got, convert => \&convert, seen => \%seen)

Return true if there is a shallow match, that is both items are arrayrefs, both
items are the same string or same number, etc. This should not recurse, as deep
checks are done in C<< $check->deltas() >>.

=item $name = $check->name

Get the name of the check.

=item $display = $check->render

What should be displayed in a table for this check, usually the name or value.

=item $delta = $check->run(id => $id, exists => $bool, got => $got, convert => \&convert, seen => \%seen)

This is where the checking is done, first a shallow check using
C<< $check->verify >>, then checking C<< $check->deltas() >>. C<\%seen> is used
to prevent cycles.

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
