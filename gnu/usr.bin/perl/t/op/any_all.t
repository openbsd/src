#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}

use feature qw( keyword_any keyword_all );
no warnings qw( experimental::keyword_any experimental::keyword_all );

# Basic true/false testing
ok(  (any { $_ > 10 } 1 .. 20), 'list contains a value above ten' );
ok( !(any { $_ > 10 } 1 .. 9), 'list does not contain a value above ten' );
ok( !(all { $_ < 10 } 1 .. 20), 'not all list values below ten' );
ok(  (all { $_ < 10 } 1 .. 9), 'all list values below ten' );

# Parens are permitted
ok( (any( { $_ > 10 } 1 .. 20) ), 'list contains a value above ten' );
ok( (all( { $_ < 10 } 1 .. 9) ), 'all list values below ten' );

# any empty list is false
{
   my $invoked;
   my $ret = any { $invoked++ } ();
   ok( defined $ret, 'any on empty list is defined' );
   ok( !$ret, 'any on empty list is false' );
   ok( !$invoked, 'any on empty list did not invoke block' );
}

# all empty list is true
{
   my $invoked;
   ok( (all { $invoked } ()), 'all on empty list is true' );
   ok( !$invoked, 'all on empty list did not invoke block' );
}

# any failure yields false in list context
{
   my @ret;
   @ret = any { $_ > 10 } 1 .. 9;
   ok( @ret == 1, 'any nothing yielded a value in list context' );
   ok( !$ret[0], 'any nothing yielded false in list context' );

   @ret = any { $_ > 10 } ();
   ok( @ret == 1, 'any nothing yielded a value in list context on empty input' );
   ok( !$ret[0], 'any nothing yielded false in list context on empty input' );
}

# all failure yields true in list context
{
   my @ret;
   @ret = all { $_ > 10 } 1 .. 9;
   ok( @ret == 1, 'all nothing yielded a value in list context' );
   ok( !$ret[0], 'all nothing yielded false in list context' );

   @ret = all { $_ > 10 } ();
   ok( @ret == 1, 'all nothing yielded a value in list context on empty input' );
   ok( $ret[0], 'all nothing yielded true in list context on empty input' );
}

# short-circuiting
{
   my @seen;
   any { push @seen, $_; $_ > 10 } 10, 20, 30, 40;
   ok( eq_array( \@seen, [ 10, 20 ] ), 'any short-circuits after first true result' );

   undef @seen;
   all { push @seen, $_; $_ < 20 } 10, 20, 30, 40;
   ok( eq_array( \@seen, [ 10, 20 ] ), 'all short-circuits after first false result' );
}

# stack discipline
{
   ok( eq_array(
        [ 1, 2, (any { $_ eq "x" } "x", "y"), 3, 4 ],
        [ 1, 2, 1, 3, 4 ] ),
    'any() preserves stack discipline' );

   ok( eq_array(
        [ 1, 2, (all { $_ eq "x" } "x", "x"), 3, 4 ],
        [ 1, 2, 1, 3, 4 ] ),
    'all() preserves stack discipline' );
}

# $_ aliases
{
    my @nums = ( 1 .. 5 );
    any { $_++; 0 } @nums;
    ok( eq_array( \@nums, [ 2 .. 6 ] ),
        '$_ holds a mutable alias to original data' );
}

done_testing;
