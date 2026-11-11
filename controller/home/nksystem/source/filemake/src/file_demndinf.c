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
 * 		DEMNDINF.csv ファイルから通信ステータス情報ファイル（DEMNDINF.dat）を生
 * 		成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_DEMNDINF_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_Dat;						/* ファイルディスクリプタ（通信ステータス情報） */
	int					fd_Tm;						/* ファイルディスクリプタ */
	int					fd_Sys;						/* ファイルディスクリプタ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePathDat[1024];		/* DATファイルパス（通信ステータス情報） */
	char				datFilePathTm[1024];		/* DATファイルパス */
	char				datFilePathSys[1024];		/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					retCode = 0;				/* 戻り値 */
	struct st_DEMNDINF	*ps_DEMNDINF;				/* 需給バランス調整情報ポインタ */
	struct st_TMDATINF	*ps_TMDATINF;				/* 計測情報ポインタ */
	struct st_PVSYSINF	*ps_PVSYSINF;				/* 通信回線情報ポインタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcat(csvFilePath, PV_FNAME_DEMNDINF );
	strcat(csvFilePath, ".csv");

	strcpy(datFilePathDat, PV_PATH_DAT );
	strcat(datFilePathDat, PV_FNAME_DEMNDINF );
	strcat(datFilePathDat, ".dat");

	strcpy(datFilePathTm, PV_PATH_DAT );
	strcat(datFilePathTm, PV_FNAME_TMDATINF );
	strcat(datFilePathTm, ".dat");

	strcpy(datFilePathTm, PV_PATH_DAT );
	strcat(datFilePathTm, PV_FNAME_TMDATINF );
	strcat(datFilePathTm, ".dat");

	strcpy(datFilePathSys, PV_PATH_DAT );
	strcat(datFilePathSys, PV_FNAME_PVSYSINF );
	strcat(datFilePathSys, ".dat");

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
	if( ftruncate( fd_Dat, sizeof(struct st_DEMNDINF)) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_DEMNDINF = (struct st_DEMNDINF *)mmap( NULL, sizeof(struct st_DEMNDINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_DEMNDINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* DATファイル生成 */
	fd_Tm = open( datFilePathTm, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Tm == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathTm );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_DEMNDINF, sizeof(struct st_DEMNDINF));
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMDATINF = (struct st_TMDATINF *)mmap( NULL, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_Tm, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathTm );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_DEMNDINF, sizeof(struct st_DEMNDINF));
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
		close( fd_Dat );
		close( fd_Tm );
		retCode = -1;
		return( retCode );
	}

	/* DATファイル生成 */
	fd_Sys = open( datFilePathSys, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Sys == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathSys );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_DEMNDINF, sizeof(struct st_DEMNDINF));
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
		close( fd_Dat );
		close( fd_Tm );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PVSYSINF = (struct st_PVSYSINF *)mmap( NULL, sizeof(struct st_PVSYSINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Sys, 0 );
	if( ps_PVSYSINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathSys );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_DEMNDINF, sizeof(struct st_DEMNDINF));
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
		close( fd_Dat );
		close( fd_Tm );
		close( fd_Sys );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_DEMNDINF, 0, sizeof(struct st_DEMNDINF));

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		retCode = func_DEMNDINF_Check( fp, PV_CSVTYPE_NEW, ps_DEMNDINF, csvFilePath, ps_PVSYSINF, ps_TMDATINF );

		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}

	/* メモリマップを解除 */
	munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
	munmap( ps_PVSYSINF, sizeof(struct st_PVSYSINF));
	munmap( ps_DEMNDINF, sizeof(struct st_DEMNDINF));
	/* DATファイルをクローズ */
	close( fd_Tm );
	close( fd_Sys );
	close( fd_Dat );
	/* 終了 */
	return( retCode );	
}


