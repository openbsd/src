package TAP::Parser::Scheduler::Job;

use strict;
use vars qw($VERSION);
use Carp;

=head1 NAME

TAP::Parser::Scheduler::Job - A single testing job.

=head1 VERSION

Version 3.23

=cut

$VERSION = '3.23';

=head1 SYNOPSIS

    use TAP::Parser::Scheduler::Job;

=head1 DESCRIPTION

Represents a single test 'job'.

=head1 METHODS

=head2 Class Methods

=head3 C<new>

    my $job = TAP::Parser::Scheduler::Job->new(
        $name, $desc 
    );

Returns a new C<TAP::Parser::Scheduler::Job> object.

=cut

sub new {
    my ( $class, $name, $desc, @ctx ) = @_;
    return bless {
        filename    => $name,
        description => $desc,
        @ctx ? ( context => \@ctx ) : (),
    }, $class;
}

=head3 C<on_finish>

Register a closure to be called when this job is destroyed.

=cut

sub on_finish {
    my ( $self, $cb ) = @_;
    $self->{on_finish} = $cb;
}

=head3 C<finish>

Called when a job is complete to unlock it.

=cut

sub finish {
    my $self = shift;
    if ( my $cb = $self->{on_finish} ) {
        $cb->($self);
    }
}

=head3 C<filename>

=head3 C<description>

=head3 C<context>

=cut

sub filename    { shift->{filename} }
sub description { shift->{description} }
sub context     { @{ shift->{context} || [] } }

=head3 C<as_array_ref>

For backwards compatibility in callbacks.

=cut

sub as_array_ref {
    my $self = shift;
    return [ $self->filename, $self->description, $self->{context} ||= [] ];
}

=head3 C<is_spinner>

Returns false indicating that this is a real job rather than a
'spinner'. Spinners are returned when the scheduler still has pending
jobs but can't (because of locking) return one right now.

=cut

sub is_spinner {0}

1;
