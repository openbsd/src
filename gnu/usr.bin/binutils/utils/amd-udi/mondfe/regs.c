static char _[] = "@(#)regs.c	5.20 93/07/30 16:38:58, Srini, AMD.";
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 **       This file contains arrays of ASCII strings which represent
 **       the register names for the the Am29000 processor.
 *****************************************************************************
 */

char *reg[] = {
    "gr0",      "gr1",      "gr2",      "gr3",
    "gr4",      "gr5",      "gr6",      "gr7",
    "gr8",      "gr9",      "gr10",     "gr11",
    "gr12",     "gr13",     "gr14",     "gr15",
    "gr16",     "gr17",     "gr18",     "gr19",
    "gr20",     "gr21",     "gr22",     "gr23",
    "gr24",     "gr25",     "gr26",     "gr27",
    "gr28",     "gr29",     "gr30",     "gr31",
    "gr32",     "gr33",     "gr34",     "gr35",
    "gr36",     "gr37",     "gr38",     "gr39",
    "gr40",     "gr41",     "gr42",     "gr43",
    "gr44",     "gr45",     "gr46",     "gr47",
    "gr48",     "gr49",     "gr50",     "gr51",
    "gr52",     "gr53",     "gr54",     "gr55",
    "gr56",     "gr57",     "gr58",     "gr59",
    "gr60",     "gr61",     "gr62",     "gr63",
    "gr64",     "gr65",     "gr66",     "gr67",
    "gr68",     "gr69",     "gr70",     "gr71",
    "gr72",     "gr73",     "gr74",     "gr75",
    "gr76",     "gr77",     "gr78",     "gr79",
    "gr80",     "gr81",     "gr82",     "gr83",
    "gr84",     "gr85",     "gr86",     "gr87",
    "gr88",     "gr89",     "gr90",     "gr91",
    "gr92",     "gr93",     "gr94",     "gr95",
    "gr96",     "gr97",     "gr98",     "gr99",
    "gr100",    "gr101",    "gr102",    "gr103",
    "gr104",    "gr105",    "gr106",    "gr107",
    "gr108",    "gr109",    "gr110",    "gr111",
    "gr112",    "gr113",    "gr114",    "gr115",
    "gr116",    "gr117",    "gr118",    "gr119",
    "gr120",    "gr121",    "gr122",    "gr123",
    "gr124",    "gr125",    "gr126",    "gr127",
    "lr0",      "lr1",      "lr2",      "lr3",
    "lr4",      "lr5",      "lr6",      "lr7",
    "lr8",      "lr9",      "lr10",     "lr11",
    "lr12",     "lr13",     "lr14",     "lr15",
    "lr16",     "lr17",     "lr18",     "lr19",
    "lr20",     "lr21",     "lr22",     "lr23",
    "lr24",     "lr25",     "lr26",     "lr27",
    "lr28",     "lr29",     "lr30",     "lr31",
    "lr32",     "lr33",     "lr34",     "lr35",
    "lr36",     "lr37",     "lr38",     "lr39",
    "lr40",     "lr41",     "lr42",     "lr43",
    "lr44",     "lr45",     "lr46",     "lr47",
    "lr48",     "lr49",     "lr50",     "lr51",
    "lr52",     "lr53",     "lr54",     "lr55",
    "lr56",     "lr57",     "lr58",     "lr59",
    "lr60",     "lr61",     "lr62",     "lr63",
    "lr64",     "lr65",     "lr66",     "lr67",
    "lr68",     "lr69",     "lr70",     "lr71",
    "lr72",     "lr73",     "lr74",     "lr75",
    "lr76",     "lr77",     "lr78",     "lr79",
    "lr80",     "lr81",     "lr82",     "lr83",
    "lr84",     "lr85",     "lr86",     "lr87",
    "lr88",     "lr89",     "lr90",     "lr91",
    "lr92",     "lr93",     "lr94",     "lr95",
    "lr96",     "lr97",     "lr98",     "lr99",
    "lr100",    "lr101",    "lr102",    "lr103",
    "lr104",    "lr105",    "lr106",    "lr107",
    "lr108",    "lr109",    "lr110",    "lr111",
    "lr112",    "lr113",    "lr114",    "lr115",
    "lr116",    "lr117",    "lr118",    "lr119",
    "lr120",    "lr121",    "lr122",    "lr123",
    "lr124",    "lr125",    "lr126",    "lr127"
    };

char *spreg[] = {
    "vab",      "ops",      "cps",      "cfg",
    "cha",      "chd",      "chc",      "rbp",
    "tmc",      "tmr",      "pc0",      "pc1",
    "pc2",      "mmu",      "lru",      "rsn",
    "rma0",     "rmc0",     "rma1",     "rmc1",
    "spc0",     "spc1",     "spc2",     "iba0",
    "ibc0",     "iba1",     "ibc1",     "sr27",
    "sr28",     "cir",      "cdr",      "sr31",
    "sr32",     "sr33",     "sr34",     "sr35",
    "sr36",     "sr37",     "sr38",     "sr39",
    "sr40",     "sr41",     "sr42",     "sr43",
    "sr44",     "sr45",     "sr46",     "sr47",
    "sr48",     "sr49",     "sr50",     "sr51",
    "sr52",     "sr53",     "sr54",     "sr55",
    "sr56",     "sr57",     "sr58",     "sr59",
    "sr60",     "sr61",     "sr62",     "sr63",
    "sr64",     "sr65",     "sr66",     "sr67",
    "sr68",     "sr69",     "sr70",     "sr71",
    "sr72",     "sr73",     "sr74",     "sr75",
    "sr76",     "sr77",     "sr78",     "sr79",
    "sr80",     "sr81",     "sr82",     "sr83",
    "sr84",     "sr85",     "sr86",     "sr87",
    "sr88",     "sr89",     "sr90",     "sr91",
    "sr92",     "sr93",     "sr94",     "sr95",
    "sr96",     "sr97",     "sr98",     "sr99",
    "sr100",    "sr101",    "sr102",    "sr103",
    "sr104",    "sr105",    "sr106",    "sr107",
    "sr108",    "sr109",    "sr110",    "sr111",
    "sr112",    "sr113",    "sr114",    "sr115",
    "sr116",    "sr117",    "sr118",    "sr119",
    "sr120",    "sr121",    "sr122",    "sr123",
    "sr124",    "sr125",    "sr126",    "sr127",
    "ipc",      "ipa",      "ipb",      "q",
    "alu",      "bp",       "fc",       "cr",
    "sr136",    "sr137",    "sr138",    "sr139",
    "sr140",    "sr141",    "sr142",    "sr143",
    "sr144",    "sr145",    "sr146",    "sr147",
    "sr148",    "sr149",    "sr150",    "sr151",
    "sr152",    "sr153",    "sr154",    "sr155",
    "sr156",    "sr157",    "sr158",    "sr159",
    "fpe",      "inte",     "fps",      "sr163",
    "exop",     "sr165",    "sr166",    "sr167",
    "sr168",    "sr169",    "sr170",    "sr171",
    "sr172",    "sr173",    "sr174",    "sr175",
    "sr176",    "sr177",    "sr178",    "sr179",
    "sr180",    "sr181",    "sr182",    "sr183",
    "sr184",    "sr185",    "sr186",    "sr187",
    "sr188",    "sr189",    "sr190",    "sr191",
    "sr192",    "sr193",    "sr194",    "sr195",
    "sr196",    "sr197",    "sr198",    "sr199",
    "sr200",    "sr201",    "sr202",    "sr203",
    "sr204",    "sr205",    "sr206",    "sr207",
    "sr208",    "sr209",    "sr210",    "sr211",
    "sr212",    "sr213",    "sr214",    "sr215",
    "sr216",    "sr217",    "sr218",    "sr219",
    "sr220",    "sr221",    "sr222",    "sr223",
    "sr224",    "sr225",    "sr226",    "sr227",
    "sr228",    "sr229",    "sr230",    "sr231",
    "sr232",    "sr233",    "sr234",    "sr235",
    "sr236",    "sr237",    "sr238",    "sr239",
    "sr240",    "sr241",    "sr242",    "sr243",
    "sr244",    "sr245",    "sr246",    "sr247",
    "sr248",    "sr249",    "sr250",    "sr251",
    "sr252",    "sr253",    "sr254",    "sr255"
   };

