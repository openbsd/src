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

print "1..76\n";

$Dfile = "Op.db-btree";
unlink $Dfile;

umask(0);

# Check the interface to BTREEINFO

$dbh = TIEHASH DB_File::BTREEINFO ;
print (($dbh->{flags} == undef) ? "ok 1\n" : "not ok 1\n") ;
print (($dbh->{cachesize} == undef) ? "ok 2\n" : "not ok 2\n") ;
print (($dbh->{psize} == undef) ? "ok 3\n" : "not ok 3\n") ;
print (($dbh->{lorder} == undef) ? "ok 4\n" : "not ok 4\n") ;
print (($dbh->{minkeypage} == undef) ? "ok 5\n" : "not ok 5\n") ;
print (($dbh->{maxkeypage} == undef) ? "ok 6\n" : "not ok 6\n") ;
print (($dbh->{compare} == undef) ? "ok 7\n" : "not ok 7\n") ;
print (($dbh->{prefix} == undef) ? "ok 8\n" : "not ok 8\n") ;

$dbh->{flags} = 3000 ;
print ($dbh->{flags} == 3000 ? "ok 9\n" : "not ok 9\n") ;

$dbh->{cachesize} = 9000 ;
print ($dbh->{cachesize} == 9000 ? "ok 10\n" : "not ok 10\n") ;
#
$dbh->{psize} = 400 ;
print (($dbh->{psize} == 400) ? "ok 11\n" : "not ok 11\n") ;

$dbh->{lorder} = 65 ;
print (($dbh->{lorder} == 65) ? "ok 12\n" : "not ok 12\n") ;

$dbh->{minkeypage} = 123 ;
print (($dbh->{minkeypage} == 123) ? "ok 13\n" : "not ok 13\n") ;

$dbh->{maxkeypage} = 1234 ;
print ($dbh->{maxkeypage} == 1234 ? "ok 14\n" : "not ok 14\n") ;

$dbh->{compare} = 1234 ;
print ($dbh->{compare} == 1234 ? "ok 15\n" : "not ok 15\n") ;

$dbh->{prefix} = 1234 ;
print ($dbh->{prefix} == 1234 ? "ok 16\n" : "not ok 16\n") ;

# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
print ($@ eq '' ? "ok 17\n" : "not ok 17\n") ;
eval '$q = $dbh->{fred}' ;
print ($@ eq '' ? "ok 18\n" : "not ok 18\n") ;

# Now check the interface to BTREE

print (($X = tie(%h, DB_File,$Dfile, O_RDWR|O_CREAT, 0640, $DB_BTREE )) ? "ok 19\n" : "not ok 19");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print (($mode & 0777) == 0640 ? "ok 20\n" : "not ok 20\n");

while (($key,$value) = each(%h)) {
    $i++;
}
print (!$i ? "ok 21\n" : "not ok 21\n");

$h{'goner1'} = 'snork';

$h{'abc'} = 'ABC';
print ($h{'abc'} == 'ABC' ? "ok 22\n" : "not ok 22\n") ;
print (defined $h{'jimmy'} ? "not ok 23\n" : "ok 23\n");

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


# tie to the same file again
print (($X = tie(%h,DB_File,$Dfile, O_RDWR, 0640, $DB_BTREE)) ? "ok 24\n" : "not ok 24\n");

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

if ($#keys == 29 && $#values == 29) {print "ok 25\n";} else {print "not ok 25\n";}

while (($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key gt $value) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

if ($i == 30) {print "ok 26\n";} else {print "not ok 26\n";}

@keys = ('blurfl', keys(h), 'dyick');
if ($#keys == 31) {print "ok 27\n";} else {print "not ok 27\n";}

#Check that the keys can be retrieved in order
$ok = 1 ;
foreach (keys %h)
{
    ($ok = 0), last if defined $previous && $previous gt $_ ;
    $previous = $_ ;
}
print ($ok ? "ok 28\n" : "not ok 28\n") ;

$h{'foo'} = '';
print ($h{'foo'} eq '' ? "ok 29\n" : "not ok 29\n") ;

$h{''} = 'bar';
print ($h{''} eq 'bar' ? "ok 30\n" : "not ok 30\n") ;

# check cache overflow and numeric keys and contents
$ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
print ($ok ? "ok 31\n" : "not ok 31\n");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print ($size > 0 ? "ok 32\n" : "not ok 32\n");

@h{0..200} = 200..400;
@foo = @h{0..200};
print join(':',200..400) eq join(':',@foo) ? "ok 33\n" : "not ok 33\n";

# Now check all the non-tie specific stuff


# Check R_NOOVERWRITE flag will make put fail when attempting to overwrite
# an existing record.
 
$status = $X->put( 'x', 'newvalue', R_NOOVERWRITE) ;
print ($status == 1 ? "ok 34\n" : "not ok 34\n") ;
 
# check that the value of the key 'x' has not been changed by the 
# previous test
print ($h{'x'} eq 'X' ? "ok 35\n" : "not ok 35\n") ;

# standard put
$status = $X->put('key', 'value') ;
print ($status == 0 ? "ok 36\n" : "not ok 36\n") ;

#check that previous put can be retrieved
$status = $X->get('key', $value) ;
print ($status == 0 ? "ok 37\n" : "not ok 37\n") ;
print ($value eq 'value' ? "ok 38\n" : "not ok 38\n") ;

# Attempting to delete an existing key should work

$status = $X->del('q') ;
print ($status == 0 ? "ok 39\n" : "not ok 39\n") ;
$status = $X->del('') ;
print ($status == 0 ? "ok 40\n" : "not ok 40\n") ;

# Make sure that the key deleted, cannot be retrieved
print (($h{'q'} eq undef) ? "ok 41\n" : "not ok 41\n") ;
print (($h{''} eq undef) ? "ok 42\n" : "not ok 42\n") ;

undef $X ;
untie %h ;

print (($X = tie(%h, DB_File,$Dfile, O_RDWR, 0640, $DB_BTREE )) ? "ok 43\n" : "not ok 43");

# Attempting to delete a non-existant key should fail

$status = $X->del('joe') ;
print ($status == 1 ? "ok 44\n" : "not ok 44\n") ;

# Check the get interface

# First a non-existing key
$status = $X->get('aaaa', $value) ;
print ($status == 1 ? "ok 45\n" : "not ok 45\n") ;

# Next an existing key
$status = $X->get('a', $value) ;
print ($status == 0 ? "ok 46\n" : "not ok 46\n") ;
print ($value eq 'A' ? "ok 47\n" : "not ok 47\n") ;

# seq
# ###

# use seq to find an approximate match
$key = 'ke' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
print ($status == 0 ? "ok 48\n" : "not ok 48\n") ;
print ($key eq 'key' ? "ok 49\n" : "not ok 49\n") ;
print ($value eq 'value' ? "ok 50\n" : "not ok 50\n") ;

# seq when the key does not match
$key = 'zzz' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
print ($status == 1 ? "ok 51\n" : "not ok 51\n") ;


# use seq to set the cursor, then delete the record @ the cursor.

$key = 'x' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
print ($status == 0 ? "ok 52\n" : "not ok 52\n") ;
print ($key eq 'x' ? "ok 53\n" : "not ok 53\n") ;
print ($value eq 'X' ? "ok 54\n" : "not ok 54\n") ;
$status = $X->del(0, R_CURSOR) ;
print ($status == 0 ? "ok 55\n" : "not ok 55\n") ;
$status = $X->get('x', $value) ;
print ($status == 1 ? "ok 56\n" : "not ok 56\n") ;

# ditto, but use put to replace the key/value pair.
$key = 'y' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
print ($status == 0 ? "ok 57\n" : "not ok 57\n") ;
print ($key eq 'y' ? "ok 58\n" : "not ok 58\n") ;
print ($value eq 'Y' ? "ok 59\n" : "not ok 59\n") ;

$key = "replace key" ;
$value = "replace value" ;
$status = $X->put($key, $value, R_CURSOR) ;
print ($status == 0 ? "ok 60\n" : "not ok 60\n") ;
print ($key eq 'replace key' ? "ok 61\n" : "not ok 61\n") ;
print ($value eq 'replace value' ? "ok 62\n" : "not ok 62\n") ;
$status = $X->get('y', $value) ;
print ($status == 1 ? "ok 63\n" : "not ok 63\n") ;

# use seq to walk forwards through a file 

$status = $X->seq($key, $value, R_FIRST) ;
print ($status == 0 ? "ok 64\n" : "not ok 64\n") ;
$previous = $key ;

$ok = 1 ;
while (($status = $X->seq($key, $value, R_NEXT)) == 0)
{
    ($ok = 0), last if ($previous cmp $key) == 1 ;
}

print ($status == 1 ? "ok 65\n" : "not ok 65\n") ;
print ($ok == 1 ? "ok 66\n" : "not ok 66\n") ;

# use seq to walk backwards through a file 
$status = $X->seq($key, $value, R_LAST) ;
print ($status == 0 ? "ok 67\n" : "not ok 67\n") ;
$previous = $key ;

$ok = 1 ;
while (($status = $X->seq($key, $value, R_PREV)) == 0)
{
    ($ok = 0), last if ($previous cmp $key) == -1 ;
    #print "key = [$key] value = [$value]\n" ;
}

print ($status == 1 ? "ok 68\n" : "not ok 68\n") ;
print ($ok == 1 ? "ok 69\n" : "not ok 69\n") ;


# check seq FIRST/LAST

# sync
# ####

$status = $X->sync ;
print ($status == 0 ? "ok 70\n" : "not ok 70\n") ;


# fd
# ##

$status = $X->fd ;
print ($status != 0 ? "ok 71\n" : "not ok 71\n") ;


undef $X ;
untie %h ;

unlink $Dfile;

# Now try an in memory file
print (($Y = tie(%h, DB_File,undef, O_RDWR|O_CREAT, 0640, $DB_BTREE )) ? "ok 72\n" : "not ok 72");

# fd with an in memory file should return failure
$status = $Y->fd ;
print ($status == -1 ? "ok 73\n" : "not ok 73\n") ;

undef $Y ;
untie %h ;

# test multiple callbacks
$Dfile1 = "btree1" ;
$Dfile2 = "btree2" ;
$Dfile3 = "btree3" ;
 
$dbh1 = TIEHASH DB_File::BTREEINFO ;
$dbh1->{compare} = sub { $_[0] <=> $_[1] } ;
 
$dbh2 = TIEHASH DB_File::BTREEINFO ;
$dbh2->{compare} = sub { $_[0] cmp $_[1] } ;
 
$dbh3 = TIEHASH DB_File::BTREEINFO ;
$dbh3->{compare} = sub { length $_[0] <=> length $_[1] } ;
 
 
tie(%h, DB_File,$Dfile1, O_RDWR|O_CREAT, 0640, $dbh1 ) ;
tie(%g, DB_File,$Dfile2, O_RDWR|O_CREAT, 0640, $dbh2 ) ;
tie(%k, DB_File,$Dfile3, O_RDWR|O_CREAT, 0640, $dbh3 ) ;
 
@Keys = qw( 0123 12 -1234 9 987654321 def  ) ;
@srt_1 = sort { $a <=> $b } @Keys ;
@srt_2 = sort { $a cmp $b } @Keys ;
@srt_3 = sort { length $a <=> length $b } @Keys ;
 
foreach (@Keys) {
    $h{$_} = 1 ;
    $g{$_} = 1 ;
    $k{$_} = 1 ;
}
 
sub ArrayCompare
{
    my($a, $b) = @_ ;
 
    return 0 if @$a != @$b ;
 
    foreach (1 .. length @$a)
    {
        return 0 unless $$a[$_] eq $$b[$_] ;
    }
 
    1 ;
}
 
print ( ArrayCompare (\@srt_1, [keys %h]) ? "ok 74\n" : "not ok 74\n") ;
print ( ArrayCompare (\@srt_2, [keys %g]) ? "ok 75\n" : "not ok 75\n") ;
print ( ArrayCompare (\@srt_3, [keys %k]) ? "ok 76\n" : "not ok 76\n") ;

untie %h ;
untie %g ;
untie %k ;
unlink $Dfile1, $Dfile2, $Dfile3 ;

exit ;
