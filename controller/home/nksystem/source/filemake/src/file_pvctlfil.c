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
 * 制御状態保存ファイルを生成する
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
 * 制御状態保存ファイル生成処理
 *
 * 機能：
 *		制御状態保存ファイル（PVCTLFIL.dat）を生成し、データで定義された装置の中
 *		から制御可能な装置を探索する。（探索対象の装置は本プログラム内で定義する）
 *		探索の結果、該当する装置については当該ファイルに登録する。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	func_PVCTLFIL_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					fd;							/* ファイルディスクリプタ */
	int					retCode = 0;				/* 戻り値 */

	struct st_PVCTLFIL	*ps_PVCTLFIL;				/* 制御状態保存ポインタ */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_PVCTLFIL );
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
	if( ftruncate( fd, sizeof(struct st_PVCTLFIL) * PV_CTL_TYPE_MAX ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PVCTLFIL = (struct st_PVCTLFIL *)mmap( NULL, sizeof(struct st_PVCTLFIL) * PV_CTL_TYPE_MAX, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_PVCTLFIL == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_PVCTLFIL, 0, sizeof(struct st_PVCTLFIL) * PV_CTL_TYPE_MAX );

	/* メモリマップを解除 */
	munmap( ps_PVCTLFIL, sizeof(struct st_PVCTLFIL) * PV_CTL_TYPE_MAX);

	/* DATファイルをクローズ */
	close( fd );

	/* 終了 */
	return( retCode );	
}

