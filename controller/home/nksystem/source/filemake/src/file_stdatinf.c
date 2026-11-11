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
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス情報ファイル生成処理
 *
 * 機能：
 * 		STDATINF.csv ファイルから通信ステータス情報ファイル（STDATINF.dat）を生
 * 		成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_STDATINF_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_Dat;						/* ファイルディスクリプタ（通信ステータス情報） */
	int					fd_Msk;						/* ファイルディスクリプタ（通信ステータス状変マスク情報） */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePathDat[1024];		/* DATファイルパス（通信ステータス情報） */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_STDATINF	*ps_STDATINF;				/* 通信ステータス情報ポインタ */
	int					stNumb;						/* 状態番号 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathDat, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_STDATINF );
	strcat(datFilePathDat, PV_FNAME_STDATINF );
	strcat(csvFilePath, ".csv");
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
	if( ftruncate( fd_Dat, sizeof(struct st_STDATINF) * PV_MAX_STPOS ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_STDATINF = (struct st_STDATINF *)mmap( NULL, sizeof(struct st_STDATINF) * PV_MAX_STPOS, PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_STDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_STDATINF, 0, sizeof(struct st_STDATINF) * PV_MAX_STPOS );

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			/* CSV処理カウンタ＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（3文字以下）はスキップ */
			if( strlen( readBuff ) < 3) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );
			/* CSVデータ数が不正の場合 */
			if( csvCount != 2)
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 2). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 状態番号の取得 */
			stNumb = atoi( s_sepStr.data[0] );
			if( (stNumb < 1) || (stNumb > PV_MAX_STPOS) )
			{
				sprintf( errMsg, "CSV-Format Error (Number). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			/* 装置状態名称の取得 */
			if( strlen(s_sepStr.data[1]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			strncpy( (ps_STDATINF + stNumb - 1)->stName, s_sepStr.data[1], 72);

#if 0
			printf("STDATINF : ST:%3d, NM:%s\n", stNumb, (ps_STDATINF + stNumb - 1)->stName);
#endif
				
		}
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
	munmap( ps_STDATINF, sizeof(struct st_STDATINF) * PV_MAX_STPOS);
	/* DATファイルをクローズ */
	close( fd_Dat );
	/* 終了 */
	return( retCode );	
}

