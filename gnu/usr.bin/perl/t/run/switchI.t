#!./perl -IFoo::Bar -IBla

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
    require './test.pl';	# for which_perl() etc
}

BEGIN {
    plan(4);
}

my $Is_MacOS = $^O eq 'MacOS';
my $Is_VMS   = $^O eq 'VMS';
my $lib;

$lib = $Is_MacOS ? ':Bla:' : 'Bla';
ok(grep { $_ eq $lib } @INC);
SKIP: {
  skip 'Double colons not allowed in dir spec', 1 if $Is_VMS;
  $lib = $Is_MacOS ? 'Foo::Bar:' : 'Foo::Bar';
  ok(grep { $_ eq $lib } @INC);
}

$lib = $Is_MacOS ? ':Bla2:' : 'Bla2';
fresh_perl_is("print grep { \$_ eq '$lib' } \@INC", $lib,
	      { switches => ['-IBla2'] }, '-I');
SKIP: {
  skip 'Double colons not allowed in dir spec', 1 if $Is_VMS;
  $lib = $Is_MacOS ? 'Foo::Bar2:' : 'Foo::Bar2';
  fresh_perl_is("print grep { \$_ eq '$lib' } \@INC", $lib,
	        { switches => ['-IFoo::Bar2'] }, '-I with colons');
}
