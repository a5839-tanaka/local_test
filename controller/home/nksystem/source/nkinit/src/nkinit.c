/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 管理プロセス起動処理
 *
 * 主な機能
 * 		管理プロセスをデーモンと同様な形態で起動する
 */
#include <sys/select.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "pvdefine.h"
#include "pvfile.h"

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 管理プロセス起動処理
 *
 * 機能：
 * 		管理プロセス nkmanage を起動する
 *
 */ 
int	main(int argc, char *argv[])
{
	char			nkmanage[256];

	/* 管理プロセスのフルパスの設定 */
	strcpy( nkmanage, PV_PATH_EXE );
	strcat( nkmanage, PV_PROCNAME_NKMANAGE );

	/* 管理プロセスが存在する場合 */
	if( access( nkmanage, R_OK | X_OK ) == 0 )
	{
		/* フォークする */
		if( fork() == 0 )
		{
			/* 子プロセスの処理 */

			/* 管理プロセスを起動 */
			if( execl( nkmanage, nkmanage, NULL ) == -1 )
			{
				printf("PVINIT : ERROR PROCESS EXECL [ %s ]\n", nkmanage );
			}
		}
	}
	else
	{
		printf("PVINIT : ERROR MANAGE-PROCESS[ %s ] NOT FOUND!\n", nkmanage );
	}

	/* WAITなしで終了する */
	return( 0 );
}
