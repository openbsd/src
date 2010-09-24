use strict;
use Test::More 'no_plan';

BEGIN { chdir 't' if -d 't' };
use lib '../lib';

my $Class   = 'Package::Constants';
my $Func    = 'list';
my $Pkg     = '_test';
my @Good    = 'A'..'C';
my @Bad     = 'D'..'E';

use_ok( $Class );
can_ok( $Class, $Func );

### enable debug statements?
$Package::Constants::DEBUG = $Package::Constants::DEBUG = @ARGV ? 1 : 0;


### small test class 
{   package _test;

    ### mark us as loaded
    $INC{'_test.pm'} = $0;
    
    use vars qw[$FOO];
    $FOO = 1;
    
    ### define various subs.. the first 3 are constants, 
    ### the others are not
    use constant A => 1;
    use constant B => sub { 1 };
    sub C ()        { 1 };
    
    sub D           { 1 };
    sub E (*)       { 1 };

}    

### get the list
{   my @list = $Class->$Func( $Pkg );
    ok( scalar(@list),          "Got a list of constants" );
    is_deeply( \@list, \@Good,  "   Contains all expected entries" );
}    


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
