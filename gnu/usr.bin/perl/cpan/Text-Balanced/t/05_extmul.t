# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..86\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( :ALL );
$loaded = 1;
print "ok 1\n";
$count=2;
use vars qw( $DEBUG );
sub debug { print "\t>>>",@_ if $DEBUG }

######################### End of black magic.

sub expect
{
	local $^W;
	my ($l1, $l2) = @_;

	if (@$l1 != @$l2)
	{
		print "\@l1: ", join(", ", @$l1), "\n";
		print "\@l2: ", join(", ", @$l2), "\n";
		print "not ";
	}
	else
	{
		for (my $i = 0; $i < @$l1; $i++)
		{
			if ($l1->[$i] ne $l2->[$i])
			{
				print "field $i: '$l1->[$i]' ne '$l2->[$i]'\n";
				print "not ";
				last;
			}
		}
	}

	print "ok $count\n";
	$count++;
}

sub divide
{
	my ($text, @index) = @_;
	my @bits = ();
	unshift @index, 0;
	push @index, length($text);
	for ( my $i= 0; $i < $#index; $i++)
	{
		push @bits, substr($text, $index[$i], $index[$i+1]-$index[$i]);
	}
	pop @bits;
	return @bits;

}


$stdtext1 = q{$var = do {"val" && $val;};};

# TESTS 2-4
$text = $stdtext1;
expect	[ extract_multiple($text,undef,1) ],
	[ divide $stdtext1 => 4 ];

expect [ pos $text], [ 4 ];
expect [ $text ], [ $stdtext1 ];

# TESTS 5-7
$text = $stdtext1;
expect	[ scalar extract_multiple($text,undef,1) ],
	[ divide $stdtext1 => 4 ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext1,4) ];


# TESTS 8-10
$text = $stdtext1;
expect	[ extract_multiple($text,undef,2) ],
	[ divide($stdtext1 => 4, 10) ];

expect [ pos $text], [ 10 ];
expect [ $text ], [ $stdtext1 ];

# TESTS 11-13
$text = $stdtext1;
expect	[ eval{local$^W;scalar extract_multiple($text,undef,2)} ],
	[ substr($stdtext1,0,4) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext1,4) ];


# TESTS 14-16
$text = $stdtext1;
expect	[ extract_multiple($text,undef,3) ],
	[ divide($stdtext1 => 4, 10, 26) ];

expect [ pos $text], [ 26 ];
expect [ $text ], [ $stdtext1 ];

# TESTS 17-19
$text = $stdtext1;
expect	[ eval{local$^W;scalar extract_multiple($text,undef,3)} ],
	[ substr($stdtext1,0,4) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext1,4) ];


# TESTS 20-22
$text = $stdtext1;
expect	[ extract_multiple($text,undef,4) ],
	[ divide($stdtext1 => 4, 10, 26, 27) ];

expect [ pos $text], [ 27 ];
expect [ $text ], [ $stdtext1 ];

# TESTS 23-25
$text = $stdtext1;
expect	[ eval{local$^W;scalar extract_multiple($text,undef,4)} ],
	[ substr($stdtext1,0,4) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext1,4) ];


# TESTS 26-28
$text = $stdtext1;
expect	[ extract_multiple($text,undef,5) ],
	[ divide($stdtext1 => 4, 10, 26, 27) ];

expect [ pos $text], [ 27 ];
expect [ $text ], [ $stdtext1 ];


# TESTS 29-31
$text = $stdtext1;
expect	[ eval{local$^W;scalar extract_multiple($text,undef,5)} ],
	[ substr($stdtext1,0,4) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext1,4) ];



# TESTS 32-34
$stdtext2 = q{$var = "val" && (1,2,3);};

$text = $stdtext2;
expect	[ extract_multiple($text) ],
	[ divide($stdtext2 => 4, 7, 12, 24) ];

expect [ pos $text], [ 24 ];
expect [ $text ], [ $stdtext2 ];

# TESTS 35-37
$text = $stdtext2;
expect	[ scalar extract_multiple($text) ],
	[ substr($stdtext2,0,4) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext2,4) ];


# TESTS 38-40
$text = $stdtext2;
expect	[ extract_multiple($text,[\&extract_bracketed]) ],
	[ substr($stdtext2,0,16), substr($stdtext2,16,7), substr($stdtext2,23) ];

expect [ pos $text], [ 24 ];
expect [ $text ], [ $stdtext2 ];

# TESTS 41-43
$text = $stdtext2;
expect	[ scalar extract_multiple($text,[\&extract_bracketed]) ],
	[ substr($stdtext2,0,16) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext2,15) ];


# TESTS 44-46
$text = $stdtext2;
expect	[ extract_multiple($text,[\&extract_variable]) ],
	[ substr($stdtext2,0,4), substr($stdtext2,4) ];

expect [ pos $text], [ length($text) ];
expect [ $text ], [ $stdtext2 ];

# TESTS 47-49
$text = $stdtext2;
expect	[ scalar extract_multiple($text,[\&extract_variable]) ],
	[ substr($stdtext2,0,4) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext2,4) ];


# TESTS 50-52
$text = $stdtext2;
expect	[ extract_multiple($text,[\&extract_quotelike]) ],
	[ substr($stdtext2,0,7), substr($stdtext2,7,5), substr($stdtext2,12) ];

expect [ pos $text], [ length($text) ];
expect [ $text ], [ $stdtext2 ];

# TESTS 53-55
$text = $stdtext2;
expect	[ scalar extract_multiple($text,[\&extract_quotelike]) ],
	[ substr($stdtext2,0,7) ];

expect [ pos $text], [ 0 ];
expect [ $text ], [ substr($stdtext2,6) ];


# TESTS 56-58
$text = $stdtext2;
expect	[ extract_multiple($text,[\&extract_quotelike],2,1) ],
	[ substr($stdtext2,7,5) ];

expect [ pos $text], [ 23 ];
expect [ $text ], [ $stdtext2 ];

# TESTS 59-61
$text = $stdtext2;
expect	[ eval{local$^W;scalar extract_multiple($text,[\&extract_quotelike],2,1)} ],
	[ substr($stdtext2,7,5) ];

expect [ pos $text], [ 6 ];
expect [ $text ], [ substr($stdtext2,0,6). substr($stdtext2,12) ];


# TESTS 62-64
$text = $stdtext2;
expect	[ extract_multiple($text,[\&extract_quotelike],1,1) ],
	[ substr($stdtext2,7,5) ];

expect [ pos $text], [ 12 ];
expect [ $text ], [ $stdtext2 ];

# TESTS 65-67
$text = $stdtext2;
expect	[ scalar extract_multiple($text,[\&extract_quotelike],1,1) ],
	[ substr($stdtext2,7,5) ];

expect [ pos $text], [ 6 ];
expect [ $text ], [ substr($stdtext2,0,6). substr($stdtext2,12) ];

# TESTS 68-70
my $stdtext3 = "a,b,c";

$_ = $stdtext3;
expect	[ extract_multiple(undef, [ sub { /\G[a-z]/gc && $& } ]) ],
	[ divide($stdtext3 => 1,2,3,4,5) ];

expect [ pos ], [ 5 ];
expect [ $_ ], [ $stdtext3 ];

# TESTS 71-73

$_ = $stdtext3;
expect	[ scalar extract_multiple(undef, [ sub { /\G[a-z]/gc && $& } ]) ],
	[ divide($stdtext3 => 1) ];

expect [ pos ], [ 0 ];
expect [ $_ ], [ substr($stdtext3,1) ];


# TESTS 74-76

$_ = $stdtext3;
expect	[ extract_multiple(undef, [ qr/\G[a-z]/ ]) ],
	[ divide($stdtext3 => 1,2,3,4,5) ];

expect [ pos ], [ 5 ];
expect [ $_ ], [ $stdtext3 ];

# TESTS 77-79

$_ = $stdtext3;
expect	[ scalar extract_multiple(undef, [ qr/\G[a-z]/ ]) ],
	[ divide($stdtext3 => 1) ];

expect [ pos ], [ 0 ];
expect [ $_ ], [ substr($stdtext3,1) ];


# TESTS 80-82

$_ = $stdtext3;
expect	[ extract_multiple(undef, [ q/([a-z]),?/ ]) ],
	[ qw(a b c) ];

expect [ pos ], [ 5 ];
expect [ $_ ], [ $stdtext3 ];

# TESTS 83-85

$_ = $stdtext3;
expect	[ scalar extract_multiple(undef, [ q/([a-z]),?/ ]) ],
	[ divide($stdtext3 => 1) ];

expect [ pos ], [ 0 ];
expect [ $_ ], [ substr($stdtext3,2) ];


# TEST 86

# Fails in Text-Balanced-1.95 with result ['1 ', '""', '1234']
$_ = q{ ""1234};
expect	[ extract_multiple(undef, [\&extract_quotelike]) ],
	[ ' ', '""', '1234' ];
