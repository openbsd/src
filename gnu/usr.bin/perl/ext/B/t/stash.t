#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') {
	@INC = qw(: ::lib ::macos:lib);
    } else {
	@INC = '../lib';
    }
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

$|  = 1;
use warnings;
use strict;
use Config;

print "1..1\n";

my $test = 1;

sub ok { print "ok $test\n"; $test++ }


my $got;
my $Is_VMS = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';

my $path = join " ", map { qq["-I$_"] } @INC;
$path = '"-I../lib" "-Iperl_root:[lib]"' if $Is_VMS;   # gets too long otherwise
my $redir = $Is_MacOS ? "" : "2>&1";

chomp($got = `$^X $path "-MB::Stash" "-Mwarnings" -e1`);

$got =~ s/-u//g;

print "# got = $got\n";

my @got = map { s/^\S+ //; $_ }
              sort { $a cmp $b }
                   map { lc($_) . " " . $_ }
                       split /,/, $got;

print "# (after sorting)\n";
print "# got = @got\n";

@got = grep { ! /^(PerlIO|open)(?:::\w+)?$/ } @got;

print "# (after perlio censorings)\n";
print "# got = @got\n";

@got = grep { ! /^Win32$/                     } @got  if $^O eq 'MSWin32';
@got = grep { ! /^NetWare$/                   } @got  if $^O eq 'NetWare';
@got = grep { ! /^(Cwd|File|File::Copy|OS2)$/ } @got  if $^O eq 'os2';
@got = grep { ! /^Cwd$/                       } @got  if $^O eq 'cygwin';

if ($Is_VMS) {
    @got = grep { ! /^File(?:::Copy)?$/    } @got;
    @got = grep { ! /^VMS(?:::Filespec)?$/ } @got;
    @got = grep { ! /^vmsish$/             } @got;
     # Socket is optional/compiler version dependent
    @got = grep { ! /^Socket$/             } @got;
}

print "# (after platform censorings)\n";
print "# got = @got\n";

$got = "@got";

my $expected = "attributes Carp Carp::Heavy DB Exporter Exporter::Heavy Internals main Regexp utf8 warnings";

{
    no strict 'vars';
    use vars '$OS2::is_aout';
}

if ((($Config{static_ext} eq ' ') || ($Config{static_ext} eq ''))
    && !($^O eq 'os2' and $OS2::is_aout)
	) {
    print "# [$got]\n# vs.\n# [$expected]\nnot " if $got ne $expected;
    ok;
} else {
    print "ok $test # skipped: one or more static extensions\n"; $test++;
}

