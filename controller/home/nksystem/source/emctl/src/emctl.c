/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置モード変更ツール
 *
 * 主な機能
 *		引数で指定された操作を管理プロセスにメッセージで送信する
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
 * ２系列装置監視プロセス：装置モード変更
 *
 * 機能：
 * 		引数で指定された指示内容をプロセス emsystem にメッセージで通知する
 * 備考：
 *		指示内容が実行されるかは、 emsystem プロセスが動作状況に合わせて判断する
 *		ので、指示内容が必ず実行されるとは限らない
 *
 */ 
int	main(int argc, char *argv[])
{
	int				retCode;								/* 戻り値 */
	int				msgKind = 0;							/* メッセージ種別 */

	if( argc != 2 )
	{
		/* コマンドライン引数が指定されていない場合 */
		printf("emctl: ERROR!! Usage: emctl <command>\n" );
		printf("   <command>\n");
		printf("      wait:         自系に自系モード「準備」を指示\n");
		printf("      stop:         自系に自系モード「停止」を指示\n");
		printf("      standby:      自系に自系モード「待機」を指示\n");
		printf("      operate:      自系に自系モード「運用」を指示\n");
		printf("      shutdown:     自系にシャットダウンを指示\n");
		printf("      par_wait:     自系に他系モード「準備」を指示\n");
		printf("      par_stop:     自系に他系モード「停止」を指示\n");
		printf("      par_standby:  自系に他系モード「待機」を指示\n");
		printf("      par_operate:  自系に他系モード「運用」を指示\n");
		printf("      par_shutdown: 自系に他系シャットダウンを指示\n");
		printf("      注意：コマンドは装置の運転状態により受け付けられない場合があります\n");

		/* 終了する */
		exit( EXIT_FAILURE );
	}

	/* 引数が以下に該当する場合 */
	if( 0 == strcmp( "wait", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：準備） */
		msgKind = EMBASE_MSG_MOD_WAIT;
	}
	else if( 0 == strcmp( "stop", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：停止） */
		msgKind = EMBASE_MSG_MOD_STOP;
	}
	else if( 0 == strcmp( "standby", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：待機） */
		msgKind = EMBASE_MSG_MOD_STANDB;
	}
	else if( 0 == strcmp( "operate", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：運用） */
		msgKind = EMBASE_MSG_MOD_OPERAT;
	}
	else if( 0 == strcmp( "par_wait", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：他系準備） */
		msgKind = EMBASE_MSG_MOD_PAR_WAIT;
	}
	else if( 0 == strcmp( "par_stop", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：他系停止） */
		msgKind = EMBASE_MSG_MOD_PAR_STOP;
	}
	else if( 0 == strcmp( "par_standby", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：他系待機） */
		msgKind = EMBASE_MSG_MOD_PAR_STANDB;
	}
	else if( 0 == strcmp( "par_operate", argv[1] ) )
	{
		/* メッセージ種別（モード遷移：他系運用） */
		msgKind = EMBASE_MSG_MOD_PAR_OPERAT;
	}
	else if( 0 == strcmp( "shutdown", argv[1] ) )
	{
		/* メッセージ種別（装置制御：シャットダウン） */
		msgKind = EMBASE_MSG_OS_SHUTDOWN;
	}
	else if( 0 == strcmp( "par_shutdown", argv[1] ) )
	{
		/* メッセージ種別（装置制御：他系シャットダウン） */
		msgKind = EMBASE_MSG_OS_PAR_SHUTDOWN;
	}
	else
	{
		/* コマンドライン引数が指定されていない場合 */
		printf("emctl: ERROR!! Command error: Usage: emctl <command>\n" );
		/* 終了する */
		exit( EXIT_FAILURE );
	}

	/* メッセージ種別が設定されている場合 */
	if( msgKind )
	{
		/* メッセージキューの取得 */
		retCode = embase_func_startup( 1 );

		if( retCode < 0 )
		{
			printf("emctl: ERROR!! [ %s ] Process Not Execute!\n", EMBASE_PROCNAME_EMSYSTEM);

			/* 終了処理 */
			embase_func_finish();
		
			/* 終了する */
			exit( EXIT_FAILURE );
		}
		
		/* メッセージを送信 */
		if( 0 != (retCode = embase_func_msgq_send( msgKind )))
		{
			/* エラー */
			printf("emctl: ERROR!! Message send error ( %d )\n",retCode );

			/* 終了処理 */
			embase_func_finish();
		
			/* 終了する */
			exit( EXIT_FAILURE );
		}

		/* 終了処理 */
		embase_func_finish();
	}

	/* WAITなしで終了する */
	return( 0 );
}
