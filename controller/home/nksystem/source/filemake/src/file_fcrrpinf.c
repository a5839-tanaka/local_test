/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20261015] 2026.10.15
 * ・起動時のファイルデータ設定を実装
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 *   一次調整力レポート蓄積項目情報ファイル
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 情報ファイル生成処理
 *
 * 機能：
 * 		FCRRPINF.csv ファイルから一次調整力レポート蓄積項目情報ファイル
 * 		（FCRRPINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_FCRRPINF_Create( void )
{
	int					retCode = 0;				/* 戻り値 */
/* [20250807] Add St */
	int					fd_Dat;						/* ファイルディスクリプタ */
	char				datFilePathDat[1024];		/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	struct st_FCRRPINF	*ps_FCRRPINF;				/* 一次調整力レポート蓄積項目情報ポインタ */
/* [20261015] Add St */
	struct st_TMDATINF	*ps_TMDATINF;				/* 計測情報ポインタ */
	struct st_PVSYSINF	*ps_PVSYSINF;				/* 通信回線情報ポインタ */
	FILE				*fp;						/* ファイルポインタ */
	int					fd_Tm;						/* ファイルディスクリプタ */
	int					fd_Sys;						/* ファイルディスクリプタ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	int					acs_ret = 0;				/* ファイルアクセス 初期値あり */
	char				datFilePathTm[1024];		/* DATファイルパス */
	char				datFilePathSys[1024];		/* DATファイルパス */
/* [20261015] Add Ed */

	/* ファイルパスの展開 */
/* [20261015] Add St */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcat(csvFilePath, PV_FNAME_FCRRPINF );
	strcat(csvFilePath, ".csv");
	strcpy(datFilePathSys, PV_PATH_DAT );
	strcat(datFilePathSys, PV_FNAME_PVSYSINF );
	strcat(datFilePathSys, ".dat");
	strcpy(datFilePathTm, PV_PATH_DAT );
	strcat(datFilePathTm, PV_FNAME_TMDATINF );
	strcat(datFilePathTm, ".dat");
/* [20261015] Add Ed */
	strcpy(datFilePathDat, PV_PATH_DAT );
	strcat(datFilePathDat, PV_FNAME_FCRRPINF );
	strcat(datFilePathDat, ".dat");

	/* DATファイル生成 */
	fd_Dat = open( datFilePathDat, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_Dat, sizeof(struct st_FCRRPINF)) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_FCRRPINF = (struct st_FCRRPINF *)mmap( NULL, sizeof(struct st_FCRRPINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_FCRRPINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

/* [20261015] Add St */
	/* DATファイル生成 */
	fd_Tm = open( datFilePathTm, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Tm == -1 )
	{
		sprintf( errMsg, "func_FCRRPINF_Create File[%s] Open error", datFilePathTm );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_FCRRPINF, sizeof(struct st_FCRRPINF));
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMDATINF = (struct st_TMDATINF *)mmap( NULL, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_Tm, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "func_FCRRPINF_Create File[%s] MMAP error", datFilePathTm );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_FCRRPINF, sizeof(struct st_FCRRPINF));
		close( fd_Dat );
		close( fd_Tm );
		retCode = -1;
		return( retCode );
	}

	/* DATファイル生成 */
	fd_Sys = open( datFilePathSys, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Sys == -1 )
	{
		sprintf( errMsg, "func_FCRRPINF_Create File[%s] Open error.", datFilePathSys );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_FCRRPINF, sizeof(struct st_FCRRPINF));
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
		close( fd_Dat );
		close( fd_Tm );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PVSYSINF = (struct st_PVSYSINF *)mmap( NULL, sizeof(struct st_PVSYSINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Sys, 0 );
	if( ps_PVSYSINF == MAP_FAILED)
	{
		sprintf( errMsg, "func_FCRRPINF_Create File[%s] MMAP error", datFilePathSys );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_FCRRPINF, sizeof(struct st_FCRRPINF));
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
		close( fd_Dat );
		close( fd_Tm );
		close( fd_Sys );
		retCode = -1;
		return( retCode );
	}
/* [20261015] Add Ed */

	/* ファイルの内容を0クリア */
	memset( ps_FCRRPINF, 0, sizeof(struct st_FCRRPINF));

/* [20261015] Chg St */
	/* ファイルの存在チェック */
	acs_ret = access( csvFilePath, F_OK );
	/* ファイルが存在する場合 */
	if( acs_ret == 0 )
	{
		/* CSVファイルオープン */
		if( (fp = fopen(csvFilePath, "r")) != NULL)
		{
			retCode = func_FCRRPINF_Check( fp, PV_CSVTYPE_NEW, ps_FCRRPINF, csvFilePath, ps_PVSYSINF, ps_TMDATINF );

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
		/* ファイルの内容を0クリア */
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}
/* [20261015] Add Ed */


	/* メモリマップを解除 */
	munmap( ps_PVSYSINF, sizeof(struct st_PVSYSINF));					/* [20261015] Add */
	munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);	/* [20261015] Add */
	munmap( ps_FCRRPINF, sizeof(struct st_FCRRPINF));
	/* DATファイルをクローズ */
	close( fd_Sys );	/* [20261015] Add */
	close( fd_Tm );		/* [20261015] Add */
	close( fd_Dat );

	/* 終了 */
/* [20250807] Add Ed */
	return( retCode );	
}


