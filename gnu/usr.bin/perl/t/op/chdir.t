#!./perl -w

BEGIN {
    # We're not going to chdir() into 't' because we don't know if
    # chdir() works!  Instead, we'll hedge our bets and put both
    # possibilities into @INC.
    @INC = qw(t . lib ../lib);
}

use Config;
require "test.pl";
plan(tests => 31);

my $IsVMS   = $^O eq 'VMS';
my $IsMacOS = $^O eq 'MacOS';

# Might be a little early in the testing process to start using these,
# but I can't think of a way to write this test without them.
use File::Spec::Functions qw(:DEFAULT splitdir rel2abs splitpath);

# Can't use Cwd::abs_path() because it has different ideas about
# path separators than File::Spec.
sub abs_path {
    my $d = rel2abs(curdir);

    $d = uc($d) if $IsVMS;
    $d = lc($d) if $^O =~ /^uwin/;
    $d;
}

my $Cwd = abs_path;

# Let's get to a known position
SKIP: {
    my ($vol,$dir) = splitpath(abs_path,1);
    my $test_dir = $IsVMS ? 'T' : 't';
    skip("Already in t/", 2) if (splitdir($dir))[-1] eq $test_dir;

    ok( chdir($test_dir),     'chdir($test_dir)');
    is( abs_path, catdir($Cwd, $test_dir),    '  abs_path() agrees' );
}

$Cwd = abs_path;

# The environment variables chdir() pays attention to.
my @magic_envs = qw(HOME LOGDIR SYS$LOGIN);

sub check_env {
    my($key) = @_;

    # Make sure $ENV{'SYS$LOGIN'} is only honored on VMS.
    if( $key eq 'SYS$LOGIN' && !$IsVMS && !$IsMacOS ) {
        ok( !chdir(),         "chdir() on $^O ignores only \$ENV{$key} set" );
        is( abs_path, $Cwd,   '  abs_path() did not change' );
        pass( "  no need to test SYS\$LOGIN on $^O" ) for 1..7;
    }
    else {
        ok( chdir(),              "chdir() w/ only \$ENV{$key} set" );
        is( abs_path, $ENV{$key}, '  abs_path() agrees' );
        chdir($Cwd);
        is( abs_path, $Cwd,       '  and back again' );

        my $warning = '';
        local $SIG{__WARN__} = sub { $warning .= join '', @_ };


        # Check the deprecated chdir(undef) feature.
#line 64
        ok( chdir(undef),           "chdir(undef) w/ only \$ENV{$key} set" );
        is( abs_path, $ENV{$key},   '  abs_path() agrees' );
        is( $warning,  <<WARNING,   '  got uninit & deprecation warning' );
Use of uninitialized value in chdir at $0 line 64.
Use of chdir('') or chdir(undef) as chdir() is deprecated at $0 line 64.
WARNING

        chdir($Cwd);

        # Ditto chdir('').
        $warning = '';
#line 76
        ok( chdir(''),              "chdir('') w/ only \$ENV{$key} set" );
        is( abs_path, $ENV{$key},   '  abs_path() agrees' );
        is( $warning,  <<WARNING,   '  got deprecation warning' );
Use of chdir('') or chdir(undef) as chdir() is deprecated at $0 line 76.
WARNING

        chdir($Cwd);
    }
}

my %Saved_Env = ();
sub clean_env {
    foreach my $env (@magic_envs) {
        $Saved_Env{$env} = $ENV{$env};

        # Can't actually delete SYS$ stuff on VMS.
        next if $IsVMS && $env eq 'SYS$LOGIN';
        next if $IsVMS && $env eq 'HOME' && !$Config{'d_setenv'};

        unless ($IsMacOS) { # ENV on MacOS is "special" :-)
            # On VMS, %ENV is many layered.
            delete $ENV{$env} while exists $ENV{$env};
        }
    }

    # The following means we won't really be testing for non-existence,
    # but in Perl we can only delete from the process table, not the job 
    # table.
    $ENV{'SYS$LOGIN'} = '' if $IsVMS;
}

END {
    no warnings 'uninitialized';

    # Restore the environment for VMS (and doesn't hurt for anyone else)
    @ENV{@magic_envs} = @Saved_Env{@magic_envs};
}


foreach my $key (@magic_envs) {
    # We're going to be using undefs a lot here.
    no warnings 'uninitialized';

    clean_env;
    $ENV{$key} = catdir $Cwd, ($IsVMS ? 'OP' : 'op');

    check_env($key);
}

{
    clean_env;
    if (($IsVMS || $IsMacOS) && !$Config{'d_setenv'}) {
        pass("Can't reset HOME, so chdir() test meaningless");
    } else {
        ok( !chdir(),                   'chdir() w/o any ENV set' );
    }
    is( abs_path, $Cwd,             '  abs_path() agrees' );
}
