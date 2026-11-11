/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
/*------------------------------------------------------------------------------
 * [20251027] 2025.10.27
 * ・外部機器から取得したスケジュール値を取り込むように修正
 * [20161201] 2016.12.01
 * ・出力制御機能に対応
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
 * スケジュール管理情報ファイル生成処理
 *
 * 機能：
 * 		スケジュール管理情報ファイル（SCHCTLFIL.dat）が存在しない場合は自動生成する。
 * 		ファイルが既に存在している場合は、既存の情報を維持するため何もしない。
 * 		（ただし、ファイルサイズが異なる場合はファイルサイズの修正を行う）
 *
 */
int	func_SCHCTLFIL_Create( void )
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
/* [20251027] Del */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	int					retCode = 0;				/* 戻り値 */
	int					fileSize = 0;				/* ファイルサイズ */
	struct st_SCHCTLFIL	*ps_SCHCTLFIL;				/* ポインタ */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_SCHCTLFIL );
	strcat(datFilePath, ".dat");
/* [20251027] Del */

	fileSize = sizeof(struct st_SCHCTLFIL) * PV_SCH_TYPE_MAX;

	/* ファイルが存在しているか確認 */
	if( access( datFilePath, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}

/* [20251027] Del */

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
	if( ftruncate( fd, fileSize ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_SCHCTLFIL = (struct st_SCHCTLFIL *)mmap( NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_SCHCTLFIL == MAP_FAILED)
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
		memset( ps_SCHCTLFIL, 0, fileSize );
	}

	/* メモリマップを解除 */
	munmap( ps_SCHCTLFIL, fileSize );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

