package Test2::Util::Table::LineBreak;
use strict;
use warnings;

our $VERSION = '0.000162';

use base 'Term::Table::LineBreak';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Table::LineBreak - Break up lines for use in tables.

=head1 DESCRIPTION

This is meant for internal use. This package takes long lines of text and
splits them so that they fit in table rows.

=head1 SYNOPSIS

    use Test2::Util::Table::LineBreak;

    my $lb = Test2::Util::Table::LineBreak->new(string => $STRING);

    $lb->break($SIZE);
    while (my $part = $lb->next) {
        ...
    }

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
