# DB_File.pm -- Perl 5 interface to Berkeley DB 
#
# written by Paul Marquess (pmarquess@bfsec.bt.co.uk)
# last modified 14th November 1995
# version 1.01

package DB_File::HASHINFO ;

use strict;
use vars qw(%elements);
use Carp;

sub TIEHASH
{
    bless {} ;
}

%elements = ( 'bsize'     => 0,
              'ffactor'   => 0,
              'nelem'     => 0,
              'cachesize' => 0,
              'hash'      => 0,
              'lorder'    => 0
            ) ;

sub FETCH 
{  
    return $_[0]{$_[1]} if defined $elements{$_[1]}  ;

    croak "DB_File::HASHINFO::FETCH - Unknown element '$_[1]'" ;
}


sub STORE 
{
    if ( defined $elements{$_[1]} )
    {
        $_[0]{$_[1]} = $_[2] ;
        return ;
    }
    
    croak "DB_File::HASHINFO::STORE - Unknown element '$_[1]'" ;
}

sub DELETE 
{
    if ( defined $elements{$_[1]} )
    {
        delete ${$_[0]}{$_[1]} ;
        return ;
    }
    
    croak "DB_File::HASHINFO::DELETE - Unknown element '$_[1]'" ;
}


sub DESTROY {undef %{$_[0]} }
sub FIRSTKEY { croak "DB_File::HASHINFO::FIRSTKEY is not implemented" }
sub NEXTKEY { croak "DB_File::HASHINFO::NEXTKEY is not implemented" }
sub EXISTS { croak "DB_File::HASHINFO::EXISTS is not implemented" }
sub CLEAR { croak "DB_File::HASHINFO::CLEAR is not implemented" }

package DB_File::BTREEINFO ;

use strict;
use vars qw(%elements);
use Carp;

sub TIEHASH
{
    bless {} ;
}

%elements = ( 'flags'	=> 0,
              'cachesize'  => 0,
              'maxkeypage' => 0,
              'minkeypage' => 0,
              'psize'      => 0,
              'compare'    => 0,
              'prefix'     => 0,
              'lorder'     => 0
            ) ;

sub FETCH 
{  
    return $_[0]{$_[1]} if defined $elements{$_[1]}  ;

    croak "DB_File::BTREEINFO::FETCH - Unknown element '$_[1]'" ;
}


sub STORE 
{
    if ( defined $elements{$_[1]} )
    {
        $_[0]{$_[1]} = $_[2] ;
        return ;
    }
    
    croak "DB_File::BTREEINFO::STORE - Unknown element '$_[1]'" ;
}

sub DELETE 
{
    if ( defined $elements{$_[1]} )
    {
        delete ${$_[0]}{$_[1]} ;
        return ;
    }
    
    croak "DB_File::BTREEINFO::DELETE - Unknown element '$_[1]'" ;
}


sub DESTROY {undef %{$_[0]} }
sub FIRSTKEY { croak "DB_File::BTREEINFO::FIRSTKEY is not implemented" }
sub NEXTKEY { croak "DB_File::BTREEINFO::NEXTKEY is not implemented" }
sub EXISTS { croak "DB_File::BTREEINFO::EXISTS is not implemented" }
sub CLEAR { croak "DB_File::BTREEINFO::CLEAR is not implemented" }

package DB_File::RECNOINFO ;

use strict;
use vars qw(%elements);
use Carp;

sub TIEHASH
{
    bless {} ;
}

%elements = ( 'bval'      => 0,
              'cachesize' => 0,
              'psize'     => 0,
              'flags'     => 0,
              'lorder'    => 0,
              'reclen'    => 0,
              'bfname'    => 0
            ) ;
sub FETCH 
{  
    return $_[0]{$_[1]} if defined $elements{$_[1]}  ;

    croak "DB_File::RECNOINFO::FETCH - Unknown element '$_[1]'" ;
}


sub STORE 
{
    if ( defined $elements{$_[1]} )
    {
        $_[0]{$_[1]} = $_[2] ;
        return ;
    }
    
    croak "DB_File::RECNOINFO::STORE - Unknown element '$_[1]'" ;
}

sub DELETE 
{
    if ( defined $elements{$_[1]} )
    {
        delete ${$_[0]}{$_[1]} ;
        return ;
    }
    
    croak "DB_File::RECNOINFO::DELETE - Unknown element '$_[1]'" ;
}


sub DESTROY {undef %{$_[0]} }
sub FIRSTKEY { croak "DB_File::RECNOINFO::FIRSTKEY is not implemented" }
sub NEXTKEY { croak "DB_File::RECNOINFO::NEXTKEY is not implemented" }
sub EXISTS { croak "DB_File::BTREEINFO::EXISTS is not implemented" }
sub CLEAR { croak "DB_File::BTREEINFO::CLEAR is not implemented" }



package DB_File ;

use strict;
use vars qw($VERSION @ISA @EXPORT $AUTOLOAD $DB_BTREE $DB_HASH $DB_RECNO) ;
use Carp;


$VERSION = "1.01" ;

#typedef enum { DB_BTREE, DB_HASH, DB_RECNO } DBTYPE;
$DB_BTREE = TIEHASH DB_File::BTREEINFO ;
$DB_HASH  = TIEHASH DB_File::HASHINFO ;
$DB_RECNO = TIEHASH DB_File::RECNOINFO ;

require Tie::Hash;
require Exporter;
use AutoLoader;
require DynaLoader;
@ISA = qw(Tie::Hash Exporter DynaLoader);
@EXPORT = qw(
        $DB_BTREE $DB_HASH $DB_RECNO 
	BTREEMAGIC
	BTREEVERSION
	DB_LOCK
	DB_SHMEM
	DB_TXN
	HASHMAGIC
	HASHVERSION
	MAX_PAGE_NUMBER
	MAX_PAGE_OFFSET
	MAX_REC_NUMBER
	RET_ERROR
	RET_SPECIAL
	RET_SUCCESS
	R_CURSOR
	R_DUP
	R_FIRST
	R_FIXEDLEN
	R_IAFTER
	R_IBEFORE
	R_LAST
	R_NEXT
	R_NOKEY
	R_NOOVERWRITE
	R_PREV
	R_RECNOSYNC
	R_SETCURSOR
	R_SNAPSHOT
	__R_UNUSED
);

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    my($pack,$file,$line) = caller;
	    croak "Your vendor has not defined DB macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap DB_File $VERSION;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

1;
__END__

=cut

=head1 NAME

DB_File - Perl5 access to Berkeley DB

=head1 SYNOPSIS

 use DB_File ;
  
 [$X =] tie %hash,  DB_File, $filename [, $flags, $mode, $DB_HASH] ;
 [$X =] tie %hash,  DB_File, $filename, $flags, $mode, $DB_BTREE ;
 [$X =] tie @array, DB_File, $filename, $flags, $mode, $DB_RECNO ;
   
 $status = $X->del($key [, $flags]) ;
 $status = $X->put($key, $value [, $flags]) ;
 $status = $X->get($key, $value [, $flags]) ;
 $status = $X->seq($key, $value [, $flags]) ;
 $status = $X->sync([$flags]) ;
 $status = $X->fd ;
    
 untie %hash ;
 untie @array ;

=head1 DESCRIPTION

B<DB_File> is a module which allows Perl programs to make use of the
facilities provided by Berkeley DB.  If you intend to use this
module you should really have a copy of the Berkeley DB manualpage at
hand. The interface defined here mirrors the Berkeley DB interface
closely.

Berkeley DB is a C library which provides a consistent interface to a
number of database formats.  B<DB_File> provides an interface to all
three of the database types currently supported by Berkeley DB.

The file types are:

=over 5

=item DB_HASH

This database type allows arbitrary key/data pairs to be stored in data
files. This is equivalent to the functionality provided by other
hashing packages like DBM, NDBM, ODBM, GDBM, and SDBM. Remember though,
the files created using DB_HASH are not compatible with any of the
other packages mentioned.

A default hashing algorithm, which will be adequate for most
applications, is built into Berkeley DB. If you do need to use your own
hashing algorithm it is possible to write your own in Perl and have
B<DB_File> use it instead.

=item DB_BTREE

The btree format allows arbitrary key/data pairs to be stored in a
sorted, balanced binary tree.

As with the DB_HASH format, it is possible to provide a user defined
Perl routine to perform the comparison of keys. By default, though, the
keys are stored in lexical order.

=item DB_RECNO

DB_RECNO allows both fixed-length and variable-length flat text files
to be manipulated using the same key/value pair interface as in DB_HASH
and DB_BTREE.  In this case the key will consist of a record (line)
number.

=back

=head2 How does DB_File interface to Berkeley DB?

B<DB_File> allows access to Berkeley DB files using the tie() mechanism
in Perl 5 (for full details, see L<perlfunc/tie()>). This facility
allows B<DB_File> to access Berkeley DB files using either an
associative array (for DB_HASH & DB_BTREE file types) or an ordinary
array (for the DB_RECNO file type).

In addition to the tie() interface, it is also possible to use most of
the functions provided in the Berkeley DB API.

=head2 Differences with Berkeley DB

Berkeley DB uses the function dbopen() to open or create a database.
Below is the C prototype for dbopen().

      DB*
      dbopen (const char * file, int flags, int mode, 
              DBTYPE type, const void * openinfo)

The parameter C<type> is an enumeration which specifies which of the 3
interface methods (DB_HASH, DB_BTREE or DB_RECNO) is to be used.
Depending on which of these is actually chosen, the final parameter,
I<openinfo> points to a data structure which allows tailoring of the
specific interface method.

This interface is handled slightly differently in B<DB_File>. Here is
an equivalent call using B<DB_File>.

        tie %array, DB_File, $filename, $flags, $mode, $DB_HASH ;

The C<filename>, C<flags> and C<mode> parameters are the direct
equivalent of their dbopen() counterparts. The final parameter $DB_HASH
performs the function of both the C<type> and C<openinfo> parameters in
dbopen().

In the example above $DB_HASH is actually a reference to a hash
object. B<DB_File> has three of these pre-defined references. Apart
from $DB_HASH, there is also $DB_BTREE and $DB_RECNO.

The keys allowed in each of these pre-defined references is limited to
the names used in the equivalent C structure. So, for example, the
$DB_HASH reference will only allow keys called C<bsize>, C<cachesize>,
C<ffactor>, C<hash>, C<lorder> and C<nelem>.

To change one of these elements, just assign to it like this

	$DB_HASH->{cachesize} = 10000 ;


=head2 RECNO


In order to make RECNO more compatible with Perl the array offset for all
RECNO arrays begins at 0 rather than 1 as in Berkeley DB.


=head2 In Memory Databases

Berkeley DB allows the creation of in-memory databases by using NULL
(that is, a C<(char *)0> in C) in place of the filename.  B<DB_File>
uses C<undef> instead of NULL to provide this functionality.


=head2 Using the Berkeley DB Interface Directly

As well as accessing Berkeley DB using a tied hash or array, it is also
possible to make direct use of most of the functions defined in the
Berkeley DB documentation.


To do this you need to remember the return value from the tie.

	$db = tie %hash, DB_File, "filename"

Once you have done that, you can access the Berkeley DB API functions
directly.

	$db->put($key, $value, R_NOOVERWRITE) ;

All the functions defined in L<dbx(3X)> are available except for
close() and dbopen() itself. The B<DB_File> interface to these
functions have been implemented to mirror the the way Berkeley DB
works. In particular note that all the functions return only a status
value. Whenever a Berkeley DB function returns data via one of its
parameters, the B<DB_File> equivalent does exactly the same.

All the constants defined in L<dbopen> are also available.

Below is a list of the functions available.

=over 5

=item get

Same as in C<recno> except that the flags parameter is optional.
Remember the value associated with the key you request is returned in
the $value parameter.

=item put

As usual the flags parameter is optional. 

If you use either the R_IAFTER or R_IBEFORE flags, the key parameter
will have the record number of the inserted key/value pair set.

=item del

The flags parameter is optional.

=item fd

As in I<recno>.

=item seq

The flags parameter is optional.

Both the key and value parameters will be set.

=item sync

The flags parameter is optional.

=back

=head1 EXAMPLES

It is always a lot easier to understand something when you see a real
example. So here are a few.

=head2 Using HASH

	use DB_File ;
	use Fcntl ;
	
	tie %h,  "DB_File", "hashed", O_RDWR|O_CREAT, 0640, $DB_HASH ;
	
	# Add a key/value pair to the file
	$h{"apple"} = "orange" ;
	
	# Check for existence of a key
	print "Exists\n" if $h{"banana"} ;
	
	# Delete 
	delete $h{"apple"} ;
	
	untie %h ;

=head2 Using BTREE

Here is sample of code which used BTREE. Just to make life more
interesting the default comparision function will not be used. Instead
a Perl sub, C<Compare()>, will be used to do a case insensitive
comparison.

        use DB_File ;
        use Fcntl ;
	 
	sub Compare
        {
	    my ($key1, $key2) = @_ ;
	
	    "\L$key1" cmp "\L$key2" ;
	}
	
        $DB_BTREE->{compare} = 'Compare' ;
	 
        tie %h,  'DB_File', "tree", O_RDWR|O_CREAT, 0640, $DB_BTREE ;
	 
        # Add a key/value pair to the file
        $h{'Wall'} = 'Larry' ;
        $h{'Smith'} = 'John' ;
	$h{'mouse'} = 'mickey' ;
	$h{'duck'}   = 'donald' ;
	 
        # Delete
        delete $h{"duck"} ;
	 
	# Cycle through the keys printing them in order.
	# Note it is not necessary to sort the keys as
	# the btree will have kept them in order automatically.
	foreach (keys %h)
	  { print "$_\n" }
	
        untie %h ;

Here is the output from the code above.

	mouse
	Smith
	Wall


=head2 Using RECNO

	use DB_File ;
	use Fcntl ;
	
	$DB_RECNO->{psize} = 3000 ;
	
	tie @h,  DB_File, "text", O_RDWR|O_CREAT, 0640, $DB_RECNO ;
	
	# Add a key/value pair to the file
	$h[0] = "orange" ;
	
	# Check for existence of a key
	print "Exists\n" if $h[1] ;
	
	untie @h ;


=head2 Locking Databases

Concurrent access of a read-write database by several parties requires
them all to use some kind of locking.  Here's an example of Tom's that
uses the I<fd> method to get the file descriptor, and then a careful
open() to give something Perl will flock() for you.  Run this repeatedly
in the background to watch the locks granted in proper order.

    use Fcntl;
    use DB_File;

    use strict;

    sub LOCK_SH { 1 }
    sub LOCK_EX { 2 }
    sub LOCK_NB { 4 }
    sub LOCK_UN { 8 }

    my($oldval, $fd, $db, %db, $value, $key);

    $key = shift || 'default';
    $value = shift || 'magic';

    $value .= " $$";

    $db = tie(%db, 'DB_File', '/tmp/foo.db', O_CREAT|O_RDWR, 0644) 
	    || die "dbcreat /tmp/foo.db $!";
    $fd = $db->fd;
    print "$$: db fd is $fd\n";
    open(DB_FH, "+<&=$fd") || die "dup $!";


    unless (flock (DB_FH, LOCK_SH | LOCK_NB)) {
	print "$$: CONTENTION; can't read during write update!
		    Waiting for read lock ($!) ....";
	unless (flock (DB_FH, LOCK_SH)) { die "flock: $!" }
    } 
    print "$$: Read lock granted\n";

    $oldval = $db{$key};
    print "$$: Old value was $oldval\n";
    flock(DB_FH, LOCK_UN);

    unless (flock (DB_FH, LOCK_EX | LOCK_NB)) {
	print "$$: CONTENTION; must have exclusive lock!
		    Waiting for write lock ($!) ....";
	unless (flock (DB_FH, LOCK_EX)) { die "flock: $!" }
    } 

    print "$$: Write lock granted\n";
    $db{$key} = $value;
    sleep 10;

    flock(DB_FH, LOCK_UN);
    untie %db;
    close(DB_FH);
    print "$$: Updated db to $key=$value\n";

=head1 HISTORY

=over

=item 0.1

First Release.

=item 0.2

When B<DB_File> is opening a database file it no longer terminates the
process if I<dbopen> returned an error. This allows file protection
errors to be caught at run time. Thanks to Judith Grass
E<lt>grass@cybercash.comE<gt> for spotting the bug.

=item 0.3

Added prototype support for multiple btree compare callbacks.

=item 1.0

B<DB_File> has been in use for over a year. To reflect that, the
version number has been incremented to 1.0.

Added complete support for multiple concurrent callbacks.

Using the I<push> method on an empty list didn't work properly. This
has been fixed.

=item 1.01

Fixed a core dump problem with SunOS.

The return value from TIEHASH wasn't set to NULL when dbopen returned
an error.

=head1 WARNINGS

If you happen find any other functions defined in the source for this
module that have not been mentioned in this document -- beware.  I may
drop them at a moments notice.

If you cannot find any, then either you didn't look very hard or the
moment has passed and I have dropped them.

=head1 BUGS

Some older versions of Berkeley DB had problems with fixed length
records using the RECNO file format. The newest version at the time of
writing was 1.85 - this seems to have fixed the problems with RECNO.

I am sure there are bugs in the code. If you do find any, or can
suggest any enhancements, I would welcome your comments.

=head1 AVAILABILITY

Berkeley DB is available at your nearest CPAN archive (see
L<perlmod/"CPAN"> for a list) in F<src/misc/db.1.85.tar.gz>, or via the
host F<ftp.cs.berkeley.edu> in F</ucb/4bsd/db.tar.gz>.  It is I<not> under
the GPL.

=head1 SEE ALSO

L<perl(1)>, L<dbopen(3)>, L<hash(3)>, L<recno(3)>, L<btree(3)> 

Berkeley DB is available from F<ftp.cs.berkeley.edu> in the directory
F</ucb/4bsd>.

=head1 AUTHOR

The DB_File interface was written by Paul Marquess
<pmarquess@bfsec.bt.co.uk>.
Questions about the DB system itself may be addressed to Keith Bostic
<bostic@cs.berkeley.edu>.

=cut
