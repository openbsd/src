package Test2::Workflow::BlockBase;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/code frame _info _lines/;
use Test2::Util::Sub qw/sub_info/;
use List::Util qw/min max/;
use Carp qw/croak/;

use Test2::Util::Trace();

BEGIN {
    local ($@, $!, $SIG{__DIE__});

    my $set_name = eval { require Sub::Util; Sub::Util->can('set_subname') }
                || eval { require Sub::Name; Sub::Name->can('subname') };

    *set_subname = $set_name ? sub {
        my $self = shift;
        my ($name) = @_;

        $set_name->($name, $self->{+CODE});
        delete $self->{+_INFO};

        return 1;
    } : sub { return 0 };
}

sub init {
    my $self = shift;

    croak "The 'code' attribute is required"
        unless $self->{+CODE};

    croak "The 'frame' attribute is required"
        unless $self->{+FRAME};

    $self->{+_LINES} = delete $self->{lines}
        if $self->{lines};
}

sub file    { shift->info->{file} }
sub lines   { shift->info->{lines} }
sub package { shift->info->{package} }
sub subname { shift->info->{name} }

sub info {
    my $self = shift;

    unless ($self->{+_INFO}) {
        my $info = sub_info($self->code);

        my $frame     = $self->frame;
        my $file      = $info->{file};
        my $all_lines = $info->{all_lines};
        my $pre_lines = $self->{+_LINES};
        my $lines     = $info->{lines} ||= [];

        if ($pre_lines && @$pre_lines) {
            @$lines = @$pre_lines;
        }
        else {
            @$lines = (
                min(@$all_lines, $frame->[2]),
                max(@$all_lines, $frame->[2]),
            ) if $frame->[1] eq $file;
        }

        # Adjust for start
        $lines->[0]-- if $lines->[0] != $lines->[1];

        $self->{+_INFO} = $info;
    }

    return $self->{+_INFO};
}

sub trace {
    my $self = shift;

    my ($hub, %params) = @_;

    croak "'hub' is required"
        unless $hub;

    return Test2::Util::Trace->new(
        frame  => $self->frame,
        detail => $self->debug,

        buffered => $hub->buffered,
        nested   => $hub->nested,
        hid      => $hub->hid,
        huuid    => $hub->uuid,

        %params,
    );
}

sub debug {
    my $self = shift;
    my $file = $self->file;
    my $lines = $self->lines;

    my $line_str = @$lines == 1 ? "around line $lines->[0]" : "around lines $lines->[0] -> $lines->[1]";
    return "at $file $line_str.";
}

sub throw {
    my $self = shift;
    my ($msg) = @_;
    die "$msg " . $self->debug . "\n";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Workflow::BlockBase - Base class for all workflow blocks.

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

