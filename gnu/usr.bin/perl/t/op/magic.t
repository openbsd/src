#!./perl

BEGIN {
    $| = 1;
    chdir 't' if -d 't';
    @INC = '../lib';
    $SIG{__WARN__} = sub { die "Dying on warning: ", @_ };
}

use warnings;
use Config;

my $test = 1;
sub ok {
    my($ok, $info, $todo) = @_;

    # You have to do it this way or VMS will get confused.
    printf "%s $test%s\n", $ok ? "ok" : "not ok",
                           $todo ? " # TODO $todo" : '';

    unless( $ok ) {
        printf "# Failed test at line %d\n", (caller)[2];
        print  "# $info\n" if defined $info;
    }

    $test++;
    return $ok;
}

sub skip {
    my($reason) = @_;

    printf "ok $test # skipped%s\n", defined $reason ? ": $reason" : '';

    $test++;
    return 1;
}

print "1..54\n";

$Is_MSWin32  = $^O eq 'MSWin32';
$Is_NetWare  = $^O eq 'NetWare';
$Is_VMS      = $^O eq 'VMS';
$Is_Dos      = $^O eq 'dos';
$Is_os2      = $^O eq 'os2';
$Is_Cygwin   = $^O eq 'cygwin';
$Is_MacOS    = $^O eq 'MacOS';
$Is_MPE      = $^O eq 'mpeix';		
$Is_miniperl = $ENV{PERL_CORE_MINITEST};

$PERL = ($Is_NetWare            ? 'perl'   :
	 ($Is_MacOS || $Is_VMS) ? $^X      :
	 $Is_MSWin32            ? '.\perl' :
	 './perl');

eval '$ENV{"FOO"} = "hi there";';	# check that ENV is inited inside eval
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
if ($Is_MSWin32)  { ok `set FOO` =~ /^(?:FOO=)?hi there$/; }
elsif ($Is_MacOS) { ok "1 # skipped", 1; }
elsif ($Is_VMS)   { ok `write sys\$output f\$trnlnm("FOO")` eq "hi there\n"; }
else              { ok `echo \$FOO` eq "hi there\n"; }

unlink 'ajslkdfpqjsjfk';
$! = 0;
open(FOO,'ajslkdfpqjsjfk');
ok $!, $!;
close FOO; # just mention it, squelch used-only-once

if ($Is_MSWin32 || $Is_NetWare || $Is_Dos || $Is_MPE || $Is_MacOS) {
    skip('SIGINT not safe on this platform') for 1..4;
}
else {
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

    $test += 4;
}

# can we slice ENV?
@val1 = @ENV{keys(%ENV)};
@val2 = values(%ENV);
ok join(':',@val1) eq join(':',@val2);
ok @val1 > 1;

# regex vars
'foobarbaz' =~ /b(a)r/;
ok $` eq 'foo', $`;
ok $& eq 'bar', $&;
ok $' eq 'baz', $';
ok $+ eq 'a', $+;

# $"
@a = qw(foo bar baz);
ok "@a" eq "foo bar baz", "@a";
{
    local $" = ',';
    ok "@a" eq "foo,bar,baz", "@a";
}

# $;
%h = ();
$h{'foo', 'bar'} = 1;
ok((keys %h)[0] eq "foo\034bar", (keys %h)[0]);
{
    local $; = 'x';
    %h = ();
    $h{'foo', 'bar'} = 1;
    ok((keys %h)[0] eq 'fooxbar', (keys %h)[0]);
}

# $?, $@, $$
if ($Is_MacOS) {
    skip('$? + system are broken on MacPerl') for 1..2;
}
else {
    system qq[$PERL "-I../lib" -e "use vmsish qw(hushed); exit(0)"];
    ok $? == 0, $?;
    system qq[$PERL "-I../lib" -e "use vmsish qw(hushed); exit(1)"];
    ok $? != 0, $?;
}

eval { die "foo\n" };
ok $@ eq "foo\n", $@;

ok $$ > 0, $$;
eval { $$++ };
ok $@ =~ /^Modification of a read-only value attempted/;

# $^X and $0
{
    if ($^O eq 'qnx') {
	chomp($wd = `/usr/bin/fullpath -t`);
    }
    elsif($Is_Cygwin || $Config{'d_procselfexe'}) {
       # Cygwin turns the symlink into the real file
       chomp($wd = `pwd`);
       $wd =~ s#/t$##;
    }
    elsif($Is_os2) {
       $wd = Cwd::sys_cwd();
    }
    elsif($Is_MacOS) {
       $wd = ':';
    }
    else {
	$wd = '.';
    }
    my $perl = ($Is_MacOS || $Is_VMS) ? $^X : "$wd/perl";
    my $headmaybe = '';
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
    elsif ($Is_MacOS) {
      $script = ":show-shebang";
    }
    elsif ($Is_VMS) {
      $script = "[]show-shebang";
    }
    if ($^O eq 'os390' or $^O eq 'posix-bc' or $^O eq 'vmesa') {  # no shebang
	$headmaybe = <<EOH ;
    eval 'exec ./perl -S \$0 \${1+"\$\@"}'
        if 0;
EOH
    }
    $s1 = "\$^X is $perl, \$0 is $script\n";
    ok open(SCRIPT, ">$script"), $!;
    ok print(SCRIPT $headmaybe . <<EOB . <<'EOF' . $tailmaybe), $!;
#!$wd/perl
EOB
print "\$^X is $^X, \$0 is $0\n";
EOF
    ok close(SCRIPT), $!;
    ok chmod(0755, $script), $!;
    $_ = ($Is_MacOS || $Is_VMS) ? `$perl $script` : `$script`;
    s/\.exe//i if $Is_Dos or $Is_Cygwin or $Is_os2;
    s{\bminiperl\b}{perl}; # so that test doesn't fail with miniperl
    s{is perl}{is $perl}; # for systems where $^X is only a basename
    s{\\}{/}g;
    ok((($Is_MSWin32 || $Is_os2) ? uc($_) eq uc($s1) : $_ eq $s1), " :$_:!=:$s1:");
    $_ = `$perl $script`;
    s/\.exe//i if $Is_Dos or $Is_os2;
    s{\\}{/}g;
    ok((($Is_MSWin32 || $Is_os2) ? uc($_) eq uc($s1) : $_ eq $s1), " :$_:!=:$s1: after `$perl $script`");
    ok unlink($script), $!;
}

# $], $^O, $^T
ok $] >= 5.00319, $];
ok $^O;
ok $^T > 850000000, $^T;

if ($Is_VMS || $Is_Dos || $Is_MacOS) {
    skip("%ENV manipulations fail or aren't safe on $^O") for 1..4;
}
else {
	if ($ENV{PERL_VALGRIND}) {
	    skip("clearing \%ENV is not safe when running under valgrind");
	} else {
	    $PATH = $ENV{PATH};
	    $PDL = $ENV{PERL_DESTRUCT_LEVEL} || 0;
	    $ENV{foo} = "bar";
	    %ENV = ();
	    $ENV{PATH} = $PATH;
	    $ENV{PERL_DESTRUCT_LEVEL} = $PDL || 0;
	    ok ($Is_MSWin32 ? (`set foo 2>NUL` eq "")
			    : (`echo \$foo` eq "\n") );
	}

	$ENV{__NoNeSuCh} = "foo";
	$0 = "bar";
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
       ok ($Is_MSWin32 ? (`set __NoNeSuCh` =~ /^(?:__NoNeSuCh=)?foo$/)
			    : (`echo \$__NoNeSuCh` eq "foo\n") );
	if ($^O =~ /^(linux|freebsd)$/ &&
	    open CMDLINE, "/proc/$$/cmdline") {
	    chomp(my $line = scalar <CMDLINE>);
	    my $me = (split /\0/, $line)[0];
	    ok($me eq $0, 'altering $0 is effective (testing with /proc/)');
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
	} else {
	    skip("\$0 check only on Linux and FreeBSD") for 0, 1;
	}
}

{
    my $ok = 1;
    my $warn = '';
    local $SIG{'__WARN__'} = sub { $ok = 0; $warn = join '', @_; };
    $! = undef;
    ok($ok, $warn, $Is_VMS ? "'\$!=undef' does throw a warning" : '');
}

# test case-insignificance of %ENV (these tests must be enabled only
# when perl is compiled with -DENV_IS_CASELESS)
if ($Is_MSWin32 || $Is_NetWare) {
    %ENV = ();
    $ENV{'Foo'} = 'bar';
    $ENV{'fOo'} = 'baz';
    ok (scalar(keys(%ENV)) == 1);
    ok exists($ENV{'FOo'});
    ok (delete($ENV{'foO'}) eq 'baz');
    ok (scalar(keys(%ENV)) == 0);
}
else {
    skip('no caseless %ENV support') for 1..4;
}

if ($Is_miniperl) {
    skip ("miniperl can't rely on loading %Errno") for 1..2;
} else {
   no warnings 'void';

# Make sure Errno hasn't been prematurely autoloaded

   ok !defined %Errno::;

# Test auto-loading of Errno when %! is used

   ok scalar eval q{
      %!;
      defined %Errno::;
   }, $@;
}

if ($Is_miniperl) {
    skip ("miniperl can't rely on loading %Errno");
} else {
    # Make sure that Errno loading doesn't clobber $!

    undef %Errno::;
    delete $INC{"Errno.pm"};

    open(FOO, "nonesuch"); # Generate ENOENT
    my %errs = %{"!"}; # Cause Errno.pm to be loaded at run-time
    ok ${"!"}{ENOENT};
}

ok $^S == 0 && defined $^S;
eval { ok $^S == 1 };
eval " BEGIN { ok ! defined \$^S } ";
ok $^S == 0 && defined $^S;

ok ${^TAINT} == 0;
eval { ${^TAINT} = 1 };
ok ${^TAINT} == 0;

# 5.6.1 had a bug: @+ and @- were not properly interpolated
# into double-quoted strings
# 20020414 mjd-perl-patch+@plover.com
"I like pie" =~ /(I) (like) (pie)/;
ok "@-" eq  "0 0 2 7";
ok "@+" eq "10 1 6 10";

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
    ok( $x eq "@y", "return a magic array ($x) vs (@y)" );
}
