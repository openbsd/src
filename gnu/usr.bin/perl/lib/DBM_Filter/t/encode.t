
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

use Test::More tests => 26;

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

eval { $db1->Filter_Push('encode' => 'blah') };
like $@, qr/^Encoding 'blah' is not available/, "push an illigal filter" ;

eval { $db1->Filter_Push('encode') };
is $@, '', "push an 'encode' filter (default to utf-8)" ;


{
    no warnings 'uninitialized';
    StoreData(\%h1,
	{	
		undef()	=> undef(),
		'alpha'	=> "\N{alpha}",
		"\N{gamma}"=> "gamma",
		"beta"	=> "\N{beta}",
	});

}

VerifyData(\%h1,
	{
		'alpha'	=> "\N{alpha}",
		"beta"	=> "\N{beta}",
		"\N{gamma}"=> "gamma",
		""		=> "",
	});

eval { $db1->Filter_Pop() };
is $@, '', "pop the 'utf8' filter" ;

eval { $db1->Filter_Push('encode' => 'iso-8859-16') };
is $@, '', "push an 'encode' filter (specify iso-8859-16)" ;

use charnames qw{:full};
StoreData(\%h1,
	{	
		'euro'	=> "\N{EURO SIGN}",
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
		'euro'	=> "\xA4",
		""		=> "",
	});

undef $db2;
{
    use warnings FATAL => 'untie';
    eval { untie %h2 };
    is $@, '', "untie without inner references" ;
}

