package Test2::Workflow::Build;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::Workflow::Task::Group;

our @BUILD_FIELDS;

BEGIN {
    @BUILD_FIELDS = qw{
        primary         variant
        setup           teardown
        variant_setup   variant_teardown
        primary_setup   primary_teardown
        stash
    };
}

use base 'Test2::Workflow::Task';
use Test2::Util::HashBase @BUILD_FIELDS, qw/events defaults stack_stop/;

sub init {
    my $self = shift;

    {
        local $Carp::CarpLevel = $Carp::CarpLevel + 1;
        $self->SUPER::init();
    }

    $self->{$_} ||= [] for @BUILD_FIELDS;
    $self->{+DEFAULTS} ||= {};
}

for my $field (@BUILD_FIELDS) {
    my $code = sub {
        my $self = shift;
        push @{$self->{$field}} => @_;
    };
    no strict 'refs';
    *{"add_$field"} = $code;
}

sub populated {
    my $self = shift;
    for my $field (@BUILD_FIELDS) {
        return 1 if @{$self->{$field}};
    }
    return 0;
}

sub compile {
    my $self = shift;

    warn "Workflow build '$self->{+NAME}' is empty " . $self->debug . "\n"
        unless $self->populated || $self->{+SKIP};

    my ($primary_setup, $primary_teardown) = @_;
    $primary_setup    ||= [];
    $primary_teardown ||= [];

    my $variant          = $self->{+VARIANT};
    my $setup            = $self->{+SETUP};
    my $teardown         = $self->{+TEARDOWN};
    my $variant_setup    = $self->{+VARIANT_SETUP};
    my $variant_teardown = $self->{+VARIANT_TEARDOWN};

    $primary_setup = [@$primary_setup, @{$self->{+PRIMARY_SETUP}}];
    $primary_teardown = [@{$self->{+PRIMARY_TEARDOWN}}, @$primary_teardown];

    # Get primaries in order.
    my $primary = [
        map {
            $_->isa(__PACKAGE__)
                ? $_->compile($primary_setup, $primary_teardown)
                : $_;
        } @{$self->{+PRIMARY}},
    ];

    if (@$primary_setup || @$primary_teardown) {
        $primary = [
            map {
                my $p = $_->clone;
                $_->isa('Test2::Workflow::Task::Action') ? Test2::Workflow::Task::Group->new(
                    before  => $primary_setup,
                    primary => [ $p ],
                    take    => $p,
                    after   => $primary_teardown,
                ) : $_;
            } @$primary
        ];
    }

    # Build variants
    if (@$variant) {
        $primary = [
            map {
                my $v = $_->clone;
                Test2::Workflow::Task::Group->new(
                    before  => $variant_setup,
                    primary => $primary,
                    after   => $variant_teardown,
                    variant => $v,
                    take    => $v,
                );
            } @$variant
        ];
    }

    my %params = map { Test2::Workflow::Task::Group->can($_) ? ($_ => $self->{$_}) : () } keys %$self;
    delete $params{$_} for @BUILD_FIELDS;

    return Test2::Workflow::Task::Group->new(
        %params,
        before  => $setup,
        after   => $teardown,
        primary => $primary,
    );
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Workflow::Build - Represents a build in progress.

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

