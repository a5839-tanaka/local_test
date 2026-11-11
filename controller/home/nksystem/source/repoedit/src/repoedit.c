/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * [20251027] 2025.10.27
 * ・外部からの出力抑制の情報を取得できるように修正
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 *   レポートデータ作成用スレッド
 * [20250106] 2025.01.06 機能追加
 * ・計測蓄積、状態蓄積ファイルの作成ディレクトリを変更
 * [20230224] 2023.02.24 機能追加
 * ・計測蓄積、状態蓄積ファイルの作成有無を追加
 * [20221011] 2022.10.11 仕様変更
 * ・遠隔制御処理時、画面表示中対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * モニタリングサーバ向け送信編集プロセス
 *
 * 機能：
 * 		以下の機能を提供する
 * 		・１分毎に１分移動平均値を取得し、過去５分間の単純移動平均を演算する。
 * 		・５分毎にモニタリングサーバ向けのデータを編集する。
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <pthread.h>	/* [20250807] Add */
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>	/* [20230529] Add */
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvcomm.h"
#include "repoedit.h"	/* [20230529] Add */

#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DB_DEBUG									/* デバッグフラグ：DB処理確認 */
#define PV_DEBUG_TM									/* デバッグフラグ：TM履歴確認 */
#define PV_DEBUG_SV									/* デバッグフラグ：SV履歴確認 */
#define PV_DEBUG_REPORT								/* デバッグフラグ：レポートデータ確認 */
#define PV_DEBUG_REPORT_WAIT						/* デバッグフラグ：レポートデータ動作WAIT */
#define PV_DEBUG_REPORT_ACUM						/* デバッグフラグ：レポートデータ蓄積 */
#define PV_DEBUG_REPORT_CREATE						/* デバッグフラグ：レポートデータ作成 */
#define PV_DEBUG_REPORT_LNKLIST						/* デバッグフラグ：レポートデータリンクリスト *//* [20250807] Add */
#define PV_DEBUG_REPORT_ACUM_DTL					/* デバッグフラグ：レポートデータ蓄積詳細 *//* [20250807] Add */
#endif

#define	PV_OWN_NAME		"REPOEDIT"					/* 自プログラム名 */
#define	TMSTOMSG_MAX	200
#define	SCHMSG_MAX		14							/* 出力抑制情報 */
#define	FCR_SNDCYC_REPO	60							/* レポートデータ生成周期 *//* [20250807] Add */

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVDATINF		*ps_g_SVDATINF;				/* 機器状態情報ファイルポインタ [20230529] Add */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_IOTCNINF		*ps_g_IOTCNINF;				/* IoT設定情報ファイルポインタ [20230529] Add */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_TMRECFIL		*ps_g_TMRECFIL;				/* 計測情報蓄積ファイルポインタ */
struct st_SVRECFIL		*ps_g_SVRECFIL;				/* 状態情報蓄積ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_CTLJHFIL		*ps_g_CTLJHFIL;				/* 制御状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */
struct st_TMSTOFIL		*ps_g_TMSTOFIL;				/* 蓄積ファイルポインタ */
struct st_SVRECINF		*ps_g_SVRECINF;				/* 状態履歴情報ファイル */
struct st_TMRECINF		*ps_g_TMRECINF;				/* 計測履歴情報ファイル */
struct st_PGCONINF		*ps_g_PGCONINF;				/* 電力会社向け情報ファイルポインタ *//* [20230529] Add */
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL;			/* スケジュール管理情報ファイルポインタ *//* [20230529] Add */
/* [20250807] Add St */
struct st_FCRRPINF		*ps_g_FCRRPINF;				/* 一次調整力レポート蓄積項目情報ファイルポインタ *//* [20250807] Add */
/* [20250807] Add Ed */
struct st_RPRECFIL		*ps_g_RPRECFIL;				/* 一次調整力レポート蓄積ファイル *//* [20260515] */

													/* 使用ファイルインデックス */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
    {   PV_FILENO_SVDATINF, (void **)&ps_g_SVDATINF, 0, 0, 0 },	/*  [20230529] Add */
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_IOTCNINF, (void **)&ps_g_IOTCNINF, 0, 0, 0 },	/*  [20230529] Add */
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_TMRECFIL, (void **)&ps_g_TMRECFIL, 0, 0, 0 },
	{	PV_FILENO_SVRECFIL, (void **)&ps_g_SVRECFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
    {   PV_FILENO_CTLJHFIL, (void **)&ps_g_CTLJHFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	PV_FILENO_TMSTOFIL, (void **)&ps_g_TMSTOFIL, 0, 0, 0 },
    {   PV_FILENO_SVRECINF, (void **)&ps_g_SVRECINF, 0, 0, 0 },
    {   PV_FILENO_TMRECINF, (void **)&ps_g_TMRECINF, 0, 0, 0 },
	{	PV_FILENO_PGCONINF, (void **)&ps_g_PGCONINF, 0, 0, 0 },	/*  [20230529] Add */
	{	PV_FILENO_SCHCTLFIL, (void **)&ps_g_SCHCTLFIL, 0, 0, 0 },	/*  [20230529] Add */
/* [20250807] Add St */
	{	PV_FILENO_FCRRPINF, (void **)&ps_g_FCRRPINF, 0, 0, 0 },	/* [20250807] Add */
/* [20250807] Add Ed */
	{	PV_FILENO_RPRECFIL, (void **)&ps_g_RPRECFIL, 0, 0, 0 },	/* [20260515] Add */
	{	0,	                NULL,                    0, 0, 0 }
};

static char						pv_own_name[128];			/* 自プロセス情報 */
static char						errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static char						oddFilePath[256];			/* 奇数分ファイルパス */
static struct st_iotDataInfo	s_iotDataInfo;				/* IoTデータ情報 *//* [20230529] Add */

/* 内部関数定義 */
int	func_report_edit( time_t, int );
int	func_report_TM_record( time_t );
int	func_report_SV_record( time_t );
int	func_abnormal_exit( int );
int	func_tm_file( time_t ); /* */
int	func_bttctldata_write( void );
int	func_iot_data_edit( time_t, int );
int	func_iot_data_init( void );
int	func_iot_rec_data_edit( void );
int	func_fcr_repodata_edit( time_t );/* [20250807] Add *//* [20260515] Chg */

/* 外部関数定義 */
extern	long long commfunc_pow10abs( short );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステムサーバ向け送信情報編集
 *
 * 機能：
 * 		１分周期に下記処理を行う
 * 		・計測情報保存ファイルから計測情報を取得し保管する。
 * 		２分周期にサーバ向け送信用のファイルを生成する。
 * 		・状変蓄積ファイルから未編集の状変情報を取得し、送信用ファイルに記録する。
 * 		・最新の計測情報を送信用ファイルに記録する。
 * 		・各装置の通信ステータスを取得し、送信用ファイルに記録する。
 * 		・各装置の装置状態を取得し、送信用ファイルに記録する。
 *		※データ送信ロックが設定されている場合は、状変、状態、計測情報は編集しない。
 *		（データ送信ロック時は基本情報と終端情報のみ）
 */
int main(int argc, char *argv[])
{
	int						procNo;						/* プログラム番号 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */
	time_t					secData = 0;				/* 積算秒 */
	time_t					chkData = 0;				/* 積算秒（最後にファイルを生成した時刻） */
	time_t					tmDataSec = 0;				/* 積算秒（最後に計測ファイルを生成した時刻） */
	struct tm				timeData;					/* 時刻情報 */
	int						stopFlag = 0;				/* 停止通知受信済みフラグ */
	int						nonTmFlag = 0;				/* 計測情報未収録フラグ */
	int						initFlag = 1;				/* 起動フラグ */
	struct timespec			sleepTm;					/* 送信待ちタイマ */


#ifdef PV_DEBUG
	struct timeval			time1;
	struct timeval			time2;
	struct timeval			time3;
#endif

	/* プロセス起動時の引数からプログラム番号を取得 */
	if( argc != 2 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("Usage : %s procNo\n", argv[0]);
		return( -1 );
	}
	else
	{
		/* プログラム番号を取得 */
		procNo = (char)atoi(argv[1]);
		/* プログラム番号が既定値範囲外の場合 */
		if( (procNo < 11) || (procNo > PV_MAX_PROC) )
		{
			printf("Usage : %s procNo\n", argv[0]);
			return( -1 );
		}
	}

	/* 自プロセス情報を設定 */
	sprintf( pv_own_name, "%s(%02d)", PV_OWN_NAME, procNo );

	/* ファイルをオープンしてＭＭＡＰ化する */
	if( (retCode = commfunc_file_open( s_fileIndex )) < 0 )
	{
		printf("REPOEDIT : File Open Error\n");
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_SET );

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_RESET );

/* [20230529] Add St */
	/* IoTデータ情報初期化 */
	func_iot_data_init();
/* [20230529] Add Ed */

	memset(oddFilePath, 0, sizeof(oddFilePath));
	while( 1 )
	{
		/* 停止通知受信済みフラグリセット */
		stopFlag = 0;
		nonTmFlag = 0;
		/* 15秒周期になるまで待機する */
		if( commfunc_wait( 15, procNo ) < 0 )
		{
#ifdef PV_DEBUG
			printf("REPOEDIT : PROCESS STOP COMMAND RECV (PROC:%d)\n", procNo );
#endif
			/* 停止通知受信済みフラグ設定 */
			stopFlag = 1;
		}
#ifdef PV_DEBUG
		gettimeofday( &time1, NULL );
		printf("REPOEDIT : ST:%d.%06d\n",time1.tv_sec, time1.tv_usec );
#endif

		/* emctlmng処理待ち */
		sleepTm.tv_sec  = 0;
		sleepTm.tv_nsec = 20000000;
		/* 処理待ち */
		nanosleep( &sleepTm, NULL);

		/* 積算秒を取得 */
		secData = time( NULL );

		/* 毎正分以外は、計測情報は収録しない */
		if( ( secData % 60 ) != 0 )
		{
			nonTmFlag = 1;
		}

		/* 停止が前回のファイル生成秒と同じ場合は、秒を1進める */
		if( ( stopFlag == 1 ) && ( chkData == secData ) )
		{
			secData++;
		}

		/* 計測情報保存ファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_TMSTSFIL );
		/* 計測情報保存ファイルの現在状態を全てコピー */
		memcpy( ps_g_TMSTOFIL->s_tmstsfil, ps_g_TMSTSFIL, sizeof( struct st_TMSTSFIL ) * PV_MAX_TMDATA );
		/* 計測情報保存ファイルのミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_TMSTSFIL );

		/* 通信ステータス状態保存ファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_STSTSFIL );
		/* 通信ステータス状態保存ファイルの現在状態を全てコピー */
		memcpy( ps_g_TMSTOFIL->s_ststsfil, ps_g_STSTSFIL, sizeof( struct st_STSTSFIL ) * ((PV_MAX_STDATA-1)/8+1));
		/* 通信ステータス状態保存ファイルのミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_STSTSFIL );

		/* 装置状態保存ファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_SVSTSFIL );
		/* 装置状態保存ファイルの現在状態を全てコピー */
		memcpy( ps_g_TMSTOFIL->s_svstsfil, ps_g_SVSTSFIL, sizeof( struct st_SVSTSFIL ) * ((PV_MAX_SVDATA-1)/8+1));
		/* 装置状態保存ファイルのミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_SVSTSFIL );


#ifdef PV_DEBUG
		gettimeofday( &time2, NULL );
		printf("REPOEDIT : ED:%d.%06d\n",time2.tv_sec, time2.tv_usec );
#endif

		/* サーバ通知メッセージの作成 */
		func_report_edit( secData, (stopFlag|initFlag|nonTmFlag) );

		/* 蓄積ファイルの作成 */
		if( ( stopFlag == 1 ) ||
			( nonTmFlag ==  0 ) )
		{
			func_report_TM_record( secData );
			func_report_SV_record( secData );
			/* レポートデータファイル生成 */
			func_fcr_repodata_edit( secData );		/* [20260515] Add */
		}

/* [20230529] Add St */
		/* 生成周期チェック */
		if( ( ps_g_IOTCNINF->sndTopicName[0] != 0 ) &&
		    (( stopFlag == 1 ) ||
			 (( secData % ps_g_IOTCNINF->sndCycle ) == 0) ) )
		{
			/* publishデータの作成 */
			func_iot_data_edit( secData, (stopFlag|initFlag) );
		}
/* [20230529] Add Ed */

		/* ファイル生成時刻の保存 */
		chkData = secData;


		/* 起動フラグを初期化 */
		initFlag = 0;

		/* 停止通知受信済フラグが設定されている場合 */
		if( stopFlag )
		{
			/* 処理を抜ける */
			break;
		}

	}

	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );

	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * サーバ向け送信情報編集処理
 *
 * 機能：
 * 		以下に示す情報をサーバ向け送信情報として編集する。
 * 		・基本情報：収集装置ID番号と情報の時刻
 * 		・状変情報：状変蓄積ファイルに蓄積されていた状変情報
 * 		・計測情報：過去５分間の移動平均情報（積算値は移動平均対象外）
 * 		・通信ステータス情報：各装置の通信ステータス状態
 * 		・装置状態情報：各装置の装置状態
 *
 * 		基本情報は、管理ファイル（PVSYSINF.dat）から取得する。
 * 		状変情報は、本関数の引数として受け取る。
 * 		計測情報は、計測値蓄積ファイル（TMSTOFIL.dat）に格納されている５分間の計
 * 		測値データから５分間の単純移動平均値を算出し、サーバ向けの計測情報として
 * 		編集する。
 * 		通信ステータス情報は、計測値蓄積ファイル（TMSTOFIL.dat）から取得する。
 * 		装置状態情報は、計測値蓄積ファイル（TMSTOFIL.dat）から取得する。
 *
 * 		移動平均は計測種別が瞬時値のみ対象とし、積算値は除外する。
 *
 * 		ファイルの生成は一時ファイル名を使用して作成し、作成後は正規の名称に変更
 * 		する。（nkftpsndプロセスの誤認識を防ぐため）
 *
 *		※装置停止指示が出ている場合は計測値の編集は行わない。
 *
 *		※データ送信ロックが設定されている場合は、状変、状態、計測情報は編集しない。
 *		（データ送信ロック時は基本情報と終端情報のみ）
 *
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_report_edit( time_t timeSec, int nonTmFlag )
{
	FILE			*fp;
	struct tm		timeData;
	int				maxNumb;						/* 処理最大数 */
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	int				loopCnt3;						/* ループカウンタ */
	int				loopCnt4;						/* ループカウンタ */
	int				linkPoint;						/* */
	int				workFlg;
	int 			workVal;						/* */
	int				dtsdLockFlag;					/* データ送信ロックフラグ */
	char			outFileName[256];				/* */
	char			tmpFileName[256];				/* */
	char			strTime[16];
	int				tmstoCnt;					/* 状変機器数 */
	char			tmstoMsg[TMSTOMSG_MAX][PV_MSG_SIZE_TMH];
    int             johenCnt;					/* 状変機器数 */
    char            johenMsg[PV_MAX_JOHEN][PV_MSG_SIZE_JHD];
	int             ctljohenCnt;				/* 状変機器数 [20160329] Add */
	char            ctljohenMsg[PV_MAX_CTLINF][PV_MSG_SIZE_CTL];
	char			schMsg[SCHMSG_MAX][PV_MSG_SIZE_SCH];	/* [20230529] Add */
	int             schCnt;									/* スケジュール数*//* [20230529] Add */

	localtime_r( &timeSec, &timeData );

	sprintf( strTime, "%04d%02d%02d%02d%02d%02d",
		timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );

	strcpy( outFileName, PV_PATH_FTP );
	strncat( outFileName, strTime, 8 );
	strcat(  outFileName, "/");
	mkdir(outFileName, ( S_IRWXG | S_IRWXU ));	/* 所有者、グループの権限付与 */
	strncat( outFileName, ps_g_PVSYSINF->siteIdName, 8 );
	strncat( outFileName, strTime, 14 );
	strcpy( tmpFileName, outFileName );

	strcat( outFileName, ".DAT" );
	strcat( tmpFileName, ".TMP" );


#ifdef PV_DEBUG
	printf("REPOEDIT : CREATE FILE [ %s ]\n", outFileName );
#endif

	/* データ送信ロックフラグを取得 */
	dtsdLockFlag = commfunc_manage_dtsdLockFlagGet( );

	/* ファイルのオープン */
	if( (fp = fopen( tmpFileName, "w" )) == NULL )
	{
		sprintf( errMsg, "Create File Error [ %s ]", tmpFileName );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return( -1 );
	}

	/* 収集装置情報を出力 */
	fprintf( fp, "BSD,%s,%s\n", ps_g_PVSYSINF->siteIdName, strTime );

	/* データ送信ロック中の場合 */
	if( dtsdLockFlag != 0 )
	{
		/* 終端情報を出力 */
		fprintf( fp, "END\n" );
		/* ファイルをクローズする */
		fclose( fp );
		/* ファイルをテンポラリ名から実際のファイル名に変更 */
		rename( tmpFileName, outFileName );

		return( 0 );
	}

/* [20221011] Add St */
	/* 遠隔制御通し番号が設定されている場合 */
	if( ps_g_CTLJHFIL->rmtCtlNumb[0] != 0 )
	{
		/* 遠隔制御通し番号を出力 */
		fprintf( fp, "RMTEND,%s\n", ps_g_CTLJHFIL->rmtCtlNumb );
		memset(ps_g_CTLJHFIL->rmtCtlNumb, 0, sizeof(ps_g_CTLJHFIL->rmtCtlNumb));
	}
/* [20221011] Add Ed */


	/* 状変メッセージ取得 */
	ctljohenCnt = commfunc_ctl_johen_msggettcp( ctljohenMsg );	/* [20160329] Add */

	/* 状変メッセージの取得 */
	for(loopCnt1 = 0 ; loopCnt1 < ctljohenCnt ; loopCnt1++ )
	{
		/* 状変メッセージの出力 */
		fprintf( fp, "%s\n", ctljohenMsg[loopCnt1] );
	}

	/* 状変メッセージ取得 */
	johenCnt = commfunc_johen_msggettcp( johenMsg );

	/* 状変メッセージの取得 */
	for(loopCnt1 = 0 ; loopCnt1 < johenCnt ; loopCnt1++ )
	{
		/* 状変メッセージの出力 */
		fprintf( fp, "%s\n", johenMsg[loopCnt1] );
	}

	/* 計測情報の出力 */

	/* 計測情報の未出力がOFFの場合（収録する場合） */
	if( nonTmFlag != 1 )
	{

		/* 全装置種別分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
		{
			/* 装置種別が実装の場合 */
			if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt == 0 )
				continue;

			/* 装置数分繰り返す */
			for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxMachine ; loopCnt2++ )
			{
				/* 装置未実装の場合は次の装置の処理を行う */
				if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[loopCnt1].statFlag, loopCnt2 ) == 0 )
					continue;

				/* 計測情報ポインタの取得 */
				linkPoint = ps_g_PVSYSINF->s_machineInfo[loopCnt1].tmIfLink + loopCnt2 * ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxTm;
				/* 計測情報数分繰り返す */
				for( loopCnt3 = 0 ; loopCnt3 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxTm ; loopCnt3++ )
				{
					/* 計測情報が実装の場合 */
					if( ps_g_TMDATINF[linkPoint + loopCnt3].statFlag )
					{
						/* 計測フラグを取得 */
						workFlg = ps_g_TMSTOFIL->s_tmstsfil[linkPoint + loopCnt3].avltmFlag;
						/* 計測値を取得 */
						workVal = ps_g_TMSTOFIL->s_tmstsfil[linkPoint + loopCnt3].avltmVal;

						/* 計測情報の出力 */
						fprintf( fp, "TMD,%d,%d,%d,%d,%d\n", loopCnt1, loopCnt2 + 1, loopCnt3 + 1, workFlg, workVal ) ;
					}
				}
			}
		}
	}	

	/* 通信ステータス情報の出力 */
	/* 収集装置は通信ステータスがないためループは１から開始 */
	for( loopCnt1 = 0 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		/* 装置種別が収集装置の場合は対象外 */
		if( loopCnt1 == PV_MKIND_PVSYS )
			continue;

		/* 装置種別が未実装の場合 */
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt == 0 )
			continue;

		/* 装置数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxMachine ; loopCnt2++ )
		{
			/* 装置が未実装の場合 */
			if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[loopCnt1].statFlag, loopCnt2 ) == 0 )
				continue;

			/* ポインタの取得 */
			linkPoint = ps_g_PVSYSINF->s_machineInfo[loopCnt1].stStLink + ((PV_MAX_STPOS-1) / 8 + 1) * loopCnt2;

			/* 通信ステータス状態を出力する */
			fprintf( fp, "STD,%d,%d,%d", loopCnt1, loopCnt2 + 1, (PV_MAX_STPOS-1) / 8 + 1 );
			for( loopCnt3 = 0 ; loopCnt3 < ((PV_MAX_STPOS-1) / 8 + 1) ; loopCnt3++ )
			{
				fprintf( fp, ",%02X", (unsigned char)ps_g_STSTSFIL[ linkPoint + loopCnt3].stStat );
			}
			fprintf( fp, "\n");
		}
	}

	/* 装置状態情報の出力 */
	for( loopCnt1 = 0 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		/* 装置種別が未実装の場合 */
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt == 0 )
			continue;

		/* 装置数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxMachine ; loopCnt2++ )
		{
			/* 装置が未実装の場合 */
			if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[loopCnt1].statFlag, loopCnt2 ) == 0 )
				continue;

			/* ポインタの取得 */
			linkPoint = ps_g_PVSYSINF->s_machineInfo[loopCnt1].svStLink + 
				((ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxSv - 1) / 8 + 1) * loopCnt2;

			/* 装置状態の出力 */
			fprintf( fp, "SVD,%d,%d,%d", loopCnt1, loopCnt2 + 1, (ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxSv - 1) / 8 + 1 );
			for( loopCnt3 = 0 ; loopCnt3 < ((ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxSv - 1) / 8 + 1) ; loopCnt3++ )
			{
				fprintf( fp, ",%02X", (unsigned char)ps_g_SVSTSFIL[linkPoint + loopCnt3].svStat );
			}
			fprintf( fp, "\n");
		}
	}

/* [20230529] Add St */
	/* 出力抑制情報の出力 */
	/* スケジュール種別分ループ */
	for( loopCnt1 = 0; loopCnt1 < PV_SCH_TYPE_MAX; loopCnt1++ )
	{

/* [20251027] Del */
		schCnt = commfunc_pvsch_msggettcp( schMsg, loopCnt1 );

		/* スケジュール情報の取得 */
		for(loopCnt2 = 0 ; loopCnt2 < schCnt ; loopCnt2++ )
		{
			/* スケジュール情報の出力 */
			fprintf( fp, "%s\n", schMsg[loopCnt2] );
		}
/* [20251027] Del */
	}
/* [20230529] Add Ed */


	/* 終端情報を出力 */
	fprintf( fp, "END\n" );
	/* ファイルをクローズする */
	fclose( fp );

	/* ファイルをテンポラリ名から実際のファイル名に変更 */
	rename( tmpFileName, outFileName );


	return( 0 );
}

/* [20230529] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IoT向け送信情報初期化処理
 *
 * 機能：
 *
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_iot_data_init( void )
{
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	int				loopCnt3;						/* ループカウンタ */
	int				machineCnt=0;
	int				tmMaxNumb;
	int				svMaxNumb;
	int				stMaxNumb;
	int				linkPoint;						/* */

	/* IoTデータ情報を初期化 */
	memset(&s_iotDataInfo, 0, sizeof(s_iotDataInfo));

	/* 全装置種別分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		/* 装置種別の台数が0の場合 */
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt == 0 )
			continue;

		/* 装置種別の最大台数分ループ */
		for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxMachine ; loopCnt2++ )
		{
			/* 装置未実装の場合は次の装置の処理を行う */
			if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[loopCnt1].statFlag, loopCnt2 ) == 0 )
				continue;

			tmMaxNumb = 0;
			svMaxNumb = 0;
			stMaxNumb = 0;

			/* 計測情報ポインタの取得 */
			linkPoint = ps_g_PVSYSINF->s_machineInfo[loopCnt1].tmIfLink + loopCnt2 * ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxTm;

			/* 計測情報数分繰り返す */
			for( loopCnt3 = 0 ; loopCnt3 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxTm ; loopCnt3++ )
			{
				/* 計測情報が実装、且つ、IOT送信有無が有の場合 */
				if( ( ps_g_TMDATINF[linkPoint + loopCnt3].statFlag == 1 ) &&
					( ps_g_TMDATINF[linkPoint + loopCnt3].iotSndFlag == 1 ) )
				{
					tmMaxNumb = loopCnt3+1;
				}
			}

			/* 装置種別に合わせて情報のリンクポインタ値を取得する */
			linkPoint = ps_g_PVSYSINF->s_machineInfo[loopCnt1].svIfLink + loopCnt2 * ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxSv;

			/* 装置状態情報数分繰り返す */
			for( loopCnt3 = 0 ; loopCnt3 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxSv ; loopCnt3++ )
			{
				/* 装置状態情報が実装、且つ、IOT送信有無が有の場合 */
				if( ( ps_g_SVDATINF[linkPoint + loopCnt3].statFlag == 1 ) &&
					( ps_g_SVDATINF[linkPoint + loopCnt3].iotSndFlag == 1 ) )
				{
					svMaxNumb = loopCnt3+1;
				}
			}

			/* 通信状態情報 */
			if( loopCnt1 == PV_MKIND_PVSYS )
			{
				stMaxNumb = 0;
			}
			else
			{
				stMaxNumb = PV_MAX_STPOS;
			}

			/* 計測情報、装置状態情報、装置通信情報のいずれかが定義されている場合、データ情報として登録する */
			if( ( tmMaxNumb > 0 ) ||
				( svMaxNumb > 0 ) ||
				( stMaxNumb > 0 ) )
			{
				s_iotDataInfo.s_machineDataInfo[machineCnt].mkind = loopCnt1;
				s_iotDataInfo.s_machineDataInfo[machineCnt].mnumb = loopCnt2+1;
				s_iotDataInfo.s_machineDataInfo[machineCnt].tmMaxNumb = tmMaxNumb;
				s_iotDataInfo.s_machineDataInfo[machineCnt].svMaxNumb = svMaxNumb;
				s_iotDataInfo.s_machineDataInfo[machineCnt].stMaxNumb = stMaxNumb;
				machineCnt++;
			}
		}
	}
	/* 装置台数を設定 */
	s_iotDataInfo.machineCnt = machineCnt;
	
	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IoT向け送信情報編集処理
 *
 * 機能：
 *
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_iot_data_edit( time_t timeSec, int initFlag )
{
	FILE			*fp;
	struct tm		timeData;
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	char			outFileName[256];				/* */
	char			tmpFileName[256];				/* */
	char			strTime[13];					/* */
	long long		calcData;						/* 演算データ */
	long long		pow10Data;						/* べき乗データ */
	long long		spow10Data;						/* べき乗データ */
	struct tm		localTime;						/* 現在時刻 */
	time_t			nowTime;						/* 現在時刻 */
	int				tmifLink;						/* 計測情報リンクポインタ */
	int				svifLink;						/* 装置状態情報リンク情報 */
	int				svstBase;						/* 装置状態リンク情報（ベース値） */
	int				ststBase;						/* 装置通信状態リンク情報（ベース値） */
	unsigned char	svstMask;						/* 機器状態取得用マスク */
	int				stifLink;						/* 通信ステータスリンク */
	int 			mKind;							/* 装置種別 */
	int 			mNumb;							/* 装置番号 */
	char			tmStr[32];						/* 計測値文字列 */
	char			svStr[32];						/* 装置状態情報文字列 */
	char			stStr[32];						/* 装置通信情報文字列 */
	int 			tmStatus;						/* 計測ステータス */
    int             johenCnt;					/* 状変機器数 */
    char            johenMsg[PV_MAX_JOHEN][PV_MSG_SIZE_JHD];
	int             ctljohenCnt;				/* 状変機器数 [20160329] Add */
	char            ctljohenMsg[PV_MAX_CTLINF][PV_MSG_SIZE_CTL];
	int				dtsdLockFlag;					/* データ送信ロックフラグ */

	timeSec = time( NULL );
	localtime_r( &timeSec, &timeData );

	sprintf( strTime, "%04d%02d%02d%02d%02d%02d",
		timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );

	strcpy( outFileName, PV_PATH_IOT );
	strncat( outFileName, strTime, 8 );
	strcat( outFileName, "/");
	mkdir(outFileName, ( S_IRWXG | S_IRWXU ));	/* ディレクトリ生成＆所有者、グループの権限付与 */
	strcat( outFileName, ps_g_IOTCNINF->deviceId );
	strcat( outFileName, "_" );
	strncat( outFileName, strTime, 14 );
	strcat( outFileName, "_" );
	strcat( outFileName, PV_AWS_IOT_FILE_PREFIX );
	strcpy( tmpFileName, outFileName );

	strcat( outFileName, ".DAT" );
	strcat( tmpFileName, ".TMP" );


#ifdef PV_DEBUG_IOT
	printf("REPOEDIT : IOT FILE [ %s ]\n", outFileName );
#endif

	/* ファイルのオープン */
	if( (fp = fopen( tmpFileName, "w" )) == NULL )
	{
		sprintf( errMsg, "Create File Error [ %s ]", tmpFileName );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return( -1 );
	}

	/* 制御メッセージ取得 */
	ctljohenCnt = commfunc_ctl_johen_msggetiot( ctljohenMsg );

	/* 状変メッセージ取得 */
	johenCnt = commfunc_johen_msggetiot( johenMsg );

	/* 収集装置情報を出力 */
	fprintf( fp, "{\n" );
	fprintf( fp, "\"device_id\":\"%s\",\n", ps_g_IOTCNINF->deviceId );		/* [20201225] Chg */
	fprintf( fp, "\"message_type\":\"measure\",\n");
	fprintf( fp, "\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d\",\n", timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );
	fprintf( fp, "\"date\":\"%04d-%02d-%02d %02d:%02d:%02d\",\n", timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );

	/* データ送信ロックフラグを取得 */
	dtsdLockFlag = commfunc_manage_dtsdLockFlagGet( );
	/* データ送信ロック中の場合 */
	if( dtsdLockFlag != 0 )
	{
		/* 計測情報、状態情報は収録しない。 */
		fprintf( fp, "\"machineinfo\":{}\n");
	}
	else
	{
		/* データが定義されている装置がある場合 */
		if( s_iotDataInfo.machineCnt > 0 )
		{
			fprintf( fp, "\"machineinfo\":{\n");

			/* 全装置台数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < s_iotDataInfo.machineCnt ; loopCnt1++ )
			{
				mKind = s_iotDataInfo.s_machineDataInfo[loopCnt1].mkind;
				mNumb = s_iotDataInfo.s_machineDataInfo[loopCnt1].mnumb;

				/* 装置台数を出力 */
				fprintf( fp, "\"mcount%d\":{\n", loopCnt1+1);

				/* 装置種別を出力 */
				fprintf( fp, "\"mkind\":%d,\n", mKind);
				/* 装置番号を出力 */
				fprintf( fp, "\"mnumb\":%d,\n", mNumb);

				/* 実装済みの計測情報がある場合 */
				if( s_iotDataInfo.s_machineDataInfo[loopCnt1].tmMaxNumb > 0 )
				{
					if( initFlag == 0 )
					{
						fprintf( fp, "\"tminfo\":{\n");

						for( loopCnt2 = 0 ; loopCnt2 < s_iotDataInfo.s_machineDataInfo[loopCnt1].tmMaxNumb ; loopCnt2++ )
						{
							/* 装置種別に合わせて計測情報のリンクポインタ値を取得する */
							tmifLink = ps_g_PVSYSINF->s_machineInfo[mKind].tmIfLink + ps_g_PVSYSINF->s_machineInfo[mKind].maxTm * (mNumb-1) + loopCnt2;

							/* 計測情報が実装、且つ、IOT送信有無が有の場合 */
							if( ( ps_g_TMDATINF[tmifLink].statFlag == 1 ) &&
								( ps_g_TMDATINF[tmifLink].iotSndFlag == 1 ) )
							{
								/* 計測番号を設定 */
								sprintf( tmStr, "\"no%d\":{", loopCnt2+1 );

								/* 積算型の場合 */
								if( PV_TM_KIND_COUNT == (ps_g_TMDATINF + tmifLink)->tmKind )
								{
									/* 最新の正常値しかないので、正常 */
									/* 初期起動時は「0」だが、積算は0からカウントアップする仕様の為、問題ない */
									tmStatus = true;
								}
								/* 瞬時型の場合 */
								else //if( PV_TM_KIND_VALUE == (ps_g_TMDATINF + tmifLink)->tmKind )
								{
									/* 正常だけ */
									if( PV_TMFLG_NORMAL == (ps_g_TMSTSFIL + tmifLink)->avltmFlag )
									{
										tmStatus = true;
									}
									else
									{
										tmStatus = false;
									}
								}
								/* 計測ステータスを出力 */
								sprintf(&tmStr[strlen(tmStr)], "\"status\":%s,\"value\":", ( true == tmStatus) ? "true" : "false");


								/* 演算結果を出力する */
								pow10Data = (long long)commfunc_pow10abs( (ps_g_TMDATINF + tmifLink)->expData );
								spow10Data = (long long)commfunc_pow10abs( (ps_g_TMDATINF + tmifLink)->expData + 3);
								calcData = (long long)((ps_g_TMSTSFIL + tmifLink)->avltmVal);
								calcData *= (long long)((ps_g_TMDATINF + tmifLink)->convData);
								calcData += (long long)((ps_g_TMDATINF + tmifLink)->baseData);

								if( (ps_g_TMDATINF + tmifLink)->expData >= 0 )
								{
									sprintf(&tmStr[strlen(tmStr)], "%lld.000", calcData * pow10Data );
								}
								else
								{
									if( ((calcData / pow10Data) == 0) && ( calcData < 0 ) )
										sprintf(&tmStr[strlen(tmStr)], "-0.");
									else
										sprintf(&tmStr[strlen(tmStr)], "%lld.", calcData / pow10Data );

									if( calcData < 0 ) calcData *= -1;
									if( (ps_g_TMDATINF[tmifLink].expData + 3) >= 0 )
									{
										sprintf(&tmStr[strlen(tmStr)], "%03lld", ( calcData * spow10Data ) % 1000 );
									}
									else
									{
										sprintf(&tmStr[strlen(tmStr)], "%03lld", ( calcData / spow10Data ) % 1000 );
									}
								}

								sprintf(&tmStr[strlen(tmStr)], "}");
								if( ( loopCnt2 + 1) != s_iotDataInfo.s_machineDataInfo[loopCnt1].tmMaxNumb )
								{
									sprintf(&tmStr[strlen(tmStr)], ",");
								}
								fprintf( fp, "%s\n", tmStr ); 
							}
						}

						if( ( s_iotDataInfo.s_machineDataInfo[loopCnt1].svMaxNumb > 0 ) ||
							( s_iotDataInfo.s_machineDataInfo[loopCnt1].stMaxNumb > 0 ) )
						{
							fprintf( fp, "},\n");
						}
						else
						{
							fprintf( fp, "}\n");
						}
					}
				}
				

				/* 実装済みの装置状態情報がある場合 */
				if( s_iotDataInfo.s_machineDataInfo[loopCnt1].svMaxNumb > 0 )
				{
					fprintf( fp, "\"svinfo\":{\n");

					for( loopCnt2 = 0 ; loopCnt2 < s_iotDataInfo.s_machineDataInfo[loopCnt1].svMaxNumb ; loopCnt2++ )
					{
						/* 装置種別に合わせて装置状態情報のリンクポインタ値を取得する */
						svifLink = ps_g_PVSYSINF->s_machineInfo[mKind].svIfLink + ps_g_PVSYSINF->s_machineInfo[mKind].maxSv  * (mNumb-1) + loopCnt2;
						/* 装置種別に合わせて状態のリンクポインタ値を取得する */
						svstBase = ps_g_PVSYSINF->s_machineInfo[mKind].svStLink +
									((ps_g_PVSYSINF->s_machineInfo[mKind].maxSv - 1) / 8 + 1) * (mNumb-1);

						/* 装置状態情報が実装、且つ、IOT送信有無が有の場合 */
						if( ( ps_g_SVDATINF[svifLink].statFlag == 1) &&
							( ps_g_SVDATINF[svifLink].iotSndFlag == 1 ) )
						{
							/* 装置状態番号を設定 */
							sprintf( svStr, "\"no%d\":", loopCnt2+1 );

							/* 装置状態取得のためのマスク値を設定 */
							svstMask = (unsigned char)(0x80 >> (((ps_g_SVDATINF + svifLink)->svNumb - 1) % 8));
							/* 装置状態の取得と表示 */
							if( (ps_g_SVSTSFIL + svstBase + ((ps_g_SVDATINF + svifLink)->svNumb - 1) / 8 )->svStat & svstMask )
								sprintf( &svStr[strlen(svStr)], "true");
							else
								sprintf( &svStr[strlen(svStr)], "false");

							if( ( loopCnt2 + 1) != s_iotDataInfo.s_machineDataInfo[loopCnt1].svMaxNumb )
							{
								sprintf(&svStr[strlen(svStr)], ",");
							}
							fprintf( fp, "%s\n", svStr ); 
						}
					}

					if( s_iotDataInfo.s_machineDataInfo[loopCnt1].stMaxNumb > 0 )
					{
						fprintf( fp, "},\n");
					}
					else
					{
						fprintf( fp, "}\n");
					}
				}

				/* 実装済みの装置通信情報がある場合 */
				if( s_iotDataInfo.s_machineDataInfo[loopCnt1].stMaxNumb > 0 )
				{
					fprintf( fp, "\"stinfo\":{\n");

					for( loopCnt2 = 0 ; loopCnt2 < s_iotDataInfo.s_machineDataInfo[loopCnt1].stMaxNumb ; loopCnt2++ )
					{
						/* 装置種別に合わせて通信状態のリンクポインタ値を取得する */
						ststBase = ps_g_PVSYSINF->s_machineInfo[mKind].stStLink + (mNumb-1) * ((PV_MAX_STPOS - 1) / 8 + 1);

						/* 装置通信番号を設定 */
						sprintf( stStr, "\"no%d\":", loopCnt2+1 );

						/* 通信ステータス状態取得用マスク値の設定 */
						svstMask = (unsigned char)(0x80 >> (loopCnt2 % 8));

						/* 通信ステータス状態の取得と表示 */
						if( (ps_g_STSTSFIL + ststBase + loopCnt2 / 8 )->stStat & svstMask )
							sprintf( &stStr[strlen(stStr)], "true");
						else
							sprintf( &stStr[strlen(stStr)], "false");

						if( ( loopCnt2 + 1) != s_iotDataInfo.s_machineDataInfo[loopCnt1].stMaxNumb )
						{
							sprintf(&stStr[strlen(stStr)], ",");
						}
						fprintf( fp, "%s\n", stStr ); 
					}
					fprintf( fp, "}\n");
				}


				if( ( loopCnt1 + 1 ) == s_iotDataInfo.machineCnt )
					fprintf( fp, "}\n");
				else
					fprintf( fp, "},\n");
			}
			fprintf( fp, "}");

			/* 状変メッセージがある場合 */
			if( johenCnt > 0 )
			{
				fprintf( fp, ",\n");
				/* 状変メッセージの出力 */
				fprintf( fp, "\"jhinfo\":[\n");

				/* 状変メッセージの取得 */
				for(loopCnt1 = 0 ; loopCnt1 < johenCnt ; loopCnt1++ )
				{
					if( ( loopCnt1 + 1 ) == johenCnt )
						/* 状変メッセージの出力 */
						fprintf( fp, "%s\n", johenMsg[loopCnt1] );
					else
						/* 状変メッセージの出力 */
						fprintf( fp, "%s,\n", johenMsg[loopCnt1] );
				}
				fprintf( fp, "]");
			}


			/* 制御メッセージがある場合 */
			if( ctljohenCnt > 0 )
			{
				fprintf( fp, ",\n");
				/* 状変メッセージの出力 */
				fprintf( fp, "\"chinfo\":[\n");

				/* 状変メッセージの取得 */
				for(loopCnt1 = 0 ; loopCnt1 < ctljohenCnt ; loopCnt1++ )
				{
					if( ( loopCnt1 + 1 ) == ctljohenCnt )
						/* 状変メッセージの出力 */
						fprintf( fp, "%s\n", ctljohenMsg[loopCnt1] );
					else
						/* 状変メッセージの出力 */
						fprintf( fp, "%s,\n", ctljohenMsg[loopCnt1] );
				}
				fprintf( fp, "]");
			}

			fprintf( fp, "\n");
		}
	}

	fprintf( fp, "}\n");

	/* ファイルをクローズする */
	fclose( fp );

	/* ファイルをテンポラリ名から実際のファイル名に変更 */
	rename( tmpFileName, outFileName );

	return( 0 );
}
/* [20230529] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測向け送信情報編集処理
 *
 * 機能：
 * 		以下に示す情報を計測ファイルとして編集する。
 * 		・基本情報：収集装置ID番号と情報の時刻
 * 		・計測情報：過去１分間の移動平均情報（積算値は移動平均対象外）
 *
 * 		計測情報は、計測値蓄積ファイル（TMSTOFIL.dat）に格納されている５分間の計
 * 		測値データから最新の１分間の移動平均値を計測情報として編集する。
 * 		移動平均は計測種別が瞬時値のみ対象とし、積算値は除外する。
 *
 * 		ファイルの生成は一時ファイル名を使用して作成し、作成後は正規の名称に変更
 * 		する。（ファイル取得時の誤認識を防ぐため）
 *
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_tm_file( time_t timeSec )
{
	FILE			*fp;
	struct tm		timeData;
	int				maxNumb;						/* 処理最大数 */
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	int				loopCnt3;						/* ループカウンタ */
	int				linkPoint;						/* */
	int				workFlg;
	int 			workVal;						/* */
	int				dtsdLockFlag;					/* データ送信ロックフラグ */
	char			outFileName[256];				/* */
	char			tmpFileName[256];				/* */
	char			strTime[13];

	timeSec = time( NULL );
	localtime_r( &timeSec, &timeData );	

	sprintf( strTime, "%04d%02d%02d%02d%02d",
		timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min );

	strcpy( outFileName, PV_PATH_DWL );
	strncat( outFileName, strTime, 8 );
	strcat(  outFileName, "/");
	mkdir(outFileName, ( S_IRWXG | S_IRWXU ));	/* 所有者、グループの権限付与 */
	strncat( outFileName, ps_g_PVSYSINF->siteIdName, 8 );
	strncat( outFileName, strTime, 12 );
	strcpy( tmpFileName, outFileName );

	strcat( outFileName, ".TXT" );
	strcat( tmpFileName, ".TMP" );

#ifdef PV_DEBUG
	printf("REPOEDIT : CREATE TM FILE [ %s ]\n", outFileName );
#endif

	/* ファイルのオープン */
	if( (fp = fopen( tmpFileName, "w" )) == NULL )
	{
		sprintf( errMsg, "Create File Error [ %s ]", tmpFileName );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return( -1 );
	}

	/* 収集装置情報を出力 */
	fprintf( fp, "BSD,%s,%s00\n", ps_g_PVSYSINF->siteIdName, strTime );

	/* 全装置種別分繰り返す */
	/* 収集装置の計測情報（演算計測情報）は送信対象外とするためループは１から開始*/
	for( loopCnt1 = 1 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		/* 装置種別が実装の場合 */
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt == 0 )
			continue;

		/* 装置数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxMachine ; loopCnt2++ )
		{
			/* 装置未実装の場合は次の装置の処理を行う */
			if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[loopCnt1].statFlag, loopCnt2 ) == 0 )
				continue;

			/* 計測情報ポインタの取得 */
			linkPoint = ps_g_PVSYSINF->s_machineInfo[loopCnt1].tmIfLink + loopCnt2 * ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxTm;
			/* 計測情報数分繰り返す */
			for( loopCnt3 = 0 ; loopCnt3 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxTm ; loopCnt3++ )
			{
				/* 計測情報が実装の場合 */
				if( ps_g_TMDATINF[linkPoint + loopCnt3].statFlag )
				{
					/* 計測フラグを取得 */
					workFlg = ps_g_TMSTOFIL->s_tmstsfil[linkPoint + loopCnt3].avltmFlag;
					/* 計測値を取得 */
					workVal = ps_g_TMSTOFIL->s_tmstsfil[linkPoint + loopCnt3].avltmVal;

					/* 計測情報の出力 */
					fprintf( fp, "TMD,%d,%d,%d,%d,%d\n", loopCnt1, loopCnt2 + 1, loopCnt3 + 1, workFlg, workVal ) ;
				}
			}
		}
	}

	/* 終端情報を出力 */
	fprintf( fp, "END\n" );
	/* ファイルをクローズする */
	fclose( fp );
	/* ファイルをテンポラリ名から実際のファイル名に変更 */
	rename( tmpFileName, outFileName );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ５分間の移動平均値保存処理
 *
 * 機能：
 * 		計測値蓄積ファイル（TMSTOFIL.dat）に保存されている１分毎の移動平均値情報
 * 		を元に５分間の移動平均値を演算する。
 * 		演算対象は、引数 linePoint のポインタが指し示す計測値とし、演算結果を引数
 * 		*tmFlg（計測フラグを格納）、*tmVal（計測値を格納）にセットする。
 * 		計測種別が積算型の場合は移動平均処理は行わず、最後に取得した計測情報を使
 * 		用する。
 * 戻り値：
 * 		 0：正常終了
 */
int	func_tmdata_get( int linkPoint, int *tmFlg, int *tmVal )
{
	int						loopCnt;
	long long				workVal = 0;
	int						workFlg = 0;
	int						workCnt = 0;

	*tmFlg = 0;
	*tmVal = 0;

	/* 計測情報が未実装の場合 */
	if( ps_g_TMDATINF[linkPoint].statFlag == 0 )
		return( -1 );

	/* 計測フラグを取得 */
	*tmFlg = ps_g_TMSTOFIL->s_tmstsfil[linkPoint].avltmFlag;
	/* 計測値を取得 */
	*tmVal = ps_g_TMSTOFIL->s_tmstsfil[linkPoint].avltmVal;

#ifdef PV_DEBUG
	printf("LINKPOINT(%04d) : FLG:%02X,%9d\n", linkPoint, *tmFlg, *tmVal );
#endif
	/* 正常終了を返す */
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
int	func_abnormal_exit( int procNo )
{
	/* プロセス異常中フラグを設定 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_SET );
	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );
	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	return( 0 );
}

int	func_report_TM_record( time_t timeSec )
{
	FILE			*fp;
	struct tm		timeData;
	int				maxNumb;						/* 処理最大数 */
	int				loopCnt1;						/* ループカウンタ */
	char			outFileName[256];				/* */
	char			tmpFileName[256];				/* */
	char			strTime[16];
	int				tmrecCnt;					/* 状変機器数 */
	char			tmrecMsg[PV_TMREC_DATA_NUM][PV_MSG_SIZE_TMH];
	int				dtsdLockFlag;					/* データ送信ロックフラグ */

	localtime_r( &timeSec, &timeData );

	sprintf( strTime, "%04d%02d%02d%02d%02d%02d",
		timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );

/* [20250106] Chg St */
	strcpy( outFileName, PV_PATH_REC );
/* [20250106] Chg Ed */
	mkdir(outFileName, ( S_IRWXG | S_IRWXU ));	/* 所有者、グループの権限付与 */
	strncat( outFileName, ps_g_PVSYSINF->siteIdName, 8 );
	strncat( outFileName, strTime, 14 );
	strcat( outFileName, PV_FNAME_TMREC_PREFIX );
	strcpy( tmpFileName, outFileName );

	strcat( outFileName, ".DAT" );
	strcat( tmpFileName, ".TMP" );



#ifdef PV_DEBUG_TM
	printf("REPOEDIT : CREATE FILE [ %s ]\n", outFileName );
#endif
	/* データ送信ロックフラグを取得 */
	dtsdLockFlag = commfunc_manage_dtsdLockFlagGet( );
/* [20230224] Chg St */
	/* データ送信ロック中、または、計測蓄積・状態蓄積ファイル作成フラグがOFFの場合 */
	if( ( dtsdLockFlag != 0 ) || ( ps_g_PVSYSINF->recFileCre == 0 ) )
/* [20230224] Chg St */
	{
		while(1)
		{
			/* 計測記録メッセージを読み捨てる */
			tmrecCnt = commfunc_tmrec_msggettcp( tmrecMsg, PV_TMREC_DATA_NUM );

			if( tmrecCnt == 0 )
				break;
		}
		return 0;
	}

	/* ファイルのオープン */
	if( (fp = fopen( tmpFileName, "w" )) == NULL )
	{
		sprintf( errMsg, "Create File Error [ %s ]", tmpFileName );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return( -1 );
	}

	/* 収集装置情報を出力 */
	fprintf( fp, "BSD,%s,%s\n", ps_g_PVSYSINF->siteIdName, strTime );

	while(1)
	{
		/* 計測記録メッセージ取得 */
		tmrecCnt = commfunc_tmrec_msggettcp( tmrecMsg, PV_TMREC_DATA_NUM );
		if( tmrecCnt == 0 )
			break;

		/* 計測記録メッセージの取得 */
		for(loopCnt1 = 0 ; loopCnt1 < tmrecCnt ; loopCnt1++ )
		{
			/* 計測記録メッセージの出力 */
			fprintf( fp, "%s\n", tmrecMsg[loopCnt1] );
#ifdef PV_DEBUG_TM
			printf("%s\n", tmrecMsg[loopCnt1]);
#endif
		}
	}

	/* 終端情報を出力 */
	fprintf( fp, "END\n" );
	/* ファイルをクローズする */
	fclose( fp );

	/* ファイルをテンポラリ名から実際のファイル名に変更 */
	rename( tmpFileName, outFileName );


	return( 0 );
}

int	func_report_SV_record( time_t timeSec )
{
	FILE			*fp;
	struct tm		timeData;
	int				maxNumb;						/* 処理最大数 */
	int				loopCnt1;						/* ループカウンタ */
	char			outFileName[256];				/* */
	char			tmpFileName[256];				/* */
	char			strTime[16];
	int				svrecCnt;					/* 状変機器数 */
	char			svrecMsg[PV_SVREC_DATA_NUM][PV_MSG_SIZE_SVH];
	int				dtsdLockFlag;					/* データ送信ロックフラグ */

	localtime_r( &timeSec, &timeData );

	sprintf( strTime, "%04d%02d%02d%02d%02d%02d",
		timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );

/* [20250106] Chg St */
	strcpy( outFileName, PV_PATH_REC );
/* [20250106] Chg Ed */
	mkdir(outFileName, ( S_IRWXG | S_IRWXU ));	/* 所有者、グループの権限付与 */
	strncat( outFileName, ps_g_PVSYSINF->siteIdName, 8 );
	strncat( outFileName, strTime, 14 );
	strcat( outFileName, PV_FNAME_SVREC_PREFIX );
	strcpy( tmpFileName, outFileName );

	strcat( outFileName, ".DAT" );
	strcat( tmpFileName, ".TMP" );



#ifdef PV_DEBUG_SV
	printf("REPOEDIT : CREATE FILE [ %s ]\n", outFileName );
#endif

	/* データ送信ロックフラグを取得 */
	dtsdLockFlag = commfunc_manage_dtsdLockFlagGet( );
/* [20230224] Chg St */
	/* データ送信ロック中、または、計測蓄積・状態蓄積ファイル作成フラグがOFFの場合 */
	if( ( dtsdLockFlag != 0 ) || ( ps_g_PVSYSINF->recFileCre == 0 ) )
/* [20230224] Chg St */
	{
		while(1)
		{
			/* 状態記録メッセージ取得 */
			svrecCnt = commfunc_svrec_msggettcp( svrecMsg, PV_SVREC_DATA_NUM );

			if( svrecCnt == 0 )
				break;
		}
		return 0;
	}

	/* ファイルのオープン */
	if( (fp = fopen( tmpFileName, "w" )) == NULL )
	{
		sprintf( errMsg, "Create File Error [ %s ]", tmpFileName );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return( -1 );
	}

	/* 収集装置情報を出力 */
	fprintf( fp, "BSD,%s,%s\n", ps_g_PVSYSINF->siteIdName, strTime );

	while(1)
	{
		/* 状態記録メッセージ取得 */
		svrecCnt = commfunc_svrec_msggettcp( svrecMsg, PV_SVREC_DATA_NUM );
		if( svrecCnt == 0 )
			break;

		/* 状態記録メッセージの取得 */
		for(loopCnt1 = 0 ; loopCnt1 < svrecCnt ; loopCnt1++ )
		{
			/* 状態記録メッセージの出力 */
			fprintf( fp, "%s\n", svrecMsg[loopCnt1] );
#ifdef PV_DEBUG_SV
			printf("%s\n", svrecMsg[loopCnt1]);
#endif
		}
	}

	/* 終端情報を出力 */
	fprintf( fp, "END\n" );
	/* ファイルをクローズする */
	fclose( fp );

	/* ファイルをテンポラリ名から実際のファイル名に変更 */
	rename( tmpFileName, outFileName );


	return( 0 );
}

/* [20250807] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力 レポートデータ編集処理
 *
 * 機能：
 *
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_fcr_repodata_edit( time_t timeSec )
{
	FILE			*fp;
	struct tm		timeData;						/* 現在時刻(整形用) */
/* [20260515] Chg St */
	int				loopCnt1;						/* ループカウンタ */
	char			outFileName[256];				/* レポートデータファイル名 */
	char			tmpFileName[256];				/* レポートデータTMPファイル名 */
	char			strTime[15];					/* レポートデータファイル日付生成用文字列 */
	int				dtsdLockFlag;					/* データ送信ロックフラグ */
	char			repoMsg[PV_RPREC_DATA_NUM][PV_MSG_SIZE_REPO];
	int				repoCnt;						/* 実績データ数 */


	localtime_r( &timeSec, &timeData );

	sprintf( strTime, "%04d%02d%02d%02d%02d%02d",
		timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );

	strcpy( outFileName, PV_PATH_FTP );
	strncat( outFileName, strTime, 8 );
	strcat( outFileName, "/");
	mkdir(outFileName, ( S_IRWXG | S_IRWXU ));	/* ディレクトリ生成＆所有者、グループの権限付与 */
	strcat( outFileName, ps_g_PVSYSINF->siteIdName );
	strncat( outFileName, strTime, 14 );
	strcat( outFileName, PV_FNAME_FCRREPO_PREFIX );
	strcpy( tmpFileName, outFileName );

	strcat( outFileName, ".DAT" );
	strcat( tmpFileName, ".TMP" );

#ifdef PV_DEBUG_REPORT_CREATE
	printf("REPOEDIT : REPO FILE [ %s ]\n", outFileName );
#endif

	/* データ送信ロックフラグを取得 */
	dtsdLockFlag = commfunc_manage_dtsdLockFlagGet( );
	/* データ送信ロック中の場合 */
	if( dtsdLockFlag != 0 )
	{
		while(1)
		{
			/* 計測記録メッセージを読み捨てる */
			repoCnt = commfunc_rprec_msggettcp( repoMsg, PV_RPREC_DATA_NUM );

			if( repoCnt == 0 )
				break;
		}
		return 0;
	}

	while(1)
	{
		/* 計測記録メッセージ取得 */
		repoCnt = commfunc_rprec_msggettcp( repoMsg, PV_RPREC_DATA_NUM );
		if( repoCnt == 0 )
			break;

		/* ファイルのオープン */
		if( (fp = fopen( tmpFileName, "w" )) == NULL )
		{
			sprintf( errMsg, "Create File Error [ %s ]", tmpFileName );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			return( -1 );
		}

		/* 基本情報を出力(BSD,統合コントローラID,情報収集日時) */
		fprintf( fp, "BSD,%s,%s\n", ps_g_PVSYSINF->siteIdName, strTime );

		/* 計測記録メッセージの取得 */
		for(loopCnt1 = 0 ; loopCnt1 < repoCnt ; loopCnt1++ )
		{
			/* 計測記録メッセージの出力 */
			fprintf( fp, "%s\n", repoMsg[loopCnt1] );
#ifdef PV_DEBUG_TM
			printf("%s\n", repoMsg[loopCnt1]);
#endif
		}

		/* 終端情報を出力 */
		fprintf( fp, "END\n" );

		/* ファイルをクローズする */
		fclose( fp );

		/* ファイルをテンポラリ名から実際のファイル名に変更 */
		rename( tmpFileName, outFileName );

	}
/* [20260515] Chg Ed */

	return( 0 );
}
/* [20250807] Add Ed */

