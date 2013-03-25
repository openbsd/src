#!./perl
#line 3 warn.t

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan 22;

my @warnings;
my $wa = []; my $ea = [];
$SIG{__WARN__} = sub { push @warnings, $_[0] };

@warnings = ();
$@ = "";
warn "foo\n";
ok @warnings==1 && $warnings[0] eq "foo\n";

@warnings = ();
$@ = "";
warn "foo", "bar\n";
ok @warnings==1 && $warnings[0] eq "foobar\n";

@warnings = ();
$@ = "";
warn "foo";
ok @warnings==1 && $warnings[0] eq "foo at warn.t line 28.\n";

@warnings = ();
$@ = "";
warn $wa;
ok @warnings==1 && ref($warnings[0]) eq "ARRAY" && $warnings[0] == $wa;

@warnings = ();
$@ = "";
warn "";
ok @warnings==1 &&
    $warnings[0] eq "Warning: something's wrong at warn.t line 38.\n";

@warnings = ();
$@ = "";
warn;
ok @warnings==1 &&
    $warnings[0] eq "Warning: something's wrong at warn.t line 44.\n";

@warnings = ();
$@ = "ERR\n";
warn "foo\n";
ok @warnings==1 && $warnings[0] eq "foo\n";

@warnings = ();
$@ = "ERR\n";
warn "foo", "bar\n";
ok @warnings==1 && $warnings[0] eq "foobar\n";

@warnings = ();
$@ = "ERR\n";
warn "foo";
ok @warnings==1 && $warnings[0] eq "foo at warn.t line 60.\n";

@warnings = ();
$@ = "ERR\n";
warn $wa;
ok @warnings==1 && ref($warnings[0]) eq "ARRAY" && $warnings[0] == $wa;

@warnings = ();
$@ = "ERR\n";
warn "";
ok @warnings==1 &&
    $warnings[0] eq "ERR\n\t...caught at warn.t line 70.\n";

@warnings = ();
$@ = "ERR\n";
warn;
ok @warnings==1 &&
    $warnings[0] eq "ERR\n\t...caught at warn.t line 76.\n";

@warnings = ();
$@ = $ea;
warn "foo\n";
ok @warnings==1 && $warnings[0] eq "foo\n";

@warnings = ();
$@ = $ea;
warn "foo", "bar\n";
ok @warnings==1 && $warnings[0] eq "foobar\n";

@warnings = ();
$@ = $ea;
warn "foo";
ok @warnings==1 && $warnings[0] eq "foo at warn.t line 92.\n";

@warnings = ();
$@ = $ea;
warn $wa;
ok @warnings==1 && ref($warnings[0]) eq "ARRAY" && $warnings[0] == $wa;

@warnings = ();
$@ = $ea;
warn "";
ok @warnings==1 && ref($warnings[0]) eq "ARRAY" && $warnings[0] == $ea;

@warnings = ();
$@ = $ea;
warn;
ok @warnings==1 && ref($warnings[0]) eq "ARRAY" && $warnings[0] == $ea;

fresh_perl_like(
 '
   $a = "\xee\n";
   print STDERR $a; warn $a;
   utf8::upgrade($a);
   print STDERR $a; warn $a;
 ',
  qr/^\xee(?:\r?\n\xee){3}/,
  { switches => [ "-C0" ] },
 'warn emits logical characters, not internal bytes [perl #45549]'  
);

SKIP: {
    skip_if_miniperl('miniperl ignores -C', 1);
fresh_perl_like(
 '
   $a = "\xee\n";
   print STDERR $a; warn $a;
   utf8::upgrade($a);
   print STDERR $a; warn $a;
 ',
  qr/^\xc3\xae(?:\r?\n\xc3\xae){3}/,
  { switches => ['-CE'] },
 'warn respects :utf8 layer'
);
}

fresh_perl_like(
 'warn chr 300',
  qr/^Wide character in warn .*\n\xc4\xac at /,
  { switches => [ "-C0" ] },
 'Wide character in warn (not print)'
);

fresh_perl_like(
 'warn []',
  qr/^ARRAY\(0x[\da-f]+\) at /a,
  { },
 'warn stringifies in the absence of $SIG{__WARN__}'
);

1;
