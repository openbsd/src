#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib lib);
}

require "./test.pl";

plan(tests => 1);

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
		 switches => [ '-Ilib', '-d:switchd' ],
		 progfile => $filename,
		);
    like($r, qr/^main,swdtest.tmp,9;Foo,swdtest.tmp,5;Foo,swdtest.tmp,6;Foo,swdtest.tmp,6;Bar,swdtest.tmp,2;Bar,swdtest.tmp,2;Bar,swdtest.tmp,2;$/i);
}

