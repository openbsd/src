package Test2::Compare::Event;
use strict;
use warnings;

use Scalar::Util qw/blessed/;

use Test2::Compare::EventMeta();

use base 'Test2::Compare::Object';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/etype/;

sub name {
    my $self = shift;
    my $etype = $self->etype;
    return "<EVENT: $etype>";
}

sub meta_class  { 'Test2::Compare::EventMeta' }
sub object_base { 'Test2::Event' }

sub got_lines {
    my $self = shift;
    my ($event) = @_;
    return unless $event;
    return unless blessed($event);
    return unless $event->isa('Test2::Event');
    return unless $event->trace;

    return ($event->trace->line);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Event - Event specific Object subclass.

=head1 DESCRIPTION

This module is used to represent an expected event in a deep comparison.

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
