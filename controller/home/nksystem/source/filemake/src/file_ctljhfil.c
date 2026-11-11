/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
/*------------------------------------------------------------------------------
 * [20240312] 2024.03.12
 * ・ファイル構造(サイズ)が前回と変わった場合、初期化するように修正
 * [20160329] 2016.03.29
 * ・エネマネ事業者向けＥＭＳ対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
 */
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
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
 * 制御情報蓄積ファイル生成処理
 *
 * 機能：
 * 		制御情報蓄積ファイル（CTLJHFIL.dat）が存在しない場合は自動生成する。
 * 		ファイルが既に存在している場合は、既存の情報を維持するため何もしない。
 * 		（ただし、ファイルサイズが異なる場合はファイルサイズの修正を行う）
 *
 */
int	func_CTLJHFIL_Create( )
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	int					retCode = 0;				/* 戻り値 */
	static struct stat	statInfo;					/* ファイル状態 *//* [20240312] Add */
	int					fileSize;					/* ファイルサイズ *//* [20240312] Add */

	struct st_CTLJHFIL	*ps_CTLJHFIL;				/* ポインタ */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_CTLJHFIL );
	strcat(datFilePath, ".dat");

	fileSize = sizeof(struct st_CTLJHFIL);	/* [20240312] Add */

	/* ファイルが存在しているか確認 */
	if( access( datFilePath, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}
/* [20240312] Add St */
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
/* [20240312] Add Ed */

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
	if( ftruncate( fd, sizeof(struct st_CTLJHFIL) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_CTLJHFIL = (struct st_CTLJHFIL *)mmap( NULL, sizeof(struct st_CTLJHFIL), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_CTLJHFIL == MAP_FAILED)
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
		memset( ps_CTLJHFIL, 0, sizeof(struct st_CTLJHFIL) );
	}

	/* メモリマップを解除 */
	munmap( ps_CTLJHFIL, sizeof(struct st_CTLJHFIL) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

