#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        unshift @INC, '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use strict;

# these files help the test run
use Test::More tests => 41;
use Cwd;

# these files are needed for the module itself
use File::Spec;
use File::Path;

# We're going to be chdir'ing and modules are sometimes loaded on the
# fly in this test, so we need an absolute @INC.
@INC = map { File::Spec->rel2abs($_) } @INC;

# keep track of everything added so it can all be deleted
my %Files;
sub add_file {
    my ($file, $data) = @_;
    $data ||= 'foo';
    1 while unlink $file;  # or else we'll get multiple versions on VMS
    open( T, '>'.$file) or return;
    print T $data;
    ++$Files{$file};
    close T;
}

sub read_manifest {
	open( M, 'MANIFEST' ) or return;
	chomp( my @files = <M> );
    close M;
	return @files;
}

sub catch_warning {
	my $warn;
	local $SIG{__WARN__} = sub { $warn .= $_[0] };
	return join('', $_[0]->() ), $warn;
}

sub remove_dir {
	ok( rmdir( $_ ), "remove $_ directory" ) for @_;
}

# use module, import functions
BEGIN { 
    use_ok( 'ExtUtils::Manifest', 
            qw( mkmanifest manicheck filecheck fullcheck 
                maniread manicopy skipcheck maniadd) ); 
}

my $cwd = Cwd::getcwd();

# Just in case any old files were lying around.
rmtree('mantest');

ok( mkdir( 'mantest', 0777 ), 'make mantest directory' );
ok( chdir( 'mantest' ), 'chdir() to mantest' );
ok( add_file('foo'), 'add a temporary file' );

# there shouldn't be a MANIFEST there
my ($res, $warn) = catch_warning( \&mkmanifest ); 
# Canonize the order.
$warn = join("", map { "$_|" } 
                 sort { lc($a) cmp lc($b) } split /\r?\n/, $warn);
is( $warn, "Added to MANIFEST: foo|Added to MANIFEST: MANIFEST|",
    "mkmanifest() displayed its additions" );

# and now you see it
ok( -e 'MANIFEST', 'create MANIFEST file' );

my @list = read_manifest();
is( @list, 2, 'check files in MANIFEST' );
ok( ! ExtUtils::Manifest::filecheck(), 'no additional files in directory' );

# after adding bar, the MANIFEST is out of date
ok( add_file( 'bar' ), 'add another file' );
ok( ! manicheck(), 'MANIFEST now out of sync' );

# it reports that bar has been added and throws a warning
($res, $warn) = catch_warning( \&filecheck );

like( $warn, qr/^Not in MANIFEST: bar/, 'warning that bar has been added' );
is( $res, 'bar', 'bar reported as new' );

# now quiet the warning that bar was added and test again
($res, $warn) = do { local $ExtUtils::Manifest::Quiet = 1; 
                     catch_warning( \&skipcheck ) 
                };
cmp_ok( $warn, 'eq', '', 'disabled warnings' );

# add a skip file with a rule to skip itself (and the nonexistent glob '*baz*')
add_file( 'MANIFEST.SKIP', "baz\n.SKIP" );

# this'll skip the new file
($res, $warn) = catch_warning( \&skipcheck );
like( $warn, qr/^Skipping MANIFEST\.SKIP/i, 'got skipping warning' );

my @skipped;
catch_warning( sub {
	@skipped = skipcheck()
});

is( join( ' ', @skipped ), 'MANIFEST.SKIP', 'listed skipped files' );

{
	local $ExtUtils::Manifest::Quiet = 1;
	is( join(' ', filecheck() ), 'bar', 'listing skipped with filecheck()' );
}

# add a subdirectory and a file there that should be found
ok( mkdir( 'moretest', 0777 ), 'created moretest directory' );
add_file( File::Spec->catfile('moretest', 'quux'), 'quux' );
ok( exists( ExtUtils::Manifest::manifind()->{'moretest/quux'} ), 
                                        "manifind found moretest/quux" );

# only MANIFEST and foo are in the manifest
$_ = 'foo';
my $files = maniread();
is( keys %$files, 2, 'two files found' );
is( join(' ', sort { lc($a) cmp lc($b) } keys %$files), 'foo MANIFEST', 
                                        'both files found' );
is( $_, 'foo', q{maniread() doesn't clobber $_} );

# poison the manifest, and add a comment that should be reported
add_file( 'MANIFEST', 'none #none' );
is( ExtUtils::Manifest::maniread()->{none}, '#none', 
                                        'maniread found comment' );

ok( mkdir( 'copy', 0777 ), 'made copy directory' );

$files = maniread();
eval { (undef, $warn) = catch_warning( sub {
 		manicopy( $files, 'copy', 'cp' ) }) 
};
like( $@, qr/^Can't read none: /, 'croaked about none' );

# a newline comes through, so get rid of it
chomp($warn);

# the copy should have given one warning and one error
like($warn, qr/^Skipping MANIFEST.SKIP/i, 'warned about MANIFEST.SKIP' );

# tell ExtUtils::Manifest to use a different file
{
	local $ExtUtils::Manifest::MANIFEST = 'albatross'; 
	($res, $warn) = catch_warning( \&mkmanifest );
	like( $warn, qr/Added to albatross: /, 'using a new manifest file' );
	
	# add the new file to the list of files to be deleted
	$Files{'albatross'}++;
}


# Make sure MANIFEST.SKIP is using complete relative paths
add_file( 'MANIFEST.SKIP' => "^moretest/q\n" );

# This'll skip moretest/quux
($res, $warn) = catch_warning( \&skipcheck );
like( $warn, qr{^Skipping moretest/quux$}i, 'got skipping warning again' );


# There was a bug where entries in MANIFEST would be blotted out
# by MANIFEST.SKIP rules.
add_file( 'MANIFEST.SKIP' => 'foo' );
add_file( 'MANIFEST'      => "foobar\n"   );
add_file( 'foobar'        => '123' );
($res, $warn) = catch_warning( \&manicheck );
is( $res,  '',      'MANIFEST overrides MANIFEST.SKIP' );
is( $warn, undef,   'MANIFEST overrides MANIFEST.SKIP, no warnings' );

$files = maniread;
ok( !$files->{wibble},     'MANIFEST in good state' );
maniadd({ wibble => undef });
maniadd({ yarrow => "hock" });
$files = maniread;
is( $files->{wibble}, '',    'maniadd() with undef comment' );
is( $files->{yarrow}, 'hock','          with comment' );
is( $files->{foobar}, '',    '          preserved old entries' );

add_file('MANIFEST'   => 'Makefile.PL');
maniadd({ foo  => 'bar' });
$files = maniread;
# VMS downcases the MANIFEST.  We normalize it here to match.
%$files = map { (lc $_ => $files->{$_}) } keys %$files;
my %expect = ( 'makefile.pl' => '',
               'foo'    => 'bar'
             );
is_deeply( $files, \%expect, 'maniadd() vs MANIFEST without trailing newline');

add_file('MANIFEST'   => 'Makefile.PL');
maniadd({ foo => 'bar' });

SKIP: {
    chmod( 0400, 'MANIFEST' );
    skip "Can't make MANIFEST read-only", 2 if -w 'MANIFEST';

    eval {
        maniadd({ 'foo' => 'bar' });
    };
    is( $@, '',  "maniadd() won't open MANIFEST if it doesn't need to" );

    eval {
        maniadd({ 'grrrwoof' => 'yippie' });
    };
    like( $@, qr/^\Qmaniadd() could not open MANIFEST:\E/,  
                 "maniadd() dies if it can't open the MANIFEST" );

    chmod( 0600, 'MANIFEST' );
}
    

END {
	is( unlink( keys %Files ), keys %Files, 'remove all added files' );
	remove_dir( 'moretest', 'copy' );

	# now get rid of the parent directory
	ok( chdir( $cwd ), 'return to parent directory' );
	remove_dir( 'mantest' );
}

