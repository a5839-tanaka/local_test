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
 * 収集装置管理ファイル生成処理
 *
 * 機能：
 * 		周波数監視情報ファイル（RCVPWINF.dat）を生成する
 * 		RCVPWINF.csv ファイルが存在しない場合は異常とせず、正常終了する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_RCVPWINF_Create( void )
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

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_RCVPWINF	*ps_RCVPWINF;				/* 通信回線情報ポインタ */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_RCVPWINF );
	strcat(datFilePath, PV_FNAME_RCVPWINF );
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
	if( ftruncate( fd, sizeof(struct st_RCVPWINF) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_RCVPWINF = (struct st_RCVPWINF *)mmap( NULL, sizeof(struct st_RCVPWINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_RCVPWINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		retCode = func_RCVPWINF_Check( fp, PV_CSVTYPE_NEW, ps_RCVPWINF, csvFilePath );

		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}

	/* メモリマップを解除 */
	munmap( ps_RCVPWINF, sizeof(struct st_RCVPWINF) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}


