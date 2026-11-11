/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 *------------------------------------------------------------------------------
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
#include <stdlib.h>
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
 * 集約状態保存ファイル生成処理
 *
 * 機能：
 * 		集約状態保存ファイル（SVAGGFIL.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_SVAGGFIL_Create( void )
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */
	int					loopCnt2;					/* ループカウンタ */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */

	struct st_SVAGGFIL	*ps_SVAGGFIL;				/* 計測値蓄積ファイルポインタ */
	int					workData;

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_SVAGGFIL );
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
	if( ftruncate( fd, sizeof(struct st_SVAGGFIL) * PV_MAX_PVSYS_AGGSVPOS ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_SVAGGFIL = (struct st_SVAGGFIL *)mmap( NULL, sizeof(struct st_SVAGGFIL) * PV_MAX_PVSYS_AGGSVPOS, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_SVAGGFIL == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの中身をALL0クリア */
	memset( ps_SVAGGFIL, 0, sizeof(struct st_SVAGGFIL) * PV_MAX_PVSYS_AGGSVPOS );

	/* メモリマップを解除 */
	munmap( ps_SVAGGFIL, sizeof(struct st_SVAGGFIL) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}
