/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
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

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状態保存ファイル生成処理
 *
 * 機能：
 * 		通信ステータス状態保存ファイル（STSTSFIL.dat）を生成する。
 * 		既にファイルが存在していた場合はファイルの内容は初期化しない。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_STSTSFIL_Create( )
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	int					retCode = 0;				/* 戻り値 */

	struct st_STSTSFIL	*ps_STSTSFIL;				/* ポインタ */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_STSTSFIL );
	strcat(datFilePath, ".dat");

	/* ファイルが存在しているか確認 */
	if( access( datFilePath, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}

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
	if( ftruncate( fd, sizeof(struct st_STSTSFIL) * ((PV_MAX_STDATA-1) / 8 + 1) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_STSTSFIL = (struct st_STSTSFIL *)mmap( NULL, sizeof(struct st_STSTSFIL) * ((PV_MAX_STDATA-1) / 8 + 1), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_STSTSFIL == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイル０クリアフラグがセットされている場合 */
	if( clrFlag )
	{
		/* ファイルの内容を0クリア */
		memset( ps_STSTSFIL, 0, sizeof(struct st_STSTSFIL) * ((PV_MAX_STDATA-1) / 8 + 1) );
	}

	/* メモリマップを解除 */
	munmap( ps_STSTSFIL, sizeof(struct st_STSTSFIL) * ((PV_MAX_STDATA-1) / 8 + 1) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

