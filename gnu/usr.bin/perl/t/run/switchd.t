#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib lib);
}

BEGIN { require "./test.pl"; }

# This test depends on t/lib/Devel/switchd*.pm.

plan(tests => 10);

my $r;

my $filename = tempfile();
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
    $| = 1; # Unbufferize.
    $r = runperl(
		 switches => [ '-Ilib', '-f', '-d:switchd' ],
		 progfile => $filename,
		 args => ['3'],
		);
    like($r,
qr/^sub<Devel::switchd::import>;import<Devel::switchd>;DB<main,$::tempfile_regexp,9>;sub<Foo::foo>;DB<Foo,$::tempfile_regexp,5>;DB<Foo,$::tempfile_regexp,6>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;$/,
    'Got debugging output: 1');
    $r = runperl(
		 switches => [ '-Ilib', '-f', '-d:switchd=a,42' ],
		 progfile => $filename,
		 args => ['4'],
		);
    like($r,
qr/^sub<Devel::switchd::import>;import<Devel::switchd a 42>;DB<main,$::tempfile_regexp,9>;sub<Foo::foo>;DB<Foo,$::tempfile_regexp,5>;DB<Foo,$::tempfile_regexp,6>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;$/,
    'Got debugging output: 2');
    $r = runperl(
		 switches => [ '-Ilib', '-f', '-d:-switchd=a,42' ],
		 progfile => $filename,
		 args => ['4'],
		);
    like($r,
qr/^sub<Devel::switchd::unimport>;unimport<Devel::switchd a 42>;DB<main,$::tempfile_regexp,9>;sub<Foo::foo>;DB<Foo,$::tempfile_regexp,5>;DB<Foo,$::tempfile_regexp,6>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;sub<Bar::bar>;DB<Bar,$::tempfile_regexp,2>;$/,
    'Got debugging output: 3');
}

# [perl #71806]
cmp_ok(
  runperl(       # less is useful for something :-)
   switches => [ '"-Mless ++INC->{q-Devel/_.pm-}"' ],
   progs    => [
    '#!perl -d:_',
    'sub DB::DB{} print scalar @{q/_</.__FILE__}',
   ],
  ),
 '>',
  0,
 'The debugger can see the lines of the main program under #!perl -d',
);

# [perl #48332]
like(
  runperl(
   switches => [ '-Ilib', '-d:switchd_empty' ],
   progs    => [
    'sub foo { print qq _1\n_ }',
    '*old_foo = \&foo;',
    '*foo = sub { print qq _2\n_ };',
    'old_foo(); foo();',
   ],
  ),
  qr "1\r?\n2\r?\n",
 'Subroutine redefinition works in the debugger [perl #48332]',
);

# [rt.cpan.org #69862]
like(
  runperl(
   switches => [ '-Ilib', '-d:switchd_empty' ],
   progs    => [
    'sub DB::sub { goto &$DB::sub }',
    'sub foo { print qq _1\n_ }',
    'sub bar { print qq _2\n_ }',
    'delete $::{foo}; eval { foo() };',
    'my $bar = *bar; undef *bar; eval { &$bar };',
   ],
  ),
  qr "1\r?\n2\r?\n",
 'Subroutines no longer found under their names can be called',
);

# [rt.cpan.org #69862]
like(
  runperl(
   switches => [ '-Ilib', '-d:switchd_empty' ],
   progs    => [
    'sub DB::sub { goto &$DB::sub }',
    'sub foo { goto &bar::baz; }',
    'sub bar::baz { print qq _ok\n_ }',
    'delete $::{bar::::};',
    'foo();',
   ],
  ),
  qr "ok\r?\n",
 'No crash when calling orphaned subroutine via goto &',
);

# test when DB::DB is seen but not defined [perl #114990]
like(
  runperl(
    switches => [ '-Ilib', '-d:nodb' ],
    prog     => [ '1' ],
    stderr   => 1,
  ),
  qr/^No DB::DB routine defined/,
  "No crash when *DB::DB exists but not &DB::DB",
);
like(
  runperl(
    switches => [ '-Ilib' ],
    prog     => 'sub DB::DB; BEGIN { $^P = 0x22; } for(0..9){ warn }',
    stderr   => 1,
  ),
  qr/^No DB::DB routine defined/,
  "No crash when &DB::DB exists but isn't actually defined",
);

# [perl #115742] Recursive DB::DB clobbering its own pad
like(
  runperl(
    switches => [ '-Ilib' ],
    progs    => [ split "\n", <<'='
     BEGIN {
      $^P = 0x22;
     }
     package DB;
     sub DB {
      my $x = 42;
      return if $__++;
      $^D |= 1 << 30; # allow recursive calls
      main::foo();
      print $x//q-u-, qq-\n-;
     }
     package main;
     chop;
     sub foo { chop; }
=
    ],
    stderr   => 1,
  ),
  qr/42/,
  "Recursive DB::DB does not clobber its own pad",
);
