testvars: MODULE

DCL bool_true BOOL INIT := TRUE;
DCL bool_false BOOL INIT := FALSE;
DCL booltable1 ARRAY (0:3) BOOL INIT := [ TRUE, FALSE, FALSE, TRUE ];
DCL booltable2 ARRAY (4:7) BOOL INIT := [ TRUE, FALSE, FALSE, TRUE ];

DCL control_char CHAR INIT := C'07';
DCL printable_char CHAR INIT := 'a';
DCL chartable1 ARRAY (0:2) CHAR INIT := [ C'00', C'01', C'02' ];
DCL chartable2 ARRAY (3:5) CHAR INIT := [ C'00', C'01', C'02' ];

DCL string1 CHARS (4) INIT := 'abcd';
DCL string2 CHARS (5) INIT := 'ef' // C'00' // 'gh';
DCL string3 CHARS (6) INIT := 'ef' // 'gh' // 'ij';
DCL string4 CHARS (7) INIT := (6) 'z' // C'00';

DCL byte_low BYTE INIT := -128;
DCL byte_high BYTE INIT := 127;
DCL bytetable1 ARRAY (0:4) BYTE INIT := [ -2, -1, 0, 1, 2 ];
DCL bytetable2 ARRAY (5:9) BYTE INIT := [ -2, -1, 0, 1, 2 ];
DCL bytetable3 ARRAY (1:2,'c':'d',FALSE:TRUE) BYTE
	INIT := [ [ [ 0, 1 ], [ 2, 3 ] ], [ [ 4, 5 ], [ 6, 7 ] ] ];
DCL bytetable4 ARRAY (1:2) ARRAY ('c':'d') ARRAY (FALSE:TRUE) BYTE
	INIT := [ [ [ 0, 1 ], [ 2, 3 ] ], [ [ 4, 5 ], [ 6, 7 ] ] ];

DCL ubyte_low UBYTE INIT := 0;
DCL ubyte_high UBYTE INIT := 255;
DCL ubytetable1 ARRAY (0:4) UBYTE INIT := [ 0, 1, 2, 3, 4 ];
DCL ubytetable2 ARRAY (5:9) UBYTE INIT := [ 0, 1, 2, 3, 4 ];

DCL int_low INT INIT := -32_768;
DCL int_high INT INIT := 32_767;
DCL inttable1 ARRAY (0:4) INT INIT := [ -2, -1, 0, 1, 2 ];
DCL inttable2 ARRAY (5:9) INT INIT := [ -2, -1, 0, 1, 2 ];

DCL uint_low UINT INIT := 0;
DCL uint_high UINT INIT := 65_535;
DCL uinttable1 ARRAY (0:4) UINT INIT := [ 0, 1, 2, 3, 4 ];
DCL uinttable2 ARRAY (5:9) UINT INIT := [ 0, 1, 2, 3, 4 ];

DCL long_low LONG INIT := -2_147_483_648;
DCL long_high LONG INIT := 2_147_483_647;
DCL longtable1 ARRAY (0:4) LONG INIT := [ -2, -1, 0, 1, 2 ];
DCL longtable2 ARRAY (5:9) LONG INIT := [ -2, -1, 0, 1, 2 ];

DCL ulong_low ULONG INIT := 0;
DCL ulong_high ULONG INIT := 4_294_967_295;
DCL ulongtable1 ARRAY (0:4) ULONG INIT := [ 0, 1, 2, 3, 4 ];
DCL ulongtable2 ARRAY (5:9) ULONG INIT := [ 0, 1, 2, 3, 4 ];

DCL real1 FLOAT INIT := 3.14159265358;
DCL real2 FLOAT INIT := -3.14159265358;
DCL realtable1 ARRAY (0:4) FLOAT INIT := [ -2.0, -1.0, 0.0, 1.0, 2.0 ];
DCL realtable2 ARRAY (5:9) FLOAT INIT := [ -2.0, -1.0, 0.0, 1.0, 2.0 ];

DCL long_real1 DOUBLE INIT := 3.14e300;
DCL long_real2 DOUBLE INIT := -3.14e-300;
DCL longrealtable1 ARRAY (0:4) DOUBLE INIT := [ -2.0, -1.0, 0.0, 1.0, 2.0 ];
DCL longrealtable2 ARRAY (5:9) DOUBLE INIT := [ -2.0, -1.0, 0.0, 1.0, 2.0 ];

/* DCL powerset1 POWERSET INT(0:7);*/
/* DCL chars1 CHAR (16) INIT := (16)'b'; */
/* DCL bits1 BIT(20) := B'11111111000010101011'; */

NEWMODE simple_struct = STRUCT (abool BOOL, aint INT, astring CHARS (8));
DCL struct1 simple_struct := [ TRUE, 123, "a string" ];

NEWMODE nested_struct = STRUCT (abool BOOL, nstruct simple_struct, aint INT);
DCL struct2 nested_struct := [ TRUE, [ FALSE, 456, "deadbeef" ], 789 ];

/* This table is used as a source for every ascii character. */

DCL asciitable ARRAY (0:255) CHAR INIT := [
    C'00', C'01', C'02', C'03', C'04', C'05', C'06', C'07',
    C'08', C'09', C'0a', C'0b', C'0c', C'0d', C'0e', C'0f',
    C'10', C'11', C'12', C'13', C'14', C'15', C'16', C'17',
    C'18', C'19', C'1a', C'1b', C'1c', C'1d', C'1e', C'1f',
    C'20', C'21', C'22', C'23', C'24', C'25', C'26', C'27',
    C'28', C'29', C'2a', C'2b', C'2c', C'2d', C'2e', C'2f',
    C'30', C'31', C'32', C'33', C'34', C'35', C'36', C'37',
    C'38', C'39', C'3a', C'3b', C'3c', C'3d', C'3e', C'3f',
    C'40', C'41', C'42', C'43', C'44', C'45', C'46', C'47',
    C'48', C'49', C'4a', C'4b', C'4c', C'4d', C'4e', C'4f',
    C'50', C'51', C'52', C'53', C'54', C'55', C'56', C'57',
    C'58', C'59', C'5a', C'5b', C'5c', C'5d', C'5e', C'5f',
    C'60', C'61', C'62', C'63', C'64', C'65', C'66', C'67',
    C'68', C'69', C'6a', C'6b', C'6c', C'6d', C'6e', C'6f',
    C'70', C'71', C'72', C'73', C'74', C'75', C'76', C'77',
    C'78', C'79', C'7a', C'7b', C'7c', C'7d', C'7e', C'7f',
    C'80', C'81', C'82', C'83', C'84', C'85', C'86', C'87',
    C'88', C'89', C'8a', C'8b', C'8c', C'8d', C'8e', C'8f',
    C'90', C'91', C'92', C'93', C'94', C'95', C'96', C'97',
    C'98', C'99', C'9a', C'9b', C'9c', C'9d', C'9e', C'9f',
    C'a0', C'a1', C'a2', C'a3', C'a4', C'a5', C'a6', C'a7',
    C'a8', C'a9', C'aa', C'ab', C'ac', C'ad', C'ae', C'af',
    C'b0', C'b1', C'b2', C'b3', C'b4', C'b5', C'b6', C'b7',
    C'b8', C'b9', C'ba', C'bb', C'bc', C'bd', C'be', C'bf',
    C'c0', C'c1', C'c2', C'c3', C'c4', C'c5', C'c6', C'c7',
    C'c8', C'c9', C'ca', C'cb', C'cc', C'cd', C'ce', C'cf',
    C'd0', C'd1', C'd2', C'd3', C'd4', C'd5', C'd6', C'd7',
    C'd8', C'd9', C'da', C'db', C'dc', C'dd', C'de', C'df',
    C'e0', C'e1', C'e2', C'e3', C'e4', C'e5', C'e6', C'e7',
    C'e8', C'e9', C'ea', C'eb', C'ec', C'ed', C'ee', C'ef',
    C'f0', C'f1', C'f2', C'f3', C'f4', C'f5', C'f6', C'f7',
    C'f8', C'f9', C'fa', C'fb', C'fc', C'fd', C'fe', C'ff'
];

DCL charmatrix ARRAY (0:255) CHAR INIT := [
  'a','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X',
  'a','a','X','X','X','X','X','X','X','X','X','X','X','X','X','X',
  'a','a','a','X','X','X','X','X','X','X','X','X','X','X','X','X',
  'a','a','a','a','X','X','X','X','X','X','X','X','X','X','X','X',
  'a','a','a','a','a','X','X','X','X','X','X','X','X','X','X','X',
  'a','a','a','a','a','a','X','X','X','X','X','X','X','X','X','X',
  'a','a','a','a','a','a','a','X','X','X','X','X','X','X','X','X',
  'a','a','a','a','a','a','a','a','X','X','X','X','X','X','X','X',
  'a','a','a','a','a','a','a','a','a','X','X','X','X','X','X','X',
  'a','a','a','a','a','a','a','a','a','a','X','X','X','X','X','X',
  'a','a','a','a','a','a','a','a','a','a','a','X','X','X','X','X',
  'a','a','a','a','a','a','a','a','a','a','a','a','X','X','X','X',
  'a','a','a','a','a','a','a','a','a','a','a','a','a','X','X','X',
  'a','a','a','a','a','a','a','a','a','a','a','a','a','a','X','X',
  'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','X',
  'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a'
];

DCL xptr PTR INIT := ->int_high;

booleans: PROC ();

	DCL val1 BOOL := TRUE;
	DCL val2 BOOL := FALSE;
	DCL val3 BOOL := TRUE;

	val1 := TRUE XOR TRUE;
	val1 := TRUE XOR FALSE;
	val1 := FALSE XOR TRUE;
	val1 := FALSE XOR FALSE;
	val1 := val2 XOR val3;

	val1 := TRUE AND TRUE;
	val1 := TRUE AND FALSE;
	val1 := FALSE AND TRUE;
	val1 := FALSE AND FALSE;
	val1 := val2 AND val3;

	val1 := TRUE ANDIF TRUE;
	val1 := TRUE ANDIF FALSE;
	val1 := FALSE ANDIF TRUE;
	val1 := FALSE ANDIF FALSE;
	val1 := val2 ANDIF val3;

	val1 := TRUE OR TRUE;
	val1 := TRUE OR FALSE;
	val1 := FALSE OR TRUE;
	val1 := FALSE OR FALSE;
	val1 := val2 OR val3;

--	val1 := NOT TRUE;
--	val1 := NOT FALSE;
--	val1 := NOT val2;
--	val1 := NOT val3;
	
END booleans;

scalar_arithmetic: PROC ();

	DCL val1 INT := 1;
	DCL val2 INT := 2;
	DCL val3 INT := 3;

	val1 := -val2;
	val1 := val2 + val3;
	val1 := val2 - val3;
	val1 := val2 * val3;
	val1 := val2 / val3;
	val1 := val2 MOD val3;
	val1 := val2 REM val3;
	
END scalar_arithmetic;

write_arrays: PROC ();

	inttable1(0) := 0;
	inttable1(1) := 1;
	inttable1(2) := 2;
	inttable1(3) := 3;
	inttable1(4) := 4;
	inttable2(5) := 5;
	inttable2(6) := 6;
	inttable2(7) := 7;
	inttable2(8) := 8;
	inttable2(9) := 9;

END write_arrays;

uint_low := 0;

scalar_arithmetic ();
write_arrays ();
booleans ();

END;
