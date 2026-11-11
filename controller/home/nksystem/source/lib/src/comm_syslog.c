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
 * 共通関数：システムログ関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・システムログファイル（PVSYSLOG.dat）にログを蓄積する
 * 		・システムログファイル（PVSYSLOG.dat）からログを取り出す
 */
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvserial.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログファイル格納処理
 *
 * 書式：
 * 		int commfunc_syslog_msgset(
 * 			char *pName,
 * 			char *pMesg )
 * 機能：
 * 		システムログファイル（PVSYSLOG.dat）に引数 pName のプロセス名と pMesg の
 * 		ログメッセージを保存する。
 * 		保存時には保存処理を行った時刻として積算秒の情報を付加して保存する。
 * 戻り値：
 * 		0：正常処理 ／ -1：異常終了
 * 入力：
 * 		*pName
 * 			プロセス名称（最大１２文字）
 * 		*pMesg
 * 			ログメッセージ
 */
int commfunc_syslog_msgset(
	char *pName,
	char *pMesg )
{
	char				recordMesg[PV_MSG_SIZE_LOG];
	struct timeval		nowtime;

	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSLOG == NULL )
	{
		printf("commfunc_syslog_msgset(): Error PVSYSLOG file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 記録情報を初期化 */
	memset( recordMesg, 0, PV_MSG_SIZE_LOG );
	/* プロセス名をコピー */
	strncpy( recordMesg, pName, PV_MSG_SIZE_LOG - 1 );
	strcat( recordMesg, ": " );
	/* メッセージをコピー */
	strncat( recordMesg, pMesg, PV_MSG_SIZE_LOG - strlen(recordMesg) );

	/* システムログファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_PVSYSLOG );

	/* システムログファイルの最終レコードにポインタがある場合 */
	if( ps_gpt_PVSYSLOG->curPoint >= (PV_MAX_LOG-1) )
		/* ポインタを先頭レコードにセット */
		ps_gpt_PVSYSLOG->curPoint = 0;
	else
		/* ポインタを+1 */
		ps_gpt_PVSYSLOG->curPoint += 1;
	
	/* システムログの処理ポインタが処理済みポインタに追いついた場合 */
	if( ps_gpt_PVSYSLOG->curPoint == ps_gpt_PVSYSLOG->outPoint )
	{
		/* 処理済みポインタを+1 */
		ps_gpt_PVSYSLOG->outPoint += 1;
		/* 処理済みポインタが最終レコードを超過した場合 */
		if(ps_gpt_PVSYSLOG->outPoint >= PV_MAX_LOG )
		{
			/* 処理済みポインタを先頭レコードにセット */
			ps_gpt_PVSYSLOG->outPoint = 0;
		}
	}

	/* 現在時刻を取得 */
	gettimeofday( &nowtime, NULL );

	/* 状変情報を記録 */
	ps_gpt_PVSYSLOG->s_logData[ps_gpt_PVSYSLOG->curPoint].sec      = nowtime.tv_sec;
	ps_gpt_PVSYSLOG->s_logData[ps_gpt_PVSYSLOG->curPoint].uSec     = nowtime.tv_usec;
	strncpy( ps_gpt_PVSYSLOG->s_logData[ps_gpt_PVSYSLOG->curPoint].logMsg, recordMesg, PV_MSG_SIZE_LOG );

	/* システムログファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSLOG );
	
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログメッセージ取得処理
 *
 * 書式：
 * 		int commfunc_syslog_msgget(
 * 			char (*logMsg)[PV_MSG_SIZE_LOG] )
 * 機能：
 * 		logMsg で指定された二次元文字配列に蓄積されたメッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（メッセージ数） ／ -1：異常終了
 * 入力：
 * 		logMsg
 * 			メッセージを格納する文字配列（二次元配列）
 */
int commfunc_syslog_msgget(
	char (*logMsg)[PV_MSG_SIZE_LOG] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		logTime;

	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSLOG == NULL )
	{
		printf("commfunc_syslog_msgget(): Error PVSYSLOG file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* システムログファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_PVSYSLOG );

	/* 処理済みポインタがシステムログ処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_PVSYSLOG->curPoint != ps_gpt_PVSYSLOG->outPoint )
	{
		/* システムログファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_PVSYSLOG->outPoint >= (PV_MAX_LOG-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_PVSYSLOG->outPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_PVSYSLOG->outPoint += 1;
	
		/* 状変時刻を取得 */
		logTime.tv_sec  = ps_gpt_PVSYSLOG->s_logData[ps_gpt_PVSYSLOG->outPoint].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &logTime.tv_sec, &localTime );

		/* メッセージを生成 */
		sprintf( logMsg[msgCnt], "%04d/%02d/%02d %02d:%02d:%02d.%06d %s",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			ps_gpt_PVSYSLOG->s_logData[ps_gpt_PVSYSLOG->outPoint].uSec,
			ps_gpt_PVSYSLOG->s_logData[ps_gpt_PVSYSLOG->outPoint].logMsg );

		msgCnt++;
	}
	/* システムログファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_PVSYSLOG );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログメッセージ取得処理（デバッガ用）
 *
 * 書式：
 * 		int commfunc_syslog_msgget(
 *			int tailCnt,
 * 			char (*logMsg)[PV_MSG_SIZE_LOG] )
 * 機能：
 * 		logMsg で指定された二次元文字配列に蓄積されたメッセージを出力する。
 *		取得するメッセージ数は最新のメッセージから遡って tailCnt 分とする。
 * 戻り値：
 * 		0～n：正常処理（メッセージ数） ／ -1：異常終了
 * 入力：
 *		tailCnt
 *			遡って取得するメッセージ数
 * 		logMsg
 * 			メッセージを格納する文字配列（二次元配列）
 */
int commfunc_syslog_msgget_tail(
	int tailCnt,
	char (*logMsg)[PV_MSG_SIZE_LOG] )
{
	int					msgCnt = 0;
	int					curPoint;
	int					syoPoint;
	int					syoCnt = 0;
	struct tm			localTime;
	struct timeval		logTime;

	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSLOG == NULL )
	{
		printf("commfunc_syslog_msgget_tail(): Error PVSYSLOG file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* システムログファイルのミューテックスをロック */
	//commfunc_mutex_lock( PV_FILENO_PVSYSLOG );	/* [20180117] DEL 呼び出し元がデバッガなので、mutexは使わない

	/* 現在のポインタを取得する */
	curPoint = ps_gpt_PVSYSLOG->curPoint;

	/* 取得開始ポインタを設定する */
	if( curPoint >= tailCnt )
		syoPoint = curPoint - tailCnt;
	else
		syoPoint = PV_MAX_LOG - tailCnt + curPoint;

	/* 処理済みポインタがシステムログ処理ポインタと同じになるまで処理を繰り返す */
	/* かつ、編集したログメッセージ数が指定された数未満の場合に処理を行う */
	while( (( curPoint != syoPoint ) && ( syoCnt < tailCnt )) || (( tailCnt == PV_MAX_LOG ) && ( syoCnt == 0 ) && ( curPoint == syoPoint ))  )
	{
		/* システムログファイルの最終レコードにポインタがある場合 */
		if( syoPoint >= (PV_MAX_LOG-1) )
			/* ポインタを先頭レコードにセット */
			syoPoint = 0;
		else
			/* ポインタを+1 */
			syoPoint += 1;
	
		/* 状変時刻を取得 */
		logTime.tv_sec  = ps_gpt_PVSYSLOG->s_logData[syoPoint].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &logTime.tv_sec, &localTime );

		if( ps_gpt_PVSYSLOG->s_logData[syoPoint].sec == 0 )
		{
			sprintf(logMsg[msgCnt], "--/-- --:--:--.------");
		
		}
		else
		{
			/* メッセージを生成 */
			sprintf( logMsg[msgCnt], "%02d/%02d %02d:%02d:%02d.%06d %s",
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				(ps_gpt_PVSYSLOG->s_logData[syoPoint].uSec),
				ps_gpt_PVSYSLOG->s_logData[syoPoint].logMsg );
		}

		msgCnt++;
		syoCnt++;
	}
	/* システムログファイルのミューテックスをアンロック */
	//commfunc_mutex_unlock( PV_FILENO_PVSYSLOG );	/* [20180117] DEL 呼び出し元がデバッガなので、mutexは使わない
	
	return( msgCnt );
}
