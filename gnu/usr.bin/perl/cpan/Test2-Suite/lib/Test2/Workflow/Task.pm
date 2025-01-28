package Test2::Workflow::Task;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::API();
use Test2::Event::Exception();

use List::Util qw/min max/;
use Scalar::Util qw/blessed/;
use Carp qw/croak/;
our @CARP_NOT = qw/Test2::Util::HashBase/;

use base 'Test2::Workflow::BlockBase';
use Test2::Util::HashBase qw/name flat async iso todo skip scaffold events is_root/;

for my $attr (FLAT, ISO, ASYNC, TODO, SKIP, SCAFFOLD) {
    my $old = __PACKAGE__->can("set_$attr");
    my $new = sub {
        my $self = shift;
        my $out = $self->$old(@_);
        $self->verify_scaffold;
        return $out;
    };

    no strict 'refs';
    no warnings 'redefine';
    *{"set_$attr"} = $new;
}

sub init {
    my $self = shift;

    $self->{+EVENTS} ||= [];

    {
        local $Carp::CarpLevel = $Carp::CarpLevel + 1;
        $self->SUPER::init();
    }

    $self->throw("the 'name' attribute is required")
        unless $self->{+NAME};

    $self->throw("the 'flat' attribute cannot be combined with 'iso' or 'async'")
        if $self->{+FLAT} && ($self->{+ISO} || $self->{+ASYNC});

    $self->set_subname($self->package . "::<$self->{+NAME}>");

    $self->verify_scaffold;
}

sub clone {
    my $self = shift;
    return bless {%$self}, blessed($self);
}

sub verify_scaffold {
    my $self = shift;

    return unless $self->{+SCAFFOLD};

    croak "The 'flat' attribute must be true for scaffolding"
        if defined($self->{+FLAT}) && !$self->{+FLAT};

    $self->{+FLAT} = 1;

    for my $attr (ISO, ASYNC, TODO, SKIP) {
        croak "The '$attr' attribute cannot be used on scaffolding"
            if $self->{$attr};
    }
}

sub exception {
    my $self = shift;
    my ($err) = @_;

    my $hub = Test2::API::test2_stack->top;

    my $trace = $self->trace($hub);

    $hub->send(
        Test2::Event::Exception->new(
            trace => $trace,
            error => $err,
        ),
    );
}

sub filter {
    my $self = shift;
    my ($filter) = @_;

    return unless $filter;
    return if $self->{+IS_ROOT};
    return if $self->{+SCAFFOLD};

    if (my $name = $filter->{name}) {
        my $ok = 0;
        unless(ref($name)) {
            $ok ||= $self->{+NAME} eq $name;
            $ok ||= $self->subname eq $name;
        }
        if (ref($name) eq 'Regexp') {
            $ok ||= $self->{+NAME} =~ $name;
            $ok ||= $self->subname =~ $name;
        }
        elsif ($name =~ m{^/}) {
            my $pattern = eval "qr$name" or die "'$name' does not appear to be a valid pattern";
            $ok ||= $self->{+NAME} =~ $pattern;
            $ok ||= $self->subname =~ $pattern;
        }

        return {skip => "Does not match name filter '$name'"}
            unless $ok;
    }

    if (my $file = $filter->{file}) {
        return {skip => "Does not match file filter '$file'"}
            unless $self->file eq $file;
    }

    if (my $line = $filter->{line}) {
        my $lines = $self->lines;

        return {skip => "Does not match line filter '$line' (no lines)"}
            unless $lines && @$lines;

        my $min = min(@$lines);
        my $max = max(@$lines);

        return {skip => "Does not match line filter '$min <= $line <= $max'"}
            unless $min <= $line && $max >= $line;
    }

    return;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Workflow::Task - Encapsulation of a Task

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

