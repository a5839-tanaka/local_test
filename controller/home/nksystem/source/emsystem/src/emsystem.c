/*
   TAB STOP SIZE = 4
*/
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * [20250723] 2025.07.23
 * ・系列間同期用のネットワークLinkDown時にDRBDサービスが起動しない問題対応の水平展開。
 * [20250717] 2025.07.17
 * ・運用モード遷移時に時刻チェックを実施するように修正。
 * [20250526] 2025.05.26
 * ・運用モード以外に遷移後、時刻同期サービスの動作チェックを行い、停止中であれば起動する。
 * [20200714] 2020.07.14
 * ・準備/待機から運用に遷移中にサービスを停止(systemctl stop embase.service)すると、
 *   primaryのまま停止してしまう問題の対応
 * [20200713] 2020.07.13
 * ・ネットワーク接続状態を取得するコマンド「/bin/nmcli device」で、電源ON時にLANが未接続の場合、
 *   誤って接続と認識してしまう問題の対応
 * ・他系が運用[S-IF:異常,I-IF:正常]時に、自系を起動[S-IF:正常,I-IF:正常]すると、
 *   自系が準備となり、運用にならない問題の対応
 * [20200710] 2020.07.10
 * ・スプリットブレイン後のDRDB同期しない問題の対応
 * [20200708] 2020.07.08
 * ・モード遷移チェックテーブルのNo.3,10の条件を変更
 * ・モード遷移チェックテーブルのNo.5の前に条件を追加
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <mqueue.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "embase.h"
#include "emsystem.h"

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
#if 0
#define DEBUG_1
#define DEBUG_NOPAGE
#define DEBUG_ERR
#endif
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/

#define EMBASE_OWN_NAME			"EMSYSTEM"						/* 自プログラム名 */

struct st_dspStsName {
	char		nameStr[5][256];								/* 状態名称 */
};

struct st_sepStr {												/* 文字列データ分割用構造体 */
	char		data[16][128];									/*  128文字 16個分 */
};
    
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
/* UDP関連 */
struct st_udpAccessInfo {
	int					openStatus;								/* オープンステータス */
	int					sock_fd;								/* ファイルディスクリプタ */
	in_addr_t			netDev_IPaddr;							/* ネットワークI/FのIPアドレス */
	int					portNumb;								/* ポート番号 */
};



/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
/* プロセス内グローバル */
struct st_EMSYSTEM		*ps_EMSYSTEM;							/* ２系列状態管理 */

static char				sysHostName[3][64] = {					/* ホスト名称 */
#if 0
	EMBASE_HOSTNAME_C,											/* 共通ホスト名称（デフォルト値） */
#else
	"",															/* 共通ホスト名称（デフォルト値） */
#endif
	EMBASE_HOSTNAME_1,											/* １系ホスト名称（デフォルト値） */
	EMBASE_HOSTNAME_2											/* ２系ホスト名称（デフォルト値） */
};
static char				subHostName[3][64] = {					/* ホスト名称 */
	"undefine",													/* 共通ホスト名称（デフォルト値） */
	EMBASE_HOSTNAME_1S,											/* １系系列間ホスト名称（デフォルト値） */
	EMBASE_HOSTNAME_2S											/* ２系系列間ホスト名称（デフォルト値） */
};
static char				netDeviceName[2][64] = {				/* ネットワークＩ／Ｆデバイス名称 */
	EMBASE_NETDEV_NAME_S,										/* サービス用ネットワークＩ／Ｆ */
	EMBASE_NETDEV_NAME_I										/* 系列間用ネットワークＩ／Ｆ */
};
																/* ＤＲＢＤ等価リソース名 */
static char				drbdResourceName[64] = EMBASE_DRBD_RESOURCE;
static char				drbdDeviceName[64] = EMBASE_DRBD_DEVICE;/* ＤＲＢＤデバイス名 */
static char				appServiceName[64] = "";				/* 運用モード時の業務アプリサービス名 */
static char				appRuncheckName[2][64] = { "", "" };	/* 運用モード時の動作チェックプロセス */
static char				appTimeSyncName[64] = "";				/* 待機モード時の時刻同期アプリサービス名 *//* [20250526] Add */

static pthread_t		threadIDrcv[2];							/* スレッドＩＤ */
static int				otherTarget = 0;						/* 処理対象 */
static int				newSysMode;								/* 遷移先モード */
static int				parSysMode;								/* 他系装置モード（旧） */
static int				modTrnSeqNumb;							/* モード遷移処理シーケンス番号 */
static int				drbdRecoveryCnt = 0;					/* リカバリ処理待ち */
static int				drbdCheckCnt = 0;						/* ＤＲＢＤチェック用カウンタ */
static int				netDevSts[4];							/* ネットワークI/F状態（旧） */
static int				lifChkSts[2];							/* 死活監視状態 */
static int				threadStop = 0;							/* スレッド停止フラグ */
static int				ctlKindSend = 0;						/* 制御依頼種別（送信用） */
static int				ctlKindRecv = 0;						/* 制御依頼種別（受信用） */
static int				opeTranStopFlg = 0;						/* 運用遷移中停止フラグ *//* [20200714] Add */
static int				timeSyncCheckFlg = 0;					/* 時刻同期チェックフラグ（0:チェックしない、1:チェックする） *//* [20250717] Add */

static char				logMsg[EMBASE_LOGMSG_SIZE];				/* ログメッセージ */
static int				logLevel = EMSYS_LOG_LV3;				/* ログレベル */

static int				drbdCheckTime = EMBASE_DRBD_CHECK_TIME;	/* ＤＲＢＤサービス動作確認周期 */
static unsigned short	udpSendPort = EMBASE_LIFCHK_SND_PORT;	/* 死活監視情報UDP送信ポート番号 */
static unsigned short	udpRecvPort = EMBASE_LIFCHK_RCV_PORT;	/* 死活監視情報UDP受信ポート番号 */
static int				udpSendTime = EMBASE_LIFCHK_SND_TIME;	/* 死活監視情報UDP送信周期（秒） */
static int				udpRecvTime = EMBASE_LIFCHK_RCV_TIME;	/* 死活監視情報UDP受信タイムアウト（秒） */
static in_addr_t		udpSendIPaddr[2];						/* 死活監視情報送信先IPアドレス */
static in_addr_t		udpRecvIPaddr[2];						/* 死活監視情報受信IPアドレス */
static in_addr_t		commonIPaddr = 0;						/* 共通ホストIPアドレス */
static int				commonNetMask = 0;						/* 共通ホストネットマスク */

pthread_mutex_t			mtx_t = PTHREAD_MUTEX_INITIALIZER;		/* ミューテックス */

struct st_modTrnChkTbl	s_modTrnChkTbl[] = {					/* モード遷移チェックテーブル */
/*     +--------------------------------------------------------------------------------------- 番号                                    */
/*     |     +--------------------------------------------------------------------------------- 自系の現在の装置モード                  */
/*     |     |                     +----------------------------------------------------------- 他系の現在の装置モード                  */
/*     |     |                     |                      +------------------------------------ 自系ネットワークI/Fの状態（サービス用） */
/*     |     |                     |                      |   +-------------------------------- 自系ネットワークI/Fの状態（系列間用）   */
/*     |     |                     |                      |   |   +---------------------------- 死活監視状態（サービス用ネットワーク）  */
/*     |     |                     |                      |   |   |   +------------------------ 死活監視状態（系列間用ネットワーク）    */
/*     |     |                     |                      |   |   |   |   +-------------------- 他系ネットワークI/Fの状態（サービス用） */
/*     |     |                     |                      |   |   |   |   |   +---------------- 他系ネットワークI/Fの状態（系列間用）   */
/*     |     |                     |                      |   |   |   |   |   |   +------------ 運用モード最終稼働時間条件              */
/*     |     |                     |                      |   |   |   |   |   |   |   +-------- 運用モード正常稼働時間条件              */
/*     |     |                     |                      |   |   |   |   |   |   |   |     +-- 遷移先装置モード                        */
/*     |     |                     |                      |   |   |   |   |   |   |   |     |                                           */
	{  1, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_UNKNOW,   0, -1, -1, -1, -1, -1, -1, -1 }, EMBASE_SYSMOD_OPERAT },	/* 片系単独起動 */
	{  2, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_WAIT,     0, -1,  0,  1,  0, -1,  0, -1 }, EMBASE_SYSMOD_OPERAT },	/* 運用モード最終稼働時刻判断 */
/* [20200708] Chg St */
//	{  3, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_WAIT,     0,  0,  0,  0,  0,  0,  0, -1 }, EMBASE_SYSMOD_OPERAT },	/* 運用モード最終稼働時刻判断 */
	{  3, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_WAIT,     0,  0, -1,  0, -1,  0,  0, -1 }, EMBASE_SYSMOD_OPERAT },	/* 運用モード最終稼働時刻判断 */
/* [20200708] Chg Ed */
	{  4, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_WAIT,     0,  0,  0,  0,  0,  0,  1, -1 }, EMBASE_SYSMOD_STANDB },	/* 運用モード最終稼働時刻判断 */
/* [20200708] Add St */
	{  5, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_WAIT,     0,  0, -1,  0,  1,  0, -1, -1 }, EMBASE_SYSMOD_OPERAT },	/* 運用モード最終稼働時刻判断 */
/* [20200708] Add Ed */
	{  6, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_STOP,     0, -1, -1, -1, -1, -1, -1, -1 }, EMBASE_SYSMOD_OPERAT },	/* 優先遷移 */
	{  7, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_STANDB,   0,  0,  0,  0,  0,  0, -1, -1 }, EMBASE_SYSMOD_OPERAT },	/* 優先遷移（他系で遷移4が先に実行された場合） */
/* [20200710] Chg St */
//	{  8, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_OPERAT,   0,  0,  0,  0,  0,  0, -1, -1 }, EMBASE_SYSMOD_STANDB },	/* 通常遷移 */
/* [20200713] Chg St */
//	{  8, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_OPERAT,  -1,  0, -1,  0,  0,  0, -1, -1 }, EMBASE_SYSMOD_STANDB },	/* 通常遷移 */
	{  8, { EMBASE_SYSMOD_WAIT,   EMBASE_SYSMOD_OPERAT,  -1,  0, -1,  0, -1,  0, -1, -1 }, EMBASE_SYSMOD_STANDB },	/* 通常遷移 */
/* [20200713] Chg St */
/* [20200710] Chg Ed */
	{  9, { EMBASE_SYSMOD_STANDB, EMBASE_SYSMOD_UNKNOW,   0, -1,  1,  1, -1, -1, -1, -1 }, EMBASE_SYSMOD_OPERAT },	/* 待機、運用->不明 */
	{ 10, { EMBASE_SYSMOD_STANDB, EMBASE_SYSMOD_STANDB,   0,  0, -1,  0, -1,  0,  1, -1 }, EMBASE_SYSMOD_OPERAT },	/* スプリットブレイン：運用モード最終稼働時刻判断 */
/* [20200708] Chg St */
//	{ 10, { EMBASE_SYSMOD_STANDB, EMBASE_SYSMOD_OPERAT,  -1, -1, -1,  0, -1,  1, -1, -1 }, EMBASE_SYSMOD_WAIT   },	/* ネットワーク異常による待機->準備 */
	{ 11, { EMBASE_SYSMOD_STANDB, EMBASE_SYSMOD_OPERAT,  -1, -1, -1,  1, -1,  1, -1, -1 }, EMBASE_SYSMOD_WAIT   },	/* ネットワーク異常による待機->準備 */
/* [20200708] Chg Ed */
	{ 12, { EMBASE_SYSMOD_OPERAT, EMBASE_SYSMOD_STANDB,   1,  0,  1,  0,  0,  0, -1, -1 }, EMBASE_SYSMOD_STANDB },	/* サービス用I/F異常による運用・待機反転 */
	{ 13, { EMBASE_SYSMOD_OPERAT, EMBASE_SYSMOD_OPERAT,   1,  0,  1,  0,  0,  0, -1, -1 }, EMBASE_SYSMOD_STANDB },	/* スプリットブレイン：サービス用I/F状態判断 */
	{ 14, { EMBASE_SYSMOD_OPERAT, EMBASE_SYSMOD_OPERAT,   0, -1,  0, -1,  0, -1, -1,  1 }, EMBASE_SYSMOD_STANDB },	/* スプリットブレイン：運用モード稼働時間判断 */
	{ -1, { -1,                   -1,                    -1, -1, -1, -1, -1, -1, -1, -1 }, -1                   }	/* ストッパ         */
};
/* 備考 */
/* 上記のテーブルによるモード遷移以外は、DRBDの同期状態によるモード遷移と、メッセージによる外部からの指示がある */

/* 内部関数の定義 */
int   func_init();												/* 初期化処理 */
int   func_chk_mode_tran();										/* モード遷移判定処理 */
void  func_chk_stat_change();									/* モード遷移判定要因検出処理 */
void  func_msg_receive();
void  func_mode_trans_seq();									/* モード遷移シーケンス処理 */
void  func_set_mode_tran( int );								/* モード遷移先設定処理 */
void* func_lifchk_recv_thread( void * );						/* 死活監視情報受信処理（スレッド） */
void* func_lifchk_send_thread();								/* 死活監視情報送信処理（スレッド） */
int   func_chk_netDev();										/* ネットワークI/F状態確認処理 */
int   func_chk_drbd();											/* ＤＲＢＤ状態確認処理処理 */
int   func_chk_oper_proc();										/* 指定プロセス動作監視処理 */
int   func_chk_stop_proc();										/* 指定プロセス停止監視処理 */
void  func_drbd_recovery();										/* ＤＲＢＤ状態復旧処理処理 */
int   func_get_pid( char * );									/* プロセスPID取得処理 */
int   func_get_netmask( char * );								/* ネットマスク値取得 */
int   func_sep_data( char *, char, int, struct st_sepStr * );	/* データセパレート処理 */
int   func_log_msgset( int, char *, char *, int );				/* ログ記録処理 */
int   func_get_config();										/* 設定ファイル読込処理 */
int   func_get_ipaddr( char *, in_addr_t * );					/* IPアドレス取得 */
int   func_chk_time_sync();										/* 時刻同期サービスの状態取得と起動 *//* [20250526] Add */
int   func_timeSync_Check( );									/* 時刻同期チェック処理 *//* [20250717] Add */

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
/* メイン処理 */
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
int main()
{
	int					retCode = 0;							/* 戻り値 */
	int					embaseStop = 0;							/* 停止フラグ */
	int					i_lloopCnt1;							/* ループカウンタ */
	int					i_lnetDevServ = EMBASE_NETDEV_SERVICE;	/* サービス用ネットワークＩ／Ｆ*/
	int					i_lnetDevIntr = EMBASE_NETDEV_INTERNAL;	/* 系列間用ネットワークＩ／Ｆ*/
	char				checkFileStop[1024];					/* 停止指示ファイル */
	static pthread_t	threadIDrcv[2];							/* スレッドＩＤ */
	static pthread_t	threadIDsnd;							/* スレッドＩＤ */
	time_t				nowTime;								/* 積算秒 */
	time_t				oldTime;								/* 積算秒 */

	printf("==============================================\n");
	printf(" 装置監視プロセス( %s ) START\n", EMBASE_OWN_NAME );
	printf("==============================================\n" );

	/* ログを出力 */
	sprintf(logMsg, "%s: main(): 装置監視プロセス START  ", EMBASE_OWN_NAME );
	func_log_msgset( EMSYS_LOG_LV3, "==========================================", NULL, 0 );
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	func_log_msgset( EMSYS_LOG_LV3, "==========================================", NULL, 0 );

	/* 停止指示ファイルの設定 */
	sprintf( checkFileStop, "%s%s", EMBASE_FPATH_SONLINE, EMBASE_FNAME_STOP_FLG );

	/* 動作準備 */
	if( (retCode = embase_func_startup_adm((void **)&ps_EMSYSTEM)) != 0 )
	{
#ifdef DEBUG_ERR
printf("%s:main(): 動作準備エラー embase_func_startup_adm() 異常終了 RET:%d\n", EMBASE_OWN_NAME, retCode );
#endif

		/* ログを出力 */
		sprintf(logMsg, "main(): 動作準備エラー embase_func_startup_adm() RET:%d異常終了", retCode );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		printf("==============================================\n");
		printf(" 装置監視プロセス( %s ) ERROR STOP\n", EMBASE_OWN_NAME );
		printf("==============================================\n" );

		/* 終了 */
		exit( EXIT_FAILURE );
	}

	/* 初期設定 */
	if( 0 != func_init() )
	{
#ifdef DEBUG_ERR
printf("%s:main(): 初期化エラー\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 初期化エラー");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		printf("==============================================\n");
		printf(" 装置監視プロセス( %s ) ERROR STOP\n", EMBASE_OWN_NAME );
		printf("==============================================\n" );

		/* 終了 */
		exit( EXIT_FAILURE );
	}

	/* 死活監視情報受信スレッド起動（サービス用ネットワークＩ／Ｆ経由受信） */
	if( pthread_create( &threadIDrcv[ 0 ], NULL, func_lifchk_recv_thread, (void*)&i_lnetDevServ ) != 0 )
	{
		/* エラー */
#ifdef DEBUG_ERR
printf("%s:main(): 死活監視情報受信スレッド（サービス用ネットワークＩ／Ｆ経由受信）起動エラー\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 死活監視情報受信スレッド（サービス用ネットワークＩ／Ｆ経由受信）起動エラー");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		printf("==============================================\n");
		printf(" 装置監視プロセス( %s ) ERROR STOP\n", EMBASE_OWN_NAME );
		printf("==============================================\n" );

		/* 終了 */
		exit( EXIT_FAILURE );
	}
	else
	{
#ifdef DEBUG_1
printf("%s:main(): スレッド起動[ 0 ] Success!\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 死活監視情報受信スレッド起動（サービス用ネットワークＩ／Ｆ経由受信）Success!" );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	}

	/* 死活監視情報受信スレッド起動（系列間用ネットワークＩ／Ｆ経由受信） */
	if( pthread_create( &threadIDrcv[ 1 ], NULL, func_lifchk_recv_thread, (void *)&i_lnetDevIntr ) != 0 )
	{
		/* エラー */
#ifdef DEBUG_ERR
printf("%s:main(): 死活監視情報受信スレッド（系列間用ネットワークＩ／Ｆ経由受信）起動エラー\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 死活監視情報受信スレッド（系列間用ネットワークＩ／Ｆ経由受信）起動エラー");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		printf("==============================================\n");
		printf(" 装置監視プロセス( %s ) ERROR STOP\n", EMBASE_OWN_NAME );
		printf("==============================================\n" );

		/* 終了 */
		exit( EXIT_FAILURE );
	}
	else
	{
#ifdef DEBUG_1
printf("%s:main(): スレッド起動[ 1 ] Success!\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 死活監視情報受信スレッド起動（系列間用ネットワークＩ／Ｆ経由受信）Success!");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	}

	sleep(2);

	/* 死活監視情報送信スレッド起動） */
	if( pthread_create( &threadIDsnd, NULL, func_lifchk_send_thread, NULL ) != 0 )
	{
		/* エラー */
#ifdef DEBUG_ERR
printf("%s:main(): 死活監視情報送信スレッド起動エラー\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 死活監視情報送信スレッド起動エラー");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		printf("==============================================\n");
		printf(" 装置監視プロセス( %s ) ERROR STOP\n", EMBASE_OWN_NAME );
		printf("==============================================\n" );

		/* 終了 */
		exit( EXIT_FAILURE );
	}
	else
	{
#ifdef DEBUG_1
printf("%s:main(): 死活監視情報送信スレッド起動 Success!\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "main(): 死活監視情報送信スレッド起動 Success!");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

	}

	/* ================ */
	/* メイン処理ループ */
	/* ================ */

	/* ネットワークI/F状態確認 */
	func_chk_netDev();

	/* ＤＲＢＤ状態確認 */
	func_chk_drbd();

	/* 積算秒を記録 */
	oldTime = time( NULL );

	/* モード変更確認時間設定（現在時刻＋30秒後） */
	ps_EMSYSTEM->modeTranTime = oldTime + 30;

	while( 1 )
	{
		/* ＤＲＢＤリカバリカウンタ処理 */
		drbdRecoveryCnt--;
		if( drbdRecoveryCnt < 0 ) drbdRecoveryCnt = 0;

		/* ネットワークI/F状態確認処理 */
		func_chk_netDev();

		/* ＤＲＢＤ状態確認処理 */
		func_chk_drbd();

/* [20250526] Add St */
		/* 時刻同期起動処理 */
		func_chk_time_sync();
/* [20250526] Add Ed */

		/* 死活監視状態が２ルート共に異常の場合 */
		if( ( ps_EMSYSTEM->lifChkSts[0] == EMBASE_LIFCHK_NG ) &&
		    ( ps_EMSYSTEM->lifChkSts[1] == EMBASE_LIFCHK_NG ) )
		{
			/* 運用モード稼働最終時間（他系）をリセット */
			ps_EMSYSTEM->operateLastTime[1] = 0;
			/* 運用モード稼働時間（他系）をリセット */
			ps_EMSYSTEM->operateTotalTime[1] = 0;

			/* 他系の装置モードを「不明」に設定 */
			ps_EMSYSTEM->s_kei[ otherTarget ].sysMode = EMBASE_SYSMOD_UNKNOW;

			/* 他系のネットワークI/Fが[正常]の場合は[不明]に変更 */
			if( ps_EMSYSTEM->s_kei[ otherTarget ].netDevSts[0] == EMBASE_NETDEV_OK )
			{
				ps_EMSYSTEM->s_kei[ otherTarget ].netDevSts[0] = EMBASE_NETDEV_UN;
#ifdef DEBUG_1
printf("%s:main(): 死活監視両ルート異常により他系ネットワークI/F状態(0)を[不明]に変更\n", EMBASE_OWN_NAME );
#endif
				/* ログを出力 */
				sprintf(logMsg, "main(): 死活監視両ルート異常により他系ネットワークI/F状態(0)を[不明]に変更");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			if( ps_EMSYSTEM->s_kei[ otherTarget ].netDevSts[1] == EMBASE_NETDEV_OK )
			{
				ps_EMSYSTEM->s_kei[ otherTarget ].netDevSts[1] = EMBASE_NETDEV_UN;
#ifdef DEBUG_1
printf("%s:main(): 死活監視両ルート異常により他系ネットワークI/F状態(1)を[不明]に変更\n", EMBASE_OWN_NAME );
#endif
				/* ログを出力 */
				sprintf(logMsg, "main(): 死活監視両ルート異常により他系ネットワークI/F状態(1)を[不明]に変更");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}

		/* 管理プロセス停止指示がある場合 */
		if( access( checkFileStop, R_OK ) == 0 )
		{
#ifdef DEBUG_1
printf("%s:main(): 停止指示ファイルを検出したため、停止処理を開始\n", EMBASE_OWN_NAME );
#endif
			/* 管理プロセス停止指示がある場合 */
			if( access( checkFileStop, R_OK ) == 0 )
			{
				/* ログを出力 */
				sprintf(logMsg, "main(): 装置監視プロセス 停止指示ファイル削除" );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 停止指示ファイルを削除 */
				unlink( checkFileStop );
			}

			sprintf(logMsg, "main(): 装置監視プロセス 停止指示ファイル検出" );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

/* [20200714] Chg St */
//			/* 運用系の場合は停止処理を行う */
//			if( EMBASE_SYSMOD_OPERAT == ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode )

			/* 運用系遷移中の場合は運用遷移中停止フラグをON */
			if( EMBASE_SYSMOD_OPERAT == ps_EMSYSTEM->newSysMode)
			{
				opeTranStopFlg = 1;
			}
			
			/* 運用系または運用系に遷移中の場合は停止処理を行う */
			if( (EMBASE_SYSMOD_OPERAT == ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode ) ||
			    (EMBASE_SYSMOD_OPERAT == ps_EMSYSTEM->newSysMode) )
/* [20200714] Chg Ed */
			{
				/* モード遷移処理シーケンスを設定（運用->待機） */
				func_set_mode_tran( EMSYS_SEQ_OPE2STB );
			}
			/* 停止フラグＯＮ */
			embaseStop = 1;
		}

		/* 停止フラグ設定あり */ 
		if( embaseStop )
		{
/* [20200714] Chg St */
//			/* 装置モードが「運用」以外の場合 */
//			if( EMBASE_SYSMOD_OPERAT != ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode )
			/* 装置モードが「運用」以外、かつ、運用遷移中停止フラグがOFF の場合 */
			if( (EMBASE_SYSMOD_OPERAT != ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode ) &&
			    (0 == opeTranStopFlg) )
/* [20200714] Chg Ed */
			{
				/* 処理を抜ける */
				break;
			}
		}

		/* 装置状態が運用以外の場合 */
		/* ネットワークＩ／Ｆ（サービス側）が正常以外の場合 */
		if( ( EMBASE_SYSMOD_OPERAT != ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode ) ||
		    ( EMBASE_NETDEV_OK != ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].netDevSts[EMBASE_NETDEV_SERVICE] ) )
		{
			/* 正常運用時間を 0 にリセット */
			ps_EMSYSTEM->operateTotalTime[0] = 0;
		}
		else
		{
			/* 現在の積算秒を取得 */
			nowTime = time( NULL );
			/* 前回からの差分時刻を運用時間に加算 */
			ps_EMSYSTEM->operateTotalTime[0] += (nowTime - oldTime);
		}
		/* 積算秒を記録 */
		oldTime = time( NULL );

		/* 装置モードが「運用」の場合 */
		if( ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode == EMBASE_SYSMOD_OPERAT ) 
		{
			/* 運用モード最終稼働時刻をセット */
			ps_EMSYSTEM->operateLastTime[0] = oldTime;

			/* 運用モード遷移中以外、かつ */
			/* 動作監視対象プロセスが動作しているか確認 */
			if( (newSysMode == -1) && (0 != func_chk_oper_proc()) )
			{
				/* モード遷移処理シーケンスを設定（運用->待機遷移） */
				func_set_mode_tran( EMSYS_SEQ_OPE2STB );

				/* ログを出力 */
				sprintf(logMsg, "main(): 装置モード「運用」中に監視対象プロセス未動作検出により「待機」へ遷移" );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			/* ＤＲＢＤチェック */
			func_drbd_recovery();
		}
		/* 装置モードが「待機」の場合 */
		else if( ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode == EMBASE_SYSMOD_STANDB ) 
		{
			/* 運用モードへの遷移中は除外 */
			if( newSysMode != EMBASE_SYSMOD_OPERAT )
			{
				/* ＤＲＢＤ接続状態が EMBASE_DRBD_CSTATE_CONNECTED 以外の場合 */
				if( EMBASE_DRBD_CSTATE_CONNECTED != ps_EMSYSTEM->drbdCstate )
				{
					/* モード遷移処理シーケンスを設定（待機->準備） */
					func_set_mode_tran( EMSYS_SEQ_STB2WAT );

					/* ログを出力 */
					sprintf(logMsg, "main(): 装置モード「待機」中にＤＲＢＤ同期異常発生により「準備」へ遷移" );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
			}
		}

		/* 他プロセスからのメッセージ受信（メッセージによる指示があるか確認） */
		func_msg_receive();

		/* ================== */
		/* 装置モード変更確認 */
		/* ================== */

		/* 運用モード変更確認時刻到達チェック */
		if( ( ps_EMSYSTEM->modeTranTime != 0 ) && ( ps_EMSYSTEM->modeTranTime <= oldTime ) )
		{
			/* ログを出力 */
			sprintf(logMsg, "main(): 装置モード遷移確認処理" );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* モード変更確認時刻リセット */
			ps_EMSYSTEM->modeTranTime = 0;

			/* 装置モード変更有無決定処理（現在の状態から運用モード遷移を行うか判定する） */
			func_chk_mode_tran();
		}

		/* 装置モードや装置状態に変化があるか確認 */
		func_chk_stat_change();

		/* モード遷移中の場合 */
		if( newSysMode != -1 )
		{
			/* モード変更シーケンス処理 */
			func_mode_trans_seq();
		}

		/* 処理待ち */
		sleep(1);
	}

	/* 死活監視スレッド停止フラグ：ＯＮ */
	threadStop = 1;
	pthread_join( threadIDrcv[ 0 ], NULL );
	pthread_join( threadIDrcv[ 1 ], NULL );
	pthread_join( threadIDsnd, NULL );

	/* 動作環境破棄 */
	embase_func_finish();

	/* 管理プロセス停止指示がある場合 */
	if( access( checkFileStop, R_OK ) == 0 )
	{
		/* ログを出力 */
		sprintf(logMsg, "main(): 装置監視プロセス 停止指示ファイル削除" );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		/* 停止指示ファイルを削除 */
		unlink( checkFileStop );
	}

	/* ログを出力 */
	sprintf(logMsg, "main(): 装置監視プロセス FINISH!" );
	func_log_msgset( EMSYS_LOG_LV3, "==========================================", NULL, 0 );
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	func_log_msgset( EMSYS_LOG_LV3, "==========================================", NULL, 0 );

#ifdef DEBUG_1
printf("%s: FINISH\n",EMBASE_OWN_NAME );
#endif

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モード遷移確認処理
 *
 * 機能：
 *		現在状態をもとに装置モードの遷移が必要か判定する。判定結果を戻り値
 *		として返す。
 *		
 * 戻り値：
 *		遷移先の装置モード
 *
 */
int func_chk_mode_tran()
{
	int					retCode = -1;							/* 戻り値 */
	int					loopCnt1;								/* ループカウンタ */
	int					loopCnt2;								/* ループカウンタ */
	int					keiInfo;								/* 系情報 */
	int					workData;								/* ワークデータ */
	int					check[10];								/* チェックデータ */
	time_t				lastTime[2];							/* 最終時間 */
	time_t				totalTime[2];							/* 積算時間 */

	/* 系情報を取得 */
	keiInfo = embase_func_get_kei_info();

	/* 運用モード関連時間情報取得 */
	lastTime[1] = embase_func_get_par_operate_last_time();
	totalTime[1] = embase_func_get_par_operate_total_time();
	lastTime[0] = embase_func_get_my_operate_last_time();
	totalTime[0] = embase_func_get_my_operate_total_time();

	/* 自系と他系の運用モード最終稼働時刻の差が 死活監視情報送信周期+2 秒以内の場合 */
	if( (udpSendTime + 2) > abs(lastTime[0] - lastTime[1]) )
	{
		/* 自系が１系の場合 */
		check[8] = 0;
	}
	
	/* 仮設定 */
	check[8] = ( lastTime[0] >= lastTime[1] ) ? 0 : 1 ;
	check[9] = ( totalTime[0] >= totalTime[1] ) ? 0 : 1 ;

	/* 自系が１系の場合 */
	if( keiInfo == EMBASE_SYSNUMBER_1 )
	{
		check[0] = embase_func_get_1kei_mode();
		check[1] = embase_func_get_2kei_mode();
		check[2] = embase_func_get_1kei_net_service();
		check[3] = embase_func_get_1kei_net_internal();
		check[6] = embase_func_get_2kei_net_service();
		check[7] = embase_func_get_2kei_net_internal();

		/* 自系と他系の運用モード最終稼働時刻の差が 死活監視情報送信周期+2 秒以内の場合 */
		if( (udpSendTime + 2) > abs(lastTime[0] - lastTime[1]) )
			check[8] = 0;
		/* 自系と他系の運用モード正常稼働時間の差が 死活監視情報送信周期+2 秒以内の場合 */
		if( (udpSendTime + 2) > abs(totalTime[0] - totalTime[1]) )
			check[9] = 0;
	}
	/* 自系が２系の場合 */
	else
	{
		check[0] = embase_func_get_2kei_mode();
		check[1] = embase_func_get_1kei_mode();
		check[2] = embase_func_get_2kei_net_service();
		check[3] = embase_func_get_2kei_net_internal();
		check[6] = embase_func_get_1kei_net_service();
		check[7] = embase_func_get_1kei_net_internal();

		/* 自系と他系の運用モード最終稼働時刻の差が 死活監視情報送信周期+2 秒以内の場合 */
		if( (udpSendTime + 2) > abs(lastTime[0] - lastTime[1]) )
			check[8] = 1;
		/* 自系と他系の運用モード正常稼働時間の差が 死活監視情報送信周期+2 秒以内の場合 */
		if( (udpSendTime + 2) > abs(totalTime[0] - totalTime[1]) )
			check[9] = 1;
			
	}
	check[4] = embase_func_get_lifchk_service();
	check[5] = embase_func_get_lifchk_internal();

	/* ログを出力 */
	sprintf(logMsg, "func_chk_mode_tran(): モード遷移 判定情報:[ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d ]",
		check[0], check[1], check[2], check[3], check[4], check[5], check[6], check[7], check[8], check[9]);
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

	/* 装置モードが同じ場合は、時間情報もログに出力する */
	if( check[0] == check[1] )
	{
		sprintf(logMsg, "func_chk_mode_tran(): モード遷移 判定情報 Last[ %d / %d ](+/-%d) Total[ %d / %d ](+/-%d)",
			lastTime[0], lastTime[1], abs(lastTime[0]-lastTime[1]), totalTime[0], totalTime[1], abs(totalTime[0]-totalTime[1]) );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	}
	

	/* モード判定テーブル確認処理 */
	for( loopCnt1 = 0 ; s_modTrnChkTbl[ loopCnt1 ].numb != -1 ; loopCnt1++ )
	{
		/* 判定情報の照合 */
		for( loopCnt2 = 0 ; loopCnt2 < 10 ; loopCnt2++ )
		{
			/* 判定対象外設定の場合は次の判定情報を処理 */
			if( -1 == s_modTrnChkTbl[ loopCnt1 ].check[ loopCnt2 ] ) continue;
			/* 一致しない場合は、次のテーブルを処理 */
			if( check[ loopCnt2 ] != s_modTrnChkTbl[ loopCnt1 ].check[ loopCnt2 ] ) break;
		}
		/* 全てのチェック項目が一致した場合 */
		if( loopCnt2 == 10 )
		{
			/* モード遷移番号（予定）設定 */
			retCode = s_modTrnChkTbl[ loopCnt1 ].newMode; 

			/* 処理シーケンス番号を設定 */
			switch( retCode )
			{
				/* 準備 */
				case EMBASE_SYSMOD_WAIT:
					/* モード遷移シーケンス設定（待機->準備） */
					func_set_mode_tran( EMSYS_SEQ_WAT2STB );
					break;

				/* 停止（現時点では遷移先としては未定義） */
				case EMBASE_SYSMOD_STOP:
					/* モード遷移シーケンス設定（処理なし） */
					modTrnSeqNumb = 0;
					break;

				/* 待機 */
				case EMBASE_SYSMOD_STANDB:
					/* 現モードが「運用」の場合 */
					if( EMBASE_SYSMOD_OPERAT == ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].sysMode )
					{
						/* モード遷移シーケンス設定（運用->待機） */
						func_set_mode_tran( EMSYS_SEQ_OPE2STB );
					}
					else
					{
						/* モード遷移シーケンス設定（準備→待機） */
						func_set_mode_tran( EMSYS_SEQ_WAT2STB );
					}
					break;

				/* 運用 */
				case EMBASE_SYSMOD_OPERAT:
					/* モード遷移シーケン設定（待機->運用、準備->運用） */
					func_set_mode_tran( EMSYS_SEQ_OPERAT );
					break;

				default:
					/* モード遷移シーケンス設定（処理なし） */
					modTrnSeqNumb = 0;
					break;
			}

			/* ログを出力 */
			sprintf(logMsg, "func_chk_mode_tran(): モード遷移 遷移決定 判断TBL-No[ %d ] モード遷移先[ %d ] 処理シーケンスNo[ %d ]",
				s_modTrnChkTbl[ loopCnt1 ].numb, retCode, modTrnSeqNumb );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			break;
		}
		/* モード遷移が決定した場合は処理を抜ける */
		if( retCode != -1 ) break;
	}
	if( retCode == -1 )
	{
		/* ログを出力 */
		sprintf(logMsg, "func_chk_mode_tran(): モード遷移 遷移対象外");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * メッセージ受信処理
 *
 * 機能：
 *		他プロセスやツールなどからメッセージによる指示があるか確認する。
 *		指示がある場合は、指示内容が実行可能か判断し、可能な場合は指示に従った処
 *		理を行う
 *		
 * 戻り値：
 *		なし
 *
 */
void func_msg_receive()
{
	int					msgKind;								/* メッセージ種別 */

	/* メッセージキューからメッセージを受信 */
	if( 0 > (msgKind = embase_func_msgq_receive()) )
	{
		/* エラー */
		/* ログを出力 */
		sprintf(logMsg, "func_msg_receive(): メッセージ受信異常 retCode [ %d ]", msgKind );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	}
	else
	{
		/* （受信メッセージなし） */
		if( msgKind == 0 )
		{
			/* 制御依頼種別（受信用）による指示があるか確認 */
			if( ctlKindRecv != 0 )
			{
				/* 種別を制御依頼種別に設定 */
				msgKind = ctlKindRecv;
				/* 制御依頼受別（受信用）をリセット */
				ctlKindRecv = 0;

				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): 他系から制御依頼 種別コード [ %d ] を受信：メッセージ受信として扱う", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}

		/* 種別による処理 */
		switch( msgKind )
		{
			/* （メッセージおよび制御依頼種別（受信用）なし） */
			case 0:
				break;

			/* （準備） */
			case EMBASE_MSG_MOD_WAIT:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「準備」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 「準備」遷移条件 */

				/* 自系が「停止」モード であること */
				if( EMBASE_SYSMOD_STOP == embase_func_get_my_mode() )
				{
					/* ログを記録 */
					func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 処理条件を満たすため「準備」に遷移", NULL, 0 );
					/* モード遷移実行 */
					func_set_mode_tran( EMSYS_SEQ_STP2WAT );
				}
				else
				{
					/* ログを記録 */
					func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 処理条件を満たさないため無処理", NULL, 0 );
				}
				break;

			/* （停止） */
			case EMBASE_MSG_MOD_STOP:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「停止」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 「停止」遷移条件 */

				/* 自系が「準備」モード または */
				/* 自系が「待機」モード であること*/
				if( (EMBASE_SYSMOD_WAIT == embase_func_get_my_mode())  ||
				    (EMBASE_SYSMOD_STANDB == embase_func_get_my_mode()) )
				{
					/* ログを記録 */
					func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 処理条件を満たすため「停止」に遷移", NULL, 0 );
					/* モード遷移実行 */
					func_set_mode_tran( EMSYS_SEQ_STB2STP );
				}
				else
				{
					/* ログを記録 */
					func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 処理条件を満たさないため無処理", NULL, 0 );
				}
				break;

			/* （待機） */
			case EMBASE_MSG_MOD_STANDB:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「待機」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 運用->待機 遷移条件 */
				/* 自系が「運用」モード、かつ、 他系が「待機」モード */
				/* 自系および他系のネットワークI/F状態が全て正常であること */
				/* 死活監視状態が全て正常であること */
				if( (EMBASE_SYSMOD_OPERAT == embase_func_get_my_mode()) &&
				    (EMBASE_SYSMOD_STANDB == embase_func_get_par_mode()) &&
				    (EMBASE_NETDEV_OK == ps_EMSYSTEM->s_kei[0].netDevSts[0]) &&
				    (EMBASE_NETDEV_OK == ps_EMSYSTEM->s_kei[0].netDevSts[1]) &&
				    (EMBASE_NETDEV_OK == ps_EMSYSTEM->s_kei[1].netDevSts[0]) &&
				    (EMBASE_NETDEV_OK == ps_EMSYSTEM->s_kei[1].netDevSts[1]) &&
				    (EMBASE_LIFCHK_OK == ps_EMSYSTEM->lifChkSts[0]) &&
				    (EMBASE_LIFCHK_OK == ps_EMSYSTEM->lifChkSts[1]) )
				{
					/* ログを記録 */
					func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 処理条件を満たすため「待機」に遷移", NULL, 0 );
					/* モード遷移実行 */
					func_set_mode_tran( EMSYS_SEQ_OPE2STB );
				}
				else
				{
					/* ログを記録 */
					func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 処理条件を満たさないため無処理", NULL, 0 );
				}
				break;

			/* （運用） */
			case EMBASE_MSG_MOD_OPERAT:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「運用」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを記録 */
				func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 遷移指示に対する処理未実装", NULL, 0 );
				break;

			/* （他系準備） */
			case EMBASE_MSG_MOD_PAR_WAIT:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「準備（他系）」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを記録 */
				func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 他系に指示を送信", NULL, 0 );
				/* 制御依頼種別（送信用）を設定 */
				ctlKindSend = EMBASE_MSG_MOD_WAIT;
				break;

			/* （他系停止） */
			case EMBASE_MSG_MOD_PAR_STOP:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「停止（他系）」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを記録 */
				func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 他系に指示を送信", NULL, 0 );
				/* 制御依頼種別（送信用）を設定 */
				ctlKindSend = EMBASE_MSG_MOD_STOP;
				break;

			/* （他系待機） */
			case EMBASE_MSG_MOD_PAR_STANDB:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「待機（他系）」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを記録 */
				func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 他系に指示を送信", NULL, 0 );
				/* 制御依頼種別（送信用）を設定 */
				ctlKindSend = EMBASE_MSG_MOD_STANDB;
				break;

			/* （他系運用） */
			case EMBASE_MSG_MOD_PAR_OPERAT:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「運用（他系）」遷移指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを記録 */
				func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 他系に指示を送信", NULL, 0 );
				/* 制御依頼種別（送信用）を設定 */
				ctlKindSend = EMBASE_MSG_MOD_OPERAT;
				break;

			/* （シャットダウン） */
			case EMBASE_MSG_OS_SHUTDOWN:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「シャットダウン」指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* ログを出力 */
				func_log_msgset( EMSYS_LOG_LV3, "----------------------------------------", NULL, 0 );
				func_log_msgset( EMSYS_LOG_LV3, " func_msg_receive(): シャットダウン実行 ", NULL, 0 );
				func_log_msgset( EMSYS_LOG_LV3, "----------------------------------------", NULL, 0 );

				/* シャットダウンコマンド実行 */
				system("/usr/sbin/shutdown 0 & 2> /dev/null");
				sleep( 3 );

				break;

			/* （他系シャットダウン） */
			case EMBASE_MSG_OS_PAR_SHUTDOWN:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：「シャットダウン（他系）」指示 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを記録 */
				func_log_msgset( EMSYS_LOG_LV3, "func_msg_receive(): 他系に指示を送信", NULL, 0 );
				/* 制御依頼種別（送信用）を設定 */
				ctlKindSend = EMBASE_MSG_OS_SHUTDOWN;
				break;

			/* （該当なし） */
			default:
				/* ログを出力 */
				sprintf(logMsg, "func_msg_receive(): メッセージ受信：種別異常 種別コード [ %d ]", msgKind );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				break;
		}
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モード遷移シーケンス設定
 *
 * 機能：
 *		引き数 Numb で指定されたモード遷移シーケンス番号をもとにモード遷移先
 *		情報を設定する
 *		
 * 戻り値：
 *		なし
 *
 */
void func_set_mode_tran(int Numb )
{
	/* モード遷移シーケンス番号設定 */
	modTrnSeqNumb = Numb;

	/* モード遷移先を設定 */
	switch( Numb )
	{
		/*（任意->運用）*/
		case EMSYS_SEQ_OPERAT:
			/* モード遷移先設定 */
			newSysMode = EMBASE_SYSMOD_OPERAT;
			/* モード遷移先（予定）設定 */
			ps_EMSYSTEM->newSysMode = newSysMode;
			break;

		/*（準備->待機）*/
		case EMSYS_SEQ_WAT2STB:
			/* モード遷移先設定 */
			newSysMode = EMBASE_SYSMOD_STANDB;
			/* モード遷移先（予定）設定 */
			ps_EMSYSTEM->newSysMode = newSysMode;
			break;

		/*（運用->待機）*/
		case EMSYS_SEQ_OPE2STB:
			/* モード遷移先設定 */
			newSysMode = EMBASE_SYSMOD_STANDB;
			/* モード遷移先（予定）設定 */
			ps_EMSYSTEM->newSysMode = newSysMode;
			break;

		/*（待機->準備）（停止->準備）*/
		case EMSYS_SEQ_STB2WAT:
			/* モード遷移先設定 */
			newSysMode = EMBASE_SYSMOD_WAIT;
			/* モード遷移先（予定）設定 */
			ps_EMSYSTEM->newSysMode = newSysMode;
			break;

		/*（待機->停止）（準備->停止）*/
		case EMSYS_SEQ_STB2STP:
			/* モード遷移先設定 */
			newSysMode = EMBASE_SYSMOD_STOP;
			/* モード遷移先（予定）設定 */
			ps_EMSYSTEM->newSysMode = newSysMode;
			break;

		default:
			break;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モード遷移判定要因検出処理
 *
 * 機能：
 *		モード遷移確認を行う条件が発生しているか確認する。
 *		以下の状態変化を確認する
 *		・他系のモード遷移
 *		・自系のネットワークI/F状態の変化
 *		・自系の死活監視情報受信状態の変化
 *		
 * 戻り値：
 *		なし
 *
 */
void func_chk_stat_change()
{
		/* 他系モード変化時 */
		if( parSysMode != embase_func_get_par_mode())
		{
			/* 他系モードを保存 */
			parSysMode   = embase_func_get_par_mode();
			/* モード変更確認時刻を更新 */
			ps_EMSYSTEM->modeTranTime = time( NULL ) + 3;
		}
		/* ネットワークI/F（サービス用）状態変化時 または */
		/* ネットワークI/F（系列間用）状態変化時 または */
		/* 死活監視情報（サービス用）状態変化時 または */
		/* 死活監視情報（系列間用）状態変化時 */
		if( (lifChkSts[0] != embase_func_get_lifchk_service())  ||
		    (lifChkSts[1] != embase_func_get_lifchk_internal()) ||
		    (netDevSts[0] != embase_func_get_my_net_service())  ||
		    (netDevSts[1] != embase_func_get_my_net_internal()) ||
		    (netDevSts[2] != embase_func_get_par_net_service()) ||
		    (netDevSts[3] != embase_func_get_par_net_internal()) )
		{
			/* 変化状態を保存 */
			lifChkSts[0] = embase_func_get_lifchk_service();
			lifChkSts[1] = embase_func_get_lifchk_internal();
			netDevSts[0] = embase_func_get_my_net_service();
			netDevSts[1] = embase_func_get_my_net_internal();
			netDevSts[2] = embase_func_get_par_net_service();
			netDevSts[3] = embase_func_get_par_net_internal();
			/* モード変更確認時刻を更新 */
			if( ps_EMSYSTEM->modeTranTime == 0 ) ps_EMSYSTEM->modeTranTime = time( NULL ) + 3;
		}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モード遷移シーケンス処理
 *
 * 機能：
 *		モード遷移に必要なシーケンス処理を行う
 *		
 * 戻り値：
 *		0:正常 ／ -1:異常
 *
 */
void func_mode_trans_seq()
{
	int					workData;								/* ワーク用変数 */
	int					workDataPar;							/* ワーク用変数 */
	int					retCode;								/* 戻り値 */
	int					OLD_modTrnSeqNumb;						/* モード遷移シーケンス番号（保存） */
	char				cmdStr[1024];							/* コマンド用文字列 */
	struct in_addr		workAddr;								/* ワーク用 */

	/* モード遷移シーケンス番号を保存 */
	OLD_modTrnSeqNumb = modTrnSeqNumb;

	switch( modTrnSeqNumb )
	{

	/* ====================== */
	/* 待機->運用、準備->運用 */
	/* ====================== */

		/* （共通ホスト名で使用するIPアドレスの設定） */
		case  1:

			/* 共有ホスト名が設定されている場合 */
			if( strlen( sysHostName[EMBASE_SYSNUMBER_C] ) > 0 )
			{
				/* サービス用ネットワークI/Fに共有ホスト名に対応したIPアドレスが付与されている場合 */
				if( func_get_netmask( sysHostName[EMBASE_SYSNUMBER_C] ) > 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名[ %s ]対応 IPアドレス設定済", 
						modTrnSeqNumb, sysHostName[EMBASE_SYSNUMBER_C] );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
				else
				{
					/* 共有ホスト名に対応したIPアドレスを設定 */
					workAddr.s_addr = commonIPaddr;
					sprintf( cmdStr, "/bin/nmcli con modify %s +ipv4.address %s/%d &> /dev/null", 
						netDeviceName[EMBASE_NETDEV_SERVICE], inet_ntoa( workAddr ), commonNetMask );
					retCode = system( cmdStr );
					/* 正常の場合 */
					if( 0 == retCode )
					{
						/* ログを出力 */
						sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名[ %s ]対応 IPアドレス[ %s/%d ]設定", 
							modTrnSeqNumb, sysHostName[EMBASE_SYSNUMBER_C], inet_ntoa( workAddr ), commonNetMask );
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

						/* ネットワークI/Fの起動 */
						sprintf( cmdStr, "/bin/nmcli con up %s &> /dev/null", netDeviceName[EMBASE_NETDEV_SERVICE] );
						retCode = system( cmdStr );
						/* 正常の場合 */
						if( 0 == retCode )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名[ %s ]対応 IPアドレス[ %s/%d ]有効設定", 
								modTrnSeqNumb, sysHostName[EMBASE_SYSNUMBER_C], inet_ntoa( workAddr ), commonNetMask );
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
					}
				}
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名未設定", modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;
/* [20250717] Add St */
		/* （時刻同期のチェック） */
		case  2:

			/* 時刻同期チェックが有効の場合 */
			if( 1 == timeSyncCheckFlg )
			{
				/* 時刻同期チェックを実施 */
				if( func_timeSync_Check() >= 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 時刻同期済み",
						modTrnSeqNumb );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 時刻同期エラー",
						modTrnSeqNumb );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 時刻同期チェック無効", modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}

			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;
/* [20250717] Add Ed */

		/* （自系のロールをプライマリに変更する） */
		case  3:

			/* 現在の自系のロール状態を取得 */
			workData = embase_func_get_drbd_my_role();
			/* 現在の状態がプライマリの場合 */
			if( EMBASE_DRBD_ROLE_PRIMARY == workData )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource[ %s ] already [ primary ]",
					modTrnSeqNumb, drbdResourceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			else
			{
				/* プライマリロールに変更するコマンドを実行 */
				sprintf( cmdStr, "/usr/sbin/drbdadm primary %s 2> /dev/null", drbdResourceName );
				retCode = system( cmdStr );
				if( retCode != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ drbdadm primary %s ] 失敗 errno:%d",
						modTrnSeqNumb, drbdResourceName, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

					/* ＤＲＢＤ状態復旧処理 */
					func_drbd_recovery();
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ drbdadm primary %s ] 成功",
						modTrnSeqNumb, drbdResourceName );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
			}
			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;

		/* （ＤＲＢＤデバイスをマウント） */
		case  4:

			/* 現在の自系のロール状態を取得 */
			workData = embase_func_get_drbd_my_role();
			/* 現在の状態がプライマリの場合 */
			if( EMBASE_DRBD_ROLE_PRIMARY == workData )
			{
				/* ＤＲＢＤデバイスがマウントされているか確認するコマンドを実行 */
				sprintf( cmdStr, "/usr/bin/grep %s /etc/mtab > /dev/null 2>&1", drbdDeviceName );
				retCode = system( cmdStr );
				if( retCode == 0 )
				{
					/* マウント済み */
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: device [ %s ]  already mounted",
						modTrnSeqNumb, drbdDeviceName );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
				else
				{
					/* ＤＲＢＤデバイスをマウントするコマンドを実行 */
					sprintf( cmdStr, "/bin/mount %s /drbd > /dev/null 2>&1", drbdDeviceName );
					retCode = system( cmdStr );
					if( retCode != 0 )
					{
						/* ログを出力 */
						sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ mount %s /drbd ] 失敗 errno:%d",
							modTrnSeqNumb, drbdDeviceName, errno );
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					}
					else
					{
						/* ログを出力 */
						sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ mount %s /drbd ] 成功",
							modTrnSeqNumb, drbdDeviceName );
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

						/* 処理シーケンス番号を次のステップに設定 */
						modTrnSeqNumb += 1;
					}
				}
			}
			else
			{
				/* ロールがプライマリではないため、処理シーケンス番号をひとつ前のステップに設定 */
				modTrnSeqNumb -= 1;
			}
			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;

		/* （業務アプリケーションの起動） */
		case  5:

			/* 業務アプリケーションを実行する */
			if( 0 != strlen(appServiceName) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 業務アプリサービス( %s ) 起動処理開始",
					modTrnSeqNumb, appServiceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				sprintf( cmdStr, "systemctl start %s > /dev/null 2>&1", appServiceName );
				retCode = system( cmdStr );
				if( retCode != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ sytemctl start %s ] 失敗 errno:%d",
						modTrnSeqNumb, appServiceName, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ systemctl start %s ] 成功",
						modTrnSeqNumb, appServiceName );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
			}
			break;

		/* （業務アプリケーションの動作状態確認） */
		case  6:

			/* 動作監視対象プロセス確認 */
			if( 0 == func_chk_oper_proc() )
			{
				/* 監視対象なし、または、監視対象プロセスが全て動作中の場合 */

				/* ================== */
				/* 運用モード遷移完了 */
				/* ================== */
			
				/* 装置モードを「運用」に設定 */
				ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode = EMBASE_SYSMOD_OPERAT;

				/* 遷移先モードを設定 */
				newSysMode = -1;

				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 業務アプリケーション動作確認 完了",
					modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移完了 確定[ 運用 ]");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 処理シーケンス番号を終了に設定 */
				modTrnSeqNumb = 0;

				/* モード変更確認時刻を更新 */
				if( ps_EMSYSTEM->modeTranTime == 0 ) ps_EMSYSTEM->modeTranTime = time( NULL ) + 10;

				/* 運用遷移中停止フラグOFF *//* [20200714] Add */
				opeTranStopFlg = 0;
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 業務アプリケーション動作確認 動作未確認", modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			break;

	/* ====================== */
	/* 準備->待機             */
	/* ====================== */

		/* （ＤＲＢＤのロール状態確認） */
		case 11:
			/* 現在の自系のロール状態を取得 */
			workData    = embase_func_get_drbd_my_role();
			/* 現在の他系のロール状態を取得 */
			workDataPar = embase_func_get_drbd_par_role();
			/* 自系がセカンダリかつ他系がプライマリの場合 */
			if( (workData == EMBASE_DRBD_ROLE_SECONDARY) && ( workDataPar == EMBASE_DRBD_ROLE_PRIMARY ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] role state 条件成立",
					modTrnSeqNumb, drbdResourceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] role state( %d / %d ) 条件不成立",
					modTrnSeqNumb, drbdResourceName, workData, workDataPar );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* ＤＲＢＤ状態復旧処理 */
				func_drbd_recovery();
			}
			break;

		/* （ＤＲＢＤのディスク状態の確認） */
		case 12:
			/* 現在の自系のディスク状態を取得 */
			workData    = embase_func_get_drbd_my_dstate();
			/* 現在の他系のディスク状態を取得 */
			workDataPar = embase_func_get_drbd_par_dstate();
			/* 自系が UpToData かつ他系が UpToData の場合 */
			if( (workData == EMBASE_DRBD_DSTATE_UPTODATE) && ( workDataPar == EMBASE_DRBD_DSTATE_UPTODATE ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] disk state 条件成立",
					modTrnSeqNumb, drbdResourceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			/* 自系が Inconsistent かつ他系が UpToData の場合 */
			else if( (workData == EMBASE_DRBD_DSTATE_INCONSISTENT) && ( workDataPar == EMBASE_DRBD_DSTATE_UPTODATE ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] disk state( %d / %d ) 条件不成立（同期完了待）",
					modTrnSeqNumb, drbdResourceName, workData, workDataPar );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] disk state( %d / %d ) 条件不成立",
					modTrnSeqNumb, drbdResourceName, workData, workDataPar );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			break;

		/* （ＤＲＢＤの通信状態の確認） */
		case 13:
			/* 現在の自系の通信状態を取得 */
			workData = embase_func_get_drbd_my_cstate();
			/* 現在の通信状態が connected の場合 */
			if( workData == EMBASE_DRBD_CSTATE_CONNECTED )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] 接続状態 条件成立",
					modTrnSeqNumb, drbdResourceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* ================== */
				/* 待機モード遷移完了 */
				/* ================== */
			
				/* 装置モードを「待機」に設定 */
				ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode = EMBASE_SYSMOD_STANDB;

				/* 遷移先モードを設定 */
				newSysMode = -1;

				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移完了 確定[ 待機 ]");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 処理シーケンス番号を終了に設定 */
				modTrnSeqNumb = 0;

				/* モード変更確認時刻を更新 */
				if( ps_EMSYSTEM->modeTranTime == 0 ) ps_EMSYSTEM->modeTranTime = time( NULL ) + 10;

				/* 運用遷移中停止フラグOFF *//* [20200714] Add */
				opeTranStopFlg = 0;
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] connect state( %d ) 条件不成立",
					modTrnSeqNumb, drbdResourceName, workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			break;

	/* ====================== */
	/* 運用->待機             */
	/* ====================== */

		/* （業務アプリケーションの停止)  */
		case 21:

			/* 業務アプリケーションを停止する */
			if( 0 != strlen(appServiceName) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 業務アプリサービス( %s ) 停止処理開始",
					modTrnSeqNumb, appServiceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				sprintf( cmdStr, "systemctl stop %s > /dev/null 2>&1", appServiceName );
				retCode = system( cmdStr );
				if( retCode != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ sytemctl stop %s ] 失敗 errno:%d",
						modTrnSeqNumb, appServiceName, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ systemctl stop %s ] 成功",
						modTrnSeqNumb, appServiceName );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
			}

		/* （業務アプリケーションの停止の確認） */
		case 22:

			/* 監視対象プロセス停止確認 */
			if( 0 == func_chk_stop_proc() )
			{
				/* 監視対象なし、または、監視対象プロセスが全て停止中の場合 */

				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: アプリケーション停止確認 条件成立", modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;

		/* （共通ホスト名で使用するIPアドレスの解除） */
		case 23:

			/* 共有ホスト名が設定されている場合 */
			if( strlen( sysHostName[EMBASE_SYSNUMBER_C] ) > 0 )
			{
				/* サービス用ネットワークI/Fに共有ホスト名に対応したIPアドレスが付与されている場合 */
				if( func_get_netmask( sysHostName[EMBASE_SYSNUMBER_C] ) > 0 )
				{
					/* 共有ホスト名に対応したIPアドレスを削除 */
					workAddr.s_addr = commonIPaddr;
					sprintf( cmdStr, "/bin/nmcli con modify %s -ipv4.address %s/%d &> /dev/null", 
						netDeviceName[EMBASE_NETDEV_SERVICE], inet_ntoa( workAddr ), commonNetMask );
					retCode = system( cmdStr );
					/* 正常の場合 */
					if( 0 == retCode )
					{
						/* ログを出力 */
						sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名[ %s ]対応 IPアドレス[ %s/%d ]削除設定", 
							modTrnSeqNumb, sysHostName[EMBASE_SYSNUMBER_C], inet_ntoa( workAddr ), commonNetMask );
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

						/* ネットワークI/Fの起動 */
						sprintf( cmdStr, "/bin/nmcli con up %s &> /dev/null", netDeviceName[EMBASE_NETDEV_SERVICE] );
						retCode = system( cmdStr );
						/* 正常の場合 */
						if( 0 == retCode )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名[ %s ]対応 IPアドレス[ %s/%d ]削除実行", 
								modTrnSeqNumb, sysHostName[EMBASE_SYSNUMBER_C], inet_ntoa( workAddr ), commonNetMask );
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
					}
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名[ %s ]対応 IPアドレス削除済", 
						modTrnSeqNumb, sysHostName[EMBASE_SYSNUMBER_C] );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
			}
			else
			{
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 共有ホスト名未設定", modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;

		/* （ＤＲＢＤデバイスのアンマウント） */
		case 24:

			/* ＤＲＢＤデバイスがマウントされているか確認するコマンドを実行 */
			sprintf( cmdStr, "/usr/bin/grep %s /etc/mtab > /dev/null 2>&1", drbdDeviceName );
			retCode = system( cmdStr );
			if( retCode == 0 )
			{
				/* ＤＲＢＤデバイスをアンマウントするコマンドを実行 */
				sprintf( cmdStr, "/bin/umount %s > /dev/null 2>&1", drbdDeviceName );
				retCode = system( cmdStr );
				if( retCode != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ umount %s ] 失敗 errno:%d",
						modTrnSeqNumb, drbdDeviceName, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ umount %s ] 成功 条件成立",
						modTrnSeqNumb, drbdDeviceName );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					/* 処理シーケンス番号を次のステップに設定 */
					modTrnSeqNumb += 1;
				}
			}
			else
			{
				/* アンマウント済み */
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: device [ %s ] already umounted 条件成立",
					modTrnSeqNumb, drbdDeviceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 処理シーケンス番号を次のステップに設定 */
				modTrnSeqNumb += 1;
			}
			/* モード遷移シーケンス番号に変更がない場合は処理を抜ける */
			if( OLD_modTrnSeqNumb >= modTrnSeqNumb )
				break;

			/* モード遷移シーケンス番号を保存 */
			OLD_modTrnSeqNumb = modTrnSeqNumb;

		/* （ＤＲＢＤをセカンダリに変更） */
		case 25:

			/* 現在の自系のロール状態を取得 */
			workData = embase_func_get_drbd_my_role();
			/* 現在の状態がプライマリの場合 */
			if( EMBASE_DRBD_ROLE_PRIMARY == workData )
			{
				/* セカンダリロールに変更するコマンドを実行 */
				sprintf( cmdStr, "/usr/sbin/drbdadm secondary %s 2> /dev/null", drbdResourceName );
				retCode = system( cmdStr );
				if( retCode != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ drbdadm secondary %s ] 失敗 errno:%d",
						modTrnSeqNumb, drbdResourceName, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: cmd [ drbdadm secondary %s ] 成功",
						modTrnSeqNumb, drbdResourceName );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
			}
			else
			{
				/* ================== */
				/* 待機モード遷移完了 */
				/* ================== */
			
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: resource [ %s ] already state [ standby ] 条件成立",
					modTrnSeqNumb, drbdResourceName );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* ログを出力 */
				sprintf(logMsg, "func_mode_trans_seq(): モード遷移完了 確定[ 待機 ]");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 処理シーケンス番号を処理終了に設定 */
				modTrnSeqNumb = 0;

				/* 装置モードを「待機」に設定 */
				ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode = EMBASE_SYSMOD_STANDB;

				/* 遷移先モードを設定 */
				newSysMode = -1;

				/* モード変更確認時刻を更新 */
				if( ps_EMSYSTEM->modeTranTime == 0 ) ps_EMSYSTEM->modeTranTime = time( NULL ) + 10;

				/* 運用遷移中停止フラグOFF *//* [20200714] Add */
				opeTranStopFlg = 0;
			}
			break;

	/* ====================== */
	/* 待機->準備、停止->準備 */
	/* ====================== */

		/* （準備への遷移完了） */
		case 31:

			/* ================== */
			/* 準備モード遷移完了 */
			/* ================== */
		
			/* ログを出力 */
			sprintf(logMsg, "func_mode_trans_seq(): モード遷移完了 確定[ 準備 ]");
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* 処理シーケンス番号を処理終了に設定 */
			modTrnSeqNumb = 0;

			/* 装置モードを「準備」に設定 */
			ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode = EMBASE_SYSMOD_WAIT;

			/* 遷移先モードを設定 */
			newSysMode = -1;

			/* モード変更確認時刻を更新 */
			if( ps_EMSYSTEM->modeTranTime == 0 ) ps_EMSYSTEM->modeTranTime = time( NULL ) + 10;

			/* 運用遷移中停止フラグOFF *//* [20200714] Add */
			opeTranStopFlg = 0;

			break;
			
	/* ====================== */
	/* 待機->停止、準備->停止 */
	/* ====================== */

		/* （停止への遷移完了） */
		case 91:

			/* ================== */
			/* 停止モード遷移完了 */
			/* ================== */
		
			/* ログを出力 */
			sprintf(logMsg, "func_mode_trans_seq(): モード遷移完了 確定[ 停止 ]");
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* 処理シーケンス番号を処理終了に設定 */
			modTrnSeqNumb = 0;

			/* 装置モードを「停止」に設定 */
			ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode = EMBASE_SYSMOD_STOP;

			/* 遷移先モードを設定 */
			newSysMode = -1;

			/* モード変更確認時刻を更新 */
			if( ps_EMSYSTEM->modeTranTime == 0 ) ps_EMSYSTEM->modeTranTime = time( NULL ) + 10;

			/* 運用遷移中停止フラグOFF *//* [20200714] Add */
			opeTranStopFlg = 0;

			break;
			
		/* （該当なし） */
		default:

			/* ログを出力 */
			sprintf(logMsg, "func_mode_trans_seq(): モード遷移シーケンス[ %d ]: 処理対象なし", modTrnSeqNumb );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			break;
	} 
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 初期化処理
 *
 * 機能：
 *		装置状態情報を初期化する
 *		
 * 戻り値：
 *		0:正常 ／ -1:異常
 *
 */
int func_init()
{
	char				hostName[128];							/* ホスト名称 */
	int					retCode;								/* 戻り値 */

	/* ログを出力 */
	sprintf(logMsg, "func_init(): 開始");
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

	/* 状態管理の初期化 */
	ps_EMSYSTEM->lifChkSts[0] = EMBASE_LIFCHK_NG;				/* 死活監視（サービスＬＡＮ経由）：異常 */
	ps_EMSYSTEM->lifChkSts[1] = EMBASE_LIFCHK_NG;				/* 死活監視（系列間ＬＡＮ経由）  ：異常 */
	ps_EMSYSTEM->s_kei[0].sysMode = EMBASE_SYSMOD_UNKNOW;		/* １系装置：装置モード：未確定 */
	ps_EMSYSTEM->s_kei[1].sysMode = EMBASE_SYSMOD_UNKNOW;		/* ２系装置：装置モード：未確定 */
	ps_EMSYSTEM->drbdRole[0] = EMBASE_DRBD_ROLE_OTHER;			/* ＤＲＢＤロール状態（自系）：そのほか */
	ps_EMSYSTEM->drbdRole[1] = EMBASE_DRBD_ROLE_OTHER;			/* ＤＲＢＤロール状態（他系）：そのほか */
	ps_EMSYSTEM->drbdDstate[0] = EMBASE_DRBD_DSTATE_OTHER;		/* ＤＲＢＤディスク状態（自系）：そのほか */
	ps_EMSYSTEM->drbdDstate[1] = EMBASE_DRBD_DSTATE_OTHER;		/* ＤＲＢＤディスク状態（他系）：そのほか */
	ps_EMSYSTEM->drbdCstate = EMBASE_DRBD_CSTATE_OTHER;			/* ＤＲＢＤ接続状態（自系）：そのほか */
	ps_EMSYSTEM->newSysMode = EMBASE_SYSMOD_WAIT;				/* 自系の遷移先モードを準備に設定 */

	newSysMode = -1;											/* 遷移先モードを設定（未設定） */
	parSysMode = EMBASE_SYSMOD_UNKNOW;							/* 他系モードを設定 */
	lifChkSts[0] = EMBASE_LIFCHK_NG;							/* 死活監視状態 */
	lifChkSts[1] = EMBASE_LIFCHK_NG;							/* 死活監視状態 */

	/* 環境設定ファイルから設定値を取り込む */
	func_get_config();
	
	/* ホスト名から自装置を判断する */
	if( ( retCode = gethostname( hostName, sizeof(hostName) ) ) == 0 ) 
	{
		/* １系装置の場合 */
		if( 0 == strcmp( hostName, sysHostName[EMBASE_SYSNUMBER_1] ) )
		{
#ifdef DEBUG_1
printf("%s:func_init(): 自装置ホスト名チェック：１系 [ %s ] \n",EMBASE_OWN_NAME , hostName );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_init(): 自装置ホスト名チェック：１系 [ %s ]", hostName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* ネットワークI/F状態設定 */
			netDevSts[0] = EMBASE_NETDEV_NG;
			netDevSts[1] = EMBASE_NETDEV_NG;
			netDevSts[2] = EMBASE_NETDEV_UN;
			netDevSts[3] = EMBASE_NETDEV_UN;
			/* 装置番号を１系に設定 */
			ps_EMSYSTEM->sysNumber = EMBASE_SYSNUMBER_1;
			/* 自系の装置モードを準備に設定 */
			ps_EMSYSTEM->s_kei[0].sysMode = EMBASE_SYSMOD_WAIT;
			/* １系装置：ネットワークＩ／Ｆ（サービス用ＬＡＮ）：異常 */
			ps_EMSYSTEM->s_kei[0].netDevSts[0] = EMBASE_NETDEV_NG;
			/* １系装置：ネットワークＩ／Ｆ（系列間用ＬＡＮ）  ：異常 */
			ps_EMSYSTEM->s_kei[0].netDevSts[1] = EMBASE_NETDEV_NG;
			/* ２系装置：ネットワークＩ／Ｆ（サービス用ＬＡＮ）：不明 */
			ps_EMSYSTEM->s_kei[1].netDevSts[0] = EMBASE_NETDEV_UN;
			/* ２系装置：ネットワークＩ／Ｆ（系列間用ＬＡＮ）  ：不明 */
			ps_EMSYSTEM->s_kei[1].netDevSts[1] = EMBASE_NETDEV_UN;
			/* 死活監視情報[受信]IPアドレス取得（サービス用ネットワーク） */
			retCode  = func_get_ipaddr( sysHostName[ EMBASE_SYSNUMBER_1 ], &udpRecvIPaddr[EMBASE_NETDEV_SERVICE] );
			/* 死活監視情報[受信]IPアドレス取得（系列間ネットワーク） */
			retCode |= func_get_ipaddr( subHostName[ EMBASE_SYSNUMBER_1 ], &udpRecvIPaddr[EMBASE_NETDEV_INTERNAL] );
			/* 死活監視情報[送信先]IPアドレス取得（サービス用ネットワーク） */
			retCode |= func_get_ipaddr( sysHostName[ EMBASE_SYSNUMBER_2 ], &udpSendIPaddr[EMBASE_NETDEV_SERVICE] );
			/* 死活監視情報[送信先]IPアドレス取得（系列間ネットワーク） */
			retCode |= func_get_ipaddr( subHostName[ EMBASE_SYSNUMBER_2 ], &udpSendIPaddr[EMBASE_NETDEV_INTERNAL] );
			if( retCode != 0 )
			{
				/* ログを出力 */
#ifdef DEBUG_ERR
printf("%s:func_init(): ホスト名からIPアドレスを取得する処理で一部取得できないものがありました\n",EMBASE_OWN_NAME );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_init(): IPアドレス取得異常");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 死活監視情報送受信情報取得エラー */
				return( -1 );
			}
			/* 他系情報 */
			otherTarget = EMBASE_SYSNUMBER_2 - 1;

#ifdef DEBUG_1
printf("%s:func_init(): 死活監視情報[受信](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, sysHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_SERVICE]) );
printf("%s:func_init(): 死活監視情報[受信](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, subHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_INTERNAL]) );
printf("%s:func_init(): 死活監視情報[送信先](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, sysHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_SERVICE]) );
printf("%s:func_init(): 死活監視情報[送信先](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, subHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_INTERNAL]) );
#endif
			sprintf(logMsg, "func_init(): 死活監視情報[受信](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				sysHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_SERVICE]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): 死活監視情報[受信](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				subHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_INTERNAL]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): 死活監視情報[送信先](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				sysHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_SERVICE]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): 死活監視情報[送信先](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				subHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_INTERNAL]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* IPアドレスからネットマスク値を取得 */
			commonNetMask = func_get_netmask( sysHostName[EMBASE_SYSNUMBER_1] );
		}
		/* ２系装置の場合 */
		else if( 0 == strcmp( hostName, sysHostName[EMBASE_SYSNUMBER_2] ) )
		{
#ifdef DEBUG_1
printf("%s:func_init(): 自装置ホスト名チェック：２系 [ %s ] \n", EMBASE_OWN_NAME, hostName );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_init(): 自装置ホスト名チェック：２系 [ %s ]", hostName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			/* ネットワークI/F状態設定 */
			netDevSts[0] = EMBASE_NETDEV_UN;
			netDevSts[1] = EMBASE_NETDEV_UN;
			netDevSts[2] = EMBASE_NETDEV_NG;
			netDevSts[3] = EMBASE_NETDEV_NG;
			/* 装置番号を２系に設定 */
			ps_EMSYSTEM->sysNumber = EMBASE_SYSNUMBER_2;
			/* 自系の装置モードを準備に設定 */
			ps_EMSYSTEM->s_kei[1].sysMode = EMBASE_SYSMOD_WAIT;
			/* １系装置：ネットワークＩ／Ｆ（サービス用ＬＡＮ）：不明 */
			ps_EMSYSTEM->s_kei[0].netDevSts[0] = EMBASE_NETDEV_UN;
			/* １系装置：ネットワークＩ／Ｆ（系列間用ＬＡＮ）  ：不明 */
			ps_EMSYSTEM->s_kei[0].netDevSts[1] = EMBASE_NETDEV_UN;
			/* ２系装置：ネットワークＩ／Ｆ（サービス用ＬＡＮ）：異常 */
			ps_EMSYSTEM->s_kei[1].netDevSts[0] = EMBASE_NETDEV_NG;
			/* ２系装置：ネットワークＩ／Ｆ（系列間用ＬＡＮ）  ：異常 */
			ps_EMSYSTEM->s_kei[1].netDevSts[1] = EMBASE_NETDEV_NG;
			/* 死活監視情報[受信]IPアドレス取得（サービス用ネットワーク） */
			retCode  = func_get_ipaddr( sysHostName[ EMBASE_SYSNUMBER_2 ], &udpRecvIPaddr[EMBASE_NETDEV_SERVICE] );
			/* 死活監視情報[受信]IPアドレス取得（系列間ネットワーク） */
			retCode |= func_get_ipaddr( subHostName[ EMBASE_SYSNUMBER_2 ], &udpRecvIPaddr[EMBASE_NETDEV_INTERNAL] );
			/* 死活監視情報[送信先]IPアドレス取得（サービス用ネットワーク） */
			retCode |= func_get_ipaddr( sysHostName[ EMBASE_SYSNUMBER_1 ], &udpSendIPaddr[EMBASE_NETDEV_SERVICE] );
			/* 死活監視情報[送信先]IPアドレス取得（系列間ネットワーク） */
			retCode |= func_get_ipaddr( subHostName[ EMBASE_SYSNUMBER_1 ], &udpSendIPaddr[EMBASE_NETDEV_INTERNAL] );
			if( retCode != 0 )
			{
#ifdef DEBUG_ERR
printf("%s:func_init(): ホスト名からIPアドレスを取得する処理で一部取得できないものがありました\n",EMBASE_OWN_NAME );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_init(): IPアドレス取得異常");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

				/* 死活監視情報送受信情報取得エラー */
				return( -1 );
			}
			/* 他系情報 */
			otherTarget = EMBASE_SYSNUMBER_1 - 1;

#ifdef DEBUG_1
printf("%s:func_init(): 死活監視情報[受信](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, sysHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_SERVICE]) );
printf("%s:func_init(): 死活監視情報[受信](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, subHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_INTERNAL]) );
printf("%s:func_init(): 死活監視情報[送信先](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, sysHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_SERVICE]) );
printf("%s:func_init(): 死活監視情報[送信先](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]\n",
	EMBASE_OWN_NAME, subHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_INTERNAL]) );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_init(): 死活監視情報[受信](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				sysHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_SERVICE]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): 死活監視情報[受信](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				subHostName[ EMBASE_SYSNUMBER_2 ], ntohl(udpRecvIPaddr[EMBASE_NETDEV_INTERNAL]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): 死活監視情報[送信先](Service Network)  HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				sysHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_SERVICE]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): 死活監視情報[送信先](Internal Network) HOSTNAME[ %s ] IPADDRESS[ %08X ]",
				subHostName[ EMBASE_SYSNUMBER_1 ], ntohl(udpSendIPaddr[EMBASE_NETDEV_INTERNAL]) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* IPアドレスからネットマスク値を取得 */
			commonNetMask = func_get_netmask( sysHostName[EMBASE_SYSNUMBER_2] );
		}
		else
		{
			/* エラー */
#ifdef DEBUG_ERR
printf("%s: func_init(): 自装置ホスト名チェック：異常\n", EMBASE_OWN_NAME );
printf("%s: func_init(): ＯＳが認識しているホスト名[ %s ]が、初期値または設定ファイル定義のホスト名[ %s ][ %s ]と異なります。\n",
	EMBASE_OWN_NAME, hostName, sysHostName[EMBASE_SYSNUMBER_1], sysHostName[EMBASE_SYSNUMBER_2] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_init(): 自装置ホスト名チェック：異常");
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			sprintf(logMsg, "func_init(): ＯＳが認識しているホスト名[ %s ]が、初期値または設定ファイル定義のホスト名[ %s ][ %s ]と異なります。",
				hostName, sysHostName[EMBASE_SYSNUMBER_1], sysHostName[EMBASE_SYSNUMBER_2] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* 装置番号を共通に設定 */
			ps_EMSYSTEM->sysNumber = EMBASE_SYSNUMBER_C;

			return( -1 );
		}

		/* 共通ホスト名が設定されている場合 */
		if( strlen( sysHostName[ EMBASE_SYSNUMBER_C ] ) > 0 )
		{
			/* 共通ホスト名に対応したIPアドレスを取得 */
			retCode = func_get_ipaddr( sysHostName[ EMBASE_SYSNUMBER_C ], &commonIPaddr );
			if( retCode != 0 )
			{
				/* ログを出力 */
#ifdef DEBUG_ERR
printf("%s:func_init(): ホスト名からIPアドレスを取得する処理で一部取得できないものがありました\n",EMBASE_OWN_NAME );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_init(): IPアドレス取得異常 Hostname[%s]", sysHostName[ EMBASE_SYSNUMBER_C ] );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 情報取得エラー */
				return( -1 );
			}
			/* ログを出力 */
			sprintf(logMsg, "func_init(): 共通ホスト情報 HOSTNAME[ %s ] IPADDRESS[ %08X ] NETMASK[ %d ]",
				sysHostName[ EMBASE_SYSNUMBER_C ], ntohl(commonIPaddr), commonNetMask );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
	}
	else
	{
		/* エラー */
#ifdef DEBUG_ERR
printf("%s:func_init(): 自装置ホスト名チェック：エラー [ %d ] \n", EMBASE_OWN_NAME, retCode );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_init(): 自装置ホスト名チェック：エラー [ %d ]", retCode );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		return( -1 );
	}

	/* ログを出力 */
	sprintf(logMsg, "func_init(): 終了");
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IPアドレス取得
 *
 * 機能：
 *		引数 hostName で指定されたホストのIPアドレスを取得し、引数 addr にセット
 *		する
 *		
 * 戻り値：
 *		0:正常 ／ -1:異常
 *
 */
int func_get_ipaddr( char *hostName, in_addr_t *hostIPaddr )
{
	struct hostent		*workHostEnt;					/* ホスト情報 */
	char				ipAddrStr[16];					/* IPアドレス情報 */

	if( ( workHostEnt = gethostbyname( hostName ) ) == NULL )
	{
		/* エラー */
#ifdef DEBUG_ERR
printf("%s:func_get_ipaddr(): ホスト名チェック：エラー Hostname:%s\n", EMBASE_OWN_NAME, hostName );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_get_ipaddr(): ホスト名チェックエラー Hostname:%s", hostName);
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		return( -1 );
	}

	sprintf( ipAddrStr, "%d.%d.%d.%d",
		 (unsigned char)*(workHostEnt->h_addr_list[0]),
		 (unsigned char)*(workHostEnt->h_addr_list[0] + 1),
		 (unsigned char)*(workHostEnt->h_addr_list[0] + 2),
		 (unsigned char)*(workHostEnt->h_addr_list[0] + 3));

	*hostIPaddr = (in_addr_t)inet_addr( ipAddrStr );

	return( 0 );
	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 死活監視情報受信処理
 *
 * 機能：
 *		他系からサービス用ネットワーク経由および系列間ネットワーク経由でUDP送信さ
 *		れる死活監視情報の受信処理を行う。UDPパケットを受信するネットワークI/Fは
 *		引数で指定されたI/Fで行う。
 *		
 * 戻り値：
 *		なし
 */
void* func_lifchk_recv_thread( void* argPoint )
{
	int						netDevNumb = 0;				/* 受信対象デバイス番号 */
	struct st_udpPortInfo	s_udpPortInfo;				/* UDPポート情報 */
	struct sockaddr_in		rcvAddr;					/* UDP受信用インターフェイス情報 */
	struct st_lifChkFrame	*ps_lifChkFrame;			/* 死活監視情報 */

														/* UDP受信バッファ */
	unsigned char			udpRecvBuff[EMSYS_UDP_MAX_BUFF*2];
	fd_set					rfds;						/* ファイルディスクリプタ集合 */
	int						readCnt = 0;				/* データ受信バイト数 */
	struct timeval			timeout;					/* selectタイムアウト設定用 */
	struct sockaddr_in		frmAddr;					/* 送信元情報格納用 */
	socklen_t				sock_len;					/* ソケット長 */
	int						sockYes = 1;				/* ソケットオプション用 */
	int						retCode;					/* 関数戻り値 */
	char					wrkMsg[EMBASE_LOGMSG_SIZE];/* ログメッセージ */

	/* 受信対象を引数から取得 */
	netDevNumb = *((int *)argPoint);

	/* 死活監視情報ポインタ設定 */
	ps_lifChkFrame = (struct st_lifChkFrame *)udpRecvBuff;

	/* UDPポート情報初期化 */
	memset( &s_udpPortInfo, 0, sizeof(s_udpPortInfo) );

#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(%d): 死活監視情報受信スレッド 受信対象[ %s ] 開始\n",
	EMBASE_OWN_NAME, netDevNumb, netDeviceName[ netDevNumb ]);
#endif

	/* ログを出力 */
	sprintf(wrkMsg, "func_lifchk_recv_thread(%d): 死活監視情報受信スレッド 受信対象[ %s ] 開始",
		netDevNumb, netDeviceName[ netDevNumb ]);
	func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

	/* スレッド停止指示があるまで処理を繰返す */
	while( threadStop == 0 )
	{
		/*========*/
		/* Socket */
		/*========*/

		/* ポートがCLOSE状態の場合 */
		if( s_udpPortInfo.openStatus == EMSYS_UDP_STS_CLOSE )
		{
			/* ネットワークデバイス異常中は処理を行わない */
			if( EMBASE_NETDEV_OK != ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].netDevSts[ netDevNumb ] )
			{
				sleep( 1 );
				continue;
			}

			/* socketオープン */
			if( ( s_udpPortInfo.sock_fd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
			{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket Error RET[ %d ] )\n",
	EMBASE_OWN_NAME, netDevNumb, s_udpPortInfo.sock_fd );
#endif
				/* ログを出力 */
				sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket Error RET[ %d ] )",
					netDevNumb, s_udpPortInfo.sock_fd );
				func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

				/* 死活監視異常を設定 */
				ps_EMSYSTEM->lifChkSts[ netDevNumb ] = EMBASE_LIFCHK_NG;

				/* 5秒待ち */
				sleep( 5 );
			}
			else
			{
#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket Open Success FD[ %d ] )\n",
	EMBASE_OWN_NAME, netDevNumb, s_udpPortInfo.sock_fd );
#endif
				/* ソケットオプション設定（address already in use 回避） */
				if( 0 != setsockopt( s_udpPortInfo.sock_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockYes, sizeof(sockYes) ) )
				{
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Setsockopt Error errno[ %d ] )",
						netDevNumb, errno );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket Open Success FD[ %d ] )",
						netDevNumb, s_udpPortInfo.sock_fd );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

					/* ステータスにオープン済みを設定	*/
					s_udpPortInfo.openStatus = EMSYS_UDP_STS_OPEN;
				}
			}
		}

		/*======*/
		/* BIND */
		/*======*/

		/* ポートがオープン済み */
		if( s_udpPortInfo.openStatus == EMSYS_UDP_STS_OPEN )
		{
			/* 初期化 */
			memset( &rcvAddr, 0, sizeof( rcvAddr ) );

			/* アドレスファミリの設定 */
			rcvAddr.sin_family = AF_INET;
			/* UDP受信用ポート番号の設定 */
			rcvAddr.sin_port = htons(udpRecvPort);
			/* UDP受信用IPアドレス */
			rcvAddr.sin_addr.s_addr = udpRecvIPaddr[ netDevNumb ];

#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Bind IPaddr[ %08X ], Port[ %d ])\n",
	EMBASE_OWN_NAME, netDevNumb, ntohl(rcvAddr.sin_addr.s_addr), ntohs(rcvAddr.sin_port) );
#endif
			/* ログを出力 */
			sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Bind IPaddr[ %08X ], Port[ %d ])",
				netDevNumb, ntohl(rcvAddr.sin_addr.s_addr), ntohs(rcvAddr.sin_port) );
			func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

			/* ソケットをバインドする */
			if( bind( s_udpPortInfo.sock_fd, (struct sockaddr *)&rcvAddr, sizeof(rcvAddr) ) != 0 )
			{
				/* BINDエラー */
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (BIND Error errno:%d)\n",
	EMBASE_OWN_NAME, netDevNumb, errno );
#endif
				/* ログを出力 */
				sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (BIND Error errno:%d)",
					netDevNumb, errno );
				func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

				/* ソケットクローズ */
				close( s_udpPortInfo.sock_fd );

				/* ステータスにクローズを設定	*/
				s_udpPortInfo.openStatus = EMSYS_UDP_STS_CLOSE;

				/* 死活監視異常を設定 */
				ps_EMSYSTEM->lifChkSts[ netDevNumb ] = EMBASE_LIFCHK_NG;

				/* 5秒待ち */
				sleep( 5 );
			}
			else
			{
				/* BIND成功 */
#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Bind Success IP:[ %08X ], Port[ %d ])\n",
	EMBASE_OWN_NAME, netDevNumb, ntohl(rcvAddr.sin_addr.s_addr), ntohs(rcvAddr.sin_port) );
#endif
				/* ログを出力 */
				sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Bind Success IP:[ %08X ], Port[ %d ])",
					netDevNumb, ntohl(rcvAddr.sin_addr.s_addr), ntohs(rcvAddr.sin_port) );
				func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

				/* ポート番号を格納 */
				s_udpPortInfo.portNumb = rcvAddr.sin_port;
				
				/* ステータスにバインド済を設定	*/
				s_udpPortInfo.openStatus = EMSYS_UDP_STS_BIND;
			}
		}

		/*==========*/
		/* UDP RECV */
		/*==========*/

		/* ソケットがバインド済の場合 */
		if( s_udpPortInfo.openStatus == EMSYS_UDP_STS_BIND )
		{
			/* データ取得処理 */

			/* ファイルディスクリプタ集合にUDPポートのファイルディスクリプタを登録 */
			FD_ZERO(&rfds);
			FD_SET(s_udpPortInfo.sock_fd, &rfds);

			/* ソケット長の設定 */
			sock_len = sizeof(frmAddr);

			/* selectのタイムアウト時間を設定 */
			timeout.tv_sec  = udpRecvTime;
			timeout.tv_usec = 0;

			/* データを受信するまでselectを使用して待機する */
			retCode = select(s_udpPortInfo.sock_fd + 1, &rfds, NULL, NULL, &timeout);

			/* select異常（ポート異常）の場合 */
			if(retCode == -1)
			{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket select errno:%d)\n",
	EMBASE_OWN_NAME, netDevNumb, errno  );
#endif
				/* ログを出力 */
				sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket select errno:%d)",
					netDevNumb, errno  );
				func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

				/* ポートをクローズ */
				close( s_udpPortInfo.sock_fd );

				/* ステータスにクローズを設定	*/
				s_udpPortInfo.openStatus = EMSYS_UDP_STS_CLOSE;

				/* 状態変化ありの場合 */
				if( EMBASE_LIFCHK_NG != ps_EMSYSTEM->lifChkSts[ netDevNumb ] )
				{
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d 死活監視異常(%s) 発生",
						netDevNumb, netDeviceName[netDevNumb] );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}

				/* 死活監視異常を設定 */
				ps_EMSYSTEM->lifChkSts[ netDevNumb ] = EMBASE_LIFCHK_NG;
				
				/* 5秒待ち */
				sleep( 5 );
			}
			/* selectタイムアウト（データ未受信の場合） */
			else if( retCode == 0)
			{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket select timeout)\n",
	EMBASE_OWN_NAME, netDevNumb );
#endif
				/* ログを出力 */
				sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket select timeout)", netDevNumb );
				func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

				/* 状態変化ありの場合 */
				if( EMBASE_LIFCHK_NG != ps_EMSYSTEM->lifChkSts[ netDevNumb ] )
				{
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d 死活監視異常(%s) 発生",
						netDevNumb, netDeviceName[netDevNumb] );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}

				/* 死活監視異常を設定 */
				ps_EMSYSTEM->lifChkSts[ netDevNumb ] = EMBASE_LIFCHK_NG;
			}
			else
			{

				/* ポートからデータ読み出し */
				readCnt = recvfrom( s_udpPortInfo.sock_fd, udpRecvBuff, 1024, 0, (struct sockaddr *)&frmAddr, &sock_len );
				if( readCnt < 0 )
				{
#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (recvfrom error errno:%d)\n",
	EMBASE_OWN_NAME, netDevNumb, errno );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (recvfrom error errno:%d)\n",
						netDevNumb, errno );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

					/* ログを出力 */
					/* func_udp_logout( PV_UDP_LOG_RECV, precvBuff, readCnt, ps_udpPortInfo, &frmAddr ); */

					/* ポートをクローズ */
					close( s_udpPortInfo.sock_fd );

					/* ステータスにクローズを設定	*/
					s_udpPortInfo.openStatus = EMSYS_UDP_STS_CLOSE;

					/* 状態変化ありの場合 */
					if( EMBASE_LIFCHK_NG != ps_EMSYSTEM->lifChkSts[ netDevNumb ] )
					{
						/* ログを出力 */
						sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d 死活監視異常(%s) 発生",
							netDevNumb, netDeviceName[netDevNumb] );
						func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
					}

					/* 死活監視異常を設定 */
					ps_EMSYSTEM->lifChkSts[ netDevNumb ] = EMBASE_LIFCHK_NG;
				
					/* 5秒待ち */
					sleep( 5 );

				}

				/* 受信データを確認 */

				/* 送信元ポート番号が異なる場合 */
				if( udpSendPort != ntohs( frmAddr.sin_port ) )
				{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Data Source Port Error port:%d)\n",
	EMBASE_OWN_NAME, netDevNumb, ntohs( frmAddr.sin_port ) );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Data Source Port Error port:%d)",
						netDevNumb, ntohs( frmAddr.sin_port ) );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}
				/* 受信データ長が死活監視情報と一致しない場合 */
				else if( readCnt != sizeof(struct st_lifChkFrame) )
				{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Data Length Error length:%d)\n",
	EMBASE_OWN_NAME, netDevNumb, readCnt );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Data Length Error length:%d)",
						netDevNumb, readCnt );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}
				/* 受信データの情報種別が異なる場合 */
				else if( ps_lifChkFrame->dataKind != EMBASE_LIFCHK_DAT_KIND )
				{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Data Kind Error kind:%d)\n",
	EMBASE_OWN_NAME, netDevNumb, ps_lifChkFrame->dataKind );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Data Kind Error kind:%d)",
						netDevNumb, ps_lifChkFrame->dataKind );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}
				else
				{
					/* 状態変化ありの場合 */
					if( EMBASE_LIFCHK_OK != ps_EMSYSTEM->lifChkSts[ netDevNumb ] )
					{
						/* ログを出力 */
						sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d 死活監視異常(%s) 復帰",
							netDevNumb, netDeviceName[netDevNumb] );
						func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
					}

					/* 死活監視正常を設定 */
					ps_EMSYSTEM->lifChkSts[ netDevNumb ] = EMBASE_LIFCHK_OK;

					/* 受信情報が最新の場合 */
					if( ps_EMSYSTEM->recvTime < ps_lifChkFrame->dataTime )
					{
#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Success)\n",
	EMBASE_OWN_NAME, netDevNumb );
#endif

#ifdef DEBUG_1
printf("死活情報受信 Route:%d  モード:%s(%d)  NET S:%s(%d)  INT:%s(%d)\n", netDevNumb,
	s_nameSysMod.nameStr[ ps_lifChkFrame->sysMode ], ps_lifChkFrame->sysMode,
	s_nameNetDev.nameStr[ ps_lifChkFrame->netDevSts[0] ], ps_lifChkFrame->netDevSts[0],
	s_nameNetDev.nameStr[ ps_lifChkFrame->netDevSts[1] ], ps_lifChkFrame->netDevSts[1] );
#endif
						/* ログを出力 */
						sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Success)", netDevNumb );
						func_log_msgset( EMSYS_LOG_LV1, wrkMsg, udpRecvBuff, readCnt );

						/* 受信データ時刻を設定 */
						ps_EMSYSTEM->recvTime = ps_lifChkFrame->dataTime;

						/* 運用モード稼働最終時刻（他系）を設定 */
						ps_EMSYSTEM->operateLastTime[1] = ps_lifChkFrame->operateLastTime;

						/* 運用モード稼働時間（他系）を設定 */
						ps_EMSYSTEM->operateTotalTime[1] = ps_lifChkFrame->operateTotalTime;

						/* 他系装置モードを設定 */
						ps_EMSYSTEM->s_kei[ otherTarget ].sysMode = ps_lifChkFrame->sysMode;
					
						/* 他系ネットワークI/F状態を設定 */
						ps_EMSYSTEM->s_kei[ otherTarget ].netDevSts[0] = ps_lifChkFrame->netDevSts[0];
						ps_EMSYSTEM->s_kei[ otherTarget ].netDevSts[1] = ps_lifChkFrame->netDevSts[1];

						/* 運用モード関連時刻を設定 */
						ps_EMSYSTEM->operateLastTime[1] = ps_lifChkFrame->operateLastTime;
						ps_EMSYSTEM->operateTotalTime[1] = ps_lifChkFrame->operateTotalTime;

						/* 制御依頼が設定されている場合 */
						if( ps_lifChkFrame->ctlKind != 0 )
						{
							/* 制御依頼種別を設定（受信用） */
							ctlKindRecv = ps_lifChkFrame->ctlKind;
						}
					}
					else
					{
						/* 後着データの場合（受信破棄） */
#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Success : LATE ARRIVAL)\n",
	EMBASE_OWN_NAME, netDevNumb );
#endif
						/* ログを出力 */
						sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Receive Success : LATE ARRIVAL)", netDevNumb );
						func_log_msgset( EMSYS_LOG_LV2, wrkMsg, NULL, 0 );
					}
				}
			}
		}
	}

	/* ソケットクローズ */
	if( s_udpPortInfo.openStatus != EMSYS_UDP_STS_CLOSE )
	{
#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket Close)\n",
	EMBASE_OWN_NAME, netDevNumb );
#endif
		/* ログを出力 */
		sprintf(wrkMsg, "func_lifchk_recv_thread(): Life Check (RECV) Route:%d (Socket Close)", netDevNumb );
		func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

		/* ソケットクローズ */
		close( s_udpPortInfo.sock_fd );

		/* ステータスにクローズを設定	*/
		s_udpPortInfo.openStatus = EMSYS_UDP_STS_CLOSE;

	}

#ifdef DEBUG_1
printf("%s:func_lifchk_recv_thread(%d): 死活監視情報受信スレッド 受信対象[ %s ] 終了\n",
	EMBASE_OWN_NAME, netDevNumb, netDeviceName[ netDevNumb ]);
#endif
	/* ログを出力 */
	sprintf(wrkMsg, "func_lifchk_recv_thread(%d): 死活監視情報受信スレッド 受信対象[ %s ] 終了", netDevNumb, netDeviceName[ netDevNumb ]);
	func_log_msgset( EMSYS_LOG_LV1, wrkMsg, NULL, 0 );

	return( NULL );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 死活監視情報送信処理
 *
 * 機能：
 *		死活監視情報をサービス用ネットワーク経由および系列間ネットワーク経由で
 *		UDP送信する。
 *		
 * 戻り値：
 *		なし
 */
void* func_lifchk_send_thread()
{
	struct st_udpPortInfo	s_udpPortInfo[2];			/* UDPポート情報 */
	struct sockaddr_in		srcAddr[2];					/* UDP送信用インターフェイス情報 */
	struct sockaddr_in		sndAddr[2];					/* UDP送信先インターフェイス情報 */
	struct st_lifChkFrame	*ps_lifChkFrame;			/* 死活監視情報 */

														/* UDP送信バッファ */
	unsigned char			udpSendBuff[EMSYS_UDP_MAX_BUFF];
	struct sockaddr_in		frmAddr;					/* 送信元情報格納用 */
	socklen_t				sock_len;					/* ソケット長 */
	int						sockYes = 1;				/* ソケットオプション用 */
	int						retCode;					/* 関数戻り値 */
	int						loopCnt;					/* ループカウンタ */
	char					wrkMsg[EMBASE_LOGMSG_SIZE];/* ログメッセージ */

#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): 死活監視情報送信スレッド 開始\n",
	EMBASE_OWN_NAME);
#endif
	/* ログを出力 */
	sprintf(wrkMsg, "func_lifchk_send_thread(): 死活監視情報送信スレッド 開始");
	func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

	/* UDPポート情報初期化 */
	memset( &s_udpPortInfo, 0, sizeof(s_udpPortInfo) );

	/*-------------------------------*/
	/* UDP送信先インターフェイス設定 */
	/*-------------------------------*/

	/* UDP送信先インターフェイス初期化 */
	memset( &sndAddr[0], 0, sizeof( struct sockaddr_in ) );
	/* UDP送信先インターフェイス初期化 */
	memset( &sndAddr[1], 0, sizeof( struct sockaddr_in ) );

	/* アドレスファミリの設定（サービス用ネットワーク経由） */
	sndAddr[0].sin_family = AF_INET;
	/* UDP送信先ポート番号の設定（サービス用ネットワーク経由） */
	sndAddr[0].sin_port = htons(udpRecvPort);
	/* UDP送信先IPアドレス（サービス用ネットワーク経由） */
	sndAddr[0].sin_addr.s_addr = udpSendIPaddr[0];

	/* アドレスファミリの設定（系列間用ネットワーク経由） */
	sndAddr[1].sin_family = AF_INET;
	/* UDP送信先ポート番号の設定（系列間用ネットワーク経由） */
	sndAddr[1].sin_port = htons(udpRecvPort);
	/* UDP送信先IPアドレス（系列間用ネットワーク経由） */
	sndAddr[1].sin_addr.s_addr = udpSendIPaddr[1];

	/* スレッド停止指示があるまで１秒ごとに処理を繰返す */
	while( threadStop == 0 )
	{
		/* 送信バッファ初期化 */
		memset( udpSendBuff, 0, sizeof( udpSendBuff ) );

		/* 死活監視情報ポインタ設定 */
		ps_lifChkFrame = (struct st_lifChkFrame *)udpSendBuff;
		
		/* 死活監視情報の設定 */
		ps_lifChkFrame->dataKind         = EMBASE_LIFCHK_DAT_KIND;
		ps_lifChkFrame->dataTime         = time( NULL );
		ps_lifChkFrame->sysNumber        = ps_EMSYSTEM->sysNumber;
		ps_lifChkFrame->sysMode          = ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode;
		ps_lifChkFrame->netDevSts[0]     = ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].netDevSts[0];
		ps_lifChkFrame->netDevSts[1]     = ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].netDevSts[1];
		ps_lifChkFrame->operateLastTime  = ps_EMSYSTEM->operateLastTime[0];
		ps_lifChkFrame->operateTotalTime = ps_EMSYSTEM->operateTotalTime[0];
		/* 制御依頼種別（送信用）が設定されている場合 */
		if( ctlKindSend )
		{
			/* 制御依頼種別を設定 */
			ps_lifChkFrame->ctlKind = ctlKindSend;
			/* 制御依頼種別をリセット */
			ctlKindSend = 0;
		}
		else
		{
			/* 制御依頼種別を設定 */
			ps_lifChkFrame->ctlKind = 0;
		}

#ifdef DEBUG_1
printf("%s:func_lichk_send_thread(): 自装置状態 モード:%s(%d)  NET S:%s(%d)  INT:%s(%d)\n",
	s_nameSysMod.nameStr[ ps_lifChkFrame->sysMode ], ps_lifChkFrame->sysMode,
	s_nameNetDev.nameStr[ ps_lifChkFrame->netDevSts[0] ], ps_lifChkFrame->netDevSts[0],
	s_nameNetDev.nameStr[ ps_lifChkFrame->netDevSts[1] ], ps_lifChkFrame->netDevSts[1] );
#endif
		/* 送信データをログ出力 */
		func_log_msgset( EMSYS_LOG_LV1, "func_lifchk_send_thread(): Life Check (SEND) Data", udpSendBuff, sizeof( struct st_lifChkFrame ) );

		/* ２ルート分処理 */
		for( loopCnt = 0 ; loopCnt < 2 ; loopCnt++ )
		{
			/*========*/
			/* Socket */
			/*========*/

			/* ポートがCLOSE状態の場合 */
			if( s_udpPortInfo[loopCnt].openStatus == EMSYS_UDP_STS_CLOSE )
			{
				/* ネットワークデバイス異常中は処理を行わない */
				if( EMBASE_NETDEV_OK != ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].netDevSts[ loopCnt ] )
					continue;

				/* socketオープン */
				if( ( s_udpPortInfo[loopCnt].sock_fd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 )
				{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:%d (Socket Error RET[ %d ] )\n",
	EMBASE_OWN_NAME, loopCnt, s_udpPortInfo[loopCnt].sock_fd );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d (Socket Error RET[ %d ] )",
						loopCnt, s_udpPortInfo[loopCnt].sock_fd );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
				}
				else
				{
					/* ソケットオプション設定（address already in use 回避） */
					if( 0 != setsockopt( s_udpPortInfo[loopCnt].sock_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockYes, sizeof(sockYes) ) )
					{
						/* ログを出力 */
						sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d (Setsockopt Error errno[ %d ] )",
							loopCnt, errno );
						func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

						/* ソケットクローズ */
						close( s_udpPortInfo[loopCnt].sock_fd );
					}
					else
					{
						/* ステータスにオープン済みを設定	*/
						s_udpPortInfo[loopCnt].openStatus = EMSYS_UDP_STS_OPEN;

#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:%d (Socket Open Success FD[ %d ] )\n",
	EMBASE_OWN_NAME, loopCnt, s_udpPortInfo[loopCnt].sock_fd );
#endif
						/* ログを出力 */
						sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d (Socket Open Success FD[ %d ] )",
							loopCnt, s_udpPortInfo[loopCnt].sock_fd );
						func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );
					}
				}
			}

			/*======*/
			/* BIND */
			/*======*/

			/* ポートがオープン済み */
			if( s_udpPortInfo[loopCnt].openStatus == EMSYS_UDP_STS_OPEN )
			{
				/* 初期化 */
				memset( &srcAddr[loopCnt], 0, sizeof( struct sockaddr_in ) );

				/* アドレスファミリの設定 */
				srcAddr[loopCnt].sin_family = AF_INET;
				/* UDP送信用ポート番号の設定 */
				srcAddr[loopCnt].sin_port = htons(udpSendPort);
				/* UDP送信元用IPアドレス */
				srcAddr[loopCnt].sin_addr.s_addr = udpRecvIPaddr[ loopCnt ];

				/* ソケットをバインドする */
				if( bind( s_udpPortInfo[loopCnt].sock_fd, (struct sockaddr *)&srcAddr[loopCnt], sizeof(struct sockaddr_in) ) != 0 )
				{
					/* BINDエラー */
#ifdef DEBUG_ERR
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:%d (BIND Error errno:%d)\n",
	EMBASE_OWN_NAME, loopCnt, errno );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d (BIND Error errno:%d)", loopCnt, errno );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

					/* ソケットクローズ */
					close( s_udpPortInfo[loopCnt].sock_fd );

					/* ステータスにクローズを設定	*/
					s_udpPortInfo[loopCnt].openStatus = EMSYS_UDP_STS_CLOSE;
				}
				else
				{
					/* BIND成功 */
#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:%d (Bind Success IP:[ %08X ], Port[ %d ])\n",
	EMBASE_OWN_NAME, loopCnt, ntohl(srcAddr[loopCnt].sin_addr.s_addr), ntohs(srcAddr[loopCnt].sin_port) );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d (Bind Success IP:[ %08X ], Port[ %d ])",
						loopCnt, ntohl(srcAddr[loopCnt].sin_addr.s_addr), ntohs(srcAddr[loopCnt].sin_port) );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

					/* ポート番号を格納 */
					s_udpPortInfo[loopCnt].portNumb = srcAddr[loopCnt].sin_port;
				
					/* ステータスにバインド済を設定	*/
					s_udpPortInfo[loopCnt].openStatus = EMSYS_UDP_STS_BIND;
				}
			}

			/*==========*/
			/* UDP SNED */
			/*==========*/

			/* ソケットがバインド済の場合 */
			if( s_udpPortInfo[loopCnt].openStatus == EMSYS_UDP_STS_BIND )
			{
				/* データを送信する */
				retCode = sendto( s_udpPortInfo[loopCnt].sock_fd,
				                  udpSendBuff,
				                  sizeof(struct st_lifChkFrame),
				                  0,
				                  (struct sockaddr *)&sndAddr[loopCnt],
				                  sizeof( struct sockaddr_in) );

				/* 送信エラーの場合 */
				if( retCode <= 0 )
				{
#ifdef DEBUG_ERR
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:%d (sendto error errno:%d)\n",
	EMBASE_OWN_NAME, loopCnt, errno );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d (sendto error errno:%d)", loopCnt, errno );
					func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

					/* ポートをクローズ */
					close( s_udpPortInfo[loopCnt].sock_fd );

					/* ステータスにクローズを設定	*/
					s_udpPortInfo[loopCnt].openStatus = EMSYS_UDP_STS_CLOSE;
				}
				else
				{
					/* 送信成功 */
#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:%d (Sendto SRC:[ %08X:%d ] ---> DST[ %08X:%d ])\n",
	EMBASE_OWN_NAME, loopCnt,
	ntohl(srcAddr[loopCnt].sin_addr.s_addr), ntohs(srcAddr[loopCnt].sin_port),
	ntohl(sndAddr[loopCnt].sin_addr.s_addr), ntohs(sndAddr[loopCnt].sin_port) );
#endif
					/* ログを出力 */
					sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:%d Len:%d (Sendto SRC:[ %08X:%d ] -> DST[ %08X:%d ])",
						loopCnt, retCode,
						ntohl(srcAddr[loopCnt].sin_addr.s_addr), ntohs(srcAddr[loopCnt].sin_port),
						ntohl(sndAddr[loopCnt].sin_addr.s_addr), ntohs(sndAddr[loopCnt].sin_port) );
					func_log_msgset( EMSYS_LOG_LV1, wrkMsg, NULL, 0 );
				}
			}
		}

		/* 待機 */
		sleep( udpSendTime );
	}

	/* ソケットクローズ */
	if( s_udpPortInfo[0].openStatus != EMSYS_UDP_STS_CLOSE )
	{
#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:0 (Socket Close)\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:0 (Socket Close)");
		func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

		/* ソケットクローズ */
		close( s_udpPortInfo[0].sock_fd );

		/* ステータスにクローズを設定	*/
		s_udpPortInfo[0].openStatus = EMSYS_UDP_STS_CLOSE;
	}
	/* ソケットクローズ */
	if( s_udpPortInfo[1].openStatus != EMSYS_UDP_STS_CLOSE )
	{
#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): Life Check (SEND) Route:1 (Socket Close)\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(wrkMsg, "func_lifchk_send_thread(): Life Check (SEND) Route:1 (Socket Close)");
		func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

		/* ソケットクローズ */
		close( s_udpPortInfo[1].sock_fd );

		/* ステータスにクローズを設定	*/
		s_udpPortInfo[1].openStatus = EMSYS_UDP_STS_CLOSE;
	}

#ifdef DEBUG_1
printf("%s:func_lifchk_send_thread(): 死活監視情報送信スレッド 終了\n", EMBASE_OWN_NAME);
#endif
	/* ログを出力 */
	sprintf(wrkMsg, "func_lifchk_send_thread(): 死活監視情報送信スレッド 終了");
	func_log_msgset( EMSYS_LOG_LV3, wrkMsg, NULL, 0 );

	return( NULL );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ネットワークＩ／Ｆの状態取得
 *
 * 機能：
 *		nmcli コマンドを実行してネットワークＩ／Ｆの状態（正常／異常）を判定する。
 *		
 * 戻り値：
 * 		カンマで区切られたデー多数を返す。
 */
int func_chk_netDev()
{
	FILE				*fp;						/* ファイルポインタ */
	char				checkData[1024];			/* チェック用文字列 */
	char				linkCheck[1024];			/* チェック用文字列 *//* [20200713] Add */
	int					sepCount;					/* セパレータデータ数 */
	int					workStat;					/* ステータス（ワーク用） */
	int					retCode;					/* 戻り値 *//* [20200713] Add */
	struct st_sepStr	s_sepStr;					/* 分割データ格納領域 */

	if( (fp = popen("LANG=C /bin/nmcli device 2> /dev/null", "r")) == NULL )
	{
		/* エラー */

#ifdef DEBUG_ERR
printf("%s:func_chk_netDev(): ネットワークデバイス状態確認コマンド失敗\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス状態確認コマンド失敗");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		/* ネットワークＩ／Ｆ（サービス用）の状態を「異常」に設定 */
		ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE]  = EMBASE_NETDEV_NG;
		/* ネットワークＩ／Ｆ（系列間用）の状態を「異常」に設定 */
		ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] = EMBASE_NETDEV_NG;

		return( -1 );
	}
	else
	{
		/* 出力がなくなるまで繰り返す */
		while( fgets( checkData, 512, fp ) != NULL )
		{
			/* 出力内容をスペースで区切る（空白が連続しても一つの区切りとする） */
			sepCount = func_sep_data( checkData, ' ', EMSYS_SEP_FLG_CONTIN, &s_sepStr );

			/* ネットワークＩ／Ｆ（サービス用）の確認 */
			if( 0 == strcmp( s_sepStr.data[0], netDeviceName[EMBASE_NETDEV_SERVICE] ) )
			{
				/* ４項目目にデバイス名がある場合は正常（--ではない場合） */
				if( 0 == strcmp( s_sepStr.data[0], s_sepStr.data[3] ) )
				{
/* [20200713] Add St */
					/* （デバイス状態正常） */

					/* ケーブルLINK確認 */
					sprintf( linkCheck, "/usr/sbin/ip link | grep %s | grep NO-CARRIER &> /dev/null", netDeviceName[EMBASE_NETDEV_SERVICE] );
					retCode = system( linkCheck );

					/* 正常:LINK状態の場合 */
					if( 0 < retCode )
					{
/* [20200713] Add Ed */
						/* 状態変化ありの場合 */
						if( EMBASE_NETDEV_OK != ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE] )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(サービス用:%s) 正常復帰", s_sepStr.data[0]);
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
					
						/* ネットワークＩ／Ｆ（サービス用）の状態を[ 正常 ]に設定 */
						ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE] = EMBASE_NETDEV_OK;
/* [20200713] Add St */
					}
					/* 異常：NO-CARRIERの場合 */
					else if( 0 == retCode )
					{
						/* 状態変化ありの場合 */
						if( EMBASE_NETDEV_NG != ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE] )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(サービス用:%s) 異常発生", s_sepStr.data[0]);
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
					
						/* ネットワークＩ／Ｆ（サービス用）の状態を「異常」に設定 */
						ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE]  = EMBASE_NETDEV_NG;
					}
/* [20200713] Add Ed */
				}
				else
				{
					/* 異常 */
					/* 状態変化ありの場合 */
					if( EMBASE_NETDEV_NG != ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE] )
					{
						/* ログを出力 */
						sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(サービス用:%s) 異常発生", s_sepStr.data[0]);
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					}
					
					/* ネットワークＩ／Ｆ（サービス用）の状態を「異常」に設定 */
					ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_SERVICE]  = EMBASE_NETDEV_NG;
				}
			}
			/* ネットワークＩ／Ｆ（系列間用）の確認 */
			else if( 0 == strcmp( s_sepStr.data[0], netDeviceName[EMBASE_NETDEV_INTERNAL] ) )
			{
				/* ４項目目にデバイス名がある場合は正常（--ではない場合） */
				if( 0 == strcmp( s_sepStr.data[0], s_sepStr.data[3]) )
				{
/* [20200713] Add St */
					/* （デバイス状態正常） */

					/* ケーブルLINK確認 */
					sprintf( linkCheck, "/usr/sbin/ip link | grep %s | grep NO-CARRIER &> /dev/null", netDeviceName[EMBASE_NETDEV_INTERNAL] );
					retCode = system( linkCheck );

					/* 正常:LINK状態の場合 */
					if( 0 < retCode )
					{
/* [20200713] Add Ed */
						/* 状態変化ありの場合 */
						if( EMBASE_NETDEV_OK != ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(系列間用:%s) 正常復帰", s_sepStr.data[0]);
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
					
						/* ネットワークＩ／Ｆ（系列間用）の状態を[ 正常 ]に設定 */
						ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] = EMBASE_NETDEV_OK;
/* [20200713] Add St */
					}
					/* 異常：NO-CARRIERの場合 */
					else if( 0 == retCode )
					{
						/* 状態変化ありの場合 */
						if( EMBASE_NETDEV_NG != ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(系列間用:%s) 異常発生", s_sepStr.data[0]);
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
					
						/* ネットワークＩ／Ｆ（系列間用）の状態を「異常」に設定 */
						ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] = EMBASE_NETDEV_NG;
					}
/* [20200713] Add Ed */
				}
				else
				{
/* [20250723] Add St */
					/* （ネットワークＩ／Ｆが利用不可：ネットワークケーブル未接続あるいはネットワークケーブル先機器停止状態の場合） */
					if(  0 == strcmp( "unavailable", s_sepStr.data[2]) )
					{
						/* ネットワークＩ／Ｆのアクティベーションを実施 */
						sprintf( linkCheck, "/bin/nmcli con up %s &> /dev/null", netDeviceName[EMBASE_NETDEV_INTERNAL] );
						retCode = system( linkCheck );

						/* ログを出力 */
						sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(系列間用:%s) アクティベーション実施", s_sepStr.data[0]);
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

						/* 正常の場合 */
						if( 0 == retCode )
						{
							/* ログを出力 */
							sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(系列間用:%s) アクティベーション成功", s_sepStr.data[0]);
							func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
						}
						
					}
/* [20250723] Add Ed */
					/* 状態変化ありの場合 */
					if( EMBASE_NETDEV_NG != ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] )
					{
						/* ログを出力 */
						sprintf(logMsg, "func_chk_netDev(): ネットワークデバイス(系列間用:%s) 異常発生", s_sepStr.data[0]);
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					}
					
					/* ネットワークＩ／Ｆ（系列間用）の状態を「異常」に設定 */
					ps_EMSYSTEM->s_kei[ps_EMSYSTEM->sysNumber - 1].netDevSts[EMBASE_NETDEV_INTERNAL] = EMBASE_NETDEV_NG;
				}
			}
		}
		
	}
	/* ファイルオープン中の場合 */
	if( fp != NULL )
	{
		/* ファイルクローズ */
		fclose( fp );
	}
	
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＤＲＢＤの状態取得
 *
 * 機能：
 *		drbdadm コマンドを実行してＤＲＢＤの状態（ロール、ディスク状態、接続状態）
 *		を判定する。
 *		
 * 戻り値：
 *		0    ：正常
 *		0以外：異常
 */
int func_chk_drbd()
{
	FILE				*fp;						/* ファイルポインタ */
	char				checkCmd[1024];				/* チェックコマンド */
	char				checkData[1024];			/* チェック用文字列 */
	int					sepCount;					/* セパレータデータ数 */
	int					retCode;					/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */
	int					loopCnt2;					/* ループカウンタ */
	struct st_sepStr	s_sepStr;					/* 分割データ格納領域 */

	/* ================================ */
	/* ＤＲＢＤサービス動作状態チェック */
	/* ================================ */
	/* チェックカウンタリセット */
	drbdCheckCnt++;
	/* カウンタが確認時間に到達した場合 */
	if( drbdCheckCnt > ( drbdCheckTime * 60 ) )
	{
		/* チェックカウンタリセット */
		drbdCheckCnt = 0;

		/* 動作状態確認コマンド */
		sprintf( checkCmd, "LANG=C systemctl | grep drbd.service | grep failed  &> /dev/null" );
		retCode = system( checkCmd );

		/* ＤＲＢＤサービス異常の場合（failedがある場合） */
		if( retCode == 0 )
		{
			/* ログを出力 */
			sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤサービス停止中を検出！ サービス起動コマンド実施" );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

			/* ＤＲＢＤサービスの起動を試みる */
			sprintf( checkCmd, "systemctl start drbd.service &> /dev/null &" );
			retCode = system( checkCmd );
		}
	}

	/* ========================== */
	/* ＤＲＢＤロール状態チェック */
	/* ========================== */
	/* コマンド設定（ロール） */
	sprintf( checkCmd, "LANG=C /usr/sbin/drbdadm role %s 2> /dev/null", drbdResourceName );

	if( (fp = popen(checkCmd, "r")) == NULL )
	{
		/* エラー */

#ifdef DEBUG_ERR
printf("%s:func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗（role %s)\n", EMBASE_OWN_NAME, drbdResourceName );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗（role %s）",drbdResourceName );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		/*ＤＲＢＤロール（自系）の状態を「その他」に設定 */
		ps_EMSYSTEM->drbdRole[ 0 ]  = EMBASE_DRBD_ROLE_OTHER;
		/*ＤＲＢＤロール（他系）の状態を「その他」に設定 */
		ps_EMSYSTEM->drbdRole[ 1 ]  = EMBASE_DRBD_ROLE_OTHER;
	}
	else
	{
		/* 出力を取得 */
		if( fgets( checkData, 512, fp ) != NULL )
		{
			/* 出力内容を'/'で区切る */
			sepCount = func_sep_data( checkData, '/', EMSYS_SEP_FLG_SPCSKP, &s_sepStr );

			/* 自系／他系の２情報の場合 */
			if( sepCount ==2 )
			{
				/* 自系と他系の２回分実施 */
				for( loopCnt1 = 0 ; loopCnt1 < 2 ; loopCnt1++ )
				{
					/* ロール状態の確認 */
					for( loopCnt2 = 0 ; s_drbdRoleTable[ loopCnt2 ].Numb != EMBASE_DRBD_STOPPER ; loopCnt2++ )
					{
						if( s_drbdRoleTable[ loopCnt2 ].Numb == EMBASE_DRBD_ROLE_OTHER )
						{
							/* 状態をその他に設定 */
							ps_EMSYSTEM->drbdRole[ loopCnt1 ] = EMBASE_DRBD_ROLE_OTHER;
							break;
						}
						/* 状態名称が定義テーブルと一致した場合 */
						if( 0 == strcmp( s_sepStr.data[ loopCnt1 ], s_drbdRoleTable[ loopCnt2 ].Name ) )
						{
							/* 一致した状態を設定 */
							ps_EMSYSTEM->drbdRole[ loopCnt1 ] = s_drbdRoleTable[ loopCnt2 ].Numb;
							break;
						}
					}
				}
			}
		}
		else
		{
#ifdef DEBUG_ERR
printf("%s:func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗 リソース未接続（role %s) ＤＲＢＤサービスチェックカウンタ（%4d／%4d）\n", EMBASE_OWN_NAME, drbdResourceName, drbdCheckCnt, ( drbdCheckTime * 60 ) );
#endif
			/*ＤＲＢＤロール（自系）の状態を「その他」に設定 */
			ps_EMSYSTEM->drbdRole[ 0 ]  = EMBASE_DRBD_ROLE_OTHER;
			/*ＤＲＢＤロール（他系）の状態を「その他」に設定 */
			ps_EMSYSTEM->drbdRole[ 1 ]  = EMBASE_DRBD_ROLE_OTHER;

			/* ログを出力 */
			sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗 リソース未接続（role %s） ＤＲＢＤサービスチェックカウンタ（%4d／%4d）",drbdResourceName, drbdCheckCnt, ( drbdCheckTime * 60 ) );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* ファイルクローズ */
		fclose( fp );
	}

	/* ============================ */
	/* ＤＲＢＤディスク状態チェック */
	/* ============================ */
	/* コマンド設定（ディスク） */
	sprintf( checkCmd, "LANG=C /usr/sbin/drbdadm dstate %s 2> /dev/null", drbdResourceName );

	if( (fp = popen(checkCmd, "r")) == NULL )
	{
		/* エラー */

#ifdef DEBUG_ERR
printf("%s:func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗（dstate %s)\n", EMBASE_OWN_NAME, drbdResourceName );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗（dstate %s）",drbdResourceName );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		/*ＤＲＢＤディスク（自系）の状態を「その他」に設定 */
		ps_EMSYSTEM->drbdDstate[ 0 ]  = EMBASE_DRBD_DSTATE_OTHER;
		/*ＤＲＢＤディスク（他系）の状態を「その他」に設定 */
		ps_EMSYSTEM->drbdDstate[ 1 ]  = EMBASE_DRBD_DSTATE_OTHER;
	}
	else
	{
		/* 出力を取得 */
		if( fgets( checkData, 512, fp ) != NULL )
		{
			/* 出力内容を'/'で区切る */
			sepCount = func_sep_data( checkData, '/', EMSYS_SEP_FLG_SPCSKP, &s_sepStr );

			/* 自系／他系の２情報の場合 */
			if( sepCount ==2 )
			{
				/* 自系と他系の２回分実施 */
				for( loopCnt1 = 0 ; loopCnt1 < 2 ; loopCnt1++ )
				{
					/* ディスク状態の確認 */
					for( loopCnt2 = 0 ; s_drbdDstateTable[ loopCnt2 ].Numb != EMBASE_DRBD_STOPPER ; loopCnt2++ )
					{
						if( s_drbdDstateTable[ loopCnt2 ].Numb == EMBASE_DRBD_DSTATE_OTHER )
						{
							/* 状態をその他に設定 */
							ps_EMSYSTEM->drbdDstate[ loopCnt1 ] = EMBASE_DRBD_DSTATE_OTHER;
							break;
						}
						/* 状態名称が定義テーブルと一致した場合 */
						if( 0 == strcmp( s_sepStr.data[ loopCnt1 ], s_drbdDstateTable[ loopCnt2 ].Name ) )
						{
							/* 一致した状態を設定 */
							ps_EMSYSTEM->drbdDstate[ loopCnt1 ] = s_drbdDstateTable[ loopCnt2 ].Numb;
							break;
						}
					}
				}
			}
		}
		else
		{
#ifdef DEBUG_ERR
printf("%s:func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗 リソース未接続（role %s)\n", EMBASE_OWN_NAME, drbdResourceName );
#endif
			/*ＤＲＢＤディスク（自系）の状態を「その他」に設定 */
			ps_EMSYSTEM->drbdDstate[ 0 ]  = EMBASE_DRBD_DSTATE_OTHER;
			/*ＤＲＢＤディスク（他系）の状態を「その他」に設定 */
			ps_EMSYSTEM->drbdDstate[ 1 ]  = EMBASE_DRBD_DSTATE_OTHER;

			/* ログを出力 */
			sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗 リソース未接続（role %s）",drbdResourceName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* ファイルクローズ */
		fclose( fp );
	}

	/* ============================ */
	/* ＤＲＢＤ接続状態チェック */
	/* ============================ */
	/* コマンド設定（接続） */
	sprintf( checkCmd, "LANG=C /usr/sbin/drbdadm cstate %s 2> /dev/null", drbdResourceName );

	if( (fp = popen(checkCmd, "r")) == NULL )
	{
		/* エラー */

#ifdef DEBUG_ERR
printf("%s:func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗（cstate %s)\n", EMBASE_OWN_NAME, drbdResourceName );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗（cstate %s）",drbdResourceName );
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		/*ＤＲＢＤ接続の状態を「その他」に設定 */
		ps_EMSYSTEM->drbdCstate  = EMBASE_DRBD_CSTATE_OTHER;
	}
	else
	{
		/* 出力を取得 */
		if( fgets( checkData, 512, fp ) != NULL )
		{
			/* 出力内容を'/'で区切る */
			sepCount = func_sep_data( checkData, '/', EMSYS_SEP_FLG_SPCSKP, &s_sepStr );

			/* 接続状態の確認 */
			for( loopCnt2 = 0 ; s_drbdCstateTable[ loopCnt2 ].Numb != EMBASE_DRBD_STOPPER ; loopCnt2++ )
			{
				if( s_drbdCstateTable[ loopCnt2 ].Numb == EMBASE_DRBD_CSTATE_OTHER )
				{
					/* 状態をその他に設定 */
					ps_EMSYSTEM->drbdCstate = EMBASE_DRBD_CSTATE_OTHER;
					break;
				}
				/* 状態名称が定義テーブルと一致した場合 */
				if( 0 == strcmp( s_sepStr.data[ 0 ], s_drbdCstateTable[ loopCnt2 ].Name ) )
				{
					/* 一致した状態を設定 */
					ps_EMSYSTEM->drbdCstate = s_drbdCstateTable[ loopCnt2 ].Numb;
					break;
				}
			}
		}
		else
		{
#ifdef DEBUG_ERR
printf("%s:func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗 リソース未接続（role %s)\n", EMBASE_OWN_NAME, drbdResourceName );
#endif
			/*ＤＲＢＤ接続の状態を「その他」に設定 */
			ps_EMSYSTEM->drbdCstate  = EMBASE_DRBD_CSTATE_OTHER;

			/* ログを出力 */
			sprintf(logMsg, "func_chk_drbd(): ＤＲＢＤ状態確認コマンド失敗 リソース未接続（role %s）",drbdResourceName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* ファイルクローズ */
		fclose( fp );
	}
	
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 動作監視対象プロセス動作確認
 *
 * 機能：
 * 		設定ファイルで指定されたプロセス（最大２つまで）が全て動作していることを
 *		確認する。
 * 戻り値：
 * 		 0: 動作監視対象プロセス指定なし、または、動作監視対象プロセスは全て動作
 *		   している。
 *		-1: 動作監視対象プロセスが少なくとも１つ以上動作していない場合
 */
int func_chk_oper_proc()
{
	char					cmdStr[1024];					/* コマンド */

	/* 監視対象プロセスが設定されている場合 */
	if( ( 0 != strlen( appRuncheckName[0] ) ) || ( 0 != strlen( appRuncheckName[1] ) ) )
	{
		/* 動作監視対象プロセス１確認 */
		if( 0 != strlen(appRuncheckName[0]) )
		{
			/* 監視対象プロセスが動作しているか確認するコマンドを実行 */
			sprintf( cmdStr, "/usr/bin/pgrep -o %s > /dev/null 2>&1", appRuncheckName[0] );
			if( 0 != system( cmdStr ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_chk_oper_proc(): 動作監視対象プロセス1 [ %s ] 動作未確認", appRuncheckName[0] );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 動作監視対象プロセス未動作 */
				/* 異常値を返す */
				return( -1 );
			}
		}
		/* 動作監視対象プロセス２確認 */
		if( 0 != strlen(appRuncheckName[1]) )
		{
			/* 監視対象プロセスが動作しているか確認するコマンドを実行 */
			sprintf( cmdStr, "/usr/bin/pgrep -o %s > /dev/null 2>&1", appRuncheckName[1] );
			if( 0 != system( cmdStr ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_chk_oper_proc(): 動作監視対象プロセス2 [ %s ] 動作未確認", appRuncheckName[1] );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 動作監視対象プロセス未動作 */
				/* 異常値を返す */
				return( -1 );
			}
		}
		/* 動作監視対象プロセスは全て動作している */
		/* 正常を返す */
		return( 0 );
	}
	else
	{
		/* 動作監視対象プロセス指定なし */
		/* 正常を返す */
		return( 0 );
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 動作監視対象プロセス停止中確認
 *
 * 機能：
 * 		設定ファイルで指定されたプロセス（最大２つまで）が動作していないことを
 *		確認する。
 * 戻り値：
 * 		 0: 動作監視対象プロセス指定なし、または、動作監視対象プロセスは全て停止
 *		   している。
 *		-1: 動作監視対象プロセスが少なくとも１つ以上動作している場合
 */
int func_chk_stop_proc()
{
	char					cmdStr[1024];					/* コマンド */

	/* 監視対象プロセスが設定されている場合 */
	if( ( 0 != strlen( appRuncheckName[0] ) ) || ( 0 != strlen( appRuncheckName[1] ) ) )
	{
		/* 動作監視対象プロセス１確認 */
		if( 0 != strlen(appRuncheckName[0]) )
		{
			/* 監視対象プロセスが動作しているか確認するコマンドを実行 */
			sprintf( cmdStr, "/usr/bin/pgrep -o %s > /dev/null 2>&1", appRuncheckName[0] );
			if( 0 == system( cmdStr ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_chk_oper_proc(): 動作監視対象プロセス1 [ %s ] 動作中", appRuncheckName[0] );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 動作監視対象プロセス動作中 */
				/* 異常値を返す */
				return( -1 );
			}
		}
		/* 動作監視対象プロセス２確認 */
		if( 0 != strlen(appRuncheckName[1]) )
		{
			/* 監視対象プロセスが動作しているか確認するコマンドを実行 */
			sprintf( cmdStr, "/usr/bin/pgrep -o %s > /dev/null 2>&1", appRuncheckName[1] );
			if( 0 == system( cmdStr ) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_chk_oper_proc(): 動作監視対象プロセス2 [ %s ] 動作中", appRuncheckName[1] );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				/* 動作監視対象プロセス動作中 */
				/* 異常値を返す */
				return( -1 );
			}
		}
		/* 動作監視対象プロセスは全て停止している */
		/* 正常を返す */
		return( 0 );
	}
	else
	{
		/* 動作監視対象プロセス指定なし */
		/* 正常を返す */
		return( 0 );
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＤＲＢＤ状態復旧処理
 *
 * 機能：
 *		ＤＲＢＤの状態が同期中以外の場合に自動復旧を試みる
 *
 * 戻り値：
 *		なし
 *
 */
void func_drbd_recovery()
{
	int						workMode;					/* ワークモード */
	int						retCode;					/* 戻り値 */
	char					cmdStr[1024];				/* コマンド用ワーク */

	/* リカバリカウンタが0以外の場合は処理を行わない */
	if( 0 == drbdRecoveryCnt )
	{
		/* 運用モード（非遷移状態） */
		if( (EMBASE_SYSMOD_OPERAT == ps_EMSYSTEM->s_kei[ ps_EMSYSTEM->sysNumber - 1 ].sysMode ) &&
		    (newSysMode == -1) )
		{
			/* 自系のＤＲＢＤロール状態がプライマリ、かつ */
			/* 他系のＤＲＢＤロール状態がセカンダリ以外、かつ */
			/* ＤＲＢＤ接続状態が StandAlone の場合 */
			if( (EMBASE_DRBD_ROLE_PRIMARY == ps_EMSYSTEM->drbdRole[0]) &&
			    (EMBASE_DRBD_ROLE_SECONDARY != ps_EMSYSTEM->drbdRole[1] ) &&
			    (EMBASE_DRBD_CSTATE_STANDALONE == ps_EMSYSTEM->drbdCstate) )
			{
				/* ログを出力 */
				sprintf(logMsg, "func_drbd_recovery(): 運用モード（非遷移状態）：他系ロール状態がアンノウンかつ接続状態がスタンドアローンのため、ＤＲＢＤ接続を実施");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	
				/* ＤＲＢＤ接続処理の実施 */
				sprintf( cmdStr, "/usr/sbin/drbdadm connect %s 2> /dev/null", drbdResourceName );
				if( (retCode = system( cmdStr )) != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_drbd_recovery(): ＤＲＢＤ復旧コマンド drbdadm connect %s 失敗 ret[ %d ] err[ %d ]",
						drbdResourceName, retCode, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_drbd_recovery(): ＤＲＢＤ復旧コマンド drbdadm connect %s 成功 ret[ %d ] err[ %d ]",
						drbdResourceName, retCode, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				/* リカバリカウンタを設定（連続制御回避の為） */
				drbdRecoveryCnt = EMSYS_DRBD_RECOVERY;
			}
		}
		/* モード遷移（運用遷移中） */
		else if( newSysMode == EMBASE_SYSMOD_OPERAT )
		{
			/* 他系のＤＲＢＤロール状態がプライマリ、かつ */
			/* ＤＲＢＤ接続状態が Connected の場合        */
			if( (EMBASE_DRBD_ROLE_PRIMARY == ps_EMSYSTEM->drbdRole[1]) ||
			    (EMBASE_DRBD_CSTATE_CONNECTED) )
			{
				/* 補足：運用モード遷移中にかかわらず他系がプライマリの状態は、他系が直前まで運用モードで系列間ネットワー */
				/*       クが正常のまま、他系の装置監視プロセスが何らかの問題により停止したと推察される。                         */
				/*       対処方法としては、disconnect を実行してＤＲＢＤの同期を切断し、ロール状態を Secondary/Unknown としたのち */
				/*       自系をプライマリに変更する。                                                                             */
				/*       disconnect すると、接続状態は StandAlone となる。一度 disconnect を実行すると、他系が connect しようとし */
				/*       しても、接続できなくなるため、connect を実施して StandAlone から WFConnection にしておく必要がある。     */
				/*       ただし、一定時間経過すると、StandAlone に戻るため、定期的に connect を実行しておく必要がある。           */

				/* ログを出力 */
				sprintf(logMsg, "func_drbd_recovery(): 運用遷移中：他系ロール状態がプライマリのため、ＤＲＢＤ切断を実施");
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
	
				/* ＤＲＢＤ切断処理の実施 */
				sprintf( cmdStr, "/usr/sbin/drbdadm disconnect %s 2> /dev/null", drbdResourceName );
				if( (retCode = system( cmdStr )) != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_drbd_recovery(): ＤＲＢＤ復旧コマンド drbdadm disconnect %s 失敗 ret[ %d ] err[ %d ]",
						drbdResourceName, retCode, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_drbd_recovery(): ＤＲＢＤ復旧コマンド drbdadm disconnect %s 成功 ret[ %d ] err[ %d ]",
						drbdResourceName, retCode, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				/* リカバリカウンタを設定（連続制御回避の為） */
				drbdRecoveryCnt = EMSYS_DRBD_RECOVERY;
			}
		}
		/* モード遷移（待機遷移中） */
		else if( newSysMode == EMBASE_SYSMOD_STANDB )
		{
			/* 自系のＤＲＢＤロール状態がプライマリの場合 */
			if( EMBASE_DRBD_ROLE_PRIMARY == ps_EMSYSTEM->drbdRole[0] )
			{
				/* モード遷移シーケンスを 運用->待機 に変更 */
				func_set_mode_tran( EMSYS_SEQ_OPE2STB );

				/* リカバリカウンタを設定（連続制御回避の為） */
				drbdRecoveryCnt = EMSYS_DRBD_RECOVERY;

				/* ログを出力 */
				sprintf(logMsg, "func_drbd_recovery(): 待機遷移中：自系ロール状態がプライマリのため、遷移シーケンスを [ %d ] に変更", modTrnSeqNumb );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			/* 自系がセカンダリで他系がアンノウンの場合 */
			else if( (EMBASE_DRBD_ROLE_SECONDARY == ps_EMSYSTEM->drbdRole[0]) &&
			         (EMBASE_DRBD_ROLE_UNKNOWN == ps_EMSYSTEM->drbdRole[1]) )
			{
				/* ＤＲＢＤ同期データの破棄を実施 */
				sprintf( cmdStr, "/usr/sbin/drbdadm -- --discard-my-data connect %s 2> /dev/null", drbdResourceName );
				if( (retCode = system( cmdStr )) != 0 )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_drbd_recovery(): ＤＲＢＤ復旧コマンド drbdadm -- --discard-my-data connect %s 失敗 ret[ %d ] err[ %d ]",
						drbdResourceName, retCode, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				else
				{
					/* ログを出力 */
					sprintf(logMsg, "func_drbd_recovery(): ＤＲＢＤ復旧コマンド drbdadm -- --discard-my-data connect %s 成功 ret[ %d ] err[ %d ]",
						drbdResourceName, retCode, errno );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
				}
				/* リカバリカウンタを設定（連続制御回避の為）少し時間が必要 */
				drbdRecoveryCnt = EMSYS_DRBD_RECOVERY + EMSYS_DRBD_RECOVERY;
			}
		}
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセスPID取得
 *
 * 機能：
 *		引数 c_procName で指定されたプロセスのPIDを取得する
 */
int func_get_pid( char *c_procName )
{
	FILE	*fp;						/* ファイルポインタ */
	int		pidNumb_check;				/* PID */
	char	checkCmd[512];				/* PID確認用コマンド格納領域 */
	char	checkData[512];				/* 確認データ処理用ワーク */

	memset( checkData, 0, sizeof(checkData) );

	/* 起動中のプロセス名のプロセスＩＤ（最古のもの）を取得するコマンドを設定 */
	sprintf( checkCmd, "pgrep -o %s 2> /dev/null", c_procName );
	/* コマンドを実行してパイプから取得 */
	if( (fp = popen( checkCmd, "r" )) == NULL )
	{
		/* 確認コマンド実行失敗 */
		return( -1 );
	}
	/* プロセスＩＤを取得 */
	if( fgets( checkData, 512, fp ) == NULL )
	{
		/* プロセスＩＤ見つからない場合 */
		return( -1 );
	}
	else
	{
		/* プロセスＩＤの取得 */
		pidNumb_check = atol( checkData );
	}

	/* パイプを閉じる */
	pclose( fp );

	return(pidNumb_check);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ネットマスクの取得
 *
 * 機能：
 *		指定されたホスト名に割り当てられたIPアドレスのネットマスク値を nmcli コマ
 *		ンドを実行して指定されたIPアドレスのネットマスク値を取得する。
 *		
 * 戻り値：
 * 		ネットマスク値を返す。エラー時（ -1 ）
 */
int func_get_netmask( char *hostName )
{
	FILE				*fp;						/* ファイルポインタ */
	char				cmdStr[1024];				/* コマンド文字列 */
	char				wrkStr[1024];				/* チェック用文字列 */
	char				checkData[1024];			/* チェック用文字列 */
	char				*workPt;					/* ワークポインタ */
	int					sepCount;					/* セパレータデータ数 */
	int					retCode;					/* 戻り値 */
	int					netMask = 0;				/* ネットマスク値 */
	struct st_sepStr	s_sepStr;					/* 分割データ格納領域 */

	struct hostent		*workHostEnt;					/* ホスト情報 */
	char				ipAddrStr[16];					/* IPアドレス情報 */

	/* ホスト名からIPアドレス情報を取得 */
	if( ( workHostEnt = gethostbyname( hostName ) ) == NULL )
	{
		/* エラー */
#ifdef DEBUG_ERR
printf("%s:func_get_netmask(): ホスト名チェック：エラー Hostname:%s\n", EMBASE_OWN_NAME, hostName );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_get_netmask(): ホスト名チェックエラー Hostname:%s", hostName);
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		return( -1 );
	}

	/* IPアドレスの取得 */
	sprintf( ipAddrStr, "%d.%d.%d.%d",
		 (unsigned char)*(workHostEnt->h_addr_list[0]),
		 (unsigned char)*(workHostEnt->h_addr_list[0] + 1),
		 (unsigned char)*(workHostEnt->h_addr_list[0] + 2),
		 (unsigned char)*(workHostEnt->h_addr_list[0] + 3));

	/* ネットマスク取得のためのコマンドの作成 */
	sprintf( cmdStr, "LANG=C /bin/nmcli | grep %s/ 2> /dev/null", ipAddrStr );

	/* 指定されたIPアドレスの情報を取得する */
	if( (fp = popen( cmdStr, "r")) == NULL )
	{
		/* エラー */
		/* ログを出力 */
		sprintf(logMsg, "func_get_netmask(): ネットワークデバイス状態確認コマンド失敗");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		return( -1 );
	}
	else
	{
		/* 出力がなくなるまで繰り返す */
		while( fgets( checkData, 512, fp ) != NULL )
		{
			/* 出力内容をスペースで区切る（空白が連続しても一つの区切りとする） */
			sepCount = func_sep_data( checkData, ' ', EMSYS_SEP_FLG_CONTIN, &s_sepStr );
			/* IPアドレス情報とネットマスク値の取得 */
			strcpy( wrkStr, s_sepStr.data[1] );
			/* 内容を'/'で区切る */
			sepCount = func_sep_data( wrkStr, '/', EMSYS_SEP_FLG_CONTIN, &s_sepStr );
			/* IPアドレスが一致することを確認 */
			if( strcmp( s_sepStr.data[0], ipAddrStr ) == 0 )
			{
				/* ネットマスク値の取得 */
				netMask = atoi( s_sepStr.data[1] );
			}
		}
	}
	/* ファイルオープン中の場合 */
	if( fp != NULL )
	{
		/* ファイルクローズ */
		fclose( fp );
	}
	
	return( netMask );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶ分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列を、引数 c_delim で指定された文字で区切り、
 *		ps_sepStr に格納する。
 * 戻り値：
 * 		引数 c_delim で区切られたデー多数を返す。
 */
int func_sep_data( char *pc_data, char c_delim, int i_flg, struct st_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	/* 分割データ格納領域を初期化 */
	memset( ps_sepStr, 0, sizeof(struct st_sepStr) );

	/* 初期ポインタを設定 */
	pt = pc_data;
	/* 文字列の終わりまで処理を実施する */
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		/* 空白文字とタブはデータとして扱わない場合 */
		if( (i_flg & EMSYS_SEP_FLG_SPCSKP) && (*pt == ' ') && (c_delim != ' ') ) continue;
		if( (i_flg & EMSYS_SEP_FLG_SPCSKP) && (*pt == '\t') && (c_delim != '\t') ) continue;

		/* 指定されたデリミタの場合 */
		if( *pt == c_delim )
		{
			/* 連続する区切り文字が終わるまで新たなデータとして処理しない場合 */ 
			if( (i_flg & EMSYS_SEP_FLG_CONTIN) && ( *(pt + 1) == c_delim ) ) continue;
			ps_sepStr->data[idx1][idx2] = 0;
			idx2 = 0;
			idx1++;
			continue;
		}
		ps_sepStr->data[idx1][idx2] = *pt;
		idx2++;
	}
	ps_sepStr->data[idx1][idx2] = 0;
	if(idx2) idx1++;

	return( idx1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ログ出力
 *
 * 機能：
 *		ログファイル（テキストファイル）への出力を行う。
 *		引数 msgStr に指定された文字列をログファイルに出力する。また、引数 dtBuff
 *		に指定されているアドレスから dtLength で指定されるバイト数分をログファイi
 *		ルに16進ダンプ出力する。ただし、dtBuff に NULL が指定されている場合は、
 *		ログファイルへの16進ダンプ出力は行わない。
 *		ログファイルのサイズが 1MByte を超過する場合は、世代管理を行う。
 * 戻り値：
 * 		 0: 正常
 *		-1: 異常
 * 備考：
 *		スレッド動作を考慮し、ログ出力は排他制御（ミューテックスロック）を行う。
 */
int func_log_msgset( int msgLevel, char *msgStr, char *dtBuff, int dtLength )
{
	FILE					*fp;						/* ファイルポインタ */	
	char					fileName1[512];				/* ファイル名 */
	char					fileName2[512];				/* ファイル名 */
	int						fd;							/* ファイルディスクリプタ */
	int						loopCnt1;					/* ループカウンタ */
	long					fileSize;					/* ファイルサイズ */
	time_t					nowTime_s;					/* 現在時刻（積算秒） */
	struct timeval			nowTime;					/* 現在時刻（積算秒） */
	struct tm				localTime;					/* 年月日時分秒 */
	int						retCode;					/* 戻り値 */

	/* ログレベルの確認 */
	if( msgLevel < logLevel ) return( 0 );

	/* MUTEXロック */
	retCode = pthread_mutex_lock(&mtx_t);

	if( retCode != 0 )
	{
#ifdef DEBUG_ERR
printf("%s:func_log_msgset() pthread_mutex_lock error retCode:%d\n", retCode );
#endif
		return( -1 );
	}

	/* ログファイル名称 */
	sprintf( fileName1, "%s%s.%s", EMBASE_FPATH_LOG, EMBASE_FNAME_EMSYSLOG, EMBASE_FNEXT_EMSYSLOG );

	/* ファイルが存在しない場合 */
	if( access( fileName1, F_OK ) != 0 )
	{
		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			/* MUTEXアンロック */
			retCode = pthread_mutex_unlock(&mtx_t);

			return( -1 );
		}
		close( fd );
	}

	/* システムログファイルをオープン */
	if( (fp = fopen( fileName1, "a" )) == NULL )
	{
		/* MUTEXアンロック */
		retCode = pthread_mutex_unlock(&mtx_t);

		return( -1 );
	}

	/* ファイルサイズの取得 */
	fileSize = ftell( fp );

	/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
	if(fileSize > 1000000 )
	{ 
		/* ファイルを閉じる */
		fclose( fp );

		/* ログファイルの保存数分繰り返す */
		for(loopCnt1 = 5 ; loopCnt1 > 1 ; loopCnt1-- )
		{
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s_%d.%s", EMBASE_FPATH_LOG, EMBASE_FNAME_EMSYSLOG, loopCnt1 - 1, EMBASE_FNEXT_EMSYSLOG );
			sprintf( fileName2, "%s%s_%d.%s", EMBASE_FPATH_LOG, EMBASE_FNAME_EMSYSLOG, loopCnt1, EMBASE_FNEXT_EMSYSLOG );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			if( access( fileName1, F_OK ) == 0 )
				rename( fileName1, fileName2 );
		}
		/* ファイル名の設定 */
		sprintf( fileName1, "%s%s.%s", EMBASE_FPATH_LOG, EMBASE_FNAME_EMSYSLOG, EMBASE_FNEXT_EMSYSLOG );
		sprintf( fileName2, "%s%s_1.%s", EMBASE_FPATH_LOG, EMBASE_FNAME_EMSYSLOG, EMBASE_FNEXT_EMSYSLOG );
		/* 古いファイルの削除 */
		if( access( fileName2, F_OK ) == 0 )
			unlink( fileName2 );
		/* ローテーションの実施 */
		rename( fileName1, fileName2 );

		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			/* MUTEXアンロック */
			retCode = pthread_mutex_unlock(&mtx_t);

			return( -1 );
		}
		close( fd );

		/* システムログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			/* MUTEXアンロック */
			retCode = pthread_mutex_unlock(&mtx_t);

			return( -1 );
		}
	}

	/* 年月日時分秒を取得 */
	gettimeofday( &nowTime, NULL );
	nowTime_s = nowTime.tv_sec;
	localtime_r( &nowTime_s, &localTime );
	
	/* ログメッセージを出力 */
	fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s\n",
		localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
		localTime.tm_hour, localTime.tm_min, localTime.tm_sec, nowTime.tv_usec,
		msgStr );

	/* ダンプデータが指定されている場合 */
	if( ( dtBuff != NULL ) && ( dtLength > 0 ) )
	{
		fprintf( fp, "    0000(00000):");
		for( loopCnt1 = 1 ; loopCnt1 <= dtLength ; loopCnt1++ )
		{
			fprintf( fp, " %02X", (unsigned char)dtBuff[ loopCnt1 - 1 ] );
			if( ( ( loopCnt1 % 16 ) == 0 ) && ( loopCnt1 != dtLength ) )
			{
				fprintf( fp, "\n" );
				fprintf( fp, "    %04X(%05d):", loopCnt1, loopCnt1 );
			}
		}
		fprintf( fp, "\n" );
	}

	/* ファイルを閉じる */
	fclose( fp );

	/* MUTEXアンロック */
	retCode = pthread_mutex_unlock(&mtx_t);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 設定値ファイル
 *
 * 機能：
 *		EMBASE_CONFIG_FILE で指定された設定ファイルが存在する場合、設定値を取得
 *		する。設定ファイルで設定可能な値は以下とする。なお、設定ファイルに設定値
 *		が存在しない場合は、デフォルト値が適用される。
 *		・1系,2系装置ホスト名：（サービス用ネットワーク側ホスト名）...... (1)(3)
 *		・1系,2系装置ホスト名：（系列間用ネットワーク側ホスト名）........ (2)(4)
 *		・ネットワークI/F名：（サービス用ネットワーク側）................ (5)
 *		・ネットワークI/F名：（系列間用ネットワーク側）.................. (6)
 *
 *		          |  <--------- (0)hostname ---------->  |
 *		          |(1)hostname                           |(3)hostname
 *		+---------+--------+                   +---------+--------+
 *		| (5)[I/F(enp2s0)] |                   | (5)[I/F(enp2s0)] |
 *		|                  |(2)hostname        |                  |
 *		|  (6)[I/F(enp3s0)]+-------------------+(6)[I/F(enp3s0)]  |
 *		|                  |        (4)hostname|                  |
 *		+------------------+                   +------------------+
 *		
 *		・死活監視情報UDP送信ポート番号 ................................. (7)
 *		・死活監視情報UDP受信ポート番号 ................................. (8)
 *		・死活監視情報送信周期（秒） .................................... (9)
 *		・死活監視情報受信タイムアウト（秒） ............................ (10)
 *
 *      ＤＲＢＤの等価リソース名称とデバイス名称を指定する。
 *		・ＤＲＢＤ等価リソース名 ........................................ (11)
 *		・ＤＲＢＤ等価デバイス名 ........................................ (12)
 *
 *      装置モードが運用系の場合に、動作させる業務アプリのサービス名を設定する
 *		設定値を nksystem.service としておくと、装置が「運用」に遷移する際に
 *		systemctl start nksystem.service が実行される。
 *		また、装置が「運用」から他のモードに遷移する際は、
 *		systemctl stop nksystem.service が実行される。
 *		設定は、embase.conf ファイルでのみ指定可能
 *		・運用モード時に動作させる業務アプリのサービス名 ................ (13)
 *
 *		装置モードが運用系の場合に、動作チェックを行うプロセス名を最大で２つま
 *		で指定可能、運用モードにもかかわらず当該プロセスが一定時間動作していな
 *		場合は、待機に自動遷移する。２つプロセスを指定した場合は、両方のプロセ
 *		スが動作している必要がある。ドフォルト状態は指定なし。
 *		設定は、embase.conf ファイルでのみ指定可能
 *		動作していない場合は
 *		・運用モード時に動作を確認するプロセス名その１ .................. (14)
 *		・運用モード時に動作を確認するプロセス名その２ .................. (15)
 *
 *		本プログラムのログ出力レベルを設定する
 *		・ログレベル .................................................... (16)
 *
 *		ＤＲＢＤサービスの動作状態を監視し、ＤＲＢＤサービスが起動していない場
 *		合は、systemctl start drbd.service コマンドでＤＲＢＤサービスの起動を
 *		試みる。サービスの起動状態を確認する周期を分で指定する。
 *		・ＤＲＢＤ動作状態チェック復旧処理チェック時間 .................. (17)
 *
 *		自系が待機/準備の際に時刻同期サービスの動作状態を監視し、
 *		時刻同期サービスが起動していない場合は、systemctl start コマンドで
 *		時刻同期サービスの起動を試みる際のサービス名を設定する。
 *		・時刻同期サービス名 ............................................ (18)
 *
 *		自系が運用モード遷移時に時刻同期ができているかをチェックするかを
 *		指定する。
 *		・運用モード遷移時の時刻同期チェック（チェック 0:しない、1:する） (19)
 *
 * 初期値:
 *		 No | プログラム定義名(定義値)          | 設定ファイル定義名 | 設定範囲
 *		----+-----------------------------------+--------------------+------------
 *		( 0)| EMBASE_HOSTNAME_C ("nksystem")    | HOSTNAME_COMM      | 64文字以内
 *		( 1)| EMBASE_HOSTNAME_1 ("nksystem1")   | HOSTNAME_1KEI      | 64文字以内
 *		( 2)| EMBASE_HOSTNAME_1S ("nksystem1s") | HOSTNAME_1KEIS     | 64文字以内
 *		( 3)| EMBASE_HOSTNAME_2 ("nksystem2")   | HOSTNAME_2KEI      | 64文字以内
 *		( 4)| EMBASE_HOSTNAME_2S ("nksystem2s") | HOSTNAME_2KEIS     | 64文字以内
 *		( 5)| EMBASE_NETDEV_NAME_S ("enp2s0")   | NETDEVNAME_SERVI   | 64文字以内
 *		( 6)| EMBASE_NETDEV_NAME_I ("enp3s0")   | NETDEVNAME_INTER   | 64文字以内
 *		( 7)| EMBASE_LIFCHK_SND_PORT (9650)     | LIFCHK_SND_PORT    | 1024-30000
 *		( 8)| EMBASE_LIFCHK_RCV_PORT (9660)     | LIFCHK_RCV_PORT    | 1024-30000
 *		( 9)| EMBASE_LIFCHK_SND_TIME (1)        | LIFCHK_SND_TIME    | 1-10
 *		(10)| EMBASE_LIFCHK_RCV_TIME (3)        | LIFCHK_RCV_TIME    | 2-20
 *		(11)| EMBASE_DRBD_RESOURCE ("r0")       | DRBD_RESOURCE      | 64文字以内
 *		(12)| EMBASE_DRBD_DEVICE ("/dev/drbd0") | DRBD_DEVICE        | 64文字以内
 *		(13)| なし                              | APP_SERVICE_NAME   | 64文字以内
 *		(14)| なし                              | APP_RUNCHECK_1     | 64文字以内
 *		(15)| なし                              | APP_RUNCHECK_2     | 64文字以内
 *		(16)| なし(3)                           | LOG_LEVEL          | 1-3
 *		(17)| EMBASE_DRBD_CHECK_TIME (5)        | DRBD_CHECK_TIME    | 1-300
 *		(18)| なし                              | TIMESYNC_SERV_NAME | 64文字以内
 *		(19)| なし(0)                           | TIMESYNC_CHECK     | 0,1
 *
 *		備考：(0)は2系列共通ホスト名
 *		      共通ホスト名を設定すると、運用系の装置が共有ホスト名のIPアドレスを
 *		      使用する。
 *		      共通ホスト名が未設定の場合は、運用系の装置は共通ホストのIPアドレス
 *		      を使用しない。
 *		注意：ホスト名とIPアドレスの対応は/etc/hosts で行う
 *		注意：ホスト名を変更する場合は、/etc/hosts ファイルと /etc/hostname の変更
 *		　　 が必要
 *		注意：死活監視情報の受信ポートを変更する場合は、Firewall の変更が必要
 *		
 * 戻り値：
 * 		 0: 正常
 *		-1: 異常
 */
int func_get_config()
{
	FILE				*fp;									/* ファイルポインタ */
	char				checkData[2048];						/* 読込情報 */
	int					sepCount;								/* 分割数 */
	int					workData;								/* ワークデータ */
	struct st_sepStr	s_sepStr;								/* 分割データ格納領域 */
	
#ifdef DEBUG_1
printf("%s: func_get_config(): 設定ファイル読込開始\n", EMBASE_OWN_NAME );
#endif
	/* ログを出力 */
	sprintf(logMsg, "func_get_config(): 設定ファイル読込開始");
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

	/* 設定ファイルをオープン */
	if( (fp = fopen( EMBASE_CONFIG_FILE, "r" )) == NULL )
	{
#ifdef DEBUG_ERR
printf("%s: func_get_config(): 設定ファイル読込（ファイルが存在しないかアクセスできません）\n", EMBASE_OWN_NAME );
#endif
		/* ログを出力 */
		sprintf(logMsg, "func_get_config(): Configuration File Not Find or Open Error!");
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

		return( -1 );
	}

	/* ファイルの最後まで繰り返す */
	while( fgets( checkData, 512, fp ) != NULL )
	{
		/*コメント行の除外 */
		if( checkData[0] == '#' ) continue;

		/* 入力を = で区切る */
		sepCount = func_sep_data( checkData, '=', EMSYS_SEP_FLG_SPCSKP, &s_sepStr );

		/* データ分割数が2以外の場合は除外 */
		if( sepCount != 2 ) continue;

		/* 分割データ2つ目の文字数が0の場合は除外 */
		if( 0 == strlen( s_sepStr.data[1] ) ) continue;

		/* 分割データ2つ目の文字数が64文字より多い場合は除外 */
		if( 64 <= strlen( s_sepStr.data[1] ) ) continue;

		/* 数値データに変換 */
		workData = atoi( s_sepStr.data[1] );

		/* (00) 設定値が共通ホスト名の場合 */
		if( 0 == strcmp( "HOSTNAME_COMM", s_sepStr.data[0] ) )
		{
			/* 共通ホスト名を設定 */
			sprintf(sysHostName[EMBASE_SYSNUMBER_C], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): 共通ホスト名を [ %s ] に設定しました。\n", EMBASE_OWN_NAME, sysHostName[EMBASE_SYSNUMBER_C] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Hostname(common) set [ %s ]", sysHostName[EMBASE_SYSNUMBER_C] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (01) 設定値が１系ホスト名（サービス用ネットワーク）の場合 */
		else if( 0 == strcmp( "HOSTNAME_1KEI", s_sepStr.data[0] ) )
		{
			/* １系ホスト名（サービス用ネットワーク側）を設定 */
			sprintf( sysHostName[EMBASE_SYSNUMBER_1], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): １系ホスト名（サービス用ネットワーク側）を [ %s ] に設定しました。\n",
	EMBASE_OWN_NAME, sysHostName[EMBASE_SYSNUMBER_1] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Hostname 1-KEI (service) set [ %s ]", sysHostName[EMBASE_SYSNUMBER_1] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (02) 設定値が２系ホスト名（サービス用ネットワーク）の場合 */
		else if( 0 == strcmp( "HOSTNAME_2KEI", s_sepStr.data[0] ) )
		{
			/* ２系ホスト名（サービス用ネットワーク側）を設定 */
			sprintf( sysHostName[EMBASE_SYSNUMBER_2], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): ２系ホスト名（サービス用ネットワーク側）を [ %s ] に設定しました。\n",
EMBASE_OWN_NAME, sysHostName[EMBASE_SYSNUMBER_2] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Hostname 2-KEI (service) set [ %s ]", sysHostName[EMBASE_SYSNUMBER_2] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (03) 設定値が１系ホスト名（系列間用ネットワーク）の場合 */
		else if( 0 == strcmp( "HOSTNAME_1KEIS", s_sepStr.data[0] ) )
		{
			/* １系ホスト名（系列間用ネットワーク側）を設定 */
			sprintf( subHostName[EMBASE_SYSNUMBER_1], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): １系ホスト名（系列間用ネットワーク側）を [ %s ] に設定しました。\n",
	EMBASE_OWN_NAME, subHostName[EMBASE_SYSNUMBER_1] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Hostname 1-KEI (internal) set [ %s ]", subHostName[EMBASE_SYSNUMBER_1] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (04) 設定値が２系ホスト名（系列間用ネットワーク）の場合 */
		else if( 0 == strcmp( "HOSTNAME_2KEIS", s_sepStr.data[0] ) )
		{
			/* ２系ホスト名（系列間用ネットワーク側）を設定 */
			sprintf( subHostName[EMBASE_SYSNUMBER_2], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): ２系ホスト名（系列間用ネットワーク側）を [ %s ] に設定しました。\n",
EMBASE_OWN_NAME, subHostName[EMBASE_SYSNUMBER_2] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Hostname 2-KEI (internal) set [ %s ]", subHostName[EMBASE_SYSNUMBER_2] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (05) 設定値がサービス用ネットワークI/F名の場合 */
		else if( 0 == strcmp( "NETDEVNAME_SERVI", s_sepStr.data[0] ) )
		{
			/* サービス用ネットワークI/Fを設定 */
			sprintf(netDeviceName[EMBASE_NETDEV_SERVICE] , "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): サービス用ネットワークI/Fを [ %s ] に設定しました。\n",
EMBASE_OWN_NAME, netDeviceName[EMBASE_NETDEV_SERVICE] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Network I/F(service) set [ %s ]", netDeviceName[EMBASE_NETDEV_SERVICE] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (06) 設定値が系列間用ネットワークI/F名の場合 */
		else if( 0 == strcmp( "NETDEVNAME_INTER", s_sepStr.data[0] ) )
		{
			/* 系列間用ネットワークI/Fを設定 */
			sprintf(netDeviceName[EMBASE_NETDEV_INTERNAL] , "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): 系列間用ネットワークI/Fを [ %s ] に設定しました。\n",
EMBASE_OWN_NAME, netDeviceName[EMBASE_NETDEV_INTERNAL] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Network I/F(internal) set [ %s ]", netDeviceName[EMBASE_NETDEV_INTERNAL] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (07) 設定値が死活監視情報送信元ポート番号の場合 */
		else if( 0 == strcmp( "LIFCHK_SND_PORT", s_sepStr.data[0] ) )
		{
			/* 死活監視情報送信元ポート番号を設定 */
			if( ( 1024 > workData ) || ( 30000 < workData ) )
			{
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報送信元ポート番号の設定値が適用範囲外です。[ %d ]\n", EMBASE_OWN_NAME, workData );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Send Port Number Error [ %d ]", workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* 送信元ポート番号を設定 */
				udpSendPort = (unsigned short)workData;
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報送信元ポート番号を [ %d ] に設定しました。\n", EMBASE_OWN_NAME, udpSendPort );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Send Port Number set [ %d ]", udpSendPort );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
		/* (08) 設定値が死活監視情報受信ポート番号の場合 */
		else if( 0 == strcmp( "LIFCHK_RCV_PORT", s_sepStr.data[0] ) )
		{
			/* 死活監視情報受信ポート番号を設定 */
			if( ( 1024 > workData ) || ( 30000 < workData ) )
			{
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報受信ポート番号の設定値が適用範囲外です。[ %d ]\n", EMBASE_OWN_NAME, workData );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Recv Port Number error [ %d ]", workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* 受信ポート番号を設定 */
				udpRecvPort = (unsigned short)workData;
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報受信ポート番号を [ %d ] に設定しました。\n", EMBASE_OWN_NAME, udpRecvPort );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Recv Port Number set [ %d ]", udpRecvPort );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
		/* (09) 設定値が死活監視情報送信周期の場合 */
		else if( 0 == strcmp( "LIFCHK_SND_TIME", s_sepStr.data[0] ) )
		{
			/* 死活監視情報送信周期を設定 */
			if( ( 1 > workData ) || ( 10 < workData ) )
			{
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報送信周期の設定値が適用範囲外です。[ %d ]\n", EMBASE_OWN_NAME, workData );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Send Interval Time error [ %d ]", workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* 送信周期を設定 */
				udpSendTime = (unsigned short)workData;
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報送信周期を [ %d ] に設定しました。\n", EMBASE_OWN_NAME, udpSendTime );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Send Interval Time set [ %d ]", udpSendTime );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
		/* (10) 設定値が死活監視情報受信タイムアウトの場合 */
		else if( 0 == strcmp( "LIFCHK_RCV_TIME", s_sepStr.data[0] ) )
		{
			/* 死活監視情報受信タイムアウトを設定 */
			if( ( 2 > workData ) || ( 20 < workData ) )
			{
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報受信タイムアウトの設定値が適用範囲外です。[ %d ]\n", EMBASE_OWN_NAME, workData );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Recv Timeout error [ %d ]", workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* 送信周期を設定 */
				udpRecvTime = (unsigned short)workData;
#ifdef DEBUG_1
printf("%s: func_get_config(): 死活監視情報受信タイムアウトを [ %d ] に設定しました。\n", EMBASE_OWN_NAME, udpRecvTime );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Life Check Recv Timeout set [ %d ]", udpRecvTime );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
		/* (11) 設定値がＤＲＢＤ等価リソース名の場合 */
		else if( 0 == strcmp( "DRBD_RESOURCE", s_sepStr.data[0] ) )
		{
			/* 等価用リソースを設定 */
			sprintf(drbdResourceName, "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): ＤＲＢＤ等価リソース名を [ %s ] に設定しました。\n",
EMBASE_OWN_NAME, drbdResourceName );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): DRBD resource name set [ %s ]", drbdResourceName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (12) 設定値がＤＲＢＤ等価デバイス名の場合 */
		else if( 0 == strcmp( "DRBD_DEVICE", s_sepStr.data[0] ) )
		{
			/* 等価用デバイスを設定 */
			sprintf(drbdDeviceName, "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): ＤＲＢＤ等価デバイスリソース名を [ %s ] に設定しました。\n",
EMBASE_OWN_NAME, drbdDeviceName );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): DRBD device name set [ %s ]", drbdDeviceName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (13) 設定値が運用モード業務アプリサービス名の場合 */
		else if( 0 == strcmp( "APP_SERVICE_NAME", s_sepStr.data[0] ) )
		{
			/* 運用モード業務アプリサービス名を設定 */
			sprintf( appServiceName, "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): 運用モード業務アプリサービス名を [ %s ] に設定しました。\n",
	EMBASE_OWN_NAME, appServiceName );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): App Service Name set [ %s ]", appServiceName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (14) 設定値が運用モード動作確認プロセス名１の場合 */
		else if( 0 == strcmp( "APP_RUNCHECK_1", s_sepStr.data[0] ) )
		{
			/* 運用モード動作確認プロセス名１を設定 */
			sprintf( appRuncheckName[0], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): 運用モード動作確認プロセス１を [ %s ] に設定しました。\n",
	EMBASE_OWN_NAME, appRuncheckName[0] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): App Run Check Process1 set [ %s ]", appRuncheckName[0] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (15) 設定値が運用モード動作確認プロセス名２の場合 */
		else if( 0 == strcmp( "APP_RUNCHECK_2", s_sepStr.data[0] ) )
		{
			/* 運用モード動作確認プロセス名２を設定 */
			sprintf( appRuncheckName[1], "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): 運用モード動作確認プロセス２を [ %s ] に設定しました。\n",
	EMBASE_OWN_NAME, appRuncheckName[1] );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): App Run Check Process2 set [ %s ]", appRuncheckName[1] );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (16) ログレベルの場合 */
		else if( 0 == strcmp( "LOG_LEVEL", s_sepStr.data[0] ) )
		{
			/* ログレベルを設定 */
			logLevel = workData;

#ifdef DEBUG_1
printf("%s: func_get_config(): ログレベルを [ %d ] に設定しました。\n",
	EMBASE_OWN_NAME, logLevel );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Log level set [ %d ]", logLevel );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
		/* (17) 設定値がＤＲＢＤサービス動作状態チェック周期の場合 */
		else if( 0 == strcmp( "DRBD_CHECK_TIME", s_sepStr.data[0] ) )
		{
			/* ＤＲＢＤサービス動作状態チェック時間（分）を設定 */
			if( ( 1 > workData ) || ( 300 < workData ) )
			{
#ifdef DEBUG_1
printf("%s: func_get_config(): ＤＲＢＤサービス動作状態チェック周期の設定値が適用範囲外です。[ %d ]\n", EMBASE_OWN_NAME, workData );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): DRBD service Running Check Time error [ %d ]", workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* 送信周期を設定 */
				drbdCheckTime = workData;
#ifdef DEBUG_1
printf("%s: func_get_config(): ＤＲＢＤサービス動作状態チェック周期を [ %d ] に設定しました。\n", EMBASE_OWN_NAME, drbdCheckTime );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): DRBD service Running Check Time set [ %d ]", drbdCheckTime );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
/* [20250526] Add St */
		/* (18) 設定値が時刻同期サービス名の場合 */
		else if( 0 == strcmp( "TIMESYNC_SERV_NAME", s_sepStr.data[0] ) )
		{
			/* 時刻同期サービス名を設定 */
			sprintf( appTimeSyncName, "%s", s_sepStr.data[1] ); 
#ifdef DEBUG_1
printf("%s: func_get_config(): 時刻同期サービス名を [ %s ] に設定しました。\n",
	EMBASE_OWN_NAME, appTimeSyncName );
#endif
			/* ログを出力 */
			sprintf(logMsg, "func_get_config(): Time Sync Service Name set [ %s ]", appTimeSyncName );
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		}
/* [20250526] Add Ed */
/* [20250717] Add St */
		/* (19) 設定値が運用モード遷移時の時刻同期チェックの場合 */
		else if( 0 == strcmp( "TIMESYNC_CHECK", s_sepStr.data[0] ) )
		{
			/* 運用モード遷移時の時刻同期チェックを設定 */
			if( ( 0 != workData ) && ( 1 != workData ) )
			{
#ifdef DEBUG_1
printf("%s: func_get_config(): 運用モード遷移時の時刻同期チェックの設定値が適用範囲外です。[ %d ]\n", EMBASE_OWN_NAME, workData );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Time Sync Check [ %d ]", workData );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
			else
			{
				/* 運用モード遷移時の時刻同期チェックを設定 */
				timeSyncCheckFlg = workData;
#ifdef DEBUG_1
printf("%s: func_get_config(): 運用モード遷移時の時刻同期チェックを [ %d ] に設定しました。\n", EMBASE_OWN_NAME, timeSyncCheckFlg );
#endif
				/* ログを出力 */
				sprintf(logMsg, "func_get_config(): Time Sync Check set [ %d ]", timeSyncCheckFlg );
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
/* [20250717] Add Ed */
	}
	
	/* ファイルクローズ */
	fclose( fp );

#ifdef DEBUG_1
printf("%s: func_get_config(): 設定ファイル読込終了\n", EMBASE_OWN_NAME );
#endif
	/* ログを出力 */
	sprintf(logMsg, "func_get_config(): 設定ファイル読込終了");
	func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

	return( 0 );
}

/* [20250526] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期サービスの状態取得と起動
 *
 * 機能：
 *		pgrepコマンドを実行して時刻同期の起動有無をチェックし、
 *		起動していなければ起動する。
 *		
 * 戻り値：
 *		0    ：正常
 *		0以外：異常
 */
int func_chk_time_sync()
{
	int		retCode;					/* 戻り値 */
	char	cmdStr[1024];				/* コマンド用ワーク */

	/* 時刻同期サービス名が設定されている場合 */
	if( strlen(appTimeSyncName) > 0 )
	{
		/* 自系が「準備」モード または */
		/* 自系が「待機」モード であること*/
		if( (EMBASE_SYSMOD_WAIT == embase_func_get_my_mode())  ||
		    (EMBASE_SYSMOD_STANDB == embase_func_get_my_mode()) )
		{
			/* 時刻同期サービスがchronydの場合 */
			if( 0 == strcmp( appTimeSyncName, "chronyd") )
			{
				/* 時刻同期プロセスが動作しているか確認するコマンドを実行 */
				sprintf( cmdStr, "/usr/bin/pgrep -o %s > /dev/null 2>&1", appTimeSyncName );
				if( 0 != system( cmdStr ) )
				{
					/* ログを出力 */
					sprintf(logMsg, "func_chk_time_sync(): 時刻同期対象プロセス [ %s ] 動作未確認 Mode(%d)", appTimeSyncName, embase_func_get_my_mode() );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					/* 動作監視対象プロセス動作中 */

					/* ログを出力 */
					sprintf(logMsg, "func_chk_time_sync(): 時刻同期アプリサービス( %s ) 起動処理開始 Mode(%d)", appTimeSyncName, embase_func_get_my_mode() );
					func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );

					sprintf( cmdStr, "systemctl start %s > /dev/null 2>&1", appTimeSyncName );
					retCode = system( cmdStr );
					if( retCode != 0 )
					{
						/* ログを出力 */
						sprintf(logMsg, "func_chk_time_sync(): cmd [ sytemctl start %s ] 失敗 errno:%d", appTimeSyncName, errno );
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					}
					else
					{
						/* ログを出力 */
						sprintf(logMsg, "func_chk_time_sync(): cmd [ systemctl start %s ] 成功", appTimeSyncName );
						func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
					}
				}
			}
			else
			{
				sprintf(logMsg, "func_chk_time_sync(): Not supported Service : %s", appTimeSyncName);
				func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			}
		}
	}

	return( 0 );
}
/* [20250526] Add Ed */
/* [20250717] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期状況チェック
 *
 * 機能：
 * 		chronyc sourcesコマンドにて時刻同期しているサーバを検索する
 *		ps_sepStr に格納する。
 * 戻り値：
 * 		 1: 正常（時刻同期あり）
 *		-1: 異常（popenエラー）
 *		-2: 異常（設定エラー）
 */
int func_timeSync_Check( void )
{
	FILE				*fp;						/* ファイルポインタ */
	char				cmdStr[1024];				/* コマンド文字列 */
	char				checkData[1024];			/* チェック用文字列 */
	int					sepCount;					/* セパレータデータ数 */
	int					retCode = -1;				/* 戻り値 */
	struct st_sepStr	s_sepStr;					/* 分割データ格納領域 */

	/* 時刻同期サービスがchronydの場合 */
	if( ( strlen(appTimeSyncName) > 0 ) &&
		( 0 == strcmp( appTimeSyncName, "chronyd") ) )
	{
		/* 時刻同期状況を取得するためのコマンドの作成 */
		sprintf( cmdStr, "chronyc sources 2> /dev/null");

		/* 時刻同期状況を取得する */
		if( (fp = popen( cmdStr, "r")) == NULL )
		{
			/* エラー */
			/* ログを出力 */
			sprintf(logMsg, "func_timeSync_Check(): chronyc sources Error");
			func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
			retCode = -1;
		}
		else
		{
			sprintf(logMsg, "func_timeSync_Check(): %s -> OK", cmdStr);
			func_log_msgset( EMSYS_LOG_LV1, logMsg, NULL, 0 );

			/* 出力がなくなるまで繰り返す */
			while( fgets( checkData, 512, fp ) != NULL )
			{
				/* 出力内容をスペースで区切る（空白が連続しても一つの区切りとする） */
				sepCount = func_sep_data( checkData, ' ', EMSYS_SEP_FLG_CONTIN, &s_sepStr );

				checkData[strlen(checkData)-1]=0;
				sprintf(logMsg, "func_timeSync_Check(): %s", checkData);
				func_log_msgset( EMSYS_LOG_LV1, logMsg, NULL, 0 );

				/* 時刻同期しているsourceが見つかった場合 */
				/*  ^：サーバー */
				/*  *：は、chronyd が現在同期しているソースを表す。 */
				if( strcmp( s_sepStr.data[0], "^*" ) == 0 )
				{
					retCode = 1;
					break;
				}
			}
		}
		/* ファイルオープン中の場合 */
		if( fp != NULL )
		{
			/* ファイルクローズ */
			fclose( fp );
		}
	}
	else
	{
		sprintf(logMsg, "func_timeSync_Check(): Not supported Service : %s", appTimeSyncName);
		func_log_msgset( EMSYS_LOG_LV3, logMsg, NULL, 0 );
		retCode = -2;
	}

	return retCode;
}
/* [20250717] Add Ed */


/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
