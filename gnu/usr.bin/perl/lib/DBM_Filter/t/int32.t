
use strict;
use warnings;
use Carp;

require "dbm_filter_util.pl";

use Test::More tests => 22;

BEGIN { use_ok('DBM_Filter') };
BEGIN { use_ok('SDBM_File') };
BEGIN { use_ok('Fcntl') };

unlink <Op_dbmx*>;
END { unlink <Op_dbmx*>; }

my %h1 = () ;
my $db1 = tie(%h1, 'SDBM_File','Op_dbmx', O_RDWR|O_CREAT, 0640) ;

ok $db1, "tied to SDBM_File";

# store before adding the filter

StoreData(\%h1,
	{	
		1234	=> 5678,
		-3	=> -5,
		"22"	=> "88",
		"-45"	=> "-88",
	});

VerifyData(\%h1,
	{
		1234	=> 5678,
		-3	=> -5,
		22	=> 88,
		-45	=> -88,
	});


eval { $db1->Filter_Push('int32') };
is $@, '', "push an 'int32' filter" ;

{
    no warnings 'uninitialized';
    StoreData(\%h1,
	{	
		undef()	=> undef(),
		"400"	=> "500",
		0	=> 1,
		1	=> 0,
		-47	=> -6,
	});

}

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
		1234	=> 5678,
		-3	=> -5,
		22	=> 88,
		-45	=> -88,

		#undef()	=> undef(),
		pack("i", 400)	=> pack("i", 500),
		pack("i", 0)	=> pack("i", 1),
		pack("i", 1)	=> pack("i", 0),
		pack("i", -47)	=> pack("i", -6),
	});

undef $db2;
{
    use warnings FATAL => 'untie';
    eval { untie %h2 };
    is $@, '', "untie without inner references" ;
}

