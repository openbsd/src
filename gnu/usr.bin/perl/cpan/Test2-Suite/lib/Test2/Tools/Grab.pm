package Test2::Tools::Grab;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::Util::Grabber;
use Test2::EventFacet::Trace();

our @EXPORT = qw/grab/;
use base 'Exporter';

sub grab { Test2::Util::Grabber->new(trace => Test2::EventFacet::Trace->new(frame => [caller(0)]) ) }

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Grab - Temporarily intercept all events without adding a scope
level.

=head1 DESCRIPTION

This package provides a function that returns an object that grabs all events.
Once the object is destroyed events will once again be sent to the main hub.

=head1 SYNOPSIS

    use Test2::Tools::Grab;

    my $grab = grab();

    # Generate some events, they are intercepted.
    ok(1, "pass");
    ok(0, "fail");

    my $events_a = $grab->flush;

    # Generate some more events, they are intercepted.
    ok(1, "pass");
    ok(0, "fail");

    my $events_b = $grab->finish;

=head1 EXPORTS

=over 4

=item $grab = grab()

This lets you intercept all events for a section of code without adding
anything to your call stack. This is useful for things that are sensitive to
changes in the stack depth.

    my $grab = grab();
        ok(1, 'foo');
        ok(0, 'bar');

    my $events = $grab->finish;

    is(@$events, 2, "grabbed 2 events.");

If the C<$grab> object is destroyed without calling C<finish()>, it will
automatically clean up after itself and restore the parent hub.

    {
        my $grab = grab();
        # Things are grabbed
    }
    # Things are back to normal

By default the hub used has C<no_ending> set to true. This will prevent the hub
from enforcing that you issued a plan and ran at least 1 test. You can turn
enforcement back one like this:

    $grab->hub->set_no_ending(0);

With C<no_ending> turned off, C<finish> will run the post-test checks to
enforce the plan and that tests were run. In many cases this will result in
additional events in your events array.

=back

=head1 SEE ALSO

L<Test2::Util::Grabber> - The object constructed and returned by this tool.

=head1 SOURCE

The source code repository for Test2 can be found at
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
