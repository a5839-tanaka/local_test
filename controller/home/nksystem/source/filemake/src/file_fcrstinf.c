/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260212] 2026.02.12
 * ・一次調整力のコマで、データアップデートされた場合に
 *   ファイル内容の初期化を行わない(0除算対策)
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 *   一次調整力パラメータ設定情報ファイル
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

#if 0
#define DEBUG_FCRSTINF_INIT
#endif

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 情報ファイル生成処理
 *
 * 機能：
 * 		FCRSTINF.csv ファイルから一次調整力パラメータ設定情報ファイル
 * 		（FCRSTINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_FCRSTINF_Create( void )
{
	int					retCode = 0;				/* 戻り値 */
/* [20250807] Add St */
	int					fd_Dat;						/* ファイルディスクリプタ */
	char				datFilePathDat[1024];		/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	struct st_FCRSTINF	*ps_FCRSTINF;				/* 一次調整力パラメタ設定情報ポインタ */
/* [20260212] Add St */
	int					acs_ret = 0;				/* ファイルアクセス 初期値あり */
/* [20260212] Add Ed */

	/* ファイルパスの展開 */
	strcpy(datFilePathDat, PV_PATH_DAT );
	strcat(datFilePathDat, PV_FNAME_FCRSTINF );
	strcat(datFilePathDat, ".dat");

/* [20260212] Add St */
	/* ファイルの存在チェック */
	acs_ret = access( datFilePathDat, F_OK );
/* [20260212] Add Ed */

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
	if( ftruncate( fd_Dat, sizeof(struct st_FCRSTINF)) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_FCRSTINF = (struct st_FCRSTINF *)mmap( NULL, sizeof(struct st_FCRSTINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_FCRSTINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

/* [20260212] Chg St */
	if( acs_ret == -1 )
	{
		/* ファイルの内容を0クリア */
		memset( ps_FCRSTINF, 0, sizeof(struct st_FCRSTINF));
	}
#ifdef DEBUG_FCRSTINF_INIT
	sprintf( errMsg, "ps_FCRSTINF acs_ret == %d (0:NONE -1:INIT) FILE %s" ,acs_ret, datFilePathDat );
	commfunc_syslog_msgset( "FILEMAKE", errMsg );
#endif /* DEBUG_FCRSTINF_INIT */
/* [20260212] Chg Ed */

	/* メモリマップを解除 */
	munmap( ps_FCRSTINF, sizeof(struct st_FCRSTINF));
	/* DATファイルをクローズ */
	close( fd_Dat );
	/* 終了 */
/* [20250807] Add Ed */
	return( retCode );	
}


