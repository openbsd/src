package ExtUtils::MM_DOS;

use strict;
use vars qw($VERSION @ISA);

$VERSION = 0.01;

require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
@ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );


=head1 NAME

ExtUtils::MM_DOS - DOS specific subclass of ExtUtils::MM_Unix

=head1 SYNOPSIS

  Don't use this module directly.
  Use ExtUtils::MM and let it choose.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Unix which contains functionality
for DOS.

Unless otherwise stated, it works just like ExtUtils::MM_Unix

=head2 Overridden methods

=over 4

=item B<replace_manpage_separator>

=cut

sub replace_manpage_separator {
    my($self, $man) = @_;

    $man =~ s,/+,__,g;
    return $man;
}

=back

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> with code from ExtUtils::MM_Unix

=head1 SEE ALSO

L<ExtUtils::MM_Unix>, L<ExtUtils::MakeMaker>

=cut
1;
