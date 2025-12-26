use strict;
use warnings;

package SmallTest;

require Exporter;

our @ISA = qw( Exporter );
our @EXPORT = qw( ok is_eq is_num );

use Test::Builder;

my $Test = Test::Builder->new;

sub ok
{
	$Test->ok(@_);
}

sub is_eq
{
	$Test->is_eq(@_);
}

sub is_num
{
	$Test->is_num(@_);
}

sub getTest
{
	return $Test;
}
1;
