-- Copyright (C) 1992, 1995 Free Software Foundation, Inc.

-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

-- Please email any bugs, comments, and/or additions to this file to:
-- bug-gdb@prep.ai.mit.edu

--
-- test program 1 (refer to tests1.exp)
--

/* These functions are defined in libchill.a */

runtime: SPEC MODULE
DCL chill_argc long;
DCL chill_argv REF ARRAY (0:1000) REF CHARS (1000) VARYING;
__print_event: PROC (arg0 ptr,
       arg1 ptr) END;
__print_buffer: PROC (arg0 ptr,
       arg1 ptr) END;
GRANT ALL;
END;

tests1: module;

seize 	__print_event,
	__print_buffer;

newmode	set1 = set(aaa, bbb, ccc);
newmode	nset1 = set(na = 1, nb = 34, nc = 20);
newmode r11 = range (0 : upper(ubyte));
newmode r12 = range (0 : upper(uint));
--newmode r13 = range (0 : upper(ulong)); -- bug in gnuchill
newmode r14 = range (lower(byte) : upper(byte));
newmode r15 = range (lower(int) : upper(int));
newmode r16 = range (lower(long): upper(long));
newmode r2 = set1(bbb : ccc);
newmode r3 = nset1(na : na);
newmode r4 = nset1(nc : nb);
newmode r5 = nset1(lower(nset1) : upper(nset1));

newmode pm1 = powerset set(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
newmode pm2 = powerset byte (1:8);
newmode pm3 = powerset int (-32768:32767);
newmode pm4 = powerset long (-32768:32768);
newmode pm5 = powerset long (lower(long):upper(long));
newmode ref1 = ref pm1;
newmode ref2 = ref byte;
newmode ref3 = ptr;
synmode ref4 = ptr;
synmode syn_int = int;

newmode prm1 = proc ();
newmode prm2 = proc (bool in, int out, long inout) returns (char);
newmode prm3 = proc (pm1, ref1 loc) returns (ref3);
newmode prm4 = proc () exceptions(ex1, ex2, ex3);
newmode prm5 = proc (r11, r16 inout, r5 out) returns (r2) exceptions (ex1);

newmode ev1m = event;
newmode ev2m = event (42);

newmode bu1m = buffer ref1;
newmode bu2m = buffer (42) ubyte;

newmode strm1 = char (5);
synmode strm2 = char (7) varying;

synmode bstr1 = bit(20);
--newmode bstr2 = bit(10) varying;

newmode arr1m = array(1:100) set1;
newmode arr2m = array(1:100, 1:100) set1;
newmode arr3m = array(r11, r12, r14) set1;
newmode arr4m = array(r2) array (r3) array (r4, r5) pm1;
newmode arr5m = array(1:10) int;
newmode arr6m = array(1:5, 1:3, 1:2) long;

newmode stru1m = struct (a, b long, 
			 case b of 
			   (42): ch1 chars(20),
			   (52): ch2 chars(10)
			   else  ch3 chars(1)
  			 esac);

newmode stru2m = struct (f set1,
			 case f of
			    (aaa): ch1 char(20),
			    (bbb): ch2 char(10) varying
			 else	ch3 char(0) varying
			 esac);
newmode stru3m = struct (f r3,
			 case f of
			    (na): ch1 char(20)
			 esac);
newmode stru4m = struct (i long,
			 case of
			   : i1, i11 int,
			     b1 bool,
			     c1 char,
			   : i2, i22 long,
			     bs2 bools (10),
			   :
			     s3 struct (i3 int,
					case of
					  : foo long
					  else bar char
					esac)
			   else
                             x stru2m
                         esac,
                         y stru3m);

synmode m_xyzmode = struct (next ref m_xyzmode,
			    i long);

-- set mode locations
dcl s1l set1 := ccc;
dcl s2l nset1 := nb;

-- range mode locations
dcl rl1 r11 := 3;
dcl rl2 r11 := lower(r11);
dcl rl3 r11 := upper(r11);

dcl rl5 r12 := 65530;
dcl rl6 r12 := lower(r12);
dcl rl7 r12 := upper(r12);

--dcl rl9 r13 := 128;
--dcl rl10 r13 := lower(r13);
--dcl rl11 r13 := upper(r13);

dcl rl13 r14 := -121;
dcl rl14 r14 := lower(r14);
dcl rl15 r14 := upper(r14);

dcl rl17 r15 := -32720;
dcl rl18 r15 := lower(r15);
dcl rl19 r15 := upper(r15);

dcl rl21 r16 := 2147483643;
dcl rl22 r16 := lower(r16);
dcl rl23 r16 := upper(r16);

-- powerset mode locations
dcl pl1 pm1 := [p1:p10];
dcl pl2 pm1 := [];
dcl pl3 pm1 := [p1, p10];
dcl pl4 pm1 := [p1:p2, p4:p6, p8:p10];
dcl pl5 pm1 := [p1:p4, p6, p8:p10];
dcl pl6 pm1 := [p1, p3:p8, p10]; 

dcl pl7 pm2 := [1:8];
dcl pl8 pm3 := [-32768:32767];
--dcl pl9 pm5 := [-2147483648:2147483647];

-- reference mode locations
dcl ref3l ref3;
dcl ref4l ref4;
dcl ref5l, ref6l, ref7l, ref8l ptr;
dcl syn_intl1 syn_int := 42;
dcl intl1 int := -42;

-- synchronization mode locations
dcl ev1l ev1m;
dcl ev2l ev2m;
dcl bu1l bu1m;
dcl bu2l bu2m;

-- timing mode locations
dcl til1 time;

-- string mode locations
dcl strl1, strl2 strm2;
dcl bstrl1 bstr1 := B'10101010101010101010';

-- array mode locations
dcl arrl1 arr1m;
dcl arrl2 arr5m := [1, -1, 32767, -32768, 0, 10, 11, 12, 13, 42];
dcl arrl3 arr6m := [(1:5): [(1:3): [(1:2): -2147483648]]];
dcl arrl4 arr6m := [(1:2): [(1:3): [(1:2): -2147483648]],
		      (3): [(1:3): [(1:2): 100]],
		    (4:5): [(1:3): [(1:2): -2147483648]]];
dcl arrl5 array(1:10) nset1;

-- structure mode locations
dcl strul1 stru1m := [-2147483648, 42, "12345678900987654321"];

dummyfunc: proc();
end dummyfunc;

ref3l:=->pl1;		-- newmode ref
ref4l:=->pl1;		-- synmode ref
ref5l:=->pl1;		-- ptr

ref6l:=->syn_intl1;	-- ref to synmode
ref7l:=->intl1;		-- ref to predefined mode
ref8l:=->pl1;		-- ref to newmode

strl1 := "ha" // C'6e' // "s" // "i" // C'00';
strl2 := C'00' // "ope";

__print_event(addr(ev1l), addr("ev1l"));
__print_event(addr(ev2l), addr("ev2l"));
__print_buffer(addr(bu1l), addr("bu1m"));
__print_buffer(addr(bu2l), addr("bu2m"));

til1 := abstime(1970, 3, 12, 10, 43, 0);
writetext(stdout, "lower(pm3) = %C; upper(pm3) = %C%..%/", 
	          lower(pm3), upper(pm3));
writetext(stdout, "lower(pm5) = %C; upper(pm5) = %C%..%/", 
	          lower(pm5), upper(pm5));
--writetext(stdout, "lower(pl9) = %C; upper(pl9) = %C%..%/", 
--	          lower(pl9), upper(pl9));
writetext(stdout, "date = %C%..%/", til1);

writetext(stdout, "slice1 = %C%..%/", strl1(3 : 5));
writetext(stdout, "slice2 = %C%..%/", strl2(0 : 3));
--writetext(stdout, "slice3 = %C%..%/", strl1(0 up 20));
writetext(stdout, "slice4 = %C%..%/", bstrl1(0));
--writetext(stdout, "slice5 = %C%..%/", arrl3(1:5));


writetext(stdout, "done.%/");

dummyfunc();

end tests1;
