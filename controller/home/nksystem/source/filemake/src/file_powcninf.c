/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20230130] 2023.01.30
 * ・ＨＵＡＷＥＩ製パワーコンディショナの下記機種に対応
 *  （SUN2000-40KTL-M、SUN2000-40KTL-NAM、SUN2000-40KTL-NHM）FW.V100R001C
 * [20180815] 2018.08.15 <#123>
 * ・SMA STP 30000TL-US-10対応
 * [20180702] 2018.07.02 <IT_BUG004>
 * ・定格出力が正しく表示されない問題の対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
 * 2013/01/29 明電舎パワーコンディショナ（型式：SP300,SP310）に対応
 *
 */
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"

/* モジュール内グローバル定義 */
struct st_typeName		s_g_powcnType[] = {			/* パワーコンディショナの対応型式 */
/*	---1234567890123456--- */
	/*     +------------------------------------------ 型式名称（ＮＫ内部定義に合致した型式名称） */
	/*     |               +-------------------------- 装置ＩＤの最小値                           */
	/*     |               |    +--------------------- 装置ＩＤの最大値                           */
	{ "PVS005T200"       , 1,  30 },				/* 新電元製 PVS005T200 */
	{ "PVS9R9T200A"      , 1,  30 },				/* 新電元製 PVS9R9T200A */
	{ "PVS9R9T200B"      , 1,  30 },				/* 新電元製 PVS9R9T200B */
	{ "PVS010T200"       , 1,  30 },				/* 新電元製 PVS010T200 */
	{ "PVS010T200A"      , 1,  30 },				/* 新電元製 PVS010T200A */
	{ "PVS010T200B"      , 1,  30 },				/* 新電元製 PVS010T200B */
	{ "PVS010S200"       , 1,  30 },				/* 新電元製 PVS010S200 */
	{ "PVS100T200A"      , 1,  30 },				/* 新電元製 PVS100T200A */
	{ "PVS100T200B"      , 1,  30 },				/* 新電元製 PVS100T200B */
	{ "PVS012T200B"      , 1,  30 },				/* 新電元製 PVS012T200B */
	{ "P100JAT1-A01"     , 1,  20 },				/* ダイヘン製 P100JAT1-A01 */
	{ "P100JDT1-A01"     , 1,  20 },				/* ダイヘン製 P100JDT1-A01 */
	{ "P240JCL1-A01"     , 1,  20 },				/* ダイヘン製 P240JCL1-A01 */
	{ "P250JCL1-A01"     , 1,  20 },				/* ダイヘン製 P250JCL1-A01 */
	{ "P240JDT1-A02"     , 1,  20 },				/* ダイヘン製 P240JDT1-A02 */
	{ "P250JDT1-A02"     , 1,  20 },				/* ダイヘン製 P250JDT1-A02 */
	{ "P240JFL1-A01"     , 1, 247 },				/* ダイヘン製 P240JFL1-A01 */
	{ "P250JFL1-A01"     , 1, 247 },				/* ダイヘン製 P250JFL1-A01 */
	{ "P333JFL1-A01"     , 1, 247 },				/* ダイヘン製 P333JFL1-A01 */
	{ "P490JFL1-A02"     , 1, 247 },				/* ダイヘン製 P490JFL1-A02 */
	{ "P500JFL1-A02"     , 1, 247 },				/* ダイヘン製 P500JFL1-A02 */
	{ "P490JHL2-A01"     , 1, 247 },				/* ダイヘン製 P490JHL2-A01 */
	{ "P500JHL2-A01"     , 1, 247 },				/* ダイヘン製 P500JHL2-A01 */
	{ "P490JHL2-B01"     , 1, 247 },				/* ダイヘン製 P490JHL2-B01 */
	{ "P500JHL2-B01"     , 1, 247 },				/* ダイヘン製 P500JHL2-B01 */
	{ "P490JJL2-B01"     , 1, 247 },				/* ダイヘン製 P490JJL2-B01 */
	{ "P500JJL2-B01"     , 1, 247 },				/* ダイヘン製 P500JJL2-B01 */
	{ "MEW-SP100"        , 1,  31 },				/* 明電舎製 SolarLink SP-100 シリーズ */
	{ "MEW-SP300"        , 1,  31 },				/* 明電舎製 SolarLink SP-300 シリーズ */
	{ "MEW-SP310"        , 1,  31 },				/* 明電舎製 SolarLink SP-310 シリーズ */
	{ "SANYO-PMCOL"      , 1,  27 },				/* 山洋電機製 SANUPS PMC旧プロトコルタイプ */
	{ "SANYO-PMCNW"      , 1,  27 },				/* 山洋電機製 SANUPS PMC新プロトコルピークカット無し */
	{ "SANYO-PMCTD"      , 1,  27 },				/* 山洋電機製 SANUPS PMD-TDプロトコルピークカット有り*/
	{ "SANYO-P73D"       , 1,  27 },				/* 山洋電機製 SANUPS P73Dxxxx */
	{ "SANYO-P73E"       , 1,  27 },				/* 山洋電機製 SANUPS P73Exxxx */
	{ "SANYO-P73F"       , 1,  27 },				/* 山洋電機製 SANUPS P73Fxxxx */
	{ "SANYO-P73H"       , 1,  27 },				/* 山洋電機製 SANUPS P73Hxxxx */
	{ "SANYO-P73J"       , 1,  27 },				/* 山洋電機製 SANUPS P73Jxxxx */
	{ "SANYO-P83A"       , 1,  27 },				/* 山洋電機製 SANUPS P83Axxxx */
	{ "SANYO-P83B"       , 1,  27 },				/* 山洋電機製 SANUPS P83Bxxxx */
	{ "SANYO-P83C"       , 1,  27 },				/* 山洋電機製 SANUPS P83Cxxxx */
	{ "SANYO-P83E"       , 1,  27 },				/* 山洋電機製 SANUPS P83Exxxx */
	{ "GSYUASA-LBSF"     , 1,  20 },				/* GSユアサ製 ラインバック アルファプラス */
	{ "GSYUASA-LBSG"     , 1,  20 },				/* GSユアサ製 ラインバック アルファ III */
	{ "GSYUASA-LBBB"     , 1,  20 },				/* GSユアサ製 ラインバック シグマプラス */
	{ "GSYUASA-LBBF"     , 1,  20 },				/* GSユアサ製 ラインバック シグマ III */
	{ "GSYUASA-LBBC"     , 1,  20 },				/* GSユアサ製 ラインバック オメガ */
	{ "GSYUASA-LBBEA"    , 1,  20 },				/* GSユアサ製 ラインバック ガンマ */
	{ "NISSHIN-SPM100"   , 1,  15 },				/* 日新電機製 SOLARPACK SPM100-CSA1/CSA2 */
	{ "NISSHIN-SPM250"   , 1,  15 },				/* 日新電機製 SOLARPACK SPM250-CS1 */
	{ "NISSHIN-SPM100B"  , 1,  15 },				/* 日新電機製 SOLARPACK SPM100-CS1B */
	{ "NISSHIN-SPM250B"  , 1,  15 },				/* 日新電機製 SOLARPACK SPM250-CS1B */
	{ "NISSHIN-SPM500B"  , 1,  15 },				/* 日新電機製 SOLARPACK SPM500-CS1B */
	{ "NISSHIN-SPM660B"  , 1,  15 },				/* 日新電機製 SOLARPACK SPM660-CS1B */
	{ "NISS2-SPM100"     , 1,  15 },				/* 日新電機製 SOLARPACK SPM100-CSA1/CSA2 */
	{ "NISS2-SPM250"     , 1,  15 },				/* 日新電機製 SOLARPACK SPM250-CS1  */
	{ "NISS2-SPM100B"    , 1,  15 },				/* 日新電機製 SOLARPACK SPM100-CS1B */
	{ "NISS2-SPM250B"    , 1,  15 },				/* 日新電機製 SOLARPACK SPM250-CS1B */
	{ "NISS2-SPM500B"    , 1,  15 },				/* 日新電機製 SOLARPACK SPM500-CS1B */
	{ "NISS2-SPM660B"    , 1,  15 },				/* 日新電機製 SOLARPACK SPM660-CS1B */
	{ "NISS2-SPCS5001"   , 1,  15 },				/* 日新電機製 SOLARPACK SPCS500-1 */
	{ "NISS2-SPCS6601"   , 1,  15 },				/* 日新電機製 SOLARPACK SPCS660-1 */
	{ "OMRON-KP55F-N"    , 0,   5 },				/* OMRON製 KP55F-N */
	{ "OMRON-KP40F"      , 0,   5 },				/* OMRON製 KP40F */
	{ "OMRON-KP40H"      , 0,   5 },				/* OMRON製 KP40H */
	{ "OMRON-KP30K"      , 0,  31 },				/* OMRON製 KP30K */
	{ "OMRON-KP40K"      , 0,  31 },				/* OMRON製 KP40K */
	{ "OMRON-KP55K"      , 0,  31 },				/* OMRON製 KP55K */
	{ "OMRON-KP40K-P"    , 0,  31 },				/* OMRON製 KP40K-P */
	{ "OMRON-KP55K-P"    , 0,  31 },				/* OMRON製 KP55K-P */
	{ "OMRON-KP30K2"     , 0,  31 },				/* OMRON製 KP30K2 */
	{ "OMRON-KP40K2"     , 0,  31 },				/* OMRON製 KP40K2 */
	{ "OMRON-KP55K2"     , 0,  31 },				/* OMRON製 KP55K2 */
	{ "OMRON-KP40K2-P"   , 0,  31 },				/* OMRON製 KP40K2-P */
	{ "OMRON-KP55K2-P"   , 0,  31 },				/* OMRON製 KP55K2-P */
	{ "OMRON-KP44M-J4"   , 0,  31 },				/* OMRON製 KP44M-J4 */
	{ "OMRON-KP55M-J4"   , 0,  31 },				/* OMRON製 KP55M-J4 */
	{ "OMRON-KP44M-PJ4"  , 0,  31 },				/* OMRON製 KP44M-PJ4 */
	{ "OMRON-KP55M-PJ4"  , 0,  31 },				/* OMRON製 KP55M-PJ4 */
	{ "OMRON-KP44M"      , 0,  31 },				/* OMRON製 KP44M */
	{ "OMRON-KP55M"      , 0,  31 },				/* OMRON製 KP55M */
	{ "OMRON-KP44M-P"    , 0,  31 },				/* OMRON製 KP44M-P */
	{ "OMRON-KP55M-P"    , 0,  31 },				/* OMRON製 KP55M-P */
	{ "TMEIC-PVL-L0490"  , 1, 247 },				/* TMEIC製 PVL-L0490 */
	{ "TMEIC-PVL-L0500"  , 1, 247 },				/* TMEIC製 PVL-L0500 */
	{ "TMEIC-PVL-L0500E" , 1, 247 },				/* TMEIC製 PVL-L0500E */
	{ "TMEIC-PVL-L0630E" , 1, 247 },				/* TMEIC製 PVL-L0630E */
	{ "TMEIC-PVL-L0665E" , 1, 247 },				/* TMEIC製 PVL-L0665E */
	{ "TMEIC-PVL-L0750E" , 1, 247 },				/* TMEIC製 PVL-L0750E */
	{ "TMEIC-PVL-L1000E" , 1, 247 },				/* TMEIC製 PVL-L1000E */
	{ "TMEIC-PVL-L0175"  , 1, 247 },				/* TMEIC製 PVL-L0175 */
	{ "TMEIC-PVL-L0250"  , 1, 247 },				/* TMEIC製 PVL-L0250 */
	{ "TMEIC-PVLL0500EJ" , 1, 247 },				/* TMEIC製 PVL-L0500E(J) */
	{ "TMEIC-PVLL0630EJ" , 1, 247 },				/* TMEIC製 PVL-L0630E(J) */
	{ "TMEIC-PVLL1000EJ" , 1, 247 },				/* TMEIC製 PVL-L1000E(J) */
	{ "TMEIC-PVF-L0100"  , 1, 247 },				/* TMEIC製 PVF-L0100 */
	{ "TMEIC-PVF-L0250"  , 1, 247 },				/* TMEIC製 PVF-L0250 */
	{ "TMEIC-PVF-L0500"  , 1, 247 },				/* TMEIC製 PVF-L0500 */
	{ "TMEIC-PVF-L0750"  , 1, 247 },				/* TMEIC製 PVF-L0750 */
	{ "TMEIC-PVF-L1000"  , 1, 247 },				/* TMEIC製 PVF-L1000 */
	{ "TMEIC-PVF-T0100"  , 1, 247 },				/* TMEIC製 PVF-T0100 */
	{ "TMEIC-PVF-T0250"  , 1, 247 },				/* TMEIC製 PVF-T0250 */
	{ "TMEIC-PVF-T0100R" , 1, 247 },				/* TMEIC製 PVF-T0100R */
	{ "TMEIC-PVF-T0100S" , 1, 247 },				/* TMEIC製 PVF-T0100S */
	{ "TMEIC-PVG-L0175"  , 1, 247 },				/* TMEIC製 PVG-L0175 */
	{ "TMEIC-PVG-L0250"  , 1, 247 },				/* TMEIC製 PVG-L0250 */
	{ "SANS-PV-100K-V"   , 1,  31 },				/* 三社電機製 PV-100K-xxT[4]-xx MV/MLV/MHV/HV */
	{ "SANS-PV-250K-V"   , 1,  31 },				/* 三社電機製 PV-250K-xxT[4]-xx MV/MLV/MHV/HV */
	{ "SANS-PV-500K-V"   , 1,  31 },				/* 三社電機製 PV-500K-xxT[4]-xx MV/MLV/MHV/HV */
	{ "HIVERTER-NP203I"  , 1,  31 },				/* 日立製 HIVERTER-NP203i */
	{ "HIVERTER-NP213I"  , 1,  31 },				/* 日立製 HIVERTER-NP213i */
	{ "TA-EPC-A-S30P"    , 1,   5 },				/* 田淵電機製 EPC-A-S30P、EPC-A-S30P-H */
	{ "TA-EPC-A-S55P"    , 1,   5 },				/* 田淵電機製 EPC-A-S55P、EPC-A-S55P-H */
	{ "TA-EPC-B-S80P"    , 1,   5 },				/* 田淵電機製 EPC-B-S80P、EPC-B-S80P-J */
	{ "TA-EPC-B-S99P"    , 1,   5 },				/* 田淵電機製 EPC-B-S99P、EPC-B-S99P-J */
	{ "TA-EPC-C-S18P"    , 1,   5 },				/* 田淵電機製 EPC-C-S18P */
	{ "KACO-XP500"       , 1,  31 },				/* KACO製 XP500-OD-TL */
	{ "YASU-PV1000"      , 1,  31 },				/* 安川電機製製 PV1000シリーズ */
	{ "YASU-P2AA"        , 1,  31 },				/* 安川電機製製 P2シリーズ */
	{ "YASU-P2AAB"       , 1,  31 },				/* 安川電機製製 P2シリーズ(単相) */
	{ "ABBPVS80057-0100" , 1, 247 },				/* APP製 PVS800-57-0100kW-x */
	{ "ABBPVS80057-0250" , 1, 247 },				/* APP製 PVS800-57-0250kW-x */
	{ "ABBPVS80057-0315" , 1, 247 },				/* APP製 PVS800-57-0315kW-x */
	{ "ABBPVS80057-0500" , 1, 247 },				/* APP製 PVS800-57-0500kW-x */
	{ "ABBPVS80057-0630" , 1, 247 },				/* APP製 PVS800-57-0630kW-x */
	{ "ABBPVS80057-0875" , 1, 247 },				/* APP製 PVS800-57-0875kW-x */
	{ "ABBPVS80057-1000" , 1, 247 },				/* APP製 PVS800-57-1000kW-x */
	{ "IDENCON-V2H"      , 1, 247 },				/* アイケイエス製 I_DENCON V2H シリーズ */
	{ "IDENCON-V2X"      , 1, 247 },				/* アイケイエス製 I_DENCON V2X シリーズ */
	{ "IDENCON-HIBIKI"   , 1, 247 },				/* アイケイエス製 I_DENCON V2X シリーズ ひびき灘発電所専用 */
	{ "SCHNEI-XC540"     , 1, 247 },				/* シュナイダー製 XC540 MODBUS(MODBUS-RTU) */
	{ "SCHNEI-XC630"     , 1, 247 },				/* シュナイダー製 XC630 MODBUS(MODBUS-RTU) */
	{ "SCHNEI-XC680"     , 1, 247 },				/* シュナイダー製 XC680 MODBUS(MODBUS-RTU) */
	{ "SCHNEI-TL8000E"   , 1, 247 },				/* シュナイダー製 TL8000E MODBUS(MODBUS-RTU) */
	{ "SCHNEI-TL10000E"  , 1, 247 },				/* シュナイダー製 TL10000E MODBUS(MODBUS-RTU) */
	{ "SCHNEI-TL15000E"  , 1, 247 },				/* シュナイダー製 TL15000E MODBUS(MODBUS-RTU) */
	{ "SCHNEI-TL20000E"  , 1, 247 },				/* シュナイダー製 TL20000E MODBUS(MODBUS-RTU) */
	{ "HSP900-1000"      , 1,  27 },				/* 日立産機システム製 HSP900-1000LFH/LFB/HFH/LFN */
	{ "HSP900-500"       , 1,  27 },				/* 日立産機システム製 HSP900-500LFH/HFH */
	{ "NF-LS168"         , 1, 247 },				/* ＮＦ回路設計ブロック製 LS168P/S/M */
	{ "SMA-SC500CP"      , 1, 247 },				/* SMA製  SUNNY CENTRAL 500CP-JP */
	{ "SMA-SC630CP"      , 1, 247 },				/* SMA製  SUNNY CENTRAL 630CP-JP */
	{ "SMA-SC800CP"      , 1, 247 },				/* SMA製  SUNNY CENTRAL 800CP-JP */
	{ "SMACLCON-10"      , 1,   1 },				/* SMA製  CLCON-10 */ 
	{ "SMACLCON-S-10"    , 1,   1 },				/* SMA製  CLCON-S-10 */ 
	{ "SMASB3500TL"      , 3,  77 },				/* SMA製  SB 3500TL-JP-22 */ 
	{ "SMASB4500TL"      , 3,  77 },				/* SMA製  SB 4500TL-JP-22 */ 
	{ "SMASTP10000TLEE"  , 3,  77 },				/* SMA製  STP 10000TLEE-JP-10 */ 
	{ "SMASTP10000TLEE2" , 3,  77 },				/* SMA製  STP 10000TLEE-JP-11 */ 
	{ "SMASTP20000TLEE"  , 3,  77 },				/* SMA製  STP 20000TLEE-JP-11 */ 
	{ "SMASTP25000TL"    , 3,  77 },				/* SMA製  STP 25000TL-JP-30 */ 
	{ "SMASTP30000TL-US" , 3,  77 },				/* SMA製  STP 30000TL-US-10 [#123] Add */ 
	{ "SMAIM-20"        ,125, 125 },				/* SMA製  IM-20 */
	{ "SMASTP60"        ,126, 167 },				/* SMA製  STP 60-JP-10 */
	{ "SMADM"            , 1,   2 },				/* SMA製  DataManagerM [20190507] Add *//* [20191211] Chg */
	{ "SMADM-STP10TLEE"  , 3,  77 },				/* SMA製  STP 10000TLEE-JP-10 [20190522] Add */ 
	{ "SMADM-STP10TLEE2" , 3,  77 },				/* SMA製  STP 10000TLEE-JP-11 [20190522] Add */ 
	{ "SMADM-STP20TLEE"  , 3,  77 },				/* SMA製  STP 20000TLEE-JP-11 [20190522] Add */ 
	{ "SMADM-STP25TL"    , 3,  77 },				/* SMA製  STP 25000TL-JP-30 [20190522] Add */ 
	{ "SMADM-STP50"      , 3,  77 },				/* SMA製  STP 50-JP [20190507] Add */ 
	{ "DELTA-RPI-M16A"   , 1, 254 },				/* デルタ電子(RPI-M16A)(Modbus-RTU) */
	{ "DELTA-RPI-M20A"   , 1, 254 },				/* デルタ電子(RPI-M20A)(Modbus-RTU) */
	{ "DELTA-RPI-M50A"   , 1, 254 },				/* デルタ電子(RPI-M50A)(Modbus-RTU) */
	{ "DELTA-RPI-M50J"   , 1, 254 },				/* デルタ電子(RPI-M50J)(Modbus-RTU) */
	{ "FUJIPVI7503500"   , 1,  31 },				/* 富士電機(PVI750-3/500)(Modbus-RTU) */
	{ "FUJIPVI10003660"  , 1,  31 },				/* 富士電機(PVI1000-3/660)(Modbus-RTU) */
	{ "FUJIPVI7503750"   , 1,  31 },				/* 富士電機(PVI750-3/750)(Modbus-RTU) */
	{ "FUJIPVI100031000" , 1,  31 },				/* 富士電機(PVI1000-3/1000)(Modbus-RTU) */
	{ "HUA-SML1000"	     , 0,   0 },				/* ＨＵＡＷＥＩ(SmartLogger1000)(Modbus-TCP) */
	{ "HUA-SML2000"	     , 0,   0 },				/* ＨＵＡＷＥＩ(SmartLogger2000)(Modbus-TCP) */
	{ "HUA-SML3000"	     , 0,   0 },				/* ＨＵＡＷＥＩ(SmartLogger3000)(Modbus-TCP) */
	{ "HUA-SUN2K-33KTL"  , 1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-33KTL-JP) */
	{ "HUA-SUN2K-40KTL"  , 1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-40KTL-JP) */
/* [20230130] Add St */
	{ "HUA-SUN2K-40KTLM"  ,1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-40KTL-N3) (FW.V100) */
	{ "HUA-SUN2K-40KTLNAM",1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-40KTL-NAM3) (FW.V100) */
	{ "HUA-SUN2K-40KTLNHM",1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-40KTL-NHM3) (FW.V100) */
/* [20230130] Add Ed */
	{ "HUA-SUN2K-50KTLM" , 1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-50KTL-JP) */
	{ "HUA-SUN2K-63KTLM" , 1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-63KTL-JPM0) */
	{ "HUA-SUN2K-63KTLH" , 1,  80 },				/* ＨＵＡＷＥＩ(SUN2000-63KTL-JPH0) */
	{ "SANK-SML1000"     , 0,   0 },				/* サンケン電気(SmartLogger1000)(Modbus-TCP) */
	{ "SANK-PPS-403FA1"  , 1,  80 },				/* サンケン電気(PPS-403FA1)(Modbus-TCP) */
	{ PV_FILEMAKE_STOPPER, 0,   0 }					/* ストッパー */
};


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * パワーコンディショナ情報生成処理
 *
 * 機能：
 * 		POWCNINF.csv ファイルからパワーコンディショナ情報ファイル（POWCNINF.dat）
 * 		を生成する。
 * 		POWCNINF.csv ファイルが存在しない場合は異常とせず、正常終了する。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	func_POWCNINF_Create( int *lineExist, struct st_linkPoint *ps_linkPoint )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd;							/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	int					stOffset;					/* リンク情報オフセット値 */
	int					svOffset;					/* リンク情報オフセット値 */

	struct st_POWCNINF	*ps_POWCNINF;				/* パワーコンディショナ情報ポインタ */
	char				setFlg1[PV_MAX_POWCN];		/* 重複設定確認用 */
	char				setFlg2[PV_MAX_COMLINE][256];/* 重複設定確認用 */
	int					pcsNumb;					/* パワーコンディショナ番号 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	int					ratedCap = 0;				/* 定格出力 */
	int					panelCap = 0;				/* パネル容量 */

	/* オフセットの取得 */
	stOffset = (PV_MAX_STPOS - 1) / 8 + 1;
	svOffset = (PV_MAX_POWCN_SVPOS - 1) / 8 + 1;

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_POWCNINF );
	strcat(datFilePath, PV_FNAME_POWCNINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePath, ".dat");

	/* CSVファイルが存在する場合 */
	if( access( csvFilePath, R_OK ) == 0 )
	{
		/* DATファイル生成 */
		fd = open( datFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
		if( fd == -1 )
		{
			sprintf( errMsg, "File[%s] Open error", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			return( retCode );
		}
		/* ファイルの長さを調整 */
		if( ftruncate( fd, sizeof(struct st_POWCNINF) * PV_MAX_POWCN ) == -1 )
		{
			sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			close( fd );
			retCode = -1;
			return( retCode );
		}

		/* ファイルをメモリマップ */
		ps_POWCNINF = (struct st_POWCNINF *)mmap( NULL, sizeof(struct st_POWCNINF) * PV_MAX_POWCN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
		if( ps_POWCNINF == MAP_FAILED)
		{
			sprintf( errMsg, "File[%s] MMAP error", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			return( retCode );
		}

		/* 重複設定確認用情報を0クリア */
		memset( setFlg1, 0, sizeof(setFlg1) );
		memset( setFlg2, 0, sizeof(setFlg2) );

		/* ファイルの内容を0クリア */
		memset( ps_POWCNINF, 0, sizeof(struct st_POWCNINF) * PV_MAX_POWCN );

		/* CSVファイルオープン */
		if( (fp = fopen(csvFilePath, "r")) != NULL)
		{
			/* CSVファイルの終端まで繰り返す */
			while( fgets( readBuff, 4096, fp) != NULL )
			{
				/* CSV処理カウンタ＋１ */
				syoriCount++;
				/* コメント行はスキップ */
				if( readBuff[0] == '#' ) continue;
				/* 未入力行（9文字以下）はスキップ */
				if( strlen( readBuff ) < 10) continue;
				/* CSVデータの分割 */
				csvCount = func_sep_data( readBuff, &s_sepStr );

				/* CSVデータ数が不正の場合 */
				if( csvCount != 8)
				{
					sprintf( errMsg, "CSV-Format Error (Data is not 7). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* パワーコンディショナ番号の取得 */
				pcsNumb = atoi( s_sepStr.data[0] );
				if( (pcsNumb < 1) || (pcsNumb > PV_MAX_POWCN) )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 実装/未実装の取得 */
				workData = atoi( s_sepStr.data[1] );
				/* 未実装の場合 */
				if( workData == 0 ) continue;
				/* 実装以外の場合 */
				if( workData != 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] LINE[%d] DAT[2]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_POWCNINF + pcsNumb - 1)->statFlag = workData;

				/* 重複設定の確認 */
				if( setFlg1[pcsNumb - 1] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number Duplication). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg1[pcsNumb - 1] = 1;

				/* 所属回線番号の取得 */
				workData = atoi( s_sepStr.data[2] );
				if( (workData < 1) || (workData > PV_MAX_COMLINE) )
				{
					sprintf( errMsg, "CSV-Format Error (Line Number). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* ４８５回線情報が未実装の場合 */
				if( *(lineExist + workData - 1) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Line Not Implimented). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_POWCNINF + pcsNumb - 1)->lineNumb = workData;
				
				/* 型式名称の取得 */
				if( strlen(s_sepStr.data[4]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 型式名称が一致するかテーブルのストッパーに到達するまで繰り返す */
				for( loopCnt = 0 ; strncmp( s_g_powcnType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 32 ) != 0 ; loopCnt++ )
				{
					/* 型式名称が一致した場合 */
					if( strcmp( s_g_powcnType[loopCnt].typeName, s_sepStr.data[4] ) == 0 )
						break;
				}

				/* 型式名称が見つからなかった場合 */
				if( strncmp( s_g_powcnType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 32 ) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (not found Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_POWCNINF + pcsNumb - 1)->typeName, s_sepStr.data[4], 32);
				/* 装置ＩＤ番号の取得 */
				workData = atoi( s_sepStr.data[3] );
				if( (workData < s_g_powcnType[loopCnt].minID) || (workData > s_g_powcnType[loopCnt].maxID ) )
				{
					sprintf( errMsg, "CSV-Format Error (ID Number). File[%s] LINE[%d] DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_POWCNINF + pcsNumb - 1)->idNumb = workData;

				/* 重複設定の確認 */
				if( setFlg2[(ps_POWCNINF + pcsNumb - 1)->lineNumb - 1][workData] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Line and ID Number Duplication). File[%s] LINE[%d] DAT[3] or DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg2[(ps_POWCNINF + pcsNumb - 1)->lineNumb - 1][workData] = 1;

				/* 名称の取得 */
				if( strlen(s_sepStr.data[5]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] LINE[%d] DAT[6]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				strncpy( (ps_POWCNINF + pcsNumb - 1)->powcnName, s_sepStr.data[5], 72);
				(ps_POWCNINF + pcsNumb - 1)->stStLink = ps_linkPoint->stStLink + (pcsNumb - 1) * stOffset;
				(ps_POWCNINF + pcsNumb - 1)->svStLink = ps_linkPoint->svStLink + (pcsNumb - 1) * svOffset;
				(ps_POWCNINF + pcsNumb - 1)->svIfLink = ps_linkPoint->svIfLink + (pcsNumb - 1) * PV_MAX_POWCN_SVPOS;
				(ps_POWCNINF + pcsNumb - 1)->tmIfLink = ps_linkPoint->tmIfLink + (pcsNumb - 1) * PV_MAX_POWCN_TMPOS;

				/* 定格容量を取得 */
				ratedCap = atoi( s_sepStr.data[6] );
				if( ratedCap < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (rating capacity < 0). File[%s] LINE[%d] DAT[7]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_POWCNINF + pcsNumb - 1)->ratedCap = ratedCap;

				/* パネル容量を取得 */
				panelCap = atoi( s_sepStr.data[7] );
				if( panelCap < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (panelCap < 0). File[%s] LINE[%d] DAT[8]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_POWCNINF + pcsNumb - 1)->panelCap = panelCap;
				

				/* 戻り値のパワーコンディショナ数を＋１設定 */
				retCode++;

				/* 実装情報を設定 */
				commfunc_bitset( ps_linkPoint->statFlag, pcsNumb - 1, 1 );
			}
			/* CSVファイルをクローズ */
			fclose( fp );
		}
		else
		{
			sprintf( errMsg, "File[%s] Open error", csvFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
		}

		/* メモリマップを解除 */
		munmap( ps_POWCNINF, sizeof(struct st_POWCNINF) * PV_MAX_POWCN);

		/* DATファイルをクローズ */
		close( fd );
	}
	else
	{
		/* DATファイルが存在する場合 */
		if( access( datFilePath, R_OK ) == 0 )
		{
			unlink( datFilePath );
		}
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}
	if( retCode > 0 )
	{
		ps_linkPoint->stStLink += PV_MAX_POWCN * stOffset;
		ps_linkPoint->svStLink += PV_MAX_POWCN * svOffset;
		ps_linkPoint->svIfLink += PV_MAX_POWCN * PV_MAX_POWCN_SVPOS;
		ps_linkPoint->tmIfLink += PV_MAX_POWCN * PV_MAX_POWCN_TMPOS;
	}
	/* 終了 */
	return( retCode );	
}
