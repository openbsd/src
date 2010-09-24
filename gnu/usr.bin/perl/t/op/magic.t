#!./perl

BEGIN {
    $| = 1;
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PATH} = '/bin' if ${^TAINT};
    $SIG{__WARN__} = sub { die "Dying on warning: ", @_ };
    require './test.pl';
}

use warnings;
use Config;

plan (tests => 80);

$Is_MSWin32  = $^O eq 'MSWin32';
$Is_NetWare  = $^O eq 'NetWare';
$Is_VMS      = $^O eq 'VMS';
$Is_Dos      = $^O eq 'dos';
$Is_os2      = $^O eq 'os2';
$Is_Cygwin   = $^O eq 'cygwin';
$Is_MPE      = $^O eq 'mpeix';		
$Is_miniperl = $ENV{PERL_CORE_MINITEST};
$Is_BeOS     = $^O eq 'beos';

$PERL = $ENV{PERL}
    || ($Is_NetWare           ? 'perl'   :
       $Is_VMS                ? $^X      :
       $Is_MSWin32            ? '.\perl' :
       './perl');

END {
    # On VMS, environment variable changes are peristent after perl exits
    delete $ENV{'FOO'} if $Is_VMS;
}

eval '$ENV{"FOO"} = "hi there";';	# check that ENV is inited inside eval
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
if ($Is_MSWin32)  { like `set FOO`, qr/^(?:FOO=)?hi there$/; }
elsif ($Is_VMS)   { is `write sys\$output f\$trnlnm("FOO")`, "hi there\n"; }
else              { is `echo \$FOO`, "hi there\n"; }

unlink 'ajslkdfpqjsjfk';
$! = 0;
open(FOO,'ajslkdfpqjsjfk');
isnt($!, 0);
close FOO; # just mention it, squelch used-only-once

SKIP: {
    skip('SIGINT not safe on this platform', 5)
	if $Is_MSWin32 || $Is_NetWare || $Is_Dos || $Is_MPE;
  # the next tests are done in a subprocess because sh spits out a
  # newline onto stderr when a child process kills itself with SIGINT.
  # We use a pipe rather than system() because the VMS command buffer
  # would overflow with a command that long.

    open( CMDPIPE, "| $PERL");

    print CMDPIPE <<'END';

    $| = 1;		# command buffering

    $SIG{"INT"} = "ok3";     kill "INT",$$; sleep 1;
    $SIG{"INT"} = "IGNORE";  kill "INT",$$; sleep 1; print "ok 4\n";
    $SIG{"INT"} = "DEFAULT"; kill "INT",$$; sleep 1; print "not ok 4\n";

    sub ok3 {
	if (($x = pop(@_)) eq "INT") {
	    print "ok 3\n";
	}
	else {
	    print "not ok 3 ($x @_)\n";
	}
    }

END

    close CMDPIPE;

    open( CMDPIPE, "| $PERL");
    print CMDPIPE <<'END';

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
    $SIG{"INT"} = "ok5";
    {
	local $SIG{"INT"}=x();
	print ""; # Needed to expose failure in 5.8.0 (why?)
    }
    sleep 1;
    delete $SIG{"INT"};
    kill "INT",$$; sleep 1;
    sub ok5 {
	print "ok 5\n";
    }
END
    close CMDPIPE;
    $? >>= 8 if $^O eq 'VMS'; # POSIX status hiding in 2nd byte
    my $todo = ($^O eq 'os2' ? ' # TODO: EMX v0.9d_fix4 bug: wrong nibble? ' : '');
    print $? & 0xFF ? "ok 6$todo\n" : "not ok 6$todo\n";

    open(CMDPIPE, "| $PERL");
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
    print $? ? "not ok 7\n" : "ok 7\n";

    curr_test(curr_test() + 5);
}

# can we slice ENV?
@val1 = @ENV{keys(%ENV)};
@val2 = values(%ENV);
is join(':',@val1), join(':',@val2);
cmp_ok @val1, '>', 1;

# regex vars
'foobarbaz' =~ /b(a)r/;
is $`, 'foo';
is $&, 'bar';
is $', 'baz';
is $+, 'a';

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

cmp_ok($$, '>', 0);
eval { $$++ };
like ($@, qr/^Modification of a read-only value attempted/);

# $^X and $0
{
    if ($^O eq 'qnx') {
	chomp($wd = `/usr/bin/fullpath -t`);
    }
    elsif($Is_Cygwin || $Config{'d_procselfexe'}) {
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
    my $perl = $Is_VMS ? $^X : "$wd/perl";
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
    if ($^O eq 'os390' or $^O eq 'posix-bc' or $^O eq 'vmesa') {  # no shebang
	$headmaybe = <<EOH ;
    eval 'exec ./perl -S \$0 \${1+"\$\@"}'
        if 0;
EOH
    }
    $s1 = "\$^X is $perl, \$0 is $script\n";
    ok open(SCRIPT, ">$script") or diag "Can't write to $script: $!";
    ok print(SCRIPT $headmaybe . <<EOB . $middlemaybe . <<'EOF' . $tailmaybe) or diag $!;
#!$wd/perl
EOB
print "\$^X is $^X, \$0 is $0\n";
EOF
    ok close(SCRIPT) or diag $!;
    ok chmod(0755, $script) or diag $!;
    $_ = $Is_VMS ? `$perl $script` : `$script`;
    s/\.exe//i if $Is_Dos or $Is_Cygwin or $Is_os2;
    s{./$script}{$script} if $Is_BeOS; # revert BeOS execvp() side-effect
    s{\bminiperl\b}{perl}; # so that test doesn't fail with miniperl
    s{is perl}{is $perl}; # for systems where $^X is only a basename
    s{\\}{/}g;
    if ($Is_MSWin32 || $Is_os2) {
	is uc $_, uc $s1;
    } else {
	is $_, $s1;
    }
    $_ = `$perl $script`;
    s/\.exe//i if $Is_Dos or $Is_os2 or $Is_Cygwin;
    s{./$perl}{$perl} if $Is_BeOS; # revert BeOS execvp() side-effect
    s{\\}{/}g;
    if ($Is_MSWin32 || $Is_os2) {
	is uc $_, uc $s1;
    } else {
	is $_, $s1;
    }
    ok unlink($script) or diag $!;
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

SKIP: {
    skip("%ENV manipulations fail or aren't safe on $^O", 4)
	if $Is_VMS || $Is_Dos;

 SKIP: {
	skip("clearing \%ENV is not safe when running under valgrind")
	    if $ENV{PERL_VALGRIND};

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

	$ENV{__NoNeSuCh} = "foo";
	$0 = "bar";
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
    	if ($Is_MSWin32) {
	    like `set __NoNeSuCh`, qr/^(?:__NoNeSuCh=)?foo$/;
	} else {
	    is `echo \$__NoNeSuCh`, "foo\n";
	}
    SKIP: {
	    skip("\$0 check only on Linux and FreeBSD", 2)
		unless $^O =~ /^(linux|freebsd)$/
		    && open CMDLINE, "/proc/$$/cmdline";

	    chomp(my $line = scalar <CMDLINE>);
	    my $me = (split /\0/, $line)[0];
	    is $me, $0, 'altering $0 is effective (testing with /proc/)';
	    close CMDLINE;
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

{
    my $ok = 1;
    my $warn = '';
    local $SIG{'__WARN__'} = sub { $ok = 0; $warn = join '', @_; $warn =~ s/\n$//; };
    $! = undef;
    local $TODO = $Is_VMS ? "'\$!=undef' does throw a warning" : '';
    ok($ok, $warn);
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

SKIP: {
    skip ("miniperl can't rely on loading %Errno", 2) if $Is_miniperl;
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
    skip ("miniperl can't rely on loading %Errno") if $Is_miniperl;
    # Make sure that Errno loading doesn't clobber $!

    undef %Errno::;
    delete $INC{"Errno.pm"};

    open(FOO, "nonesuch"); # Generate ENOENT
    my %errs = %{"!"}; # Cause Errno.pm to be loaded at run-time
    ok ${"!"}{ENOENT};
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

# Test for bug [perl #27839]
{
    my $x;
    sub f {
	"abc" =~ /(.)./;
	$x = "@+";
	return @+;
    };
    my @y = f();
    is $x, "@y", "return a magic array ($x) vs (@y)";
}

# Test for bug [perl #36434]
# Can not do this test on VMS, EPOC, and SYMBIAN according to comments
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
