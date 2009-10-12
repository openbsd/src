#./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

BEGIN {
    require "./test.pl";
    plan(tests => 35);
}

use Scalar::Util qw(refaddr);

{
    package Stringifies;

    use overload (
	fallback => 1,
	'""' => sub { "foo" },
	'0+' => sub { 42 },
	cos => sub { "far side of overload table" },
    );

    sub new { bless {}, shift };
}

my $x = Stringifies->new;

is( "$x", "foo", "stringifies" );
is( 0 + $x, 42, "numifies" );
is( cos($x), "far side of overload table", "cosinusfies" );

{
    no overloading;
    is( "$x", overload::StrVal($x), "no stringification" );
    is( 0 + $x, refaddr($x), "no numification" );
    is( cos($x), cos(refaddr($x)), "no cosinusfication" );

    {
	no overloading '""';
	is( "$x", overload::StrVal($x), "no stringification" );
	is( 0 + $x, refaddr($x), "no numification" );
	is( cos($x), cos(refaddr($x)), "no cosinusfication" );
    }
}

{
    no overloading '""';

    is( "$x", overload::StrVal($x), "no stringification" );
    is( 0 + $x, 42, "numifies" );
    is( cos($x), "far side of overload table", "cosinusfies" );

    {
	no overloading;
	is( "$x", overload::StrVal($x), "no stringification" );
	is( 0 + $x, refaddr($x), "no numification" );
	is( cos($x), cos(refaddr($x)), "no cosinusfication" );
    }

    use overloading '""';

    is( "$x", "foo", "stringifies" );
    is( 0 + $x, 42, "numifies" );
    is( cos($x), "far side of overload table", "cosinusfies" );

    no overloading '0+';
    is( "$x", "foo", "stringifies" );
    is( 0 + $x, refaddr($x), "no numification" );
    is( cos($x), "far side of overload table", "cosinusfies" );

    {
	no overloading '""';
	is( "$x", overload::StrVal($x), "no stringification" );
	is( 0 + $x, refaddr($x), "no numification" );
	is( cos($x), "far side of overload table", "cosinusfies" );

	{
	    use overloading;
	    is( "$x", "foo", "stringifies" );
	    is( 0 + $x, 42, "numifies" );
	    is( cos($x), "far side of overload table", "cosinusfies" );
	}
    }

    is( "$x", "foo", "stringifies" );
    is( 0 + $x, refaddr($x), "no numification" );
    is( cos($x), "far side of overload table", "cosinusfies" );

    no overloading "cos";
    is( "$x", "foo", "stringifies" );
    is( 0 + $x, refaddr($x), "no numification" );
    is( cos($x), cos(refaddr($x)), "no cosinusfication" );

    BEGIN { ok(exists($^H{overloading}), "overloading hint present") }

    use overloading;

    BEGIN { ok(!exists($^H{overloading}), "overloading hint removed") }
}
