package Test2::Compare::Object;
use strict;
use warnings;

use Test2::Util qw/try/;

use Test2::Compare::Meta();

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/calls meta refcheck ending/;

use Carp qw/croak confess/;
use Scalar::Util qw/reftype blessed/;

sub init {
    my $self = shift;
    $self->{+CALLS} ||= [];
    $self->SUPER::init();
}

sub name { '<OBJECT>' }

sub meta_class  { 'Test2::Compare::Meta' }
sub object_base { 'UNIVERSAL' }

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;
    return 0 unless defined $got;
    return 0 unless ref($got);
    return 0 unless blessed($got);
    return 0 unless $got->isa($self->object_base);
    return 1;
}

sub add_prop {
    my $self = shift;
    $self->{+META} = $self->meta_class->new unless defined $self->{+META};
    $self->{+META}->add_prop(@_);
}

sub add_field {
    my $self = shift;
    $self->{+REFCHECK} = Test2::Compare::Hash->new unless defined $self->{+REFCHECK};

    croak "Underlying reference does not have fields"
        unless $self->{+REFCHECK}->can('add_field');

    $self->{+REFCHECK}->add_field(@_);
}

sub add_item {
    my $self = shift;
    $self->{+REFCHECK} = Test2::Compare::Array->new unless defined $self->{+REFCHECK};

    croak "Underlying reference does not have items"
        unless $self->{+REFCHECK}->can('add_item');

    $self->{+REFCHECK}->add_item(@_);
}

sub add_call {
    my $self = shift;
    my ($meth, $check, $name, $context) = @_;
    $name ||= ref $meth eq 'ARRAY' ? $meth->[0]
        : ref $meth eq 'CODE' ? '\&CODE'
        : $meth;
    push @{$self->{+CALLS}} => [$meth, $check, $name, $context || 'scalar'];
}

sub deltas {
    my $self = shift;
    my %params = @_;
    my ($got, $convert, $seen) = @params{qw/got convert seen/};

    my @deltas;
    my $meta     = $self->{+META};
    my $refcheck = $self->{+REFCHECK};

    push @deltas => $meta->deltas(%params) if defined $meta;

    for my $call (@{$self->{+CALLS}}) {
        my ($meth, $check, $name, $context)= @$call;
        $context ||= 'scalar';

        $check = $convert->($check);

        my @args;
        if (ref($meth) eq 'ARRAY') {
            ($meth,@args) = @{$meth};
        }

        my $exists = ref($meth) || $got->can($meth);
        my $val;
        my ($ok, $err) = try {
            $val = $exists
                ? ( $context eq 'list' ? [ $got->$meth(@args) ] :
                    $context eq 'hash' ? { $got->$meth(@args) } :
                    $got->$meth(@args)
                )
                : undef;
        };

        if (!$ok) {
            push @deltas => $self->delta_class->new(
                verified  => undef,
                id        => [METHOD => $name],
                got       => undef,
                check     => $check,
                exception => $err,
            );
        }
        else {
            push @deltas => $check->run(
                id      => [METHOD => $name],
                convert => $convert,
                seen    => $seen,
                exists  => $exists,
                $exists ? (got => $val) : (),
            );
        }
    }

    return @deltas unless defined $refcheck;

    $refcheck->set_ending($self->{+ENDING});

    if ($refcheck->verify(%params)) {
        push @deltas => $refcheck->deltas(%params);
    }
    else {
        push @deltas => $self->delta_class->new(
            verified => undef,
            id       => [META => 'Object Ref'],
            got      => $got,
            check    => $refcheck,
        );
    }

    return @deltas;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Object - Representation of an object during deep
comparison.

=head1 DESCRIPTION

This class lets you specify an expected object in a deep comparison. You can
check the fields/elements of the underlying reference, call methods to verify
results, and do meta checks for object type and ref type.

=head1 METHODS

=over 4

=item $class = $obj->meta_class

The meta-class to be used when checking the object type. This is mainly listed
because it is useful to override for specialized object subclasses.

This normally just returns L<Test2::Compare::Meta>.

=item $class = $obj->object_base

The base-class to be expected when checking the object type. This is mainly
listed because it is useful to override for specialized object subclasses.

This normally just returns 'UNIVERSAL'.

=item $obj->add_prop(...)

Add a meta-property to check, see L<Test2::Compare::Meta>. This method
just delegates.

=item $obj->add_field(...)

Add a hash-field to check, see L<Test2::Compare::Hash>. This method
just delegates.

=item $obj->add_item(...)

Add an array item to check, see L<Test2::Compare::Array>. This method
just delegates.

=item $obj->add_call($method, $check)

=item $obj->add_call($method, $check, $name)

=item $obj->add_call($method, $check, $name, $context)

Add a method call check. This will call the specified method on your object and
verify the result. C<$method> may be a method name, an array ref, or a coderef.

If it's an arrayref, the first element must be the method name, and
the rest are arguments that will be passed to it.

In the case of a coderef it can be helpful to provide an alternate
name. When no name is provided the name is either C<$method> or the
string '\&CODE'.

If C<$context> is C<'list'>, the method will be invoked in list
context, and the result will be an arrayref.

If C<$context> is C<'hash'>, the method will be invoked in list
context, and the result will be a hashref (this will warn if the
method returns an odd number of values).

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
