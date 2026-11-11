/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251027] 2025.10.27
 * ・出力制御ユニット用ＰＬＣ（ModbusTCP）に対応
 * [20250421] 2025.04.21
 * ・Tesla社製蓄電池 Controls-V2に対応
 * [20240312] 2024.03.12
 * ・ファイル構造(サイズ)が前回と変わった場合、初期化するように修正
 * [20230902] 2023.09.02
 * ・ログ出力ファイルの保持数をdefine定義に変更
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 管理プロセス（システムマネージャ）
 *
 * 主な機能
 * 		（１）オンラインファイル生成処理の呼び出し
 * 		（２）通信処理プロセスの起動管理
 * 		（３）業務処理プロセスの起動管理
 * 		（４）プロセスの動作監視
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvcomm.h"
#include "embase.h"

#include "nkmanage.h"

#define	PV_OWN_NAME		"NKMANAGE"					/* 自プログラム名 */

#define	PV_OWN_INI_TIME	PV_WDOG_CYCLE				/* 初期状態作成期間（状変ロック期間） *//* 統合コントローラでは状変ロックはなしとする */

#define	PV_NTPD_OFFSET	30000						/* ntpd再起動検出用同期ずれミリ秒 */

#define	PV_M_MODE_SHUTDOWN			1				/* シャットダウン */
#define	PV_M_MODE_REBOOT			2				/* リブート */
#define	PV_M_RECOVERY_SET			1				/* リカバリーフラグ設定 */	
#define	PV_M_RECOVERY_NONE			2				/* リカバリーフラグ設定無し */	
#define	PV_M_PERSHUTDOWN_SET		1				/* 他系シャットダウンする */	
#define	PV_M_PERSHUTDOWN_NONE		2				/* 他系シャットダウンしない */	

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* 通信回線情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* 状変蓄積ファイルポインタ */

/* 使用ファイル定義 */
struct st_fileIndex     s_fileIndex[] =
{
    {   PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
    {   PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
    {   PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
    {   PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
    {   PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
    {   PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
    {   0,                  NULL,                    0, 0, 0 }
};

/* 通信プロトコルと対応プロセス情報 */
struct st_lineProcInfo	s_lineProcInfo[] = {
	/*      +--------------------------------------------- 通信プロトコル名称                     */
	/*      |                       +--------------------- 通信プロトコルに対応した通信プロセス名 */
	/*      |                       |                                                             */
	{ PV_COMPRO_TESLA1,      PV_PROCNAME_TESLA1      }, /* ＴＥＳＬＡ製バッテリー（ModbusTCP） */
	{ PV_COMPRO_TESLA2,      PV_PROCNAME_TESLA2      }, /* ＴＥＳＬＡ製バッテリー Controls-v2（ModbusTCP） *//* [20250421] Add */
	{ PV_COMPRO_SMA4,        PV_PROCNAME_SMA4        },	/* SMAパワーコンディショナ（ModbusTCP）*/
	{ PV_COMPRO_NKIOPLC1,    PV_PROCNAME_NKIOPLC1    }, /* 収集装置外部Ｉ／Ｏ用ＰＬＣ（SLMP:Ethernet） */
	{ PV_COMPRO_CEC1,        PV_PROCNAME_CEC1        }, /* 中央電子社製 接点伝送装置プロトコル（SMNP通信） */
	{ PV_COMPRO_TAKEMULTIM2, PV_PROCNAME_TAKEMULTIM2 }, /* タケモトデンキ製マルチメータ（ModbusRTU:RS485） */
	{ PV_COMPRO_HUAWEI2,     PV_PROCNAME_HUAWEI2     }, /* ＨＵＡＷＥＩ製パワーコンディショナ（ModbusTCP:SmartLogger経由） */
	{ PV_COMPRO_SANKEN2,     PV_PROCNAME_SANKEN2     }, /* サンケン電気製パワーコンディショナ（ModbusTCP:SmartLogger経由） */
	{ PV_COMPRO_MITSUMULTIM1,PV_PROCNAME_MITSUMULTIM1}, /* 三菱電機製EcoMonitor（ModbusRTU:RS485） */
	{ PV_COMPRO_COMPOWAYF1,  PV_PROCNAME_COMPOWAYF1  },	/* オムロンスマート計測監視機器ＫＭ１シリーズ */
	{ PV_COMPRO_SCHNEIDER1,  PV_PROCNAME_SCHNEIDER1  }, /* シュナイダー製マルチメータ（ModbusTCP） *//* [20230529] Add */
	{ PV_COMPRO_SCHPLC1,     PV_PROCNAME_SCHPLC1     }, /* 出力制御ユニット用ＰＬＣ（ModbusTCP） *//* [20251027] Add */
	{ PV_COMPRO_PPMSMA1,     PV_PROCNAME_PPMSMA1     }, /* SMA製PPM（ModbusTCP） *//* [20251101] Add */
	{ "-STOPPER-",           "-STOPPER-"             }	/* （ストッパー） */
};

static char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int				gpio_flg = 0;				/* ＧＰＩＯ使用可フラグ */
static int				gpio_errflg = 0;			/* ＧＰＩＯエラーＬＥＤ点灯フラグ */
static int				silentrb_flg = 0;			/* サイレントリブートフラグ */
static int				ntpdCheck = 3000;			/* ntpd 動作確認時間 */
static int				ntpAllRejectCnt = 0;		/* ntpd 同期先なしカウンタ */
static int				ntpOffsetDifCnt = 0;		/* ntpd 同期サーバ遅延カウンタ */
static int				ntpStopFlg = 0;				/* ntpd 停止要求フラグ */

/* 内部関数定義 */
int	func_filemake( void );
int	func_syslog_out( void );
int	func_johenlog_out( void );
int	func_comproc_search( int, int );
int	func_comproc_execute( int, int, char * );
int	func_gyomuproc_execute( int, int );
int	func_process_stop();
int	func_PVSYSLOG_Create();
int	func_SIGUSR1_send();
int	func_shutdown_exe( int, int, int );
int	func_proc_shutdown_exe( int, int, int, int );
void func_router_reset();							/* */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 管理プロセス主処理
 *
 * 機能：
 *		（１）通信処理プロセスの起動
 *		通信回線情報ファイル（COMLIINF.dat）に登録された通信回線１から１０
 *		に定義された通信プロトコルを確認し、通信プロトコルに対応したプロセスを通
 *		信回線番号を引数として子プロセスとして起動する。
 *		通信回線番号はプログラム番号１から１０としても管理する。
 *
 *		（２）業務処理プロセスの起動
 *		管理ファイル（PVSYSINF.dat）に設定されている業務プロセス１から１０（プロ
 *		グラム番号としては１１から２０）の起動設定を確認し、起動する必要のある業
 *		務プロセスを子プロセスとして起動する。業務プロセス起動時はプログラム番号
 *		を引数として渡す（１１から２０）。
 *
 *		（３）プロセスの動作管理
 *		プロセスの動作状態を５秒周期で確認する。確認の方法は２通りあり、一つは
 *		ウォッチドッグ、もう一つはプロセス終了通知。
 *		ウォッチドッグは管理ファイル上のプログラム番号毎に用意したウォッチドッグ
 *		フラグを５秒周期でセットし、フラグがリセットされていない場合はウォッチ
 *		ドッグカウンタを＋１する。ウォッチドッグカウンタが一定異常に達した場合は
 *		プロセスを強制停止した後に、再起動を行う。
 *		プロセス終了通知（wait()関数でプロセスIDをキャッチした場合も同様にプロセ
 *		スの再起動を行う。
 *
 * 		（４）プロセス再起動管理
 * 		前項３で再起動を行う場合、再起動回数が一定異常に達した場合は、アプリケー
 * 		ションの停止を行う。
 */ 
int	main(int argc, char *argv[])
{
	FILE					*fp;
	int						pidNumb_check;
	int						loopCnt1;
	int						procNumb;
	int						lineNumb;
	int						retCode;
	int						fdLog;
	int						pidNumb;			/* プロセスID */
	int						dtsdLockFlag;		/* */
	int						initTime = PV_OWN_INI_TIME;	/* */
	int						mShutdown_flg = 0;	/* */
	char					checkFile[1024];
	char					checkCmd[512];
	char					checkData[512];
	char					*checkPt;
	time_t					chktim_old;			/* */
	time_t					chktim_now;			/* */
	struct tm				localTime;			/* */

	/* 多重起動防止 */

	/* プロセスＩＤの取得 */
	pidNumb = getpid();

	printf("Start %s Process : pid( %d )\n", PV_OWN_NAME, pidNumb );

	/* 自プロセス名の取得（不要なパスはカット） */
	if( (checkPt = strrchr( argv[0], (int)'/' )) == NULL )
		strcpy( checkFile, argv[0] );
	else
		strcpy( checkFile, checkPt + 1 );
	/* 起動中のプロセス名のプロセスＩＤ（最古のもの）を取得するコマンドを設定 */
	sprintf( checkCmd, "pgrep -o %s", checkFile );
	/* コマンドを実行してパイプから取得 */
	if( (fp = popen( checkCmd, "r" )) == NULL )
	{
		/* 確認コマンド実行失敗 */
		printf("%s : TEMP CHECK FILE OPEN Error!\n", PV_OWN_NAME);
		return( -1 );
	}
	/* プロセスＩＤを取得 */
	fgets( checkData, 512, fp );
	/* パイプを閉じる */
	pclose( fp );
	/* プロセスＩＤの取得 */
	pidNumb_check = atol( checkData );
	/* 自プロセスＩＤと異なる場合は多重起動 */
	if( pidNumb != pidNumb_check )
	{
		printf("%s : PROCESS DUBLE EXECUTE Error!\n", PV_OWN_NAME );
		exit( 0 );
	}

	/* カーネルパラメータの有効化 */
	system( "/sbin/sysctl -p" );

	/*----------------------------*/
	/* メイン処理（起動準備処理） */
	/*----------------------------*/

	/*----------------------------*/
	/* システムログファイルの生成 */
	/*----------------------------*/
	retCode = func_PVSYSLOG_Create();

	/*------------------------------------------------*/
	/* システムログファイルをマップファイルとして展開 */
	/*------------------------------------------------*/

	/* システムログファイルパスの展開 */
	strcpy(checkFile, PV_PATH_DAT );
	strcat(checkFile, PV_FNAME_PVSYSLOG );
	strcat(checkFile, ".dat");

	/* システムログファイルを開く */
	fdLog = open( checkFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fdLog == -1 )
	{
		return( -1 );
	}

	/* パーミッションを変更する */
	chmod( checkFile, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) ;

	/* ファイルをメモリマップ */
	ps_g_PVSYSLOG = (struct st_PVSYSLOG *)mmap( NULL, sizeof(struct st_PVSYSLOG), PROT_READ | PROT_WRITE, MAP_SHARED, fdLog, 0 );
	if( ps_g_PVSYSLOG == MAP_FAILED)
	{
		return( -1 );
	}

	/* メモリマップアドレスをグローバルアドレスに設定 */
	ps_gpt_PVSYSLOG = (struct st_PVSYSLOG *)ps_g_PVSYSLOG;

	/*------------------------------------------------------*/
	/* ファイル排他制御用のミューテックスを共有メモリに生成 */
	/*------------------------------------------------------*/
	retCode = commfunc_mutex_create();
	if(retCode < 0)
	{
		printf("%s : Mutex Create Error!\n", PV_OWN_NAME);
		return( -1 );
	}

	/* 管理プロセス起動ログ */
	commfunc_syslog_msgset( PV_OWN_NAME, "##### START #####" ); 

	/*------------------------------------------------------------*/
	/* 設定ファイル（*.csv）からオンラインファイル（*.dat）を生成 */
	/*------------------------------------------------------------*/

	/* 設定ファイルの展開 */
	if( (retCode = func_filemake()) != 0 )
	{
		/*----------------------------------------*/
		/* 設定ファイルの展開に問題が発生した場合 */
		/* （収集装置の再起動を実施）             */
		/*----------------------------------------*/

		/* システムログに出力 */
		printf("%s(PID:%d): ##### ABNORMAL STOP (FILEMAKE ERROR) #####\n", PV_OWN_NAME, pidNumb );
		commfunc_syslog_msgset( PV_OWN_NAME, "FILE CREATE (CSV->DAT) Error!" ); 
		commfunc_syslog_msgset( PV_OWN_NAME, "##### ABNORMAL STOP #####" ); 

		/* システムログの保存 */
		func_syslog_out();

		/* メモリマップを解除 */
		munmap( ps_g_PVSYSLOG, sizeof(struct st_PVSYSLOG) );
		/* システムログファイルをクローズ */
		close( fdLog );

		/* ファイル排他制御用のミューテックス用の共有メモリをデタッチ */
		commfunc_mutex_detach();

		/* リブート処理を実施（３０秒待ち、リカバリーフラグセット） */
		func_shutdown_exe( PV_M_MODE_REBOOT, 30, PV_M_RECOVERY_SET );

		return( -1 );
	}

	/* システムログファイルのメモリマップを解除 */
	munmap( ps_g_PVSYSLOG, sizeof(struct st_PVSYSLOG) );
	/* システムログファイルをクローズ */
	close( fdLog );

	/* グローバルアドレスを初期化 */
	ps_gpt_PVSYSLOG = NULL;

	/*---------------------------------------------------------------*/
	/* システムログファイルを含む全てのオンラインファイル（*.dat）を */
	/* オープンしてＭＭＡＰ化                                        */
	/*---------------------------------------------------------------*/
	retCode = commfunc_file_open( s_fileIndex );
	if(retCode < 0)
	{
		/* システムログファイルには出力できないため通常出力 */
		printf("%s : File Resource Open Error!\n", PV_OWN_NAME);

		/* ミューテックスを破棄 */
		commfunc_mutex_destroy();

		/* リブート処理を実施（３０秒待ち、リカバリーフラグセット） */
		func_shutdown_exe( PV_M_MODE_REBOOT, 30, PV_M_RECOVERY_SET );

		return( -1 );
	}

	/* システムログの保存 */
	func_syslog_out();

	
	/*----------------------------*/
	/* embaseライブラリ関数使用開始 */
	/*----------------------------*/
	if( embase_func_startup( 1 ) == 0 )
	{
		/* embaselib使用有無を設定 */
		ps_g_PVSYSINF->embaseUseFlg = 1;
		commfunc_syslog_msgset( PV_OWN_NAME, "called embase_func_startup" ); 

 	}
	else
	{
		/* システムログに出力 */
		printf("%s(PID:%d): ##### embase_func_startup ERROR #####\n", PV_OWN_NAME, pidNumb );
		commfunc_syslog_msgset( PV_OWN_NAME, "##### embase_func_startup ERROR #####" ); 
	}
	func_syslog_out();
	

	/*--------------------------------------------------*/
	/* サイレントリブートファイル（フラグ）チェック処理 */
	/* （収集装置の運転/停止状変マスク）                */
	/*--------------------------------------------------*/

	/* サイレントリブート確認用ファイルパスの設定 */
	strcpy( checkFile, PV_PATH_DAT );
	strcat( checkFile, PV_FNAME_PV_SILENTRB );
	/* 収集装置のサイレントリブートが設定されている場合 */
	if( access( checkFile, R_OK | W_OK ) == 0 )
	{
		commfunc_syslog_msgset( PV_OWN_NAME, "### NK-CEMS START JOHEN MASK (SILENT REBOOT MODE) ###" ); 

		/* 装置状態状変：装置運転（運転）状変マスク */
		commfunc_johen_pos_mask( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 1, 0, 0,
			ps_g_SVSTSFIL, ps_g_SVMSKINF );

		/* サイレントモードフラグを削除 */
		unlink( checkFile );

		commfunc_syslog_msgset( PV_OWN_NAME, "### SILENT REBOOT MODE [ OFF ] ###" ); 
	}
	else
	{
		/* 装置状態状変：装置運転（停止）状変マスク */
		commfunc_johen_pos_mask( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 0, 0, 0,
			ps_g_SVSTSFIL, ps_g_SVMSKINF );

		/* 装置状態状変：装置運転（運転） */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 1, 0, 0,
			ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	if( 1 == ps_g_PVSYSINF->embaseUseFlg )
	{
		/* 1系が動作中の場合 */
		if( embase_func_get_kei_info() == EMBASE_SYSNUMBER_1 )
		{
			/* 装置状態状変：2系（停止） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_NUMB_2, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

			/* 装置状態状変：1系（運転） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_NUMB_1, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
		/* 2系が動作中の場合 */
		else //if( embase_func_get_kei_info() == EMBASE_SYSNUMBER_2 )
		{
			/* 装置状態状変：1系（停止） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_NUMB_1, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

			/* 装置状態状変：2系（運転） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_NUMB_2, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
	}
	/* 装置状態状変：UPSシャットダウン指令（復帰） */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_UPS_SHUTDOWN_EXE, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/*----------------------------------------*/
	/* 初期状態作成期間設定（状変ロック設定） */
	/*----------------------------------------*/
//	commfunc_manage_johenLockFlagSet( PV_FLG_STS_SET );

//	commfunc_syslog_msgset( PV_OWN_NAME, "### INITIAL START-UP MODE (JOHEN LOCK) [ START ] ###" ); 

	/* 状変ロックを解除 */
	commfunc_manage_johenLockFlagSet( PV_FLG_STS_RESET );

	/* 装置状態状変：手動停止（復帰） */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_MANUALSTOP, 0, 0, 0,
		ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/*------------------------------------------------*/
	/* 縮退運転（リカバーフラグファイル）チェック処理 */
	/*------------------------------------------------*/

	/* 縮退運転中フラグ（ファイルリカバリ実施フラグ）が存在するか確認 */
	strcpy( checkFile, PV_PATH_DAT );
	strcat( checkFile, PV_FNAME_PV_FALLBACK );
	/* 縮退運転中フラグが設定されている場合 */
	if( access( checkFile, R_OK | W_OK ) == 0 )
	{
		/* 装置状態状変：縮退運転（縮退モード） 発生 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SHUKUTAI, 1, 0, 0,
			ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}
	else
	{
		/* 装置状態状変：縮退運転（縮退モード） 復帰 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SHUKUTAI, 0, 0, 0,
			ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	/*----------------------------------------------------------------*/
	/* データ送信ロック（データ送信ロックフラグファイル）チェック処理 */
	/* （データ送信ロック＝中身が空のデータを送信する）               */
	/*----------------------------------------------------------------*/

	/* データ送信ロック用ファイルパスの設定 */
	strcpy( checkFile, PV_PATH_DAT );
	strcat( checkFile, PV_FNAME_PV_DTSDLOCK );
	/* データ送信ロックが設定されている場合 */
	if( access( checkFile, R_OK | W_OK ) == 0 )
	{
		/* データ送信ロックフラグを設定 */
		commfunc_manage_dtsdLockFlagSet( PV_FLG_STS_SET );
		/* データ送信ロックフラグＯＮ */
		dtsdLockFlag = 1;
		/* ログ出力 */
		commfunc_syslog_msgset( PV_OWN_NAME, "### DATA SEND LOCK [ ON ] ###" ); 
	}
	else
	{
		/* データ送信ロックフラグを解除 */
		commfunc_manage_dtsdLockFlagSet( PV_FLG_STS_RESET );
		/* データ送信ロックフラグＯＦＦ */
		dtsdLockFlag = 0;
		/* ログ出力 */
		commfunc_syslog_msgset( PV_OWN_NAME, "### DATA SEND LOCK [ OFF ] ###" ); 
	}

	/*----------------------------------------------*/
	/* メイン処理（通信系・業務系プロセス起動処理） */
	/*----------------------------------------------*/

	/*----------------------*/
	/* 通信系プロセスの起動 */
	/*----------------------*/
	for(lineNumb = 1 ; lineNumb <= PV_MAX_COMLINE ; lineNumb++)
	{
		retCode = func_comproc_search( lineNumb, PV_PROC_NEWSTART );
		if(retCode < 0)
		{
			printf("##### FATAL ERROR !! [ Communication Process Execute Error Line:%02d ]\n", lineNumb );

			/* 致命的障害 */
			sprintf( errMsg, "FATAL ERROR: PROC-NO ( %d ) Failure to start", lineNumb );
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

			/* リブート処理を実施（１２０秒待ち、リカバリーフラグセット） */
			func_proc_shutdown_exe( PV_M_MODE_REBOOT, 120, PV_M_RECOVERY_SET, 0 );

			/* 終了 */
			exit( -1 );
		}
	}
	/* 業務系プロセスの起動 */
	for(procNumb = 11 ; procNumb <= PV_MAX_PROC ; procNumb++)
	{
		retCode = func_gyomuproc_execute( procNumb, PV_PROC_NEWSTART );
		if(retCode < 0)
		{
			printf("##### FATAL ERROR !! [ Bussiness Process Execute Error Numb:%02d ]\n", procNumb );

			/* 致命的障害 */
			sprintf( errMsg, "FATAL ERROR: PROC-NO ( %d ) Failure to start", procNumb );
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

			/* リブート処理を実施（１２０秒待ち、リカバリーフラグセット） */
			func_proc_shutdown_exe( PV_M_MODE_REBOOT, 120, PV_M_RECOVERY_SET, 0 );

			/* 終了 */
			exit( -1 );
		}
	}

	/*--------------------------*/
	/* メイン処理（ループ処理） */
	/*--------------------------*/

	/* 現在時刻を取得（保存用） */
	chktim_old = time( NULL );

	/* 主処理ループ */
	while( 1 )
	{

		/*--------------------------------*/
		/* データ送信ロックファイルの確認 */
		/*--------------------------------*/

		/* データ送信ロック用ファイルパスの設定 */
		strcpy( checkFile, PV_PATH_DAT );
		strcat( checkFile, PV_FNAME_PV_DTSDLOCK );
		/* データ送信ロックが設定されている場合 */
		if( access( checkFile, R_OK | W_OK ) == 0 )
		{
			if( dtsdLockFlag == 0 )
			{
				/* データ送信ロックフラグを設定 */
				commfunc_manage_dtsdLockFlagSet( PV_FLG_STS_SET );
				/* データ送信ロックフラグＯＮ */
				dtsdLockFlag = 1;
				/* ログ出力 */
				commfunc_syslog_msgset( PV_OWN_NAME, "### DATA SEND LOCK [ ON ] ###" ); 
			}
		}
		else
		{
			if( dtsdLockFlag != 0 )
			{
				/* データ送信ロックフラグを解除 */
				commfunc_manage_dtsdLockFlagSet( PV_FLG_STS_RESET );
				/* データ送信ロックフラグＯＦＦ */
				dtsdLockFlag = 0;
				/* ログ出力 */
				commfunc_syslog_msgset( PV_OWN_NAME, "### DATA SEND LOCK [ OFF ] ###" ); 
			}
		}

		/* プロセス異常中フラグが設定されている場合 */
		if( commfunc_manage_abnFlagCheck() != 0 )
		{
			/* 装置状態状変：装置異常（発生） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}

		/*------------------------------------------------*/
		/* 子プロセス（通信系・業務系プロセス）の動作確認 */
		/*------------------------------------------------*/

		/* 子プロセスが死んでいないか確認 */
		pidNumb = waitpid(-1, NULL, WNOHANG);
		/* 子プロセスが死んでいた場合 */
		if( pidNumb > 0 )
		{
			/* プログラム番号を確認 */
			retCode = commfunc_manage_stopProc( &procNumb, pidNumb );
			sprintf( errMsg, "ERROR: CHILD PROCESS DEAD (PID:%d PROC:%d RET:%d)", pidNumb, procNumb, retCode );
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

			if( retCode != 0 )
			{
				/* 装置状態状変：装置異常（発生） */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

				/* 再起動可否チェックを実施 */
				retCode = commfunc_manage_reStartCheck( procNumb );
				if( retCode < 0 )
				{
					/* 再起動不可の場合（収集装置の再起動を実施） */

					printf("%s(PID:%d): ##### FATAL ERROR PROGRAM RESTART DISABLE (PROC-NO:%d) #####\n", PV_OWN_NAME, getpid(), procNumb );

					/* システムログに出力 */
					sprintf( errMsg, "ATTENTION: PROC-NO:%02d  RESTART DISABLE !! (SYSTEM-REBOOT!!)", procNumb );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

					/* リブート処理を実施（３０秒待ち、リカバリーフラグセット） */
					func_proc_shutdown_exe( PV_M_MODE_REBOOT, 30, PV_M_RECOVERY_SET, 0 );

					exit( 0 );
				}
				else
				{
					/* 再起動可の場合 */

					/* 対象が通信系プロセスの場合 */
					if( procNumb <= 10 )
					{
						/* システムログに出力 */
						sprintf( errMsg, "ATTENTION: PROC-NO:%d  RESTART ENABLE (COMLINE-PROCESS)", procNumb );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

						/* プロセスの再起動を実施 */
						retCode = func_comproc_search( procNumb, PV_PROC_RESTART );
					}
					/* 対象が業務系プロセスの場合 */
					else
					{
						/* システムログに出力 */
						sprintf( errMsg, "ATTENTION: PROC-NO:%d  RESTART ENABLE (BUSINES-PROCESS)", procNumb );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

						/* プロセスの再起動を実施 */
						retCode = func_gyomuproc_execute( procNumb, PV_PROC_RESTART );
					}
				}
			}
			continue;
		}

		/*--------------------------------------------------------*/
		/* 遠隔指示ファイルの確認処理                             */
		/* （リモートによる遠隔指示ファイルの有無チェックを行う） */
		/*--------------------------------------------------------*/

		/*--------------------------------------*/
		/* サイレントリブート指示ファイルの確認 */
		/*--------------------------------------*/

		/* サイレントリブート確認用ファイルパスの設定 */
		strcpy( checkFile, PV_PATH_DAT );
		strcat( checkFile, PV_FNAME_PV_SILENTRB );
		/* 収集装置のサイレントリブートが設定されている場合 */
		if( access( checkFile, R_OK | W_OK ) == 0 )
		{
			if( silentrb_flg == 0 )
			{
				commfunc_syslog_msgset( PV_OWN_NAME, "### SILENT REBOOT MODE [ ON ] ###" ); 
			}
			silentrb_flg = 1;
		}
		else
		{
			if( silentrb_flg == 1 )
			{
				commfunc_syslog_msgset( PV_OWN_NAME, "### SILENT REBOOT MODE [ OFF ] ###" ); 
			}
			silentrb_flg = 0;
		}

		/*---------------------------------------------------------------*/
		/* 収集装置の停止または再起動指示ファイルの確認                  */
		/* （モニタリングサーバーに送信するファイル名は分単位のため、停  */
		/*   止または再起動はモニタリングサーバーへのデータ送信に影響が  */
		/*   出ないように5分で割り切れる時間帯は確認対象としない）       */
		/*---------------------------------------------------------------*/

		/* 現在時刻を取得 */
		localtime_r( &chktim_old, &localTime );
		/* 15秒で割り切れる時間以外の場合 */
		//if( ( localTime.tm_sec % 15 ) == 0 )
		{

			/*----------------------------------*/
			/* シャットダウン指示ファイルの確認 */
			/*----------------------------------*/

			/* 装置シャットダウン用ファイルパスの設定 */
			strcpy( checkFile, PV_PATH_DAT );
			strcat( checkFile, PV_FNAME_PV_SHUTDOWN );

			/* 収集装置のシャットダウンが設定されている場合 */
			if( ( access( checkFile, R_OK | W_OK ) == 0 ) || ( mShutdown_flg ) )
			{
				commfunc_syslog_msgset( PV_OWN_NAME, "##### SHUTDOWN SYSTEM START #####" ); 

				/* 手動停止扱いが設定されている場合 */
				if( commfunc_manage_manualStopGet() != 0 )
				{
					/* 装置状態状変：手動停止 */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_MANUALSTOP, 1, 0, 0,
						ps_g_SVSTSFIL, ps_g_SVMSKINF );
				}

				/* 停止指示ファイルを削除 */
				unlink( checkFile );

				/* シャットダウン処理を実施（０秒待ち、リカバリーフラグセットしない） */
				func_proc_shutdown_exe( PV_M_MODE_SHUTDOWN, 0, PV_M_RECOVERY_NONE, PV_M_PERSHUTDOWN_SET );

				exit( 0 );
			}

			/*--------------------------*/
			/* 再起動指示ファイルの確認 */
			/*--------------------------*/

			/* 装置再起動確認用ファイルパスの設定 */
			strcpy( checkFile, PV_PATH_DAT );
			strcat( checkFile, PV_FNAME_PV_REBOOT );

			/* 収集装置の再起動が設定されている場合 */
			if( access( checkFile, R_OK | W_OK ) == 0 )
			{
				commfunc_syslog_msgset( PV_OWN_NAME, "##### REBOOT SYSTEM START #####" ); 

				/* 手動停止扱いが設定されている場合 */
				if( commfunc_manage_manualStopGet() != 0 )
				{
					/* 装置状態状変：手動停止 */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_MANUALSTOP, 1, 0, 0,
						ps_g_SVSTSFIL, ps_g_SVMSKINF );
				}

				/* 停止指示ファイルを削除 */
				unlink( checkFile );

				/* リブート処理を実施（０秒待ち、リカバリーフラグセットしない） */
				func_proc_shutdown_exe( PV_M_MODE_REBOOT, 0, PV_M_RECOVERY_NONE, 0 );

				exit( 0 );
			}

			/*------------------------------------------*/
			/* アプリケーションの停止指示ファイルの確認 */
			/*------------------------------------------*/

			/* 業務アプリケーション停止確認用ファイルパスの設定 */
			strcpy( checkFile, PV_PATH_DAT );
			strcat( checkFile, PV_FNAME_PV_STOP );

			/* 収集装置のアプリケーション停止が設定されている場合 */
			if( access( checkFile, R_OK | W_OK ) == 0 )
			{
				commfunc_syslog_msgset( PV_OWN_NAME, "##### STOP SYSTEM START #####" ); 

				/* サイレント設定なしの場合 */
				if( silentrb_flg == 0 )
				{
					/* 装置状態状変：装置運転（停止） */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 0, 0, 0,
						ps_g_SVSTSFIL, ps_g_SVMSKINF );

					/* 手動停止扱いが設定されている場合 */
					if( commfunc_manage_manualStopGet() != 0 )
					{
						/* 装置状態状変：手動停止 */
						commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_MANUALSTOP, 1, 0, 0,
							ps_g_SVSTSFIL, ps_g_SVMSKINF );
					}
				}
				else
				{
					/* 装置状態状変：装置運転（停止）状変マスク対象 */
					commfunc_johen_pos_mask( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 0, 0, 0,
						ps_g_SVSTSFIL, ps_g_SVMSKINF );

					commfunc_syslog_msgset( PV_OWN_NAME, "#####   (SILENT MODE)   #####" ); 
				}

				/* 全プロセス停止を実施 */
				func_process_stop();
				/* 停止指示ファイルを削除 */
				unlink( checkFile );

				/* 処理ループを抜ける */
				break;
			}

		}

		/*----------------------------------------------------------*/
		/* 子プロセス（通信系・業務系プロセス）のウォッチドッグ処理 */
		/*----------------------------------------------------------*/

		/* ウォッチドッグ処理 */
		retCode = commfunc_manage_watchdogSet( );
		if( retCode == 0 )
		{
			/* プロセス異常中フラグが未設定？ */
			if( commfunc_manage_abnFlagCheck() == 0 )
			{
				/* 装置状態状変：装置異常（復帰） */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSERR, 0, 0, 0,
					ps_g_SVSTSFIL, ps_g_SVMSKINF );
			}
		}
		else
		{
			/* 装置状態状変：装置異常（発生） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSERR, 1, 0, 0,
				ps_g_SVSTSFIL, ps_g_SVMSKINF );

		}

		/* 状変記録の保存 */
		/* （マップドファイルからテキストファイルへ保存） */
		func_johenlog_out();

		/* システムログの保存 */
		/* （マップドファイルからテキストファイルへ保存） */
		func_syslog_out();

		/* 一時休止                            */
		/* （管理プロセスは5秒周期で動作する） */
		//sleep(PV_WDOG_CYCLE);
		/* 5秒周期になるまで待機する */
		commfunc_wait( PV_WDOG_CYCLE, 0 );

		/*------------------------------------------------------------------*/
		/* OSの時刻が著しく変化（20秒以上）した場合はシステムログに記録する */
		/*------------------------------------------------------------------*/

		/* 保存時刻を一時休止時間分補正する */
		chktim_old += PV_WDOG_CYCLE;
		/* 現在時刻を取得 */
		chktim_now = time( NULL );
		/* 現在時刻が保存時刻と比較して20秒以上差異が存在する場合はログを記録する */
		if( ( chktim_now < ( chktim_old - 20 ) ) || ( chktim_now > ( chktim_old + 20 ) ) )
		{
			/* 現在時刻を取得 */
			localtime_r( &chktim_old, &localTime );
			/* ログメッセージを作成 */
			sprintf( errMsg, "ATTENTION: SIGNIFICANT TIME CORRECTION : Assumed Time [%04d/%02d/%02d %02d:%02d:%02d]",
				localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
				localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
			/* ログ出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
		}
		/* 現在時刻を保存 */
		chktim_old = chktim_now;

		/*----------------------------*/
		/* 初期状態作成期間の終了確認 */
		/*----------------------------*/

//		/* 初期状態作成時間が残っている場合 */
//		if( initTime )
//		{
//			/* 初期状態作成期間を削減 */
//			initTime -= PV_WDOG_CYCLE;
//
//			/* 初期状態作成期間終了の場合 */
//			if( initTime <= 0 )
//			{
//				/* 初期状態作成期間を0に設定 */
//				initTime = 0;
//
//				/* 状変ロックを解除 */
//				commfunc_manage_johenLockFlagSet( PV_FLG_STS_RESET );
//
//				/* ログ出力 */
//				commfunc_syslog_msgset( PV_OWN_NAME, "### INITIAL START-UP MODE (JOHEN LOCK) [ END ] ###" ); 
//			}
//		}

		/*--------------------------*/
		/* ルータリセット要求の確認 */
		/*--------------------------*/

		/* ルータリセット要求が設定されているか確認 */
		if( commfunc_manage_routerResetGet() )
		{
			/* ルータリセット処理を実行 */
			func_router_reset();
		}
	}

	/*------------------------------*/
	/* 収集装置ソフトウェア終了処理 */
	/*------------------------------*/

	/* 状変記録の保存 */
	func_johenlog_out();

	if( 1 == ps_g_PVSYSINF->embaseUseFlg )
	{
		commfunc_syslog_msgset( PV_OWN_NAME, "call embase_func_finish" ); 
		func_syslog_out();
		/* embase_func_startup() で確保したリソースを開放 */
		embase_func_finish();
	}

	/* ログを出力 */
	commfunc_syslog_msgset( PV_OWN_NAME, "##### NORMAL STOP #####" ); 
	func_syslog_out();

	/* オンラインデバッガ強制停止を実施 */
	func_SIGUSR1_send();

	/* 使用ファイルをクローズ */
	commfunc_file_close( s_fileIndex );

	/* ミューテックスを破棄 */
	commfunc_mutex_destroy();


printf("NKMANAGE : ############ EXIT ############\n");

	/* 正常終了 */
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインファイル生成処理呼び出し
 *
 * 機能：
 * 		オンラインファイル生成処理を呼び出す。
 * 		オンラインファイル生成処理が失敗した場合は異常終了する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_filemake( void )
{
	int						retCode;
	char					prgData[1024];

	/* 通信プロセスのパス設定 */
	sprintf( prgData, "%s%s", PV_PATH_EXE, "filemake" );
	/* データ作成プロセスの起動 */
	retCode = system( prgData );

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信プロセス確認処理
 *
 * 機能：
 * 		引数 lineNumb の通信回線番号に該当する通信プロトコルを通信回線情報
 * 		ファイル（COMLIINF.dat）をから検索し、該当する通信プロトコルがある場合は
 * 		プロセスを起動する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了（該当するプロセスが無い）
 */
int	func_comproc_search(int lineNumb, int startMode )
{
	int					loopCnt;					/* ループカウンタ */
	int					retCode = -1;				/* 関数戻り値 */

	/* 通信回線番号が不正 */
	if( (lineNumb < 1) || (lineNumb > PV_MAX_COMLINE))
		return( -1 );

	/* 通信回線が未実装 */
	if((ps_g_COMLIINF + lineNumb - 1)->statFlag == 0)
		return( 0 );

	for( loopCnt = 0 ; strncmp( s_lineProcInfo[loopCnt].protoName, "-STOPPER-", 12 ) != 0 ; loopCnt++ )
	{
		/* 一致する通信プロトコルが見つかった場合 */
		if( strncmp( (ps_g_COMLIINF + lineNumb - 1)->protoName, s_lineProcInfo[loopCnt].protoName, 12 ) == 0 )
		{
			/* 通信プロトコルに対応したプロセスを起動 */
			retCode = func_comproc_execute( lineNumb, startMode, s_lineProcInfo[loopCnt].procName );
			break;
		}
	}

	if( retCode != 0 )
	{
		/* 異常終了 */
		return( -1 );
	}
	/* 正常終了 */
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信プロセス起動処理
 *
 * 機能：
 * 		引数 lineNumb の通信回線番号に該当する通信プロセスを起動し、起動情報を管
 * 		理ファイルに登録する。
 * 		プロセスは子プロセスとして起動する。
 * 戻り値：
 * 		 0：正常処理
 */
int	func_comproc_execute(int lineNumb, int startMode, char *procName)
{
	int						pidNumb;
	int						retCode;
	char					prgData[1024];
	char					argData[1024];

	/* 通信プロセスのパス設定 */
	sprintf( prgData, "%s%s", PV_PATH_EXE, procName);
	/* 通信プロセスの引数（通信回線番号）設定 */
	sprintf( argData, "%d", lineNumb);

	sprintf( errMsg, "Child Process Start (%s %s)", prgData, argData);
	commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

	/* プロセス起動フラグをセット */
	retCode = commfunc_manage_execFlagSet( lineNumb, startMode );
	if( retCode != 0 )
	{
		/* 致命的障害が発生 */
		return( -1 );
	}

	/* プロセスの名称を格納 */
	commfunc_manage_procNameSet( lineNumb, procName );

	/* フォークする */
	pidNumb = fork();

	/* 子プロセス */
	if(pidNumb == 0)
	{
		/* （子プロセスの場合） */

		/* 通信プロセスを起動 */
		retCode = execl( prgData, prgData, argData, NULL );
		/* 起動失敗 */
		if(retCode == -1)
		{
			sprintf( errMsg, "ERROR: Child Process Start Error (%s %s)", prgData, argData);
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
		}
		/* 通信プロセスの起動に失敗した場合は子プロセス（自分自身）を終了する */
		exit( -1 );
	}
	else
	{
		/* フォーク失敗？ */
		if(pidNumb == -1)
		{
			sprintf( errMsg, "ERROR: Fork Error");
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
			return( -1 );
		}
		else
		{
			sprintf( errMsg, "Fork Success (PID:%d)", pidNumb);
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
			/* 子プロセスのＰＩＤを格納 */
			commfunc_manage_pidSet( lineNumb, pidNumb );
		}
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 業務プロセス確認処理
 *
 * 機能：
 * 		引数 gyomuNumb の業務プログラム番号に該当する業務プロセスの起動有無を管
 * 		理ファイル（PVSYSINF.dat）をから検索し、該当するプログラム番号が起動対象
 * 		の場合はプロセスを起動する。
 * 		業務プロセス起動後は、起動情報を管理ファイルに登録する。
 * 		プロセスは子プロセスとして起動する。
 * 戻り値：
 * 		 0：正常終了
 */
int	func_gyomuproc_execute( int gyomuNumb, int startMode )
{
	int						pidNumb;
	int						retCode;
	char					procName[9];
	char					prgData[1024];
	char					argData[1024];

	/* プログラム番号が不正の場合 */
	if( ( gyomuNumb <= PV_MAX_COMLINE ) || ( gyomuNumb > PV_MAX_PROC ) )
	{
		return( -1 );
	}
	/* プロセス起動設定が未設定の場合 */
	if( ps_g_PVSYSINF->gyomuProc[ gyomuNumb - 11 ] == 0 )
	{
		return( 0 );
	}
	/* プロセス名の設定 */
	switch( gyomuNumb )
	{
		case 11:	/* サーバ送信編集プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS01, 8 );
			break;
		case 12:	/* サーバ送信プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS02, 8 );
			break;
		case 13:	/* 期間差分・合計演算処理プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS03, 8 );
			break;
		case 14:	/* 他発電所向け情報送信プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS04, 8 );
			break;
		case 15:	/* 他発電所情報受信プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS05, 8 );
			break;
		case 16:	/* 業務プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS06, 8 );
			break;
		case 17:	/* 業務プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS07, 8 );
			break;
		case 18:	/* 業務プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS08, 8 );
			break;
		case 19:	/* スケジュール管理プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS09, 8 );
			break;
		case 20:	/* 制御管理プロセスの場合 */
			strncpy( procName, PV_PROCNAME_BUSINESS10, 8 );
			break;
		default:	/* 上記以外の場合 */
			return( -1 );
			break;
	}
	procName[8] = 0;

	/* 業務プロセスのパス設定 */
	sprintf( prgData, "%s%s", PV_PATH_EXE, procName);
	/* 業務プロセスの引数（プロセス番号）設定 */
	sprintf( argData, "%d", gyomuNumb);

	/* 業務プロセスが存在しない場合 */
	if( access( prgData, R_OK ) != 0 )
	{
		/* 新規起動時、且つ、プログラム起動が（20:制御管理プロセス）の場合は異常としない */
		if( ( startMode == PV_PROC_NEWSTART) && ( gyomuNumb == 20 ) )
		{
			sprintf( errMsg, "Child Process Start Skip!! (%s %s)", prgData, argData);
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
			return( 0 );
		}
		sprintf( errMsg, "Child Process Start Error!! (Not-Exist Program) (%s %s)", prgData, argData);
		commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
		/* 致命的障害が発生 */
		return( -1 );
	}

	sprintf( errMsg, "Child Process Start (%s %s)", prgData, argData);
	commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 

	/* プロセス起動フラグをセット */
	retCode = commfunc_manage_execFlagSet( gyomuNumb, startMode);
	if( retCode != 0 )
	{
		/* 致命的障害が発生 */
		return( -1 );
	}

	/* プロセスの名称を格納 */
	commfunc_manage_procNameSet( gyomuNumb, procName );

	/* フォークする */
	pidNumb = fork();

	/* 子プロセス */
	if(pidNumb == 0)
	{
		/* （子プロセスの場合） */

		/* 業務プロセスを起動 */
		retCode = execl( prgData, prgData, argData, NULL );
		/* 起動失敗 */
		if(retCode == -1)
		{
			sprintf( errMsg, "ERROR: Child Process Start Error (%s %s)", prgData, argData);
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
		}
		/* 通信プロセスの起動に失敗した場合は子プロセス（自分自身）を終了する */
		exit( -1 );
	}
	else
	{
		/* フォーク失敗？ */
		if(pidNumb == -1)
		{
			sprintf( errMsg, "ERROR: Fork Error");
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
			return( -1 );
		}
		else
		{
			sprintf( errMsg, "Fork Success (PID:%d)", pidNumb);
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
			/* 子プロセスのＰＩＤを格納 */
			commfunc_manage_pidSet( gyomuNumb, pidNumb );
		}
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 収集装置アプリ停止処理
 *
 * 機能：
 * 		収集装置のアプリケーションを停止する。
 * 		起動中の子プロセス（通信処理プロセス・業務処理プロセス）を全て停止した後
 * 		自プロセスを停止する。
 * 		管理ファイルに登録されている動作中プロセスに対してプロセス停止指示フラグ
 * 		を設定する。（各プロセスは定期的に子のフラグをチェックしているため、チェ
 * 		ックしたプロセスは停止する）
 * 		一定期間プロセスが停止しない場合は、強制的にSIGKILLを送信する。
 */
int	func_process_stop()
{
	int					pidNumb;
	int					procNumb;
	int					retCode;
	int					loopCnt;

	commfunc_syslog_msgset( PV_OWN_NAME, "### ALL PROCESS NORMAL STOP CALL ###" ); 

	/* 全プロセス停止指示を実施 */
	commfunc_manage_allstop(  );

	for( loopCnt = 0 ; loopCnt < 36 ; loopCnt++ )
	{
		/* 5秒処理待ち */
		sleep( 5 );
		/* プロセス停止情報の収集 */
		while( 1 )
		{
			/* 停止プロセスのプロセスＩＤを取得 */
			pidNumb = waitpid(-1, NULL, WNOHANG);
			/* プロセスＩＤを何も取得できなかった場合はループを抜ける */
			if( pidNumb <= 0 )
				break;

			/* プロセスＩＤを取得できた場合は情報を更新 */
			commfunc_manage_stopProc( &procNumb, pidNumb );
		}
		/* プロセスの停止確認 */
		retCode = commfunc_manage_stopCheck( );
		if( retCode == 1 )
		{
			commfunc_syslog_msgset( PV_OWN_NAME, "### ALL PROCESS NORMAL STOP OK ###" ); 
			/* ログ出力 */
			func_syslog_out();
			/* 全プロセス正常停止確認 */
			return( 0 );
		}
		/* ログ出力 */
		func_syslog_out();
	}

	commfunc_syslog_msgset( PV_OWN_NAME, "### ALL PROCESS SIGKILL SEND ###" ); 

	/* 全プロセス強制ＫＩＬＬを実施 */
	commfunc_manage_allkill( );

	/* ＫＩＬＬしたプロセスのプロセスＩＤ回収のため５秒待機 */
	sleep( 5 );

	/* プロセス最大数分繰り返す */
	for( loopCnt = 0 ; loopCnt < PV_MAX_PROC ; loopCnt++ )
	{
		/* 子プロセスの停止プロセスＩＤの回収 */
		pidNumb = waitpid(-1, NULL, WNOHANG);
		if( pidNumb > 0 )
		{
			sprintf( errMsg, "KILL WAITPID RECV:%d", pidNumb );
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg ); 
		}
	}

	commfunc_syslog_msgset( PV_OWN_NAME, "##### ABNORMAL STOP #####" ); 
	/* ログ出力 */
	func_syslog_out();

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 収集装置ログ出力処理
 *
 * 機能：
 * 		収集装置のログを出力する。
 */
int	func_syslog_out( void )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
													/* ログ情報格納用文字列 */
	char					logmsg[PV_MAX_LOG][PV_MSG_SIZE_LOG];
	int						fd;						/* ファイルディスクリプタ */
	int						logcnt;					/* ログ情報数 */
	int						loopCnt;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */

	/* 出力用ログファイル名の設定 */
	sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_SYSTEM_LOG );

	/* ファイルが存在しない場合 */
	if( access( fileName1, F_OK ) != 0 )
	{
		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			return( -1 );
		}
		close( fd );
	}

	/* システムログファイルをオープン */
	if( (fp = fopen( fileName1, "a" )) == NULL )
	{
		return( -1 );
	}

	/* ファイルサイズの取得	*/
	fileSize = ftell( fp );

	/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
	if(fileSize > 1000000 )
	{
		/* ファイルを閉じる */
		fclose( fp );

		/* ログファイルの保存数分繰り返す */
		for(loopCnt = PV_LOG_STOCK_NUM ; loopCnt > 1 ; loopCnt-- )	/* [20230902] */
		{ 
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_SYSTEM_LOG, loopCnt - 1 );
			sprintf( fileName2, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_SYSTEM_LOG, loopCnt );
			/* 古いファイルの削除 */
			if( access( fileName2, R_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			if( access( fileName1, R_OK ) == 0 )
				rename( fileName1, fileName2 );
		}
		/* ファイル名の設定 */
		sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_SYSTEM_LOG );
		sprintf( fileName2, "%s%s_1.log", PV_PATH_LOG, PV_FNAME_SYSTEM_LOG );
		/* 古いファイルの削除 */
		if( access( fileName2, R_OK ) == 0 )
			unlink( fileName2 );
		/* ローテーションの実施 */
		rename( fileName1, fileName2 );

		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			return( -1 );
		}
		close( fd );

		/* システムログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			return( -1 );
		}
	}

	/* システムログメッセージ出力処理 */
	logcnt = commfunc_syslog_msgget( logmsg );
	for(loopCnt = 0; loopCnt < logcnt ; loopCnt++)
		fprintf( fp, "%s\n", logmsg[loopCnt] );

	/* ファイルを閉じる */
	fclose( fp );

	return( 0 );

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログファイル生成処理
 *
 * 機能：
 * 		システムログファイル（PVSYSLOG.dat）を生成する。
 * 		ファイルが存在しない場合は新たに生成するが、ファイルが存在する場合は内容
 * 		の変更は行わない。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_PVSYSLOG_Create()
{
	int					fd;							/* ファイルディスクリプタ */
	char				datFilePath[1024];			/* DATファイルパス */
	int					clrFlag = 0;				/* ファイル０クリアフラグ */
	int					retCode = 0;				/* 戻り値 */
	static struct stat	statInfo;					/* ファイル状態 *//* [20240312] Add */
	int					fileSize;					/* ファイルサイズ *//* [20240312] Add */

	struct st_PVSYSLOG	*ps_PVSYSLOG;				/* ポインタ */

	fileSize = sizeof(struct st_CTLJHFIL);	/* [20240312] Add */

	/* ファイルパスの展開 */
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(datFilePath, PV_FNAME_PVSYSLOG );
	strcat(datFilePath, ".dat");

	/* ファイルが存在しているか確認 */
	if( access( datFilePath, R_OK | W_OK ) != 0 )
	{
		clrFlag = 1;
	}
/* [20240312] Add St */
	else
	{
		/* ファイル状態の取得に失敗 */
		if( stat( datFilePath, &statInfo ) != 0 )
		{
			printf( errMsg, "File[%s] stat error!", datFilePath );
		}
		else
		{
			/* ファイルサイズが前回起動時と異なる場合 */
			if( statInfo.st_size != fileSize )
			{
				clrFlag = 1;
				printf( errMsg, "File[%s] Initialize for fileSize update.", datFilePath );
			}
		}
	}
/* [20240312] Add Ed */


	/* DATファイル生成 */
	fd = open( datFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd == -1 )
	{
		printf("FILEMAKE : OutputFile[%s] Open error!\n", datFilePath );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd, sizeof(struct st_PVSYSLOG) ) == -1 )
	{
		printf("FILEMAKE : OutputFile[%s] ftruncate error!\n", datFilePath );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PVSYSLOG = (struct st_PVSYSLOG *)mmap( NULL, sizeof(struct st_PVSYSLOG), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_PVSYSLOG == MAP_FAILED)
	{
		printf("FILEMAKE : OutputFile[%s] MMAP error!\n", datFilePath );
		retCode = -1;
		return( retCode );
	}

	/* ファイル０クリアフラグがセットされている場合 */
	if( clrFlag == 1 )
	{
		/* ファイルの内容を0クリア */
		memset( ps_PVSYSLOG, 0, sizeof(struct st_PVSYSLOG) );
	}

	/* メモリマップを解除 */
	munmap( ps_PVSYSLOG, sizeof(struct st_PVSYSLOG) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 収集装置状変ログ出力処理
 *
 * 機能：
 * 		収集装置の状変ログを出力する。
 */
int	func_johenlog_out( void )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
													/* ログ情報格納用文字列 */
	char					logmsg[PV_MAX_JOHEN][PV_MSG_SIZE_JHD];
	int						fd;						/* ファイルディスクリプタ */
	int						logcnt;					/* ログ情報数 */
	int						loopCnt;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */

	/* 出力用ログファイル名の設定 */
	sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_JOHEN_LOG );

	/* ファイルが存在しない場合 */
	if( access( fileName1, F_OK ) != 0 )
	{
		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			return( -1 );
		}
		close( fd );
	}

	/* 状変ログファイルをオープン */
	if( (fp = fopen( fileName1, "a" )) == NULL )
	{
		return( -1 );
	}

	/* ファイルサイズの取得	*/
	fileSize = ftell( fp );

	/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
	if(fileSize > 1000000 )
	{
		/* ファイルを閉じる */
		fclose( fp );

		/* ログファイルの保存数分繰り返す */
		for(loopCnt = 3 ; loopCnt > 1 ; loopCnt-- )
		{ 
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_JOHEN_LOG, loopCnt - 1 );
			sprintf( fileName2, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_JOHEN_LOG, loopCnt );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			if( access( fileName1, F_OK ) == 0 )
				rename( fileName1, fileName2 );
		}
		/* ファイル名の設定 */
		sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_JOHEN_LOG );
		sprintf( fileName2, "%s%s_1.log", PV_PATH_LOG, PV_FNAME_JOHEN_LOG );
		/* 古いファイルの削除 */
		if( access( fileName2, F_OK ) == 0 )
			unlink( fileName2 );
		/* ローテーションの実施 */
		rename( fileName1, fileName2 );

		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			return( -1 );
		}
		close( fd );

		/* 状変ログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			return( -1 );
		}
	}

	/* 状変ログメッセージ出力処理 */
	logcnt = commfunc_johen_msggetlog( logmsg );
	for(loopCnt = 0; loopCnt < logcnt ; loopCnt++)
		fprintf( fp, "%s\n", logmsg[loopCnt] );

	/* ファイルを閉じる */
	fclose( fp );

	return( 0 );

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ強制終了シグナル送信
 *
 * 機能：
 * 		オンラインデバッガが動作している場合は、SIGUSR1 シグナルを送信する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_SIGUSR1_send()
{
	FILE					*fp;					/* ファイルポインタ */
	char					checkPid[512];			/* プロセスID（文字列） */
	int						chkFlg = 0;				/* チェックフラグ */
	int						retCode;
	pid_t					pidNumb;				/* プロセスID */
	

	/* 実行中のオンラインデバッガのPIDをパイプから取得 */
	if( (fp = popen( "/usr/bin/pgrep sysdebug", "r" )) == NULL )
	{
		/* 確認コマンド実行失敗 */
		printf("%s : SYSDEBUG CHECK FILE OPEN Error!\n", PV_OWN_NAME);
		return( -1 );
	}

	/* プロセスＩＤがなくなるまで繰り返す */
	while( fgets( checkPid, 512, fp ) != NULL )
	{
		/* プロセスＩＤの取得 */
		pidNumb = (pid_t)atoi( checkPid );

		/* オンラインデバッガに SIGUSR1 を送信 */
		retCode = kill( pidNumb, SIGUSR1 );

		chkFlg = 1;
	}
	/* パイプを閉じる */
	pclose( fp );

	if( chkFlg )
	{
		/* 10秒待機する */
		sleep( 10 );
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * シャットダウン処理
 *
 * 機能：
 * 		引数 sMode が PV_M_MODE_SHUTDOWN の場合、シャットダウン処理を行う。
 *		PV_M_MODE_REBOOT の場合は、リブート処理を行う。
 *		シャットダウンまたはリブートの実施は、引数 sTime で指定された秒数が経過
 *		した後実施する。
 *		引数 sFlag に PV_M_RECOVERY_SET が指定されている場合は、リカバリーフラ
 *		グの生成を行う。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 * 入力：
 *		sMode
 *			シャットダウンモード
 *			1(PV_M_MODE_SHUTDONW)：シャットダウン
 *			2(PV_M_MODE_REBOOT)  ：リブート
 *		sTime
 *			シャットダウン／リブート実施待ち時間（秒）
 *		sFlag
 *			リカバリーフラグ設定指定
 *			1(PV_M_RECOVERY_SET) ：リカバリーフラグ設定
 *			2(PV_M_RECOVERY_NONE)：リカバリーフラグ未設定
 */
int	func_shutdown_exe( int sMode, int sTime , int sFlag )
{
	char					checkCmd[128];

	/* シャットダウン実施待ち時間が指定されている場合は待機する */
	if( sTime > 0 )
	{
		if( sMode == PV_M_MODE_SHUTDOWN )
			printf("##### System Shutdown Start After %dsec ...\n", sTime );
		else
			printf("##### System Reboot Start After %dsec ...\n", sTime );
		printf("##### %s ( pid : %d )\n", PV_OWN_NAME, getpid() );
		sleep( sTime );
	}

	/* リカバリフラグ設定が指定されている場合 */
	if( sFlag == PV_M_RECOVERY_SET )
	{
		/* リカバリフラグを設定する */
		sprintf( checkCmd, "/usr/bin/touch %s%s", PV_PATH_DAT, PV_FNAME_PV_RECOVERY );
		system( checkCmd );
	}

	/* シャットダウン／リブート処理を実施 */
	if( sMode == PV_M_MODE_SHUTDOWN )
	{
		printf("##### System Shutdown Start Now !!\n");
		system("cd /; shutdown 0 &");
	}
	else
	{
		printf("##### System Reboot Start Now !!\n");
		system("cd /; reboot &");
	}

	/* 再起動が完了するまでの親プロセス（自プロセス）を生かす（3秒程度） */
	sleep( 3 );

	/* プログラムを終了する */
	exit( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プログラムシャットダウン処理
 *
 * 機能：
 *		アプリケーションの停止処理を実施した後、シャットダウン／リブート処理を
 *		を行う。
 * 		引数 sMode が PV_M_MODE_SHUTDOWN の場合、シャットダウン処理を行う。
 *		PV_M_MODE_REBOOT の場合は、リブート処理を行う。
 *		シャットダウンまたはリブートの実施は、引数 sTime で指定された秒数が経過
 *		した後実施する。
 *		引数 sFlag に PV_M_RECOVERY_SET が指定されている場合は、リカバリーフラ
 *		グの生成を行う。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 * 入力：
 *		sMode
 *			シャットダウンモード
 *			1(PV_M_MODE_SHUTDONW)：シャットダウン
 *			2(PV_M_MODE_REBOOT)  ：リブート
 *		sTime
 *			シャットダウン／リブート実施待ち時間（秒）
 *		sFlag
 *			リカバリーフラグ設定指定
 *			1(PV_M_RECOVERY_SET) ：リカバリーフラグ設定
 *			2(PV_M_RECOVERY_NONE)：リカバリーフラグ未設定
 *		sPer
 *			他系シャットダウン有無
 *			1(PV_M_PERSHUTDOWN_SET) ：他系シャットダウンする
 *			2(PV_M_PERSHUTDOWN_NONE)：他系シャットダウンしない
 */
int	func_proc_shutdown_exe( int sMode, int sTime , int sFlag, int sPer)
{
	char					checkCmd[128];

	/* モードがシャットダウンの場合 */
	if( sMode == PV_M_MODE_SHUTDOWN )
	{
		/* 装置状態状変：装置運転（停止） */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 0, 0, 0,
			ps_g_SVSTSFIL, ps_g_SVMSKINF );

		/* 他系シャットダウン設定、且つ、embaselibを使用している場合 */
		if( ( PV_M_PERSHUTDOWN_SET == sPer ) &&
			( 1 == ps_g_PVSYSINF->embaseUseFlg ) )
		{
			/* システムログに出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, "##### embase_func_msgq_send( EMBASE_MSG_OS_PAR_SHUTDOWN ) #####" ); 

			/* メッセージ送信 */
			embase_func_msgq_send( EMBASE_MSG_OS_PAR_SHUTDOWN );
		}
	}
	else
	{
		/* サイレントモードフラグが設定されている場合 */
		if( silentrb_flg == 0 )
		{
			/* 装置状態状変：装置運転（停止） */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 0, 0, 0,
				ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
		else
		{
			/* 装置状態状変：装置運転（停止）状変マスク */
			commfunc_johen_pos_mask( PV_MKIND_PVSYS, 1, PV_SYS_SV_SYSRUN, 0, 0, 0,
				ps_g_SVSTSFIL, ps_g_SVMSKINF );
			commfunc_syslog_msgset( PV_OWN_NAME, "#####     (SILENT MODE)   #####" ); 
		}
	}

	/* 全ての子プロセス停止を実施 */
	func_process_stop();

	/* オンラインデバッガ強制停止を実施 */
	func_SIGUSR1_send();

	if( 1 == ps_g_PVSYSINF->embaseUseFlg )
	{
		commfunc_syslog_msgset( PV_OWN_NAME, "call embase_func_finish" ); 
		/* embase_func_startup() で確保したリソースを開放 */
		embase_func_finish();
	}

	commfunc_syslog_msgset( PV_OWN_NAME, "##### STOP #####" ); 
	/* システムログの強制出力 */
	func_syslog_out();

	/* 使用ファイルをクローズ */
	commfunc_file_close( s_fileIndex );

	/* ミューテックを破棄 */
	commfunc_mutex_destroy();

	/* シャットダウン／リブート処理の実施 */
	func_shutdown_exe( sMode, sTime, sFlag );

	/* プログラムを終了する */
	exit( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モバイルルータ状態確認処理
 *
 * 機能：
 *      ルータの再起動可否状態をスクリプトを用いて取得する
 * 戻り値：
 *      なし
 */
void func_router_reset( )
{
	char			rresetCommand[512];				/* モバイルルータ用コマンド */
	int				retCode;						/* 戻り値 */

	/* モバイルルータ状態確認用コマンドの設定 */
	sprintf( rresetCommand, "%s%s", PV_PATH_EXE, PV_FNAME_RRESET_SCRIPT );

	/* コマンドが存在するか確認 */
	if( access( rresetCommand, R_OK ) != 0 )
	{
		/* リセット用ファイルが存在しない場合 */
		return;
	}

	/* コマンドを実行 */
	retCode = system( rresetCommand );

	/* 戻り値を確認 */
	if( WIFEXITED(retCode) )
		retCode = WEXITSTATUS(retCode);
	else if ( WIFSIGNALED(retCode) )
	{
		retCode = WTERMSIG(retCode);
		sprintf( errMsg, "MOBILE ROUTER RESET COMMAND ABORT!! SIGNAL-CODE[%d]", retCode );
		commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
	}

	switch( retCode )
	{
		case 0:
			/* ルータの再起動成功の場合	*/
			/* ログを出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, "MOBILE ROUTER RESET!!" );
			break;
		case 1:
			/* ルータは動作中、ただし、ただし再起動対象（Ｈ１００やＲＸシリーズ）ではない場合 */
			/* ログを出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, "Router is ALIVE (Router Type is not H100/RX)" );
			break;
		case 2:
			/* 収集装置にルータ（ゲートウェイ）の定義が存在しない場合 */
			/* ログを出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, "Router is Undefined." );
			break;
		case 3:
			/* ルータは動作中、再起動対象の機種だが再起動条件（ソフト等）がインストールされていない場合 */
			/* ログを出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, "Router Restart Disabled. (telnet,expect is not installed)" );
			break;
		case 20:
			/* ルータがダウンしているか、ネットワークの問題によりルータにアクセスできない場合 */
			/* ログを出力 */
			commfunc_syslog_msgset( PV_OWN_NAME, "Router is DOWN or Unreachable to Router !" );
			break;
		default:
			/* 上記以外 */
			/* ログを出力 */
			sprintf( errMsg, "Router Reset Error! RET-CODE[%d]", retCode );
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
		break;
	}
}

