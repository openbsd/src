#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib lib);
}

BEGIN { require "./test.pl"; }

# This test depends on t/lib/Devel/switchd.pm.

plan(tests => 2);

my $r;
my @tmpfiles = ();
END { unlink @tmpfiles }

my $filename = 'swdtest.tmp';
SKIP: {
	open my $f, ">$filename"
	    or skip( "Can't write temp file $filename: $!" );
	print $f <<'__SWDTEST__';
package Bar;
sub bar { $_[0] * $_[0] }
package Foo;
sub foo {
  my $s;
  $s += Bar::bar($_) for 1..$_[0];
}
package main;
Foo::foo(3);
__SWDTEST__
    close $f;
    push @tmpfiles, $filename;
    $| = 1; # Unbufferize.
    $r = runperl(
		 switches => [ '-Ilib', '-f', '-d:switchd' ],
		 progfile => $filename,
		 args => ['3'],
		);
    like($r, qr/^sub<Devel::switchd::import>;import<Devel::switchd>;DB<main,swdtest.tmp,9>;sub<Foo::foo>;DB<Foo,swdtest.tmp,5>;DB<Foo,swdtest.tmp,6>;DB<Foo,swdtest.tmp,6>;sub<Bar::bar>;DB<Bar,swdtest.tmp,2>;sub<Bar::bar>;DB<Bar,swdtest.tmp,2>;sub<Bar::bar>;DB<Bar,swdtest.tmp,2>;$/);
    $r = runperl(
		 switches => [ '-Ilib', '-f', '-d:switchd=a,42' ],
		 progfile => $filename,
		 args => ['4'],
		);
    like($r, qr/^sub<Devel::switchd::import>;import<Devel::switchd a 42>;DB<main,swdtest.tmp,9>;sub<Foo::foo>;DB<Foo,swdtest.tmp,5>;DB<Foo,swdtest.tmp,6>;DB<Foo,swdtest.tmp,6>;sub<Bar::bar>;DB<Bar,swdtest.tmp,2>;sub<Bar::bar>;DB<Bar,swdtest.tmp,2>;sub<Bar::bar>;DB<Bar,swdtest.tmp,2>;$/);
}

