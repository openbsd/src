#!./perl -T
use strict;
use Test::More;
BEGIN {
    plan(
        ${^TAINT}
        ? (tests => 45)
        : (skip_all => "A perl without taint support") 
    );
}
use lib qw( ./t/lib );
use Testing qw(
    create_file_ok
    mkdir_ok
    symlink_ok
    dir_path
    file_path
);

my %Expect_File = (); # what we expect for $_
my %Expect_Name = (); # what we expect for $File::Find::name/fullname
my %Expect_Dir  = (); # what we expect for $File::Find::dir
my ($cwd, $cwd_untainted);

BEGIN {
    require File::Spec;
    if ($ENV{PERL_CORE}) {
        # May be doing dynamic loading while @INC is all relative
        @INC = map { $_ = File::Spec->rel2abs($_); /(.*)/; $1 } @INC;
    }
}

use Config;

BEGIN {
    if ($^O ne 'VMS') {
	for (keys %ENV) { # untaint ENV
	    ($ENV{$_}) = $ENV{$_} =~ /(.*)/;
	}
    }

    # Remove insecure directories from PATH
    my @path;
    my $sep = $Config{path_sep};
    foreach my $dir (split(/\Q$sep/,$ENV{'PATH'}))
    {
	##
	## Match the directory taint tests in mg.c::Perl_magic_setenv()
	##
	push(@path,$dir) unless (length($dir) >= 256
				 or
				 substr($dir,0,1) ne "/"
				 or
				 (stat $dir)[2] & 002);
    }
    $ENV{'PATH'} = join($sep,@path);
}

my $symlink_exists = eval { symlink("",""); 1 };

use File::Find;
use File::Spec;
use Cwd;

my $orig_dir = cwd();
( my $orig_dir_untainted ) = $orig_dir =~ m|^(.+)$|; # untaint it

cleanup();

my $found;
find({wanted => sub { ++$found if $_ eq 'taint.t' },
		untaint => 1, untaint_pattern => qr|^(.+)$|}, File::Spec->curdir);

is($found, 1, 'taint.t found once');
$found = 0;

finddepth({wanted => sub { ++$found if $_ eq 'taint.t'; },
           untaint => 1, untaint_pattern => qr|^(.+)$|}, File::Spec->curdir);

is($found, 1, 'taint.t found once again');

my $case = 2;
my $FastFileTests_OK = 0;

sub cleanup {
    chdir($orig_dir_untainted);
    my $need_updir = 0;
    if (-d dir_path('for_find')) {
        $need_updir = 1 if chdir(dir_path('for_find'));
    }
    if (-d dir_path('fa')) {
	unlink file_path('fa', 'fa_ord'),
	       file_path('fa', 'fsl'),
	       file_path('fa', 'faa', 'faa_ord'),
	       file_path('fa', 'fab', 'fab_ord'),
	       file_path('fa', 'fab', 'faba', 'faba_ord'),
	       file_path('fb', 'fb_ord'),
	       file_path('fb', 'fba', 'fba_ord');
	rmdir dir_path('fa', 'faa');
	rmdir dir_path('fa', 'fab', 'faba');
	rmdir dir_path('fa', 'fab');
	rmdir dir_path('fa');
	rmdir dir_path('fb', 'fba');
	rmdir dir_path('fb');
    }
    if ($need_updir) {
        my $updir = $^O eq 'VMS' ? File::Spec::VMS->updir() : File::Spec->updir;
        chdir($updir);
    }
    if (-d dir_path('for_find')) {
	rmdir dir_path('for_find') or print "# Can't rmdir for_find: $!\n";
    }
}

END {
    cleanup();
}

sub wanted_File_Dir {
    print "# \$File::Find::dir => '$File::Find::dir'\t\$_ => '$_'\n";
    s#\.$## if ($^O eq 'VMS' && $_ ne '.'); #
    s/(.dir)?$//i if ($^O eq 'VMS' && -d _);
    ok( $Expect_File{$_}, "found $_ for \$_, as expected" );
    if ( $FastFileTests_OK ) {
        delete $Expect_File{$_}
          unless ( $Expect_Dir{$_} && ! -d _ );
    }
    else {
        delete $Expect_File{$_}
          unless ( $Expect_Dir{$_} && ! -d $_ );
    }
}

sub wanted_File_Dir_prune {
    &wanted_File_Dir;
    $File::Find::prune=1 if  $_ eq 'faba';
}

sub simple_wanted {
    print "# \$File::Find::dir => '$File::Find::dir'\n";
    print "# \$_ => '$_'\n";
}

# Use topdir() to specify a directory path that you want to pass to
# find/finddepth. Historically topdir() differed on Mac OS classic.

*topdir = \&dir_path;

# Use file_path_name() to specify a file path that's expected for
# $File::Find::Name (%Expect_Name). Note: When the no_chdir => 1
# option is in effect, $_ is the same as $File::Find::Name. In that
# case, also use this function to specify a file path that's expected
# for $_.
#
# Historically file_path_name differed on Mac OS classic.

*file_path_name = \&file_path;


mkdir_ok( dir_path('for_find'), 0770 );
ok( chdir( dir_path('for_find')), 'successful chdir() to for_find' );

$cwd = cwd(); # save cwd
( $cwd_untainted ) = $cwd =~ m|^(.+)$|; # untaint it

mkdir_ok( dir_path('fa'), 0770 );
mkdir_ok( dir_path('fb'), 0770  );
create_file_ok( file_path('fb', 'fb_ord') );
mkdir_ok( dir_path('fb', 'fba'), 0770  );
create_file_ok( file_path('fb', 'fba', 'fba_ord') );
SKIP: {
	skip "Creating symlink", 1, unless $symlink_exists;
	ok( symlink('../fb','fa/fsl'), 'Created symbolic link' );
}
create_file_ok( file_path('fa', 'fa_ord') );

mkdir_ok( dir_path('fa', 'faa'), 0770  );
create_file_ok( file_path('fa', 'faa', 'faa_ord') );
mkdir_ok( dir_path('fa', 'fab'), 0770  );
create_file_ok( file_path('fa', 'fab', 'fab_ord') );
mkdir_ok( dir_path('fa', 'fab', 'faba'), 0770  );
create_file_ok( file_path('fa', 'fab', 'faba', 'faba_ord') );

print "# check untainting (no follow)\n";

# untainting here should work correctly

%Expect_File = (File::Spec->curdir => 1, file_path('fsl') =>
                1,file_path('fa_ord') => 1, file_path('fab') => 1,
                file_path('fab_ord') => 1, file_path('faba') => 1,
                file_path('faa') => 1, file_path('faa_ord') => 1);
delete $Expect_File{ file_path('fsl') } unless $symlink_exists;
%Expect_Name = ();

%Expect_Dir = ( dir_path('fa') => 1, dir_path('faa') => 1,
                dir_path('fab') => 1, dir_path('faba') => 1,
                dir_path('fb') => 1, dir_path('fba') => 1);

delete @Expect_Dir{ dir_path('fb'), dir_path('fba') } unless $symlink_exists;

File::Find::find( {wanted => \&wanted_File_Dir_prune, untaint => 1,
		   untaint_pattern => qr|^(.+)$|}, topdir('fa') );

is(scalar keys %Expect_File, 0, 'Found all expected files');

# don't untaint at all, should die
%Expect_File = ();
%Expect_Name = ();
%Expect_Dir  = ();
undef $@;
eval {File::Find::find( {wanted => \&simple_wanted}, topdir('fa') );};
like( $@, qr|Insecure dependency|, 'Tainted directory causes death (good)' );
chdir($cwd_untainted);


# untaint pattern doesn't match, should die
undef $@;

eval {File::Find::find( {wanted => \&simple_wanted, untaint => 1,
                         untaint_pattern => qr|^(NO_MATCH)$|},
                         topdir('fa') );};

like( $@, qr|is still tainted|, 'Bad untaint pattern causes death (good)' );
chdir($cwd_untainted);


# untaint pattern doesn't match, should die when we chdir to cwd
print "# check untaint_skip (No follow)\n";
undef $@;

eval {File::Find::find( {wanted => \&simple_wanted, untaint => 1,
                         untaint_skip => 1, untaint_pattern =>
                         qr|^(NO_MATCH)$|}, topdir('fa') );};

print "# $@" if $@;
#$^D = 8;
like( $@, qr|insecure cwd|, 'Bad untaint pattern causes death in cwd (good)' );

chdir($cwd_untainted);


SKIP: {
    skip "Symbolic link tests", 17, unless $symlink_exists;
    print "# --- symbolic link tests --- \n";
    $FastFileTests_OK= 1;

    print "# check untainting (follow)\n";

    # untainting here should work correctly
    # no_chdir is in effect, hence we use file_path_name to specify the expected paths for %Expect_File

    %Expect_File = (file_path_name('fa') => 1,
		    file_path_name('fa','fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = ();

    %Expect_Dir = (dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb', 'fba') => 1);

    File::Find::find( {wanted => \&wanted_File_Dir, follow_fast => 1,
                       no_chdir => 1, untaint => 1, untaint_pattern =>
                       qr|^(.+)$| }, topdir('fa') );

    is( scalar(keys %Expect_File), 0, 'Found all files in symlink test' );


    # don't untaint at all, should die
    undef $@;

    eval {File::Find::find( {wanted => \&simple_wanted, follow => 1},
			    topdir('fa') );};

    like( $@, qr|Insecure dependency|, 'Not untainting causes death (good)' );
    chdir($cwd_untainted);

    # untaint pattern doesn't match, should die
    undef $@;

    eval {File::Find::find( {wanted => \&simple_wanted, follow => 1,
                             untaint => 1, untaint_pattern =>
                             qr|^(NO_MATCH)$|}, topdir('fa') );};

    like( $@, qr|is still tainted|, 'Bat untaint pattern causes death (good)' );
    chdir($cwd_untainted);

    # untaint pattern doesn't match, should die when we chdir to cwd
    print "# check untaint_skip (Follow)\n";
    undef $@;

    eval {File::Find::find( {wanted => \&simple_wanted, untaint => 1,
                             untaint_skip => 1, untaint_pattern =>
                             qr|^(NO_MATCH)$|}, topdir('fa') );};
    like( $@, qr|insecure cwd|, 'Cwd not untainted with bad pattern (good)' );

    chdir($cwd_untainted);
}
