/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20230309] 2023.03.09
 * ・未実装のSVはSVSTSFILに反映しない
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * オムロン製スマート計測監視機器通信プロセス主処理
 *
 * 機能：
 *		＊対応機種はKM1シリーズのみとし、KE1-CTD8E以外のKE1シリーズには未対応と
 *		　する。
 *		（１）通信機能
 *		起動時に指定された通信回線番号に接続されたオムロン社製の下記に示すスマー
 *		ト監視機器と通信し、計測値と装置状態の情報を収集する。
 *		（対応機種：KM1-PMU1A-FLK）
 *		（対応機種：KM1-PMU2A-FLK）
 *		（対応機種：KM1-EMU8A-FLK）
 *		（対応機種：KE1-CTD8E）
 *		情報収集は「４８５通信回線情報ファイル（COMLIINF.dat）」と「スマート計測
 *		監視機器情報ファイル（MLTMTINF.dat）」の設定を元に行う。
 *		通信は、前述の設定ファイルの内容に従い定周期に行う。
 *
 *		（２）情報の取得および管理
 *		収集した情報は、計測情報の場合は「計測値保存ファイル（TMSTSFIL.dat）」に
 *		行い、状態情報は「装置状態ファイル（SVSTSFIL.dat）」に行う、なお、スマー
 *		ト計測監視機器との通信時に発生した通信の状態は「通信ステータス状態ファイ
 *		ル（STSTSFIL.dat）」に保存する。
 *		なお、計測値に関しては瞬時値の他に単純移動平均（過去１分間分）の演算を行
 *		い、瞬時値と同様に「計測状態ファイル（TMSTSFIL.dat）」に保存する。
 *
 *		（３）状変処理
 *		装置状態および通信ステータスに変化が生じた場合は、状態変化情報を状変情報
 *		として「状変蓄積ファイル（JOHENFIL.dat）」に蓄積する。
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvcomm.h"

#include "omronkm1.h"
													/* ４８５回線で使用するプロトコル */
#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_DT2								/* デバッグフラグ：移動平均値演算結果表示 */
#define	PV_DEBUG_DT3								/* デバッグフラグ：移動平均ストックデータ表示 */
#define	PV_DEBUG_SERIAL								/* デバッグフラグ：移動平均ストックデータ表示 */
#endif

#define	PV_OWN_NAME			"OMRONKM1"				/* 自プログラム名 */

#define	PV_COMPROTOCOL		PV_COMPRO_COMPOWAYF1	/* オムロンスマート計測監視機器KM1シリーズ（KE1シリーズを除く） */

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* ４８５通信回線情報ファイルポインタ */
struct st_MLTMTINF		*ps_g_MLTMTINF;				/* スマート計測監視機器情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVDATINF		*ps_g_SVDATINF;				/* 状態情報ファイルポインタ *//* [20230309] Add */
struct st_TMHSTFIL		*ps_g_TMHSTFIL;				/* 蓄積ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */

/* 使用ファイル定義 */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVDATINF, (void **)&ps_g_SVDATINF, 0, 0, 0 },	/* [20230309] Add */
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_MLTMTINF, (void **)&ps_g_MLTMTINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0 }
};

static char				pv_own_name[128];			/* 自プロセス情報 */
static char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

static int						mltMaxCnt;					/* 処理対象スマート計測監視機器数 */
static struct st_serialinf		s_serial;					/* シリアル回線情報 */
static struct st_stdDevInfo		s_mltInfo[PV_MAX_MLTMT];	/* スマート計測監視機器情報格納領域 */
static struct st_threadArg		s_threadArg;				/* スレッド管理情報 */
static int						threadStop = 0;				/* スレッドストップフラグ */
static struct st_mltData		s_mltData[PV_MAX_MLTMT];	/* 計測情報（瞬時値格納用）*/
static struct st_mltAvlData		s_mltAvlData[PV_MAX_MLTMT];	/* 計測情報（移動平均演算用）*/
static int						s_mltReqFlg[PV_MAX_MLTMT][100];	/* 要求有無(100は送信回数の上限を暫定で定義) */

/* 内部関数定義 */
void	*func_mltOpen_thread( void *args );
int		func_mltData_recv( struct st_serialinf *, int, int, int, char *, struct st_serialLog * );
int		func_mltData_get( int, int, int, int, struct st_mltData *, char * );
int		func_abnormal_exit( int );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オムロン製スマート計測監視機器ー通信プロセス主処理
 *
 * 機能：
 * 		引数で指定された通信回線番号を元に４８５通信回線情報ファイル
 * 		（COMLIINF.dat）から通信回線の設定情報を取得する。
 * 		引数で指定された通信回線番号を元にスマート計測監視機器情報ファイル
 * 		（MLTMTINF.dat）から通信回線番号が一致する装置の情報を取得する。
 * 		ファイルに設定された周期でスマート計測監視機器と通信を行い、情報を取得し
 * 		て編集結果を指定されたファイルに保存する。
 * 		管理プロセスから停止指示が出た場合はプロセスを終了する。
 */
int main(int argc, char *argv[])
{
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */
	char					lineNo;						/* 通信回線番号 */
	pthread_t				threadID;					/* スレッドＩＤ */
	int						requestCnt = 0;				/* 合計要求回数 */
	int						mltNumb;					/* 処理中のパワーコンディショナ番号 */
	int						minDataMax;					/* 移動平均データ最大数 */
	int						stocMax;					/* 計測蓄積ストック最大数 */
	struct timespec			sleepTm;					/* 送信待ちタイマ */
	int						retWait;
	struct timeval			nowTime;
	struct timeval 			wrkTime;
	struct st_mltData		mltData[PV_MAX_MLTMT];	/* 計測情報（瞬時値格納用） */

	/* プロセス起動時の引数から通信回線番号を取得 */
	if( argc != 2 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("Usage : %s lineno\n", argv[0]);
		return( -1 );
	}
	else
	{
		/* 通信回線番号を取得 */
		lineNo = (char)atoi(argv[1]);
		/* 通信回線番号が既定値範囲外の場合 */
		if( (lineNo < 1) || (lineNo > PV_MAX_COMLINE) )
		{
			printf("Usage : %s lineno\n", argv[0]);
			return( -1 );
		}
	}

	/* 自プロセス名を設定 */
	sprintf( pv_own_name, "%s(%02d)", PV_OWN_NAME, lineNo );

	/* ファイルをオープンしてＭＭＡＰ化する */
	retCode = commfunc_file_open( s_fileIndex );
	if( retCode < 0 )
	{
		printf("OMRONKM1 : File Open Error\n");
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_SET );

	/* シリアル回線情報を初期化 */
	memset( &s_serial, 0, sizeof(struct st_serialinf) );
	/* データ格納領域を初期化 */
	memset( &s_mltData, 0, sizeof(s_mltData));

	/* スレッド管理情報を初期化 */
	memset( &s_threadArg, 0, sizeof(s_threadArg));

	/* 通信回線情報を取得 */
	if( (retCode = commfunc_serial_infoget( lineNo, &s_serial, ps_g_COMLIINF, PV_COMPROTOCOL )) != 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [COMLINLIF.csv] Information Get Error Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

	/* スマート計測監視機器情報を取得 */
	if( (mltMaxCnt = commfunc_devInfo_get( PV_MKIND_MLTMT, lineNo, s_mltTypeName, s_mltInfo, &requestCnt, NULL, NULL, ps_g_MLTMTINF )) <= 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [MLTMTINF.csv] Not Exist Target Machine of Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

#ifdef PV_DEBUG
	printf("%s : KM1MAXCNT : %d\n", pv_own_name, mltMaxCnt );
#endif

	/* 計測値の実装情報から要求有無をチェックする */
	memset( s_mltReqFlg, 0, sizeof(s_mltReqFlg));
	for( loopCnt1 = 0; loopCnt1 < mltMaxCnt; loopCnt1 ++)
	{
		/* 状態情報取得は必ず取得する */
		s_mltReqFlg[loopCnt1][0] = 1;

		/* 計測情報取得の要求回数分ループする */
		for( loopCnt2 = 1 ; s_mltCommand[loopCnt2].startAddr != 0xFFFF; loopCnt2++ )
		{
			/* 処理対象外の要求は行わない */
			if( s_mltCommand[loopCnt2].type[s_mltInfo[loopCnt1].devType-1] == 0)
				continue;

			/* 計測情報のデータ数分ループ */
			for( loopCnt3 = 0; loopCnt3 < s_mltCommand[loopCnt2].dataCount; loopCnt3++ )
			{
				/* 計測情報の実装有無をチェック */
				if( (ps_g_TMDATINF + (ps_g_MLTMTINF + s_mltInfo[loopCnt1].devNumb - 1)->tmIfLink + s_mltCommand[loopCnt2].tmNumb + loopCnt3 -1 )->statFlag == 1 )
				{
					/* 要求対象とする */
					s_mltReqFlg[loopCnt1][loopCnt2] = 1;
					break;
				}
			}
		}
	}
#ifdef PV_DEBUG
	for( loopCnt1 = 0; loopCnt1 < mltMaxCnt; loopCnt1 ++)
	{
		for( loopCnt2 = 0 ; s_mltCommand[loopCnt2].startAddr != 0xFFFF; loopCnt2++ )
		{
			printf("%s : KM1NO:%02d  KM1ID-No:%02d : ReqCnt[%02d]:%d\n", 
					pv_own_name, s_mltInfo[loopCnt1].devNumb, s_mltInfo[loopCnt1].idNumb, loopCnt2+1, s_mltReqFlg[loopCnt1][loopCnt2] );
		}
	}
#endif

	/* スレッド管理情報を設定 */
	s_threadArg.indexNumb = 0;
	s_threadArg.lineNo = lineNo;

	/* 収集周期の調整と移動平均算出用データ数の取得 */
	commfunc_serial_cycletmchk( &s_serial, &minDataMax, ps_g_COMLIINF );

	/* 移動平均用データの初期化 */
	commfunc_devAvlData_init( PV_MKIND_MLTMT, minDataMax, s_mltAvlData );

	/* 移動平均用データから積算型計測値を復元する */
	retCode = commfunc_tmstsfil_get( PV_MKIND_MLTMT, mltMaxCnt, PV_MAX_MLTMT_TMPOS, s_mltInfo, s_mltData, s_mltAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );

	/* マルチメーターの装置状態を復元する */
	for( loopCnt1 = 0 ; loopCnt1 < mltMaxCnt ; loopCnt1++ )
	{
		/* マルチメーター番号の取得 */
		mltNumb = s_mltInfo[loopCnt1].devNumb;
		/* マルチメーターの装置状態を復元する */
		commfunc_sv_get( PV_MKIND_MLTMT, mltNumb, PV_MAX_MLTMT_SVPOS, s_mltData[loopCnt1].mltStatus, ps_g_SVSTSFIL );
	}

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_RESET );

	/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
	s_threadArg.stFlg = 1;
	if( pthread_create( &threadID, NULL, func_mltOpen_thread, &s_threadArg ) != 0 )
	{

#ifdef PV_DEBUG
printf("%s : PCS[%02d] OPEN-Thread EXECUTE Error\n", pv_own_name, s_mltInfo[0].devNumb );
#endif
		sprintf( errMsg, "Thread Execute Error" );
		commfunc_syslog_msgset( pv_own_name, errMsg );

		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );
		return( -1 );
	}

	while( 1 )
	{
		/* 収集時刻になるまで待機する */
		if( commfunc_wait( s_serial.cycleTime, lineNo ) < 0 )
		{
#ifdef PV_DEBUG
			printf("%s : PROCESS STOP COMMAND RECV (LINE:%d)\n", pv_own_name, lineNo );
#endif
			/* 処理ループを抜ける */
			break;
		}
	}

	/* スレッドの終了を待つ（3秒ｘ5回） */
	threadStop = 1;
	for(loopCnt2 = 0 ; loopCnt2 < 5; loopCnt2++)
	{
		/* スレッドが終了していない場合 */
		if( threadID != 0 )
		{
			/* スレッドの終了を待つ */
			if( pthread_join( threadID, NULL ) == 0 )
			{
				threadID = 0;
			}
		}
		else
		{
			break;
		}
		sleep(3);
	}

	/* スレッドが終了していない場合 */
	if( threadID != 0 )
	{
		sprintf( errMsg, "pthread_detach (tid=%d)", threadID	 );
		/* スレッドのリソースをデタッチする */
		retCode = pthread_detach( threadID );
		if( retCode != 0 )
		{
			sprintf( errMsg, "pthread_detach Error(%d)", retCode );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
	}

	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_RESET );
	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	return( 0 );
}



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * マルチメータＴＣＰ／ＩＰ通信接続処理
 *
 * 機能：
 *		引数 args で指定された情報内に格納されているマルチメータに対
 *		して、接続を行う（オープンからコネクトまで）
 *		実際の接続処理は共通関数 commfunc_serial_open() を使用する。
 *		
 */
void *func_mltOpen_thread( void *args )
{
	int						workIdxNumb;				/* 処理インデックス番号 */
	int						mltNumb;					/* 処理中のスマート計測監視機器番号 */
	int						idNumb;						/* 処理中のスマート計測監視機器装置ＩＤ */
	struct st_threadArg		*ps_threadArg;				/* スレッド引数処理用 */
	struct timeval			nowTime;
														/* スマート計測監視機器への送信コマンドベース */
	char					sendBuff[24] = {
									0x02,				/* STX */
									0x30,0x30,			/* ユニットNo（初期状態） */
									0x30,0x30,			/* サブアドレス（００固定） */
									0x30,				/* SID（０固定） */
														/* コマンドテキスト */
									0x30,0x31,0x30,0x31,	/* MRC（０１）,SRC（０１）：変数エリア読み出し */
									0x43,0x30,				/* 変数種別（Ｃ０） */
									0x00,0x00,0x00,0x00,	/* 読出開始アドレス（初期状態） */
									0x30,0x30,				/* ビット位置（００固定） */
									0x00,0x00,0x00,0x00,	/* 読出要素数（初期状態） */
									0x03,				/* ETX */
									0x00				/* BCC */
							}; 
	char					sendWork[5];				/* ワーク */
	char					readBuff[4096];				/* 受信バッファ */
	char					lineNo;						/* 通信回線番号 */
	int						openRetCode;				/* シリアルポートオープン結果 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */
	struct st_serialLog		s_serialLog;				/* シリアル通信ログ格納領域 */
	struct timespec			sleepTm;					/* 送信待ちタイマ */

	ps_threadArg = (struct st_threadArg *)args;
	/* 処理インデックス番号の取得 */
	workIdxNumb = ps_threadArg->indexNumb;
	/* パワコン番号の取得 */
	mltNumb = s_mltInfo[workIdxNumb].devNumb;
	/* マルチメータ装置IDの取得 */
	idNumb  = s_mltInfo[workIdxNumb].idNumb ;

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));

	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = s_serial.sndTime / 1000;
	sleepTm.tv_nsec = (s_serial.sndTime % 1000) * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( threadStop == 0 )
	{
		/* シリアルポートをオープン */
		if( (openRetCode = commfunc_serial_open( &s_serial )) >= 0 )
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（復帰） */
			commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST14_LANERR, 0,
				(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );

			/* シリアルポートステータスを更新 */
			for(loopCnt1 = 0 ; loopCnt1 < mltMaxCnt ; loopCnt1++ )
				s_mltData[loopCnt1].serialStatus = 0;
		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST14_LANERR, 1,
				(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );

			/* シリアルポートステータスを更新 */
			for(loopCnt1 = 0 ; loopCnt1 < mltMaxCnt ; loopCnt1++ )
				s_mltData[loopCnt1].serialStatus = -1;

			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 3 );
		}

		/* スマート計測監視機器から情報を収集する */
		/* 当該通信回線に所属するスマート計測監視機器の台数分繰り返す */
		for(loopCnt1 = 0 ; loopCnt1 < mltMaxCnt ; loopCnt1++ )
		{
			/* 処理結果の初期値を正常に設定 */
			retCode = 0;

			/* 要求実施回数を初期化 */
			s_serial.requestCount = 0;

			/* マルチメーター番号の取得 */
			mltNumb = s_mltInfo[loopCnt1].devNumb;
			/* マルチメーター装置IDの取得 */
			idNumb  = s_mltInfo[loopCnt1].idNumb ;

#ifdef PV_DEBUG
			printf("OMRONKM1 : LOOP:%02d  KM1NO:%02d  KM1ID-No:%02d\n", loopCnt1, mltNumb, idNumb );
#endif
			/* 送信先装置IDの設定 */
			sendBuff[1] = 0x30 + s_mltInfo[loopCnt1].idNumb / 10;
			sendBuff[2] = 0x30 + s_mltInfo[loopCnt1].idNumb % 10;

			/* 複数回に分けてデータを収集する */
			for( loopCnt2 = 0 ; s_mltCommand[loopCnt2].startAddr != 0xFFFF; loopCnt2++ )
			{
				/* 処理対象外の要求は行わない */
				if( s_mltCommand[loopCnt2].type[s_mltInfo[loopCnt1].devType-1] == 0)
					continue;

				/* 計測情報の実装対象外の場合は、要求は行わない */
				if( s_mltReqFlg[loopCnt1][loopCnt2] == 0 )
					continue;

				/* シリアルポートオープン正常、且つ、前回の要求が正常な場合 */
				if( ( openRetCode >= 0 ) && ( retCode == 0 ))
				{
					/* 要求アドレスを設定 */
					sprintf( sendWork, "%04X", s_mltCommand[loopCnt2].startAddr );
					strncpy( &sendBuff[12], sendWork, 4);
					/* 要求要素数を設定 */
					sprintf( sendWork, "%04X", s_mltCommand[loopCnt2].dataCount );
					strncpy( &sendBuff[18], sendWork, 4);


#ifdef PV_DEBUG_SERIAL
printf("%s : SEND-DATA: %02X ", pv_own_name, sendBuff[0]);
#endif
					/* BCC計算 */
					sendBuff[23] = 0;
					for(loopCnt3 = 1; loopCnt3 < 23 ; loopCnt3++)
					{
#ifdef PV_DEBUG_SERIAL
printf("%02X ",sendBuff[loopCnt3]);
#endif
						sendBuff[23] ^= sendBuff[loopCnt3];
					}
#ifdef PV_DEBUG_SERIAL
printf("%02X\n",sendBuff[23]);
#endif
					/* 送信待ち処理 */
					nanosleep( &sleepTm, NULL);
	
					/* 処理結果の初期値を異常に設定 */
					retCode = -1;

					/* 要求実施回数の上限まで繰り返す */
					/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
					while( s_serial.requestCount < PV_SERIAL_REQUEST_MAX )
					{
						/* シリアルログ領域を初期化 */
						s_serialLog.dataCnt = 0;

						/* スマート計測監視機器に要求を送信 */
						retCode = commfunc_serial_send( &s_serial, sendBuff, sizeof(sendBuff), &s_serialLog );
						if( retCode < 0 )
						{
							sprintf( errMsg, "Serial Port Write Error! RET:%d num/id:%02d/%02d (%s)",
								retCode, mltNumb, idNumb, s_serial.targetName );
							commfunc_syslog_msgset( pv_own_name, errMsg );
							/* データ送信異常（送信不可異常）発生 */
							/* 状変（通信ステータス状変）発生 */
							commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST17_NOTSENDERR, 1,
								(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
							/* 再要求不要のため処理を抜ける */
							break;
						}
						else
						{
							/* データ送信異常（送信不可異常）復帰 */
							/* 状変（通信ステータス状変）復帰 */
							commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST17_NOTSENDERR, 0,
								(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
						}
		
						/* データ処理バッファをクリア */
						memset(readBuff, 0, sizeof(readBuff));
		
						/* シリアルポートからデータを受信し、データ処理バッファに保存 */
						retCode = func_mltData_recv( &s_serial, mltNumb, idNumb, s_mltCommand[loopCnt2].dataCount, readBuff, &s_serialLog );
						if( retCode == 0 )
						{
							/* 正常データ受信完了 */
							/* シリアル通信ログを出力 */
							commfunc_serial_logout( PV_SERIAL_LOG_AUTO, &s_serial, &s_serialLog );
							/* 通信ステータス状変：全ステータス（復帰） */
							commfunc_johen_resetstat( PV_MKIND_MLTMT, mltNumb,
								(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
							/* 再要求不要のため処理を抜ける */
							break;
						}
						else
						{
							/* シリアル通信ログを出力 */
							commfunc_serial_logout( PV_SERIAL_LOG_ON, &s_serial, &s_serialLog );
							/* タイムアウトおよびＢＣＣ異常以外は再要求不要のため処理を抜ける */
							if( ( retCode != PV_SERIAL_ERRTIMEOUT ) && ( retCode != PV_SERIAL_ERRBCCCRC ) )
								break;
						}
					}
				}
				else
				{
					retCode = -1;
				}
	
				/* データ処理バッファから装置状態と計測値を抽出 */
				func_mltData_get( mltNumb, retCode, s_mltCommand[loopCnt2].tmNumb, s_mltCommand[loopCnt2].dataCount, &s_mltData[loopCnt1], readBuff);

			}

			/* 計測種別毎の保持 */
			retCode = commfunc_devTmData_set( PV_MKIND_MLTMT, mltNumb, PV_MAX_MLTMT_TMPOS, &s_mltData[loopCnt1], &s_mltAvlData[loopCnt1], ps_g_TMDATINF );

			/* 計測情報を計測値格納ファイルに保存 */
			retCode = commfunc_tmstsfil_devset( PV_MKIND_MLTMT, mltNumb, PV_MAX_MLTMT_TMPOS, &s_mltData[loopCnt1], &s_mltAvlData[loopCnt1], ps_g_TMDATINF, ps_g_TMSTSFIL );
		}
	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &s_serial );

	/* スレッド終了フラグＯＮ */
	ps_threadArg->edFlg = 1;

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ受信処理
 *
 * 機能：
 * 		シリアルポートからデータを受信し、引数 dataBuff に指定されたアドレスへ
 * 		受信データを保存する。
 * 		データ受信する際に、受信データが引数 targetID で指定された装置IDからの
 * 		データであるかチェックし、IDが異なるデータは破棄する。
 * 		引数 ps_serial 内で設定されている受信タイムアウトまで受信データを待つ。
 * 		データが正常に受信できない場合は、通信ステータス状態を更新する。
 * 戻り値：
 * 		 0：正常処理
 * 		0>：異常処理（シリアルポートアクセス不可、データ未受信等）
 *
 */
int	func_mltData_recv(struct st_serialinf *ps_serial, int mltNumb, int targetID, int dataCount, char *dataBuff, struct st_serialLog *ps_serialLog )
{
	fd_set			rfds;					/* ファイルディスクリプタ集合 */
	char			readBuff[4096];			/* リードバッファ */
	char			flag_STX = 0;			/* STX(0x02)受信済みフラグ */
	char			flag_ETX = 0;			/* ETX(0x03)受信済みフラグ */
	char			flag_GOD = 0;			/* 受信完了フラグ */
	char			*workPoint;				/* データ格納バッファワークポインタ */
	char			bccData = 0;			/* BCCチェック */
	int				readCnt = 0;			/* データ受信バイト数 */
	int				totalLength = 0;		/* 受信データ量 */
	int				dataLength = 0;			/* 受信データ長 */
	int				checkLength = 0;		/* 受信データ長（期待値） */
	int				checkPoint;
	struct timeval	timedata;				/* 現在時刻取得用 */
	struct timeval	timeout;				/* 応答タイムアウト時刻 */
	int				retCode;				/* 関数戻り値 */
	int				errCode = 0;			/* エラー戻り値 */
	int				loopCnt;				/* ループカウンタ */

	/* シリアルポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* 受信データ長（期待値）を設定 */
	checkLength = 9 + 8 + dataCount * 8 ;

#ifdef PV_DEBUG
	printf("OMRONKM1 : Data-Recv TargetID:%d  LENGTH:%d\n", targetID , checkLength );
#endif
	/* データバッファのアドレスをコピー */
	workPoint = dataBuff;

	/* 現在時刻の取得 */
	gettimeofday(&timedata, NULL);

	/* 応答タイムアウト時刻の設定 */
	timeout.tv_usec = timedata.tv_usec + (ps_serial->rcvTime % 1000) * 1000;
	if(timeout.tv_usec >= 1000000)
	{
		timeout.tv_sec  = timedata.tv_sec + 1 + (ps_serial->rcvTime / 1000);
		timeout.tv_usec %= 1000000;
	}
	else
	{
		timeout.tv_sec  = timedata.tv_sec + (ps_serial->rcvTime / 1000);
	}

	/* 以下の条件を満たすまでシリアルポートからデータを読み込む */
	/* ・受信予定のデータをすべて受信 */
	/* ・タイムアウト */
	readCnt = 0;
	while( (flag_GOD == 0) && (readCnt >= 0) )
	{
		/* シリアルポートからデータ読み出し */
		readCnt = commfunc_serial_recv(ps_serial, readBuff, &timeout, ps_serialLog );
		if( readCnt < 0 )
		{
			switch(readCnt)
			{
				case PV_SERIAL_ERRNOTREADY :
					sprintf( errMsg, "Serial Port Not Open! Numb:%02d Id:%02d (%s)", mltNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRNOTREADY );
					break;
				case PV_SERIAL_ERRTIMEOUT :
					sprintf( errMsg, "Read Timeout! DataLen:%d Numb:%02d Id:%02d (%s)", totalLength, mltNumb, targetID, ps_serial->targetName);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 要求実施回数を＋１ */
					ps_serial->requestCount += 1;
					/* 要求実施回数が上限に達している場合 */
					if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
					{
						/* 通信ステータス状変：通信不応答異常（発生） */
						commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST01_NONRESPONSE, 1,
							(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", mltNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRACCESS );
					break;
				default:
					break;
			}
			/* シリアルポート異常 */
			return( PV_SERIAL_ERROTHER );
		}
		totalLength += readCnt;

		/* 受信データ数分繰り返す */
		for(loopCnt = 0 ; loopCnt < readCnt ; loopCnt++)
		{
#ifdef PV_DEBUG_SERIAL
printf("%02X ", readBuff[loopCnt] );
#endif
			/* STX受信 */
			if( (flag_STX == 0 ) && (readBuff[loopCnt] == 0x02) )
			{
				/* STX受信済みフラグ設定 */
				flag_STX = 1;
				/* ETX受信済みフラグ初期化 */
				flag_ETX = 0;
				/* 受信完了フラグ初期化 */
				flag_GOD = 0;
				/* データバッファに受信データ（１バイト）を格納 */
				dataLength = 1;
				workPoint = dataBuff;
				*workPoint = readBuff[loopCnt];
				workPoint++;
			}
			/* ETX受信 */
			else if( (flag_ETX == 0) && (readBuff[loopCnt] == 0x03) )
			{
				/* STX未受信？ */
				if( flag_STX == 0 )
				{
					dataLength = 0;
					continue;
				}
				/* ETXの位置が本来の位置にない場合 */
				if( dataLength != (checkLength - 2) )
				{
					/* STX受信済みフラグ初期化 */
					flag_STX = 0;
					dataLength = 0;
					continue;
				}
				/* ETX受信済みフラグ設定 */
				flag_ETX = 1;
				/* データバッファにETX受信データ（１バイト）を格納 */
				dataLength++;
				*workPoint = readBuff[loopCnt];
				workPoint++;
			}
			/* その他のデータ受信 */
			else
			{
				/* STX未受信？ */
				if( flag_STX == 0 )
				{
					continue;
				}
				/* データバッファに受信データを格納 */
				*workPoint = readBuff[loopCnt];
				workPoint++;
				dataLength++;
				/* 受信データの送信元装置ＩＤに正しい値が格納されていない場合 */
				if( dataLength == 3 )
				{
					if( (*(dataBuff+1) != (0x30 + targetID / 10)) ||
						(*(dataBuff+2) != (0x30 + targetID % 10)) )
					{
						sprintf( errMsg, "Data-Recv Address Error! Numb:%02d", mltNumb );
						commfunc_syslog_msgset( pv_own_name, errMsg );
						/* STX受信済みフラグ初期化 */
						flag_STX = 0;
						dataLength = 0;
					}
				}
				/* 受信データの終了コードを確認する */
				if( dataLength == 7 )
				{
					/* 終了コードが００意外の場合 */
					if( (*(dataBuff+5) != 0x30) || (*(dataBuff+6) != 0x30) )
					{
						/* 受信データ長（期待値）を９に変更 */
						checkLength = 9;
						
						/* コードが０Ｆ：ＦＩＮＳコマンドエラーの場合 */
						if( (*(dataBuff+5) == 0x30) && (*(dataBuff+6) == 0x46) )
						{
							/* 異常コードに下りコマンド異常を設定 */
							errCode = PV_ST19_DWCOMMANDERR;
							/* 受信データ長（期待値）を17に変更 */
							checkLength = 9 + 8;
						}
						/* コードが１０：パリティエラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x30) )
						{
							/* 異常コードに下りパリティ異常を設定 */
							errCode = PV_ST24_DWPARITYERR;
						}
						/* コードが１１：フレーミングエラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x31) )
						{
							/* 異常コードに下りフレーム異常を設定 */
							errCode = PV_ST21_DWFRAMEERR;
						}
						/* コードが１２：オーバーランエラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x32) )
						{
							/* 異常コードに下りバッファフル異常を設定 */
							errCode = PV_ST26_DWBUFFERERR;
						}
						/* コードが１３：ＢＣＣエラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x33) )
						{
							/* 異常コードに下りＢＣＣ異常を設定 */
							errCode = PV_ST18_DWBCCERR;
						}
						/* コードが１４：フォーマットエラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x34) )
						{
							/* 異常コードに下りフォーマット異常を設定 */
							errCode = PV_ST20_DWFORMATERR;
						}
						/* コードが１６：サブアドレスエラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x36) )
						{
							/* 異常コードに下りサブアドレス異常を設定 */
							errCode = PV_ST25_DWSUBADDERR;
						}
						/* コードが１８：フレーム長エラーの場合 */
						else if( (*(dataBuff+5) == 0x31) && (*(dataBuff+6) == 0x38) )
						{
							/* 異常コードに下りフレーム長異常を設定 */
							errCode = PV_ST21_DWFRAMEERR;
						}
						else
						{
							/* STX受信済みフラグ初期化 */
							flag_STX = 0;
							dataLength = 0;
						}
					}
				}
				/* 受信データのレスポンスコードを確認する */
				if( dataLength == 15 )
				{
					/* レスポンスコードが００００意外の場合 */
					if( (*(dataBuff+11) != 0x30) || (*(dataBuff+12) != 0x30) ||
						(*(dataBuff+13) != 0x30) || (*(dataBuff+14) != 0x30) ) 
					{
						/* 受信データ長（期待値）を9+8に変更 */
						checkLength = 9 + 8;
						/* 異常コードに下りコマンド異常を設定 */
						errCode = PV_ST19_DWCOMMANDERR;
					}
				}
				/* 受信データが規程の長さに達した場合 */
				if( dataLength >= checkLength )
				{
					/* ETX未受信の場合 */
					if( flag_ETX == 0 )
					{
						/* STX受信済みフラグ初期化 */
						flag_STX = 0;
						dataLength = 0;
					}
					else
					{
#ifdef PV_DEBUG
						printf("OMRONKM1 : Data-Recv Compleat!!\n");
#endif
						/* データ受信完了フラグ設定 */
						flag_GOD = 1;
						/* read処理から抜ける */
						break;
					}
				}
			}
		}
#ifdef PV_DEBUG_SERIAL
printf("\n");
#endif
	}
	/* データ受信完了フラグがセットされている？ */
	if( flag_GOD == 1 )
	{
		/* BCCチェック（STXを除く送信先アドレスからETXまで） */
		workPoint = dataBuff;
		workPoint++;
		bccData = 0;
		for( loopCnt = 0 ; loopCnt < (checkLength - 2) ; loopCnt++, workPoint++ )
		{
			bccData ^= *workPoint;	
		}
		if( bccData != *(workPoint) )
		{
#ifdef PV_DEBUG
			printf("OMRONKM1 : Data-Recv BCC Error\n");
#endif
			/* 通信ステータス状変：上りＢＣＣ異常（発生） */
			commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST02_UPBCCERR, 1,
				(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
			/* BCCエラー */
			/* 要求実施回数を＋１ */
			ps_serial->requestCount += 1;
			return( PV_SERIAL_ERRBCCCRC );
		}
		else
		{
			/* BCCチェックが正常でもレスポンスや終了コードが異常の場合 */
			if( errCode )
			{
				/* 通信ステータス状変：errCodeに設定されている異常（発生） */
				commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, errCode, 1,
					(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
		}
	}
	else
	{
		/* 要求実施回数を＋１ */
		ps_serial->requestCount += 1;
		/* 要求実施回数が上限に達している場合 */
		if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
		{
			/* 通信ステータス状変：通信不応答異常（発生） */
			commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST01_NONRESPONSE, 1,
				(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
		}
		/* タイムアウト */
		return( PV_SERIAL_ERRTIMEOUT );
	}
	/* 正常受信完了 */
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測情報取得処理
 *
 * 機能：
 * 		引数 mltNumb で指定されたスマート計測監視機器について以下の処理を行う。
 * 		引数 dataBuff で指定された受信バッファの内容から装置状態と計測値を取得する。
 * 		受信結果 resultCode が異常の場合は、計測値を０、計測ステータスを欠測に設定
 * 		する。
 * 		受信結果 resultCode が正常な場合は受信バッファのASCIIデータから装置状態と
 * 		計測値を取得し、計測ステータスを生成する。
 * 		計測番号は引数 tmNumbBase を先頭に、引数 tmMaxCnt 分の計測情報の取得を行う。
 * 		引数 tmNumbBase が 0 の場合は、計測情報ではなく装置状態の取得処理を行う。
 * 		装置状態が取得できた場合は、状態更新処理を行う。
 *
 */
int	func_mltData_get( int mltNumb, int resultCode, int tmNumbBase, int tmMaxCnt, struct st_mltData *ps_mltData, char *dataBuff)
{
	char			workStr[16];					/* 数値変換処理用 */
													/* 変換文字数 */
	int				workPoint;						/* 文字列開始ポインタ */
	int				loopCnt;						/* ループカウンタ */
	long long		workStat;						/* 数値変換処理用 */
	long long		workVal;						/* 計測値変換処理用 */
	long long		bitMask;						/* ビットマスク情報 */
	int				tmIfLinkBase;					/* 計測情報ファイルリンク */
	int				svStLinkBase;					/* 状態情報ファイルリンク */
	int				svIfLinkBase;					/* 装置状態情報ファイルリンク */
	int				tmNumb;							/* 計測番号 */

	/* 格納位置を取得 */
	tmIfLinkBase = ps_g_MLTMTINF[mltNumb - 1].tmIfLink;
	svStLinkBase = ps_g_MLTMTINF[mltNumb - 1].svStLink;
	svIfLinkBase = ps_g_MLTMTINF[mltNumb - 1].svIfLink;

#ifdef PV_DEBUG_DT1
	printf("OMRONKM1 : LINKBASE [TMST:%6d , SVST:%6d , SVIF:%6d]  BASETM [%3d]  TMCNT [%2d]\n",
		tmIfLinkBase, svStLinkBase, svIfLinkBase,
		tmNumbBase, tmMaxCnt );
#endif

	/* 処理が装置状態取得の場合 */
	if( tmNumbBase == 0 )
	{
		/* 受信処理結果が正常の場合のみ計測値を取得 */
		if( resultCode == 0 )
		{
			/* 装置状態の取得 */
			/* 文字列->数値変換用変数を初期化 */
			memset(workStr, 0, sizeof(workStr));
			/* 装置状態（文字列）の取得 */
			memcpy(workStr, dataBuff + 15, 8);
			/* 装置状態を取得（16進文字列->数値化） */
			workStat = strtoll(workStr, NULL, 16);
			/* 装置状態を初期化 */
			memset( ps_mltData->mltStatus, 0, 4 );
			/* 状態情報分繰り返す */
			for( bitMask = 1, loopCnt = 0 ; loopCnt < 32 ; loopCnt++, bitMask<<=1 )
			{
/* [20230309] Add St */
				/* 状態情報の実装有無をチェック */
				if( (ps_g_SVDATINF + svIfLinkBase + loopCnt )->statFlag == 0 )
					/* 未実装の場合はSVSTSFILに反映しない */
					continue;
/* [20230309] Add Ed */

				if( workStat & bitMask )
				{
					/* 装置状態を格納 */
					ps_mltData->mltStatus[loopCnt / 8] |= (unsigned char)(0x80 >> (loopCnt % 8));
				}
			}
#ifdef PV_DEBUG_DT1
			printf("OMRONKM1 : KM1 STATUS : ");
			for( loopCnt = 0 ; loopCnt < 4 ; loopCnt++ )
			{
				printf("%02X ", ps_mltData->mltStatus[loopCnt]);
			}
			printf("\n");
#endif

			/* 装置状態情報の更新（ビットステータス形式で更新） */
			commfunc_johen_sv( PV_MKIND_MLTMT, mltNumb, PV_MAX_MLTMT_SVPOS, ps_mltData->mltStatus,
				svIfLinkBase, svStLinkBase,
				ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
	}
	else
	{
		/* 計測値の取得 */
		for(workPoint = 15, loopCnt = 0 ; loopCnt < tmMaxCnt ; loopCnt++)
		{
			tmNumb = loopCnt + tmNumbBase -1;
			/* 計測情報が未実装の場合 */
			if( ps_g_TMDATINF[ tmIfLinkBase + tmNumb].statFlag == 0 )
			{
				/* 計測ステータスをリセット */
				ps_mltData->tmFlgData[tmNumb] = 0;
				/* 計測値を０に設定 */
				ps_mltData->tmValData[tmNumb] = 0;
				/* データ取得ポインタを移動 */
				workPoint += 8;
				continue;
			}
	
			/* 受信結果が異常の場合 */
			if( resultCode != 0 )
			{
				/* 計測ステータスに欠測をセット */
				ps_mltData->tmFlgData[tmNumb] = PV_TMFLG_DROPTM;
				/* 計測値を０に設定 */
				ps_mltData->tmValData[tmNumb] = 0;
			}
			/* 受信結果が正常の場合 */
			else
			{
				/* 文字列->数値変換用変数を初期化 */
				memset(workStr, 0, sizeof(workStr));
				/* 計測値（文字列）を取得 */
				memcpy(workStr, (dataBuff + workPoint), 8);
				/* 計測値を取得（文字列->数値化） */
				workVal = strtoll(workStr, NULL, 16);
				/* 計測値が既定値の範囲内か？ */
				if( ( s_mltTmRange1[tmNumb].tmLower > workVal ) ||
					( s_mltTmRange1[tmNumb].tmUpper < workVal ) )
				{
					/* 計測異常２をセット */
					ps_mltData->tmFlgData[tmNumb] = PV_TMFLG_TMERR2;
					ps_mltData->tmValData[tmNumb] = workVal;
				}
				else
				{
					/* 計測フラグを正常にセット */
					ps_mltData->tmValData[tmNumb] = workVal;
					ps_mltData->tmFlgData[tmNumb] = PV_TMFLG_NORMAL;
				}
		
				/* データ取得ポインタを移動 */
				workPoint += 8;
			}

			/********************************/
			/* 特殊処理 *********************/
			/********************************/
			/* 積算有効電力量を0.1kw単位に変更し、且つ、積算有効電力量がリセットされた場合 */
			/* 繰り上げと判断し設定するように修正 */
			if( ( tmNumb  >= PV_OMRONKM1_TMNUMB_WH1 -1 ) && 
			    ( tmNumb  < (PV_OMRONKM1_TMNUMB_WH1 + PV_OMRONKM1_WH_NUM -1 )))
			{
				/* 単位をw→0.1kwに変換する */
			    ps_mltData->tmValData[tmNumb] /= commfunc_pow10abs(PV_OMRONKM1_WH_CONV_FACT);

			    /* 計測値が正常な場合、繰り上げ桁を加算し、さらに繰り上げがあるか判定を行う */
			    if( ps_mltData->tmFlgData[tmNumb] == PV_TMFLG_NORMAL )
			    {
					/* 繰り上げ桁を加算 */
					ps_mltData->tmValData[tmNumb] += ((s_mltAvlData[mltNumb-1].tmDataAvlVal[tmNumb] / commfunc_pow10abs(PV_OMRONKM1_WH_CONV_DIGIT)) * commfunc_pow10abs(PV_OMRONKM1_WH_CONV_DIGIT));

					/* 計測種別が積算値の平均値は最新の正常値が格納されている */
					/* 最新の計測値の最上位桁が９の場合、且つ、現在の計測値が最新の計測値より小さい場合 */
					if( ((( s_mltAvlData[mltNumb-1].tmDataAvlVal[tmNumb] % commfunc_pow10abs(PV_OMRONKM1_WH_CONV_DIGIT) ) / commfunc_pow10abs(PV_OMRONKM1_WH_CONV_DIGIT-1) ) == 9 ) &&
						(   s_mltAvlData[mltNumb-1].tmDataAvlVal[tmNumb] > ps_mltData->tmValData[tmNumb] ))
					{
						/* 繰り上げと判断し、加算 */
						ps_mltData->tmValData[tmNumb] += commfunc_pow10abs(PV_OMRONKM1_WH_CONV_DIGIT);
					}
				}
			}

#ifdef PV_DEBUG_DT1
			printf("OMRONKM1 : TM[%03d]  FLG:VAL[%Xh:%9d]  NAME:[%s]\n",
				loopCnt + tmNumbBase,
				ps_mltData->tmFlgData[tmNumb], ps_mltData->tmValData[tmNumb],
				ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
		}
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス異常終了処理
 *
 * 機能：
 * 		プロセスの異常終了をシステム監視ファイルに登録し、ファイルリソースを開放
 * 		する。
 * 戻り値：
 * 		0：正常処理
 */
int	func_abnormal_exit( int lineNo )
{
		/* プロセス異常中フラグを設定 */
		commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_SET );
		/* プロセス動作中フラグを解除 */
		commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_RESET );
		/* ファイルをオープンしてＭＭＡＰ化する */
		commfunc_file_close( s_fileIndex );

		return( 0 );
}

