-- Copyright (C) 1992 Free Software Foundation, Inc.

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
-- test program 2 (refer to tests2.exp)
--

tests2: module;

-- testpattern
syn pat1 ulong = H'aaaaaaaa;
syn pat2 ulong = H'55555555;

-- discrete modes
newmode bytem = struct ( 
	p1 ulong,
	m byte,
	p2 ulong);
newmode ubytem = struct ( 
	p1 ulong,
	m ubyte,
	p2 ulong);
newmode intm = struct ( 
	p1 ulong,
	m int,
	p2 ulong);
newmode uintm = struct ( 
	p1 ulong,
	m uint,
	p2 ulong);
newmode longm = struct ( 
	p1 ulong,
	m long,
	p2 ulong);
newmode ulongm = struct ( 
	p1 ulong,
	m ulong,
	p2 ulong);
newmode boolm = struct ( 
	p1 ulong,
	m bool,
	p2 ulong);
newmode charm1 = struct ( 
	p1 ulong,
	m char(4),
	p2 ulong);
newmode charm2 = struct ( 
	p1 ulong,
	m char(7),
	p2 ulong);
newmode charm3 = struct ( 
	p1 ulong,
	m char(8) varying,
	p2 ulong);
newmode charm4 = struct ( 
	p1 ulong,
	m char,
	p2 ulong);
newmode bitm1 = struct ( 
	p1 ulong,
	m bit(8),
	p2 ulong);
newmode bitm2 = struct ( 
	p1 ulong,
	m bit(10),
	p2 ulong);
newmode setm1 = struct ( 
	p1 ulong,
	m set (a, b, c, d, e, f, g, h),
	p2 ulong);
newmode nset1 = struct ( 
	p1 ulong,
	m set (na = 2147483648, nb = 1024, nc = 4294967295),
	p2 ulong);
newmode rm1 = struct ( 
	p1 ulong,
	m range (lower(byte):upper(byte)),
	p2 ulong);
newmode rm2 = struct ( 
	p1 ulong,
	m range (lower(int):upper(int)),
	p2 ulong);
newmode rm3 = struct ( 
	p1 ulong,
	m range (lower(long):upper(long)),
	p2 ulong);
newmode pm1 = struct ( 
	p1 ulong,
	m powerset set (pa, pb, pc, pd, pe, pf, pg, ph),
	p2 ulong);
newmode pm2 = struct ( 
	p1 ulong,
	m powerset int (1:32),
	p2 ulong);
-- this should be rejected by the gnuchill compiler ! 
newmode pm3 = struct ( 
	p1 ulong,
--	m powerset long (lower(long): upper(long)),
	p2 ulong);
newmode refm1 = struct ( 
	p1 ulong,
	m ptr,
	p2 ulong);
newmode refm2 = struct ( 
	p1 ulong,
	m ref bytem,
	p2 ulong);
newmode prm1 = struct ( 
	p1 ulong,
	m proc (),
	p2 ulong);
newmode tim1 = struct ( 
	p1 ulong,
	m time,
	p2 ulong);
newmode tim2 = struct ( 
	p1 ulong,
	m duration,
	p2 ulong);
newmode rem1 = struct ( 
	p1 ulong,
	m real,
	p2 ulong);
newmode rem2 = struct ( 
	p1 ulong,
	m long_real,
	p2 ulong);
newmode arrm1 = struct ( 
	p1 ulong,
	m array(1:3, 1:2) int,
	p2 ulong);
newmode strum1 = struct ( 
	p1 ulong,
	m struct (a, b int, ch char(4)),
	p2 ulong);


-- dummyfunction for breakpoints
dummyfunc: proc();
end dummyfunc;


dcl b1 bytem init := [pat1, -128, pat2];
dcl ub1 ubytem init := [pat1, 0, pat2];
dcl i1 intm init := [pat1, -32768, pat2];
dcl ui1 uintm init := [pat1, 0, pat2];
dcl l1 longm init := [pat1, -2147483648, pat2];
dcl ul1 ulongm init := [pat1, 0, pat2];
dcl bo1 boolm init := [pat1, true, pat2];
dcl c1 charm1 init := [pat1, "1234", pat2];
dcl c2 charm2 init := [pat1, "1234567", pat2];
dcl c3 charm3 init := [pat1, "12345678", pat2];
dcl c4 charm4 init := [pat1, C'00', pat2];
dcl bi1 bitm1 init := [pat1, B'01011010', pat2];
dcl bi2 bitm2 init := [pat1, B'1010110101', pat2];
dcl se1 setm1 init := [pat1, a, pat2];
dcl nse1 nset1 init := [pat1, na, pat2];
dcl r1 rm1 init := [pat1, -128, pat2];
dcl r2 rm2 init := [pat1, -32768, pat2];
dcl r3 rm3 init := [pat1, -2147483648, pat2];
dcl p1 pm1 init := [pat1, [pa], pat2];
dcl p2 pm2 init := [pat1, [1], pat2];
-- dcl p3 pm3 init := [pat1, [-1], pat2]; -- FIXME: bug in gnuchill
dcl ref1 refm1 init := [pat1, null, pat2];
dcl ref2 refm2 init := [pat1, null, pat2];
dcl pr1 prm1; 
dcl ti1 tim1 init := [pat1, 0, pat2];
dcl ti2 tim2 init := [pat1, 0, pat2];
dcl re1 rem1 init := [pat1, 0.0, pat2];
dcl re2 rem2 init := [pat1, 0.0, pat2];
dcl arrl1 arrm1 init:=[pat1, [(1:3): [0,0]], pat2];
dcl strul1 strum1 init := [pat1, [.a: 0, .b: 0, .ch: "0000"], pat2];

pr1 := [pat1, dummyfunc, pat2];
dummyfunc();

end tests2;
