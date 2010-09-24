#!./perl -IFoo::Bar -IBla

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
    require './test.pl';	# for which_perl() etc
}

BEGIN {
    plan(4);
}

my $Is_VMS   = $^O eq 'VMS';
my $lib;

$lib = 'Bla';
ok(grep { $_ eq $lib } @INC[0..($#INC-1)]);
SKIP: {
  skip 'Double colons not allowed in dir spec', 1 if $Is_VMS;
  $lib = 'Foo::Bar';
  ok(grep { $_ eq $lib } @INC[0..($#INC-1)]);
}

$lib = 'Bla2';
fresh_perl_is("print grep { \$_ eq '$lib' } \@INC[0..(\$#INC-1)]", $lib,
	      { switches => ['-IBla2'] }, '-I');
SKIP: {
  skip 'Double colons not allowed in dir spec', 1 if $Is_VMS;
  $lib = 'Foo::Bar2';
  fresh_perl_is("print grep { \$_ eq '$lib' } \@INC", $lib,
	        { switches => ['-IFoo::Bar2'] }, '-I with colons');
}
