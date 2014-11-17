#!./perl

BEGIN {
    $| = 1;
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    plan (tests => 187);
}

# Test that defined() returns true for magic variables created on the fly,
# even before they have been created.
# This must come first, even before turning on warnings or setting up
# $SIG{__WARN__}, to avoid invalidating the tests.  warnings.pm currently
# does not mention any special variables, but that could easily change.
BEGIN {
    # not available in miniperl
    my %non_mini = map { $_ => 1 } qw(+ - [);
    for (qw(
	SIG ^OPEN ^TAINT ^UNICODE ^UTF8LOCALE ^WARNING_BITS 1 2 3 4 5 6 7 8
	9 42 & ` ' : ? ! _ - [ ^ ~ = % . ( ) < > \ / $ | + ; ] ^A ^C ^D
	^E ^F ^H ^I ^L ^N ^O ^P ^S ^T ^V ^W ^UTF8CACHE ::12345 main::98732
	^LAST_FH
    )) {
	my $v = $_;
	# avoid using any global vars here:
	if ($v =~ s/^\^(?=.)//) {
	    for(substr $v, 0, 1) {
		$_ = chr ord() - 64;
	    }
	}
	SKIP:
	{
	    skip_if_miniperl("the module for *$_ may not be available in "
			     . "miniperl", 1) if $non_mini{$_};
	    ok defined *$v, "*$_ appears to be defined at the outset";
	}
    }
}

# This must be in a separate BEGIN block, as the mere mention of ${^TAINT}
# will invalidate the test for it.
BEGIN {
    $ENV{PATH} = '/bin' if ${^TAINT};
    $SIG{__WARN__} = sub { die "Dying on warning: ", @_ };
}

use warnings;
use Config;


$Is_MSWin32  = $^O eq 'MSWin32';
$Is_NetWare  = $^O eq 'NetWare';
$Is_VMS      = $^O eq 'VMS';
$Is_Dos      = $^O eq 'dos';
$Is_os2      = $^O eq 'os2';
$Is_Cygwin   = $^O eq 'cygwin';

$PERL =
   ($Is_NetWare ? 'perl'   :
    $Is_VMS     ? $^X      :
    $Is_MSWin32 ? '.\perl' :
                  './perl');

sub env_is {
    my ($key, $val, $desc) = @_;

    use open IN => ":raw";
    if ($Is_MSWin32) {
        # cmd.exe will echo 'variable=value' but 4nt will echo just the value
        # -- Nikola Knezevic
	require Win32;
	my $cp = Win32::GetConsoleOutputCP();
	Win32::SetConsoleOutputCP(Win32::GetACP());
        (my $set = `set $key 2>nul`) =~ s/\r\n$/\n/;
	Win32::SetConsoleOutputCP($cp);
        like $set, qr/^(?:\Q$key\E=)?\Q$val\E$/, $desc;
    } elsif ($Is_VMS) {
        my $eqv = `write sys\$output f\$trnlnm("\Q$key\E")`;
        # A single null byte in the equivalence string means
        # an undef value for Perl, so mimic that here.
        $eqv = "\n" if length($eqv) == 2 and $eqv eq "\000\n";
        is $eqv, "$val\n", $desc;
    } else {
        my @env = `env`;
        SKIP: {
            skip("env doesn't work on this android", 1) if !@env && $^O =~ /android/;
            chomp (my @env = grep { s/^$key=// } @env);
            is "@env", $val, $desc;
        }
    }
}

END {
    # On VMS, environment variable changes are peristent after perl exits
    if ($Is_VMS) {
        delete $ENV{'FOO'};
        delete $ENV{'__NoNeSuCh'};
        delete $ENV{'__NoNeSuCh2'};
    }
}

eval '$ENV{"FOO"} = "hi there";';	# check that ENV is inited inside eval
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
if ($Is_MSWin32)  { like `set FOO`, qr/^(?:FOO=)?hi there$/; }
elsif ($Is_VMS)   { is `write sys\$output f\$trnlnm("FOO")`, "hi there\n"; }
else              { is `echo \$FOO`, "hi there\n"; }

unlink_all 'ajslkdfpqjsjfk';
$! = 0;
open(FOO,'ajslkdfpqjsjfk');
isnt($!, 0, "Unlinked file can't be opened");
close FOO; # just mention it, squelch used-only-once

SKIP: {
    skip('SIGINT not safe on this platform', 5)
	if $Is_MSWin32 || $Is_NetWare || $Is_Dos;
  # the next tests are done in a subprocess because sh spits out a
  # newline onto stderr when a child process kills itself with SIGINT.
  # We use a pipe rather than system() because the VMS command buffer
  # would overflow with a command that long.

    # For easy interpolation of test numbers:
    $next_test = curr_test() - 1;
    sub TIEARRAY {bless[]}
    sub FETCH { $next_test + pop }
    tie my @tn, __PACKAGE__;

    open( CMDPIPE, "|-", $PERL);

    print CMDPIPE "\$t1 = $tn[1]; \$t2 = $tn[2];\n", <<'END';

    $| = 1;		# command buffering

    $SIG{"INT"} = "ok1";     kill "INT",$$; sleep 1;
    $SIG{"INT"} = "IGNORE";  kill "INT",$$; sleep 1; print "ok $t2\n";
    $SIG{"INT"} = "DEFAULT"; kill "INT",$$; sleep 1; print" not ok $t2\n";

    sub ok1 {
	if (($x = pop(@_)) eq "INT") {
	    print "ok $t1\n";
	}
	else {
	    print "not ok $t1 ($x @_)\n";
	}
    }

END

    close CMDPIPE;

    open( CMDPIPE, "|-", $PERL);
    print CMDPIPE "\$t3 = $tn[3];\n", <<'END';

    { package X;
	sub DESTROY {
	    kill "INT",$$;
	}
    }
    sub x {
	my $x=bless [], 'X';
	return sub { $x };
    }
    $| = 1;		# command buffering
    $SIG{"INT"} = "ok3";
    {
	local $SIG{"INT"}=x();
	print ""; # Needed to expose failure in 5.8.0 (why?)
    }
    sleep 1;
    delete $SIG{"INT"};
    kill "INT",$$; sleep 1;
    sub ok3 {
	print "ok $t3\n";
    }
END
    close CMDPIPE;
    $? >>= 8 if $^O eq 'VMS'; # POSIX status hiding in 2nd byte
    my $todo = ($^O eq 'os2' ? ' # TODO: EMX v0.9d_fix4 bug: wrong nibble? ' : '');
    $todo = ($Config{usecrosscompile} ? '# TODO: Not sure whats going on here when cross-compiling' : '');
    print $? & 0xFF ? "ok $tn[4]$todo\n" : "not ok $tn[4]$todo\n";

    open(CMDPIPE, "|-", $PERL);
    print CMDPIPE <<'END';

    sub PVBM () { 'foo' }
    index 'foo', PVBM;
    my $pvbm = PVBM;

    sub foo { exit 0 }

    $SIG{"INT"} = $pvbm;
    kill "INT", $$; sleep 1;
END
    close CMDPIPE;
    $? >>= 8 if $^O eq 'VMS';
    print $? ? "not ok $tn[5]\n" : "ok $tn[5]\n";

    curr_test(curr_test() + 5);
}

# can we slice ENV?
@val1 = @ENV{keys(%ENV)};
@val2 = values(%ENV);
is join(':',@val1), join(':',@val2);
cmp_ok @val1, '>', 1;

# deleting $::{ENV}
is runperl(prog => 'delete $::{ENV}; chdir; print qq-ok\n-'), "ok\n",
  'deleting $::{ENV}';

# regex vars
'foobarbaz' =~ /b(a)r/;
is $`, 'foo';
is $&, 'bar';
is $', 'baz';
is $+, 'a';

# [perl #24237]
for (qw < ` & ' >) {
 fresh_perl_is
  qq < \@$_; q "fff" =~ /(?!^)./; print "[\$$_]\\n" >,
  "[f]\n", {},
  "referencing \@$_ before \$$_ etc. still saws off ampersands";
}

# $"
@a = qw(foo bar baz);
is "@a", "foo bar baz";
{
    local $" = ',';
    is "@a", "foo,bar,baz";
}

# $;
%h = ();
$h{'foo', 'bar'} = 1;
is((keys %h)[0], "foo\034bar");
{
    local $; = 'x';
    %h = ();
    $h{'foo', 'bar'} = 1;
    is((keys %h)[0], 'fooxbar');
}

# $?, $@, $$
system qq[$PERL "-I../lib" -e "use vmsish qw(hushed); exit(0)"];
is $?, 0;
system qq[$PERL "-I../lib" -e "use vmsish qw(hushed); exit(1)"];
isnt $?, 0;

eval { die "foo\n" };
is $@, "foo\n";

ok !*@{HASH}, 'no %@';

cmp_ok($$, '>', 0);
my $pid = $$;
eval { $$ = 42 };
is $$, 42, '$$ can be modified';
SKIP: {
    skip "no fork", 1 unless $Config{d_fork};
    (my $kidpid = open my $fh, "-|") // skip "cannot fork: $!", 1;
    if($kidpid) { # parent
	my $kiddollars = <$fh>;
	close $fh or die "cannot close pipe from kid proc: $!";
	is $kiddollars, $kidpid, '$$ is reset on fork';
    }
    else { # child
	print $$;
	$::NO_ENDING = 1; # silence "Looks like you only ran..."
	exit;
    }
}
$$ = $pid; # Tests below use $$

# $^X and $0
{
    my $is_abs = $Config{d_procselfexe} || $Config{usekernprocpathname}
      || $Config{usensgetexecutablepath};
    if ($^O eq 'qnx') {
	chomp($wd = `/usr/bin/fullpath -t`);
    }
    elsif($^O =~ /android/) {
        chomp($wd = `sh -c 'pwd'`);
    }
    elsif($Is_Cygwin || $is_abs) {
       # Cygwin turns the symlink into the real file
       chomp($wd = `pwd`);
       $wd =~ s#/t$##;
       $wd =~ /(.*)/; $wd = $1; # untaint
       if ($Is_Cygwin) {
	   $wd = Cygwin::win_to_posix_path(Cygwin::posix_to_win_path($wd, 1));
       }
    }
    elsif($Is_os2) {
       $wd = Cwd::sys_cwd();
    }
    else {
	$wd = '.';
    }
    my $perl = $Is_VMS || $is_abs ? $^X : "$wd/perl";
    my $headmaybe = '';
    my $middlemaybe = '';
    my $tailmaybe = '';
    $script = "$wd/show-shebang";
    if ($Is_MSWin32) {
	chomp($wd = `cd`);
	$wd =~ s|\\|/|g;
	$perl = "$wd/perl.exe";
	$script = "$wd/show-shebang.bat";
	$headmaybe = <<EOH ;
\@rem ='
\@echo off
$perl -x \%0
goto endofperl
\@rem ';
EOH
	$tailmaybe = <<EOT ;

__END__
:endofperl
EOT
    }
    elsif ($Is_os2) {
      $script = "./show-shebang";
    }
    elsif ($Is_VMS) {
      $script = "[]show-shebang";
    }
    elsif ($Is_Cygwin) {
      $middlemaybe = <<'EOX'
$^X = Cygwin::win_to_posix_path(Cygwin::posix_to_win_path($^X, 1));
$0 = Cygwin::win_to_posix_path(Cygwin::posix_to_win_path($0, 1));
EOX
    }
    if ($^O eq 'os390' or $^O eq 'posix-bc') {  # no shebang
	$headmaybe = <<EOH ;
    eval 'exec ./perl -S \$0 \${1+"\$\@"}'
        if 0;
EOH
    }
    $s1 = "\$^X is $perl, \$0 is $script\n";
    ok open(SCRIPT, ">$script") or diag "Can't write to $script: $!";
    ok print(SCRIPT $headmaybe . <<EOB . $middlemaybe . <<'EOF' . $tailmaybe) or diag $!;
#!$perl
EOB
print "\$^X is $^X, \$0 is $0\n";
EOF
    ok close(SCRIPT) or diag $!;
    ok chmod(0755, $script) or diag $!;
    $_ = $Is_VMS ? `$perl $script` : `$script`;
    s/\.exe//i if $Is_Dos or $Is_Cygwin or $Is_os2;
    s{is perl}{is $perl}; # for systems where $^X is only a basename
    s{\\}{/}g;
    if ($Is_MSWin32 || $Is_os2) {
	is uc $_, uc $s1;
    } else {
  SKIP:
     {
	  skip "# TODO: Hit bug posix-2058; exec does not setup argv[0] correctly." if ($^O eq "vos");
	  is $_, $s1;
     }
    }
    $_ = `$perl $script`;
    s/\.exe//i if $Is_Dos or $Is_os2 or $Is_Cygwin;
    s{\\}{/}g;
    if ($Is_MSWin32 || $Is_os2) {
	is uc $_, uc $s1;
    } else {
	is $_, $s1;
    }
    ok unlink($script) or diag $!;
    # CHECK
    # Could this be replaced with:
    # unlink_all($script);
}

# $], $^O, $^T
cmp_ok $], '>=', 5.00319;
ok $^O;
cmp_ok $^T, '>', 850000000;

# Test change 25062 is working
my $orig_osname = $^O;
{
local $^I = '.bak';
is $^O, $orig_osname, 'Assigning $^I does not clobber $^O';
}
$^O = $orig_osname;

{
    #RT #72422
    foreach my $p (0, 1) {
	fresh_perl_is(<<"EOP", '2 4 8', undef, "test \$^P = $p");
\$DB::single = 2;
\$DB::trace = 4;
\$DB::signal = 8;
\$^P = $p;
print "\$DB::single \$DB::trace \$DB::signal";
EOP
    }
}

# Check that assigning to $0 on Linux sets the process name with both
# argv[0] assignment and by calling prctl()
{
  SKIP: {
    skip "We don't have prctl() here, or we're on Android", 2 unless $Config{d_prctl_set_name} && $^O ne 'android';

    # We don't really need these tests. prctl() is tested in the
    # Kernel, but test it anyway for our sanity. If something doesn't
    # work (like if the system doesn't have a ps(1) for whatever
    # reason) just bail out gracefully.
    my $maybe_ps = sub {
        my ($cmd) = @_;
        local ($?, $!);

        no warnings;
        my $res = `$cmd`;
        skip "Couldn't shell out to '$cmd', returned code $?", 2 if $?;
        return $res;
    };

    my $name = "Good Morning, Dave";
    $0 = $name;

    chomp(my $argv0 = $maybe_ps->("ps h $$"));
    chomp(my $prctl = $maybe_ps->("ps hc $$"));

    like($argv0, $name, "Set process name through argv[0] ($argv0)");
    like($prctl, substr($name, 0, 15), "Set process name through prctl() ($prctl)");
  }
}

{
    my $ok = 1;
    my $warn = '';
    local $SIG{'__WARN__'} = sub { $ok = 0; $warn = join '', @_; $warn =~ s/\n$//; };
    $! = undef;
    local $TODO = $Is_VMS ? "'\$!=undef' does throw a warning" : '';
    ok($ok, $warn);
}

SKIP: {
    skip_if_miniperl("miniperl can't rely on loading %Errno", 2);
   no warnings 'void';

# Make sure Errno hasn't been prematurely autoloaded

   ok !keys %Errno::;

# Test auto-loading of Errno when %! is used

   ok scalar eval q{
      %!;
      scalar %Errno::;
   }, $@;
}

SKIP:  {
    skip_if_miniperl("miniperl can't rely on loading %Errno", 2);
    # Make sure that Errno loading doesn't clobber $!

    undef %Errno::;
    delete $INC{"Errno.pm"};

    open(FOO, "nonesuch"); # Generate ENOENT
    my %errs = %{"!"}; # Cause Errno.pm to be loaded at run-time
    ok ${"!"}{ENOENT};

    # Make sure defined(*{"!"}) before %! does not stop %! from working
    is
      runperl(
	prog => 'BEGIN { defined *{q-!-} } print qq-ok\n- if tied %!',
      ),
     "ok\n",
     'defined *{"!"} does not stop %! from working';
}

# Check that we don't auto-load packages
foreach (['powie::!', 'Errno'],
	 ['powie::+', 'Tie::Hash::NamedCapture']) {
    my ($symbol, $package) = @$_;
    SKIP: {
	(my $extension = $package) =~ s|::|/|g;
	skip "$package is statically linked", 2
	    if $Config{static_ext} =~ m|\b\Q$extension\E\b|;
	foreach my $scalar_first ('', '$$symbol;') {
	    my $desc = qq{Referencing %{"$symbol"}};
	    $desc .= qq{ after mentioning \${"$symbol"}} if $scalar_first;
	    $desc .= " doesn't load $package";

	    fresh_perl_is(<<"EOP", 0, {}, $desc);
use strict qw(vars subs);
my \$symbol = '$symbol';
$scalar_first;
1 if %{\$symbol};
print scalar %${package}::;
EOP
	}
    }
}

is $^S, 0;
eval { is $^S,1 };
eval " BEGIN { ok ! defined \$^S } ";
is $^S, 0;

my $taint = ${^TAINT};
is ${^TAINT}, $taint;
eval { ${^TAINT} = 1 };
is ${^TAINT}, $taint;

# 5.6.1 had a bug: @+ and @- were not properly interpolated
# into double-quoted strings
# 20020414 mjd-perl-patch+@plover.com
"I like pie" =~ /(I) (like) (pie)/;
is "@-",  "0 0 2 7";
is "@+", "10 1 6 10";

# Tests for the magic get of $\
{
    my $ok = 0;
    # [perl #19330]
    {
	local $\ = undef;
	$\++; $\++;
	$ok = $\ eq 2;
    }
    ok $ok;
    $ok = 0;
    {
	local $\ = "a\0b";
	$ok = "a$\b" eq "aa\0bb";
    }
    ok $ok;
}

# Test for bug [perl #36434]
# Can not do this test on VMS, and SYMBIAN according to comments
# in mg.c/Perl_magic_clear_all_env()
SKIP: {
    skip('Can\'t make assignment to \%ENV on this system', 3) if $Is_VMS;

    local @ISA;
    local %ENV;
    # This used to be __PACKAGE__, but that causes recursive
    #  inheritance, which is detected earlier now and broke
    #  this test
    eval { push @ISA, __FILE__ };
    is $@, '', 'Push a constant on a magic array';
    $@ and print "# $@";
    eval { %ENV = (PATH => __PACKAGE__) };
    is $@, '', 'Assign a constant to a magic hash';
    $@ and print "# $@";
    eval { my %h = qw(A B); %ENV = (PATH => (keys %h)[0]) };
    is $@, '', 'Assign a shared key to a magic hash';
    $@ and print "# $@";
}

# Tests for Perl_magic_clearsig
foreach my $sig (qw(__WARN__ INT)) {
    $SIG{$sig} = lc $sig;
    is $SIG{$sig}, 'main::' . lc $sig, "Can assign to $sig";
    is delete $SIG{$sig}, 'main::' . lc $sig, "Can delete from $sig";
    is $SIG{$sig}, undef, "$sig is now gone";
    is delete $SIG{$sig}, undef, "$sig remains gone";
}

# And now one which doesn't exist;
{
    no warnings 'signal';
    $SIG{HUNGRY} = 'mmm, pie';
}
is $SIG{HUNGRY}, 'mmm, pie', 'Can assign to HUNGRY';
is delete $SIG{HUNGRY}, 'mmm, pie', 'Can delete from HUNGRY';
is $SIG{HUNGRY}, undef, "HUNGRY is now gone";
is delete $SIG{HUNGRY}, undef, "HUNGRY remains gone";

# Test deleting signals that we never set
foreach my $sig (qw(__DIE__ _BOGUS_HOOK KILL THIRSTY)) {
    is $SIG{$sig}, undef, "$sig is not present";
    is delete $SIG{$sig}, undef, "delete of $sig returns undef";
}

{
    $! = 9999;
    is int $!, 9999, q{[perl #72850] Core dump in bleadperl from perl -e '$! = 9999; $a = $!;'};

}

# %+ %-
SKIP: {
    skip_if_miniperl("No XS in miniperl", 2);
    # Make sure defined(*{"+"}) before %+ does not stop %+ from working
    is
      runperl(
	prog => 'BEGIN { defined *{q-+-} } print qq-ok\n- if tied %+',
      ),
     "ok\n",
     'defined *{"+"} does not stop %+ from working';
    is
      runperl(
	prog => 'BEGIN { defined *{q=-=} } print qq-ok\n- if tied %-',
      ),
     "ok\n",
     'defined *{"-"} does not stop %- from working';
}

SKIP: {
    skip_if_miniperl("No XS in miniperl", 3);

    for ( [qw( %- Tie::Hash::NamedCapture )], [qw( $[ arybase )],
          [qw( %! Errno )] ) {
	my ($var, $mod) = @$_;
	my $modfile = $mod =~ s|::|/|gr . ".pm";
	fresh_perl_is
	   qq 'sub UNIVERSAL::AUTOLOAD{}
	       $mod\::foo() if 0;
	       $var;
	       print "ok\\n" if \$INC{"$modfile"}',
	  "ok\n",
	   { switches => [ '-X' ] },
	  "$var still loads $mod when stash and UNIVERSAL::AUTOLOAD exist";
    }
}

# ${^LAST_FH}
() = tell STDOUT;
is ${^LAST_FH}, \*STDOUT, '${^LAST_FH} after tell';
() = tell STDIN;
is ${^LAST_FH}, \*STDIN, '${^LAST_FH} after another tell';
{
    my $fh = *STDOUT;
    () = tell $fh;
    is ${^LAST_FH}, \$fh, '${^LAST_FH} referencing lexical coercible glob';
}
# This also tests that ${^LAST_FH} is a weak reference:
is ${^LAST_FH}, undef, '${^LAST_FH} is undef when PL_last_in_gv is NULL';


# $|
fresh_perl_is 'print $| = ~$|', "1\n", {switches => ['-l']},
 '[perl #4760] print $| = ~$|';
fresh_perl_is
 'select f; undef *f; ${q/|/}; print STDOUT qq|ok\n|', "ok\n", {},
 '[perl #115206] no crash when vivifying $| while *{+select}{IO} is undef';

# ${^OPEN} and $^H interaction
# Setting ${^OPEN} causes $^H to change, but setting $^H would only some-
# times make ${^OPEN} change, depending on whether it was in the same BEGIN
# block.  Don’t test actual values (subject to change); just test for
# consistency.
my @stuff;
eval '
    BEGIN { ${^OPEN} = "a\0b"; $^H = 0;          push @stuff, ${^OPEN} }
    BEGIN { ${^OPEN} = "a\0b"; $^H = 0 } BEGIN { push @stuff, ${^OPEN} }
1' or die $@;
is $stuff[0], $stuff[1], '$^H modifies ${^OPEN} consistently';

# deleting $::{"\cH"}
is runperl(prog => 'delete $::{qq-\cH-}; ${^OPEN}=foo; print qq-ok\n-'),
  "ok\n",
  'deleting $::{"\cH"}';

# Tests for some non-magic names:
is ${^MPE}, undef, '${^MPE} starts undefined';
is ++${^MPE}, 1, '${^MPE} can be incremented';

# This one used to behave as ${^MATCH} due to a missing break:
is ${^MPEN}, undef, '${^MPEN} starts undefined';
# This one used to croak due to that missing break:
is ++${^MPEN}, 1, '${^MPEN} can be incremented';

# ^^^^^^^^^ New tests go here ^^^^^^^^^

SKIP: {
    skip("%ENV manipulations fail or aren't safe on $^O", 19)
	if $Is_Dos;

 SKIP: {
	skip("clearing \%ENV is not safe when running under valgrind or on VMS")
	    if $ENV{PERL_VALGRIND} || $Is_VMS;

	    $PATH = $ENV{PATH};
	    $PDL = $ENV{PERL_DESTRUCT_LEVEL} || 0;
	    $ENV{foo} = "bar";
	    %ENV = ();
	    $ENV{PATH} = $PATH;
	    $ENV{PERL_DESTRUCT_LEVEL} = $PDL || 0;
	    if ($Is_MSWin32) {
		is `set foo 2>NUL`, "";
	    } else {
		is `echo \$foo`, "\n";
	    }
	}

	$ENV{__NoNeSuCh} = 'foo';
	$0 = 'bar';
	env_is(__NoNeSuCh => 'foo', 'setting $0 does not break %ENV');

	$ENV{__NoNeSuCh2} = 'foo';
	$ENV{__NoNeSuCh2} = undef;
	env_is(__NoNeSuCh2 => '', 'setting a key as undef does not delete it');

	# stringify a glob
	$ENV{foo} = *TODO;
	env_is(foo => '*main::TODO', 'ENV store of stringified glob');

	# stringify a ref
	my $ref = [];
	$ENV{foo} = $ref;
	env_is(foo => "$ref", 'ENV store of stringified ref');

	# downgrade utf8 when possible
	$bytes = "eh zero \x{A0}";
	utf8::upgrade($chars = $bytes);
	$forced = $ENV{foo} = $chars;
	ok(!utf8::is_utf8($forced) && $forced eq $bytes, 'ENV store downgrades utf8 in SV');
	env_is(foo => $bytes, 'ENV store downgrades utf8 in setenv');

	# warn when downgrading utf8 is not possible
	$chars = "X-Day \x{1998}";
	utf8::encode($bytes = $chars);
	{
	  my $warned = 0;
	  local $SIG{__WARN__} = sub { ++$warned if $_[0] =~ /^Wide character in setenv/; print "# @_" };
	  $forced = $ENV{foo} = $chars;
	  ok($warned == 1, 'ENV store warns about wide characters');
	}
	ok(!utf8::is_utf8($forced) && $forced eq $bytes, 'ENV store encodes high utf8 in SV');
	env_is(foo => $bytes, 'ENV store encodes high utf8 in SV');

	# test local $ENV{foo} on existing foo
	{
	  local $ENV{__NoNeSuCh};
	  { local $TODO = 'exists on %ENV should reflect real env';
	    ok(!exists $ENV{__NoNeSuCh}, 'not exists $ENV{existing} during local $ENV{existing}'); }
	  env_is(__NoNeLoCaL => '');
	}
	ok(exists $ENV{__NoNeSuCh}, 'exists $ENV{existing} after local $ENV{existing}');
	env_is(__NoNeSuCh => 'foo');

	# test local $ENV{foo} on new foo
	{
	  local $ENV{__NoNeLoCaL} = 'foo';
	  ok(exists $ENV{__NoNeLoCaL}, 'exists $ENV{new} during local $ENV{new}');
	  env_is(__NoNeLoCaL => 'foo');
	}
	ok(!exists $ENV{__NoNeLoCaL}, 'not exists $ENV{new} after local $ENV{new}');
	env_is(__NoNeLoCaL => '');

    SKIP: {
	    skip("\$0 check only on Linux and FreeBSD", 2)
		unless $^O =~ /^(linux|android|freebsd)$/
		    && open CMDLINE, "/proc/$$/cmdline";

	    chomp(my $line = scalar <CMDLINE>);
	    my $me = (split /\0/, $line)[0];
	    is $me, $0, 'altering $0 is effective (testing with /proc/)';
	    close CMDLINE;
            skip("\$0 check with 'ps' only on Linux (but not Android) and FreeBSD", 1) if $^O eq 'android';
            # perlbug #22811
            my $mydollarzero = sub {
              my($arg) = shift;
              $0 = $arg if defined $arg;
	      # In FreeBSD the ps -o command= will cause
	      # an empty header line, grab only the last line.
              my $ps = (`ps -o command= -p $$`)[-1];
              return if $?;
              chomp $ps;
              printf "# 0[%s]ps[%s]\n", $0, $ps;
              $ps;
            };
            my $ps = $mydollarzero->("x");
            ok(!$ps  # we allow that something goes wrong with the ps command
	       # In Linux 2.4 we would get an exact match ($ps eq 'x') but
	       # in Linux 2.2 there seems to be something funny going on:
	       # it seems as if the original length of the argv[] would
	       # be stored in the proc struct and then used by ps(1),
	       # no matter what characters we use to pad the argv[].
	       # (And if we use \0:s, they are shown as spaces.)  Sigh.
               || $ps =~ /^x\s*$/
	       # FreeBSD cannot get rid of both the leading "perl :"
	       # and the trailing " (perl)": some FreeBSD versions
	       # can get rid of the first one.
	       || ($^O eq 'freebsd' && $ps =~ m/^(?:perl: )?x(?: \(perl\))?$/),
		       'altering $0 is effective (testing with `ps`)');
	}
}

# test case-insignificance of %ENV (these tests must be enabled only
# when perl is compiled with -DENV_IS_CASELESS)
SKIP: {
    skip('no caseless %ENV support', 4) unless $Is_MSWin32 || $Is_NetWare;

    %ENV = ();
    $ENV{'Foo'} = 'bar';
    $ENV{'fOo'} = 'baz';
    is scalar(keys(%ENV)), 1;
    ok exists $ENV{'FOo'};
    is delete $ENV{'foO'}, 'baz';
    is scalar(keys(%ENV)), 0;
}

__END__

# Put new tests before the various ENV tests, as they blow %ENV away.
