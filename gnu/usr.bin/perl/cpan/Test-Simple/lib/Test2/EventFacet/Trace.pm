package Test2::EventFacet::Trace;
use strict;
use warnings;

our $VERSION = '1.302133';

BEGIN { require Test2::EventFacet; our @ISA = qw(Test2::EventFacet) }

use Test2::Util qw/get_tid pkg_to_file/;
use Carp qw/confess/;

use Test2::Util::HashBase qw{^frame ^pid ^tid ^cid -hid -nested details -buffered -uuid -huuid};

{
    no warnings 'once';
    *DETAIL = \&DETAILS;
    *detail = \&details;
    *set_detail = \&set_details;
}

sub init {
    confess "The 'frame' attribute is required"
        unless $_[0]->{+FRAME};

    $_[0]->{+DETAILS} = delete $_[0]->{detail} if $_[0]->{detail};

    $_[0]->{+PID} = $$        unless defined $_[0]->{+PID};
    $_[0]->{+TID} = get_tid() unless defined $_[0]->{+TID};
}

sub snapshot {
    my ($orig, @override) = @_;
    bless {%$orig, @override}, __PACKAGE__;
}

sub signature {
    my $self = shift;

    # Signature is only valid if all of these fields are defined, there is no
    # signature if any is missing. '0' is ok, but '' is not.
    return join ':' => map { (defined($_) && length($_)) ? $_ : return undef } (
        $self->{+CID},
        $self->{+PID},
        $self->{+TID},
        $self->{+FRAME}->[1],
        $self->{+FRAME}->[2],
    );
}

sub debug {
    my $self = shift;
    return $self->{+DETAILS} if $self->{+DETAILS};
    my ($pkg, $file, $line) = $self->call;
    return "at $file line $line";
}

sub alert {
    my $self = shift;
    my ($msg) = @_;
    warn $msg . ' ' . $self->debug . ".\n";
}

sub throw {
    my $self = shift;
    my ($msg) = @_;
    die $msg . ' ' . $self->debug . ".\n";
}

sub call { @{$_[0]->{+FRAME}} }

sub package { $_[0]->{+FRAME}->[0] }
sub file    { $_[0]->{+FRAME}->[1] }
sub line    { $_[0]->{+FRAME}->[2] }
sub subname { $_[0]->{+FRAME}->[3] }

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::EventFacet::Trace - Debug information for events

=head1 DESCRIPTION

The L<Test2::API::Context> object, as well as all L<Test2::Event> types need to
have access to information about where they were created.  This object
represents that information.

=head1 SYNOPSIS

    use Test2::EventFacet::Trace;

    my $trace = Test2::EventFacet::Trace->new(
        frame => [$package, $file, $line, $subname],
    );

=head1 FACET FIELDS

=over 4

=item $string = $trace->{details}

=item $string = $trace->details()

Used as a custom trace message that will be used INSTEAD of
C<< at <FILE> line <LINE> >> when calling C<< $trace->debug >>.

=item $frame = $trace->{frame}

=item $frame = $trace->frame()

Get the call frame arrayref.

=item $int = $trace->{pid}

=item $int = $trace->pid()

The process ID in which the event was generated.

=item $int = $trace->{tid}

=item $int = $trace->tid()

The thread ID in which the event was generated.

=item $id = $trace->{cid}

=item $id = $trace->cid()

The ID of the context that was used to create the event.

=item $uuid = $trace->{uuid}

=item $uuid = $trace->uuid()

The UUID of the context that was used to create the event. (If uuid tagging was
enabled)

=back

=head2 DISCOURAGED HUB RELATED FIELDS

These fields were not always set properly by tools. These are B<MOSTLY>
deprecated by the L<Test2::EventFacet::Hub> facets. These fields are not
required, and may only reflect the hub that was current when the event was
created, which is not necessarily the same as the hub the event was sent
through.

Some tools did do a good job setting these to the correct hub, but you cannot
always rely on that. Use the 'hubs' facet list instead.

=over 4

=item $hid = $trace->{hid}

=item $hid = $trace->hid()

The ID of the hub that was current when the event was created.

=item $huuid = $trace->{huuid}

=item $huuid = $trace->huuid()

The UUID of the hub that was current when the event was created. (If uuid
tagging was enabled).

=item $int = $trace->{nested}

=item $int = $trace->nested()

How deeply nested the event is.

=item $bool = $trace->{buffered}

=item $bool = $trace->buffered()

True if the event was buffered and not sent to the formatter independent of a
parent (This should never be set when nested is C<0> or C<undef>).

=back

=head1 METHODS

B<Note:> All facet frames are also methods.

=over 4

=item $trace->set_detail($msg)

=item $msg = $trace->detail

Used to get/set a custom trace message that will be used INSTEAD of
C<< at <FILE> line <LINE> >> when calling C<< $trace->debug >>.

C<detail()> is an alias to the C<details> facet field for backwards
compatibility.

=item $str = $trace->debug

Typically returns the string C<< at <FILE> line <LINE> >>. If C<detail> is set
then its value will be returned instead.

=item $trace->alert($MESSAGE)

This issues a warning at the frame (filename and line number where
errors should be reported).

=item $trace->throw($MESSAGE)

This throws an exception at the frame (filename and line number where
errors should be reported).

=item ($package, $file, $line, $subname) = $trace->call()

Get the caller details for the debug-info. This is where errors should be
reported.

=item $pkg = $trace->package

Get the debug-info package.

=item $file = $trace->file

Get the debug-info filename.

=item $line = $trace->line

Get the debug-info line number.

=item $subname = $trace->subname

Get the debug-info subroutine name.

=item $sig = trace->signature

Get a signature string that identifies this trace. This is used to check if
multiple events are related.

If UUID's are enabled then a uuid is returned. Otherwise the signature includes
pid, tid, file, line number, and the cid which is C<'C\d+'> for traces created
by a context, or C<'T\d+'> for traces created by C<new()>.

=back

=head1 SOURCE

The source code repository for Test2 can be found at
F<http://github.com/Test-More/test-more/>.

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
