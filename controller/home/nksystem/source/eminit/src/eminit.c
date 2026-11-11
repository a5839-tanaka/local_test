/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 管理プロセス起動処理
 *
 * 主な機能
 * 		管理プロセスをデーモンと同様な形態で起動・停止
 */
#include <sys/select.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "embase.h"

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ２系列装置監視プロセス起動処理
 *
 * 機能：
 * 		プロセス emsystem を起動/停止する
 *
 */ 
int	main(int argc, char *argv[])
{
	char			emsystem[256];							/* 管理プロセスフルパス */
	char			chkStr[256];							/* ワーク用文字列 */
	int				retCode;								/* 戻り値 */

	if( argc != 2 )
	{
		/* コマンドライン引数が指定されていない場合 */
		printf("eminit: ERROR!! Usage: eminit start/stop\n" );

		/* 終了する */
		exit( EXIT_FAILURE );
	}
		
	/* 引数が start の場合 */
	if( 0 == strcmp( "start", argv[1] ) )
	{
		/* 管理プロセスが既に起動されている場合 */
		sprintf( chkStr, "/bin/pgrep %s", EMBASE_PROCNAME_EMSYSTEM );
		if( (retCode = system( chkStr )) == 0 )
		{
			/* 多重起動の場合 */
			printf("eminit: ERROR!! Process Already Executed! [ %s ]\n", EMBASE_PROCNAME_EMSYSTEM );
	
			/* 終了する */
			exit( EXIT_FAILURE );
		}

		/* 管理プロセスのフルパスの設定 */
		sprintf( emsystem, "%s%s", EMBASE_FPATH_SBIN, EMBASE_PROCNAME_EMSYSTEM );

		/* 管理プロセスが存在する場合 */
		if( access( emsystem, R_OK | X_OK ) == 0 )
		{
			/* フォークする */
			if( fork() == 0 )
			{
				/* 子プロセスの処理 */

				/* 管理プロセスを起動 */
				if( execl( emsystem, emsystem, NULL ) == -1 )
				{
					printf("eminit: ERROR!! PROCESS EXECL [ %s ]\n", emsystem );
				}
			}
		}
		else
		{
			printf("eminit: ERROR!! MANAGE-PROCESS[ %s ] NOT FOUND!\n", emsystem );
		}
	}
	/* 引数が stop の場合 */
	else if( 0 == strcmp( "stop", argv[1] ) )
	{
		/* 管理プロセスが既に起動されている場合 */
		sprintf( chkStr, "/bin/pgrep %s", EMBASE_PROCNAME_EMSYSTEM );
		if( (retCode = system( chkStr )) == 0 )
		{
			/* 停止指示ファイルを作成する */
			sprintf( chkStr, "/usr/bin/touch %s%s", EMBASE_FPATH_SONLINE, EMBASE_FNAME_STOP_FLG );
			system( chkStr );
			sprintf( chkStr, "/usr/bin/chmod 555 %s%s", EMBASE_FPATH_SONLINE, EMBASE_FNAME_STOP_FLG );
			system( chkStr );
	
			/* 終了する */
		}
	}
	/* WAITなしで終了する */
	return( 0 );
}
