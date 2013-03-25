use strict;
use warnings;

package Software::License::VaporWare;
our $VERSION = '0.001';

use Software::License;
our @ISA = qw/Software::License/;

sub name      { 'VaporWare License' }
sub url       { 'http://example.com/vaporware/' }
sub meta_name { 'unrestricted' }
sub meta2_name { 'unrestricted' }

1;


