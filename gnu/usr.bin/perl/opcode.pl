#!/usr/bin/perl

open(OC, ">opcode.h") || die "Can't create opcode.h: $!\n";
select OC;

# Read data.

while (<DATA>) {
    chop;
    next unless $_;
    next if /^#/;
    ($key, $desc, $check, $flags, $args) = split(/\t+/, $_, 5);

    warn qq[Description "$desc" duplicates $seen{$desc}\n] if $seen{$desc};
    die qq[Opcode "$key" duplicates $seen{$key}\n] if $seen{$key};
    $seen{$desc} = qq[description of opcode "$key"];
    $seen{$key} = qq[opcode "$key"];

    push(@ops, $key);
    $desc{$key} = $desc;
    $check{$key} = $check;
    $ckname{$check}++;
    $flags{$key} = $flags;
    $args{$key} = $args;
}

# Emit defines.

$i = 0;
print <<"END";
#define pp_i_preinc pp_preinc
#define pp_i_predec pp_predec
#define pp_i_postinc pp_postinc
#define pp_i_postdec pp_postdec

typedef enum {
END
for (@ops) {
    print "\t", &tab(3,"OP_\U$_,"), "/* ", $i++, " */\n";
}
print "\t", &tab(3,"OP_max"), "\n";
print "} opcode;\n";
print "\n#define MAXO ", scalar @ops, "\n\n"; 

# Emit op names and descriptions.

print <<END;
#ifndef DOINIT
EXT char *op_name[];
#else
EXT char *op_name[] = {
END

for (@ops) {
    print qq(\t"$_",\n);
}

print <<END;
};
#endif

END

print <<END;
#ifndef DOINIT
EXT char *op_desc[];
#else
EXT char *op_desc[] = {
END

for (@ops) {
    print qq(\t"$desc{$_}",\n);
}

print <<END;
};
#endif

END

# Emit function declarations.

for (sort keys %ckname) {
    print "OP *\t", &tab(3,$_),"_((OP* op));\n";
}

print "\n";

for (@ops) {
    print "OP *\t", &tab(3, "pp_\L$_"), "_((void));\n";
}

# Emit ppcode switch array.

print <<END;

#ifndef DOINIT
EXT OP * (*ppaddr[])();
#else
EXT OP * (*ppaddr[])() = {
END

for (@ops) {
    print "\tpp_\L$_,\n";
}

print <<END;
};
#endif

END

# Emit check routines.

print <<END;
#ifndef DOINIT
EXT OP * (*check[])();
#else
EXT OP * (*check[])() = {
END

for (@ops) {
    print "\t", &tab(3, "$check{$_},"), "/* \L$_ */\n";
}

print <<END;
};
#endif

END

# Emit allowed argument types.

print <<END;
#ifndef DOINIT
EXT U32 opargs[];
#else
EXT U32 opargs[] = {
END

%argnum = (
    S,	1,		# scalar
    L,	2,		# list
    A,	3,		# array value
    H,	4,		# hash value
    C,	5,		# code value
    F,	6,		# file value
    R,	7,		# scalar reference
);

for (@ops) {
    $argsum = 0;
    $flags = $flags{$_};
    $argsum |= 1 if $flags =~ /m/;		# needs stack mark
    $argsum |= 2 if $flags =~ /f/;		# fold constants
    $argsum |= 4 if $flags =~ /s/;		# always produces scalar
    $argsum |= 8 if $flags =~ /t/;		# needs target scalar
    $argsum |= 16 if $flags =~ /i/;		# always produces integer
    $argsum |= 32 if $flags =~ /I/;		# has corresponding int op
    $argsum |= 64 if $flags =~ /d/;		# danger, unknown side effects
    $argsum |= 128 if $flags =~ /u/;		# defaults to $_
    $mul = 256;
    for $arg (split(' ',$args{$_})) {
	$argnum = ($arg =~ s/\?//) ? 8 : 0;
	$argnum += $argnum{$arg};
	$argsum += $argnum * $mul;
	$mul <<= 4;
    }
    $argsum = sprintf("0x%08x", $argsum);
    print "\t", &tab(3, "$argsum,"), "/* \L$_ */\n";
}

print <<END;
};
#endif
END

###########################################################################
sub tab {
    local($l, $t) = @_;
    $t .= "\t" x ($l - (length($t) + 1) / 8);
    $t;
}
###########################################################################
__END__

# Nothing.

null		null operation		ck_null		0	
stub		stub			ck_null		0
scalar		scalar			ck_fun		s	S

# Pushy stuff.

pushmark	pushmark		ck_null		s	
wantarray	wantarray		ck_null		is	

const		constant item		ck_svconst	s	

gvsv		scalar variable		ck_null		ds	
gv		glob value		ck_null		ds	
gelem		glob elem		ck_null		d	S S
padsv		private variable	ck_null		ds
padav		private array		ck_null		d
padhv		private hash		ck_null		d
padany		private something	ck_null		d

pushre		push regexp		ck_null		0

# References and stuff.

rv2gv		ref-to-glob cast	ck_rvconst	ds	
rv2sv		scalar deref		ck_rvconst	ds	
av2arylen	array length		ck_null		is	
rv2cv		subroutine deref	ck_rvconst	d
anoncode	anonymous subroutine	ck_null		0	
prototype	subroutine prototype	ck_null		s	S
refgen		reference constructor	ck_spair	m	L
srefgen		scalar ref constructor	ck_null		fs	S
ref		reference-type operator	ck_fun		stu	S?
bless		bless			ck_fun		s	S S?

# Pushy I/O.

backtick	backticks		ck_null		t	
glob		glob			ck_glob		t	S S
readline	<HANDLE>		ck_null		t	
rcatline	append I/O operator	ck_null		t	

# Bindable operators.

regcmaybe	regexp comp once	ck_fun		s	S
regcomp		regexp compilation	ck_null		s	S
match		pattern match		ck_match	d
subst		substitution		ck_null		dis	S
substcont	substitution cont	ck_null		dis	
trans		character translation	ck_null		is	S

# Lvalue operators.

sassign		scalar assignment	ck_null		s
aassign		list assignment		ck_null		t	L L

chop		chop			ck_spair	mts	L
schop		scalar chop		ck_null		stu	S?
chomp		safe chop		ck_spair	mts	L
schomp		scalar safe chop	ck_null		stu	S?
defined		defined operator	ck_rfun		isu	S?
undef		undef operator		ck_lfun		s	S?
study		study			ck_fun		su	S?
pos		match position		ck_lfun		stu	S?

preinc		preincrement		ck_lfun		dIs	S
i_preinc	integer preincrement	ck_lfun		dis	S
predec		predecrement		ck_lfun		dIs	S
i_predec	integer predecrement	ck_lfun		dis	S
postinc		postincrement		ck_lfun		dIst	S
i_postinc	integer postincrement	ck_lfun		dist	S
postdec		postdecrement		ck_lfun		dIst	S
i_postdec	integer postdecrement	ck_lfun		dist	S

# Ordinary operators.

pow		exponentiation		ck_null		fst	S S

multiply	multiplication		ck_null		Ifst	S S
i_multiply	integer multiplication	ck_null		ifst	S S
divide		division		ck_null		Ifst	S S
i_divide	integer division	ck_null		ifst	S S
modulo		modulus			ck_null		Iifst	S S
i_modulo	integer modulus		ck_null		ifst	S S
repeat		repeat			ck_repeat	mt	L S

add		addition		ck_null		Ifst	S S
i_add		integer addition	ck_null		ifst	S S
subtract	subtraction		ck_null		Ifst	S S
i_subtract	integer subtraction	ck_null		ifst	S S
concat		concatenation		ck_concat	fst	S S
stringify	string			ck_fun		fst	S

left_shift	left bitshift		ck_null		ifst	S S
right_shift	right bitshift		ck_null		ifst	S S

lt		numeric lt		ck_null		Iifs	S S
i_lt		integer lt		ck_null		ifs	S S
gt		numeric gt		ck_null		Iifs	S S
i_gt		integer gt		ck_null		ifs	S S
le		numeric le		ck_null		Iifs	S S
i_le		integer le		ck_null		ifs	S S
ge		numeric ge		ck_null		Iifs	S S
i_ge		integer ge		ck_null		ifs	S S
eq		numeric eq		ck_null		Iifs	S S
i_eq		integer eq		ck_null		ifs	S S
ne		numeric ne		ck_null		Iifs	S S
i_ne		integer ne		ck_null		ifs	S S
ncmp		spaceship operator	ck_null		Iifst	S S
i_ncmp		integer spaceship	ck_null		ifst	S S

slt		string lt		ck_null		ifs	S S
sgt		string gt		ck_null		ifs	S S
sle		string le		ck_null		ifs	S S
sge		string ge		ck_null		ifs	S S
seq		string eq		ck_null		ifs	S S
sne		string ne		ck_null		ifs	S S
scmp		string comparison	ck_null		ifst	S S

bit_and		bitwise and		ck_null		fst	S S
bit_xor		bitwise xor		ck_null		fst	S S
bit_or		bitwise or		ck_null		fst	S S

negate		negate			ck_null		Ifst	S
i_negate	integer negate		ck_null		ifst	S
not		not			ck_null		ifs	S
complement	1's complement		ck_null		fst	S

# High falutin' math.

atan2		atan2			ck_fun		fst	S S
sin		sin			ck_fun		fstu	S?
cos		cos			ck_fun		fstu	S?
rand		rand			ck_fun		st	S?
srand		srand			ck_fun		s	S?
exp		exp			ck_fun		fstu	S?
log		log			ck_fun		fstu	S?
sqrt		sqrt			ck_fun		fstu	S?

int		int			ck_fun		fstu	S?
hex		hex			ck_fun		istu	S?
oct		oct			ck_fun		istu	S?
abs		abs			ck_fun		fstu	S?

# String stuff.

length		length			ck_lengthconst	istu	S?
substr		substr			ck_fun		st	S S S?
vec		vec			ck_fun		ist	S S S

index		index			ck_index	ist	S S S?
rindex		rindex			ck_index	ist	S S S?

sprintf		sprintf			ck_fun		mst	S L
formline	formline		ck_formline	ms	S L
ord		ord			ck_fun		ifstu	S?
chr		chr			ck_fun		fstu	S?
crypt		crypt			ck_fun		fst	S S
ucfirst		upper case first	ck_fun		fst	S
lcfirst		lower case first	ck_fun		fst	S
uc		upper case		ck_fun		fst	S
lc		lower case		ck_fun		fst	S
quotemeta	quote metachars		ck_fun		fst	S

# Arrays.

rv2av		array deref		ck_rvconst	dt	
aelemfast	known array element	ck_null		s	A S
aelem		array element		ck_null		s	A S
aslice		array slice		ck_null		m	A L

# Associative arrays.

each		each			ck_fun		t	H
values		values			ck_fun		t	H
keys		keys			ck_fun		t	H
delete		delete			ck_delete	s	S
exists		exists operator		ck_delete	is	S
rv2hv		associative array deref	ck_rvconst	dt	
helem		associative array elem	ck_null		s	H S
hslice		associative array slice	ck_null		m	H L

# Explosives and implosives.

unpack		unpack			ck_fun		0	S S
pack		pack			ck_fun		mst	S L
split		split			ck_split	t	S S S
join		join			ck_fun		mst	S L

# List operators.

list		list			ck_null		m	L
lslice		list slice		ck_null		0	H L L
anonlist	anonymous list		ck_fun		ms	L
anonhash	anonymous hash		ck_fun		ms	L

splice		splice			ck_fun		m	A S? S? L
push		push			ck_fun		imst	A L
pop		pop			ck_shift	s	A
shift		shift			ck_shift	s	A
unshift		unshift			ck_fun		imst	A L
sort		sort			ck_sort		m	C? L
reverse		reverse			ck_fun		mt	L

grepstart	grep			ck_grep		dm	C L
grepwhile	grep iterator		ck_null		dt	

mapstart	map			ck_grep		dm	C L
mapwhile	map iterator		ck_null		dt

# Range stuff.

range		flipflop		ck_null		0	S S
flip		range (or flip)		ck_null		0	S S
flop		range (or flop)		ck_null		0

# Control.

and		logical and		ck_null		0	
or		logical or		ck_null		0	
xor		logical xor		ck_null		fs	S S	
cond_expr	conditional expression	ck_null		d	
andassign	logical and assignment	ck_null		s	
orassign	logical or assignment	ck_null		s	

method		method lookup		ck_null		d
entersub	subroutine entry	ck_subr		dmt	L
leavesub	subroutine exit		ck_null		0	
caller		caller			ck_fun		t	S?
warn		warn			ck_fun		imst	L
die		die			ck_fun		dimst	L
reset		reset			ck_fun		is	S?

lineseq		line sequence		ck_null		0	
nextstate	next statement		ck_null		s	
dbstate		debug next statement	ck_null		s	
unstack		unstack			ck_null		s
enter		block entry		ck_null		0	
leave		block exit		ck_null		0	
scope		block			ck_null		0	
enteriter	foreach loop entry	ck_null		d	
iter		foreach loop iterator	ck_null		0	
enterloop	loop entry		ck_null		d	
leaveloop	loop exit		ck_null		0	
return		return			ck_null		dm	L
last		last			ck_null		ds	
next		next			ck_null		ds	
redo		redo			ck_null		ds	
dump		dump			ck_null		ds	
goto		goto			ck_null		ds	
exit		exit			ck_fun		ds	S?

#nswitch		numeric switch		ck_null		d	
#cswitch		character switch	ck_null		d	

# I/O.

open		open			ck_fun		ist	F S?
close		close			ck_fun		is	F?
pipe_op		pipe			ck_fun		is	F F

fileno		fileno			ck_fun		ist	F
umask		umask			ck_fun		ist	S?
binmode		binmode			ck_fun		s	F

tie		tie			ck_fun		idms	R S L
untie		untie			ck_fun		is	R
tied		tied			ck_fun		s	R
dbmopen		dbmopen			ck_fun		is	H S S
dbmclose	dbmclose		ck_fun		is	H

sselect		select system call	ck_select	t	S S S S
select		select			ck_select	st	F?

getc		getc			ck_eof		st	F?
read		read			ck_fun		imst	F R S S?
enterwrite	write			ck_fun		dis	F?
leavewrite	write exit		ck_null		0	

prtf		printf			ck_listiob	ims	F? L
print		print			ck_listiob	ims	F? L

sysopen		sysopen			ck_fun		s	F S S S?
sysread		sysread			ck_fun		imst	F R S S?
syswrite	syswrite		ck_fun		imst	F S S S?

send		send			ck_fun		imst	F S S S?
recv		recv			ck_fun		imst	F R S S

eof		eof			ck_eof		is	F?
tell		tell			ck_fun		st	F?
seek		seek			ck_fun		s	F S S
truncate	truncate		ck_trunc	is	S S

fcntl		fcntl			ck_fun		st	F S S
ioctl		ioctl			ck_fun		st	F S S
flock		flock			ck_fun		ist	F S

# Sockets.

socket		socket			ck_fun		is	F S S S
sockpair	socketpair		ck_fun		is	F F S S S

bind		bind			ck_fun		is	F S
connect		connect			ck_fun		is	F S
listen		listen			ck_fun		is	F S
accept		accept			ck_fun		ist	F F
shutdown	shutdown		ck_fun		ist	F S

gsockopt	getsockopt		ck_fun		is	F S S
ssockopt	setsockopt		ck_fun		is	F S S S

getsockname	getsockname		ck_fun		is	F
getpeername	getpeername		ck_fun		is	F

# Stat calls.

lstat		lstat			ck_ftst		u	F
stat		stat			ck_ftst		u	F
ftrread		-R			ck_ftst		isu	F
ftrwrite	-W			ck_ftst		isu	F
ftrexec		-X			ck_ftst		isu	F
fteread		-r			ck_ftst		isu	F
ftewrite	-w			ck_ftst		isu	F
fteexec		-x			ck_ftst		isu	F
ftis		-e			ck_ftst		isu	F
fteowned	-O			ck_ftst		isu	F
ftrowned	-o			ck_ftst		isu	F
ftzero		-z			ck_ftst		isu	F
ftsize		-s			ck_ftst		istu	F
ftmtime		-M			ck_ftst		stu	F
ftatime		-A			ck_ftst		stu	F
ftctime		-C			ck_ftst		stu	F
ftsock		-S			ck_ftst		isu	F
ftchr		-c			ck_ftst		isu	F
ftblk		-b			ck_ftst		isu	F
ftfile		-f			ck_ftst		isu	F
ftdir		-d			ck_ftst		isu	F
ftpipe		-p			ck_ftst		isu	F
ftlink		-l			ck_ftst		isu	F
ftsuid		-u			ck_ftst		isu	F
ftsgid		-g			ck_ftst		isu	F
ftsvtx		-k			ck_ftst		isu	F
fttty		-t			ck_ftst		is	F
fttext		-T			ck_ftst		isu	F
ftbinary	-B			ck_ftst		isu	F

# File calls.

chdir		chdir			ck_fun		ist	S?
chown		chown			ck_fun		imst	L
chroot		chroot			ck_fun		istu	S?
unlink		unlink			ck_fun		imstu	L
chmod		chmod			ck_fun		imst	L
utime		utime			ck_fun		imst	L
rename		rename			ck_fun		ist	S S
link		link			ck_fun		ist	S S
symlink		symlink			ck_fun		ist	S S
readlink	readlink		ck_fun		stu	S?
mkdir		mkdir			ck_fun		ist	S S
rmdir		rmdir			ck_fun		istu	S?

# Directory calls.

open_dir	opendir			ck_fun		is	F S
readdir		readdir			ck_fun		0	F
telldir		telldir			ck_fun		st	F
seekdir		seekdir			ck_fun		s	F S
rewinddir	rewinddir		ck_fun		s	F
closedir	closedir		ck_fun		is	F

# Process control.

fork		fork			ck_null		ist	
wait		wait			ck_null		ist	
waitpid		waitpid			ck_fun		ist	S S
system		system			ck_exec		imst	S? L
exec		exec			ck_exec		dimst	S? L
kill		kill			ck_fun		dimst	L
getppid		getppid			ck_null		ist	
getpgrp		getpgrp			ck_fun		ist	S?
setpgrp		setpgrp			ck_fun		ist	S? S?
getpriority	getpriority		ck_fun		ist	S S
setpriority	setpriority		ck_fun		ist	S S S

# Time calls.

time		time			ck_null		ist	
tms		times			ck_null		0	
localtime	localtime		ck_fun		t	S?
gmtime		gmtime			ck_fun		t	S?
alarm		alarm			ck_fun		istu	S?
sleep		sleep			ck_fun		ist	S?

# Shared memory.

shmget		shmget			ck_fun		imst	S S S
shmctl		shmctl			ck_fun		imst	S S S
shmread		shmread			ck_fun		imst	S S S S
shmwrite	shmwrite		ck_fun		imst	S S S S

# Message passing.

msgget		msgget			ck_fun		imst	S S
msgctl		msgctl			ck_fun		imst	S S S
msgsnd		msgsnd			ck_fun		imst	S S S
msgrcv		msgrcv			ck_fun		imst	S S S S S

# Semaphores.

semget		semget			ck_fun		imst	S S S
semctl		semctl			ck_fun		imst	S S S S
semop		semop			ck_fun		imst	S S

# Eval.

require		require			ck_require	du	S?
dofile		do 'file'		ck_fun		d	S
entereval	eval string		ck_eval		d	S
leaveeval	eval exit		ck_null		0	S
#evalonce	eval constant string	ck_null		d	S
entertry	eval block		ck_null		0	
leavetry	eval block exit		ck_null		0	

# Get system info.

ghbyname	gethostbyname		ck_fun		0	S
ghbyaddr	gethostbyaddr		ck_fun		0	S S
ghostent	gethostent		ck_null		0	
gnbyname	getnetbyname		ck_fun		0	S
gnbyaddr	getnetbyaddr		ck_fun		0	S S
gnetent		getnetent		ck_null		0	
gpbyname	getprotobyname		ck_fun		0	S
gpbynumber	getprotobynumber	ck_fun		0	S
gprotoent	getprotoent		ck_null		0	
gsbyname	getservbyname		ck_fun		0	S S
gsbyport	getservbyport		ck_fun		0	S S
gservent	getservent		ck_null		0	
shostent	sethostent		ck_fun		is	S
snetent		setnetent		ck_fun		is	S
sprotoent	setprotoent		ck_fun		is	S
sservent	setservent		ck_fun		is	S
ehostent	endhostent		ck_null		is	
enetent		endnetent		ck_null		is	
eprotoent	endprotoent		ck_null		is	
eservent	endservent		ck_null		is	
gpwnam		getpwnam		ck_fun		0	S
gpwuid		getpwuid		ck_fun		0	S
gpwent		getpwent		ck_null		0	
spwent		setpwent		ck_null		is	
epwent		endpwent		ck_null		is	
ggrnam		getgrnam		ck_fun		0	S
ggrgid		getgrgid		ck_fun		0	S
ggrent		getgrent		ck_null		0	
sgrent		setgrent		ck_null		is	
egrent		endgrent		ck_null		is	
getlogin	getlogin		ck_null		st	

# Miscellaneous.

syscall		syscall			ck_fun		imst	S L
