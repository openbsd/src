#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') { 
	@INC = qw(: ::lib ::macos:lib); 
    } else { 
	@INC = '.'; 
	push @INC, '../lib'; 
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bFile\/Glob\b/i) {
        print "1..0\n";
        exit 0;
    }
    print "1..12\n";
}
END {
    print "not ok 1\n" unless $loaded;
}
use File::Glob ':glob';
use Cwd ();
$loaded = 1;
print "ok 1\n";

sub array {
    return '(', join(", ", map {defined $_ ? "\"$_\"" : "undef"} @a), ")\n";
}

# look for the contents of the current directory
$ENV{PATH} = "/bin";
delete @ENV{BASH_ENV, CDPATH, ENV, IFS};
@correct = ();
if (opendir(D, $^O eq "MacOS" ? ":" : ".")) {
   @correct = grep { !/^\./ } sort readdir(D);
   closedir D;
}
@a = File::Glob::glob("*", 0);
@a = sort @a;
if ("@a" ne "@correct" || GLOB_ERROR) {
    print "# |@a| ne |@correct|\nnot ";
}
print "ok 2\n";

# look up the user's home directory
# should return a list with one item, and not set ERROR
if ($^O ne 'MSWin32' && $^O ne 'NetWare' && $^O ne 'VMS' && $^O ne 'os2') {
  eval {
    ($name, $home) = (getpwuid($>))[0,7];
    1;
  } and do {
    if (defined $home && defined $name && -d $home) {
	@a = bsd_glob("~$name", GLOB_TILDE);
	if ((scalar(@a) != 1 || $a[0] ne $home || GLOB_ERROR)) {
	    print "not ";
	}
    }
  };
}
print "ok 3\n";

# check backslashing
# should return a list with one item, and not set ERROR
@a = bsd_glob('TEST', GLOB_QUOTE);
if (scalar @a != 1 || $a[0] ne 'TEST' || GLOB_ERROR) {
    local $/ = "][";
    print "# [@a]\n";
    print "not ";
}
print "ok 4\n";

# check nonexistent checks
# should return an empty list
# XXX since errfunc is NULL on win32, this test is not valid there
@a = bsd_glob("asdfasdf", 0);
if (($^O ne 'MSWin32' && $^O ne 'NetWare') and scalar @a != 0) {
    print "# |@a|\nnot ";
}
print "ok 5\n";

# check bad protections
# should return an empty list, and set ERROR
if ($^O eq 'mpeix' or $^O eq 'MSWin32' or $^O eq 'NetWare' or $^O eq 'os2' or $^O eq 'VMS'
    or $^O eq 'cygwin' or Cwd::cwd() =~ m#^$Config{'afsroot'}#s or not $>)
{
    print "ok 6 # skipped\n";
}
else {
    $dir = "pteerslo";
    mkdir $dir, 0;
    @a = bsd_glob("$dir/*", GLOB_ERR);
    #print "\@a = ", array(@a);
    rmdir $dir;
    if (scalar(@a) != 0 || GLOB_ERROR == 0) {
	if ($^O eq 'vos') {
	    print "not ok 6 # TODO hit VOS bug posix-956\n";
	} else {
	    print "not ok 6\n";
	}
    }
    else {
	print "ok 6\n";
    }
}

# check for csh style globbing
@a = bsd_glob('{a,b}', GLOB_BRACE | GLOB_NOMAGIC);
unless (@a == 2 and $a[0] eq 'a' and $a[1] eq 'b') {
    print "not ";
}
print "ok 7\n";

@a = bsd_glob(
    '{TES*,doesntexist*,a,b}',
    GLOB_BRACE | GLOB_NOMAGIC | ($^O eq 'VMS' ? GLOB_NOCASE : 0)
);

# Working on t/TEST often causes this test to fail because it sees Emacs temp
# and RCS files.  Filter them out, and .pm files too, and patch temp files.
@a = grep !/(,v$|~$|\.(pm|ori?g|rej)$)/, @a;
@a = (grep !/test.pl/, @a) if $^O eq 'VMS';

print "# @a\n";

unless (@a == 3
        and $a[0] eq ($^O eq 'VMS'? 'test.' : 'TEST')
        and $a[1] eq 'a'
        and $a[2] eq 'b')
{
    print "not ok 8 # @a\n";
} else {
    print "ok 8\n";
}

# "~" should expand to $ENV{HOME}
$ENV{HOME} = "sweet home";
@a = bsd_glob('~', GLOB_TILDE | GLOB_NOMAGIC);
unless ($^O eq "MacOS" || (@a == 1 and $a[0] eq $ENV{HOME})) {
    print "not ";
}
print "ok 9\n";

# GLOB_ALPHASORT (default) should sort alphabetically regardless of case
mkdir "pteerslo", 0777;
chdir "pteerslo";

@f_names = qw(Ax.pl Bx.pl Cx.pl aY.pl bY.pl cY.pl);
@f_alpha = qw(Ax.pl aY.pl Bx.pl bY.pl Cx.pl cY.pl);
if ('a' lt 'A') { # EBCDIC char sets sort lower case before UPPER
    @f_names = sort(@f_names);
}
if ($^O eq 'VMS') { # VMS is happily caseignorant
    @f_alpha = qw(ax.pl ay.pl bx.pl by.pl cx.pl cy.pl);
    @f_names = @f_alpha;
}

for (@f_names) {
    open T, "> $_";
    close T;
}

$pat = "*.pl";

$ok = 1;
@g_names = bsd_glob($pat, 0);
print "# f_names = @f_names\n";
print "# g_names = @g_names\n";
for (@f_names) {
    $ok = 0 unless $_ eq shift @g_names;
}
print $ok ? "ok 10\n" : "not ok 10\n";

$ok = 1;
@g_alpha = bsd_glob($pat);
print "# f_alpha = @f_alpha\n";
print "# g_alpha = @g_alpha\n";
for (@f_alpha) {
    $ok = 0 unless $_ eq shift @g_alpha;
}
print $ok ? "ok 11\n" : "not ok 11\n";

unlink @f_names;
chdir "..";
rmdir "pteerslo";

# this can panic if PL_glob_index gets passed as flags to bsd_glob
<*>; <*>;
print "ok 12\n";
