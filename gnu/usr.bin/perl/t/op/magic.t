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
        print  "# $info" if defined $info;
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

print "1..46\n";

$Is_MSWin32 = $^O eq 'MSWin32';
$Is_NetWare = $^O eq 'NetWare';
$Is_VMS     = $^O eq 'VMS';
$Is_Dos     = $^O eq 'dos';
$Is_os2     = $^O eq 'os2';
$Is_Cygwin  = $^O eq 'cygwin';
$Is_MacOS   = $^O eq 'MacOS';
$Is_MPE     = $^O eq 'mpeix';		

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
    skip('SIGINT not safe on this platform') for 1..2;
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

    $test += 2;
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
    skip("%ENV manipulations fail or aren't safe on $^O") for 1..2;
}
else {
	$PATH = $ENV{PATH};
	$PDL = $ENV{PERL_DESTRUCT_LEVEL} || 0;
	$ENV{foo} = "bar";
	%ENV = ();
	$ENV{PATH} = $PATH;
	$ENV{PERL_DESTRUCT_LEVEL} = $PDL || 0;
	ok ($Is_MSWin32 ? (`set foo 2>NUL` eq "")
				: (`echo \$foo` eq "\n") );

	$ENV{__NoNeSuCh} = "foo";
	$0 = "bar";
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
       ok ($Is_MSWin32 ? (`set __NoNeSuCh` =~ /^(?:__NoNeSuCh=)?foo$/)
			    : (`echo \$__NoNeSuCh` eq "foo\n") );
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

# Make sure Errno hasn't been prematurely autoloaded

ok !defined %Errno::;

# Test auto-loading of Errno when %! is used

ok scalar eval q{
   my $errs = %!;
   defined %Errno::;
}, $@;


# Make sure that Errno loading doesn't clobber $!

undef %Errno::;
delete $INC{"Errno.pm"};

open(FOO, "nonesuch"); # Generate ENOENT
my %errs = %{"!"}; # Cause Errno.pm to be loaded at run-time
ok ${"!"}{ENOENT};

ok $^S == 0;
eval { ok $^S == 1 };
ok $^S == 0;

ok ${^TAINT} == 0;
eval { ${^TAINT} = 1 };
ok ${^TAINT} == 0;

# 5.6.1 had a bug: @+ and @- were not properly interpolated
# into double-quoted strings
# 20020414 mjd-perl-patch+@plover.com
"I like pie" =~ /(I) (like) (pie)/;
ok "@-" eq  "0 0 2 7";
ok "@+" eq "10 1 6 10";

