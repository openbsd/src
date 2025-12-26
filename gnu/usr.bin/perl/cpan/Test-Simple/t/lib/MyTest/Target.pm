package MyTest::Target;

use Carp qw/confess/;

use overload bool => sub { confess( 'illegal use of overloaded bool') } ;
use overload '""' => sub { $_[0] };

1;
