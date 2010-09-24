#!./perl
# Tests to ensure that we don't unexpectedly change prototypes of builtins

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

BEGIN { require './test.pl'; }
plan tests => 237;

while (<DATA>) {
    chomp;
    (my $keyword, my $proto, local $TODO) = split " ", $_, 3;
    if ($proto eq 'undef') {
	ok( !defined prototype "CORE::".$keyword, $keyword );
    }
    elsif ($proto eq 'unknown') {
	eval { prototype "CORE::".$keyword };
	like( $@, qr/Can't find an opnumber for/, $keyword );
    }
    else {
	is( "(".prototype("CORE::".$keyword).")", $proto, $keyword );
    }
}

# the keyword list :

__DATA__
abs (_)
accept (**)
alarm (_)
and ()
atan2 ($$)
bind (*$)
binmode (*;$)
bless ($;$)
caller (;$)
chdir (;$)
chmod (@)
chomp undef
chop undef
chown (@)
chr (_)
chroot (_)
close (;*)
closedir (*)
cmp unknown
connect (*$)
continue ()
cos (_)
crypt ($$)
dbmclose (\%)
dbmopen (\%$$)
defined undef
delete undef
die (@)
do undef
dump ()
each (\[@%])
else undef
elsif undef
endgrent ()
endhostent ()
endnetent ()
endprotoent ()
endpwent ()
endservent ()
eof (;*)
eq ($$)
eval undef
exec undef
exists undef
exit (;$)
exp (_)
fcntl (*$$)
fileno (*)
flock (*$)
for undef
foreach undef
fork ()
format undef
formline ($@)
ge ($$)
getc (;*)
getgrent ()
getgrgid ($)
getgrnam ($)
gethostbyaddr ($$)
gethostbyname ($)
gethostent ()
getlogin ()
getnetbyaddr ($$)
getnetbyname ($)
getnetent ()
getpeername (*)
getpgrp (;$)
getppid ()
getpriority ($$)
getprotobyname ($)
getprotobynumber ($)
getprotoent ()
getpwent ()
getpwnam ($)
getpwuid ($)
getservbyname ($$)
getservbyport ($$)
getservent ()
getsockname (*)
getsockopt (*$$)
given undef
glob undef
gmtime (;$)
goto undef
grep undef
gt ($$)
hex (_)
if undef
index ($$;$)
int (_)
ioctl (*$$)
join ($@)
keys (\[@%])
kill (@)
last undef
lc (_)
lcfirst (_)
le ($$)
length (_)
link ($$)
listen (*$)
local undef
localtime (;$)
lock (\$)
log (_)
lstat (*)
lt ($$)
m undef
map undef
mkdir (_;$)
msgctl ($$$)
msgget ($$)
msgrcv ($$$$$)
msgsnd ($$$)
my undef
ne ($$)
next undef
no undef
not ($)
oct (_)
open (*;$@)
opendir (*$)
or ()
ord (_)
our undef
pack ($@)
package undef
pipe (**)
pop (;\@)
pos undef
print undef
printf undef
prototype undef
push (\@@)
q undef
qq undef
qr undef
quotemeta (_)
qw undef
qx undef
rand (;$)
read (*\$$;$)
readdir (*)
readline (;*)
readlink (_)
readpipe (_)
recv (*\$$$)
redo undef
ref (_)
rename ($$)
require undef
reset (;$)
return undef
reverse (@)
rewinddir (*)
rindex ($$;$)
rmdir (_)
s undef
say undef
scalar undef
seek (*$$)
seekdir (*$)
select (;*)
semctl ($$$$)
semget ($$$)
semop ($$)
send (*$$;$)
setgrent ()
sethostent ($)
setnetent ($)
setpgrp (;$$)
setpriority ($$$)
setprotoent ($)
setpwent ()
setservent ($)
setsockopt (*$$$)
shift (;\@)
shmctl ($$$)
shmget ($$$)
shmread ($$$$)
shmwrite ($$$$)
shutdown (*$)
sin (_)
sleep (;$)
socket (*$$$)
socketpair (**$$$)
sort undef
splice (\@;$$@)
split undef
sprintf ($@)
sqrt (_)
srand (;$)
stat (*)
state undef
study undef
sub undef
substr ($$;$$)
symlink ($$)
syscall ($@)
sysopen (*$$;$)
sysread (*\$$;$)
sysseek (*$$)
system undef
syswrite (*$;$$)
tell (;*)
telldir (*)
tie undef
tied undef
time ()
times ()
tr undef
truncate ($$)
uc (_)
ucfirst (_)
umask (;$)
undef undef
unless undef
unlink (@)
unpack ($;$)
unshift (\@@)
untie undef
until undef
use undef
utime (@)
values (\[@%])
vec ($$$)
wait ()
waitpid ($$)
wantarray ()
warn (@)
when undef
while undef
write (;*)
x unknown
xor ($$)
y undef
