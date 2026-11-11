/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20250508] 2025.05.08
 * ・三菱電子式マルチ指示計器の以下の型式に対応
 *   ME110SR-MB,ME110NSR-MB,ME110SSR-MB,ME110GR-MBH
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

/* モジュール内グローバル変数定義 */
struct st_typeName		s_g_mltmtType[] = {		/* 型式名称 */
	{ "SCHNEI-PM8000"    , 1, 254 },			/* シュナイダー製 PM8000 (Modbus-TCP) */
	{ "SCHNEI-PM8240"    , 1, 254 },			/* シュナイダー製 PM8000 (Modbus-TCP) */
	{ "KM1-PMU1A-FLK"    , 1,  99 },			/* OMRON KM1シリーズ：電力計測ユニット */
	{ "KM1-PMU2A-FLK"    , 1,  99 },			/* OMRON KM1シリーズ：電力２系統計測ユニット */
	{ "KM1-EMU8A-FLK"    , 1,  99 },			/* OMRON KM1シリーズ：パルス/温度入力ユニット */
	{ "KE1-CTD8E"        , 1,  99 },			/* OMRON KM1シリーズ：ＣＴ増設ユニット */
	{ "TAKE-XS2-110"     , 1,  99 },			/* タケモトデンキ製マルチメータXS2-110シリーズ */
	{ "TAKE-XM2-110"     , 1,  99 },			/* タケモトデンキ製マルチメータXM2-110シリーズ */
	{ "MITSU-EMU4-BD1"   , 1,  31 },			/* 三菱電機製EcoMonitor EMU4-BD1-MB */
	{ "MITSU-EMU4-HD1"   , 1,  31 },			/* 三菱電機製EcoMonitor EMU4-HD1-MB  */
	{ "MITSU-ME110SR-MB" , 1,  31 },			/* 三菱電子式マルチ指示計器 ME110SR-MB *//* [20250508] Add */
	{ "MITSU-ME110NSR-MB", 1,  31 },			/* 三菱電子式マルチ指示計器 ME110NSR-MB *//* [20250508] Add */
	{ "MITSU-ME110SSR-MB", 1,  31 },			/* 三菱電子式マルチ指示計器 ME110SSR-MB *//* [20250508] Add */
	{ "MITSU-ME110GR-MBH", 1,  31 },			/* 三菱電子式マルチ指示計器 ME110GR-MBH *//* [20250508] Add */
	{ PV_FILEMAKE_STOPPER, 0,  0 }				/* ストッパー */
};


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * マルチメータ情報ファイル生成
 *
 * 機能：
 * 		MLTMTINF.csv ファイルからマルチメータ情報ファイル（MLTMTINF.dat）
 * 		を生成する。
 * 		MLTMTINF.csv ファイルが存在しない場合は異常終了としない。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */ 
int	func_MLTMTINF_Create( int *lineExist, struct st_linkPoint *ps_linkPoint )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd;							/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウント */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	int					stOffset;					/* リンク情報オフセット値 */
	int					svOffset;					/* リンク情報オフセット値 */

	struct st_MLTMTINF	*ps_MLTMTINF;				/* スマート監視機器情報ポインタ */
	char				setFlg1[PV_MAX_MLTMT];		/* 重複設定確認用 */
	char				setFlg2[PV_MAX_COMLINE][100];/* 重複設定確認用 */
	int					km1Numb;					/* スマート監視機器番号 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSVファイル処理カウンタ */

	/* オフセット値の取得 */
	stOffset = (PV_MAX_STPOS - 1) / 8 + 1;
	svOffset = (PV_MAX_MLTMT_SVPOS - 1) / 8 + 1;

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_MLTMTINF );
	strcat(datFilePath, PV_FNAME_MLTMTINF );
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
		if( ftruncate( fd, sizeof(struct st_MLTMTINF) * PV_MAX_MLTMT ) == -1 )
		{
			sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			close( fd );
			retCode = -1;
			return( retCode );
		}

		/* ファイルをメモリマップ */
		ps_MLTMTINF = (struct st_MLTMTINF *)mmap( NULL, sizeof(struct st_MLTMTINF) * PV_MAX_MLTMT, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
		if( ps_MLTMTINF == MAP_FAILED)
		{
			sprintf( errMsg, "File[%s] MMAP error", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			return( retCode );
		}

		/* 重複設定確認用領域の初期化 */
		memset( setFlg1, 0, sizeof(setFlg1) );
		memset( setFlg2, 0, sizeof(setFlg2) );

		/* ファイルの内容を0クリア */
		memset( ps_MLTMTINF, 0, sizeof(struct st_MLTMTINF) * PV_MAX_MLTMT );

		/* CSVファイルオープン */
		if( (fp = fopen(csvFilePath, "r")) != NULL)
		{
			/* CSVファイルの終端まで繰り返す */
			while( fgets( readBuff, 4096, fp) != NULL )
			{
				/* CSV処理行数を＋１ */
				syoriCount++;
				/* コメント行はスキップ */
				if( readBuff[0] == '#' ) continue;
				/* 未入力行（9文字以下）はスキップ */
				if( strlen( readBuff ) < 10) continue;
				/* CSVデータの分割 */
				csvCount = func_sep_data( readBuff, &s_sepStr );
				/* CSVデータ数が不正の場合 */
				if( csvCount != 6)
				{
					sprintf( errMsg, "CSV-Format Error (Data is not 6). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* スマート監視機器番号の取得 */
				km1Numb = atoi( s_sepStr.data[0] );
				if( (km1Numb < 1) || (km1Numb > PV_MAX_MLTMT) )
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
				(ps_MLTMTINF + km1Numb - 1)->statFlag = workData;

				/* 重複設定の確認 */
				if( setFlg1[km1Numb - 1] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number Duplication). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg1[km1Numb - 1] = 1;

				/* 所属回線番号の取得 */
				workData = atoi( s_sepStr.data[2] );
				if( (workData < 1) || (workData > PV_MAX_COMLINE) )
				{
					sprintf( errMsg, "CSV-Format Error (Line Number). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 所属回線が未実装の場合 */
				if( *(lineExist + workData - 1) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Line Not Implimented). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_MLTMTINF + km1Numb - 1)->lineNumb = workData;
				/* 型式名称の取得 */
				if( strlen(s_sepStr.data[4]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 型式名称が一致するか、ストッパーまで繰り返す */
				for( loopCnt = 0 ; strncmp( s_g_mltmtType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 32 ) != 0 ; loopCnt++ )
				{
					if( strcmp( s_g_mltmtType[loopCnt].typeName, s_sepStr.data[4] ) == 0 )
						break;
				}
				/* 型式名称が一致しなかった場合 */
				if( strncmp( s_g_mltmtType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 32 ) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_MLTMTINF + km1Numb - 1)->typeName, s_sepStr.data[4], 32);
				/* 装置ＩＤ番号の取得 */
				workData = atoi( s_sepStr.data[3] );
				if( (workData < s_g_mltmtType[loopCnt].minID) || (workData > s_g_mltmtType[loopCnt].maxID) )
				{
					sprintf( errMsg, "CSV-Format Error (ID Number). File[%s] LINE[%d] DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_MLTMTINF + km1Numb - 1)->idNumb = workData;

				/* 重複設定の確認 */
				if( setFlg2[(ps_MLTMTINF + km1Numb - 1)->lineNumb - 1][workData] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Line and ID Number Duplication). File[%s] LINE[%d] DAT[3] or DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg2[(ps_MLTMTINF + km1Numb - 1)->lineNumb - 1][workData] = 1;

				/* 名称の取得 */
				if( strlen(s_sepStr.data[5]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] LINE[%d] DAT[6]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_MLTMTINF + km1Numb - 1)->mltmtName, s_sepStr.data[5], 72);
				(ps_MLTMTINF + km1Numb - 1)->stStLink = ps_linkPoint->stStLink + (km1Numb - 1) * stOffset;
				(ps_MLTMTINF + km1Numb - 1)->svStLink = ps_linkPoint->svStLink + (km1Numb - 1) * svOffset;
				(ps_MLTMTINF + km1Numb - 1)->svIfLink = ps_linkPoint->svIfLink + (km1Numb - 1) * PV_MAX_MLTMT_SVPOS;
				(ps_MLTMTINF + km1Numb - 1)->tmIfLink = ps_linkPoint->tmIfLink + (km1Numb - 1) * PV_MAX_MLTMT_TMPOS;
	
				/* 戻り値のスマート監視機器情報数を＋１設定 */
				retCode++;

				/* 実装情報を設定 */
				commfunc_bitset( ps_linkPoint->statFlag, km1Numb - 1, 1 );
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
		munmap( ps_MLTMTINF, sizeof(struct st_MLTMTINF) * PV_MAX_MLTMT);

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
		ps_linkPoint->stStLink += PV_MAX_MLTMT * stOffset;
		ps_linkPoint->svStLink += PV_MAX_MLTMT * svOffset;
		ps_linkPoint->svIfLink += PV_MAX_MLTMT * PV_MAX_MLTMT_SVPOS;
		ps_linkPoint->tmIfLink += PV_MAX_MLTMT * PV_MAX_MLTMT_TMPOS;
	}
	/* 終了 */
	return( retCode );	
}

