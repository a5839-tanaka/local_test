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
 * 共通関数：システム管理情報関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・システム管理ファイルの情報を更新する
 */
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * プロセス起動指示済みフラグセット
 *
 * 機能：
 * 		引数 procNumb に指定されたプログラム番号に対して、起動指示済みフラグをセ
 * 		ットする。
 * 		引数 startMode が PV_PROC_NEWSTART（新規起動）の場合は、他のフラグ関連の
 * 		情報は全てリセットされるが、PV_PROC_RESTART（再起動）の場合は、異常中フ
 * 		ラグと再起動カウンタはリセットされない。
 * 戻り値：
 * 		0：正常処理
 */
int commfunc_manage_execFlagSet( int procNumb, int startMode )
{
	if((procNumb < 1) || (procNumb > PV_MAX_PROC))
		return( -1 );

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_execFlagSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag  = 1;

	if( startMode == PV_PROC_NEWSTART )
	{
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].restFlag  = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag  = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].runFlag   = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].abnFlag   = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogFlag  = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogCnt   = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].reStCnt   = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].ctlInst   = 0;	/* */
	}
	else
	{
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].restFlag  = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag  = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].runFlag   = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogFlag  = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogCnt   = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID = 0;
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].ctlInst   = 0;	/* */
	}

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 全プロセス停止指示
 *
 * 機能：
 * 		起動指示済みのフラグがセットされているプロセスに対して、停止指示済みフラ
 * 		グをセットし、通常停止タイマ（カウンタ）をセットする。
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_allstop( void )
{
	int					procNumb;
	char				errMsg[PV_MSG_SIZE_LOG];

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_allstop(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 全プロセス数分繰り返す */
	for( procNumb = 1 ; procNumb <= PV_MAX_PROC ; procNumb++ )
	{
		/* 起動指示済みフラグが設定されている場合 */
		if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag )
		{
			sprintf( errMsg, "PROC:%d stopFlag SET", procNumb );
			commfunc_syslog_msgset( "commfunc_manage_allstop()", errMsg );

			/* 停止指示済みフラグをセット */
			ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag  = 1;
			/* 起動指示済みフラグをリセット */
			ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag  = 0;
			/* 再起動対象フラグをリセット */
			ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].restFlag  = 0;
		}
	}
	
	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセスの停止確認処理
 *
 * 機能：
 * 		全プロセスが停止している場合は正常停止確認の場合は１、未停止の場合は０を
 * 		返す。
 * 戻り値：
 * 		 0：正常（停止未完了）
 * 		 1：正常停止完了
 */
int	commfunc_manage_stopCheck( void )
{
	int						procNumb;
	int						retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_stopCheck(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 全プロセス分繰り返す */
	for( procNumb = 1 ; procNumb <= PV_MAX_PROC ; procNumb++ )
	{
		/* 停止指示済みフラグがセットされている場合 */
		if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag )
		{
			/* 動作中フラグがセットされている場合 */
			if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].runFlag )
			{
				/* 未停止プロセスアリ */
				break;
			}
			/* プロセスＩＤがセットされている場合 */
			if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID != 0 )
			{
				/* 未停止プロセスアリ */
				break;
			}
			/* 停止指示済みフラグをリセット */
			ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag = 0;
		}
	}
	/* 全プロセス停止していた場合 */
	if( procNumb > PV_MAX_PROC )
	{
		/* 停止確認完了 */
		retCode = 1;
	}

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 全プロセス強制停止
 *
 * 機能：
 * 		起動指示済みフラグまたは停止指示済みフラグがセットされたプロセスに対して
 * 		以下に示す処理を行う。
 * 		・起動指示済みフラグのリセット
 *		・再起動指示済みフラグのリセット
 *		・子プロセスＩＤがセットされている場合は子プロセスにSIGKILLを送信
 *		・強制停止確認タイマカウンタをセット
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_allkill( void )
{
	int					procNumb;
	char				errMsg[PV_MSG_SIZE_LOG];

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_allkill(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 全プロセス数分繰り返す */
	for( procNumb = 1 ; procNumb <= PV_MAX_PROC ; procNumb++ )
	{
		/* 次のいずれかの条件に合致している場合 */
		/* ・起動指示済みフラグがＯＮ */
		/* ・停止指示済みフラグがＯＮ */
		if( ( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag ) ||
			( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag ) )
		{
			/* 起動指示済みフラグをリセット */
			ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag  = 0;
			/* 再起動対象フラグをリセット */
			ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].restFlag  = 0;
			/* PIDがセットされているか？ */
			if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID != 0 )
			{
				sprintf( errMsg, "SIGKILL SEND PROC:%d PID:%d",
					procNumb, ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID );
				commfunc_syslog_msgset( "commfunc_manage_allkill()", errMsg );

				/* 子プロセスにＫＩＬＬシグナルを送信 */
				kill( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID, SIGKILL );
			}
		}
	}
	
	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ウォッチドッグセット処理
 *
 * 機能：
 * 		起動中のプロセスに対するウォッチドッグ処理とプロセス異常の確認。
 * 		１．ウォッチドッグ処理
 * 		起動指示済みのプロセスに対して以下の処理を行う、ただし再起動指示済みの
 * 		プロセスは除外する。
 * 		＜ウォッチドッグフラグがセットされたままの場合＞
 * 		・ウォッチドッグカウンタを＋１
 * 		・ウォッチドッグカウンタが規程値以上の場合は子プロセスがロック状態であ
 * 		　ると判断し、子プロセスの強制停止（SIGKILL送信）を行い、再起動指示済み
 * 		　フラグをセットする。
 * 		＜ウォッチドッグフラグがセットされていない場合＞
 * 		・ウォッチドッグカウンタをリセット
 * 		・ウォッチドッグフラグをセット
 * 		２．異常中確認
 * 		異常中フラグが設定されているプロセスが存在する場合、または、ウォッチド
 * 		ッグ異常を検出した場合は戻り値を1で返す。
 * 戻り値：
 * 		0：正常処理（異常中プロセスなし）
 * 		1：正常処理（異常中プロセスあり）
 */
int	commfunc_manage_watchdogSet( void )
{
	int					procNumb;
	int					retCode = 0;
	char				errMsg[PV_MSG_SIZE_LOG];
	time_t				nowTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_watchdogSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 現在時刻（積算秒）を取得 */
	nowTime = time( NULL );
	nowTime -= PV_WDOG_RES_RESET;

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 全プログラム分繰り返す */
	for( procNumb = 1 ; procNumb <= PV_MAX_PROC ; procNumb++ )
	{
		/* 異常中フラグが設定されている場合 */
		if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].abnFlag )
		{
			/* 戻り値に異常中を設定 */
			retCode = 1;
		}
		/* 起動指示済みフラグがＯＮの場合 */
		if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag )
		{
			/* 再起動指示済みフラグがセットされている場合 */
			if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].restFlag )
				continue;

			/* ウォッチドッグフラグがＯＮの場合 */
			if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogFlag )
			{
				/* ウォッチドッグカウンタを＋１ */
				ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogCnt += 1;
				/* ウォッチドッグカウンタが規程値以上になった場合 */
				if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogCnt >= PV_WDOG_LMTCNT )
				{
					/* プロセスＩＤがセットされている場合 */
					if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID )
					{
						sprintf( errMsg, "Watchdog Counter Over PROC:%d PID:%d SIGKILL SEND",
							procNumb, ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID );
						commfunc_syslog_msgset( "commfunc_manage_watchdogSet()", errMsg );

						/* 再起動指示済みフラグをセット */
						ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].restFlag = 1;
						/* 異常中フラグをセット */
						ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].abnFlag  = 1;
						/* 当該プロセスに対してSIGKILLを送信 */
						kill( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID, SIGKILL );
						/* 戻り値に異常中を設定 */
						retCode = 1;
					}
				}
			}
			else
			{
				/* ウォッチドッグカウンタをリセット */
				ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogCnt = 0;
				/* ウォッチドッグフラグをセット */
				ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogFlag = 1;
				/* プロセス起動時間がプロセスリスタートカウンタのリセット範囲の場合 */
				if( nowTime > ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].startTime )
				{
					/* プログラムリスタートカウンタをリセット */
					ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].reStCnt = 0;
				}
			}
		}
	}

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 再起動可否チェック
 *
 * 機能：
 * 		引数 procNumb で指定されたプログラム番号が再起動可能か確認する。
 * 		procNumb で指定されたプログラム番号が起動指示済みフラグがセットされてい
 * 		た場合に再起動カウンタを＋１し、再起動カウンタが既定値以内であれば再起動
 * 		可とし、既定値を越える場合は再起動不可とする。
 * 		起動指示済みフラグがセットされていない場合は、再起動不可とする。
 * 戻り値：
 * 		 0：再起動可
 * 		-1：再起動不可
 */
int commfunc_manage_reStartCheck( int procNumb )
{
	char					errMsg[PV_MSG_SIZE_LOG];

	if((procNumb < 1) || (procNumb > PV_MAX_PROC))
		return( -1 );

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_reStartCheck(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* プロセス起動指示済みフラグが未設定の場合 */
	if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].execFlag == 0 )
		return( -1 );


	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 再起動実施カウンタ＋１ */
	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].reStCnt  += 1;

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].reStCnt > PV_MAX_RESTART )
	{
		sprintf( errMsg, "PROC:%d RESTART COUNTER OVER!", procNumb );
		commfunc_syslog_msgset( "commfunc_manage_reStartCheck()", errMsg );

		return( -1 );
	}

	sprintf( errMsg, "PROC:%d RESTART OK!", procNumb );
	commfunc_syslog_msgset( "commfunc_manage_reStartCheck()", errMsg );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセスＩＤ登録処理
 *
 * 機能：
 * 		引数 procNumb で指定されたプログラム番号のプロセスID pidNumb を管理ファ
 * 		イルに登録する。
 * 戻り値：
 * 		0：正常処理
 */
int commfunc_manage_pidSet( int procNumb, int pidNumb )
{
	if((procNumb < 1) || (procNumb > PV_MAX_PROC))
		return( -1 );

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_pidSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* プロセスＩＤをセット */
	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].processID = pidNumb;

	/* プロセス起動時刻をセット */
	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].startTime = time( NULL );

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( 0 );
}
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス名称登録処理
 *
 * 機能：
 * 		引数 procNumb で指定されたプログラム番号のプロセス名称 procName を管理
 *		ファイルに登録する。
 * 戻り値：
 * 		0：正常処理
 */
int commfunc_manage_procNameSet( int procNumb, char *procName )
{
	if((procNumb < 1) || (procNumb > PV_MAX_PROC))
		return( -1 );

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_procNameSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* プロセス名称をセット */
	memset( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].procName, 0, sizeof(ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].procName) );
	strncpy( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].procName, procName, 8 );

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( 0 );
}
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 子プロセス停止（停止ＰＩＤ受信）処理
 *
 * 機能：
 * 		引数 pidNumb で指定されたプロセスＩＤがシステム管理ファイル上に存在する
 * 		か確認する。
 * 		該当するプロセスＩＤがある場合は、引数 procNumb に該当するプログラム番号
 * 		をセットした後、以下の処理を行う。
 * 		＜共通処理＞
 * 		・プロセスＩＤ情報のリセット
 * 		＜停止指示済みプロセスの場合＞
 * 		・停止指示済みフラグをリセット
 * 		・プロセスＩＤをリセット
 * 		＜停止指示が出ていないプロセスの場合＞
 * 		・再起動カウンタを
 * 戻り値：
 * 		 0：正常処理（正常停止）
 * 		 1：ＫＩＬＬによる停止または異常停止のため当該プロセスの再起動が必要
 * 		-1：異常終了（想定外の異常または該当するプロセスが存在しない）
 */
int commfunc_manage_stopProc( int *procNumb, int pidNumb )
{
	int						retCode = 0;
	int						loopCnt;
	char					errMsg[PV_MSG_SIZE_LOG];

	/* プログラム番号を初期化 */
	*procNumb = 0;
	
	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_stopProc(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	for(loopCnt = 1 ; loopCnt <= PV_MAX_PROC ; loopCnt++)
	{
		/* プロセスＩＤがシステム管理ファイル上のものと一致した場合 */
		if( ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].processID == pidNumb )
		{
			/* プログラム番号をセット */
			*procNumb = loopCnt;

			/* プロセスＩＤをリセット */
			ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].processID = 0;

			/* 停止指示済みの場合 */
			if( ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].stopFlag )
			{
				sprintf( errMsg, "PROC:%d PID:%d [NORMAL-STOP:OK]", loopCnt, pidNumb );
				commfunc_syslog_msgset( "commfunc_manage_stopProc()", errMsg );

				/* 停止設定 */
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].execFlag  = 0;
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].restFlag  = 0;
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].stopFlag  = 0;
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].runFlag   = 0;
				/* 戻り値に正常をセット */
				retCode = 0;
			}
			/* 再起動指示済み */
			/* （ウォッチドッグでプロセスロックと判定され、SIGKILLが送信されたプロセスの場合） */
			else if( ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].restFlag )
			{
				sprintf( errMsg, "PROC:%d PID:%d [SIGKILL-STOP:RESTART]", loopCnt, pidNumb );
				commfunc_syslog_msgset( "commfunc_manage_stopProc()", errMsg );
				/* 再起動指示済みフラグをリセット */
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].restFlag  = 0;
				/* 戻り値に要プロセス再起動をセット */
				retCode = 1;
			}
			/* 起動指示済み（停止指示をしていない場合） */
			else if( ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].execFlag )
			{
				sprintf( errMsg, "PROC:%d PID:%d [ABNORMAL-STOP:RESTART]", loopCnt, pidNumb );
				commfunc_syslog_msgset( "commfunc_manage_stopProc()", errMsg );
				/* 異常中を設定 */
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].abnFlag   = 1;
				/* 戻り値にプロセス再起動をセット */
				retCode = 1;
			}
			/* 上記以外の場合 */
			/* （想定外の異常） */
			else
			{
				sprintf( errMsg, "PROC:%d PID:%d [ERROR-STOP]", loopCnt, pidNumb );
				commfunc_syslog_msgset( "commfunc_manage_stopProc()", errMsg );
				/* 異常中を設定 */
				ps_gpt_PVSYSINF->s_procInfo[loopCnt - 1].abnFlag   = 1;
				/* 異常終了をセット（プロセスの再起動が必要） */
				retCode = -1;
			}
			/* ループを抜ける */
			break;
		}
	}
	/* 該当するプロセスＩＤが見つからなかった場合 */
	if( loopCnt > PV_MAX_PROC )
	{
		sprintf( errMsg, "PROC Not Found PID:%d", pidNumb );
		commfunc_syslog_msgset( "commfunc_manage_stopProc()", errMsg );
	}

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );

	return( retCode );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス動作中設定／解除処理
 *
 * 機能：
 * 		引数 procNumb で指定されたプログラム番号について、システム管理ファイルの
 * 		プロセス動作中フラグを引数 flagSet でセットする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_runFlagSet( int procNumb, int flagSet )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_runFlagSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 動作中フラグをセットする */
	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].runFlag = flagSet;

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ウォッチドッグリセット処理
 *
 * 機能：
 * 		引数 procNumb で指定されたプログラム番号について、システム管理ファイルの
 * 		ウォッチドッグフラグをリセットする。
 * 		また、停止指示済みフラグを確認し、停止指示フラグがセットされていた場合は
 * 		戻り値として -1 を戻り値として呼び出し元に返す。
 *
 * 戻り値：
 * 		0：正常処理
 * 		-1：当該プロセスに停止指示が出されている
 */
int	commfunc_manage_watchdogReset( int procNumb )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_watchdogReset(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* nkmanageの場合 */
	if( procNumb == 0 )
	{
		/* 処理しない */
		return 0;
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* ウォッチドッグフラグをリセットする */
	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].wdogFlag = 0;

	/* 停止指示済みフラグがセットされていた場合 */
	if( ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].stopFlag )
	{
		/* プロセス動作中フラグをリセット */
		ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].runFlag = 0;
		/* プロセス停止を返す */
		retCode = -1;
	}

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス異常中設定／解除処理
 *
 * 機能：
 * 		引数 procNumb で指定されたプログラム番号について、システム管理ファイルの
 * 		プロセス異常中フラグを引数 flagSet の値でセットする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_abnFlagSet( int procNumb, int flagSet )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_abnFlagSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* プロセス異常中フラグをセットする */
	ps_gpt_PVSYSINF->s_procInfo[procNumb - 1].abnFlag = flagSet;

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス異常中確認処理
 *
 * 機能：
 * 		システム管理ファイルのプロセス異常中フラグを確認する。
 *		通信プロセスと業務プロセスの異常中フラグを確認し、全てのプロセスが正常
 *		の場合に 0（正常）を返し、１つ以上のプロセスが異常の場合は 1（異常）を
 *		返す
 *
 * 戻り値：
 * 		0：正常（異常発生中プロセスなし）
 * 		1：異常（異常発生中プロセスあり）
 */
int	commfunc_manage_abnFlagCheck( void )
{
	int					loopCnt1;
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_abnFlagReset(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 全プロセス分確認する */
	for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_PROC ; loopCnt1++ )
	{
		/* プロセス異常中フラグを確認する */
		if( ps_gpt_PVSYSINF->s_procInfo[loopCnt1].abnFlag != 0 )
		{
			retCode = 1;
			break;
		}
	}

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ送信ロック設定／解除処理
 *
 * 機能：
 * 		システム管理ファイルのデータ送信ロック中フラグを引数 flagSet の値でセッ
 *		トする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_dtsdLockFlagSet( int flagSet )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_dtsdLockFlagSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* データ送信ロック中フラグをセットする */
	ps_gpt_PVSYSINF->dtsdLock = flagSet;
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ送信ロック状態取得処理
 *
 * 機能：
 * 		システム管理ファイルのデータ送信ロック中フラグの状態を取得する。
 *
 * 戻り値：
 * 		0：平常状態
 *		1：ロック中
 */
int	commfunc_manage_dtsdLockFlagGet( void )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_dtsdLockFlagGet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* データ送信ロック中フラグの状態を取得する */
	retCode = ps_gpt_PVSYSINF->dtsdLock;
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変ロック設定／解除処理
 *
 * 機能：
 * 		システム管理ファイルの状変ロック中フラグを引数 flagSet の値でセットする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_johenLockFlagSet( int flagSet )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_johenLockFlagSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変ロック中フラグをセットする */
	ps_gpt_PVSYSINF->johenLock = flagSet;
	
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変ロック状態取得処理
 *
 * 機能：
 * 		システム管理ファイルの状変ロック中フラグの状態を取得する。
 *
 * 戻り値：
 * 		0：平常状態
 *		1：ロック中
 */
int	commfunc_manage_johenLockFlagGet( void )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_johenLockFlagGet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変ロック中フラグの状態を取得する */
	retCode = ps_gpt_PVSYSINF->johenLock;

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＳＦＴＰ送信状態設定処理
 *
 * 機能：
 * 		システム管理ファイルのＳＦＴＰ送信状態を引数 sftpStatus および sftpFiles
 *		の値を元にセットする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_sftpSndStsSet( int sftpStatus, int sftpFiles )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_sftpSndStsSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ＳＦＴＰ送信状態をセットする */
	if( sftpStatus == PV_FLG_STS_SET )
	{
		/* 送信ファイル数を負値（エラー状態）で設定 */
		ps_gpt_PVSYSINF->sftpStsChk = 0x80 | sftpFiles;
	}
	else
	{
		/* 送信ファイル数を設定 */
		ps_gpt_PVSYSINF->sftpStsChk = sftpFiles;
	}

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＳＦＴＰ送信状態取得処理
 *
 * 機能：
 * 		システム管理ファイルのＳＦＴＰ送信状態を取得する。
 *
 * 戻り値：
 *		0<：正常送信（送信済みファイル数）
 * 		0 ：送信未実施
 *		0>：送信エラー（送信失敗ファイル数）
 */
int	commfunc_manage_sftpSndStsGet( void )
{
	unsigned char				workData;
	int							retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_sftpSndStsGet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ＳＦＴＰ送信状態を取得する */
	workData = (unsigned char)ps_gpt_PVSYSINF->sftpStsChk;

	/* ファイル数を取得 */
	retCode = (int)(workData & (unsigned char)0x7F);

	/* 送信結果がエラーの場合ファイル数を負値にする */
	if( workData & (unsigned char)0x80 )
		retCode *= -1;

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 手動停止扱い設定処理
 *
 * 機能：
 * 		システム管理ファイルの手動停止扱い状態をセットする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_manualStopSet( int status )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_manualStopSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 手動停止扱いをセットする */
	if( status == 0 )
		ps_gpt_PVSYSINF->manualStop = 0;
	else
		ps_gpt_PVSYSINF->manualStop = 1;

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 手動停止扱い状態取得処理
 *
 * 機能：
 * 		システム管理ファイルの手動停止扱いの状態を取得する。
 *
 * 戻り値：
 * 		0 ：設定無し
 *		1 ：手動停止扱い設定
 */
int	commfunc_manage_manualStopGet( void )
{
	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_manualStopGet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 手動停止扱いの状態を取得する */
	if( ps_gpt_PVSYSINF->manualStop == 0 )
		return( 0 );
	else
		return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ルータリセット設定処理
 *
 * 機能：
 * 		システム管理ファイルのルータリセット要求状態をセットする。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_routerResetSet( int status )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_routerResetSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ルータリセット要求をセットする */
	if( status == 0 )
		ps_gpt_PVSYSINF->routerReset = 0;
	else
		ps_gpt_PVSYSINF->routerReset = 1;

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ルータリセット状態取得処理
 *
 * 機能：
 * 		システム管理ファイルのルータリセット要求状態を取得する。
 *		状態の取得と同時に要求状態をリセットする。
 *
 * 戻り値：
 * 		0 ：設定無し
 *		1 ：設定あり
 */
int	commfunc_manage_routerResetGet( void )
{
	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_routerResetGet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状態を取得する */
	if( ps_gpt_PVSYSINF->routerReset == 0 )
	{
		ps_gpt_PVSYSINF->routerReset = 0;
		return( 0 );
	}
	else
	{
		ps_gpt_PVSYSINF->routerReset = 0;
		return( 1 );
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 制御指示設定処理
 *
 * 機能：
 * 		引数 procNumb で指定されたシステム管理ファイルの制御指示状態を
 *		引数 status の値に設定する。
 *
 * 戻り値：
 * 		0：正常処理
 */
int	commfunc_manage_ctlInstSet( int procNumb, int status )
{
	int					retCode = 0;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_crlInstSet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ミューテックスをロック */
	(void)commfunc_mutex_lock( PV_FILENO_PVSYSINF );

	/* 制御指示フラグを設定する */
	if( status == 0 )
		ps_gpt_PVSYSINF->s_procInfo[ procNumb - 1 ].ctlInst = 0;
	else
		ps_gpt_PVSYSINF->s_procInfo[ procNumb - 1 ].ctlInst = 1;

	/* ミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSINF );
	
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 制御指示解除処理
 *
 * 機能：
 * 		引数 procNumb で指定されたシステム管理ファイルの制御指示状態を取得する。
 *		（状態確認のみの処理のためミューテックはロックしない）
 *
 * 戻り値：
 * 		0 ：設定無し
 *		1 ：設定あり
 */
int	commfunc_manage_ctlInstGet( int procNumb )
{
	int			retCode;					/* 戻り値 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_manage_crlInstGet(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状態を取得する */
	if( ps_gpt_PVSYSINF->s_procInfo[ procNumb - 1 ].ctlInst == 0 )
		retCode = 0;
	else
		retCode = 1;

	/* 設定状態 */
	return( retCode );
}
