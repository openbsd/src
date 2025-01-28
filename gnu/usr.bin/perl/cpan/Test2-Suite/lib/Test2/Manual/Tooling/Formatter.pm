package Test2::Manual::Tooling::Formatter;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Formatter - How to write a custom formatter, in our
case a JSONL formatter.

=head1 DESCRIPTION

This tutorial explains a minimal formatter that outputs each event as a json
string on its own line. A true formatter will probably be significantly more
complicated, but this will give you the basics needed to get started.

=head1 COMPLETE CODE UP FRONT

    package Test2::Formatter::MyFormatter;
    use strict;
    use warnings;

    use JSON::MaybeXS qw/encode_json/;

    use base qw/Test2::Formatter/;

    sub new { bless {}, shift }

    sub encoding {};

    sub write {
        my ($self, $e, $num, $f) = @_;
        $f ||= $e->facet_data;

        print encode_json($f), "\n";
    }

    1;

=head1 LINE BY LINE

=over 4

=item use base qw/Test2::Formatter/;

All formatters should inherit from L<Test2::Formatter>.

=item sub new { bless {}, shift }

Formatters need to be instantiable objects, this is a minimal C<new()> method.

=item sub encoding {};

For this example we leave this sub empty. In general you should implement this
sub to make sure you honor situations where the encoding is set. L<Test2::V0>
itself will try to set the encoding to UTF8.

=item sub write { ... }

The C<write()> method is the most important, each event is sent here.

=item my ($self, $e, $num, $f) = @_;

The C<write()> method receives 3 or 4 arguments, the fourth is optional.

=over 4

=item $self

The formatter itself.

=item $e

The event being written

=item $num

The most recent assertion number. If the event being processed is an assertion
then this will have been bumped by 1 since the last call to write. For non
assertions this number is set to the most recent assertion.

=item $f

This MAY be a hashref containing all the facet data from the event. More often
then not this will be undefined. This is only set if the facet data was needed
by the hub, and it usually is not.

=back

=item $f ||= $e->facet_data;

We want to dump the event facet data. This will set C<$f> to the facet data
unless we already have the facet data.

=item print encode_json($f), "\n";

This line prints the JSON encoded facet data, and a newline.

=back

=head1 SEE ALSO

L<Test2::Manual> - Primary index of the manual.

=head1 SOURCE

The source code repository for Test2-Manual can be found at
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
