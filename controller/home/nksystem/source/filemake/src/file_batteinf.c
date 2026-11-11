/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20250421] 2025.04.21
 * ・Tesla社製蓄電池 Controls-V2に対応
 * 
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
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
struct st_typeName		s_g_batteType[] = {			/* バッテリーの対応型式 */
/*	---1234567890123456--- */
	/*     +------------------------------------------ 型式名称（ＮＫ内部定義に合致した型式名称） */
	/*     |               +-------------------------- 装置ＩＤの最小値                           */
	/*     |               |    +--------------------- 装置ＩＤの最大値                           */
	{ "TESLA-POWERPK20"  , 1, 254 },				/* TESLA製 PowerPack 2.0 (Modbus-TCP) */
	{ "TESLA-POWERPK20+SEL",1, 254 },				/* TESLA製 PowerPack 2.0+SEL (Modbus-TCP) *//* [20230529] Add */
	{ "NEC-AEROS"        , 1, 254 },				/* NEC製 AEROS (Modbus-TCP) */
	{ "SANSHA-BP250K"    , 1, 30  },				/* 三社電機製 蓄電池 */
	{ "TESLA-CONTROLS-V2", 1, 254 },				/* TESLA製 Controls-V2 (Modbus-TCP) *//* [20250421] Add */
	{ "PPM-10",            1, 254 },				/* SMA製PPM (Modbus-TCP) *//* [20251103] Add */
	{ PV_FILEMAKE_STOPPER, 0,   0 }					/* ストッパー */
};

/* 内部関数定義 */
int	func_BATTEINF_Create( int *, struct st_linkPoint * );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * バッテリー情報生成処理
 *
 * 機能：
 * 		BATTEINF.csv ファイルからバッテリー情報ファイル（BATTEINF.dat）
 * 		を生成する。
 * 		BATTEINF.csv ファイルが存在しない場合は異常とせず、正常終了する。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	func_BATTEINF_Create( int *lineExist, struct st_linkPoint *ps_linkPoint )
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

	struct st_BATTEINF	*ps_BATTEINF;				/* バッテリー情報ポインタ */
	char				setFlg1[PV_MAX_BATTE];		/* 重複設定確認用 */
	char				setFlg2[PV_MAX_COMLINE][256];/* 重複設定確認用 */
	int					bttNumb;					/* バッテリー番号 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* オフセットの取得 */
	stOffset = (PV_MAX_STPOS - 1) / 8 + 1;
	svOffset = (PV_MAX_BATTE_SVPOS - 1) / 8 + 1;

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_BATTEINF );
	strcat(datFilePath, PV_FNAME_BATTEINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePath, ".dat");

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
	if( ftruncate( fd, sizeof(struct st_BATTEINF) * PV_MAX_BATTE ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_BATTEINF = (struct st_BATTEINF *)mmap( NULL, sizeof(struct st_BATTEINF) * PV_MAX_BATTE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_BATTEINF == MAP_FAILED)
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
	memset( ps_BATTEINF, 0, sizeof(struct st_BATTEINF) * PV_MAX_BATTE );

	/* CSVファイルが存在する場合 */
	if( access( csvFilePath, R_OK ) == 0 )
	{
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
				if( csvCount != 13)
				{
					sprintf( errMsg, "CSV-Format Error (Data is not 13). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* バッテリー番号の取得 */
				bttNumb = atoi( s_sepStr.data[0] );
				if( (bttNumb < 1) || (bttNumb > PV_MAX_BATTE) )
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
				(ps_BATTEINF + bttNumb - 1)->statFlag = workData;

				/* 重複設定の確認 */
				if( setFlg1[bttNumb - 1] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number Duplication). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg1[bttNumb - 1] = 1;

				/* 所属回線番号の取得 */
				workData = atoi( s_sepStr.data[2] );
				if( (workData < 1) || (workData > PV_MAX_COMLINE) )
				{
					sprintf( errMsg, "CSV-Format Error (Line Number). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 通信回線情報が未実装の場合 */
				if( *(lineExist + workData - 1) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Line Not Implimented). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->lineNumb = workData;
				
				/* 型式名称の取得 */
				if( strlen(s_sepStr.data[4]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 型式名称が一致するかテーブルのストッパーに到達するまで繰り返す */
				for( loopCnt = 0 ; strncmp( s_g_batteType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 32 ) != 0 ; loopCnt++ )
				{
					/* 型式名称が一致した場合 */
					if( strcmp( s_g_batteType[loopCnt].typeName, s_sepStr.data[4] ) == 0 )
						break;
				}

				/* 型式名称が見つからなかった場合 */
				if( strncmp( s_g_batteType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 32 ) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_BATTEINF + bttNumb - 1)->typeName, s_sepStr.data[4], 32);
				/* 装置ＩＤ番号の取得 */
				workData = atoi( s_sepStr.data[3] );
				if( (workData < s_g_batteType[loopCnt].minID) || (workData > s_g_batteType[loopCnt].maxID ) )
				{
					sprintf( errMsg, "CSV-Format Error (ID Number). File[%s] LINE[%d] DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->idNumb = workData;

				/* 重複設定の確認 */
				if( setFlg2[(ps_BATTEINF + bttNumb - 1)->lineNumb - 1][workData] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Line and ID Number Duplication). File[%s] LINE[%d] DAT[3] or DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg2[(ps_BATTEINF + bttNumb - 1)->lineNumb - 1][workData] = 1;

				/* 名称の取得 */
				if( strlen(s_sepStr.data[5]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] LINE[%d] DAT[6]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				strncpy( (ps_BATTEINF + bttNumb - 1)->batteName, s_sepStr.data[5], 72);


#if 0
				/* バッテリＩＤの取得 */
				if( strlen(s_sepStr.data[6]) != 8 )
				{
					sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] LINE[%d] DAT[7]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_BATTEINF + bttNumb - 1)->batteId, s_sepStr.data[6], 16);
#else
/* [20230529] Chg St */
				/* バッテリ用途の取得 */
				workData = atoi( s_sepStr.data[6] );
				if( ( workData < BT_TYPE_STORAGE_NO_OFFGRID) ||
					( workData > BT_TYPE_MAX) )
				{
					sprintf( errMsg, "CSV-Format Error (batteType <= 0). File[%s] LINE[%d] DAT[7]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->batteType = workData;
/* [20230529] Chg Ed */
#endif
				/* バッテリ容量を取得 */
				workData = atoi( s_sepStr.data[7] );
				if( workData <= 0 )
				{
					sprintf( errMsg, "CSV-Format Error (batteCap <= 0). File[%s] LINE[%d] DAT[8]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->batteCap = workData;
				
				/* 最大定格電力を取得 */
				workData = atoi( s_sepStr.data[8] );
				if( workData <= 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Max Rated Power <= 0). File[%s] LINE[%d] DAT[9]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->maxRatedPwr = workData;
				
				/* 最大放電電力を取得 */
				workData = atoi( s_sepStr.data[9] );
				if( workData < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (discharge Power < 0). File[%s] LINE[%d] DAT[10]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->maxDisChgPwr = workData;

				/* 最大充電電力を取得 */
				workData = atoi( s_sepStr.data[10] );
				if( workData < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (charge Power < 0 ). File[%s] LINE[%d] DAT[11]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->maxChargePwr = workData;


				/* SoC閾値上限を取得 */
				workData = atoi( s_sepStr.data[11] );
				if( (workData < 0 ) ||
				    ( workData > 100 ))
				{
					sprintf( errMsg, "CSV-Format Error (SoC Upper Threshold Error). File[%s] LINE[%d] DAT[12]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->socUpper = workData;

				/* SoC閾値下限を取得 */
				workData = atoi( s_sepStr.data[12] );
				if( (workData < 0 ) ||
				    ( (ps_BATTEINF + bttNumb - 1)->socUpper <= workData ))
				{
					sprintf( errMsg, "CSV-Format Error (SoC Lower Threshold Error). File[%s] LINE[%d] DAT[13]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_BATTEINF + bttNumb - 1)->socLower = workData;

				(ps_BATTEINF + bttNumb - 1)->stStLink = ps_linkPoint->stStLink + (bttNumb - 1) * stOffset;
				(ps_BATTEINF + bttNumb - 1)->svStLink = ps_linkPoint->svStLink + (bttNumb - 1) * svOffset;
				(ps_BATTEINF + bttNumb - 1)->svIfLink = ps_linkPoint->svIfLink + (bttNumb - 1) * PV_MAX_BATTE_SVPOS;
				(ps_BATTEINF + bttNumb - 1)->tmIfLink = ps_linkPoint->tmIfLink + (bttNumb - 1) * PV_MAX_BATTE_TMPOS;



				/* 戻り値のバッテリー数を＋１設定 */
				retCode++;

				/* 実装情報を設定 */
				commfunc_bitset( ps_linkPoint->statFlag, bttNumb - 1, 1 );
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
	}
	else
	{
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}

	/* メモリマップを解除 */
	munmap( ps_BATTEINF, sizeof(struct st_BATTEINF) * PV_MAX_BATTE);

	/* DATファイルをクローズ */
	close( fd );

	if( retCode > 0 )
	{
		ps_linkPoint->stStLink += PV_MAX_BATTE * stOffset;
		ps_linkPoint->svStLink += PV_MAX_BATTE * svOffset;
		ps_linkPoint->svIfLink += PV_MAX_BATTE * PV_MAX_BATTE_SVPOS;
		ps_linkPoint->tmIfLink += PV_MAX_BATTE * PV_MAX_BATTE_TMPOS;
	}
	/* 終了 */
	return( retCode );	
}
