
use strict;
use warnings;
use Carp;

BEGIN 
{

    eval { require Encode; };
    
    if ($@) {
        print "1..0 #  Skip: Encode is not available\n";
        exit 0;
    }
}

require "dbm_filter_util.pl";

use Test::More tests => 20;

BEGIN { use_ok('DBM_Filter') };
BEGIN { use_ok('SDBM_File') };
BEGIN { use_ok('Fcntl') };
BEGIN { use_ok('charnames', qw{greek})};

use charnames qw{greek};

unlink <Op_dbmx*>;
END { unlink <Op_dbmx*>; }

my %h1 = () ;
my $db1 = tie(%h1, 'SDBM_File','Op_dbmx', O_RDWR|O_CREAT, 0640) ;

ok $db1, "tied to SDBM_File";

eval { $db1->Filter_Push('utf8') };
is $@, '', "push a 'utf8' filter" ;

{
    no warnings 'uninitialized';
    StoreData(\%h1,
	{	
		undef()	=> undef(),
		"beta"	=> "\N{beta}",
		'alpha'	=> "\N{alpha}",
		"\N{gamma}"=> "gamma",
	});

}

VerifyData(\%h1,
	{
		'alpha'	=> "\N{alpha}",
		"beta"	=> "\N{beta}",
		"\N{gamma}"=> "gamma",
		""		=> "",
	});

undef $db1;
{
    use warnings FATAL => 'untie';
    eval { untie %h1 };
    is $@, '', "untie without inner references" ;
}

# read the dbm file without the filter
my %h2 = () ;
my $db2 = tie(%h2, 'SDBM_File','Op_dbmx', O_RDWR|O_CREAT, 0640) ;

ok $db2, "tied to SDBM_File";

VerifyData(\%h2,
	{
		'alpha'	=> "\xCE\xB1",
		'beta'	=> "\xCE\xB2",
		"\xCE\xB3"=> "gamma",
		""		=> "",
	});

undef $db2;
{
    use warnings FATAL => 'untie';
    eval { untie %h2 };
    is $@, '', "untie without inner references" ;
}

