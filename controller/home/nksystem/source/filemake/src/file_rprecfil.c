/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
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

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 実績データ蓄積ファイル生成処理
 *
 * 機能：
 * 		実績データ蓄積ファイル（RPRECFIL.dat）を生成する。
 * 		本ファイルはファイルが作成されていない場合、
 * 		もしくはファイルサイズが前回起動時と異なる場合に内容を初期化する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_RPRECFIL_Create( )
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */
	int					loopCnt2;					/* ループカウンタ */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	struct st_RPRECFIL	*ps_RPRECFIL;				/* 計測値蓄積ファイルポインタ */
	int					workData;
	static struct stat	statInfo;					/* ファイル状態 */
	int					fileSize;					/* ファイルサイズ */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_RPRECFIL );
	strcat(datFilePath, ".dat");

	fileSize = sizeof(struct st_RPRECFIL);

	/* ファイルが存在しているか確認 */
	if( access( datFilePath, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}
	else
	{
		/* ファイル状態の取得に失敗 */
		if( stat( datFilePath, &statInfo ) != 0 )
		{
			sprintf( errMsg, "File[%s] stat error!", datFilePath );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
		}
		else
		{
			/* ファイルサイズが前回起動時と異なる場合 */
			if( statInfo.st_size != fileSize )
			{
				clrFlag = 1;
				sprintf( errMsg, "File[%s] Initialize for fileSize update.", datFilePath );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
			}
		}
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
	if( ftruncate( fd, fileSize) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_RPRECFIL = (struct st_RPRECFIL *)mmap( NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_RPRECFIL == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイル０クリアフラグがセットされている場合 */
	if( clrFlag )
	{
		/* ファイルの中身をALL0クリア */
		memset( ps_RPRECFIL, 0, fileSize );
	}

	/* メモリマップを解除 */
	munmap( ps_RPRECFIL, fileSize );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

