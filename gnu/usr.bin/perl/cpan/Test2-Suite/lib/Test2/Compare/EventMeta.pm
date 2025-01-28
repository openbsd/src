package Test2::Compare::EventMeta;
use strict;
use warnings;

use base 'Test2::Compare::Meta';

our $VERSION = '0.000162';

use Test2::Util::HashBase;

sub get_prop_file    { $_[1]->trace->file }
sub get_prop_line    { $_[1]->trace->line }
sub get_prop_package { $_[1]->trace->package }
sub get_prop_subname { $_[1]->trace->subname }
sub get_prop_debug   { $_[1]->trace->debug }
sub get_prop_tid     { $_[1]->trace->tid }
sub get_prop_pid     { $_[1]->trace->pid }

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::EventMeta - Meta class for events in deep comparisons

=head1 DESCRIPTION

This is used in deep comparisons of event objects. You should probably never
use this directly.

=head1 DEFINED CHECKS

=over 4

=item file

File that generated the event.

=item line

Line where the event was generated.

=item package

Package that generated the event.

=item subname

Name of the tool that generated the event.

=item debug

The debug information that will be printed in event of a failure.

=item tid

Thread ID of the thread that generated the event.

=item pid

Process ID of the process that generated the event.

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
