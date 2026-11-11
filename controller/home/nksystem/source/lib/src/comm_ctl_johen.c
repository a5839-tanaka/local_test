/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260518] 2026.05.18
 * ・別案件(RJ)からの不具合を水平展開
 *   ※sysdebugにて制御状変を表示中にターミナルが落ちた場合、タイミングによって
 *     mutexロックが発生し、repoeditでウォッチドックタイマが発生する
 * [20160329] 2016.03.29
 * ・エネマネ事業者向けＥＭＳ対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * エネルギーマネジメントシステム
 * -----------------------------------------------------------------------------
 * 共通関数：状変処理関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・状変蓄積ファイルに状変情報を蓄積する。
 * 		・状変蓄積ファイルから状変情報を取り出す。
 */
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvserial.h"

#define	GET_CTL_KIND(ctl_kind)		( (ctl_kind==PV_CTL_JOHEN_KIND_SET) ? "SET" : "ERR" )


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態状変処理（単独処理）
 *
 * 書式：
 * 		int	commfunc_ctl_johen_pos(
 * 			int soKind,
 * 			int soNumb,
 * 			int svNumb,
 * 			int svStat,
 * 			int userNo,
 * 			int svIfLink)
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、svNumb（状態番号）、svStat（状態情報）
 * 		の情報を元に、状変処理を行う。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		ctlKind
 * 			制御種別
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		svNumb
 * 			状態番号
 * 		svStat
 * 			状態情報
 * 		svIfLink
 * 			装置状態情報ファイルリンク情報
 */
int	commfunc_ctl_johen_pos(
	int ctlKind,								/* 制御種別 */
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int svNumb,									/* 状態番号 */
	int	svStat,									/* 装置状態 */
	int	userNo,									/* ユーザ番号 */
	int svIfLink								/* 状態情報ファイルポインタ（当該装置の先頭ポインタ） */
)
{
	/* 状変処理 */
	if( svStat )
	{
		/* 発生状変（マスク無し） */
		commfunc_ctl_johen_msgset(soKind, soNumb, ctlKind, svNumb, 1, userNo, PV_SV_KIND_NORMAL, svIfLink);

	}
	else
	{
		/* 復帰状変（マスク無し） */
		commfunc_ctl_johen_msgset(soKind, soNumb, ctlKind, svNumb, 0, userNo, PV_SV_KIND_NORMAL, svIfLink);

	}

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変蓄積ファイル格納処理
 *
 * 書式：
 * 		int commfunc_ctl_johen_msgset(
 * 			int soKind,
 * 			int soNumb,
 * 			int svstKind,
 * 			int svstNumb,
 * 			int svstStat,
 * 			int userNo,
 *			int maskFlag,
 * 			int svstIfLink )
 * 機能：
 * 		引数で指定された状態情報を状変情報として状変蓄積ファイル（JOHENFIL.dat）
 *		に保存する。
 * 		装置種別（soKind）、装置番号（soNumb）、状変種別（svstKind）、状態番号
 *		（svstNumb）、状態情報（svstStat）、状態情報リンク情報（svstIfLink）を
 *		状変蓄積ファイルに保存する。
 * 		保存時には保存処理を行った時刻として積算秒の情報を付加して保存する。
 * 戻り値：
 * 		0：正常処理 ／ -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		ctlKind
 * 			制御種別
 *		svstNumb
 * 			状態番号
 *		maskFlag
 *			状変マスクフラグ（0：マスクなし／1：マスクあり）
 * 		svstLink
 * 			状態情報リンク情報（状態情報が直接参照可能なリンク情報）
 */
int commfunc_ctl_johen_msgset(
	int soKind,
	int soNumb,
	int ctlKind,
	int svstNumb,
	int svstStat,
	int userNo,
	int	maskFlag,
	int svstIfLink )
{
	struct timeval		nowtime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_CTLJHFIL == NULL )
	{
		printf("commfunc_ctl_johen_msgset(): Error CTLJHFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_CTLJHFIL );

	/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
	if( ps_gpt_CTLJHFIL->curPoint >= PV_MAX_CTLINF )
		/* ポインタを先頭レコードにセット */
		ps_gpt_CTLJHFIL->curPoint = 0;
	else
		/* ポインタを+1 */
		ps_gpt_CTLJHFIL->curPoint += 1;
	
	/* 状変蓄積の処理ポインタがサーバ送信用処理ポインタに追いついた場合 */
	if( ps_gpt_CTLJHFIL->curPoint == ps_gpt_CTLJHFIL->tcpPoint )
	{
		/* サーバ送信用ポインタを+1 */
		ps_gpt_CTLJHFIL->tcpPoint += 1;
		/* サーバ送信用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_CTLJHFIL->tcpPoint > PV_MAX_CTLINF )
		{
			/* サーバ送信用ポインタを先頭レコードにセット */
			ps_gpt_CTLJHFIL->tcpPoint = 0;
		}
	}
	/* 状変蓄積の処理ポインタがログ出力用ポインタに追いついた場合 */
	if( ps_gpt_CTLJHFIL->curPoint == ps_gpt_CTLJHFIL->outPoint )
	{
		/* ログ出力用ポインタを+1 */
		ps_gpt_CTLJHFIL->outPoint += 1;
		/* ログ出力用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_CTLJHFIL->outPoint > PV_MAX_CTLINF )
		{
			/* ログ出力用ポインタを先頭レコードにセット */
			ps_gpt_CTLJHFIL->outPoint = 0;
		}
	}

	/* 現在時刻を取得 */
	gettimeofday( &nowtime, NULL );

	/* 状変情報を記録 */
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].sec      = nowtime.tv_sec;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].uSec     = nowtime.tv_usec;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].mKind    = soKind;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].mNumb    = soNumb;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].ctlNumb  = svstNumb;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].ctlKind  = ctlKind;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].ctlStat  = svstStat;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].ctlLink  = svstIfLink;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].maskFlag = maskFlag;
	ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->curPoint].userNo = userNo;
	
	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_CTLJHFIL );
	
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * サーバ送信用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_ctl_johen_msggettcp(
 * 			char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgTcp で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgTcp
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_ctl_johen_msggettcp( char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;
	int					ctlKind;
	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_CTLJHFIL == NULL )
	{
		printf("commfunc_ctl_johen_msggettcp(): Error CTLJHFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_CTLJHFIL );


	/* サーバ送信用ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_CTLJHFIL->curPoint != ps_gpt_CTLJHFIL->tcpPoint )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_CTLJHFIL->tcpPoint >= PV_MAX_CTLINF )
			/* ポインタを先頭レコードにセット */
			ps_gpt_CTLJHFIL->tcpPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_CTLJHFIL->tcpPoint += 1;
	
		/* 状変時刻を取得 */
		johenTime.tv_sec  = ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* 状変マスクフラグが通常状変扱いの場合 */
		if( ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].maskFlag == PV_SV_KIND_NORMAL )
		{
			/* 状変メッセージを生成 */
			sprintf( johenMsgTcp[msgCnt], "CTL,%04d%02d%02d%02d%02d%02d%06ld,%s,%d,%d,%d,%d,%d",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].uSec,
				GET_CTL_KIND(ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].ctlKind),
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].mKind,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].mNumb,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].ctlNumb,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].ctlStat,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->tcpPoint].userNo);

			msgCnt++;
		}
	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_CTLJHFIL );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IOT送信用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_johen_msggetiot(
 * 			char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgTcp で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgTcp
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_ctl_johen_msggetiot( char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_CTLJHFIL == NULL )
	{
		printf("commfunc_johen_msggettcp(): Error CTLJHFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_CTLJHFIL );

	/* IOT送信用ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_CTLJHFIL->curPoint != ps_gpt_CTLJHFIL->iotPoint )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_CTLJHFIL->iotPoint >= (PV_MAX_JOHEN-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_CTLJHFIL->iotPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_CTLJHFIL->iotPoint += 1;
	
		/* 状変時刻を取得 */
		johenTime.tv_sec  = ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* 状変マスクフラグが通常状変扱いの場合 */
		if( ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].maskFlag == PV_SV_KIND_NORMAL )
		{
	        //<例>
	        //  {"date":"2018-07-01 01:30:00","type":"SET","mkind":1,"mnumb":10,"no":100,"state":false},

			/* 状変メッセージを生成 */
			sprintf( johenMsgTcp[msgCnt], "{\"date\":\"%04d-%02d-%02d %02d:%02d:%02d.%06d\",\"type\":\"%s\",\"mkind\":%d,\"mnumb\":%d,\"no\":%d,\"state\":%s}",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].uSec,
				GET_CTL_KIND(ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].ctlKind),
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].mKind,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].mNumb,
				ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].ctlNumb,
				(ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->iotPoint].ctlStat == 1)? "true" : "false");

			msgCnt++;
		}

	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_CTLJHFIL );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログ用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_ctl_johen_msggetlog(
 * 			char (*johenMsgLog)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgLog で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgLog
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_ctl_johen_msggetlog( char (*johenMsgLog)[PV_MSG_SIZE_JHD] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_CTLJHFIL == NULL )
	{
		printf("commfunc_ctl_johen_msggetlog(): Error CTLJHFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_CTLJHFIL );

	/* ログ出力用ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_CTLJHFIL->curPoint != ps_gpt_CTLJHFIL->outPoint )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_CTLJHFIL->outPoint >= PV_MAX_CTLINF )
			/* ポインタを先頭レコードにセット */
			ps_gpt_CTLJHFIL->outPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_CTLJHFIL->outPoint += 1;
	
		/* 状変時刻を取得 */
		johenTime.tv_sec  = ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].sec;
		johenTime.tv_usec = ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_JOHENFIL->outPoint].uSec,

		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* 状変メッセージを生成 */
		sprintf( johenMsgLog[msgCnt], "%04d/%02d/%02d %02d:%02d:%02d.%06d %s [%02d,%03d,%03d] : %d %s",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			johenTime.tv_usec,
			GET_CTL_KIND(ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].ctlKind),
			ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].mKind,
			ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].mNumb,
			ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].ctlNumb,
			ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].ctlStat,
			(ps_gpt_CTLJHFIL->s_ctlData[ps_gpt_CTLJHFIL->outPoint].maskFlag == PV_SV_KIND_NORMAL)? "" : "[MASK]" );

		msgCnt++;
	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_CTLJHFIL );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 最終状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_ctl_johen_msgget_tail(
 *			int tailCnt,
 * 			char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgTcp で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 *		一番最後の状変メッセージから遡った tailCnt 分とする。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 *		tailCnt
 *			最後から遡った状変数
 * 		ps_CDATA
 * 			状変メッセージを格納する構造体ポインタ
 */
int commfunc_ctl_johen_msgget_tail( int tailCnt, struct st_CTLDATA *ps_CDATA )
{
	int					msgCnt = 0;
	int					curPoint;
	int					syoPoint;
	int					syoCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_CTLJHFIL == NULL )
	{
		printf("commfunc_ctl_johen_msgget_tail(): Error CTLJHFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	//commfunc_mutex_lock( PV_FILENO_CTLJHFIL );	/* [20260518] DEL 呼び出し元がデバッガなので、mutexは使わない

	/* 現在の処理ポインタを取得 */
	curPoint = ps_gpt_CTLJHFIL->curPoint;

	/* 処理開始ポインタを最新のポインタから tailCnt 遡った位置に設定する */
	if( (curPoint + 1) >= tailCnt )
	{
		syoPoint = curPoint - tailCnt;
	}
	else
	{
		syoPoint = PV_MAX_CTLINF - tailCnt + curPoint + 1;
		
	}

	/* 処理ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	/* かつ、編集した状変数が指定された数以下の場合に処理を行う */
	while( ( curPoint != syoPoint ) && ( syoCnt < tailCnt ) )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( syoPoint >= PV_MAX_CTLINF )
			/* ポインタを先頭レコードにセット */
			syoPoint = 0;
		else
			/* ポインタを+1 */
			syoPoint += 1;
	
		ps_CDATA->sec		 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].sec;
		ps_CDATA->uSec		 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].uSec;
		ps_CDATA->mKind		 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].mKind;
		ps_CDATA->mNumb		 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].mNumb;
		ps_CDATA->ctlKind	 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].ctlKind;
		ps_CDATA->ctlNumb	 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].ctlNumb;
		ps_CDATA->ctlStat	 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].ctlStat;
		ps_CDATA->ctlLink	 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].ctlLink;
		ps_CDATA->maskFlag	 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].maskFlag;
		ps_CDATA->userNo	 = ps_gpt_CTLJHFIL->s_ctlData[syoPoint].userNo;

//printf("commfunc_ctl_johen_msgget_tail : syoPoint=%d, [kind,Numb,Sv,Val]=[%d,%d,%d,%d]\n", syoPoint, ps_CDATA->mKind, ps_CDATA->mNumb, ps_CDATA->ctlNumb, ps_CDATA->ctlStat);
		ps_CDATA++;
		msgCnt++;
		syoCnt++;
	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	//commfunc_mutex_unlock( PV_FILENO_CTLJHFIL );	/* [20260518] DEL 呼び出し元がデバッガなので、mutexは使わない
	
//printf("commfunc_ctl_johen_msgget_tail : msgCnt=%d\n", msgCnt );
	return( msgCnt );
}

