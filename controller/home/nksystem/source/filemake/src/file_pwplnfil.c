/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
/*------------------------------------------------------------------------------
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
 * エネルギーマネジメント管理ファイル生成処理
 *
 * 機能：
 * 		エネルギーマネジメント制御管理ファイル（PWPLNFIL.dat）が存在しない場合は自動生成する。
 * 		本ファイルは前回起動時に本ファイルに登録されているバッテリー情報が、
 * 		バッテリー情報ファイル（BATTEINF.dat）に存在するか確認し、存在するバッテリー
 * 		の管理情報は削除せず、存在しないバッテリーは削除し、新規のバッテリーは
 * 		バッテリー情報を追加する。
 *
 */
int	func_PWPLNFIL_Create( void )
{
	int					fd;							/* ファイルディスクリプタ */
	int					fd_work;					/* ファイルディスクリプタ */
	FILE				*fp;						/* ファイルポインタ */
	char				wokFilePath[1024];			/* DATファイルパス */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	int					retCode = 0;				/* 戻り値 */
	int					emInfoCnt = 0;				/* バッテリ機器数 */
	int					loopCnt1;					/* ループカウンタ */
	int					loopCnt2;					/* ループカウンタ */
	struct st_PWPLNFIL	*ps_PWPLNFIL;				/* ポインタ */
	static struct stat	statInfo;					/* ファイル状態 *//* [20230529] Add */
	int					fileSize;					/* ファイルサイズ *//* [20230529] Add */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_PWPLNFIL );
	strcat(datFilePath, ".dat");

	fileSize = sizeof(struct st_PWPLNFIL);	/* [20230529] Add */

	/* ファイルが存在しているか確認 */
	if( access( datFilePath, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}
/* [20230529] Add St */
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
/* [20230529] Add Ed */

	/* DATファイル生成 */
	fd = open( datFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd == -1 )
	{
		sprintf( errMsg, "File[%s] Open error!n", datFilePath );
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
	ps_PWPLNFIL = (struct st_PWPLNFIL *)mmap( NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_PWPLNFIL == MAP_FAILED)
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
		/* ファイルの内容を0クリア */
		memset( ps_PWPLNFIL, 0, fileSize );
	}

	/* メモリマップを解除 */
	munmap( ps_PWPLNFIL, fileSize );


	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

