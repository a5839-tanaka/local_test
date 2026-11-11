/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260507] 2026.05.07
 * ・以下の型式の力率の変換値が-1に設定されていた場合、
 *   計測値が1000の時は-1000に変換する(符号を反転する)処理を追加する
 *   ME110SR-MB,ME110NSR-MB,ME110SSR-MB,ME110GR-MBH
 * [20250508] 2025.05.08
 * ・三菱電子式マルチ指示計器の以下の型式に対応
 *   ME110SR-MB,ME110NSR-MB,ME110SSR-MB,ME110GR-MBH
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * マイクログリッドシステム
 * -----------------------------------------------------------------------------
 * 三菱電機製EcoMonitor通信プロセス（タイプ１：ＲＳ４８５：ＭｏｄｂｕｓＲＴＵ版）
 *
 * 主な機能
 * 		（１）通信機能
 * 		起動時に指定された通信回線番号に接続されたマルチメーターと通信し、
 *		計測値と装置状態の情報を収集する。
 * 		情報収集は「４８５通信回線情報ファイル（COMLIINF.dat）」と「スマート計測
 *		監視機器情報ファイル（MLTMTINF.dat）」の設定を元に行う。
 * 		通信は、前述の設定ファイルの内容に従い定周期に行う。
 *
 * 		（２）情報の取得および管理
 * 		収集した情報は、計測情報の場合は「計測値保存ファイル（TMSTSFIL.dat）」に
 * 		行い、状態情報は「装置状態ファイル（SVSTSFIL.dat）」に行う、なお、マルチ
 *		メーターとの通信時に発生した通信の状態は「通信ステータス状態ファイル
 *		（STSTSFIL.dat）」に保存する。
 * 		なお、計測値に関しては瞬時値の他に単純移動平均（過去１分間分）の演算を行
 * 		い、瞬時値と同様に「計測状態ファイル（TMSTSFIL.dat）」に保存する。
 *
 * 		（３）状変処理
 * 		装置状態および通信ステータスに変化が生じた場合は、状態変化情報を状変情報
 * 		として「状変蓄積ファイル（JOHENFIL.dat）」に蓄積する。
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <stdio.h>
#include <time.h>
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

#include "mitsumlm1.h"

/* デバッグフラグ */
#if 0
#define	PV_DEBUG_SERIAL
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_DT2								/* デバッグフラグ：移動平均値演算結果表示 */
#define	PV_DEBUG_DT3								/* デバッグフラグ：移動平均ストックデータ表示 */
#endif

#define	PV_OWN_NAME			"MITSUMLM1"				/* 自プログラム名 */

/* ４８５回線で使用するプロトコル名 */
#define	PV_COMPROTOCOL		PV_COMPRO_MITSUMULTIM1	/* タイプ１（MEMOBUS） */

/* ＭＭＡＰアクセス用ポインタ変数 */
struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* 通信回線情報ファイルポインタ */
struct st_MLTMTINF		*ps_g_MLTMTINF;				/* マルチメーター情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */

/* ファイルインデックス（本プロセスで使用するファイルの登録） */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_MLTMTINF, (void **)&ps_g_MLTMTINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0,}
};

static char						pv_own_name[128];			/* 自プロセス情報 */
static char						errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int						mltMaxCnt;					/* 処理対象スマート計測監視機器数 */
static struct st_serialinf		s_serial;					/* シリアル回線情報 */
static struct st_stdDevInfo		s_mltInfo[PV_MAX_MLTMT];	/* スマート計測監視機器情報格納領域 */
static struct st_threadArg		s_threadArg;				/* スレッド管理情報 */
static int						threadStop = 0;				/* スレッドストップフラグ */
static struct st_mltData		s_mltData[PV_MAX_MLTMT];	/* 計測情報（瞬時値格納用）*/
static struct st_mltAvlData		s_mltAvlData[PV_MAX_MLTMT];	/* 計測情報（移動平均演算用）*/

/* 内部関数定義 */
void	*func_mltOpen_thread( void *args );
int	func_mltData_recv( struct st_serialinf *, int, int, int, int, unsigned char *, struct st_serialLog * );
int	func_mltData_get( int, int, int, int, struct st_mltData *, unsigned char * );
int	func_abnormal_exit( int );


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * タケモトデンキ製マルチメーターー通信プロセス主処理
 *
 * 機能：
 * 		引数で指定された通信回線番号を元に４８５通信回線情報ファイル
 * 		（COMLIINF.dat）から通信回線の設定情報を取得する。
 * 		引数で指定された通信回線番号を元にスマート計測監視機器情報ファイル
 * 		（MLTMTINF.dat）から通信回線番号が一致する装置の情報を取得する。
 * 		ファイルに設定された周期でマルチメーターと通信を行い、情報を取得し
 * 		て編集結果を指定されたファイルに保存する。
 * 		管理プロセスから停止指示が出た場合はプロセスを終了する。
 *
 */
int main(int argc, char *argv[])
{
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	char					lineNo;						/* 通信回線番号 */
	pthread_t				threadID;					/* スレッドＩＤ */
	int						requestCnt = 0;				/* 合計要求回数 */
	int						mltNumb;					/* 処理中のマルチメーター番号 */
	int						minDataMax;					/* 移動平均データ最大数 */
	struct timeval			nowTime;					/* 現在時刻 */
	struct timeval 			wrkTime;					/* 欠測判定用時刻 */
	struct st_mltData		mltData[PV_MAX_MLTMT];		/* 計測情報（瞬時値格納用） */

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

	/* 自プロセス情報を設定 */
	sprintf( pv_own_name, "%s(%02d)", PV_OWN_NAME, lineNo );

	/* ファイルをオープンしてＭＭＡＰ化する */
	retCode = commfunc_file_open( s_fileIndex );
	if( retCode < 0 )
	{
		printf("%s : File Open Error\n", PV_OWN_NAME);
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

	/* マルチメーター情報を取得 */
	if( (mltMaxCnt = commfunc_devInfo_get( PV_MKIND_MLTMT, lineNo, s_mltTypeName, s_mltInfo, &requestCnt, NULL, NULL, ps_g_MLTMTINF )) <= 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [MLTMTINF.csv] Not Exist Target Machine of Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

#ifdef PV_DEBUG
	printf("%s : MLMMAXCNT : %d\n", PV_OWN_NAME, mltMaxCnt );
#endif

	/* スレッド管理情報を設定 */
	s_threadArg.indexNumb = 0;
	s_threadArg.lineNo = lineNo;

	/* 収集周期の調整と移動平均算出用データ数の取得 */
	commfunc_serial_cycletmchk( &s_serial, &minDataMax, ps_g_COMLIINF );

	/* 移動平均用データの初期化 */
	commfunc_devAvlData_init( PV_MKIND_MLTMT, minDataMax, s_mltAvlData );

	/* 移動平均用データから積算型計測値を復元する */
	retCode = commfunc_tmstsfil_get( PV_MKIND_MLTMT, mltMaxCnt, PV_MITSUMLM1_MAXTM, s_mltInfo, s_mltData, s_mltAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );

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
		if( commfunc_mswait( s_serial.cycleTime, lineNo ) < 0 )
		{
#ifdef PV_DEBUG
			printf("OMRONKM1 : PROCESS STOP COMMAND RECV (LINE:%d)\n", lineNo );
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
 * マルチメータＭｏｄｂｕｓＲＴＵ通信接続処理
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
														/* マルチメーターへの送信コマンドベース */
	unsigned  char			sendBuff[8] = {
								0x00,					/* 送信先局番（初期値） */
								0x00,					/* ファンクションコード（入力レジスタ読出） */
								0x00,0x00,				/* 開始アドレス */
								0x00,0x00,				/* レジスタの数 */
								0x00,0x00				/* CRC（初期値） */
							};
								
	unsigned char			readBuff[4096];				/* 受信バッファ */
	char					lineNo;						/* 通信回線番号 */
	int						openRetCode;				/* シリアルポートオープン結果 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */
	struct st_serialLog		s_serialLog;				/* シリアル通信ログ格納領域 */
	struct timespec			sleepTm;					/* 送信待ちタイマ */
	struct st_mltMitsuSdInf	*ps_mltSdInf;				/* 送信情報ポインタ */

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

			/* 送信先装置IDの設定 */
			sendBuff[0] = s_mltInfo[loopCnt1].idNumb;

			/* 処理タイプの設定 */
			if( s_mltInfo[loopCnt1].devType == PV_MITSUMLM1_TYPE1 )
			{
				/* 送信情報を処理タイプ１に設定 */
				ps_mltSdInf = s_type1SdInf;
			}
/* [20250508] Add St */
			else if( s_mltInfo[loopCnt1].devType == PV_MITSUMLM1_TYPE2 )
			{
				/* 送信情報を処理タイプ１に設定 */
				ps_mltSdInf = s_type2SdInf;
			}
/* [20250508] Add Ed */
			else
			{
				/* 送信情報を処理タイプ１に設定（暫定処置） */
				ps_mltSdInf = s_type1SdInf;
				sprintf( errMsg, "Not supported Unsuport mltNumb=%d, TYPE=%d", mltNumb, s_mltInfo[loopCnt1].devType );
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}

#ifdef PV_DEBUG
			printf("%s : LOOP:%02d  MLMNO:%02d  MLMID-No:%02d\n", PV_OWN_NAME, loopCnt1+1, mltNumb, idNumb );
#endif

			/* 複数回に分けて情報を収集する */
			for( loopCnt2 = 0 ; ps_mltSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
			{
				/* シリアルポートオープン正常、且つ、前回の要求が正常な場合 */
				if( ( openRetCode >= 0 ) && ( retCode == 0 ))
				{
					/* 送信バッファにファンクションと取得開始アドレス、取得データ数を設定 */
					sendBuff[1] = ps_mltSdInf[loopCnt2].funcCode;
					sendBuff[2] = ps_mltSdInf[loopCnt2].startAdU;
					sendBuff[3] = ps_mltSdInf[loopCnt2].startAdL;
					sendBuff[4] = ps_mltSdInf[loopCnt2].dataCntU;
					sendBuff[5] = ps_mltSdInf[loopCnt2].dataCntL;
		
					/* CRC計算 */
					commfunc_serial_crc16modbus( &sendBuff[0], 6, &sendBuff[7], &sendBuff[6] );

					/* 送信待ち処理 */
					nanosleep( &sleepTm, NULL);

					/* 要求実施回数の上限まで繰り返す */
					/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
					while( s_serial.requestCount < PV_SERIAL_REQUEST_MAX )
					{
						/* シリアル通信ログ領域を初期化 */
						s_serialLog.dataCnt = 0;
	
						/* マルチメーターに要求を送信 */
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
						retCode = func_mltData_recv(	&s_serial, mltNumb, idNumb,
														ps_mltSdInf[loopCnt2].funcCode, ps_mltSdInf[loopCnt2].dataCntL,
														readBuff, &s_serialLog );	
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
							/* タイムアウトおよびＣＲＣ異常以外の場合は再要求不要のため処理を抜ける */
							if( ( retCode != PV_SERIAL_ERRTIMEOUT ) && ( retCode != PV_SERIAL_ERRBCCCRC ) )
							{
								break;
							}
						}
					}

				}
				else
				{
					/* シリアルポートが異常の場合 */
					retCode = -1;
				}
				/* データ処理バッファから装置状態と計測値を抽出 */
				func_mltData_get( s_mltInfo[loopCnt1].devType, loopCnt2 + 1, mltNumb, retCode, &s_mltData[loopCnt1], readBuff);

				/* 正常の場合 */
				if( retCode == 0 )
				{
					/* 現在時刻を取得 */
					gettimeofday( &nowTime, NULL );
					s_mltData[loopCnt1].checkTime = nowTime.tv_sec;
				}
				/*  異常の場合は、残りの要求は送信を行わず欠測させる  */
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
int	func_mltData_recv(
	struct st_serialinf *ps_serial,
	int mltNumb,
	int targetID,
	int reqFunc,
	int reqCnt,
	unsigned char *dataBuff,
	struct st_serialLog *ps_serialLog )
{
	fd_set			rfds;					/* ファイルディスクリプタ集合 */
	unsigned char	readBuff[4096];			/* リードバッファ */
	char			flag_GOD = 0;			/* 受信完了フラグ */
	char			*workPoint;				/* データ格納バッファワークポインタ */
	int				crcData = 0;			/* CRCチェック */
	int				readCnt = 0;			/* データ受信バイト数 */
	int				totalLength = 0;		/* 受信データ量 */
	int				dataLength = 0;			/* 受信データ長 */
	int				checkLength = 0;		/* 受信データ長（期待値） */
	struct timeval	timedata;				/* 現在時刻取得用 */
	struct timeval	timeout;				/* 応答タイムアウト時刻 */
	int				retCode;				/* 関数戻り値 */
	int				loopCnt;				/* ループカウンタ */
	unsigned char	crcHi;					/* CRC上位データ */
	unsigned char	crcLo;					/* CRC下位位データ */


	/* シリアルポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

#ifdef PV_DEBUG
	printf("%s : DATA-RECV : Data-Recv TargetID: %d  START\n", PV_OWN_NAME, targetID);
#endif

	/* ファンクションコードから受信データサイズを決定する（期待値） */
	switch( reqFunc )
	{
		case PV_MITSUMLM1_FUNC_03:
			checkLength = reqCnt * 2 + 5;
			break;
		default:
			checkLength = reqCnt * 2 + 5;
			break;
	}

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

#ifdef PV_DEBUG_SERIAL
printf("%s : FD:%d : RECV:%3dByte : ", pv_own_name, ps_serial->fd_sockfd, readCnt );
#endif
		/* 受信データ数分繰り返す */
		for(loopCnt = 0 ; loopCnt < readCnt ; loopCnt++)
		{
#ifdef PV_DEBUG_SERIAL
printf("[%02X]", readBuff[loopCnt] );
#endif
			/* データバッファに受信データを格納 */
			*workPoint = readBuff[loopCnt];
			workPoint++;
			dataLength++;
			/* ３バイト分のデータを受信したら先頭データのチェックを行う */
			if( dataLength == 3 )
			{
				/* 送信先（送信元）アドレスのチェック */
				if( *dataBuff != targetID )
				{
					sprintf( errMsg, "Data-Recv Address Error! Numb:%02d Expect[%02X],Result[%02X]", mltNumb, targetID, *dataBuff );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* ファンクションコードが不一致の場合 */
				else if ( *(dataBuff+1) != (unsigned char)reqFunc ) 
				{
					sprintf( errMsg, "Data-Recv function Error! Numb:%02d Expect[%02X],Result[%02X]", mltNumb, reqFunc, *(dataBuff+1) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 例外レスポンスの場合 */
					if( (*(dataBuff+1) & (unsigned char)0x80) == (unsigned char)0x80 )
					{
						/* 受信データ長（期待値）を５バイトに設定 */
						checkLength = 5;
					}
					else
					{
						/* 受信データの初期化 */
						dataLength = 0;
					}
				}
				/* ファンクションコードが０３でデータバイト数が異なる場合 */
				else if ( ( reqFunc == PV_MITSUMLM1_FUNC_03 ) && ( *(dataBuff+2) != (reqCnt * 2) ) )
				{
					sprintf( errMsg, "Data-Recv datasize Error! Numb:%02d Expect[%02X],Result[%02X]", mltNumb, reqCnt * 2, *(dataBuff+2) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
			}
			/* 受信データが規程の長さに達した場合 */
			if( dataLength >= checkLength)
			{
#ifdef PV_DEBUG_SERIAL
printf("Data-Recv Compleat!!\n");
#endif
				/* データ受信完了フラグ設定 */
				flag_GOD = 1;
				/* read処理から抜ける */
				break;
			}
		}
#ifdef PV_DEBUG_SERIAL
printf("\n");
#endif
	}
	/* データ受信完了フラグがセットされている？ */
	if( flag_GOD == 1 )
	{
		/* CRCチェック */
		commfunc_serial_crc16modbus( dataBuff, checkLength - 2, &crcHi, &crcLo );
		if( ( crcHi != *(dataBuff + checkLength - 1) ) ||
			( crcLo != *(dataBuff + checkLength - 2) ) )
		{
#ifdef PV_DEBUG
			printf("%s : Data-Recv CRC Error\n", PV_OWN_NAME);
#endif
			/* 通信ステータス状変：上りＢＣＣ／ＣＲＣ異常（発生） */
			commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST02_UPBCCERR, 1,
				(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
			/* 要求実施回数を＋１ */
			ps_serial->requestCount += 1;
			return( PV_SERIAL_ERRBCCCRC );
		}
		/* 受信データが例外レスポンスの場合（ */
		if( checkLength == 5 )
		{
#ifdef PV_DEBUG
			printf("%s : Data-Poling Error\n", PV_OWN_NAME);
#endif
			/* 例外コードが不正ファンクションの場合 */
			if( *(dataBuff+2) == (unsigned char)0x01 )
			{
				/* 通信ステータス状変：下りコマンド異常（発生） */
				commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST19_DWCOMMANDERR, 1,
					(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			if( *(dataBuff+2) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST22_DWPARAMERR, 1,
					(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ の場合 */
			if( *(dataBuff+2) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST23_DWDATAERR, 1,
					(ps_g_MLTMTINF+mltNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				commfunc_johen_stat( PV_MKIND_MLTMT, mltNumb, PV_ST20_DWFORMATERR, 1,
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
 * 		引数 mltNumb で指定されたマルチメーターについて以下の処理を行う。
 * 		引数 dataBuff で指定された受信バッファの内容から装置状態と計測値を取得する。
 * 		受信結果 resultCode が異常の場合は、計測値を０、計測ステータスを欠測に設定
 * 		する。
 * 		受信結果 resultCode が正常な場合は受信バッファのASCIIデータから装置状態と
 * 		計測値を取得し、計測ステータスを生成する。
 * 		装置状態が取得できた場合は、状態更新処理を行う。
 * 戻り値：
 * 		0：正常処理
 */
int	func_mltData_get( int mltType, int reqCnt, int mltNumb, int resultCode, struct st_mltData *ps_mltData, unsigned char *dataBuff)
{
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char		dataByte[4];			/* データ格納用領域 */
		short int			data16bit;				/* 16bitデータ取得用 */
		unsigned short int	data16bitu;				/* 16bitデータ取得用（符号無し） */				
		int					data32bit;				/* 32bitデータ取得用 */
		long int			data64bit;				/* 64bitデータ取得用 */
	} u_change;
	int				tmNumb;							/* 計測情報番号 */
	int				workPoint;						/* 文字列開始ポインタ */
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	int				loopCnt3;						/* ループカウンタ */
	int				bitMask;						/* ビットマスク */
	int				tmIfLinkBase;					/* 計測情報ファイルリンク */
	int				svStLinkBase;					/* 状態情報ファイルリンク */
	int				svIfLinkBase;					/* 装置状態情報ファイルリンク */
	time_t			nowTime;						/* 現在の積算秒数 */
	unsigned char	workStatus[4];					/* 装置状態一時保管用（2バイト分）*/
	struct st_mltMitsuSvInf	*ps_mltSvInf;			/* 装置状態情報取得情報 */
	struct st_mltMitsuTmInf	*ps_mltTmInf;			/* 計測値用情報取得情報 */
    struct st_devTmRange    *ps_mltTmRange;         /* 計測値範囲情報 [20150624] Add */
	struct st_stsMaskTbl	*ps_mltMaskTbl;			/* 状態取得用マスクテーブル [20150624] Add */

	/* 格納位置を取得 */
	tmIfLinkBase = ps_g_MLTMTINF[mltNumb - 1].tmIfLink;
	svStLinkBase = ps_g_MLTMTINF[mltNumb - 1].svStLink;
	svIfLinkBase = ps_g_MLTMTINF[mltNumb - 1].svIfLink;

#ifdef PV_DEBUG_DT1
	printf("%s : DATA-GET : MLM[ %02d ] TYPE[%d] REQ[%d] COD[%d] LINK[TMST:%6d, SVST:%6d, SVIF:%6d]\n",
		PV_OWN_NAME, mltNumb, mltType, reqCnt, resultCode, tmIfLinkBase, svStLinkBase, svIfLinkBase );
#endif

	/* 処理タイプ１の場合 */
	if( mltType == PV_MITSUMLM1_TYPE1 )
	{
		ps_mltSvInf = s_type1SvInf;
		ps_mltTmInf = s_type1TmInf;
		ps_mltTmRange = s_mltTmRange1;	/* [20150624] Add */
	}
/* [20250508] Add St */
	else if( mltType == PV_MITSUMLM1_TYPE2 )
	{
		ps_mltSvInf = s_type2SvInf;
		ps_mltTmInf = s_type2TmInf;
		ps_mltTmRange = s_mltTmRange2;	/* [20150624] Add */
	}
/* [20250508] Add Ed */
	else
	{
		ps_mltSvInf = s_type1SvInf;
		ps_mltTmInf = s_type1TmInf;
		ps_mltTmRange = s_mltTmRange1;	/* [20150624] Add */
	}
	
	/* ------------------ */
	/* 装置状態情報の取得 */
	/* ------------------ */

	/* 受信処理結果が正常の場合のみ装置状態を取得 */
	if( resultCode == 0 )
	{
		/* ストッパーまで繰り返す */
		for( loopCnt1 = 0 ; ps_mltSvInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
		{
			/* 取得情報の要求回数が一致しない場合は次のデータへ */
			if( reqCnt != ps_mltSvInf[loopCnt1].reqCnt )
				continue;

			/* 処理タイプがビットタイプの場合 */
			if( ps_mltSvInf[loopCnt1].svType == PV_SVTYPE_BIT )
			{
#ifdef PV_DEBUG_DT1
    printf("%s : BIT-TYPE\n", PV_OWN_NAME);
#endif
				/* 初期化が必要な場合 */
				if( ps_mltSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_mltData->mltStatus[ ps_mltSvInf[loopCnt1].mltSvLink ], 0, ps_mltSvInf[loopCnt1].iniSize );
				}

				/* 指定バイト数分コピーする */
				for( loopCnt2 = 1 ; loopCnt2 <= ps_mltSvInf[loopCnt1].dataSize ; loopCnt2++ )
				{
					/* 状態を取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
					workStatus[loopCnt2 - 1] = *(dataBuff + ps_mltSvInf[loopCnt1].buffLink + (ps_mltSvInf[loopCnt1].dataSize - loopCnt2));

					/* ビット数分繰り返す（MSB-LSB変換） */
					for( bitMask = 1, loopCnt3 = 0 ; loopCnt3 < 8 ; loopCnt3++, bitMask<<=1 )
					{
						if( workStatus[loopCnt2 - 1] & bitMask )
							ps_mltData->mltStatus[ ps_mltSvInf[loopCnt1].mltSvLink + loopCnt2 - 1 ] |= (unsigned char)(0x80 >> loopCnt3);
#ifdef PV_DEBUG_SV
	/* 状態データバイト位置、装置状態データ */
	printf("%s : SV-STS[%2d] : %02X\n", PV_OWN_NAME, ps_mltSvInf[loopCnt1].mltSvLink + loopCnt2 - 1, (short int)ps_mltData->mltStatus[ ps_mltSvInf[loopCnt1].mltSvLink + loopCnt2 - 1 ] );
#endif
					}
				}
			}
			/* 処理タイプがビットマスクタイプの場合 */
			else if( ps_mltSvInf[loopCnt1].svType == PV_SVTYPE_MSK )
			{
#ifdef PV_DEBUG_DT1
    printf("%s : MSK-TYPE\n", PV_OWN_NAME);
#endif
				/* マスクテーブルポインタを取得 */
				ps_mltMaskTbl = (struct st_stsMaskTbl *)ps_mltSvInf[loopCnt1].ps_tblPointer;

				/* 状態を初期化 */
				memset( &ps_mltData->mltStatus[ ps_mltSvInf[loopCnt1].mltSvLink ], 0, ps_mltSvInf[loopCnt1].iniSize );

				/* 状態テーブルの設定数分処理を行う */
				for( loopCnt2 = 0 ; ps_mltMaskTbl[loopCnt2].buffLink != -1 ; loopCnt2++ )
				{
					/* 状態にビットが設定されている場合 */
					if( *(dataBuff + ps_mltMaskTbl[loopCnt2].buffLink) & ps_mltMaskTbl[loopCnt2].svMask )
					{
						/* パワコン装置状態の該当ビットをセット */
						ps_mltData->mltStatus[ ps_mltMaskTbl[loopCnt2].mltSvLink ] |= ps_mltMaskTbl[loopCnt2].setMask;
					}
				}
#ifdef PV_DEBUG_DT1
    for(loopCnt2 = 0 ; loopCnt2 < ps_mltSvInf[loopCnt1].iniSize ; loopCnt2++ )
    printf("%s :    SV-STS[%2d] : %02X\n", PV_OWN_NAME, ps_mltSvInf[loopCnt1].mltSvLink + loopCnt2, ps_mltData->mltStatus[ ps_mltSvInf[loopCnt1].mltSvLink + loopCnt2 ] );
#endif

			}

		}

		/* 装置状態情報の更新（ビットステータス形式で更新） */
		commfunc_johen_sv( PV_MKIND_MLTMT, mltNumb, PV_MAX_MLTMT_SVPOS, ps_mltData->mltStatus,
			svIfLinkBase, svStLinkBase, 
			ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	/* ------------------------------------ */
	/* 通常形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */

	/* ストッパーまで繰り返す */
	for( loopCnt1 = 0 ; ps_mltTmInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
	{
		/* 取得情報の要求回数が一致しない場合は次のデータへ */
		if( reqCnt != ps_mltTmInf[loopCnt1].reqCnt )
			continue;

		/* 計測情報番号を取得 */
		tmNumb = ps_mltTmInf[loopCnt1].mltTmNumb - 1;

		/* 受信結果が異常 または 計測情報が未実装の場合 */
		if( ( resultCode != 0 ) || ( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 ) )
		{
			/* 未実装の場合 */
			if( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 )
			{
				/* 計測ステータスをリセット */
				ps_mltData->tmFlgData[tmNumb] = 0;
			}
			else
			{
				/* 計測ステータスに欠測を設定 */
				ps_mltData->tmFlgData[tmNumb] = PV_TMFLG_DROPTM;
			}
			/* 計測値を０に設定 */
			ps_mltData->tmValData[tmNumb] = 0;

#ifdef PV_DEBUG_DT1
		printf("%s : TM[%2d] ERR FLG:VAL[%Xh:%9d]  NAME:[%s]\n",
			PV_OWN_NAME, tmNumb+1, ps_mltData->tmFlgData[tmNumb], ps_mltData->tmValData[tmNumb], ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
		}
		/* 受信結果正常 かつ 実装の場合 */
		else
		{
			/* 計測フラグをリセット */
			ps_mltData->tmFlgData[tmNumb] = 0;
			/* 計測値を初期化 */
			ps_mltData->tmValData[tmNumb] = 0;

			/* 変換用領域の初期化 */
			memset( &u_change, 0, sizeof(u_change));
	
			/* 計測値の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
			for( loopCnt2 = 1 ; loopCnt2 <= ps_mltTmInf[loopCnt1].dataSize ; loopCnt2++ )
				u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_mltTmInf[loopCnt1].buffLink + ps_mltTmInf[loopCnt1].dataSize - loopCnt2];

			/* 計測値のサイズが２バイトサイズの場合 */
			if( ps_mltTmInf[loopCnt1].dataSize == 2 )
			{
				/* 符号付き情報の場合 */
				if( ps_mltTmInf[loopCnt1].signedFlg == 1 )
				{
					/* 16bitサイズのデータとして計測値を取得 */
					ps_mltData->tmValData[tmNumb] = u_change.data16bit;
				}
				else
				{
					/* 16bitサイズのデータとして計測値を取得 */
					ps_mltData->tmValData[tmNumb] = (int)u_change.data16bitu;
				}
			}
			/* 計測値のサイズが４バイトサイズの場合 */
			else if( ps_mltTmInf[loopCnt1].dataSize == 4 )
			{
				/* 32bitサイズのデータとして計測値を取得 */
				ps_mltData->tmValData[tmNumb] = u_change.data32bit;
			}
			else
			{
				/* 暫定的に16bitサイズのデータとして計測値を取得 */
				ps_mltData->tmValData[tmNumb] = u_change.data16bit;
			}

			/* 計測値が既定値の範囲内か？ */
			if( ( ps_mltTmRange[tmNumb].tmLower > ps_mltData->tmValData[tmNumb] ) ||
				( ps_mltTmRange[tmNumb].tmUpper < ps_mltData->tmValData[tmNumb] ) )
			{
				/* 計測異常２をセット */
				ps_mltData->tmFlgData[tmNumb] = PV_TMFLG_TMERR2;
			}

/* [20260507] Add St */
			/**** 特殊処理 ****/
			/**************
			 * 処理タイプ２の場合、且つ、
			 * 計測情報が力率(計測番号が19～22)の場合、且つ、
			 * 計測情報の変換値が-1の場合、且つ、
			 * 計測値が1000の場合、符号を反転させる
			**************/
			if(( mltType == PV_MITSUMLM1_TYPE2 ) &&
			   ( tmNumb >= 19) && ( tmNumb <= 22 ) &&
			   ( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].convData == -1 ) &&
			   ( ps_mltData->tmValData[tmNumb] == 1000 ))
			{
				/* 符号を反転する */
				ps_mltData->tmValData[tmNumb] *= -1;
			}
/* [20260507] Add St */
		}

#ifdef PV_DEBUG_DT1
		printf("%s : TM[%2d] NOR FLG:VAL[%Xh:%9d]  NAME:[%s]\n",
			PV_OWN_NAME, tmNumb+1, ps_mltData->tmFlgData[tmNumb], ps_mltData->tmValData[tmNumb], ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
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
