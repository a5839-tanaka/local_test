/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・ファイル構造(サイズ)が前回と変わった場合、初期化するように修正
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
/* [20260515] Add St */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	static struct stat	statInfo;					/* ファイル状態 */
	int					fileSize;					/* ファイルサイズ */
/* [20260515] Add Ed */

	/* ファイルパスの展開 */
	strcpy(datFilePathDat, PV_PATH_DAT );
	strcat(datFilePathDat, PV_FNAME_FCRRPINF );
	strcat(datFilePathDat, ".dat");

/* [20260515] Add St */
	/* ファイルが存在しているか確認 */
	if( access( datFilePathDat, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}
	else
	{
		/* ファイル状態の取得に失敗 */
		if( stat( datFilePathDat, &statInfo ) != 0 )
		{
			sprintf( errMsg, "File[%s] stat error!", datFilePathDat );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
		}
		else
		{
			/* ファイルサイズが前回起動時と異なる場合 */
			if( statInfo.st_size != fileSize )
			{
				clrFlag = 1;
				sprintf( errMsg, "File[%s] Initialize for fileSize update.", datFilePathDat );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
			}
		}
	}
/* [20260515] Add Ed */

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

/* [20260515] Chg St */
	/* ファイル０クリアフラグがセットされている場合 */
	if( clrFlag )
	{
		/* ファイルの内容を0クリア */
		memset( ps_FCRRPINF, 0, sizeof(struct st_FCRRPINF));
	}
/* [20260515] Chg Ed */

	/* メモリマップを解除 */
	munmap( ps_FCRRPINF, sizeof(struct st_FCRRPINF));
	/* DATファイルをクローズ */
	close( fd_Dat );
	/* 終了 */
/* [20250807] Add Ed */
	return( retCode );	
}


