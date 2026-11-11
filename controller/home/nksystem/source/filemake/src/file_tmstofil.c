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
 * 計測値蓄積ファイル生成処理
 *
 * 機能：
 * 		計測値蓄積ファイル（TMSTOFIL.dat）を生成する。
 * 		本ファイルは起動毎に内容を初期化する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_TMSTOFIL_Create( )
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */
	

	struct st_TMSTOFIL	*ps_TMSTOFIL;				/* 計測値蓄積ファイルポインタ */
	int					workData;

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_TMSTOFIL );
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
	if( ftruncate( fd, sizeof(struct st_TMSTOFIL) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMSTOFIL = (struct st_TMSTOFIL *)mmap( NULL, sizeof(struct st_TMSTOFIL), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_TMSTOFIL == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの中身をALL0クリア */
	memset( ps_TMSTOFIL, 0, sizeof(struct st_TMSTOFIL) );

	/* 計測情報数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_TMDATA ; loopCnt1++ )
	{
		ps_TMSTOFIL->s_tmstsfil[loopCnt1].tmFlag    = PV_TMFLG_DROPTM;
		ps_TMSTOFIL->s_tmstsfil[loopCnt1].avltmFlag = PV_TMFLG_DROPTM;
	}

	/* メモリマップを解除 */
	munmap( ps_TMSTOFIL, sizeof(struct st_TMSTOFIL) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

