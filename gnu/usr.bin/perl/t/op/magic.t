#!./perl

BEGIN {
    $^W = 1;
    $| = 1;
    chdir 't' if -d 't';
    @INC = '../lib';
    $SIG{__WARN__} = sub { die "Dying on warning: ", @_ };
}

sub ok {
    my ($n, $result, $info) = @_;
    if ($result) {
	print "ok $n\n";
    }
    else {
    	print "not ok $n\n";
	print "# $info\n" if $info;
    }
}

$Is_MSWin32 = $^O eq 'MSWin32';
$Is_VMS     = $^O eq 'VMS';
$PERL = ($Is_MSWin32 ? '.\perl' : './perl');

print "1..30\n";

eval '$ENV{"foo"} = "hi there";';	# check that ENV is inited inside eval
if ($Is_MSWin32) { ok 1, `cmd /x /c set foo` eq "foo=hi there\n"; }
else             { ok 1, `echo \$foo` eq "hi there\n"; }

unlink 'ajslkdfpqjsjfk';
$! = 0;
open(FOO,'ajslkdfpqjsjfk');
ok 2, $!, $!;
close FOO; # just mention it, squelch used-only-once

if ($Is_MSWin32) {
    ok 3,1;
    ok 4,1;
}
else {
  # the next tests are embedded inside system simply because sh spits out
  # a newline onto stderr when a child process kills itself with SIGINT.
  system './perl', '-e', <<'END';

    $| = 1;		# command buffering

    $SIG{"INT"} = "ok3";     kill "INT",$$;
    $SIG{"INT"} = "IGNORE";  kill "INT",$$; print "ok 4\n";
    $SIG{"INT"} = "DEFAULT"; kill "INT",$$; print "not ok\n";

    sub ok3 {
	if (($x = pop(@_)) eq "INT") {
	    print "ok 3\n";
	}
	else {
	    print "not ok 3 ($x @_)\n";
	}
    }

END
}

# can we slice ENV?
@val1 = @ENV{keys(%ENV)};
@val2 = values(%ENV);
ok 5, join(':',@val1) eq join(':',@val2);
ok 6, @val1 > 1;

# regex vars
'foobarbaz' =~ /b(a)r/;
ok 7, $` eq 'foo', $`;
ok 8, $& eq 'bar', $&;
ok 9, $' eq 'baz', $';
ok 10, $+ eq 'a', $+;

# $"
@a = qw(foo bar baz);
ok 11, "@a" eq "foo bar baz", "@a";
{
    local $" = ',';
    ok 12, "@a" eq "foo,bar,baz", "@a";
}

# $;
%h = ();
$h{'foo', 'bar'} = 1;
ok 13, (keys %h)[0] eq "foo\034bar", (keys %h)[0];
{
    local $; = 'x';
    %h = ();
    $h{'foo', 'bar'} = 1;
    ok 14, (keys %h)[0] eq 'fooxbar', (keys %h)[0];
}

# $?, $@, $$
system qq[$PERL -e "exit(0)"];
ok 15, $? == 0, $?;
system qq[$PERL -e "exit(1)"];
ok 16, $? != 0, $?;

eval { die "foo\n" };
ok 17, $@ eq "foo\n", $@;

ok 18, $$ > 0, $$;

# $^X and $0
{
    if ($^O eq 'qnx') {
	chomp($wd = `pwd`);
    }
    else {
	$wd = '.';
    }
    my $perl = "$wd/perl";
    my $headmaybe = '';
    my $tailmaybe = '';
    $script = "$wd/show-shebang";
    if ($Is_MSWin32) {
	chomp($wd = `cd`);
	$perl = "$wd\\perl.exe";
	$script = "$wd\\show-shebang.bat";
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
    $s1 = $s2 = "\$^X is $perl, \$0 is $script\n";
    if ($^O eq 'os2') {
	# Started by ksh, which adds suffixes '.exe' and '.' to perl and script
	$s2 = "\$^X is $wd/perl.exe, \$0 is $script.\n";
    }
    ok 19, open(SCRIPT, ">$script"), $!;
    ok 20, print(SCRIPT $headmaybe . <<EOB . <<'EOF' . $tailmaybe), $!;
#!$wd/perl
EOB
print "\$^X is $^X, \$0 is $0\n";
EOF
    ok 21, close(SCRIPT), $!;
    ok 22, chmod(0755, $script), $!;
    $_ = `$script`;
    s{\bminiperl\b}{perl}; # so that test doesn't fail with miniperl
    s{is perl}{is $perl}; # for systems where $^X is only a basename
    ok 23, ($Is_MSWin32 ? uc($_) eq uc($s2) : $_ eq $s2), ":$_:!=:$s2:";
    $_ = `$perl $script`;
    ok 24, ($Is_MSWin32 ? uc($_) eq uc($s1) : $_ eq $s1), ":$_:!=:$s1: after `$perl $script`";
    ok 25, unlink($script), $!;
}

# $], $^O, $^T
ok 26, $] >= 5.00319, $];
ok 27, $^O;
ok 28, $^T > 850000000, $^T;

if ($Is_VMS) {
	ok 29, 1;
	ok 30, 1;
}
else {
	$PATH = $ENV{PATH};
	$ENV{foo} = "bar";
	%ENV = ();
	$ENV{PATH} = $PATH;
	ok 29, ($Is_MSWin32 ? (`cmd /x /c set foo 2>NUL` eq "")
				: (`echo \$foo` eq "\n") );

	$ENV{NoNeSuCh} = "foo";
	$0 = "bar";
	ok 30, ($Is_MSWin32 ? (`cmd /x /c set NoNeSuCh` eq "NoNeSuCh=foo\n")
						: (`echo \$NoNeSuCh` eq "foo\n") );
}

