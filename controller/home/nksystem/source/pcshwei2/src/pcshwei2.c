/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20230130] 2023.01.30
 * ・ＨＵＡＷＥＩ製パワーコンディショナの下記機種に対応
 *  （SUN2000-40KTL-M、SUN2000-40KTL-NAM、SUN2000-40KTL-NHM）FW.V100R001C
 * [20220721] 2022.07.21
 * ・通信異常が発生した際、パワコン毎に欠測するように修正
 *   [20210322]からの修正
 * [20210407] 2021.04.07
 * ・トランザクションIDに装置IDを含めるように修正
 * [20210322] 2021.03.22
 * ・通信異常が発生した場合、計測情報を即時欠測にせずに、収集周期のタイミングで
 *   欠測の判定を行うように修正
 * [20201030] 2020.10.30
 * ・出力制御を収集周期時のみ行うように修正(制御指示による待ち解除時は行わない)
 *   pvschmngプロセスが先に動作するように待ち解除後に100msスリープする
 * [20200902] 2020.09.02
 * ・SmartLoggerの環境モニタ機器の計測情報（気温、日射量）を取得
 *   (環境モニタ機器の計測値を未実装にしている場合は要求しない)
 * [20200825] 2020.08.25
 * ・ＨＵＡＷＥＩ製パワーコンディショナの出力制御の不具合に対応
 * [20200813] 2020.08.13
 * ・ＨＵＡＷＥＩ製パワーコンディショナの下記機種の出力制御に対応
 *  （SmartLogger1000）
 * [20200728] 2020.07.28
 * ・ＨＵＡＷＥＩ製パワーコンディショナの下記機種に対応
 *  （SUN2000-50KTL-JP、SUN2000-63KTL-JP）
 * [20200213] 2020.02.13
 * ・ModbusTCPヘッダのデータ長の設定値誤りを修正
 * [20190523] 2019.05.23
 * ・特高発電所向け運転状態監視機能に対応
 */

/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * ＨＵＡＷＥＩ（サンケン電気）製パワーコンディショナ通信プロセス
 * （タイプ２：Ｅｔｈｅｒｎｅｔ：ＭｏｄｂｕｓＴＣＰ）
 *
 * 主な機能
 * 		（１）通信機能
 * 		起動時に指定された通信回線番号に接続されたＨＵＡＷＥＩ（サンケン電気）製
 * 		の下記パワーコンディショナと通信し、計測値と装置状態の情報を収集する。
 * 		情報収集は「４８５通信回線情報ファイル（COMLIINF.dat）」と「パワーコンデ
 * 		ィショナ情報ファイル（POWCNINF.dat）」の設定を元に行う。
 * 		通信は、前述の設定ファイルの内容に従い定周期に行う。
 *
 * 		（２）情報の取得および管理
 * 		収集した情報は、計測情報の場合は「計測値保存ファイル（TMSTSFIL.dat）」に
 * 		行い、状態情報は「装置状態ファイル（SVSTSFIL.dat）」に行う、なお、パワー
 * 		コンディショナとの通信時に発生した通信の状態は「通信ステータス状態ファイ
 * 		ル（STSTSFIL.dat）」に保存する。
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
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvcomm.h"
#include "pvctldata.h"

#include "pcshwei2.h"

/* デバッグフラグ */
#if 0
#define	PV_DEBUG_SERIAL
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_DT2								/* デバッグフラグ：移動平均値演算結果表示 */
#define	PV_DEBUG_DT3								/* デバッグフラグ：移動平均ストックデータ表示 */
#define	PV_DEBUG_DT4								/* デバッグフラグ：出力制御ログ表示 */
#endif

#define	PV_OWN_NAME			"PCSHWEI2"				/* 自プログラム名 */

/* ４８５回線で使用するプロトコル名 */
#define	PV_COMPROTOCOL1		PV_COMPRO_HUAWEI2			/* ＨＵＡＷＥＩ通信プロトコル（MODBUS-TCP） */
#define	PV_COMPROTOCOL2		PV_COMPRO_SANKEN2			/* サンケン電気通信プロトコル（MODBUS-TCP） */

/* ＭＭＡＰアクセス用ポインタ変数 */
struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* ４８５通信回線情報ファイルポインタ */
struct st_POWCNINF		*ps_g_POWCNINF;				/* パワーコンディショナ情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVCTLFIL		*ps_g_PVCTLFIL;				/* 制御状態保存ファイルポインタ [20190523] Add */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */

/* ファイルインデックス（本プロセスで使用するファイルの登録） */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_POWCNINF, (void **)&ps_g_POWCNINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVCTLFIL, (void **)&ps_g_PVCTLFIL, 0, 0, 0 },	/* [20190523] Add */
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0,}
};

static char					pv_own_name[128];			/* 自プロセス情報 */
static char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

static struct st_serialinf	s_serial;					/* 通信回線情報(接続は1回線) */
static int					pcsMaxCnt;					/* 処理対象パワーコンディショナ数 */
static struct st_stdDevInfo	s_pcsInfo[PV_MAX_POWCN];	/* パワーコンディショナ情報格納領域 */
static struct st_serialLog	s_serialLog;				/* シリアル通信ログ格納領域 */
static struct st_pcsData	s_pcsData[PV_MAX_POWCN];	/* 計測情報（瞬時値格納用） */
static struct st_pcsAvlData	s_pcsAvlData[PV_MAX_POWCN];	/* 計測情報（移動平均演算用） */
static int					threadStop = 0;				/* スレッドストップフラグ */


/* 内部関数定義 */
void	*func_pcsOpen_thread( void *args );
int		func_pcsData_recv( struct st_serialinf *, int, int, int, int, int, unsigned char *, unsigned char *, struct st_serialLog * );
int		func_pcsData_get( int, int, int, int, struct st_pcsData *, unsigned char * );
int		func_abnormal_exit( int );
int		func_getTmPowSav( int, int, int );	/* [20200813] Add */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＨＵＡＷＥＩ（サンケン電気）製パワーコンディショナー通信プロセス主処理
 *
 * 機能：
 * 		引数で指定された通信回線番号を元に４８５通信回線情報ファイル
 * 		（COMLIINF.dat）から通信回線の設定情報を取得する。
 * 		引数で指定された通信回線番号を元にパワーコンディショナ情報ファイル
 * 		（POWCNINF.dat）から通信回線番号が一致する装置の情報を取得する。
 * 		ファイルに設定された周期でパワーコンディショナと通信を行い、情報を取得し
 * 		て編集結果を指定されたファイルに保存する。
 * 		管理プロセスから停止指示が出た場合はプロセスを終了する。
 *
 */
int main(int argc, char *argv[])
{
	int						retCode;					/* 関数戻り値格納用 */
	int						retWait;					/* 待ち解除結果 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						requestCnt = 0;				/* 合計要求回数 */
	int						pcsNumb;					/* 処理中のパワーコンディショナ番号 */
	int						stocMax;					/* 移動平均ストック最大数 */
	pthread_t				threadID;					/* スレッドＩＤ */
	struct st_pcsData		pcsData[PV_MAX_POWCN];		/* 計測情報（瞬時値格納用） */
	char					lineNo;						/* 通信回線番号 */
	struct timeval			nowTime;					/* 現在時刻 */
	struct timeval 			wrkTime;					/* 欠測判定用時刻 */

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
	memset( &s_pcsData, 0, sizeof(s_pcsData));
	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));

	/* 通信回線情報を取得 */
	/* Huawei、サンケン電気の通信プロトコル名と一致しない場合 */
	if( (retCode = commfunc_serial_infoget( lineNo, &s_serial, ps_g_COMLIINF, PV_COMPROTOCOL1 )) != 0 &&
		(retCode = commfunc_serial_infoget( lineNo, &s_serial, ps_g_COMLIINF, PV_COMPROTOCOL2 )) != 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [COMLINLIF.csv] Information Get Error Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );
		return( -1 );
	}

	/* パワーコンディショナ情報を取得 */
	if( (pcsMaxCnt = commfunc_devInfo_get( PV_MKIND_POWCN, lineNo, s_pcsTypeName, s_pcsInfo, &requestCnt, NULL, NULL, ps_g_POWCNINF )) <= 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [POWCNINF.csv] Not Exist Target Machine of Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

#ifdef PV_DEBUG
	printf("%s : PCSMAXCNT : %d\n", PV_OWN_NAME, pcsMaxCnt );
#endif

	/* 収集周期の調整と移動平均算出用データ数の取得 */
	commfunc_serial_cycletmchk( &s_serial, &stocMax, ps_g_COMLIINF );
	
	/* 移動平均用データの初期化 */
	commfunc_devAvlData_init( PV_MKIND_POWCN, stocMax, s_pcsAvlData );

	/* 移動平均用データから積算型計測値を復元する */
	retCode = commfunc_tmstsfil_get( PV_MKIND_POWCN, pcsMaxCnt, PV_PCSHWEI2_MAXTM, s_pcsInfo, s_pcsData, s_pcsAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );

	/* パワーコンディショナの装置状態を復元 */
	for( loopCnt1 = 0 ; loopCnt1 < pcsMaxCnt ; loopCnt1++ )
	{
		/* パワーコンディショナ番号の取得 */
		pcsNumb = s_pcsInfo[loopCnt1].devNumb;

		/* パワーコンディショナの装置状態を復元する */
		commfunc_sv_get( PV_MKIND_POWCN, pcsNumb, PV_MAX_POWCN_SVPOS, s_pcsData[loopCnt1].pcsStatus, ps_g_SVSTSFIL );
	}

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_RESET );

	/* シリアル通信スレッドを起動する */
	if( pthread_create( &threadID, NULL, func_pcsOpen_thread, NULL ) != 0 )
	{
		sprintf( errMsg, "Thread Execute Error" );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );
		return( -1 );
	}

	while( 1 )
	{

		/* 定期的に終了チェックを行う */
		if( commfunc_mswait( 1000, lineNo ) < 0 )
		{
#ifdef PV_DEBUG
			printf("%s : PROCESS STOP COMMAND RECV (LINE:%d)\n", PV_OWN_NAME, lineNo );
#endif
			/* 処理ループを抜ける */
			break;
		}
	}

	/* スレッドの終了を待つ */
	threadStop = 1;
	pthread_join( threadID, NULL );

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
 * パワーコンディショナＴＣＰ／ＩＰ通信接続処理
 *
 * 機能：
 *		引数 args で指定された情報内に格納されているパワーコンディショナに対
 *		して、接続を行う（オープンからコネクトまで）
 *		実際の接続処理は共通関数 commfunc_serial_open() を使用する。
 *		
 */
void *func_pcsOpen_thread( void *args )
{
														/* パワーコディショナへの送信コマンドベース */
	unsigned  char			sendBuff[12] = {
								0x00,0x00,				/* トランザクションID（要求フレーム毎に固有の値を使用） */
								0x00,0x00,				/* プロトコルID（MODBUS=0を使用） */
								0x00,0x06,				/* データ長 *//* [20200213] Chg */
								0x00,					/* 送信先局番 */
								0x00,					/* ファンクションコード（入力レジスタ読出） */
								0x00,0x00,				/* 開始アドレス */
								0x00,0x00				/* レジスタの数 */
							};						
/* [20200213] Add St */
	unsigned  char			sendCtlBuff[12] = {
								0x00,0x00,				/* トランザクションID（本装置では固定的に0を使用） */
								0x00,0x00,				/* プロトコルID（MODBUS=0を使用） */
								0x00,0x06,				/* データ長 */
								0x00,					/* 送信先局番（初期値） */
								0x00,					/* ファンクションコード（入力レジスタ読出） */
								0x00,0x00,				/* 開始アドレス */
								0x00,0x00				/* 設定値 */
							};
/* [20200213] Add Ed */
	int						openRetCode;				/* シリアルポートオープン結果 */
	int						dataLength;					/* 送信データサイズ */
	unsigned char			*p_sendBuff;				/* 送信バッファポインタ */
	unsigned char			readBuff[4096];				/* 受信バッファ */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */
	int						pcsNumb;					/* 処理中のパワーコンディショナ番号 */
	int						idNumb;						/* 処理中のパワーコンディショナ装置ＩＤ */				
	struct timespec			sleepTm;					/* 送信待ちタイマ */
	struct st_pcsHweiSdInf	*ps_pcsSdInf;				/* 送信情報ポインタ */
	int						setVal;						/* 設定値 */
/* [20200813] Add Ed */
/* [20200902] Add St */
	int						tmIfLinkBase;					/* 計測情報ファイルリンク */
	int						reqEnvFlg;						/* 環境モニタ機器要求フラグ */
/* [20200902] Add Ed */
	int						tmifLink;					/* 計測情報リンクポインタ */
	int						convExp;					/* 補正乗率 */
	int						tranID;					/* トランザクションID *//* [20210407] Add */
	int						setPer;						/* 出力制御値 */
	int						pvStartFlg;				/* PV運転開始フラグ */
	int						pvStopFlg;				/* PV運転停止フラグ */
	int						threadStop_local;			/* 停止フラグ(thread用) */
	int						svStat;					/* 状態 */
	int						svStatRes;				/* 応動SV状態 */

	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = s_serial.sndTime / 1000;
	sleepTm.tv_nsec = (s_serial.sndTime % 1000) * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( 1 )
	{
		/* 停止フラグを保持(ループの最初から停止フラグの情報を使用するようにここで保持する) */
		threadStop_local = threadStop;

		/* シリアルポートをオープン */
		if( (openRetCode = commfunc_serial_open( &s_serial )) >= 0 )
		{
			/* 回線に所属するパワーコンディショナの台数分繰り返す */
			for(loopCnt1 = 0 ; loopCnt1 < pcsMaxCnt ; loopCnt1++ )
			{
				/* 通信ステータス状変：ＬＡＮ接続異常（復帰） */
				commfunc_johen_stat( PV_MKIND_POWCN, s_pcsInfo[loopCnt1].devNumb, PV_ST14_LANERR, 0,
					(ps_g_POWCNINF+s_pcsInfo[loopCnt1].devNumb-1)->stStLink, ps_g_STSTSFIL );

				/* シリアルポートステータスを更新 */
				s_pcsData[loopCnt1].serialStatus = 0;
			}
		}
		else
		{
			
			/* 回線に所属するパワーコンディショナの台数分繰り返す */
			for(loopCnt1 = 0 ; loopCnt1 < pcsMaxCnt ; loopCnt1++ )
			{
				/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, s_pcsInfo[loopCnt1].devNumb, PV_ST14_LANERR, 1,
					(ps_g_POWCNINF+s_pcsInfo[loopCnt1].devNumb-1)->stStLink, ps_g_STSTSFIL );

				/* シリアルポートステータスを更新 */
				s_pcsData[loopCnt1].serialStatus = 1;
			}
			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 3 );
		}

/* [20200813] Add St */

/* [20200825] Del */

/* [20200813] End St */


		/* EDMMの場合、以下の動作処理を実装する */
		/* 制御指示値(収集装置の計測値)を出力値として設定する。 */
		/* 取得する共通関数を用意する */
		/*  補足 */
		/* 強制出力抑制(0%)の状態SV(収集装置)のOn/Offを確認するのは、制御管理プロセスが行う。 */
		/* 制御管理プロセスでON/OFFによって、制御指示値を変化させる。 */
		
		/* EDMM以外のパワコンは、以下の動作処理を実装する */
		/* ①PV運転停止制御(収集装置SVNo.31)のON/OFFで運転状態を制御する */
		/*   制御する前に状態を確認し、一致していたら制御しない。 */
		/*   また、通信異常から復帰時も状態のチェック/設定を行う */
		/*   故障などの場合は、一度停止するから大丈夫か？ */
		/*   緊急停止は、どういう感じで実施するのか？ */


		/* パワーコンディショナから情報を収集する */
		/* 当該通信回線に所属するパワーコンディショナの台数分繰り返す */
		for(loopCnt1 = 0 ; loopCnt1 < pcsMaxCnt ; loopCnt1++ )
		{
			/* 要求実施回数を初期化 */
			s_serial.requestCount = 0;

			/* パワーコンディショナ番号の取得 */
			pcsNumb = s_pcsInfo[loopCnt1].devNumb;
			/* パワーコンディショナ装置IDの取得 */
			idNumb  = s_pcsInfo[loopCnt1].idNumb ;


			/* SmartLoggerの場合 */
			pvStartFlg = 0;
			pvStopFlg = 0;
			if( s_pcsInfo[loopCnt1].devType == PV_PCSHWEI2_TYPE1 )
			{
				/* 状態取得：PV運転開始制御 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_RUN_START, &svStat, ps_g_SVSTSFIL);
				/* 状態取得：PV運転開始状態 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_ALLSART, &svStatRes, ps_g_SVSTSFIL);

				/* 運転開始指示があり、且つ、運転開始状態でない場合 */
				if( ( svStat == 1 ) &&
					( svStatRes != 1 ) )
				{
					pvStartFlg = 1;
#ifdef PV_DEBUG_CTL
					printf("%s : RUNSV(%d), svStat=%d\n", PV_OWN_NAME, PV_SYS_SV_PV_RUN_START, svStat );
#endif
				}

				/* 状態取得：PV運転停止制御 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_RUN_STOP, &svStat, ps_g_SVSTSFIL);
				/* 状態取得：PV運転停止状態 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_ALLSTOP, &svStatRes, ps_g_SVSTSFIL);

				/* 運転停止指示があり、且つ、運転停止状態でない場合 */
				if( ( svStat == 1 ) &&
					( svStatRes != 1 ) )
				{
					pvStopFlg = 1;
#ifdef PV_DEBUG_CTL
					printf("%s : RUNSV(%d), svStat=%d\n", PV_OWN_NAME, PV_SYS_SV_PV_RUN_STOP, svStat );
#endif
				}
			}


			/* 処理タイプの設定 */
			switch( s_pcsInfo[loopCnt1].devType )
			{
				/* パワーコンディショナ */
				case PV_PCSHWEI2_TYPE2 :
					/* 送信情報を処理タイプ２に設定 */
					ps_pcsSdInf = s_type2SdInf;
					break;
				case PV_PCSHWEI2_TYPE1 :
					/* 送信情報を処理タイプ１に設定 */
					ps_pcsSdInf = s_type1SdInf;
/* [20200902] Add St */
					reqEnvFlg = 0;
					tmIfLinkBase = ps_g_POWCNINF[pcsNumb - 1].tmIfLink;
					for( loopCnt2 = PV_PCSHWEI2_ENV_TMNUMB_S; loopCnt2 <= PV_PCSHWEI2_ENV_TMNUMB_E; loopCnt2++ )
					{
						/* 環境モニタ機器の計測情報が実装の場合は、要求フラグをONにする */
						if( ps_g_TMDATINF[ tmIfLinkBase + loopCnt2 -1 ].statFlag == 1 )
						{
							/* 環境モニタ要求フラグをON */
							reqEnvFlg = 1;
						}
					}
/* [20200902] Add Ed */
					break;
/* [20200728] Add St */
				case PV_PCSHWEI2_TYPE3 :
					/* 送信情報を処理タイプ３に設定 */
					ps_pcsSdInf = s_type3SdInf;
					break;
/* [20200728] Add Ed */
/* [20230130] Add St */
				case PV_PCSHWEI2_TYPE4 :
					/* 送信情報を処理タイプ４に設定 */
					ps_pcsSdInf = s_type4SdInf;
					break;
/* [20230130] Add Ed */
				default :
					/* 送信情報を処理タイプ１に設定 */
					ps_pcsSdInf = s_type1SdInf;
					break;
			}

			/* 処理結果を初期化 */
			retCode = 0;
			/* 複数回に分けて情報を収集する */
			for( loopCnt2 = 0 ; ps_pcsSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
			{

#ifdef PV_DEBUG
			printf("%s : LOOP:%02d PCSNO:%02d PCSID-No:%02d REQUEST:%d OPN-STS:%d\n", PV_OWN_NAME, loopCnt1+1, pcsNumb, idNumb, loopCnt2+1, s_serial.status );
#endif

				/* ＴＣＰ／ＩＰ通信ポートオープン正常の場合、且つ、前回の通信が正常の場合 */
				if( ( s_serial.status == PV_STAT_COM_CONNECT ) && ( retCode == 0 ) )
				{
#ifdef PV_DEBUG_SERIAL
					printf("%s : SERIAL POERT OPENED\n", PV_OWN_NAME);
#endif
/* [20200213] Chg st */
					/* 送信バッファ、送信データサイズの設定 */
					if( ps_pcsSdInf[loopCnt2].funcCode == PV_PCSHWEI2_FUNC_03 )
					{
						p_sendBuff = &sendBuff[0];
						
					}
					else
					{
						p_sendBuff = &sendCtlBuff[0];
					}
/* [20200213] Chg Ed */

					/* 送信データサイズ */
					dataLength = ps_pcsSdInf[loopCnt2].dataLength;

					/* 送信識別子を設定する */
/* [20210407] Chg St */
					p_sendBuff[0] = idNumb;
					p_sendBuff[1] = loopCnt2+1;
					tranID = (p_sendBuff[0] * 0x100) + p_sendBuff[1];
/* [20210407] Chg Ed */

/* [20200213] Del */
/* [20200902] Chg st */
					/* 要求が環境モニタ機器の場合 */
					if( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_ENV )
					{
						/* 環境モニタ機器のデバイスIDを設定 */
						p_sendBuff[6] = PV_PCSHWEI2_ENV_DEVICEID;
						/* 環境モニタ送信フラグがOFFの場合、スキップ */
						if( reqEnvFlg == 0 )
						{
							continue;
						}
					}
					else
					{
						/* 送信先局番設定する */
						p_sendBuff[6] = idNumb;
					}
/* [20200902] Chg Ed */

					/* 送信バッファにファンクションと取得開始アドレス、取得データ数を設定 */
					p_sendBuff[7] = ps_pcsSdInf[loopCnt2].funcCode;
					p_sendBuff[8] = ps_pcsSdInf[loopCnt2].startAdU;
					p_sendBuff[9] = ps_pcsSdInf[loopCnt2].startAdL;

					/* 読み込みの場合 */
					if( ps_pcsSdInf[loopCnt2].funcCode == PV_PCSHWEI2_FUNC_03 )
					{
						p_sendBuff[10] = ps_pcsSdInf[loopCnt2].dataCntU;
						p_sendBuff[11] = ps_pcsSdInf[loopCnt2].dataCntL;
						
					}
					/* 書き込みの場合 */
					else if( ps_pcsSdInf[loopCnt2].funcCode == PV_PCSHWEI2_FUNC_06 )
					{
						/* 運転開始/停止の場合 */
						if( ( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE1 ) ||
							( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE2 ) )
						{
							/* 運転開始指示の場合 */
							if( pvStartFlg == 1 )
							{
								/* 書き込みデータを設定 */
								if( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE1 )
								{
									setVal = 0;
								}
								else if( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE2 )
								{
									setVal = 1;
								}
							}
							/* 運転停止指示の場合 */
							else if( pvStopFlg == 1 )
							{
								/* 書き込みデータを設定 */
								if( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE1 )
								{
									setVal = 1;
								}
								else if( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE2 )
								{
									setVal = 0;
								}
							}
							else
							{
								continue;
							}
							p_sendBuff[10] = setVal / 0x100;
							p_sendBuff[11] = setVal % 0x100;
						}

						/* 出力制御の場合 */
						if( ps_pcsSdInf[loopCnt2].reqKind == PV_PCSHWEI2_REQ_WRITE3 )
						{
							/* スレッド停止フラグがONの場合は、発電を停止する */
							if( threadStop_local != 0 )
							{
								setPer = 0;
							}
							else
							{
								/* 出力制御値を取得 */
								commfunc_pvctl_pvPerGet( &setPer, ps_g_TMSTSFIL);
							}
							// 出力制御値(0.01%)単位を0.1%単位に変更し、設定する
							p_sendBuff[10] = (unsigned char)((setPer /10) / 0x100);
							p_sendBuff[11] = (unsigned char)((setPer /10) & 0xFF);

#ifdef PV_DEBUG_DT4
							printf("%s : CONTROL PCS:[%d] (ID:%03d) Request setPer=%d, buff[10][11]=0x%02x,0x%02x\n", PV_OWN_NAME, 
											pcsNumb, idNumb, setPer, p_sendBuff[10], p_sendBuff[11]);
#endif
						}

					}

					/* 送信待ち処理 */
					nanosleep( &sleepTm, NULL);
	
					/* 処理結果の初期値を異常に設定 */
					retCode = -1;
	
					/* 要求実施回数の上限まで繰り返す */
					/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
					while( s_serial.requestCount < PV_SERIAL_REQUEST_MAX )
					{								
						/* シリアル通信ログ領域を初期化 */
						s_serialLog.dataCnt = 0;
		
						/* パワーコンディショナに要求を送信 */
						retCode = commfunc_serial_send( &s_serial, p_sendBuff, dataLength, &s_serialLog );
						if( retCode < 0 )
						{
							sprintf( errMsg, "TCP Port Write Error! RET:%d num/id:%02d/%02d (%s)",
								retCode, pcsNumb, idNumb, s_serial.targetName );
							commfunc_syslog_msgset( pv_own_name, errMsg );
	
							/* データ送信異常（送信不可異常）発生 */
							/* 状変（通信ステータス状変）発生 */
							commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 1,
								(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
			
							/* 再要求不要のため処理を抜ける */
							break;
						}
						else
						{
							/* データ送信異常（送信不可異常）復帰 */
							/* 状変（通信ステータス状変）復帰 */
							commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 0,
								(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
						}
				
						/* データ処理バッファをクリア */
						memset(readBuff, 0, sizeof(readBuff));
	
						/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
/* [20210407] Chg St */
/* [20200902] Chg St */
						retCode = func_pcsData_recv(	&s_serial, tranID, pcsNumb, p_sendBuff[6],
														ps_pcsSdInf[loopCnt2].funcCode,
														ps_pcsSdInf[loopCnt2].dataCntU * 256 + ps_pcsSdInf[loopCnt2].dataCntL,
														p_sendBuff, readBuff, &s_serialLog );
/* [20200902] Chg Ed */
/* [20210407] Chg Ed */
						if( retCode == 0 )
						{			
							/* 正常データ受信完了 */
							/* ＴＣＰ／ＩＰ通信ログを出力 */
							commfunc_serial_logout( PV_SERIAL_LOG_AUTO, &s_serial, &s_serialLog );
		
							/* 通信ステータス状変：全ステータス（復帰） */
							commfunc_johen_resetstat( PV_MKIND_POWCN, pcsNumb,
								(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
					    
							/* 再要求不要のため処理を抜ける */
							break;
						}
						else
						{					
							/* ＴＣＰ／ＩＰ通信ログを出力 */
							commfunc_serial_logout( PV_SERIAL_LOG_ON, &s_serial, &s_serialLog );
							/* タイムアウト以外の場合は再要求不要のため処理を抜ける */
							if( retCode != PV_SERIAL_ERRTIMEOUT )
							{
								break;
							}
						}
					}
				}
				else
				{
#ifdef PV_DEBUG_SERIAL
					printf("%s : SERIAL POERT NOT-OPENED STATUS[%d]\n", PV_OWN_NAME, s_serial.status );
#endif
					/* ＴＣＰ／ＩＰポートが異常の場合 */
					retCode = -1;
				}

				/* 処理結果が正常応答の場合 */
				if(retCode == 0)
				{
					/* 現在時刻を取得 */
					s_pcsData[loopCnt1].checkTime = time( NULL );
					/* 読み込みの場合 */
					if( ps_pcsSdInf[loopCnt2].funcCode == PV_PCSHWEI2_FUNC_03 )
					{
						/* データ処理バッファから装置状態と計測値を抽出 */
						retCode = func_pcsData_get( s_pcsInfo[loopCnt1].devType, loopCnt2 + 1, pcsNumb, retCode, &s_pcsData[loopCnt1], readBuff);
					}
				}
				else
				{
					if( ps_pcsSdInf[loopCnt3].funcCode == PV_PCSHWEI2_FUNC_03 )
					{
						/* データ処理バッファから装置状態と計測値を抽出 */
						func_pcsData_get( s_pcsInfo[loopCnt1].devType, loopCnt2 + 1, pcsNumb, retCode, &s_pcsData[loopCnt1], readBuff);
					}
				}
			}

			/* 計測種別毎の保持 */
			retCode = commfunc_devTmData_set( PV_MKIND_POWCN, pcsNumb, PV_MAX_POWCN_TMPOS, &s_pcsData[loopCnt1], &s_pcsAvlData[loopCnt1], ps_g_TMDATINF );

			/* 計測情報を計測値格納ファイルに保存 */
			retCode = commfunc_tmstsfil_devset( PV_MKIND_POWCN, pcsNumb, PV_MAX_POWCN_TMPOS, &s_pcsData[loopCnt1], &s_pcsAvlData[loopCnt1], ps_g_TMDATINF, ps_g_TMSTSFIL );
		}

		/* スレッド停止フラグがONの場合 */
		if( threadStop_local != 0 )
		{
			break;
		}
	}
	/* シリアルポートをクローズする */
	commfunc_serial_close( &s_serial );

	/* スレッド終了処理 */
	return( NULL );
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
int	func_pcsData_recv(
	struct st_serialinf *ps_serial,
	int tranID,
	int pcsNumb,
	int targetID,
	int reqFunc,
	int reqCnt,
	unsigned char *sendBuff,
	unsigned char *dataBuff,
	struct st_serialLog *ps_serialLog )
{
	fd_set			rfds;					/* ファイルディスクリプタ集合 */
	unsigned char	readBuff[4096];			/* リードバッファ */
	char			flag_GOD = 0;			/* 受信完了フラグ */
	char			*workPoint;				/* データ格納バッファワークポインタ */
	int				readCnt = 0;			/* データ受信バイト数 */
	int				totalLength = 0;		/* 受信データ量 */
	int				dataLength = 0;			/* 受信データ長 */
	int				checkLength = 0;		/* 受信データ長（期待値） */
	int				dataByte = 0;			/* 受信データ内データバイト数（期待値） */
	struct timeval	timedata;				/* 現在時刻取得用 */
	struct timeval	timeout;				/* 応答タイムアウト時刻 */
	int				loopCnt;				/* ループカウンタ */


	/* シリアルポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

	switch( reqFunc )
	{
		case PV_PCSHWEI2_FUNC_03:
			dataByte = reqCnt * 2;
			checkLength = dataByte + 9;
			break;
		case PV_PCSHWEI2_FUNC_06:
			checkLength = 12;
			break;
		case PV_PCSHWEI2_FUNC_10:
		default :
			dataByte = reqCnt * 2;
			checkLength = dataByte + 9;
			break;
	}

#ifdef PV_DEBUG
	printf("%s : Data-Recv TargetID: %d  (length:%d)  START\n", PV_OWN_NAME, targetID,checkLength);
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
					sprintf( errMsg, "Serial Port Not Open! Numb:%02d Id:%02d (%s)", pcsNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRNOTREADY );
					break;
				case PV_SERIAL_ERRTIMEOUT :
					sprintf( errMsg, "Read Timeout! DataLen:%d Numb:%02d Id:%02d (%s)", totalLength, pcsNumb, targetID, ps_serial->targetName);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 要求実施回数を＋１ */
					ps_serial->requestCount += 1;
					/* 要求実施回数が上限に達している場合 */
					if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
					{
						/* 通信ステータス状変：通信不応答異常（発生） */
						commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST01_NONRESPONSE, 1,
							(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", pcsNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
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
			/* ９バイト分のデータを受信したら先頭データのチェックを行う */
			if( dataLength == 9 )
			{
				/* トランザクションIDの確認（データフレーム毎に固有の値を使用） */
				if( ( *dataBuff != (unsigned char)(tranID / 0x100) ) || ( *(dataBuff + 1) != (unsigned char)(tranID % 0x100) ) )		/* [20210407] Chg */
				{
#ifdef PV_DEBUG_SERIAL
printf("<TRAN-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv TransactionID Error! Numb:%02d Expect[00][%02X],Result[%02X][%02X]", pcsNumb, tranID, *dataBuff, *(dataBuff+1) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* プロトコルIDの確認（本装置では固定的に0x0000を使用） */
				else if( ( *(dataBuff + 2) != (unsigned char)0 ) || ( *(dataBuff + 3) != (unsigned char)0 ) )
				{
#ifdef PV_DEBUG_SERIAL
printf("<PROT-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d Expect[00][00],Result[%02X][%02X]", pcsNumb, *(dataBuff+2), *(dataBuff+3) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* 送信先（送信元）アドレスのチェック */
				else if(*(dataBuff + 6) != (unsigned char)targetID)
				{
#ifdef PV_DEBUG_SERIAL
printf("<UNIT-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv Address Error! Numb:%02d Expect[%02X],Result[%02X]", pcsNumb, targetID, *(dataBuff+6) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* ファンクションコードが不一致の場合 */
				else if ( *(dataBuff+7) != (unsigned char)reqFunc ) 
				{
#ifdef PV_DEBUG_SERIAL
printf("<FUNC ERROR>");
#endif
					sprintf( errMsg, "Data-Recv function Error! Numb:%02d Expect[%02X],Result[%02X]", pcsNumb, reqFunc, *(dataBuff+7) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 例外レスポンスの場合 */
					if( (*(dataBuff+7) & (unsigned char)0x80) == (unsigned char)0x80 )
					{
						/* 受信データ長（期待値）を９に変更 */
						checkLength = 9;
					}
					else
					{
						/* 受信データの初期化 */
						dataLength = 0;
					}
				}
				/* ファンクションコードが０３でデータバイト数が異なる場合 */
				else if ( (reqFunc == PV_PCSHWEI2_FUNC_03) && ( ( *(dataBuff+5) != (dataByte + 3) ) || (  *(dataBuff+8) != dataByte ) ) )
				{
					sprintf( errMsg, "Data-Recv datasize Error! Numb:%02d Expect[%02X][%02X],Result[%02X][%02X]",
						pcsNumb, dataByte + 3, dataByte, *(dataBuff+5), *(dataBuff+8) );
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
		/* 受信データが例外レスポンスの場合（ */
		if( checkLength == 9 )
		{
#ifdef PV_DEBUG
			printf("%s : Data-Poling Error\n", PV_OWN_NAME);
#endif
			/* 例外コードが不正ファンクションの場合 */
			if( *(dataBuff+8) == (unsigned char)0x01 )
			{
				/* 通信ステータス状変：下りコマンド異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST19_DWCOMMANDERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST22_DWPARAMERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST23_DWDATAERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードがスレーブデバイス異常の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x04 )
			{
				/* 通信ステータス状変：デバイス異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST16_DEVICEERROR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが肯定応答の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x05 )
			{
				/* 通信ステータス状変：発生なし */
				/* 通信異常として対応 */
				return( PV_SERIAL_ERROTHER );
			}	
			/* 例外コードがデバイスビジー状態の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x06 )
			{
				/* 通信ステータス状変：デバイス異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST16_DEVICEERROR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードがパリティエラーの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x08 )
			{
				/* 通信ステータス状変：下りパリティ異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST24_DWPARITYERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードがゲートウェイ異常の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x0A )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST22_DWPARAMERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードがデバイス応答失敗の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x0B )
			{
				/* 通信ステータス状変：通信無応答異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST01_NONRESPONSE, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが権限なしの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x80 )
			{
				/* 通信ステータス状変：下り制御異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST27_DWCONTROLERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 上記以外の例外レスポンスの場合 */
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST20_DWFORMATERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
		}
/* [20190523] Add St */
		else if( reqFunc == PV_PCSHWEI2_FUNC_06 )
		{
			/* 書込みデータと一致しない場合は、異常 */
			if( ( sendBuff[8]  != *(dataBuff+8 ) ) ||
				( sendBuff[9]  != *(dataBuff+9 ) ) ||
				( sendBuff[10] != *(dataBuff+10) ) ||
				( sendBuff[11] != *(dataBuff+11) ) )
			{
				/* 要求実施回数を＋１ */
				ps_serial->requestCount += 1;
				/* 通信ステータス状変：上りデータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST04_UPDATAERR, 1,
					(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
				return PV_SERIAL_ERROTHER;
			}
		}
/* [20190523] Add Ed */

	}
	else
	{
		/* 要求実施回数を＋１ */
		ps_serial->requestCount += 1;
		/* 要求実施回数が上限に達している場合 */
		if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
		{
			/* 通信ステータス状変：通信不応答異常（発生） */
			commfunc_johen_stat( PV_MKIND_POWCN, pcsNumb, PV_ST01_NONRESPONSE, 1,
				(ps_g_POWCNINF+pcsNumb-1)->stStLink, ps_g_STSTSFIL );
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
 * 		引数 pcsNumb で指定されたパワーコンディショナについて以下の処理を行う。
 * 		引数 dataBuff で指定された受信バッファの内容から装置状態と計測値を取得する。
 * 		受信結果 resultCode が異常の場合は、計測値を０、計測ステータスを欠測に設定
 * 		する。
 * 		受信結果 resultCode が正常な場合は受信バッファのASCIIデータから装置状態と
 * 		計測値を取得し、計測ステータスを生成する。
 * 		装置状態が取得できた場合は、状態更新処理を行う。
 * 戻り値：
 * 		0：正常処理
 */
int	func_pcsData_get( int pcsType, int reqCnt, int pcsNumb, int resultCode, struct st_pcsData *ps_pcsData, unsigned char *dataBuff)
{
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char			dataByte[8];		/* データ格納用領域 */
		short int				data16bit;			/* 16bitデータ取得用 */
		unsigned short int		data16bitu;			/* 16bitデータ取得用（符号無し） */
		int						data32bit;			/* 32bitデータ取得用 */
	} u_change;
	unsigned char	workStatus[4];					/* 装置状態一時保管用（2バイト分）*/
	int				codNumb;						/* 受信バッファ内の状態番号 */
	int				stsNumb;						/* 装置の状態番号 */
	int				tmNumb;							/* 計測情報番号 */
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	int				loopCnt3;						/* ループカウンタ */
	int				bitMask;						/* ビットマスク */
	int				tmIfLinkBase;					/* 計測情報ファイルリンク */
	int				svStLinkBase;					/* 状態情報ファイルリンク */
	int				svIfLinkBase;					/* 装置状態情報ファイルリンク */
	struct st_pcsHweiSvInf	*ps_pcsSvInf;			/* 装置状態情報取得情報 */
	struct st_pcsHweiTmInf	*ps_pcsTmInf;			/* 計測値用情報取得情報 */
	struct st_devTmRange	*ps_pcsTmRange;			/* 計測値範囲情報 */
	struct st_stsMaskTbl	*ps_pcsMaskTbl;			/* 状態マスク情報 */
	struct st_stsCodeTbl	*ps_pcsCodeTbl;			/* 数値変換情報 */
	struct timeval			nowtime;				/* 現在時刻 */

	/* 格納位置を取得 */
	tmIfLinkBase = ps_g_POWCNINF[pcsNumb - 1].tmIfLink;
	svStLinkBase = ps_g_POWCNINF[pcsNumb - 1].svStLink;
	svIfLinkBase = ps_g_POWCNINF[pcsNumb - 1].svIfLink;

#ifdef PV_DEBUG_DT1
	printf("%s : PCS[ %02d ] TYPE[%d] REQ[%d] COD[%d] LINK[TMST:%6d, SVST:%6d, SVIF:%6d]\n", PV_OWN_NAME,
		pcsNumb, pcsType, reqCnt, resultCode, tmIfLinkBase, svStLinkBase, svIfLinkBase );
#endif

	/* SmartLogger */
	if( pcsType == PV_PCSHWEI2_TYPE1 )
	{	
		ps_pcsSvInf = s_type1SvInf;
		ps_pcsTmInf = s_type1TmInf;
		ps_pcsTmRange  = s_type1TmRange;
	}
	/* パワーコンディショナ */
	else if( pcsType == PV_PCSHWEI2_TYPE2 )
	{
		ps_pcsSvInf = s_type2SvInf;
		ps_pcsTmInf = s_type2TmInf;
		ps_pcsTmRange  = s_type2TmRange;
	}
/* [20200728] Add St */
	else if( pcsType == PV_PCSHWEI2_TYPE3 )
	{
		ps_pcsSvInf = s_type3SvInf;
		ps_pcsTmInf = s_type3TmInf;
		ps_pcsTmRange  = s_type3TmRange;
	}
/* [20200728] Add Ed */
/* [20230130] Add St */
	else if( pcsType == PV_PCSHWEI2_TYPE4 )
	{
		ps_pcsSvInf = s_type4SvInf;
		ps_pcsTmInf = s_type4TmInf;
		ps_pcsTmRange  = s_type4TmRange;
	}
/* [20230130] Add Ed */
	else
	{
		/* 応答情報を処理タイプ１に設定 */
		ps_pcsSvInf = s_type1SvInf;
		ps_pcsTmInf = s_type1TmInf;
		ps_pcsTmRange  = s_type1TmRange;
	}
	
	/* ------------------ */
	/* 装置状態情報の取得 */
	/* ------------------ */

	/* 受信処理結果が正常の場合のみ装置状態を取得 */
	if( resultCode == 0 )
	{
		/* ストッパーまで繰り返す */
		for( loopCnt1 = 0 ; ps_pcsSvInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
		{
			/* 取得情報の要求回数が一致しない場合は次のデータへ */
			if( reqCnt != ps_pcsSvInf[loopCnt1].reqCnt )
				continue;

			/* 処理タイプがビットタイプの場合 */
			if( ps_pcsSvInf[loopCnt1].svType == PV_SVTYPE_BIT )
			{

#ifdef PV_DEBUG_DT1
	printf("%s : BIT-TYPE\n", PV_OWN_NAME);
#endif

				/* 初期化が必要な場合 */
				if( ps_pcsSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink ], 0, ps_pcsSvInf[loopCnt1].iniSize );
				}

				/* 指定バイト数分コピーする */
				for( loopCnt2 = 1 ; loopCnt2 <= ps_pcsSvInf[loopCnt1].dataSize ; loopCnt2++ )	
				{
					/* 状態を取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
					workStatus[loopCnt2 - 1] = *(dataBuff + ps_pcsSvInf[loopCnt1].buffLink + (ps_pcsSvInf[loopCnt1].dataSize - loopCnt2));

					/* ビット数分繰り返す（MSB-LSB変換） */
					for( bitMask = 1, loopCnt3 = 0 ; loopCnt3 < 8 ; loopCnt3++, bitMask<<=1 )
					{
						if( workStatus[loopCnt2 - 1] & bitMask )
							ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2 - 1 ] |= (unsigned char)(0x80 >> loopCnt3);	

#ifdef PV_DEBUG_DT1
	printf("%s :    SV-STS[%2d] : %02X\n", PV_OWN_NAME, ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2 - 1, (short int)ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2 - 1 ] );
#endif
					}
				}
			}
			/* 処理タイプがマスクタイプの場合 */
			else if( ps_pcsSvInf[loopCnt1].svType == PV_SVTYPE_MSK )
			{

#ifdef PV_DEBUG_DT1
	printf("%s : MSK-TYPE\n", PV_OWN_NAME);
#endif
				/* マスクテーブルポインタを取得 */
				ps_pcsMaskTbl = (struct st_stsMaskTbl *)ps_pcsSvInf[loopCnt1].ps_tblPointer;

				/* 初期化が必要な場合 */
				if( ps_pcsSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink ], 0, ps_pcsSvInf[loopCnt1].iniSize );
				}

				/* 状態テーブルの設定数分繰り返す */
				for( loopCnt2 = 0 ; ps_pcsMaskTbl[loopCnt2].buffLink != -1 ; loopCnt2++ )
				{
					/* ビットが設定されている場合 */
					if( *(dataBuff + ps_pcsMaskTbl[loopCnt2].buffLink) & ps_pcsMaskTbl[loopCnt2].svMask )
					{
						/* ビット状態を設定 */
						ps_pcsData->pcsStatus[ ps_pcsMaskTbl[loopCnt2].pcsSvLink ] |= ps_pcsMaskTbl[loopCnt2].setMask;
					}
				}

#ifdef PV_DEBUG_DT1
	for(loopCnt2 = 0 ; loopCnt2 < ps_pcsSvInf[loopCnt1].iniSize ; loopCnt2++ )
	printf("%s :    SV-STS[%2d] : %02X\n", PV_OWN_NAME, ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2, ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2 ] );
#endif
			}
			/* 処理タイプが数値タイプの場合 */
			else if( ps_pcsSvInf[loopCnt1].svType == PV_SVTYPE_NUM )
			{
				/* コード変換テーブルポインタを取得 */
				ps_pcsCodeTbl = (struct st_stsCodeTbl *)ps_pcsSvInf[loopCnt1].ps_tblPointer;

				/* 初期化が必要な場合 */
				if( ps_pcsSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink ], 0, ps_pcsSvInf[loopCnt1].iniSize );
				}

				/* 状態・コード番号の取得（ビッグエンディアンからリトルエンディアンへの配置変更）*/
				for( loopCnt2 = 1 ; loopCnt2 <= ps_pcsSvInf[loopCnt1].dataSize ; loopCnt2++ )
					u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_pcsSvInf[loopCnt1].buffLink + ps_pcsSvInf[loopCnt1].dataSize - loopCnt2];

				/* サイズが２バイトサイズの場合 */
				if( ps_pcsSvInf[loopCnt1].dataSize == 2 )
				{
					/* 16bitサイズ（符号無し）のデータとして数値を取得（コード番号0～65535に対応） */
					codNumb = u_change.data16bitu;
				}
				/* サイズが４バイトサイズの場合 */
				else if( ps_pcsSvInf[loopCnt1].dataSize == 4 )
				{
					/* 32bitサイズのデータとして数値を取得 */
					codNumb = u_change.data32bit;
				}
				else
				{
					/* 8bitサイズのデータとして数値を取得 */
					codNumb = u_change.dataByte[0];
				}

				/* コード番号から状態番号を取得 */
				for( stsNumb = -1, loopCnt2 = 0 ; loopCnt2 <= 99; loopCnt2++ )
				{
					/* 変換テーブルのコード番号がストッパーの場合 */
					if( ps_pcsCodeTbl[loopCnt2].codNumb == -1 )
					{
						/* 状態番号を設定 */
						stsNumb = ps_pcsCodeTbl[loopCnt2].stsNumb;
						break;
					}
					/* 変換テーブルのコード番号が一致する場合 */
					if( ps_pcsCodeTbl[loopCnt2].codNumb == codNumb )
					{
						/* 状態番号を設定 */
						stsNumb = ps_pcsCodeTbl[loopCnt2].stsNumb;
						break;
					}
				}

#ifdef PV_DEBUG_DT1
				printf("%s : NUM-TYPE CODE[ %d ] SV-NUM[ %d ]\n", PV_OWN_NAME, codNumb, stsNumb );
#endif

				/* 状態番号が１以上の場合 */
				if( stsNumb > 0 )
				{
					/* 状態を更新 */
					ps_pcsData->pcsStatus[ (stsNumb - 1) / 8 ] |= (unsigned char)0x80 >> ((stsNumb - 1) % 8);
				}
				/* コード番号に該当する状態番号が見つからなかった場合 */
				else if( stsNumb == -1 )
				{
					sprintf( errMsg, "SV Status Number Not Found !! PCS-No[%02d] Type[%d] Buff[%d] Size[%d] NUMBER[%d]",
					pcsNumb, pcsType, ps_pcsSvInf[loopCnt1].buffLink, ps_pcsSvInf[loopCnt1].dataSize, codNumb );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
			}

#ifdef PV_DEBUG_DT1
//			for( loopCnt2 = 0 ; loopCnt2 < sizeof(ps_pcsData->pcsStatus); loopCnt2++ )
//			{
//				printf("%s : SV-STS[%2d] : %02X\n", PV_OWN_NAME,
//					ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2, ps_pcsData->pcsStatus[ ps_pcsSvInf[loopCnt1].pcsSvLink + loopCnt2 ] );
//			}
#endif
		}

		/* 装置状態情報の更新（ビットステータス形式で更新） */
		commfunc_johen_sv( PV_MKIND_POWCN, pcsNumb, PV_MAX_POWCN_SVPOS, ps_pcsData->pcsStatus,
			svIfLinkBase, svStLinkBase, 
			ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	/* ------------------------------------ */
	/* 通常形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */

/* [20220721] Chg St */
	for( loopCnt1 = 0 ; ps_pcsTmInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
	{
		/* 取得情報の要求回数が一致しない場合は次のデータへ */
		if( reqCnt != ps_pcsTmInf[loopCnt1].reqCnt )
			continue;

		/* 計測情報番号を取得 */
		tmNumb = ps_pcsTmInf[loopCnt1].pcsTmNumb - 1;

		/* 受信結果が異常 または 計測情報が未実装の場合 */
		if( ( resultCode != 0 ) || ( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 ) )
		{
			/* 未実装の場合 */
			if( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 )
			{
				/* 計測ステータスをリセット */
				ps_pcsData->tmFlgData[tmNumb] = 0;
			}
			else
			{
				/* 計測ステータスに欠測を設定 */
				ps_pcsData->tmFlgData[tmNumb] = PV_TMFLG_DROPTM;
			}
			/* 計測値を０に設定 */
			ps_pcsData->tmValData[tmNumb] = 0;

#ifdef PV_DEBUG_DT1
		printf("%s : TM[%2d] ERR FLG:VAL[%Xh:%9d]  NAME:[%s]\n", PV_OWN_NAME,
			tmNumb+1, ps_pcsData->tmFlgData[tmNumb], ps_pcsData->tmValData[tmNumb], ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
			continue;
		}
		/* 実装の場合 */
		else
		{
			/* 計測フラグをリセット */
			ps_pcsData->tmFlgData[tmNumb] = 0;
			/* 計測値を初期化 */
			ps_pcsData->tmValData[tmNumb] = 0;

			/* 変換用領域の初期化 */
			memset( &u_change, 0, sizeof(u_change));

			/* 計測値の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
			for( loopCnt2 = 1 ; loopCnt2 <= ps_pcsTmInf[loopCnt1].dataSize ; loopCnt2++ )
				u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_pcsTmInf[loopCnt1].buffLink + ps_pcsTmInf[loopCnt1].dataSize - loopCnt2];
			
			/* 計測値のサイズが２バイトサイズの場合 */
			if( ps_pcsTmInf[loopCnt1].dataSize == 2 )
			{
				/* 符号付き判定 */
				if( ps_pcsTmInf[loopCnt1].signedFlg == 1 )
				{
					/* 16bitサイズのデータとして計測値を取得 */
					ps_pcsData->tmValData[tmNumb] = u_change.data16bit;
				}
				else
				{
					/* 符号なし16bitサイズのデータとして計測値を取得 */
					ps_pcsData->tmValData[tmNumb] = u_change.data16bitu;
				}
				
			}
			/* 計測値のサイズが４バイトサイズの場合 */
			else if( ps_pcsTmInf[loopCnt1].dataSize == 4 )
			{
				/* 32bitサイズのデータとして計測値を取得 */
				ps_pcsData->tmValData[tmNumb] = u_change.data32bit;
			}
			else
			{
				/* 暫定的に16bitサイズのデータとして計測値を取得 */
				ps_pcsData->tmValData[tmNumb] = u_change.data16bit;
			}
					
			/* 計測値が既定値の範囲内か？ */
			if( ( ps_pcsTmRange[tmNumb].tmLower > ps_pcsData->tmValData[tmNumb] ) ||
				( ps_pcsTmRange[tmNumb].tmUpper < ps_pcsData->tmValData[tmNumb] ) )
			{
				/* 計測異常２をセット */
				ps_pcsData->tmFlgData[tmNumb] = PV_TMFLG_TMERR2;
			}
		}

#ifdef PV_DEBUG_DT1
		printf("%s : TM[%2d] NOR FLG:VAL[%Xh:%9d] UP[%9d] LW[%9d] NAME:[%s]\n", PV_OWN_NAME, 
			tmNumb+1, ps_pcsData->tmFlgData[tmNumb], ps_pcsData->tmValData[tmNumb], ps_pcsTmRange[tmNumb].tmUpper, ps_pcsTmRange[tmNumb].tmLower, ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
	}
/* [20220721] Chg Ed */

	/* ------------------------------------ */
	/* 特殊形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */
	/* ＨＵＡＷＥＩ２では特になし           */
	/* ------------------------------------ */

	return( 0 );
}

/* [20200813] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 設定する有効電力値の取得
 *
 * 機能：
 * 		現在の有効電力値から設定する有効電力値を算出する
 * 戻り値：
 * 		設定する有効電力値
 */
int	func_getTmPowSav( int nowPer, int reqPer, int stepPer )
{
	int		setTmData;

	/* ＋方向に変化させる場合 */
	if( nowPer < reqPer)
	{
		/* ステップ値を加算しても指令値を上回らない場合 */
		if( (nowPer + stepPer ) < reqPer )
		{
			/* ステップ値を加算する*/
			setTmData = nowPer + stepPer;
		}
		else
		{
			/* 出力指令値を設定する*/
			setTmData = reqPer;
		}
	}
	/* －方向に変化させる場合 */
	else
	{
		/* ステップ値を原産しても指令値を下回らない場合 */
		if( (nowPer - stepPer ) > reqPer )
		{
			/* ステップ値を原産する*/
			setTmData = nowPer - stepPer;
		}
		else
		{
			/* 出力指令値を設定する*/
			setTmData = reqPer;
		}
	}

	return setTmData;
}
/* [20200813] Add Ed */

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


