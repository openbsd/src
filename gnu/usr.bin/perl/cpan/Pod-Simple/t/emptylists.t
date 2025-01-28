use strict;
use warnings;
use Test::More tests => 3;

use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";

my $x = 'Pod::Simple::XMLOutStream';
$Pod::Simple::XMLOutStream::ATTR_PAD   = ' ';
$Pod::Simple::XMLOutStream::SORT_ATTRS = 1; # for predictably testable output

sub on {shift->parse_empty_lists(1)}
sub off {shift->parse_empty_lists(0)}

my $pod = <<POD;
=over

=over

=over

=over

=back

=over

=back

=back

=back

=back
POD

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

print "# Testing default behavior (parse_empty_lists( FALSE )) ...\n";

is( $x->_out($pod),
  '<Document><over-block indent="4"><over-block indent="4"><over-block indent="4"></over-block></over-block></over-block></Document>'
);

print "# Testing explicit parse_empty_lists( FALSE ) ...\n";

is( $x->_out(\&off, $pod),
  '<Document><over-block indent="4"><over-block indent="4"><over-block indent="4"></over-block></over-block></over-block></Document>'
);

print "# Testing parse_empty_lists( TRUE ) ...\n";

is( $x->_out(\&on, $pod),
  '<Document><over-block indent="4"><over-block indent="4"><over-block indent="4"><over-empty indent="4"></over-empty><over-empty indent="4"></over-empty></over-block></over-block></over-block></Document>'
);
