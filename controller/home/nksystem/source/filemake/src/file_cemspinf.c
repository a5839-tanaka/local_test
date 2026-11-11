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
 * 情報ファイル生成処理
 *
 * 機能：
 * 		CEMSPINF.csv ファイルから通信ステータス情報ファイル（CEMSPINF.dat）を生
 * 		成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_CEMSPINF_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_Dat;						/* ファイルディスクリプタ（通信ステータス情報） */
	int					fd_Msk;						/* ファイルディスクリプタ（通信ステータス状変マスク情報） */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePath[1024];		/* DATファイルパス（通信ステータス情報） */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					retCode = 0;				/* 戻り値 */
	struct st_CEMSPINF	*ps_CEMSPINF;				/* CEMS情報ポインタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_CEMSPINF );
	strcat(datFilePath, PV_FNAME_CEMSPINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePath, ".dat");

	/* DATファイル生成 */
	fd_Dat = open( datFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_Dat, sizeof(struct st_CEMSPINF)) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_CEMSPINF = (struct st_CEMSPINF *)mmap( NULL, sizeof(struct st_CEMSPINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_CEMSPINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		retCode = func_CEMSPINF_Check( fp, PV_CSVTYPE_NEW, ps_CEMSPINF, csvFilePath );

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
	munmap( ps_CEMSPINF, sizeof(struct st_CEMSPINF));
	/* DATファイルをクローズ */
	close( fd_Dat );
	/* 終了 */
	return( retCode );	
}


