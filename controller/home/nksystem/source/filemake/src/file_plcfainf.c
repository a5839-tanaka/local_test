/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251210] 2025.12.10
 * ・越路原対応PLC追加
 * [20251108] 2025.11.08
 * ・栗野対応PLC追加
 * [20251027] 2025.10.27
 * ・出力制御ユニット用ＰＬＣ（ModbusTCP）に対応
 * [20250725] 2025.07.25
 * ・北広島対応PLC追加
 * [20180505] 2018.05.05
 * ・収集装置外部Ｉ／Ｏ用ＰＬＣ（SLMP:Ethernet） に対応
 * [20150612] 2015.06.12
 * ・特別高圧発電所（ＮＫ方式１）の追加
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
 */
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
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
struct st_typeName		s_g_plcfaType[] = {		/* 型式名称 */
	{ "HYDRO-PLC1"       , 1, 247 },			/* 小水力発電所（ＮＫ方式１） */
	{ "EHVPS-PLC1"       , 1, 247 },			/* 特別高圧発電所（ＮＫ方式１）[20150612] Add */
	{ "NKIOPLC1-MGE1"    , 1, 247 },			/* 収集装置外部Ｉ／Ｏ用ＰＬＣ [20230529] Chg */
	{ "NKIOPLC1-MGE2"    , 1, 247 },			/* 収集装置外部Ｉ／Ｏ用ＰＬＣ [20230529] Add */
	{ "NKIOPLC1-MGE3"    , 1, 247 },			/* 収集装置外部Ｉ／Ｏ用ＰＬＣ [20250725] Add */
	{ "NKIOPLC1-MGE4"    , 1, 247 },			/* 収集装置外部Ｉ／Ｏ用ＰＬＣ TYPE4[20251108] Add */
	{ "NKIOPLC1-MGE5"    , 1, 247 },			/* 収集装置外部Ｉ／Ｏ用ＰＬＣ TYPE5[20251210] Add */
	{ "NKBIOPLC1"        , 1, 247 },			/* BGIO盤 ModbusTCP */
	{ "NKDERPLC1"        , 1, 247 },			/* DER盤 ModbusTCP */
	{ "PVUF-PCS001K"     , 1, 247 },			/* 出力制御ユニット用ＰＬＣ（ModbusTCP） *//* [20251027] Add */
	{ PV_FILEMAKE_STOPPER, 0,   0 }				/* ストッパー */
};


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＰＬＣ情報ファイル生成
 *
 * 機能：
 * 		PLCFAINF.csv ファイルからＰＬＣ情報ファイル（PLCFAINF.dat）を生成する。
 * 		PLCFAINF.csv ファイルが存在しない場合は異常終了としない。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */ 
int	func_PLCFAINF_Create( int *lineExist, struct st_linkPoint *ps_linkPoint )
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

	struct st_PLCFAINF	*ps_PLCFAINF;				/* ＰＬＣ情報ポインタ */
	char				setFlg1[PV_MAX_PLCFA];		/* 重複設定確認用 [20140904] Add */
	char				setFlg2[PV_MAX_COMLINE][256];/* 重複設定確認用 [20140904] Add */
	int					plcNumb;					/* ＰＬＣ番号 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSVファイル処理カウンタ */

	/* オフセット値の取得 */
	stOffset = (PV_MAX_STPOS - 1) / 8 + 1;
	svOffset = (PV_MAX_PLCFA_SVPOS - 1) / 8 + 1;

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_PLCFAINF );
	strcat(datFilePath, PV_FNAME_PLCFAINF );
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
		if( ftruncate( fd, sizeof(struct st_PLCFAINF) * PV_MAX_PLCFA ) == -1 )
		{
			sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			close( fd );
			retCode = -1;
			return( retCode );
		}

		/* ファイルをメモリマップ */
		ps_PLCFAINF = (struct st_PLCFAINF *)mmap( NULL, sizeof(struct st_PLCFAINF) * PV_MAX_PLCFA, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
		if( ps_PLCFAINF == MAP_FAILED)
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
		memset( ps_PLCFAINF, 0, sizeof(struct st_PLCFAINF) * PV_MAX_PLCFA );

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
				/* ＰＬＣ番号の取得 */
				plcNumb = atoi( s_sepStr.data[0] );
				if( (plcNumb < 1) || (plcNumb > PV_MAX_PLCFA) )
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
				(ps_PLCFAINF + plcNumb - 1)->statFlag = workData;

				/* 重複設定の確認 */
				if( setFlg1[plcNumb - 1] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number Duplication). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg1[plcNumb - 1] = 1;

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
				(ps_PLCFAINF + plcNumb - 1)->lineNumb = workData;
				/* 型式名称の取得 */
				if( strlen(s_sepStr.data[4]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 型式名称が一致するか、ストッパーまで繰り返す */
				for( loopCnt = 0 ; strncmp( s_g_plcfaType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 16 ) != 0 ; loopCnt++ )
				{
					/* 型式名称が一致した場合 */
					if( strcmp( s_g_plcfaType[loopCnt].typeName, s_sepStr.data[4] ) == 0 )
						break;
				}
				/* 型式名称に処理可能な装置以外のものがある場合 */
				if( strncmp( s_g_plcfaType[loopCnt].typeName, PV_FILEMAKE_STOPPER, 16 ) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Type Name). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_PLCFAINF + plcNumb - 1)->typeName, s_sepStr.data[4], 16);
				/* 装置ＩＤ番号の取得 */
				workData = atoi( s_sepStr.data[3] );
				if( (workData < s_g_plcfaType[loopCnt].minID) || (workData > s_g_plcfaType[loopCnt].maxID) )
				{
					sprintf( errMsg, "CSV-Format Error (ID Number). File[%s] LINE[%d] DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				(ps_PLCFAINF + plcNumb - 1)->idNumb = workData;

				/* 重複設定の確認 */
				if( setFlg2[(ps_PLCFAINF + plcNumb - 1)->lineNumb - 1][workData] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (Line and ID Number Duplication). File[%s] LINE[%d] DAT[3] or DAT[4]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlg2[(ps_PLCFAINF + plcNumb - 1)->lineNumb - 1][workData] = 1;

				/* 名称の取得 */
				if( strlen(s_sepStr.data[5]) == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] LINE[%d] DAT[6]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( (ps_PLCFAINF + plcNumb - 1)->plcfaName, s_sepStr.data[5], 72);
				(ps_PLCFAINF + plcNumb - 1)->stStLink = ps_linkPoint->stStLink + (plcNumb - 1) * stOffset;
				(ps_PLCFAINF + plcNumb - 1)->svStLink = ps_linkPoint->svStLink + (plcNumb - 1) * svOffset;
				(ps_PLCFAINF + plcNumb - 1)->svIfLink = ps_linkPoint->svIfLink + (plcNumb - 1) * PV_MAX_PLCFA_SVPOS;
				(ps_PLCFAINF + plcNumb - 1)->tmIfLink = ps_linkPoint->tmIfLink + (plcNumb - 1) * PV_MAX_PLCFA_TMPOS;
	
				/* 戻り値のＰＬＣ情報数を＋１設定 */
				retCode++;

				/* 実装情報を設定 */
				commfunc_bitset( ps_linkPoint->statFlag, plcNumb - 1, 1 );
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
		munmap( ps_PLCFAINF, sizeof(struct st_PLCFAINF) * PV_MAX_PLCFA);

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
		ps_linkPoint->stStLink += PV_MAX_PLCFA * stOffset;
		ps_linkPoint->svStLink += PV_MAX_PLCFA * svOffset;
		ps_linkPoint->svIfLink += PV_MAX_PLCFA * PV_MAX_PLCFA_SVPOS;
		ps_linkPoint->tmIfLink += PV_MAX_PLCFA * PV_MAX_PLCFA_TMPOS;
	}
	/* 終了 */
	return( retCode );	
}

