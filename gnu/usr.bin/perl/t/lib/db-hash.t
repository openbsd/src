#!./perl

BEGIN {
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bDB_File\b/) {
	print "1..0\n";
	exit 0;
    }
}

use DB_File; 
use Fcntl;

print "1..43\n";

$Dfile = "Op.db-hash";
unlink $Dfile;

umask(0);

# Check the interface to HASHINFO

$dbh = TIEHASH DB_File::HASHINFO ;
print (($dbh->{bsize} == undef) ? "ok 1\n" : "not ok 1\n") ;
print (($dbh->{ffactor} == undef) ? "ok 2\n" : "not ok 2\n") ;
print (($dbh->{nelem} == undef) ? "ok 3\n" : "not ok 3\n") ;
print (($dbh->{cachesize} == undef) ? "ok 4\n" : "not ok 4\n") ;
print (($dbh->{hash} == undef) ? "ok 5\n" : "not ok 5\n") ;
print (($dbh->{lorder} == undef) ? "ok 6\n" : "not ok 6\n") ;

$dbh->{bsize} = 3000 ;
print ($dbh->{bsize} == 3000 ? "ok 7\n" : "not ok 7\n") ;

$dbh->{ffactor} = 9000 ;
print ($dbh->{ffactor} == 9000 ? "ok 8\n" : "not ok 8\n") ;
#
$dbh->{nelem} = 400 ;
print (($dbh->{nelem} == 400) ? "ok 9\n" : "not ok 9\n") ;

$dbh->{cachesize} = 65 ;
print (($dbh->{cachesize} == 65) ? "ok 10\n" : "not ok 10\n") ;

$dbh->{hash} = "abc" ;
print (($dbh->{hash} eq "abc") ? "ok 11\n" : "not ok 11\n") ;

$dbh->{lorder} = 1234 ;
print ($dbh->{lorder} == 1234 ? "ok 12\n" : "not ok 12\n") ;

# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
print ($@ eq '' ? "ok 13\n" : "not ok 13\n") ;
eval '$q = $dbh->{fred}' ;
print ($@ eq '' ? "ok 14\n" : "not ok 14\n") ;

# Now check the interface to HASH

print (($X = tie(%h, DB_File,$Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH )) ? "ok 15\n" : "not ok 15");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print (($mode & 0777) == 0640 ? "ok 16\n" : "not ok 16\n");

while (($key,$value) = each(%h)) {
    $i++;
}
print (!$i ? "ok 17\n" : "not ok 17\n");

$h{'goner1'} = 'snork';

$h{'abc'} = 'ABC';
print ($h{'abc'} == 'ABC' ? "ok 18\n" : "not ok 18\n") ;
print (defined $h{'jimmy'} ? "not ok 19\n" : "ok 19\n");

$h{'def'} = 'DEF';
$h{'jkl','mno'} = "JKL\034MNO";
$h{'a',2,3,4,5} = join("\034",'A',2,3,4,5);
$h{'a'} = 'A';

#$h{'b'} = 'B';
$X->STORE('b', 'B') ;

$h{'c'} = 'C';

#$h{'d'} = 'D';
$X->put('d', 'D') ;

$h{'e'} = 'E';
$h{'f'} = 'F';
$h{'g'} = 'X';
$h{'h'} = 'H';
$h{'i'} = 'I';

$h{'goner2'} = 'snork';
delete $h{'goner2'};


# IMPORTANT - $X must be undefined before the untie otherwise the
#             underlying DB close routine will not get called.
undef $X ;
untie(%h);


# tie to the same file again, do not supply a type - should default to HASH
print (($X = tie(%h,DB_File,$Dfile, O_RDWR, 0640)) ? "ok 20\n" : "not ok 20: $!\n");

# Modify an entry from the previous tie
$h{'g'} = 'G';

$h{'j'} = 'J';
$h{'k'} = 'K';
$h{'l'} = 'L';
$h{'m'} = 'M';
$h{'n'} = 'N';
$h{'o'} = 'O';
$h{'p'} = 'P';
$h{'q'} = 'Q';
$h{'r'} = 'R';
$h{'s'} = 'S';
$h{'t'} = 'T';
$h{'u'} = 'U';
$h{'v'} = 'V';
$h{'w'} = 'W';
$h{'x'} = 'X';
$h{'y'} = 'Y';
$h{'z'} = 'Z';

$h{'goner3'} = 'snork';

delete $h{'goner1'};
$X->DELETE('goner3');

@keys = keys(%h);
@values = values(%h);

if ($#keys == 29 && $#values == 29) {print "ok 21\n";} else {print "not ok 21\n";}

while (($key,$value) = each(h)) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key gt $value) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

if ($i == 30) {print "ok 22\n";} else {print "not ok 22\n";}

@keys = ('blurfl', keys(h), 'dyick');
if ($#keys == 31) {print "ok 23\n";} else {print "not ok 23\n";}

$h{'foo'} = '';
print ($h{'foo'} eq '' ? "ok 24\n" : "not ok 24\n") ;

$h{''} = 'bar';
print ($h{''} eq 'bar' ? "ok 25\n" : "not ok 25\n") ;

# check cache overflow and numeric keys and contents
$ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
print ($ok ? "ok 26\n" : "not ok 26\n");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print ($size > 0 ? "ok 27\n" : "not ok 27\n");

@h{0..200} = 200..400;
@foo = @h{0..200};
print join(':',200..400) eq join(':',@foo) ? "ok 28\n" : "not ok 28\n";


# Now check all the non-tie specific stuff

# Check NOOVERWRITE will make put fail when attempting to overwrite
# an existing record.
 
$status = $X->put( 'x', 'newvalue', R_NOOVERWRITE) ;
print ($status == 1 ? "ok 29\n" : "not ok 29\n") ;
 
# check that the value of the key 'x' has not been changed by the 
# previous test
print ($h{'x'} eq 'X' ? "ok 30\n" : "not ok 30\n") ;

# standard put
$status = $X->put('key', 'value') ;
print ($status == 0 ? "ok 31\n" : "not ok 31\n") ;

#check that previous put can be retrieved
$status = $X->get('key', $value) ;
print ($status == 0 ? "ok 32\n" : "not ok 32\n") ;
print ($value eq 'value' ? "ok 33\n" : "not ok 33\n") ;

# Attempting to delete an existing key should work

$status = $X->del('q') ;
print ($status == 0 ? "ok 34\n" : "not ok 34\n") ;

# Make sure that the key deleted, cannot be retrieved
print (($h{'q'} eq undef) ? "ok 35\n" : "not ok 35\n") ;

# Attempting to delete a non-existant key should fail

$status = $X->del('joe') ;
print ($status == 1 ? "ok 36\n" : "not ok 36\n") ;

# Check the get interface

# First a non-existing key
$status = $X->get('aaaa', $value) ;
print ($status == 1 ? "ok 37\n" : "not ok 37\n") ;

# Next an existing key
$status = $X->get('a', $value) ;
print ($status == 0 ? "ok 38\n" : "not ok 38\n") ;
print ($value eq 'A' ? "ok 39\n" : "not ok 39\n") ;

# seq
# ###

# ditto, but use put to replace the key/value pair.

# use seq to walk backwards through a file - check that this reversed is

# check seq FIRST/LAST

# sync
# ####

$status = $X->sync ;
print ($status == 0 ? "ok 40\n" : "not ok 40\n") ;


# fd
# ##

$status = $X->fd ;
print ($status != 0 ? "ok 41\n" : "not ok 41\n") ;

undef $X ;
untie %h ;

unlink $Dfile;

# Now try an in memory file
print (($X = tie(%h, DB_File,undef, O_RDWR|O_CREAT, 0640, $DB_HASH )) ? "ok 42\n" : "not ok 42");

# fd with an in memory file should return fail
$status = $X->fd ;
print ($status == -1 ? "ok 43\n" : "not ok 43\n") ;

untie %h ;
undef $X ;

exit ;
