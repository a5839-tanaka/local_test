/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20240125] 2024.01.25 清水エネプラから流用
 * ・ログファイル／蓄積ファイル出力設定処理の追加
 * [20231211] 2023.12.11]
 * ・前回起動中の設定変更ファイルと収集装置ＩＤと一致しない場合、
 *   設定変更ファイルを初期化し、ログ出力するように修正
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

/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20130624] 2013.06.24
 * ・PVSYSINFファイルのデータ構成を変更
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


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 収集装置管理ファイル生成処理
 *
 * 機能：
 * 		pvsysinf.csv ファイルを元に収集装置管理ファイル（PVSYSINF.dat）を生成する
 * 		上記の他、BATTEINF.csv、POWCNINF.csv、MLTMTINF.csv ファイル（＊１）から
 * 		装置の実装数を取得して格納する。
 * 		（＊１：実際はメイン処理内で取得した情報を使用して装置数を設定するため、
 * 		　　　　本関数ないでは、各装置のcsvファイルは参照しない）
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_PVSYSINF_Create( struct st_linkPoint *ps_allLinkPoint )
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
	char				updateconf[64];				/* 設定変更ファイル名 */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_PVSYSINF	*ps_PVSYSINF;				/* 通信回線情報ポインタ */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	char				tmpFile[512];				/* 設定ファイルパス *//* [20240125] Add */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_PVSYSINF );
	strcat(datFilePath, PV_FNAME_PVSYSINF );
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
	if( ftruncate( fd, sizeof(struct st_PVSYSINF) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PVSYSINF = (struct st_PVSYSINF *)mmap( NULL, sizeof(struct st_PVSYSINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_PVSYSINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	memcpy(updateconf, ps_PVSYSINF->updateConfFile, sizeof( updateconf ));
	/* ファイルの中身をALL0クリア */
	memset( ps_PVSYSINF, 0, sizeof(struct st_PVSYSINF) );
	memcpy(ps_PVSYSINF->updateConfFile, updateconf, sizeof( updateconf ));

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
			/* 未入力行（8文字以下）はスキップ */
			if( strlen( readBuff ) < 8) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );
			/* CSVデータ数が不正の場合 */
			if( csvCount != 11)
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 11). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 収集装置ＩＤ名称の取得 */
			if( strlen(s_sepStr.data[0]) != 8 )
			{
				sprintf( errMsg, "CSV-Format Error (ID Name Error). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PVSYSINF->siteIdName, s_sepStr.data[0], 16);
			/* 業務プロセス起動情報の取得 */
			for( loopCnt = 0 ; loopCnt < 10 ; loopCnt++ )
			{
                /* 業務プロセス起動情報の取得 */
				workData = atoi( s_sepStr.data[loopCnt + 1] );
				if( (workData != 0 ) && (workData != 1 ) )
				{
					sprintf( errMsg, "CSV-Format Error (Program Execute Flag). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, 2 + loopCnt );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				ps_PVSYSINF->gyomuProc[loopCnt] = workData;
			}
			retCode = 1;
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

/* [20231211] Add St */
	/* 正常終了の場合（異常終了の場合は前回の設定変更ファイルを保持する為） */
	if( retCode >= 0 )
	{
		/* 設定変更ファイル名の先頭８文字が収集装置ＩＤと一致しない場合 */
/* [20240912] Chg St */
		if( ( ps_PVSYSINF->updateConfFile[0] != 0 ) &&
			( strncmp( ps_PVSYSINF->updateConfFile, ps_PVSYSINF->siteIdName, 8 ) != 0 ) )
/* [20240912] Chg Ed */
		{
			sprintf( errMsg, "updateConfFile unmatch.(%s != %s)", ps_PVSYSINF->siteIdName, ps_PVSYSINF->updateConfFile );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );

			/* 設定変更ファイル名を初期化 */
			memset(ps_PVSYSINF->updateConfFile, 0, sizeof(ps_PVSYSINF->updateConfFile));
		}
	}
/* [20231211] Add Ed */

	/* 装置種別毎の装置数を格納 */
	for( loopCnt = 0 ; loopCnt < PV_MAX_MKIND ; loopCnt++ )
	{
		/* 実装装置数を設定 */
		ps_PVSYSINF->s_machineInfo[loopCnt].machineCnt = ps_allLinkPoint[loopCnt].statCnt;
		/* 諸元関連情報を設定 */
		ps_PVSYSINF->s_machineInfo[loopCnt].maxMachine = ps_allLinkPoint[loopCnt].maxMachine;
		ps_PVSYSINF->s_machineInfo[loopCnt].maxTm      = ps_allLinkPoint[loopCnt].maxTm;
		ps_PVSYSINF->s_machineInfo[loopCnt].maxSv      = ps_allLinkPoint[loopCnt].maxSv;
		/* 各リンク情報を設定 */
		ps_PVSYSINF->s_machineInfo[loopCnt].tmIfLink   = ps_allLinkPoint[loopCnt].tmIfLink;
		ps_PVSYSINF->s_machineInfo[loopCnt].svIfLink   = ps_allLinkPoint[loopCnt].svIfLink;
		ps_PVSYSINF->s_machineInfo[loopCnt].svStLink   = ps_allLinkPoint[loopCnt].svStLink;
		ps_PVSYSINF->s_machineInfo[loopCnt].stStLink   = ps_allLinkPoint[loopCnt].stStLink;
		/* 実装/未実装情報を設定 */
		memcpy( ps_PVSYSINF->s_machineInfo[loopCnt].statFlag, ps_allLinkPoint[loopCnt].statFlag, sizeof(ps_allLinkPoint[loopCnt].statFlag) );
	}

/* [20240125] Add St */
	/* 蓄積ファイル生成設定ファイルがある場合 */
	sprintf( tmpFile, "%sREC_FILE_NOT_CREATE", PV_PATH_CSV );
	if( access( tmpFile, R_OK | W_OK ) == 0 )
	{
		/* ファイル生成をOFFにする */
		ps_PVSYSINF->recFileCre = 0;
		sprintf( errMsg, "rec file not create." );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}
	else
	{
		/* ファイル生成をONにする */
		ps_PVSYSINF->recFileCre = 1;
	}

	/* 通信回線分ループ */
	for( loopCnt = 0; loopCnt < PV_MAX_COMLINE; loopCnt++ )
	{
		/* 通信回線の通信ログ出力設定ファイルがある場合 */
		sprintf( tmpFile, "%sSERIAL_LINE_%02d_LOG_ON", PV_PATH_CSV, loopCnt+1 );
		if( access( tmpFile, R_OK | W_OK ) == 0 )
		{
			/* ログ出力をONにする */
			ps_PVSYSINF->lineLogFlag[loopCnt] = 1;
			sprintf( errMsg, "serial-line%02d log ON", loopCnt+1 );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
		}
	}
/* [20240125] Add Ed */

	/* メモリマップを解除 */
	munmap( ps_PVSYSINF, sizeof(struct st_PVSYSINF) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

