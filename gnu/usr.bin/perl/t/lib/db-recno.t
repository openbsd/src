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

print "1..30\n";

$Dfile = "Op.db-recno";
unlink $Dfile;

umask(0);

# Check the interface to RECNOINFO

$dbh = TIEHASH DB_File::RECNOINFO ;
print (($dbh->{bval} == undef) ? "ok 1\n" : "not ok 1\n") ;
print (($dbh->{cachesize} == undef) ? "ok 2\n" : "not ok 2\n") ;
print (($dbh->{psize} == undef) ? "ok 3\n" : "not ok 3\n") ;
print (($dbh->{flags} == undef) ? "ok 4\n" : "not ok 4\n") ;
print (($dbh->{lorder} == undef) ? "ok 5\n" : "not ok 5\n") ;
print (($dbh->{reclen} == undef) ? "ok 6\n" : "not ok 6\n") ;
print (($dbh->{bfname} == undef) ? "ok 7\n" : "not ok 7\n") ;

$dbh->{bval} = 3000 ;
print ($dbh->{bval} == 3000 ? "ok 8\n" : "not ok 8\n") ;

$dbh->{cachesize} = 9000 ;
print ($dbh->{cachesize} == 9000 ? "ok 9\n" : "not ok 9\n") ;

$dbh->{psize} = 400 ;
print (($dbh->{psize} == 400) ? "ok 10\n" : "not ok 10\n") ;

$dbh->{flags} = 65 ;
print (($dbh->{flags} == 65) ? "ok 11\n" : "not ok 11\n") ;

$dbh->{lorder} = 123 ;
print (($dbh->{lorder} == 123) ? "ok 12\n" : "not ok 12\n") ;

$dbh->{reclen} = 1234 ;
print ($dbh->{reclen} == 1234 ? "ok 13\n" : "not ok 13\n") ;

$dbh->{bfname} = 1234 ;
print ($dbh->{bfname} == 1234 ? "ok 14\n" : "not ok 14\n") ;


# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
print ($@ eq '' ? "ok 15\n" : "not ok 15\n") ;
eval '$q = $dbh->{fred}' ;
print ($@ eq '' ? "ok 16\n" : "not ok 16\n") ;

# Now check the interface to RECNOINFO

print (($X = tie(@h, DB_File,$Dfile, O_RDWR|O_CREAT, 0640, $DB_RECNO )) ? "ok 17\n" : "not ok 17");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
print (($mode & 0777) == 0640 ? "ok 18\n" : "not ok 18\n");

#$l = @h ;
$l = $X->length ;
print (!$l ? "ok 19\n" : "not ok 19\n");

@data = qw( a b c d ever f g h  i j k longername m n o p) ;

$h[0] = shift @data ;
print ($h[0] eq 'a' ? "ok 20\n" : "not ok 20\n") ;

foreach (@data)
  { $h[++$i] = $_ }

unshift (@data, 'a') ;

print (defined $h[1] ? "ok 21\n" : "not ok 21\n");
print (! defined $h[16] ? "ok 22\n" : "not ok 22\n");
print ($X->length == @data ? "ok 23\n" : "not ok 23\n") ;


# Overwrite an entry & check fetch it
$h[3] = 'replaced' ;
$data[3] = 'replaced' ;
print ($h[3] eq 'replaced' ? "ok 24\n" : "not ok 24\n");

#PUSH
@push_data = qw(added to the end) ;
#push (@h, @push_data) ;
$X->push(@push_data) ;
push (@data, @push_data) ;
print ($h[++$i] eq 'added' ? "ok 25\n" : "not ok 25\n");

# POP
pop (@data) ;
#$value = pop(@h) ;
$value = $X->pop ;
print ($value eq 'end' ? "not ok 26\n" : "ok 26\n");

# SHIFT
#$value = shift @h
$value = $X->shift ;
print ($value eq shift @data ? "not ok 27\n" : "ok 27\n");

# UNSHIFT

# empty list
$X->unshift ;
print ($X->length == @data ? "ok 28\n" : "not ok 28\n") ;

@new_data = qw(add this to the start of the array) ;
#unshift @h, @new_data ;
$X->unshift (@new_data) ;
unshift (@data, @new_data) ;
print ($X->length == @data ? "ok 29\n" : "not ok 29\n") ;

# SPLICE

# Now both arrays should be identical

$ok = 1 ;
$j = 0 ;
foreach (@data)
{
   $ok = 0, last if $_ ne $h[$j ++] ; 
}
print ($ok ? "ok 30\n" : "not ok 30\n") ;

# IMPORTANT - $X must be undefined before the untie otherwise the
#             underlying DB close routine will not get called.
undef $X ;
untie(@h);

unlink $Dfile;

exit ;
