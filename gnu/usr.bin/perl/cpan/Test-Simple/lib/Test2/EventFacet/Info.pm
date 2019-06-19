package Test2::EventFacet::Info;
use strict;
use warnings;

our $VERSION = '1.302133';

sub is_list { 1 }

BEGIN { require Test2::EventFacet; our @ISA = qw(Test2::EventFacet) }
use Test2::Util::HashBase qw{-tag -debug -important};

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::EventFacet::Info - Facet for information a developer might care about.

=head1 DESCRIPTION

This facet represents messages intended for humans that will help them either
understand a result, or diagnose a failure.

=head1 NOTES

This facet appears in a list instead of being a single item.

=head1 FIELDS

=over 4

=item $string_or_structure = $info->{details}

=item $string_or_structure = $info->details()

Human readable string or data structure, this is the information to display.
Formatters are free to render the structures however they please. This may
contain a blessed object.

=item $short_string = $info->{tag}

=item $short_string = $info->tag()

Short tag to categorize the info. This is usually 10 characters or less,
formatters may truncate longer tags.

=item $bool = $info->{debug}

=item $bool = $info->debug()

Set this to true if the message is critical, or explains a failure. This is
info that should be displayed by formatters even in less-verbose modes.

When false the information is not considered critical and may not be rendered
in less-verbose modes.

=item $bool = $info->{important}

=item $bool = $info->important

This should be set for non debug messages that are still important enough to
show when a formatter is in quiet mode. A formatter should send these to STDOUT
not STDERR, but should show them even in non-verbose mode.

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
