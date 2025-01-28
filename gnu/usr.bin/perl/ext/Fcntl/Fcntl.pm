package Fcntl;

=head1 NAME

Fcntl - various flag constants and helper functions from C's fcntl.h

=head1 SYNOPSIS

    use Fcntl;
    use Fcntl qw(:DEFAULT :flock);
    use Fcntl qw(F_GETFD F_SETFD FD_CLOEXEC);

=head1 DESCRIPTION

This module provides flags and helper functions for use with L<perlfunc/chmod>
(S_*), L<perlfunc/fcntl> (F_*), L<perlfunc/flock> (LOCK_*), L<perlfunc/seek>
(SEEK_*), L<perlfunc/stat> (S_*), L<perlfunc/sysopen> (O_*), and
L<perlfunc/sysseek> (SEEK_*). They correspond to the C macros defined in
F<fcntl.h>.

Not all symbols are available on all systems. Except where noted otherwise,
the constants and functions provided by this module will throw a runtime
exception if the corresponding C macro is not available. Consult your system
documentation to see the full description of each symbol and whether it is
available on your platform: L<chmod(2)>, L<fcntl(2)>, L<flock(2)>,
L<lseek(2)>, L<open(2)>, L<stat(2)>.

(In particular, some of the F_* symbols are highly non-portable because they
only exist on a single platform or require system-specific C data structures to
be passed as the third argument to C<fcntl>, which can't be portably
constructed in pure Perl.)

=head1 EXPORTED SYMBOLS

=head2 Default exports and export tags

The full list of default exports can be found below in L</APPENDIX A>.

In addition, the following export tags are available (see L<Exporter> for more
information on export tags):

=over

=item C<:DEFAULT>

Equivalent to the list of default export symbols (see L</APPENDIX A>).

=item C<:flock>

Equivalent to all LOCK_* symbols listed below.

=item C<:mode>

Equivalent to all S_* symbols listed below.

=item C<:seek>

Equivalent to all SEEK_* symbols listed below.

=item C<:Fcompat>

Equivalent to C<qw(FAPPEND FASYNC FCREAT FDEFER FDSYNC FEXCL FLARGEFILE FNDELAY
FNONBLOCK FRSYNC FSYNC FTRUNC)>. These only exist for compatibility with old
code (if your platform defines them at all) and should not be used in new code.

=back

=head2 Symbols for use with C<fcntl>

=over

=item C<F_ALLOCSP>

File storage manipulation.

=item C<F_ALLOCSP64>

File storage manipulation.

=item C<F_DUP2FD>

Duplicate a file descriptor to the number specified in the third argument to
C<fcntl> (if it refers to an open file, it is automatically closed first).

=item C<F_DUPFD>

Duplicate a file descriptor to the lowest unused number greater than or equal
to the third argument of C<fcntl>.

=item C<F_FREESP>

File storage manipulation.

=item C<F_FREESP64>

File storage manipulation.

=item C<F_FSYNC>

Synchronize file data to disk.

=item C<F_FSYNC64>

Synchronize file data to disk.

=item C<F_GETFD>

Return (as a number) the set of file descriptor flags, in which the following
bits may be set:

=over

=item C<FD_CLOEXEC>

During a successful C<exec> call, the file descriptor will be closed
automatically.

=back

=item C<F_GETFL>

Return (as a number) the set of file description status flags (O_*) as set by
C<open> and C<fcntl>. To determine the file access mode, perform a bitwise AND
with L</C<O_ACCMODE>> and see whether the result is equal to C<O_RDONLY>,
C<O_WRONLY>, or C<O_RDWR>.

=item C<F_GETLEASE>

Indicate the type of lease associated with the filehandle (if any) by returning
one of the following flags:

=over

=item C<F_RDLCK>

A read lease.

=item C<F_WRLCK>

A write lease.

=item C<F_UNLCK>

No lease.

=back

=item C<F_GETLK>

Test for the existence of record locks on the file.

=item C<F_GETLK64>

Test for the existence of record locks on the file.

=item C<F_GETOWN>

Return the ID of the process (as a positive number) or group (as a negative
number) that is currently receiving signals for events on the file descriptor.

=item C<F_GETPIPE_SZ>

Return the capacity of the pipe associated with the filehandle.

=item C<F_GETSIG>

Return the number of the signal sent when input or output becomes possible on
the filehandle. A return value of C<0> means C<SIGIO>.

=item C<F_NOTIFY>

File and directory change notification with signals.

=over

=item C<DN_ACCESS>

=item C<DN_ATTRIB>

=item C<DN_CREATE>

=item C<DN_DELETE>

=item C<DN_MODIFY>

=item C<DN_MULTISHOT>

=item C<DN_RENAME>

=back

Z<>

=item C<F_SETFD>

Set the file descriptor flags. See L</C<F_GETFD>> for the list of available
flags.

=item C<F_SETFL>

Set the file description status flags (O_*). Only some flags can be changed
this way.

=item C<F_SETLEASE>

Set a file lease as specified by the third C<fnctl> argument, which must be one
of the following:

=over

=item C<F_RDLCK>

Set a read lease.

=item C<F_WRLCK>

Set a write lease.

=item C<F_UNLCK>

Remove a lease.

=back

=item C<F_SETLK>

Acquire a record lock.

=item C<F_SETLK64>

Acquire a record lock.

=item C<F_SETLKW>

Acquire a record lock and wait for conflicting locks to be released.

=item C<F_SETLKW64>

Acquire a record lock and wait for conflicting locks to be released.

=item C<F_SETOWN>

Set the ID of the process (as a positive number) or group (as a negative
number) that will receive signals for events on the file descriptor.

=item C<F_SETPIPE_SZ>

Set the capacity of the pipe associated with the filehandle. Return the actual
capacity reserved for the pipe, which may be higher than requested.

=item C<F_SETSIG>

Set the number of the signal sent when input or output becomes possible on the
filehandle. An argument of C<0> means C<SIGIO>.

=item C<F_SHARE>

Set share reservation.

=item C<F_UNSHARE>

Remove share reservation.

=item C<F_COMPAT>

=item C<F_EXLCK>

=item C<F_NODNY>

=item C<F_POSIX>

=item C<F_RDACC>

=item C<F_RDDNY>

=item C<F_RWACC>

=item C<F_RWDNY>

=item C<F_SHLCK>

=item C<F_WRACC>

=item C<F_WRDNY>

=back

=head2 Symbols for use with C<flock>

=over

=item C<LOCK_EX>

Request an exclusive lock.

=item C<LOCK_MAND>

Request a mandatory lock.

=item C<LOCK_NB>

Make lock request non-blocking (can be combined with other LOCK_* flags using bitwise OR).

=item C<LOCK_READ>

With C<LOCK_MAND>: Allow concurrent reads.

=item C<LOCK_RW>

With C<LOCK_MAND>: Allow concurrent reads and writes.

=item C<LOCK_SH>

Request a shared lock.

=item C<LOCK_UN>

Release a held lock.

=item C<LOCK_WRITE>

With C<LOCK_MAND>: Allow concurrent writes.

=back

=head2 Symbols for use with C<sysopen>

=over

=item C<O_ACCMODE>

Bit mask for extracting the file access mode (read-only, write-only, or
read/write) from the other flags. This is mainly useful in combination with
L</C<F_GETFL>>.

=item C<O_ALIAS>

(Mac OS) Open alias file (instead of the file that the alias refers to).

=item C<O_ALT_IO>

(NetBSD) Use alternative I/O semantics.

=item C<O_APPEND>

Open the file in append mode. Writes always go to the end of the file.

=item C<O_ASYNC>

Enable signal-based I/O. When the file becomes readable or writable, a signal
is sent.

=item C<O_BINARY>

(Windows) Open the file in binary mode.

=item C<O_CREAT>

If the file to be opened does not exist yet, create it.

=item C<O_DEFER>

(AIX) Changes to the file are kept in memory and not written to disk until the
program performs an explicit L<< C<< $fh->sync() >>|IO::Handle/$io->sync >>.

=item C<O_DIRECT>

Perform direct I/O to/from user-space buffers; avoid caching at the OS level.

=item C<O_DIRECTORY>

Fail if the filename to be opened does not refer to a directory.

=item C<O_DSYNC>

Synchronize file data immediately, like calling L<fdatasync(2)> after each
write.

=item C<O_EVTONLY>

(Mac OS) Open the file for event notifications, not reading or writing.

=item C<O_EXCL>

If the file already exists, fail and set C<$!> to L<C<EEXIST>|Errno> (this only
makes sense in combination with C<O_CREAT>).

=item C<O_EXLOCK>

When the file is opened, atomically obtain an exclusive lock.

=item C<O_IGNORE_CTTY>

(Hurd) If the file to be opened is the controlling terminal for this process,
don't recognize it as such. Operations on this filehandle won't trigger job
control signals.

=item C<O_LARGEFILE>

On 32-bit platforms, allow opening files whose size exceeds 2 GiB
(2,147,483,647 bytes).

=item C<O_NDELAY>

Compatibility symbol. Use C<O_NONBLOCK> instead.

=item C<O_NOATIME>

Don't update the access time of the file when reading from it.

=item C<O_NOCTTY>

If the process does not have a controlling terminal and the file to be opened
is a terminal device, don't make it the controlling terminal of the process.

=item C<O_NOFOLLOW>

If the final component of the filename is a symbolic link, fail and set C<$!>
to L<C<ELOOP>|Errno>.

=item C<O_NOINHERIT>

(Windows) Don't let child processes inherit the opened file descriptor.

=item C<O_NOLINK>

(Hurd) If the file to be opened is a symbolic link, don't follow it; open the
link itself.

=item C<O_NONBLOCK>

Open the file in non-blocking mode. Neither the open itself nor any read/write
operations on the filehandle will block. (This is mainly useful for pipes and
sockets. It has no effect on regular files.)

=item C<O_NOSIGPIPE>

If the file to be opened is a pipe, then don't raise C<SIGPIPE> for write
operations when the read end of the pipe is closed; make the write fail with
C<EPIPE> instead.

=item C<O_NOTRANS>

(Hurd) If the file to be opened is specially translated, don't invoke the
translator; open the bare file itself.

=item C<O_RANDOM>

(Windows) Indicate that the program intends to access the file contents
randomly (without a predictable pattern). This is an optimization hint for the
file cache (but may cause excessive memory use on large files).

=item C<O_RAW>

(Windows) Same as C<O_BINARY>.

=item C<O_RDONLY>

Open the file for reading (only).

=item C<O_RDWR>

Open the file for reading and writing.

=item C<O_RSRC>

(Mac OS) Open the resource fork of the file.

=item C<O_RSYNC>

Extend the effects of C<O_SYNC> and C<O_DSYNC> to read operations. In
particular, reading from a filehandle opened with C<O_SYNC | O_RSYNC> will wait
until the access time of the file has been modified on disk.

=item C<O_SEQUENTIAL>

(Windows) Indicate that the program intends to access the file contents
sequentially. This is an optimization hint for the file cache.

=item C<O_SHLOCK>

When the file is opened, atomically obtain a shared lock.

=item C<O_SYMLINK>

If the file to be opened is a symbolic link, don't follow it; open the link
itself.

=item C<O_SYNC>

Synchronize file data and metadata immediately, like calling L<fsync(2)> after
each write.

=item C<O_TEMPORARY>

(Windows) Delete the file when its last open file descriptor is closed.

=item C<O_TEXT>

(Windows) Open the file in text mode.

=item C<O_TMPFILE>

Create an unnamed temporary file. The filename argument specifies the directory
the unnamed file should be placed in.

=item C<O_TRUNC>

If the file already exists, truncate its contents to length 0.

=item C<O_TTY_INIT>

If the file to be opened is a terminal that is not already open in any process,
initialize its L<termios|POSIX/C<POSIX::Termios>> parameters.

=item C<O_WRONLY>

Open the file for writing (only).

=item C<FAPPEND>

Compatibility symbol. Use C<O_APPEND> instead.

=item C<FASYNC>

Compatibility symbol. Use C<O_ASYNC> instead.

=item C<FCREAT>

Compatibility symbol. Use C<O_CREAT> instead.

=item C<FDEFER>

Compatibility symbol. Use C<O_DEFER> instead.

=item C<FDSYNC>

Compatibility symbol. Use C<O_DSYNC> instead.

=item C<FEXCL>

Compatibility symbol. Use C<O_EXCL> instead.

=item C<FLARGEFILE>

Compatibility symbol. Use C<O_LARGEFILE> instead.

=item C<FNDELAY>

Compatibility symbol. Use C<O_NDELAY> instead.

=item C<FNONBLOCK>

Compatibility symbol. Use C<O_NONBLOCK> instead.

=item C<FRSYNC>

Compatibility symbol. Use C<O_RSYNC> instead.

=item C<FSYNC>

Compatibility symbol. Use C<O_SYNC> instead.

=item C<FTRUNC>

Compatibility symbol. Use C<O_TRUNC> instead.

=back

=head2 Symbols for use with C<seek> and C<sysseek>

=over

=item C<SEEK_CUR>

File offsets are relative to the current position in the file.

=item C<SEEK_END>

File offsets are relative to the end of the file (i.e. mostly negative).

=item C<SEEK_SET>

File offsets are absolute (i.e. relative to the beginning of the file).

=back

=head2 Symbols for use with C<stat> and C<chmod>

=over

=item C<S_ENFMT>

Enforce mandatory file locks. (This symbol typically shares its value with
C<S_ISGID>.)

=item C<S_IEXEC>

Compatibility symbol. Use C<S_IXUSR> instead.

=item C<S_IFBLK>

File type: Block device.

=item C<S_IFCHR>

File type: Character device.

=item C<S_IFDIR>

File type: Directory.

=item C<S_IFIFO>

File type: Fifo/pipe.

=item C<S_IFLNK>

File type: Symbolic link.

=item C<S_IFMT>

Bit mask for extracting the file type bits. This symbol can also be used as a
function: C<S_IFMT($mode)> acts like C<$mode & S_IFMT>. The result will be
equal to one of the other S_IF* constants.

=item C<_S_IFMT>

Bit mask for extracting the file type bits. This symbol is an actual constant
and cannot be used as a function; otherwise it is identical to C<S_IFMT>.

=item C<S_IFREG>

File type: Regular file.

=item C<S_IFSOCK>

File type: Socket.

=item C<S_IFWHT>

File type: Whiteout file (used to mark the absence/deletion of a file in overlays).

=item C<S_IMODE>

Function for extracting the permission bits from a file mode.

=item C<S_IREAD>

Compatibility symbol. Use C<S_IRUSR> instead.

=item C<S_IRGRP>

Permissions: Readable by group.

=item C<S_IROTH>

Permissions: Readable by others.

=item C<S_IRUSR>

Permissions: Readable by owner.

=item C<S_IRWXG>

Bit mask for extracting group permissions.

=item C<S_IRWXO>

Bit mask for extracting other permissions.

=item C<S_IRWXU>

Bit mask for extracting owner ("user") permissions.

=item C<S_ISBLK>

Convenience function to check for block devices: C<S_ISBLK($mode)> is
equivalent to C<S_IFMT($mode) == S_IFBLK>.

=item C<S_ISCHR>

Convenience function to check for character  devices: C<S_ISCHR($mode)> is
equivalent to C<S_IFMT($mode) == S_IFCHR>.

=item C<S_ISDIR>

Convenience function to check for directories: C<S_ISDIR($mode)> is
equivalent to C<S_IFMT($mode) == S_IFDIR>.

=item C<S_ISENFMT>

Broken function; do not use. (C<S_ISENFMT($mode)> should always return false,
anyway.)

=item C<S_ISFIFO>

Convenience function to check for fifos: C<S_ISFIFO($mode)> is
equivalent to C<S_IFMT($mode) == S_IFIFO>.

=item C<S_ISGID>

Permissions: Set effective group ID from file (when running executables);
mandatory locking (on non-group-executable files); new files inherit their
group from the directory (on directories).

=item C<S_ISLNK>

Convenience function to check for symbolic links: C<S_ISLNK($mode)> is
equivalent to C<S_IFMT($mode) == S_IFLNK>.

=item C<S_ISREG>

Convenience function to check for regular files: C<S_ISREG($mode)> is
equivalent to C<S_IFMT($mode) == S_IFREG>.

=item C<S_ISSOCK>

Convenience function to check for sockets: C<S_ISSOCK($mode)> is
equivalent to C<S_IFMT($mode) == S_IFSOCK>.

=item C<S_ISTXT>

Compatibility symbol. Use C<S_ISVTX> instead.

=item C<S_ISUID>

Permissions: Set effective user ID from file (when running executables).

=item C<S_ISVTX>

Permissions: Files in this directory can only be deleted/renamed by their owner
(or the directory's owner), even if other users have write permissions to the
directory ("sticky bit").

=item C<S_ISWHT>

Convenience function to check for whiteout files: C<S_ISWHT($mode)> is
equivalent to C<S_IFMT($mode) == S_IFWHT>.

=item C<S_IWGRP>

Permissions: Writable by group.

=item C<S_IWOTH>

Permissions: Writable by others.

=item C<S_IWRITE>

Compatibility symbol. Use C<S_IWUSR> instead.

=item C<S_IWUSR>

Permissions: Writable by owner.

=item C<S_IXGRP>

Permissions: Executable/searchable by group.

=item C<S_IXOTH>

Permissions: Executable/searchable by others.

=item C<S_IXUSR>

Permissions: Executable/searchable by owner.

=back

=head1 SEE ALSO

L<perlfunc/chmod>, L<chmod(2)>,
L<perlfunc/fcntl>, L<fcntl(2)>,
L<perlfunc/flock>, L<flock(2)>,
L<perlfunc/seek>, L<fseek(3)>,
L<perlfunc/stat>, L<stat(2)>,
L<perlfunc/sysopen>, L<open(2)>,
L<perlfunc/sysseek>, L<lseek(2)>

=head1 APPENDIX A

By default, if you say C<use Fcntl;>, the following symbols are exported:

    FD_CLOEXEC
    F_ALLOCSP
    F_ALLOCSP64
    F_COMPAT
    F_DUP2FD
    F_DUPFD
    F_EXLCK
    F_FREESP
    F_FREESP64
    F_FSYNC
    F_FSYNC64
    F_GETFD
    F_GETFL
    F_GETLK
    F_GETLK64
    F_GETOWN
    F_NODNY
    F_POSIX
    F_RDACC
    F_RDDNY
    F_RDLCK
    F_RWACC
    F_RWDNY
    F_SETFD
    F_SETFL
    F_SETLK
    F_SETLK64
    F_SETLKW
    F_SETLKW64
    F_SETOWN
    F_SHARE
    F_SHLCK
    F_UNLCK
    F_UNSHARE
    F_WRACC
    F_WRDNY
    F_WRLCK
    O_ACCMODE
    O_ALIAS
    O_APPEND
    O_ASYNC
    O_BINARY
    O_CREAT
    O_DEFER
    O_DIRECT
    O_DIRECTORY
    O_DSYNC
    O_EXCL
    O_EXLOCK
    O_LARGEFILE
    O_NDELAY
    O_NOCTTY
    O_NOFOLLOW
    O_NOINHERIT
    O_NONBLOCK
    O_RANDOM
    O_RAW
    O_RDONLY
    O_RDWR
    O_RSRC
    O_RSYNC
    O_SEQUENTIAL
    O_SHLOCK
    O_SYNC
    O_TEMPORARY
    O_TEXT
    O_TRUNC
    O_WRONLY

=cut

use strict;

use Exporter 'import';
require XSLoader;
our $VERSION = '1.18';

XSLoader::load();

# Named groups of exports
our %EXPORT_TAGS = (
    'flock'   => [qw(LOCK_SH LOCK_EX LOCK_NB LOCK_UN)],
    'Fcompat' => [qw(FAPPEND FASYNC FCREAT FDEFER FDSYNC FEXCL FLARGEFILE
		     FNDELAY FNONBLOCK FRSYNC FSYNC FTRUNC)],
    'seek'    => [qw(SEEK_SET SEEK_CUR SEEK_END)],
    'mode'    => [qw(S_ISUID S_ISGID S_ISVTX S_ISTXT
		     _S_IFMT S_IFREG S_IFDIR S_IFLNK
		     S_IFSOCK S_IFBLK S_IFCHR S_IFIFO S_IFWHT S_ENFMT
		     S_IRUSR S_IWUSR S_IXUSR S_IRWXU
		     S_IRGRP S_IWGRP S_IXGRP S_IRWXG
		     S_IROTH S_IWOTH S_IXOTH S_IRWXO
		     S_IREAD S_IWRITE S_IEXEC
		     S_ISREG S_ISDIR S_ISLNK S_ISSOCK
		     S_ISBLK S_ISCHR S_ISFIFO
		     S_ISWHT S_ISENFMT
		     S_IFMT S_IMODE
                  )],
);

# Items to export into callers namespace by default
# (move infrequently used names to @EXPORT_OK below)
our @EXPORT =
  qw(
	FD_CLOEXEC
	F_ALLOCSP
	F_ALLOCSP64
	F_COMPAT
	F_DUP2FD
	F_DUPFD
	F_EXLCK
	F_FREESP
	F_FREESP64
	F_FSYNC
	F_FSYNC64
	F_GETFD
	F_GETFL
	F_GETLK
	F_GETLK64
	F_GETOWN
	F_NODNY
	F_POSIX
	F_RDACC
	F_RDDNY
	F_RDLCK
	F_RWACC
	F_RWDNY
	F_SETFD
	F_SETFL
	F_SETLK
	F_SETLK64
	F_SETLKW
	F_SETLKW64
	F_SETOWN
	F_SHARE
	F_SHLCK
	F_UNLCK
	F_UNSHARE
	F_WRACC
	F_WRDNY
	F_WRLCK
	O_ACCMODE
	O_ALIAS
	O_APPEND
	O_ASYNC
	O_BINARY
	O_CREAT
	O_DEFER
	O_DIRECT
	O_DIRECTORY
	O_DSYNC
	O_EXCL
	O_EXLOCK
	O_LARGEFILE
	O_NDELAY
	O_NOCTTY
	O_NOFOLLOW
	O_NOINHERIT
	O_NONBLOCK
	O_RANDOM
	O_RAW
	O_RDONLY
	O_RDWR
	O_RSRC
	O_RSYNC
	O_SEQUENTIAL
	O_SHLOCK
	O_SYNC
	O_TEMPORARY
	O_TEXT
	O_TRUNC
	O_WRONLY
     );

# Other items we are prepared to export if requested
our @EXPORT_OK = (qw(
	DN_ACCESS
	DN_ATTRIB
	DN_CREATE
	DN_DELETE
	DN_MODIFY
	DN_MULTISHOT
	DN_RENAME
	F_ADD_SEALS
	F_GETLEASE
	F_GETPIPE_SZ
	F_GET_SEALS
	F_GETSIG
	F_NOTIFY
	F_SEAL_FUTURE_WRITE
	F_SEAL_GROW
	F_SEAL_SEAL
	F_SEAL_SHRINK
	F_SEAL_WRITE
	F_SETLEASE
	F_SETPIPE_SZ
	F_SETSIG
	LOCK_MAND
	LOCK_READ
	LOCK_RW
	LOCK_WRITE
        O_ALT_IO
        O_EVTONLY
	O_IGNORE_CTTY
	O_NOATIME
	O_NOLINK
        O_NOSIGPIPE
	O_NOTRANS
        O_SYMLINK
        O_TMPFILE
        O_TTY_INIT
), map {@{$_}} values %EXPORT_TAGS);

1;
