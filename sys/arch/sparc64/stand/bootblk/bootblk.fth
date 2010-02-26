\	$OpenBSD: bootblk.fth,v 1.6 2010/02/26 23:03:50 deraadt Exp $
\	$NetBSD: bootblk.fth,v 1.3 2001/08/15 20:10:24 eeh Exp $
\
\	IEEE 1275 Open Firmware Boot Block
\
\	Parses disklabel and UFS and loads the file called `ofwboot'
\
\
\	Copyright (c) 1998 Eduardo Horvath.
\	All rights reserved.
\
\	Redistribution and use in source and binary forms, with or without
\	modification, are permitted provided that the following conditions
\	are met:
\	1. Redistributions of source code must retain the above copyright
\	   notice, this list of conditions and the following disclaimer.
\	2. Redistributions in binary form must reproduce the above copyright
\	   notice, this list of conditions and the following disclaimer in the
\	   documentation and/or other materials provided with the distribution.
\	3. All advertising materials mentioning features or use of this software
\	   must display the following acknowledgement:
\	     This product includes software developed by Eduardo Horvath.
\	4. The name of the author may not be used to endorse or promote products
\	   derived from this software without specific prior written permission
\
\	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
\	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
\	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
\	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
\	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
\	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
\	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
\	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
\	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
\	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\

offset16
hex
headers

false value boot-debug?

\
\ First some housekeeping:  Open /chosen and set up vectors into
\	client-services

" /chosen" find-package 0=  if ." Cannot find /chosen" 0 then
constant chosen-phandle

" /openprom/client-services" find-package 0=  if 
	." Cannot find client-services" cr abort
then constant cif-phandle

defer cif-claim ( align size virt -- base )
defer cif-release ( size virt -- )
defer cif-open ( cstr -- ihandle|0 )
defer cif-close ( ihandle -- )
defer cif-read ( len adr ihandle -- #read )
defer cif-seek ( low high ihandle -- -1|0|1 )
\ defer cif-peer ( phandle -- phandle )
\ defer cif-getprop ( len adr cstr phandle -- )

: find-cif-method ( method,len -- xf )
   cif-phandle find-method drop 
;

" claim" find-cif-method to cif-claim
" open" find-cif-method to cif-open
" close" find-cif-method to cif-close
" read" find-cif-method to cif-read
" seek" find-cif-method to cif-seek

: twiddle ( -- ) ." ." ; \ Need to do this right.  Just spit out periods for now.

\
\ Support routines
\

: strcmp ( s1 l1 s2 l2 -- true:false )
   rot tuck <> if  3drop false exit then
   comp 0=
;

\ Move string into buffer

: strmov ( s1 l1 d -- d l1 )
   dup 2over swap -rot		( s1 l1 d s1 d l1 )
   move				( s1 l1 d )
   rot drop swap
;

\ Move s1 on the end of s2 and return the result

: strcat ( s1 l1 s2 l2 -- d tot )
   2over swap 				( s1 l1 s2 l2 l1 s1 )
   2over + rot				( s1 l1 s2 l2 s1 d l1 )
   move rot + 				( s1 s2 len )
   rot drop				( s2 len )
;

: strchr ( s1 l1 c -- s2 l2 )
   begin
      dup 2over 0= if			( s1 l1 c c s1  )
         2drop drop exit then
      c@ = if				( s1 l1 c )
         drop exit then
      -rot /c - swap ca1+		( c l2 s2 )
     swap rot
  again
;

   
: cstr ( ptr -- str len )
   dup 
   begin dup c@ 0<>  while + repeat
   over -
;

\
\ BSD FFS parameters
\

fload	assym.fth.h

sbsize buffer: sb-buf
-1 value boot-ihandle
dev_bsize value bsize
0 value raid-offset	\ Offset if it's a raid-frame partition

: strategy ( addr size start -- nread )
   raid-offset + bsize * 0 " seek" boot-ihandle $call-method
   -1 = if 
      ." strategy: Seek failed" cr
      abort
   then
   " read" boot-ihandle $call-method
;

\
\ Cylinder group macros
\

: cgbase ( cg fs -- cgbase ) fs_fpg l@ * ;
: cgstart ( cg fs -- cgstart ) 
   2dup fs_cgmask l@ not and		( cg fs stuff -- )
   over fs_cgoffset l@ * -rot		( stuffcg fs -- )
   cgbase +
;
: cgdmin ( cg fs -- 1st-data-block ) dup fs_dblkno l@ -rot cgstart + ;
: cgimin ( cg fs -- inode-block ) dup fs_iblkno l@ -rot cgstart + ;
: cgsblock ( cg fs -- super-block ) dup fs_sblkno l@ -rot cgstart + ;
: cgstod ( cg fs -- cg-block ) dup fs_cblkno l@ -rot cgstart + ;

\
\ Block and frag position macros
\

: blkoff ( pos fs -- off ) fs_qbmask x@ and ;
: fragoff ( pos fs -- off ) fs_qfmask x@ and ;
: lblktosize ( blk fs -- off ) fs_bshift l@ << ;
: lblkno ( pos fs -- off ) fs_bshift l@ >> ;
: numfrags ( pos fs -- off ) fs_fshift l@ >> ;
: blkroundup ( pos fs -- off ) dup fs_bmask l@ -rot fs_qbmask x@ + and ;
: fragroundup ( pos fs -- off ) dup fs_fmask l@ -rot fs_qfmask x@ + and ;
\ : fragroundup ( pos fs -- off ) tuck fs_qfmask x@ + swap fs_fmask l@ and ;
: fragstoblks ( pos fs -- off ) fs_fragshift l@ >> ;
: blkstofrags ( blk fs -- frag ) fs_fragshift l@ << ;
: fragnum ( fsb fs -- off ) fs_frag l@ 1- and ;
: blknum ( fsb fs -- off ) fs_frag l@ 1- not and ;
: dblksize ( lbn dino fs -- size )
   -rot 				( fs lbn dino )
   di_size x@				( fs lbn di_size )
   -rot dup 1+				( di_size fs lbn lbn+1 )
   2over fs_bshift l@			( di_size fs lbn lbn+1 di_size b_shift )
   rot swap <<	>=			( di_size fs lbn res1 )
   swap ndaddr >= or if			( di_size fs )
      swap drop fs_bsize l@ exit	( size )
   then	tuck blkoff swap fragroundup	( size )
;


: ino-to-cg ( ino fs -- cg ) fs_ipg l@ / ;
: ino-to-fsbo ( ino fs -- fsb0 ) fs_inopb l@ mod ;
: ino-to-fsba ( ino fs -- ba )	\ Need to remove the stupid stack diags someday
   2dup 				( ino fs ino fs )
   ino-to-cg				( ino fs cg )
   over					( ino fs cg fs )
   cgimin				( ino fs inode-blk )
   -rot					( inode-blk ino fs )
   tuck 				( inode-blk fs ino fs )
   fs_ipg l@ 				( inode-blk fs ino ipg )
   mod					( inode-blk fs mod )
   swap					( inode-blk mod fs )
   dup 					( inode-blk mod fs fs )
   fs_inopb l@ 				( inode-blk mod fs inopb )
   rot 					( inode-blk fs inopb mod )
   swap					( inode-blk fs mod inopb )
   /					( inode-blk fs div )
   swap					( inode-blk div fs )
   blkstofrags				( inode-blk frag )
   +
;
: fsbtodb ( fsb fs -- db ) fs_fsbtodb l@ << ;

\
\ File stuff
\

niaddr /w* constant narraysize

struct 
   8		field	>f_ihandle	\ device handle
   8 		field 	>f_seekp	\ seek pointer
   8 		field 	>f_fs		\ pointer to super block
   ufs1_dinode_SIZEOF 	field 	>f_di	\ copy of on-disk inode
   8		field	>f_buf		\ buffer for data block
   4		field 	>f_buf_size	\ size of data block
   4		field	>f_buf_blkno	\ block number of data block
constant file_SIZEOF

file_SIZEOF buffer: the-file
sb-buf the-file >f_fs x!

ufs1_dinode_SIZEOF buffer: cur-inode
h# 2000 buffer: indir-block
-1 value indir-addr

\
\ Translate a fileblock to a disk block
\
\ We only allow single indirection
\

: block-map ( fileblock -- diskblock )
   \ Direct block?
   dup ndaddr <  if 			( fileblock )
      cur-inode di_db			( arr-indx arr-start )
      swap la+ l@ exit			( diskblock )
   then 				( fileblock )
   ndaddr -				( fileblock' )
   \ Now we need to check the indirect block
   dup sb-buf fs_nindir l@ <  if	( fileblock' )
      cur-inode di_ib l@ dup		( fileblock' indir-block indir-block )
      indir-addr <>  if 		( fileblock' indir-block )
         to indir-addr			( fileblock' )
         indir-block 			( fileblock' indir-block )
         sb-buf dup fs_bsize l@		( fileblock' indir-block fs fs_bsize )
         swap indir-addr swap		( fileblock' indir-block fs_bsize indiraddr fs )
         fsbtodb 			( fileblock' indir-block fs_bsize db )
         strategy			( fileblock' nread )
      then				( fileblock' nread|indir-block )
      drop \ Really should check return value
      indir-block swap la+ l@ exit
   then
   dup sb-buf fs_nindir -		( fileblock'' )
   \ Now try 2nd level indirect block -- just read twice 
   dup sb-buf fs_nindir l@ dup * < if	( fileblock'' )
      cur-inode di_ib 1 la+ l@		( fileblock'' indir2-block )
      to indir-addr			( fileblock'' )
      \ load 1st level indir block 
      indir-block 			( fileblock'' indir-block )
      sb-buf dup fs_bsize l@		( fileblock'' indir-block fs fs_bsize )
      swap indir-addr swap		( fileblock'' indir-block fs_bsize indiraddr fs )
      fsbtodb 				( fileblock'' indir-block fs_bsize db )
      strategy				( fileblock'' nread )
      drop				( fileblock'' )
      dup sb-buf fs_nindir /		( fileblock'' indir-offset )
      indir-block swap la+ l@		( fileblock'' indirblock )
      to indir-addr			( fileblock'' )
      \ load 2nd level indir block
      indir-block 			( fileblock'' indir-block )
      sb-buf dup fs_bsize l@		( fileblock'' indir-block fs fs_bsize )
      swap indir-addr swap		( fileblock'' indir-block fs_bsize indiraddr fs )
      fsbtodb 				( fileblock'' indir-block fs_bsize db )
      strategy				( fileblock'' nread )
      drop				( fileblock'' )
      sb-buf fs_nindir l@ mod indir-block swap la+ l@ exit
   then
   ." block-map: exceeded max file size" cr
   abort
;

\
\ Read file into internal buffer and return pointer and len
\

0 value cur-block			\ allocated dynamically in ufs-open
0 value cur-blocksize			\ size of cur-block
-1 value cur-blockno
0 value cur-offset

: buf-read-file ( fs -- len buf )
   cur-offset swap			( seekp fs )
   2dup blkoff				( seekp fs off )
   -rot 2dup lblkno			( off seekp fs block )
   swap 2dup cur-inode			( off seekp block fs block fs inop )
   swap dblksize			( off seekp block fs size )
   rot dup cur-blockno			( off seekp fs size block block cur )
   <>  if 				( off seekp fs size block )
      block-map				( off seekp fs size diskblock )
      dup 0=  if			( off seekp fs size diskblock )
         over cur-block swap 0 fill	( off seekp fs size diskblock )
         boot-debug?  if ." buf-read-file fell off end of file" cr then
      else
         2dup sb-buf fsbtodb cur-block -rot strategy	( off seekp fs size diskblock nread )
         rot 2dup <>  if " buf-read-file: short read." cr abort then
      then				( off seekp fs diskblock nread size )
      nip nip				( off seekp fs size )
   else					( off seekp fs size block block cur )
      2drop				( off seekp fs size )
   then
\   dup cur-offset + to cur-offset	\ Set up next xfer -- not done
   nip nip swap -			( len )
   cur-block
;

\
\ Read inode into cur-inode -- uses cur-block
\ 

: read-inode ( inode fs -- )
   twiddle				( inode fs -- inode fs )

   cur-block				( inode fs -- inode fs buffer )

   over					( inode fs buffer -- inode fs buffer fs )
   fs_bsize l@				( inode fs buffer -- inode fs buffer size )

   2over				( inode fs buffer size -- inode fs buffer size inode fs )
   2over				( inode fs buffer size inode fs -- inode fs buffer size inode fs buffer size )
   2swap tuck				( inode fs buffer size inode fs buffer size -- inode fs buffer size buffer size fs inode fs )

   ino-to-fsba 				( inode fs buffer size buffer size fs inode fs -- inode fs buffer size buffer size fs fsba )
   swap					( inode fs buffer size buffer size fs fsba -- inode fs buffer size buffer size fsba fs )
   fsbtodb				( inode fs buffer size buffer size fsba fs -- inode fs buffer size buffer size db )

   dup to cur-blockno			( inode fs buffer size buffer size dstart -- inode fs buffer size buffer size dstart )
   strategy				( inode fs buffer size buffer size dstart -- inode fs buffer size nread )
   <>  if ." read-inode - residual" cr abort then
   dup 2over				( inode fs buffer -- inode fs buffer buffer inode fs )
   ino-to-fsbo				( inode fs buffer -- inode fs buffer buffer fsbo )
   ufs1_dinode_SIZEOF * +			( inode fs buffer buffer fsbo -- inode fs buffer dinop )
   cur-inode ufs1_dinode_SIZEOF move 	( inode fs buffer dinop -- inode fs buffer )
	\ clear out the old buffers
   drop					( inode fs buffer -- inode fs )
   2drop
;

\ Identify inode type

: is-dir? ( dinode -- true:false ) di_mode w@ ifmt and ifdir = ;
: is-symlink? ( dinode -- true:false ) di_mode w@ ifmt and iflnk = ;



\
\ Hunt for directory entry:
\ 
\ repeat
\    load a buffer
\    while entries do
\       if entry == name return
\       next entry
\ until no buffers
\

: search-directory ( str len -- ino|0 )
   0 to cur-offset
   begin cur-offset cur-inode di_size x@ < while	( str len )
      sb-buf buf-read-file		( str len len buf )
      over 0=  if ." search-directory: buf-read-file zero len" cr abort then
      swap dup cur-offset + to cur-offset	( str len buf len )
      2dup + nip			( str len buf bufend )
      swap 2swap rot			( bufend str len buf )
      begin dup 4 pick < while		( bufend str len buf )
         dup d_ino l@ 0<>  if 		( bufend str len buf )
            boot-debug?  if dup dup d_name swap d_namlen c@ type cr then
            2dup d_namlen c@ =  if	( bufend str len buf )
               dup d_name 2over		( bufend str len buf dname str len )
               comp 0= if		( bufend str len buf )
                  \ Found it -- return inode
                  d_ino l@ nip nip nip	( dino )
                  boot-debug?  if ." Found it" cr then 
                  exit 			( dino )
               then
            then			( bufend str len buf )
         then				( bufend str len buf )
         dup d_reclen w@ +		( bufend str len nextbuf )
      repeat
      drop rot drop			( str len )
   repeat
   2drop 2drop 0			( 0 )
;

: ffs_oldcompat ( -- )
\ Make sure old ffs values in sb-buf are sane
   sb-buf fs_npsect dup l@ sb-buf fs_nsect l@ max swap l!
   sb-buf fs_interleave dup l@ 1 max swap l!
   sb-buf fs_postblformat l@ fs_42postblfmt =  if
      8 sb-buf fs_nrpos l!
   then
   sb-buf fs_inodefmt l@ fs_44inodefmt <  if
      sb-buf fs_bsize l@ 
      dup ndaddr * 1- sb-buf fs_maxfilesize x!
      niaddr 0 ?do
	sb-buf fs_nindir l@ * dup	( sizebp sizebp -- )
	sb-buf fs_maxfilesize dup x@	( sizebp sizebp *fs_maxfilesize fs_maxfilesize -- )
	rot 				( sizebp *fs_maxfilesize fs_maxfilesize sizebp -- )
	+ 				( sizebp *fs_maxfilesize new_fs_maxfilesize  -- ) 
        swap x! 			( sizebp -- )
      loop drop 			( -- )
      sb-buf dup fs_bmask l@ not swap fs_qbmask x!
      sb-buf dup fs_fmask l@ not swap fs_qfmask x!
   then
;

: read-super ( sector -- )
0 " seek" boot-ihandle $call-method
   -1 = if 
      ." Seek failed" cr
      abort
   then
   sb-buf sbsize " read" boot-ihandle $call-method
   dup sbsize <>  if
      ." Read of superblock failed" cr
      ." requested" space sbsize .
      ." actual" space . cr
      abort
   else 
      drop
   then
;

: ufs-open ( bootpath,len -- )
   boot-ihandle -1 =  if
      over cif-open dup 0=  if 		( boot-path len ihandle? )
         ." Could not open device" space type cr 
         abort
      then 				( boot-path len ihandle )
      to boot-ihandle			\ Save ihandle to boot device
   then 2drop
   sboff read-super
   sb-buf fs_magic l@ fs_magic_value <>  if
      64 dup to raid-offset 
      dev_bsize * sboff + read-super
      sb-buf fs_magic l@ fs_magic_value <>  if
         ." Invalid superblock magic" cr
         abort
      then
   then
   sb-buf fs_bsize l@ dup maxbsize >  if
      ." Superblock bsize" space . ." too large" cr
      abort
   then 
   dup fs_SIZEOF <  if
      ." Superblock bsize < size of superblock" cr
      abort
   then
   ffs_oldcompat	( fs_bsize -- fs_bsize )
   dup to cur-blocksize alloc-mem to cur-block    \ Allocate cur-block
   boot-debug?  if ." ufs-open complete" cr then
;

: ufs-close ( -- ) 
   boot-ihandle dup -1 <>  if
      cif-close -1 to boot-ihandle 
   then
   cur-block 0<> if
      cur-block cur-blocksize free-mem
   then
;

: boot-path ( -- boot-path )
   " bootpath" chosen-phandle get-package-property  if
      ." Could not find bootpath in /chosen" cr
      abort
   else
      decode-string 2swap 2drop
   then
;

: boot-args ( -- boot-args )
   " bootargs" chosen-phandle get-package-property  if
      ." Could not find bootargs in /chosen" cr
      abort
   else
      decode-string 2swap 2drop
   then
;

2000 buffer: boot-path-str
2000 buffer: boot-path-tmp

: split-path ( path len -- right len left len )
\ Split a string at the `/'
   begin
      dup -rot				( oldlen right len left )
      ascii / left-parse-string		( oldlen right len left len )
      dup 0<>  if 4 roll drop exit then
      2drop				( oldlen right len )
      rot over =			( right len diff )
   until
;

: find-file ( load-file len -- )
   rootino dup sb-buf read-inode	( load-file len -- load-file len ino )
   -rot					( load-file len ino -- pino load-file len )
   \
   \ For each path component
   \ 
   begin split-path dup 0<> while	( pino right len left len -- )
      cur-inode is-dir? not  if ." Inode not directory" cr abort then
      boot-debug?  if ." Looking for" space 2dup type space ." in directory..." cr then
      search-directory			( pino right len left len -- pino right len ino|false )
      dup 0=  if ." Bad path" cr abort then	( pino right len cino )
      sb-buf read-inode			( pino right len )
      cur-inode is-symlink?  if		\ Symlink -- follow the damn thing
         \ Save path in boot-path-tmp
         boot-path-tmp strmov		( pino new-right len )

         \ Now deal with symlink
         cur-inode di_size x@		( pino right len linklen )
         dup sb-buf fs_maxsymlinklen l@	( pino right len linklen linklen maxlinklen )
         <  if				\ Now join the link to the path
            cur-inode di_shortlink l@	( pino right len linklen linkp )
            swap boot-path-str strmov	( pino right len new-linkp linklen )
         else				\ Read file for symlink -- Ugh
            \ Read link into boot-path-str
            boot-path-str dup sb-buf fs_bsize l@
            0 block-map			( pino right len linklen boot-path-str bsize blockno )
            strategy drop swap		( pino right len boot-path-str linklen )
         then 				( pino right len linkp linklen )
         \ Concatenate the two paths
         strcat				( pino new-right newlen )
         swap dup c@ ascii / =  if	\ go to root inode?
            rot drop rootino -rot	( rino len right )
         then
         rot dup sb-buf read-inode	( len right pino )
         -rot swap			( pino right len )
      then				( pino right len )
   repeat
   2drop drop
;

: read-file ( size addr -- )
   \ Read x bytes from a file to buffer
   begin over 0> while
      cur-offset cur-inode di_size x@ >  if ." read-file EOF exceeded" cr abort then
      sb-buf buf-read-file		( size addr len buf )
      over 2over drop swap		( size addr len buf addr len )
      move				( size addr len )
      dup cur-offset + to cur-offset	( size len newaddr )
      tuck +				( size len newaddr )
      -rot - swap			( newaddr newsize )
   repeat
   2drop
;

\
\ According to the 1275 addendum for SPARC processors:
\ Default load-base is 0x4000.  At least 0x8.0000 or
\ 512KB must be available at that address.  
\
\ The Fcode bootblock can take up up to 8KB (O.K., 7.5KB) 
\ so load programs at 0x4000 + 0x2000=> 0x6000
\

h# 6000 constant loader-base

\
\ Elf support -- find the load addr
\

: is-elf? ( hdr -- res? ) h# 7f454c46 = ;

\
\ Finally we finish it all off
\

: load-file-signon ( load-file len boot-path len -- load-file len boot-path len )
   ." Loading file" space 2over type cr ." from device" space 2dup type cr
;

: load-file-print-size ( size -- size )
   ." Loading" space dup . space ." bytes of file..." cr 
;

: load-file ( load-file len boot-path len -- load-base )
   boot-debug?  if load-file-signon then
   the-file file_SIZEOF 0 fill		\ Clear out file structure
   ufs-open 				( load-file len )
   find-file				( )

   \
   \ Now we've found the file we should read it in in one big hunk
   \

   cur-inode di_size x@			( file-len )
   dup " to file-size" evaluate		( file-len )
   boot-debug?  if load-file-print-size then
   0 to cur-offset
   loader-base				( buf-len addr )
   2dup read-file			( buf-len addr )
   ufs-close				( buf-len addr )
   dup is-elf?  if ." load-file: not an elf executable" cr abort then

   \ Luckily the prom should be able to handle ELF executables by itself

   nip					( addr )
;

: do-boot ( bootfile -- )
   ." OpenBSD IEEE 1275 Bootblock 1.2" cr
   boot-path load-file ( -- load-base )
   dup 0<> if  " to load-base init-program" evaluate then
;


boot-args ascii V strchr 0<> swap drop if
 true to boot-debug?
then

boot-args ascii D strchr 0= swap drop if
  " /ofwboot" do-boot
then exit


