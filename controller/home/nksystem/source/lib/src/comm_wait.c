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
 * 共通関数：処理待ち関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・指定された秒数（区切り秒）に到達するまで処理待ちを行う
 * 		・処理待ちの間、最低でも１分以内にウォッチドッグの応答処理を行う
 */
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 処理待ち待機
 *
 * 機能：
 * 		（１）処理待ち
 * 		引数 waitTime で指定された秒の区切り時刻まで待機する。
 * 		区切り時刻とは 0秒を起点とし、引数 waitTime秒 の倍数の時刻とする。
 * 		waitTime が60秒より大きい場合（原則として60より大きい値は300秒のみ）は、
 *		毎正時0,5,10,15,20,25,30,35,40,45,50,55分00秒を区切り時間とする。
 * 		例） waitTime が 10、現在時刻が 12:22:32 の場合、12:22:40 が区切り時刻。
 * 		例） waitTime が 20、現在時刻が 14:59:41 の場合、15:00:00 が区切り時刻。
 * 		例） waitTime が 300、現在時刻が 17:23:41 の場合、17:25:00 が区切り時刻。
 *
 * 		（２）ウォッチドッグ処理
 * 		管理プロセス nkmanage が管理ファイル（PVSYSINF.dat）上に設定したウォッチ
 * 		ドッグフラグをリセットする。
 * 		リセットするウォッチドッグフラグは引数 procNumb で指定されたプログラム番
 * 		号のウォッチドッグフラグとする。
 *
 * 		（３）プロセス停止通知確認処理
 * 		管理プロセス nkmanage が管理ファイル（PVSYSINF.dat）上にプロセス停止指示
 * 		済フラグを設定しているか確認する。
 * 		引数 procNumb で指定されたプログラム番号に対してプロセス停止指示済フラグ
 * 		が設定されている場合は戻り値に -1 をセットする。
 *
 *		（４）制御要求指示フラグ [20151116] Add
 * 		管理ファイル（PVSYSINF.dat）上に制御指示フラグが設定されているか確認する。
 *		制御指示フラグが設定されている場合は、当該フラグをリセットする。
 * 		引数 procNumb で指定されたプログラム番号に対して制御指示フラグが設定され
 *		ている場合は戻り値に 1 をセットする。
 *
 * 戻り値：
 * 		 0：正常処理（処理待ち終了）
 * 		 1：正常処理（制御指示で処理待ち終了）
 * 		-1：プロセス停止指示あり
 */
int	commfunc_wait(int waitTime, int procNumb )
{
	int							wdogFlg = 0;			/* ウォッチドッグ処理フラグ */
	int							retCode;				/* 戻り値 */
	time_t						nextSec;				/* 次回処理時刻（秒） */
	time_t						nextMin;				/* 次回処理時刻（分） */
	time_t						chekSec;				/* 確認時刻（秒） */
	time_t						nowTime;				/* 現在時刻（積算秒） */
	time_t						oldTime;				/* 旧時刻（積算秒） */
	time_t						cntTime;				/* 積算秒 */
	time_t						wrkTime;				/* 次回処理時刻（積算秒） */
	struct timespec				sleepTm;				/* nanosleep用 */

	/* 積算秒をリセット */
	cntTime = 0;
	/* 旧時刻をリセット */
	oldTime = 0;

	/* 0.010000000sec(10msec) 待ち設定 */
	sleepTm.tv_sec = 0;
	sleepTm.tv_nsec = 100000000;

	/* 処理開始時の時刻を取得 */
	nowTime = time(NULL);
	wrkTime = nowTime + waitTime;
	/* 処理再開時刻（秒）を設定 */
	nextSec = (wrkTime % 60) / waitTime * waitTime;
	/* 処理再開時刻（分）を設定 */
	nextMin = (wrkTime / 60 ) % 60;
	if( waitTime > 60 )
		nextMin = (nextMin / (waitTime / 60)) * (waitTime / 60);

	/* 待ち時間調整用情報の設定 */
	if(nextSec == 0)
		chekSec = 60;
	else
		chekSec = nextSec;

	/* 待ち単位が３秒以上の場合はウォッチドッグ処理を行う */
	if( waitTime >= 3 )
	{
		/* ウォッチドッグ処理フラグＯＮ */
		wdogFlg = 1;
		/* ウォッチドッグリセット */
		retCode = commfunc_manage_watchdogReset( procNumb );
		if( retCode < 0 ) return( -1 );
	}

	/* 処理再開時刻まで待つ */
	while( 1 )
	{
//		/* 制御指示フラグが設定されている場合 */
//		if( commfunc_manage_ctlInstGet( procNumb ) == 1 )
//		{
//			/* 制御指示フラグをリセット */
//			commfunc_manage_ctlInstSet( procNumb, 0 );
//			/* 制御指示有りで処理待ちを解除 */
//			return( 1 );
//		}

		/* 現在時刻と旧時刻が異なる場合は旧時刻を保存 */
		if( oldTime != nowTime ) oldTime = nowTime;

		/* 現在時刻を取得 */
		nowTime = time(NULL);
		/* 指定された単位でウォッチドッグ処理を行う */
		if( (nowTime % PV_WDOG_CYCLE) == 0 )
		{
			/* ウォッチドッグ処理フラグＯＦＦの場合 */
			if( wdogFlg == 0 )
			{
				/* ウォッチドッグ処理フラグＯＮ */
				wdogFlg = 1;
				/* ウォッチドッグリセット */
				retCode = commfunc_manage_watchdogReset( procNumb );
				if( retCode < 0 )
					return( -1 );
			}
		}
		else
		{
			/* ウォッチドッグ処理フラグＯＦＦ */
			wdogFlg = 0;
		}
		/* 処理待ち時刻（秒）が一致？ */
		if( (nowTime % 60) >= nextSec )
		{
			/* 処理待ち時刻（分）が一致 */
			if( nextMin == ((nowTime / 60) % 60 ) )
			{
				/* 処理待ち時間到来！ */
				break;
			}
		}

		/* 現在時刻と旧時刻の差分を積算時間に加算（時刻補正を考慮する） */
		if( nowTime > oldTime )
			cntTime += nowTime - oldTime;
		else if ( nowTime < oldTime )
			cntTime += oldTime - nowTime;

		/* 積算時間が設定時の区切り時間を超過した場合 */
		if( cntTime > waitTime )
		{
			/* 分に跨る時刻補正が行われた場合の対策として */
			/* 本来の処理待ち時間ではないが強制的に処理待ちから抜ける */
			break;
		}

		/* 処理待ち時刻まで2秒以上ある場合 */
		if( (chekSec - (nowTime % 60)) > 1 )
		{
			/* 0.500000000sec(500msec) 待ち設定 */
			sleepTm.tv_nsec = 500000000;
		}
		else
		{
			/* 0.010000000sec(10msec) 待ち設定 */
			sleepTm.tv_nsec = 10000000;
		}
		/* 若干待機 */
		nanosleep( &sleepTm, NULL);
	}
	
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 処理待ち待機
 *
 * 機能：
 * 		（１）処理待ち
 * 		引数 waitTime で指定されたミリ秒の区切り時刻まで待機する。
 * 		区切り時刻とは 0ミリ秒を起点とし、引数 waitTimeミリ秒 の倍数の時刻とする。
 * 		waitTime が1000ミリ秒より大きい場合は、commfunc_wait関数をコールする。
 *
 * 		（２）ウォッチドッグ処理
 * 		管理プロセス nkmanage が管理ファイル（PVSYSINF.dat）上に設定したウォッチ
 * 		ドッグフラグをリセットする。
 * 		リセットするウォッチドッグフラグは引数 procNumb で指定されたプログラム番
 * 		号のウォッチドッグフラグとする。
 *
 * 		（３）プロセス停止通知確認処理
 * 		管理プロセス nkmanage が管理ファイル（PVSYSINF.dat）上にプロセス停止指示
 * 		済フラグを設定しているか確認する。
 * 		引数 procNumb で指定されたプログラム番号に対してプロセス停止指示済フラグ
 * 		が設定されている場合は戻り値に -1 をセットする。
 *
 *		（４）制御要求指示フラグ 
 * 		管理ファイル（PVSYSINF.dat）上に制御指示フラグが設定されているか確認する。
 *		制御指示フラグが設定されている場合は、当該フラグをリセットする。
 * 		引数 procNumb で指定されたプログラム番号に対して制御指示フラグが設定され
 *		ている場合は戻り値に 1 をセットする。
 *
 * 戻り値：
 * 		 0：正常処理（処理待ち終了）
 * 		 1：正常処理（制御指示で処理待ち終了）
 * 		-1：プロセス停止指示あり
 */
int	commfunc_mswait(int waitTime, int procNumb )
{
	int							retCode;				/* 戻り値 */
	struct timeval 				nowTime;				/* 現在時刻（積算秒） */
	struct timespec				sleepTm;				/* nanosleep用 */
	long						waitTime_ms;

	if( waitTime > 1000 )
	{
		retCode = commfunc_wait((waitTime/1000), procNumb );
		return retCode; 
	}
	sleepTm.tv_sec = 0;

	waitTime_ms = waitTime*1000;
	/* 処理開始時の時刻を取得 */
	gettimeofday(&nowTime, NULL);

	/* 処理再開時刻（秒、nsec）を設定 */
	if( waitTime >= 1000 )
	{
		sleepTm.tv_nsec = 1000000000 - (nowTime.tv_usec*1000);
	}
	else
	{
		sleepTm.tv_nsec = ((nowTime.tv_usec + waitTime_ms) / waitTime_ms) * waitTime_ms * 1000;
		sleepTm.tv_nsec -= (nowTime.tv_usec * 1000);
	}

//	/* 制御指示フラグが設定されている場合 */
//	if( commfunc_manage_ctlInstGet( procNumb ) == 1 )
//	{
//		/* 制御指示フラグをリセット */
//		commfunc_manage_ctlInstSet( procNumb, 0 );
//		/* 制御指示有りで処理待ちを解除 */
//		return( 1 );
//	}

	/* 指定された単位でウォッチドッグ処理を行う */
	if( (nowTime.tv_sec % PV_WDOG_CYCLE) == 0 )
	{
		/* ウォッチドッグリセット */
		retCode = commfunc_manage_watchdogReset( procNumb );
		if( retCode < 0 )
			return( -1 );
	}

	/* 待機 */
	nanosleep( &sleepTm, NULL);
	
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 処理待ち待機
 *
 * 機能：
 * 		（１）処理待ち
 * 		引数 waitTime で指定されたミリ秒の区切り時刻まで待機する。
 * 		区切り時刻とは 0ミリ秒を起点とし、引数 waitTimeミリ秒 の倍数の時刻とする。
 * 		waitTime が1000ミリ秒より大きい場合は、commfunc_wait関数をコールする。
 *
 *
 * 戻り値：
 * 		 0：正常処理（処理待ち終了）
 * 		 1：正常処理（制御指示で処理待ち終了）
 * 		-1：プロセス停止指示あり
 */
int	commfunc_mswait2(int waitTime )
{
	int							retCode;				/* 戻り値 */
	struct timeval 				nowTime;				/* 現在時刻（積算秒） */
	struct timespec				sleepTm;				/* nanosleep用 */
	long						waitTime_ms;

	sleepTm.tv_sec = 0;

	waitTime_ms = waitTime*1000;
	/* 処理開始時の時刻を取得 */
	gettimeofday(&nowTime, NULL);

	/* 処理再開時刻（秒、nsec）を設定 */
	if( waitTime >= 1000 )
	{
		sleepTm.tv_nsec = 1000000000 - (nowTime.tv_usec*1000);
	}
	else
	{
		sleepTm.tv_nsec = ((nowTime.tv_usec + waitTime_ms) / waitTime_ms) * waitTime_ms * 1000;
		sleepTm.tv_nsec -= (nowTime.tv_usec * 1000);
	}

	/* 待機 */
	nanosleep( &sleepTm, NULL);
	
	return( 0 );
}
