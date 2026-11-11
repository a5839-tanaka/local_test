/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260625] 2026.06.25
 * ・送信待ち時間の上限変更
 * [20260130] 2026.01.30
 * ・TM制御判定周波数、異常継続時間の削除
 * [20251218] 2025.12.18
 * ・需給計画画面のレイアウトを変更
 * ・一次調整力パラメータの表示追加
 * [20251211] 2025.12.11 不具合修正
 * ・模擬周波数読込時の乗数保持
 * [20251209] 2025.12.09
 * ・ACシステム指令情報表示の画面で、表示切替の計算を修正
 * [20251208] 2025.12.08 需給計画一次調整力対応
 * ・需給計画の画面に、一次調整力使用、約定△kWを追加
 * [20251120] 2025.11.20 Ph5品証指摘No.37対応
 * ・スケジュールなし(-1)がスケジュール管理情報に更新されない問題の対応
 * [20250828] 2025.08.28 
 * ・火力発電所対応（蓄電池が未実装の場合のNULLアクセス回避）
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 *   模擬周波数設定、EMS管理制御情報の一次調整力開放
 * [20240910] 2024.09.10 Ph4.2対応
 * ・容量市場の実需給、実効性テストの優先度を外部ファイルから取得するように修正
 * [20240701] 2024.07.01 Ph4.2対応
 * ・需給調整市場(三次①、②）、容量市場(実需給)、容量市場(実効性テスト)で
 *   個々のストックを管理するように修正
 * [20240220] 2024.02.20 Ph4対応
 * ・RCVPWINFにSoC不感帯を追加
 * ・CEMSPINFに需給調整市場、容量市場の優先度を追加
 * ・PWPLNINFにピークカット使用有無、容量市場使用有無を追加
 * [20230902] 2023.09.02
 * ・IoT-Coreの指令データのログ出力を常に出力するように修正
 * [20230301] 2023.03.01 仕様変更
 * ・BT出力変化量制御の有効/無効を廃止(常に有効)し、変化量を充電/放電方向に分ける
 * [20230224] 2023.02.24 機能追加
 * ・計測蓄積、状態蓄積ファイルの作成有無を追加
 * [20230220] 2023.02.20 仕様変更
 * ・制御不感帯を%->Wに変更
 * [20230217] 2023.02.17 結合試験
 * ・需給計画に対する制御を見直し
 * [20210128] 2020.01.28
 * ・受電電力計画値機能追加
 * [20210115] 2021.01.15
 * ・放電再開SoC、充電再開SoCを絶対値ではなく、相対値に変更
 * [20210112] 2020.01.12
 * ・受電電力目標値の切り替え時にを変化量に従って移行するように修正
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * オンラインデバッガ
 *
 * 機能：
 * 	以下の機能を提供する。
 *	・収集装置のプロセス動作状態をモニタリングする
 *	・収集装置の状変記録とシステムログ（最新数件分）をモニタリングする
 *	・収集装置の装置状態、通信ステータス状態、計測値状態をモニタリングする
 *	
 */
#include <sys/types.h>

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <termio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvctldata.h"
#include "pvudp.h"
#include "pvcomm.h"

#include "sysdebug.h"

#define	OWN_VERSION	"1.00"							/* バージョン */

#define	OWN_PROC_NAME	"SYSDEBUG"					/* 自プロセス名 */

/* [20250807] Add St */
#if 0
#define	DEBUG_FCR_FREQ_SIM
#endif
/* [20250807] Add Ed */
#define	FCR_FREQ_SIM_FILENAME	"freq_sim_list.txt"	/* [20251211] 模擬周波数ファイルパス整形 Add */

#define	PV_SYSDEBUG_PCS_CTL	1						/* バッテリーの制御機能を実装した場合に1を設定 */

#define TIME_STR_PTN_1			1					/* 年～秒 */
#define TIME_STR_PTN_2			2					/* 年～マイクロ秒 */
#define TIME_STR_PTN_3			3					/* 月～マイクロ秒 */
#define TIME_STR_PTN_4			4					/* 時～マイクロ秒 */

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* 通信回線情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVDATINF		*ps_g_SVDATINF;				/* 機器状態情報ファイルポインタ */
struct st_STDATINF		*ps_g_STDATINF;				/* 通信ステータス情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_PVSVRINF		*ps_g_PVSVRINF;				/* モニタリングサーバ情報ファイルポインタ */
struct st_TMAGGINF		*ps_g_TMAGGINF;				/* 集約計測情報ファイルポインタ */
struct st_SVAGGINF		*ps_g_SVAGGINF;				/* 集約状態情報ファイルポインタ */
struct st_TMMONINF		*ps_g_TMMONINF;				/* 計測監視情報ファイルポインタ */
struct st_CEMSPINF		*ps_g_CEMSPINF;				/* ＣＥＭＳ制御情報保存ファイル */
struct st_DEMNDINF		*ps_g_DEMNDINF;				/* デマンド監視情報保存ファイル */
struct st_RCVPWINF		*ps_g_RCVPWINF;				/* 受電電力一定制御情報保存ファイル */
struct st_OVRCHINF		*ps_g_OVRCHINF;				/* 過充電回避制御情報保存ファイル */
struct st_MDCTLINF		*ps_g_MDCTLINF;				/* モード切替時制御情報保存ファイル */
struct st_RTCTLINF		*ps_g_RTCTLINF;				/* 遠隔制御情報ファイルポインタ */
struct st_POWCNINF		*ps_g_POWCNINF;				/* パワーコンディショナ情報ファイル */
struct st_MLTMTINF		*ps_g_MLTMTINF;				/* スマート計測監視機器情報ファイル */
struct st_BATTEINF		*ps_g_BATTEINF;				/* バッテリー情報ファイル */
struct st_REMIOINF		*ps_g_REMIOINF;				/* 汎用リモートＩＯ装置情報ファイル [Ver1.03] Add */
struct st_PLCFAINF		*ps_g_PLCFAINF;				/* ＰＬＣ装置情報ファイル [Ver3.01] Add */
struct st_DEMNDFIL		*ps_g_DEMNDFIL;				/* デマンド制御情報保存ファイル */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値保存ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_SVAGGFIL		*ps_g_SVAGGFIL;				/* 集約状態保存ファイルポインタ */
struct st_CTLJHFIL		*ps_g_CTLJHFIL;				/* 制御状変蓄積ファイルポインタ */
struct st_PVCTLFIL		*ps_g_PVCTLFIL;				/* 制御状態保存ファイルポインタ */
struct st_EMCTLFIL		*ps_g_EMCTLFIL;				/* エネルギーマネジメント管理ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイル */
struct st_PWPLNFIL		*ps_g_PWPLNFIL;				/* 受電電力計画値保存ファイルポインタ [20210128] Add */
struct st_IOTCNINF		*ps_g_IOTCNINF;				/* IoT設定情報ファイルポインタ */
struct st_PGCONINF		*ps_g_PGCONINF;				/* 電力会社向け情報ファイル [Ver5.01] Add*/
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL;			/* スケジュール管理情報ファイル [Ver5.01] Add*/
struct st_FCRSTINF		*ps_g_FCRSTINF;				/* 一次調整力パラメータ設定情報ファイル [20251218] Add */
struct st_FCRRPINF		*ps_g_FCRRPINF;				/* 一次調整力レポート蓄積項目情報ファイル [20251218] Add */

struct st_fileIndex     s_fileIndex[] =				/* ファイル定義テーブル（必須ファイル） */
{
    {   PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
    {   PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
    {   PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
    {   PV_FILENO_SVDATINF, (void **)&ps_g_SVDATINF, 0, 0, 0 },
    {   PV_FILENO_STDATINF, (void **)&ps_g_STDATINF, 0, 0, 0 },
    {   PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
    {   PV_FILENO_PVSVRINF, (void **)&ps_g_PVSVRINF, 0, 0, 0 },
    {   PV_FILENO_TMAGGINF, (void **)&ps_g_TMAGGINF, 0, 0, 0 },
    {   PV_FILENO_SVAGGINF, (void **)&ps_g_SVAGGINF, 0, 0, 0 },
    {   PV_FILENO_TMMONINF, (void **)&ps_g_TMMONINF, 0, 0, 0 },
    {   PV_FILENO_CEMSPINF, (void **)&ps_g_CEMSPINF, 0, 0, 0 },
    {   PV_FILENO_DEMNDINF, (void **)&ps_g_DEMNDINF, 0, 0, 0 },
    {   PV_FILENO_RCVPWINF, (void **)&ps_g_RCVPWINF, 0, 0, 0 },
    {   PV_FILENO_OVRCHINF, (void **)&ps_g_OVRCHINF, 0, 0, 0 },
    {   PV_FILENO_MDCTLINF, (void **)&ps_g_MDCTLINF, 0, 0, 0 },
    {   PV_FILENO_RTCTLINF, (void **)&ps_g_RTCTLINF, 0, 0, 0 },
    {   PV_FILENO_DEMNDFIL, (void **)&ps_g_DEMNDFIL, 0, 0, 0 },
    {   PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
    {   PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
    {   PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
    {   PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
    {   PV_FILENO_SVAGGFIL, (void **)&ps_g_SVAGGFIL, 0, 0, 0 },
    {   PV_FILENO_CTLJHFIL, (void **)&ps_g_CTLJHFIL, 0, 0, 0 },
    {   PV_FILENO_PVCTLFIL, (void **)&ps_g_PVCTLFIL, 0, 0, 0 },
	{	PV_FILENO_EMCTLFIL, (void **)&ps_g_EMCTLFIL, 0, 0, 0 },
    {   PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	PV_FILENO_PWPLNFIL, (void **)&ps_g_PWPLNFIL, 0, 0, 0 },	/* [20210128] Add */
	{	PV_FILENO_IOTCNINF, (void **)&ps_g_IOTCNINF, 0, 0, 0 },
    {   PV_FILENO_PGCONINF, (void **)&ps_g_PGCONINF, 0, 0, 0 },
    {   PV_FILENO_SCHCTLFIL,(void **)&ps_g_SCHCTLFIL,0, 0, 0 },
    {   PV_FILENO_FCRSTINF, (void **)&ps_g_FCRSTINF, 0, 0, 0 },	/* [20251218] Add */
    {   PV_FILENO_FCRRPINF, (void **)&ps_g_FCRRPINF, 0, 0, 0 },	/* [20251218] Add */
    {   0,                                     NULL, 0, 0, 0 }
};

struct st_machineName	s_machineName[] =			/* 装置名称 */
{
	{ "収集装置","収集装置" },
	{ "バッテリ","バッテリー" },
	{ "パワコン","パワーコンディショナ" },
	{ "メーター","マルチメータ" },
	{ "汎用ＩＯ","汎用リモートＩＯ装置" },
	{ "ＰＬＣ装","ＰＬＣ装置" },
	{ "","" },
	{ "","" },
	{ "","" },
	{ "","" },
	{ "","" },
};
char c_powerGridName[PV_SCH_POWERGRID_MAX][32] = {  "北海道電力"
                                                        ,"東北電力"
                                                        ,"東京電力"
                                                        ,"中部電力"
                                                        ,"北陸電力"
                                                        ,"関西電力"
                                                        ,"中国電力"
                                                        ,"四国電力"
                                                        ,"九州電力"
                                                        ,"沖縄電力"
                                                    };
char c_resorceTypeName[AWS_RESORCE_MAX][32] = {     "火力発電"
                                                   ,"水力発電"
                                                   ,"揚水発電"
                                                   ,"蓄電池"
                                                   ,"VPP(発電)"
                                                   ,"VPP(発電+需要)"
                                                   ,"VPP(需要)"
                                                    };
char c_batteryTypeName[BT_TYPE_MAX][48] = {         "蓄電所（自立なし）"
                                                   ,"蓄電所（自立あり）"
                                                   ,"需要家蓄電池（切替自動）"
                                                   ,"需要家蓄電池（切替手動）"
                                                    };


char c_strData[72];
													/* 制御モード名称 */
char c_ctlModeName[8][64] = { "----", "停止中", "待機中", "自立運転中", "連系運転中", "待機中（障害発生）", "自立運転中（障害発生）", "連系運転中（障害発生）" };

char c_switchModeName[6][64] = { "未設定", "遠隔制御", "自立運転切替", "連系運転切替１", "連系運転切替2", "連系運転切替3" };

													/* 計測種別 */
char c_tmKindName[4][16] = { "----", "瞬時", "積算", "平均" };

													/* ファイル定義テーブル */
struct st_fileIndex		s_fileIndex2[PV_MAX_MKIND];

struct winsize			termSize;					/* 端末の画面サイズ */

static int				mainte_flg = 0;				/* モニタ専用フラグ */

static int				signal_flg = 0;				/* シグナル受信フラグ */

static int				viewTime = 50;				/* キー入力待ち時間(初期値5秒) */
static int				pvsch_flg = 0;				/* スケジュール管理機能ありフラグ */
static int				awsiot_flg = 0;				/* AWS IoT-Core機能ありフラグ */


#define				DEBUG_TM_LOG_MAX		20
struct st_tmLogInfo{						/* 計測ログ情報 */
	pthread_t		threadID;				/* スレッドＩＤ */
	int				status;					
	char			inputFilePath[128];		
	char			outputDir[128];	
	char			outputFilePath[128];	
	int				cycleTim;				
	struct st_tminfo {
		int				statFlg;			/* 実装/未実装 */
		int				tmIfLink;			/* 計測情報リンク情報 */
		short			soKind;				/* 装置種別 */
		short			soNumb;				/* 装置番号 */
		short			tmNumb;				/* 計測番号 */
	} s_tminfo[DEBUG_TM_LOG_MAX];
};
struct st_dbg_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};
struct st_tmLogInfo		s_tmlogInfo;

/* [20250807] Add St */
struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};
/* [20250807] Add Ed */

#define GET_OUT_LIMIT(ws_row, no_out) ( ws_row < no_out ? 1 : (ws_row - no_out ))

/* シグナル受信時の処理 */
void	func_signal_recv( int signum )
{
	/* シグナル受信フラグセット */
	signal_flg = 1;
}

/* 関数定義 */
/* 注意----------------------------------------------------------------------------------------- */
/* main、func_line_id_get、func_display_machineinfo 関数は装置種別が増加した場合は追加改修が必要 */
/* --------------------------------------------------------------------------------------------- */
int	func_nkmanage_pidget( void );
void	func_line_id_get( int, int, int *, int * );
void	func_display_machineinfo( int, int );
int		func_display_menu();
int		func_display_menu_guest();
void	func_display_proc();
void	func_display_line();
void	func_display_pvsvrinf();
void	func_display_pgconinf();
void	func_display_iotconfinfo();
void	func_display_tm( int );
void	func_display_unit( short );
void	func_display_sv( int );
void	func_display_st( int );
void	func_display_aggtminfo( );
void	func_display_aggsvinfo( );
void	func_display_tmmoninfo( );
void	func_display_logsetting( );
void	func_display_emsorderinfo( );
void	func_display_ManualMode( );
void	func_display_pvManual( );
void	func_display_btManual( );
void	func_display_timesetting( );
void	func_display_setupdatetime( );				/* [Ver5.02] Add */
void	func_display_pvctlinfo();
void	func_display_emctlinfo();
int		get_list_data( char *, int *, int );	/* [20240226] Add */
void	func_display_rcvpowplninfo();	/* [20210128] Add  */
void	func_display_pvctldmdinfo( );
void	func_johenmsg_out( int, int );
void	func_ctljohenmsg_out( int, int );
void	func_syslog_out( int, int  );
void	func_termsize_get();
void	func_displaysize_set();
void	func_display_setoption( );				/* [20230224] Add */
void	func_pvmode_change();
int		func_display_pvschdays( int, int, int *, int *, int * );
int		func_display_get_days( int, int );
void	func_display_modectrlinfo(  );
void	func_display_syste_status_en();
void	func_pvmode_change_en( );
void	func_pvmode_shutdown_en( int );
void	func_display_demandinfo( );
void	func_display_cemsdinfo( );
void	func_display_rcvpowerinfo( );
void	func_display_fcrparaminfo( );	/* [20251218] Add */
void	func_display_overchargeinfo( );
void	func_tmlogsetting();
int 	func_dbg_sep_data( char *, struct st_dbg_sepStr * );
void 	*func_tmlog_thread( void *args );
void	func_display_pgconinf();
void	func_display_pvschinfo();
void 	func_mesasge_view_wait( void );
void 	func_display_ctrlinfo( struct st_CTLINF * );
void	func_display_cmn_strlenBlank( int, char *);
/* [20250807] Add St */
void	func_display_freqsim_set( void );
int		func_sep_data( char *, struct st_sepStr * );
time_t	func_cmn_mktime( int , int , int , int , int , int);
void	func_display_freq_maxpow_set( void );
/* [20250807] Add Ed */

/* 外部関数定義 */
extern	long long commfunc_pow10abs( short );
double	func_cmn_ConvEnzan( int , short , long long , long long  );
void func_cmn_setTimeStr( int cnvPtn, struct timeval *cnvtime, char *cnvStr );


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 主処理
 *
 * 機能：
 *		オンラインデバッガのメニューを表示し、入力されたメニュー番号の機能を呼び
 *		出す。メニューに表示する内容は外部機器の実装状況に合わせて表示する。
 *
 */
int	main(int argc, char *argv[])
{
	char					keyData[1024];
	int						keyByte;
	int						menuNumb;
	int						retCode;
	int						loopCnt1;
	int						fileCnt = 0;
	int						msgLimit;
	int						nkmanagePid;
	int						mutexCreate = 0;
	uid_t					user_id;

	struct sigaction		sigact;

	memset( &sigact, 0, sizeof(sigact) );
	sigemptyset( &sigact.sa_mask );
	sigaddset( &sigact.sa_mask, SIGUSR1 );
	sigact.sa_handler  = func_signal_recv;
	sigact.sa_flags    = SA_RESETHAND;
	sigact.sa_restorer = 0;

	memset(&s_tmlogInfo, 0, sizeof(s_tmlogInfo));

	if( sigaction( SIGUSR1, &sigact, NULL ) != 0 )
	{
		perror("SYSDEBUG: sigacion");
		exit( 1 );
	}
	

	/* 起動ユーザＩＤを判定 */
	user_id = getuid();
	/* root または nksystem ユーザ以外の場合 */
	if( (user_id != 0) && (user_id != 500) )
	{
		/* メンテナンスユーザを無効にする */
		/* メンテナンス専用ユーザフラグＯＮ */
		//mainte_flg = 1;
	}

	/* 自分のプロセスの優先度を下げる */
	setpriority( PRIO_PROCESS, 0, 10 );

	/* 端末のサイズを取得 */
	func_termsize_get();

	/* ミューテックス領域をアタッチ */
	if( commfunc_mutex_attach() != 0 )
	{
		/* ミューテックス領域を作成 */
		if( commfunc_mutex_create() != 0 )
		{
			printf("%s : File Resource Create Error!\n", OWN_PROC_NAME );
			return( -1 );
		}
		/* ミューテックス作成フラグ設定 */
		mutexCreate = 1;
	}

	/* 使用ファイルをオープンしてＭＭＡＰ化 */
	retCode = commfunc_file_open( s_fileIndex );
	if(retCode < 0)
	{
		printf("%s : File Resource Open Error!\n", OWN_PROC_NAME );
		return( -1 );
	}

	/* ＭＭＡＰ化するファイルを特定する（通信対象装置の実装状態に併せてファイルを設定する） */
	memset( s_fileIndex2, 0, sizeof(s_fileIndex2) );
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_BATTE].machineCnt !=  0 )
	{
		/* バッテリーが存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_BATTEINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_BATTEINF;
		fileCnt++;
	}
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].machineCnt !=  0 )
	{
		/* パワーコンディショナが存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_POWCNINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_POWCNINF;
		fileCnt++;
	}
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_MLTMT].machineCnt !=  0 )
	{
		/* スマート計測監視機器が存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_MLTMTINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_MLTMTINF;
		fileCnt++;
	}
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_REMIO].machineCnt !=  0 )
	{
		/* 汎用リモートＩＯ装置が存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_REMIOINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_REMIOINF;
		fileCnt++;
	}
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_PLCFA].machineCnt !=  0 )
	{
		/* ＰＬＣ装置が存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_PLCFAINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_PLCFAINF;
		fileCnt++;
	}

	/* ストッパーを設定 */
	s_fileIndex2[fileCnt].fileNumb = 0;
	s_fileIndex2[fileCnt].point    = NULL;

	/* 使用ファイルをオープンしてＭＭＡＰ化 */
	retCode = commfunc_file_open( s_fileIndex2 );
	if(retCode < 0)
	{
		/* 使用ファイルをクローズ */
		commfunc_file_close( s_fileIndex );
		printf("%s : File Resource2 Open Error!\n", OWN_PROC_NAME );
		return( -1 );
	}

	/* スケジュール管理プロセス起動設定有りの場合 */
//	if( ps_g_PGCONINF->statFlag != 0 )	/* [20251027] Del */
	{
		/* スケジュール管理プロセス定義有り */
		pvsch_flg = 1;
	}
	/* AWS IoT-Core機能設定有りの場合 */
	if( ps_g_IOTCNINF->statFlag != 0 )
	{
		/* AWS IoT-Core定義有り */
		awsiot_flg = 1;
	}


	retCode = 1;
	while( retCode == 1 )
	{
		/* ユーザにより表示メニューを変更 */
		if( mainte_flg == 1 )
			retCode = func_display_menu_guest();
		else
			retCode = func_display_menu();
	}


	/* 使用ファイルをクローズ */
	commfunc_file_close( s_fileIndex );
	commfunc_file_close( s_fileIndex2 );

/* 先にオンラインモニタを起動していた場合、mutexを削除してしまう為、削除処理はしない */
#if 0
	/* ミューテックス領域を生成していた場合 */
	if( mutexCreate )
	{
		/* ミューテックス領域を削除 */
		commfunc_mutex_destroy();
	}
#endif
	if( signal_flg )
	{
		if( s_tmlogInfo.threadID != 0 )
		{
			s_tmlogInfo.status = 2;
			pthread_join( s_tmlogInfo.threadID, NULL );
		}
			
		/* 終了メッセージ表示 */
		printf("\n\n");
		if( mainte_flg == 1 )
		{
			printf("=======================================================\n");
			printf("Online monitor force terminated to shutdown the device.\n");
			printf("=======================================================\n");
		}
		else
		{
			printf("==========================================\n");
			printf(" 収集装置の停止または再起動処理を行うため \n");
			printf(" オンラインモニタを強制的に終了しました \n");
			printf("==========================================\n");
		}
		printf("\n\n");
		fflush(NULL);
		sleep( 2 );
	}

	/* 正常終了 */
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 所属回線番号と装置ID取得
 *
 * 機能：
 *		引数 mKind で指定された装置種別と引数 soutiNumb で指定された装置番号から
 *		所属回線番号と装置IDを取得する。
 *		所属回線番号は引数 plineNumb に格納し、装置IDは引数 psoutiId に格納する。
 *		装置番号 soutiNumb は、0から始まる点に注意
 */
void	 func_line_id_get( int mKind, int soutiNumb, int *plineNumb, int *psoutiId )
{
	/* 装置種別毎の処理 */
	switch( mKind )
	{
		case PV_MKIND_PVSYS:		/* （収集装置の場合） */
			*plineNumb = 0;
			*psoutiId  = 1;
			break;
		case PV_MKIND_POWCN:		/* （パワーコンディショナの場合） */
			*plineNumb = (ps_g_POWCNINF + soutiNumb)->lineNumb;
			*psoutiId  = (ps_g_POWCNINF + soutiNumb)->idNumb;
			break;
		case PV_MKIND_MLTMT:		/* （スマート計測監視機器の場合） */
			*plineNumb = (ps_g_MLTMTINF + soutiNumb)->lineNumb;
			*psoutiId  = (ps_g_MLTMTINF + soutiNumb)->idNumb;
			break;
		case PV_MKIND_BATTE:		/* （バッテリーの場合） */
			*plineNumb = (ps_g_BATTEINF + soutiNumb)->lineNumb;
			*psoutiId  = (ps_g_BATTEINF + soutiNumb)->idNumb;
			break;
		case PV_MKIND_REMIO:		/* （汎用リモートＩＯ装置の場合） */
			*plineNumb = (ps_g_REMIOINF + soutiNumb)->lineNumb;
			*psoutiId  = (ps_g_REMIOINF + soutiNumb)->idNumb;
			break;
		case PV_MKIND_PLCFA:		/* （ＰＬＣ装置の場合） */
			*plineNumb = (ps_g_PLCFAINF + soutiNumb)->lineNumb;
			*psoutiId  = (ps_g_PLCFAINF + soutiNumb)->idNumb;
			break;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 装置情報表示
 *
 * 機能：
 *		引数 mKind で指定された装置種別と引数 soutiNumb で指定された装置番号に関
 *		する装置情報を表示する。
 *		表示する情報は以下の通り
 *		・所属回線番号、装置ＩＤ、装置種別、装置番号、型式名称、装置名称
 */
void	 func_display_machineinfo( int mKind, int soutiNumb )
{
	char		typeName[32];		/* 名称表示用変数 */

	/* 内容を初期化する */
	memset( typeName, 0, sizeof(typeName) );
	
	switch( mKind )
	{
		case PV_MKIND_BATTE:		/* （バッテリーの場合） */
			strncpy( typeName, (ps_g_BATTEINF + soutiNumb)->typeName, 20 );
			printf(" %2d  %3d  %s  %2d  %-20s %s\n",
				(ps_g_BATTEINF + soutiNumb)->lineNumb,
				(ps_g_BATTEINF + soutiNumb)->idNumb,
				s_machineName[mKind].nameShort,
				soutiNumb + 1,
				typeName,
				(ps_g_BATTEINF + soutiNumb)->batteName );
			break;
		case PV_MKIND_POWCN:		/* （パワーコンディショナの場合） */
			strncpy( typeName, (ps_g_POWCNINF + soutiNumb)->typeName, 20 );
			printf(" %2d  %3d  %s  %2d  %-20s %s\n",
				(ps_g_POWCNINF + soutiNumb)->lineNumb,
				(ps_g_POWCNINF + soutiNumb)->idNumb,
				s_machineName[mKind].nameShort,
				soutiNumb + 1,
				typeName,
				(ps_g_POWCNINF + soutiNumb)->powcnName );
			break;
		case PV_MKIND_MLTMT:		/* （スマート計測監視機器の場合） */
			strncpy( typeName, (ps_g_MLTMTINF + soutiNumb)->typeName, 20 );
			printf(" %2d  %3d  %s  %2d  %-20s %s\n",
				(ps_g_MLTMTINF + soutiNumb)->lineNumb,
				(ps_g_MLTMTINF + soutiNumb)->idNumb,
				s_machineName[mKind].nameShort,
				soutiNumb + 1,
				typeName,
				(ps_g_MLTMTINF + soutiNumb)->mltmtName );
			break;
		case PV_MKIND_REMIO:		/* （汎用リモートＩＯ装置の場合） */
			strncpy( typeName, (ps_g_REMIOINF + soutiNumb)->typeName, 20 );	/* [Ver1.13] Add */
			printf(" %2d  %3d  %s  %2d  %-20s %s\n",
				(ps_g_REMIOINF + soutiNumb)->lineNumb,
				(ps_g_REMIOINF + soutiNumb)->idNumb,
				s_machineName[mKind].nameShort,
				soutiNumb + 1,
				typeName,									/*	[Ver1.13] Chg	*/
				(ps_g_REMIOINF + soutiNumb)->remioName );
			break;
		case PV_MKIND_PLCFA:		/* （ＰＬＣ装置の場合） */
			strncpy( typeName, (ps_g_PLCFAINF + soutiNumb)->typeName, 20 );
			printf(" %2d  %3d  %s  %2d  %-20s %s\n",
				(ps_g_PLCFAINF + soutiNumb)->lineNumb,
				(ps_g_PLCFAINF + soutiNumb)->idNumb,
				s_machineName[mKind].nameShort,
				soutiNumb + 1,
				typeName,
				(ps_g_PLCFAINF + soutiNumb)->plcfaName );
			break;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * トップメニュー表示
 *
 * 機能：
 *		トップメニュー画面を表示する。
 */
int	func_display_menu()
{
	char					keyData[1024];
	char					verStr[32];
	int						keyByte;
	int						menuNumb;
	int						loopCnt1;
	int						nkmanagePid;

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* NKMANAGEプロセスのpidを取得 */
	nkmanagePid = func_nkmanage_pidget();

	if( commfunc_get_soft_ver( verStr ) != 0 )
	{
		sprintf( verStr, "UnKnown");
	}
	

	printf("= 収集装置オンラインモニタ ================================ [Version:%8s] =\n", verStr );
	printf("収集装置ID:[%s]  運転状態:", ps_g_PVSYSINF->siteIdName );
	if(nkmanagePid > 0)
		printf("[動作中]  ");
	else
		printf("[停止中]  ");
	if( ps_g_PVSYSINF->dtsdLock == 0 )
		printf("データ送信ロック状態:[平常]\n");
	else
		printf("データ送信ロック状態:[ロック]\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("   1.業務プロセス動作状態表示\n");
	printf("   2.通信回線設定・接続装置確認\n");
	printf("   3.モニタリングサーバ情報確認\n");
	printf("   4.CEMS制御情報確認\n");
	printf("   5.受電電力一定制御情報確認\n");
	printf("   6.一次調整力パラメータ情報確認\n");	/* [20251218] Chg */
	printf("   7.モード切替時制御情報確認確認\n");
	if( awsiot_flg == 1 )
		printf("   8.IoT設定情報\n");
	if( pvsch_flg == 1 )
		printf("   9.電力会社向け情報確認\n");
	printf("  ----------------------------------------------------------------------------\n");
	for( loopCnt1 = 0 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt > 0 )
			printf("  %02d.装置状態表示（%s）\n", loopCnt1 + 10, s_machineName[loopCnt1].nameFull );
	}
	printf("  ----------------------------------------------------------------------------\n");
	for( loopCnt1 = 1 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt > 0 )
			printf("  %02d.通信ステータス状態表示（%s）\n", loopCnt1 + 20, s_machineName[loopCnt1].nameFull );
	}
	printf("  ----------------------------------------------------------------------------\n");

	/* デバッガ専用ユーザの場合は収集装置本体の計測情報（集約計測情報）は表示しない */
	if( mainte_flg == 0 ) loopCnt1 = 0; else loopCnt1 = 1;
	for( ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
	{
		if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt > 0 )
			printf("  %02d.計測値表示（%s）\n", loopCnt1 + 30, s_machineName[loopCnt1].nameFull );
	}
	if( mainte_flg == 0 )
	{
		printf("  ----------------------------------------------------------------------------\n");
		printf("  40.集約計測情報表示\n");
		printf("  41.集約状態情報表示\n");
		printf("  42.計測監視情報表示\n");
		printf("  ----------------------------------------------------------------------------\n");
/* [20250807] Chg St */
		printf("  70.CEMS制御管理情報表示                71.制御状態管理情報表示\n");
		if( awsiot_flg == 1 )
		printf("  72.受電電力計画値表示                  73.ACシステム指令情報表示\n");
		else
		printf("  72.受電電力計画値表示\n");
		if( pvsch_flg == 1 )
		printf("  74.スケジュール管理情報表示            75.模擬周波数設定\n");
		else
		printf("                                         75.模擬周波数設定\n");
/* [20250807] Chg Ed */
	}
	printf("  ----------------------------------------------------------------------------\n");
	printf("  80.通信ログ設定                        81.画面サイズ設定（現在値:%03dx%03d）\n", termSize.ws_col, termSize.ws_row );
	printf("  82.オプション設定                      83.画面更新時間設定（現在値:%d秒）\n", viewTime/10);
	if( mainte_flg == 0 )
	{
		//printf("  83.計測ログ取得\n");
	}
	printf("  88.時刻設定                            89.シャットダウン／リブート\n");
	if( mainte_flg == 0 )
	{
		if( (ps_g_PVSYSINF->pvManualMode == 1) || (ps_g_PVSYSINF->btManualMode == 1) )
		{
			printf("  ----------------------------------------------------------------------------\n");
			if( ps_g_PVSYSINF->pvManualMode == 1 )
				printf("  91.ＰＶ出力制御設定                  ");
			if( ps_g_PVSYSINF->btManualMode == 1 )
				printf("  92.ＢＴ充放電電力設定");
			printf("\n");
			
		}
		//printf("  83.計測ログ取得\n");
	}
	printf("  ----------------------------------------------------------------------------\n");
	printf("  99.終了\n");
	printf("================================================================================\n");
	printf("番号を選択してください（改行のみの場合はメニューを再表示） >> ");
	fflush( NULL );

	if( signal_flg ) 
		/* ツールを終了 */
		return 0;

	memset( keyData, 0, sizeof( keyData ) );
	keyByte = read( 0, &keyData, 1024 );

	if(keyByte < 0 )
		/* ツールを終了 */
		return 0;

	/* 入力値を数値化 */
	menuNumb = atoi( keyData );

	/* 入力された番号により各機能を呼び出す */
	if( menuNumb == 1 )
		/* プロセス動作状態を表示 */
		func_display_proc();
	else if( menuNumb == 2 )
		/* 通信回線設定確認表示 */
		func_display_line();
	else if( menuNumb == 3 )
		/* モニタリングサーバ情報確認表示 */
		func_display_pvsvrinf();
	else if( ( menuNumb == 4 ) && ( mainte_flg == 0 ) )
		/* CEMS制御情報表示 */
		func_display_cemsdinfo( );
	else if( ( menuNumb == 5 ) && ( mainte_flg == 0 ) )
		/* 受電電力一定制御情報表示 */
		func_display_rcvpowerinfo( );
	else if( ( menuNumb == 6 ) && ( mainte_flg == 0 ) )
/* [20251218] Chg St */
		/* 一次調整力パラメータ情報表示 */
		func_display_fcrparaminfo( );
/* [20251218] Chg Ed */
	else if( ( menuNumb == 7 ) && ( mainte_flg == 0 ) )
		/* モード切替時制御情報表示 */
		func_display_modectrlinfo( );
	else if( ( menuNumb == 8 ) && ( mainte_flg == 0 ) && ( awsiot_flg == 1))
		/* IoT設定情報表示 */
		func_display_iotconfinfo();
	else if( ( menuNumb == 9 ) && ( mainte_flg == 0 ) && ( pvsch_flg == 1) )
		/* 電力会社向け情報確認表示 */
		func_display_pgconinf();
	else if( ( menuNumb >= 10 ) && ( menuNumb <= 19 ) )
	{
		/* 装置状態表示（対象装置が実装の場合） */
		if( ps_g_PVSYSINF->s_machineInfo[menuNumb % 10].machineCnt != 0 )
			func_display_sv( menuNumb % 10 );
	}
	else if( ( menuNumb >= 21 ) && ( menuNumb <= 29 ) )
	{
		/* 通信ステータス状態表示（対象装置が実装の場合） */
		if( ps_g_PVSYSINF->s_machineInfo[menuNumb % 20].machineCnt != 0 )
			func_display_st( menuNumb % 20 );
	}
	else if( ( menuNumb >= 30 ) && ( menuNumb <= 39 ) )
	{
		/* メンテナンス専用ユーザの場合は収集装置本体の計測値表示は選択不可 */
		if(( mainte_flg == 1 ) && ( menuNumb == 30 )) 
			return 1;
		/* 計測値表示（対象装置が実装の場合） */
		if( ps_g_PVSYSINF->s_machineInfo[menuNumb % 30].machineCnt != 0 )
			func_display_tm( menuNumb % 30 );
	}
	else if( menuNumb == 40 )
	{
		if( mainte_flg == 0 )
		{
			/* 集約計測情報 */
			func_display_aggtminfo( );
		}
	}
	else if( menuNumb == 41 )
	{
		if( mainte_flg == 0 )
		{
			/* 集約状態情報 */
			func_display_aggsvinfo( );
		}
	}
	else if( menuNumb == 42 )
	{
		if( mainte_flg == 0 )
		{
			/* 計測監視情報 */
			func_display_tmmoninfo( );
		}
	}
	else if( ( menuNumb == 70 ) && ( mainte_flg == 0 ) )
	{
		/* CEMS制御状態表示 */
		func_display_emctlinfo( );
	}
	else if( ( menuNumb == 71 ) && ( mainte_flg == 0 ) )
	{
		/* 制御状態表示 */
		func_display_pvctlinfo( );
	}
/* [20210128] Add St */
	else if( ( menuNumb == 72 ) && ( mainte_flg == 0 ) )
	{
		/* 受電電力計画値 */
		func_display_rcvpowplninfo( );
	}
/* [20210128] Add Ed */
//	else if( ( menuNumb == 73 ) && ( mainte_flg == 0 ) )
//	{
//		/* デマンド制御情報表示 */
//		func_display_pvctldmdinfo();
//	}
	else if( ( menuNumb == 73 ) && ( awsiot_flg == 1) && ( mainte_flg == 0 ) )
	{
		/* EMS制御指示情報表示 */
		func_display_emsorderinfo( );
	}
	else if( ( menuNumb == 74 ) && ( pvsch_flg == 1 ) && ( mainte_flg == 0 ) )
	{
		/* スケジュール情報表示 */
		func_display_pvschinfo( );
	}
/* [20250807] Add St */
	else if( menuNumb == 75 )
		/* 模擬周波数設定 */
		func_display_freqsim_set( );
/* [20250807] Add Ed */
	else if( menuNumb == 80 )
		/* シリアル通信ログ出力設定 */
		func_display_logsetting( );
	else if( menuNumb == 81 )
		/* 画面サイズ設定 */
		func_displaysize_set( );
/* [20230224] Add St */
	else if( ( menuNumb == 82 ) && ( mainte_flg == 0 ) )
		/* オプション設定 */
		func_display_setoption( );
/* [20230224] Add Ed */
	else if( ( menuNumb == 83 ) && ( mainte_flg == 0 ) )
		/* 画面更新時間設定 */
		func_display_setupdatetime( );
	else if( menuNumb == 88 )
		/* 時刻設定 */
		func_display_timesetting( );
	else if( menuNumb == 89 )
		/* 収集装置シャットダウン／リブート */
		func_pvmode_change( );
	else if (menuNumb == 99 )
		/* ツールを終了 */
		return 0;
	/* 特別なコードを入力した場合はメンテナンスモードを切り換える */
	else if( menuNumb == 55581 )
		/* メンテナンスモードを切替 */
		if ( mainte_flg == 0 ) mainte_flg = 1; else mainte_flg = 0;
	else if( menuNumb == 1234 )
		func_display_ManualMode();
	else if( menuNumb == 91 )
		func_display_pvManual( );
	else if( menuNumb == 92 )
		func_display_btManual( );
/* [20250807] Add St */
	else if( menuNumb == 119 )
		func_display_freq_maxpow_set( );
/* [20250807] Add Ed */
	/* 正常終了 */
	return 1;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * トップメニュー表示（ゲストアカウント）
 *
 * 機能：
 *		トップメニュー画面を表示する。
 */
int	func_display_menu_guest()
{
	char					keyData[1024];
	int						keyByte;
	int						menuNumb;
	int						loopCnt1;
	int						nkmanagePid;
	int						retCode = 0;


	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

//	func_pvmode_shutdown_en();
//	return 0;


	/* NKMANAGEプロセスのpidを取得 */
	nkmanagePid = func_nkmanage_pidget();

	printf("= Online Monitor ============================================== [Version:%s] =\n", OWN_VERSION );
	printf("Operating condition:");
	if(nkmanagePid > 0)
		printf("[Run]\n");
	else
		printf("[Stop]\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("   1.System status summary\n");
	printf("   6.Shutdown\n");
	printf("   7.Reboot\n");
	printf("  99.Quit\n");
	printf("================================================================================\n");
	printf("Please Input Number >> ");
	fflush( NULL );

	if( signal_flg ) 
		/* ツールを終了 */
		return 0;

	memset( keyData, 0, sizeof( keyData ) );
	keyByte = read( 0, &keyData, 1024 );

	if(keyByte < 0 )
		/* ツールを終了 */
		return 0;

	/* 入力値を数値化 */
	menuNumb = atoi( keyData );

	/* 入力値を数値化 */
	menuNumb = atoi( keyData );
	if( menuNumb == 1 )
		/* プロセス動作状態を表示 */
		func_display_syste_status_en();
	else if( menuNumb == 6 )
		/* 収集装置シャットダウン／リブート */
		func_pvmode_shutdown_en(0);
	else if( menuNumb == 7 )
		func_pvmode_shutdown_en(1);
	else if (menuNumb == 99 )
		/* ツールを終了 */
		return 0;
	/* 特別なコードを入力した場合はメンテナンスモードを切り換える */
	else if( menuNumb == 55581 )
		/* メンテナンスモードを切替 */
		if ( mainte_flg == 0 ) mainte_flg = 1; else mainte_flg = 0;

	/* 正常終了 */
	return 1;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * プロセス動作状態表示
 *
 * 機能：
 *		各プロセスの動作状態をPVSYSINFファイルの情報を元に５秒周期で表示を行う。
 *		５秒周期は１回の表示が終了してから５秒経過後に表示する形式
 *		プロセスの動作状態を表示した後、端末画面下部に余裕がある場合は、状変メ
 *		ッセージとシステムログを表示する
 */
void	func_display_proc()
{
	int						msgLimit1;				/* 状変メッセージ表示行数 */
	int						msgLimit2;				/* システムログ表示行数 */
	int						loopCnt1;				/* ループカウンタ */
	int						nkmanagePid;			/* NKMANAGEプロセスPID */
	char					inKeyData;				/* キー入力値 */
	char					readByte;				/* キー入力バイト数 */
	struct termio			ttyBackup;				/* 端末設定保存用 */
	struct termio			ttyChange;				/* 端末設定変更用 */
	struct tm				localTime;				/* 現在時刻 */
	time_t					nowTime;				/* 現在時刻 */
	struct winsize			termSize_OLD;			/* 端末サイズ保存 */
	int						dispMode = 0;			/* 表示モード */
	int						offSet=0;				/* 表示開始位置用オフセット */
	int						maxNum;					/* 最大蓄積データ数 */
	int						viewLine;				/* 表示行数 */
	char					updateFile[32];			/* 設定変更ファイル名 */

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;

	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末サイズを取得 */
	func_termsize_get();
	/* 端末サイズを保存 */
	termSize_OLD.ws_row = termSize.ws_row;
	/* メッセージ表示行数を決定 */
	msgLimit1 = (termSize.ws_row - 20 ) / 2;
	msgLimit2 = msgLimit1;

	/* 画面をクリア */
	printf("\033[2J");

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末サイズを取得 */
		func_termsize_get();
		/* 端末サイズ（行数）が変更された場合 */
		if( termSize_OLD.ws_row != termSize.ws_row )
		{
			/* 画面をクリア */
			printf("\033[2J");
			/* メッセージ表示行数を決定 */
			msgLimit1 = (termSize.ws_row - 20 ) / 2;
			msgLimit2 = msgLimit1;
			/* 端末サイズを保存 */
			termSize_OLD.ws_row = termSize.ws_row;
		}

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* タイトルを表示 */
		printf("= [収集装置ID:%s] ================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			ps_g_PVSYSINF->siteIdName,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* 年月日通し番号(4+2+2+4)だけを抽出 */
		//memset(updateFile, 0, sizeof(updateFile));
		//memcpy(updateFile, &ps_g_PVSYSINF->updateConfFile[8], 12 );
			
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		/* ロック状態を表示 */
/* [20240110] Chg St */
		printf("状変ロック[%s]   設定変更ファイル[%s]\n",
			(commfunc_manage_johenLockFlagGet() == 0)? "平常" : "ロック", (ps_g_PVSYSINF->updateConfFile[0] == 0)? "なし" : ps_g_PVSYSINF->updateConfFile );
/* [20240110] Chg Ed */

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* プロセス動作状態の見出しを表示 */
		printf("No PROCNAME   PID  E R S R A W C WC  RC No PROCNAME   PID  E R S R A W C WC  RC\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 管理プロセス（NKMANAGE）の動作状態を表示 */
		if( (nkmanagePid = func_nkmanage_pidget()) > 0 )
			printf(" 0 nkmanage %5d  - - - - - - - --- --", nkmanagePid);	/* */
		else
			printf(" 0 nkmanage  stop  - - - - - - - --- --", nkmanagePid);	/* */
		printf(" -- -------- -----  - - - - - - - --- --\n", nkmanagePid);	/* */
			
		/* 通信系プロセスと業務系プロセスの動作状態をPVSYSINFファイルの内容を元に表示 */
		for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_PROC ; loopCnt1++ )
		{
			/* カーソルの位置を調節 */
			printf("\033[%d;%dH",6 + loopCnt1 % 10,(loopCnt1 / 10) * 41);

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* プロセス動作状態の表示 */
			printf("%2d %-8s %5d  %d %d %d %d %d %d %d %03d %02d\n",	/* */
				loopCnt1+1,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].procName,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].processID,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].execFlag,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].restFlag,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].stopFlag,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].runFlag,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].abnFlag,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].wdogFlag,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].ctlInst,		/* */
				ps_g_PVSYSINF->s_procInfo[loopCnt1].wdogCnt,
				ps_g_PVSYSINF->s_procInfo[loopCnt1].reStCnt );
		}

		if( dispMode == 0 )
		{
			/* 状変メッセージ処理 */
			func_johenmsg_out( msgLimit1, offSet );

			/* システムログメッセージ処理 */
			func_syslog_out( msgLimit2, offSet );
		
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る:[q]       表示切替:[d]                                           (5秒更新)");
		}
		else
		{
			if( dispMode == 1 )
			{
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");

				/* 状変メッセージ処理 */
				func_johenmsg_out( msgLimit1+msgLimit2, offSet );
			}
			else if( dispMode == 2 )
			{
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");

				/* システムログメッセージ処理 */
				func_syslog_out( msgLimit1+msgLimit2, offSet );
			}
			else //if( dispMode == 3 )
			{
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");

				/* 制御ログメッセージ処理 */
				func_ctljohenmsg_out( msgLimit1+msgLimit2, offSet );
			}

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る:[q]       表示切替:[d]       頁切替:[前:b / 次:n]                (5秒更新)");
		}

		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );

		if( readByte != 0 )
		{
			/* read関数がエラーとなった場合処理を抜ける */
			if( readByte < 0 ) break;
			/* 終了のキー入力の場合処理を抜ける */
			if( (inKeyData == 'q' ) || (inKeyData == 'Q') ) break;

			/* キー入力が表示切替の場合は表示モードを切り替える */
			if( (inKeyData == 'd') || (inKeyData == 'D') )
			{
				dispMode = (dispMode + 1 ) % 4;
				offSet=0;
				if( dispMode == 1 )
					maxNum = PV_MAX_JOHEN;
				else if ( dispMode == 2 )
					maxNum = PV_MAX_LOG;
				/* 画面をクリア */
				printf("\033[2J");
			}

			/* キー入力が頁切替の場合は表示頁を切り替える */
			if( (inKeyData == 'b') || (inKeyData == 'B' ) )
			{
				if( dispMode == 0 )
					viewLine = msgLimit1;
				else
					viewLine = (msgLimit1+msgLimit2);

				offSet -= viewLine;
				if( offSet < 0 )
				{
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			if( (inKeyData == 'n') || (inKeyData == 'N' ) )
			{
				if( dispMode == 0 )
					viewLine = msgLimit1;
				else
					viewLine = (msgLimit1+msgLimit2);

				if( (offSet + viewLine) < maxNum )
				{
					offSet += viewLine;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 端末サイズ取得
 *
 * 機能：
 *		オンラインデバッガが動作している端末のサイズ（縦・横）を取得する。
 *		サイズが 80x24 以下の場合は、80x24 として扱う。
 */
void	func_termsize_get()
{
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize );
	/* 端末サイズが取得できなかった場合は暫定的に 80x24 とする */
	if( termSize.ws_col == 0 ) termSize.ws_col = 80;
	if( termSize.ws_row == 0 ) termSize.ws_row = 24;
	/* 端末サイズが 80x24 以下の場合は 80x24 とする */
	if( termSize.ws_col < 80 ) termSize.ws_col = 80;
	if( termSize.ws_row < 24 ) termSize.ws_row = 24;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * システムログ表示
 *
 * 機能：
 *		引数 logCnt で指定された行数分システムログファイルからメッセージを取り出
 *		して画面に表示する
 */
void	func_syslog_out( int logCnt, int offset )
{
	int						msgCnt;				/* メッセージ数 */
	int						loopCnt;			/* ループカウンタ */
	int						getCnt;				/* 取得メッセージ数 */
												/* ログメッセージ格納用領域 */
	char					logmsg[PV_MAX_LOG][PV_MSG_SIZE_LOG];
	int						viewPoint;			/* 表示位置 */

	memset(logmsg, 0, sizeof(logmsg));

	/* 取得メッセージ数を調整する */
	if( (logCnt + offset) <= PV_MAX_LOG )
	{
		getCnt = logCnt + offset;
	}
	else
	{
		getCnt = PV_MAX_LOG;
	}
	
	/* カーソルの位置からその行の右端までクリア */
	printf("\033[0K");

	printf("= システムログ（ %04d ～ %04d メッセージ分）====================================\n", 1 + offset, getCnt );

	/* システムログファイルからgetCnt数分のメッセージを取得する */
	msgCnt = commfunc_syslog_msgget_tail( getCnt, logmsg );

	if( ( msgCnt == PV_MAX_LOG ) && ( logCnt != ( getCnt - offset + 1 )))
	{
		viewPoint =  logCnt - ( getCnt - offset);
		for( loopCnt = 0; loopCnt < viewPoint; loopCnt++ )
		{
			printf("--/-- --:--:--.------ [--,-,--,---,-:-]\n");
		}
	}
	else
	{
		viewPoint = 0;
	}

	/* 取得したメッセージ数分表示する */
	for( loopCnt = 0; loopCnt < (logCnt-viewPoint); loopCnt++ )
	{
		/* メッセージが画面からはみ出す場合は終端を調整する */
		logmsg[loopCnt][termSize.ws_col-3] = '.';
		logmsg[loopCnt][termSize.ws_col-2] = '.';
		logmsg[loopCnt][termSize.ws_col-1] = '.';
		logmsg[loopCnt][termSize.ws_col] = 0;

		/* カーソルの位置の行をクリア */
		printf("\033[2K");

		/* ログメッセージを表示 */
		printf("%s\n", logmsg[loopCnt]);
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 状変メッセージ表示
 *
 * 機能：
 *		引数 johenCnt で指定された行数分状変蓄積ファイルからメッセージを取り出
 *		して画面に表示する
 */
void	func_johenmsg_out( int johenCnt, int offset )
{
	int						msgCnt;						/* メッセージ数 */
	struct st_JDATA			s_johendata[PV_MAX_JOHEN];	/* 状変メッセージ格納用領域 */
	struct timeval			johenTime;					/* 状変時刻 */
	struct tm				localTime;					/* 状変時刻 */
	int						loopCnt;					/* ループカウンタ */
	int						getCnt;						/* 取得メッセージ数 */
	int						viewPoint;					/* 表示位置 */

	memset(s_johendata, 0, sizeof(s_johendata));

	/* 取得メッセージ数を調整する */
	if( (johenCnt + offset) <= PV_MAX_JOHEN )
	{
		getCnt = johenCnt + offset;
	}
	else
	{
		getCnt = PV_MAX_JOHEN;
	}

	/* カーソルの位置からその行の右端までクリア */
	printf("\033[0K");

	printf("= 状変メッセージ（ %04d ～ %04d メッセージ分）==================================\n", 1 + offset, getCnt );

	/* 状変メッセージを最新 johenCnt 分取得する */
	msgCnt = commfunc_johen_msgget_tail( getCnt, s_johendata );

	if( ( msgCnt == PV_MAX_JOHEN ) && ( johenCnt != ( getCnt - offset + 1 )))
	{
		viewPoint =  johenCnt - ( getCnt - offset);
		for( loopCnt = 0; loopCnt < viewPoint; loopCnt++ )
		{
			printf("--/-- --:--:--.------ [--,-,--,---,-:-]\n");
		}
	}
	else
	{
		viewPoint = 0;
	}
	/* 状変メッセージ数分繰り返す */
	for( loopCnt = 0; loopCnt < (johenCnt-viewPoint); loopCnt++ )
	{
		/* 状変時刻を取得 */
		johenTime.tv_sec = s_johendata[loopCnt].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* カーソルの位置の行をクリア */
		printf("\033[2K");

		if( ((s_johendata[loopCnt].mKind == 0) && (s_johendata[loopCnt].mNumb == 0)) || ( s_johendata[loopCnt].sec == 0 ))
		{
			printf("--/-- --:--:--.------ [--,-,--,---,-:-]\n");
		}
		else
		{
			/* 状変日時を表示 */
			printf("%02d/%02d %02d:%02d:%02d.%06d ",
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				(int)(s_johendata[loopCnt].uSec)
				 );
			/* 状変内容（生値）を表示 */
			printf("[%s,%d,%02d,%03d,%d:%s] ",
				(s_johendata[loopCnt].svstKind == PV_SV_SVST_ST)? "ST" : "SV",
				s_johendata[loopCnt].mKind,
				s_johendata[loopCnt].mNumb,
				s_johendata[loopCnt].svstNumb,
				s_johendata[loopCnt].svstStat, (s_johendata[loopCnt].maskFlag)? "M" : "-" );

			/* 状変種別を表示（日本語表示） */
			if( s_johendata[loopCnt].svstKind == PV_SV_SVST_ST )
				printf("通信 ");
			else
				printf("装置 ");

			/* 装置種別を表示 */
			printf("%s:%02d ", s_machineName[ s_johendata[loopCnt].mKind ].nameShort, s_johendata[loopCnt].mNumb );

			/* 状態名称と動作状態を表示 */
			if( s_johendata[loopCnt].svstKind == PV_SV_SVST_ST )
			{
				printf("%s ", (ps_g_STDATINF + s_johendata[loopCnt].svstLink)->stName) ;
				if( s_johendata[loopCnt].svstStat == 1 )
					printf("［発生］");
				else
					printf("［復帰］");
			}
			else
			{
				printf("%s ", (ps_g_SVDATINF + s_johendata[loopCnt].svstLink)->svName) ;
				if( s_johendata[loopCnt].svstStat == 1 )
					printf("［１］");
				else
					printf("［０］");
			}
			printf("\n");
		}

	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 状変メッセージ表示
 *
 * 機能：
 *		引数 ctljohenCnt で指定された行数分制御状変蓄積ファイルからメッセージを
 *		取り出して画面に表示する
 */
void	func_ctljohenmsg_out( int ctljohenCnt, int offset )
{
	int						msgCnt;						/* メッセージ数 */
	struct st_CTLDATA		s_ctldata[PV_MAX_JOHEN];	/* 状変メッセージ格納用領域 */
	struct timeval			johenTime;					/* 状変時刻 */
	struct tm				localTime;					/* 状変時刻 */
	int						loopCnt;					/* ループカウンタ */
	int						getCnt;						/* 取得メッセージ数 */
	int						viewPoint;					/* 表示位置 */

	memset(s_ctldata, 0, sizeof(s_ctldata));

	/* 取得メッセージ数を調整する */
	if( (ctljohenCnt + offset) <= PV_MAX_CTLINF )
	{
		getCnt = ctljohenCnt + offset;
	}
	else
	{
		getCnt = PV_MAX_CTLINF;
	}

	/* カーソルの位置からその行の右端までクリア */
	printf("\033[0K");

	printf("= 状変メッセージ（ %04d ～ %04d メッセージ分）==================================\n", 1 + offset, getCnt );

	/* 状変メッセージを最新 ctljohenCnt 分取得する */
	msgCnt = commfunc_ctl_johen_msgget_tail( getCnt, s_ctldata );

	if( ( msgCnt == PV_MAX_CTLINF ) && ( ctljohenCnt != ( getCnt - offset + 1 )))
	{
		viewPoint =  ctljohenCnt - ( getCnt - offset);
		for( loopCnt = 0; loopCnt < viewPoint; loopCnt++ )
		{
			printf("--/-- --:--:--.------ [--,-,--,---,-:-]\n");
		}
	}
	else
	{
		viewPoint = 0;
	}
	/* 状変メッセージ数分繰り返す */
	for( loopCnt = 0; loopCnt < (ctljohenCnt-viewPoint); loopCnt++ )
	{
		/* 状変時刻を取得 */
		johenTime.tv_sec = s_ctldata[loopCnt].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* カーソルの位置の行をクリア */
		printf("\033[2K");

		if( ((s_ctldata[loopCnt].mKind == 0) && (s_ctldata[loopCnt].mNumb == 0)) || ( s_ctldata[loopCnt].sec == 0 ))
		{
			printf("--/-- --:--:--.------ [-,--,---]\n");
		}
		else
		{
			/* 状変日時を表示 */
			printf("%02d/%02d %02d:%02d:%02d.%06d ",
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				(int)(s_ctldata[loopCnt].uSec)
				 );
			/* 状変内容（生値）を表示 */
			printf("[%d,%02d,%03d] ",
				s_ctldata[loopCnt].mKind,
				s_ctldata[loopCnt].mNumb,
				s_ctldata[loopCnt].ctlNumb );

			/* 装置種別を表示 */
			printf("%s:%02d ", s_machineName[ s_ctldata[loopCnt].mKind ].nameShort, s_ctldata[loopCnt].mNumb );

			func_display_cmn_strlenBlank( 23, (ps_g_SVDATINF + s_ctldata[loopCnt].ctlLink)->svName );

			printf("%s ", c_strData );
			printf("［%s：%s］",  (s_ctldata[loopCnt].ctlKind == PV_CTL_JOHEN_KIND_SET ? "操作" : "失敗" ), ( s_ctldata[loopCnt].ctlStat == 1 ? "１" : "０"));
			if( s_ctldata[loopCnt].userNo > 0 )
			{
				printf("[User：%d]", s_ctldata[loopCnt].userNo );
			}
			printf("\n");
		}

	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 管理プロセスPID取得
 *
 * 機能：
 *		NKMANAGEプロセスのPIDを取得する
 */
int	func_nkmanage_pidget( void )
{
	FILE	*fp;						/* ファイルポインタ */
	int		pidNumb_check;				/* PID */
	char	checkCmd[512];				/* PID確認用コマンド格納領域 */
	char	checkData[512];				/* 確認データ処理用ワーク */

	memset( checkData, 0, sizeof(checkData) );

	/* 起動中のプロセス名のプロセスＩＤ（最古のもの）を取得するコマンドを設定 */
	sprintf( checkCmd, "pgrep -o nkmanage" );
	/* コマンドを実行してパイプから取得 */
	if( (fp = popen( checkCmd, "r" )) == NULL )
	{
		/* 確認コマンド実行失敗 */
		return( -1 );
	}
	/* プロセスＩＤを取得 */
	fgets( checkData, 512, fp );
	/* パイプを閉じる */
	pclose( fp );
	/* プロセスＩＤの取得 */
	pidNumb_check = atol( checkData );

	return(pidNumb_check);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 通信回線情報表示
 *
 * 機能：
 *		通信回線の設定情報および通信回線毎の接続機器の設定情報を表示する
 *
 */
void	func_display_line()
{
	char				workName[128];	/* 文字列処理用 */
	int					loopCnt1;		/* ループカウンタ */
	int					loopCnt2;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	int					dispMode = 0;	/* 表示モード */
	char				typeName[32];		/* 名称表示用変数 */
	int 				soutiNumb;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );
	while( 1 )
	{

		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 見出しの表示 */
		printf("= 通信回線設定 =================================================================\n");
		printf("回線 PROTOCOL     DEVICE DEVICE or             通信パラメータ\n");
		printf("番号 NAME________ TYPE__ IP___________________ SPEED_ DTS PAR STB TM1__ TM2_ CYCL\n");	/* [20260625] Chg */
		/* 通信回線数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_COMLINE ; loopCnt1++ )
		{
			/* 通信回線が未実装の場合は次の通信回線を処理する */
			if( (ps_g_COMLIINF + loopCnt1)->statFlag == 0 ) continue;

			/* 通信回線に定義された通信プロトコル名を取得して表示する */
			memset( workName, 0, sizeof( workName ) );
			strncpy( workName,  (ps_g_COMLIINF + loopCnt1)->protoName, 12);
			printf(" %2d  %-12s ", loopCnt1 + 1, workName );

			/* 通信回線のタイプとデバイス名（IPアドレスとポート番号）を表示 */
			if( (ps_g_COMLIINF + loopCnt1)->deviceType == PV_STYPE_TTY )
			{
				printf("SERIAL %-21s ", (ps_g_COMLIINF + loopCnt1)->targetName );
			}
			else if( (ps_g_COMLIINF + loopCnt1)->deviceType == PV_STYPE_TCP )
			{
				sprintf( workName, "%s:%d", (ps_g_COMLIINF + loopCnt1)->targetName, (ps_g_COMLIINF + loopCnt1)->portNumb );
				printf("485TCP %-21s ", workName );
			}
			else if( (ps_g_COMLIINF + loopCnt1)->deviceType == PV_STYPE_DIRTCP )
			{
				sprintf( workName, "%s:%d", (ps_g_COMLIINF + loopCnt1)->targetName, (ps_g_COMLIINF + loopCnt1)->portNumb );
				printf("TCP/IP %-21s ", workName );
			}
			else if( (ps_g_COMLIINF + loopCnt1)->deviceType == PV_STYPE_UDPDIR )
			{
				sprintf( workName, "%s:%d", (ps_g_COMLIINF + loopCnt1)->targetName, (ps_g_COMLIINF + loopCnt1)->portNumb );
				printf("UDP/IP %-21s ", workName );
			}
			else
			{
				sprintf( workName, "%s:%d", (ps_g_COMLIINF + loopCnt1)->targetName, (ps_g_COMLIINF + loopCnt1)->portNumb );
				printf("UDP/IP %-21s ", workName );
			}
			/* 通信速度とデータサイズの表示 */
			printf("%6d  %d  ", (ps_g_COMLIINF + loopCnt1)->baudRate, (ps_g_COMLIINF + loopCnt1)->dataSize );
			/* パリティーの表示 */
			if ( (ps_g_COMLIINF + loopCnt1)->parity == 0 )
				printf("NON ");
			else if ( (ps_g_COMLIINF + loopCnt1)->parity == 1 )
				printf("ODD ");
			else
				printf("EVE ");
			/* ストップビット、受信待ちタイマ、送信待ちタイマ、収集周期の表示 */
			printf(" %d  %5d %4d %4d\n",	/* [20260625] Chg */
				(ps_g_COMLIINF + loopCnt1)->stopBit,
				(ps_g_COMLIINF + loopCnt1)->rcvTime,
				(ps_g_COMLIINF + loopCnt1)->sndTime,
				(ps_g_COMLIINF + loopCnt1)->cycleTime );
		}

		/* 通信回線接続機器の表示 */
		if( dispMode == 0 )
		{
			/* 見出しの表示 */
			printf("\n");
			printf("= 接続機器一覧 =================================================================\n");
			printf("回線 装置 装置     装置 装置                 装置\n");
			printf("番号 ID__ 種別____ No__ 型式________________ 名称_______________________________\n");

			/* 収集装置を除く全ての装置分処理を行う */
			for( loopCnt1 = 1 ; loopCnt1 <= PV_MAX_MKIND ; loopCnt1++ )
			{
				/* 装置種別が未実装の場合は次の装置の処理を行う */
				if( ps_g_PVSYSINF->s_machineInfo[loopCnt1].machineCnt == 0 )
					continue;

				/* 装置最大数分繰り返す */
				for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[loopCnt1].maxMachine ; loopCnt2++ )
				{
					/* 装置未実装の場合 */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[loopCnt1].statFlag, loopCnt2 ) == 0 )
						continue;

					/* 装置情報の表示 */
					func_display_machineinfo( loopCnt1, loopCnt2 );

				}
			}
		}
		else
		{
			/* 見出しの表示 */
			printf("\n");
			printf("\033[0K");	printf("= 接続機器一覧 =================================================================\n");

			if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].machineCnt != 0 )
			{
				printf("\033[0K");	printf("回線 装置 装置     装置 装置                 定格     パネル\n");
				printf("\033[0K");	printf("番号 ID__ 種別____ No__ 型式________________ 容量(W)  容量(W)\n");

				/* パワコンのみ表示対象 */
				/* 装置最大数分繰り返す */
				for( loopCnt2 = 0 ; loopCnt2 < ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].maxMachine ; loopCnt2++ )
				{
					/* 装置未実装の場合 */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].statFlag, loopCnt2 ) == 0 )
						continue;

					memset( typeName, 0, sizeof(typeName) );
					strncpy( typeName, (ps_g_POWCNINF + loopCnt2)->typeName, 20 );
					printf(" %2d  %3d  %s  %2d  %-20s %8d %8d\n",
						(ps_g_POWCNINF + loopCnt2)->lineNumb,
						(ps_g_POWCNINF + loopCnt2)->idNumb,
						s_machineName[PV_MKIND_POWCN].nameShort,
						loopCnt2 + 1,
						typeName,
						(ps_g_POWCNINF + loopCnt2)->ratedCap,
						(ps_g_POWCNINF + loopCnt2)->panelCap);
				}
				printf("\033[0K");
				printf("--------------------------------------------------------------------------------\n");
			}
				

			if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_BATTE].machineCnt != 0 )
			{
				printf("\033[0K");
				printf(" バッテリー情報-----------------------------------------------------------------\n");
				printf("\033[0K");
				printf("装置  バッテリ            バッテリ  最大定格  最大放電  最大充電   SoC閾値  \n");
				printf("\033[0K");
				printf("ID__  用途                容量(Wh)_ 電力(W)__ 電力(W)__ 電力(W)__  上限 下限\n");

				for( soutiNumb = 0; soutiNumb < ps_g_PVSYSINF->s_machineInfo[PV_MKIND_BATTE].maxMachine; soutiNumb ++)
				{
		  			if( (ps_g_BATTEINF + soutiNumb)->statFlag != 0 )
					{
						printf("\033[0K");
						printf("%3d  %24s %9d %9d %9d %9d  %4d %4d\n",
							(ps_g_BATTEINF + soutiNumb)->idNumb,
							(((ps_g_BATTEINF + soutiNumb)->batteType < BT_TYPE_STORAGE_NO_OFFGRID || ( (ps_g_BATTEINF + soutiNumb)->batteType > BT_TYPE_MAX )) ? "範囲外" : c_batteryTypeName[(ps_g_BATTEINF + soutiNumb)->batteType-1] ),
							(ps_g_BATTEINF + soutiNumb)->batteCap,
							(ps_g_BATTEINF + soutiNumb)->maxRatedPwr,
							(ps_g_BATTEINF + soutiNumb)->maxDisChgPwr,
							(ps_g_BATTEINF + soutiNumb)->maxChargePwr,
							(ps_g_BATTEINF + soutiNumb)->socUpper,
							(ps_g_BATTEINF + soutiNumb)->socLower);
					}
				}
				printf("\033[0K");
				printf("--------------------------------------------------------------------------------\n");
				printf("\033[0K");
				printf("\n");
			}
		}

		printf("================================================================================\n");
		printf("\033[0K");
		printf("戻る:[q]  表示切替:[m]");
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が表示切替の場合は表示モードを切り替える */
		if( (inKeyData[0] == 'm') || (inKeyData[0] == 'M') )
		{
			dispMode = (dispMode + 1 ) % 2;
			/* 画面をクリア */
			printf("\033[2J");
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * モニタリングサーバ情報確認表示
 *
 * 機能：
 *		モニタリングサーバの設定情報を表示する
 *
 */
void	func_display_pvsvrinf()
{
	FILE				*fp;			/* ファイルポインタ */
	int					loopCnt1;		/* ループカウンタ */
	int					fileCount;		/* 送信待ちファイル数 */
	char				inKeyData;		/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	char				workCmd[1024];	/* ワークコマンド */
	char				workBuff[1024];	/* ワークバッファ */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct tm			localTime;				/* 現在時刻 */
	time_t				nowTime;				/* 現在時刻 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */

	/* 送信待ちファイル数取得コマンド */
	sprintf( workCmd, "ls -1 %s%s*.DAT 2> /dev/null | wc -l", PV_PATH_FTP, ps_g_PVSYSINF->siteIdName );

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;

	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末サイズを取得 */
	func_termsize_get();
	/* 端末サイズを保存 */
	termSize_OLD.ws_row = termSize.ws_row;

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末サイズを取得 */
		func_termsize_get();
		/* 端末サイズ（行数）が変更された場合 */
		if( termSize_OLD.ws_row != termSize.ws_row )
		{
			/* 画面をクリア */
			printf("\033[2J");
			/* 端末サイズを保存 */
			termSize_OLD.ws_row = termSize.ws_row;
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );
		/* タイトルを表示 */
		printf("= モニタリングサーバ関連情報 ===========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("収集装置ID                   : %s\n", ps_g_PVSYSINF->siteIdName );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------------------------------------------------------------------------------\n");
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("直近の送信結果               : %s （処理対象ファイル数:%d）\n", (commfunc_manage_sftpSndStsGet() < 0)? "異常" : "平常", abs((int)commfunc_manage_sftpSndStsGet()) );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("データ送信ロック状態         : %s\n", (ps_g_PVSYSINF->dtsdLock == 0)? "平常" : "ロック" );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------------------------------------------------------------------------------\n");

		/* モニタリングサーバ情報数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_SVR ; loopCnt1++ )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("モニタリングサーバ:%02d\n", loopCnt1 + 1 );

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			if( strlen( ps_g_PVSVRINF->s_svrinfo[loopCnt1].ipAddr ) == 0 )
			{
				/* メンテナンス専用ユーザの場合はポート番号を非表示 */
				if( mainte_flg == 0 )
				{
					printf("  HOST-Address ( port )      : 0.0.0.0 ( %d )\n",
						ps_g_PVSVRINF->s_svrinfo[loopCnt1].portNumb ); 
				}
				else
				{
					printf("  HOST-Address               : 0.0.0.0\n");
				}
			}
			else
			{
				/* メンテナンス専用ユーザの場合はポート番号を非表示 */
				if( mainte_flg == 0 )
				{
					printf("  HOST-Address ( port )      : %s ( %d )\n",
						ps_g_PVSVRINF->s_svrinfo[loopCnt1].ipAddr,
						ps_g_PVSVRINF->s_svrinfo[loopCnt1].portNumb ); 
				}
				else
				{
					printf("  HOST-Address               : %s\n",
						ps_g_PVSVRINF->s_svrinfo[loopCnt1].ipAddr );
				}
			}

			/* メンテナンスユーザの場合はアクセス要ユーザＩＤは表示しない */
			if( mainte_flg == 0 )
			{
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");

				printf("  Access User Name           : %s\n",
					ps_g_PVSVRINF->s_svrinfo[loopCnt1].loginName );

			}
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("  Status Data Send Path      : %s\n",
				ps_g_PVSVRINF->s_svrinfo[loopCnt1].sendPath );

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("  Process Update Check Path  : %s\n",
				ps_g_PVSVRINF->s_svrinfo[loopCnt1].procPath );

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("  CSV Data Update Check Path : %s\n",
				ps_g_PVSVRINF->s_svrinfo[loopCnt1].dataPath );

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("--------------------------------------------------------------------------------\n");
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("サーバ送信用ファイル保持期間 : %03d 時間 （%d日%d時間）\n",
			ps_g_PVSVRINF->stockHour,
			ps_g_PVSVRINF->stockHour / 24,
			ps_g_PVSVRINF->stockHour % 24 );

		/* 送信待ちファイル数確認 */
		fileCount = 0;
		if( (fp = popen( workCmd, "r" )) != NULL )
		{
			fgets( workBuff, 256, fp );
			fileCount = atoi( workBuff );
			pclose( fp );
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("サーバ送信待ちファイル数     : %03d FILE\n", fileCount );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q]                                                              (5秒更新)");
		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );

		if( readByte != 0 )
		{
			/* read関数がエラーとなった場合処理を抜ける */
			if( readByte < 0 ) break;
			/* 終了のキー入力の場合処理を抜ける */
			if( (inKeyData == 'q' ) || (inKeyData == 'Q') ) break;
		}

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 電力会社向け情報確認表示
 *
 * 機能：
 *		電力会社向けの設定情報を表示する
 *
 */
void	func_display_pgconinf()
{
	FILE				*fp;			/* ファイルポインタ */
	int					loopCnt1;		/* ループカウンタ */
	int					fileCount;		/* 送信待ちファイル数 */
	char				inKeyData;		/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	char				workCmd[1024];	/* ワークコマンド */
	char				workBuff[1024];	/* ワークバッファ */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct tm			localTime;				/* 現在時刻 */
	time_t				nowTime;				/* 現在時刻 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* カーソルの位置からその行の右端までクリア */
	printf("\033[0K");

	/* 現在時刻を取得 */
	nowTime = time( NULL );
	/* 年月日時分秒を取得 */
	localtime_r( &nowTime, &localTime );
	/* タイトルを表示 */
	printf("= 電力会社向け関連情報 =================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
		localTime.tm_year + 1900,
		localTime.tm_mon + 1,
		localTime.tm_mday,
		localTime.tm_hour,
		localTime.tm_min,
		localTime.tm_sec );

	/* カーソルの位置からその行の右端までクリア */
	printf("\033[0K");

	printf("発電所情報\n");
	printf("  電力会社仕様                           : %02d[%s]\n", ps_g_PGCONINF->powerGrid, 
		( ps_g_PGCONINF->powerGrid < PV_SCH_POWERGRID_MIN || ( ps_g_PGCONINF->powerGrid > PV_SCH_POWERGRID_MAX )) ? "範囲外" : c_powerGridName[ps_g_PGCONINF->powerGrid-1] );

	printf("  発電所ＩＤ                             : %s\n", ps_g_PGCONINF->plantId );
	printf("  ＭＡＣアドレス                         : %s\n", ps_g_PGCONINF->macAddr );
	printf("  出力変化時間（出荷時）                 : %d\n", ps_g_PGCONINF->chgTime );
	printf("  スケジュール未登録時の制御値（出荷時） : %d\n", ps_g_PGCONINF->schNonPer );	/* [20210330] Add */
	printf("--------------------------------------------------------------------------------\n");
	printf("スケジュール情報配信サーバ\n");
	printf("  ＵＲＬ                 : %s\n", ps_g_PGCONINF->ipAddr ); 
	printf("  ポート番号             : %d\n", ps_g_PGCONINF->portNumb ); 
	printf("--------------------------------------------------------------------------------\n");
	printf("ＮＴＰサーバ\n");
	printf("  ホストアドレス         : %s\n", ps_g_PGCONINF->ntpIpAddr ); 
	printf("  ポート番号             : %d\n", ps_g_PGCONINF->ntpPortNumb ); 
	printf("================================================================================\n");
	printf("任意のキー押下でメニューに戻ります ");
	fflush(NULL);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );
	while( 1 )
	{
		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte != 0 ) break;
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * IoT設定情報表示
 *
 * 機能：
 *		IoT設定情報の表示を行う
 *
 */
void func_display_iotconfinfo( void )
{
	char				workStr[32];	/* ワーク */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=4;		/* 表示不可行数 */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
//	ttyChange.c_cc[VTIME] = viewTime;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = GET_OUT_LIMIT(termSize.ws_row, noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <IoT設定情報> ========================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		printf("IoT設定情報---------------------------------------------------------------------\n");
		if( ps_g_IOTCNINF->statFlag == 0 )
		{
			printf("\033[0K");	printf("未実装です\n");
		}
		else
		{
			printf("\033[0K");	printf("リソース種別                ：%02d[%s]\n", ps_g_IOTCNINF->resorceType, 
			( ps_g_IOTCNINF->resorceType < AWS_RESORCE_THERMAL_POWER || ( ps_g_IOTCNINF->resorceType > AWS_RESORCE_MAX )) ? "範囲外" : c_resorceTypeName[ps_g_IOTCNINF->resorceType-1] );
			printf("\033[0K");	printf("接続先アドレス              ：%s\n", ps_g_IOTCNINF->hostAddress);
			printf("\033[0K");	printf("ポート番号                  ：%d\n", ps_g_IOTCNINF->portNumb);
			printf("\033[0K");	printf("デバイスＩＤ                ：%s\n", ps_g_IOTCNINF->deviceId);
			printf("\033[0K");	printf("------------------------------------------------------------------------------\n");
			printf("\033[0K");	printf("制御指示トピック名          ：%s\n", ps_g_IOTCNINF->rcvTopicName);
			printf("\033[0K");	printf("応答送信リトライ回数        ：%d\n", ps_g_IOTCNINF->sndRetry);
			printf("\033[0K");	printf("------------------------------------------------------------------------------\n");
			printf("\033[0K");	printf("計測・状態情報送信トピック名：%s\n", ps_g_IOTCNINF->sndTopicName);
			printf("\033[0K");	printf("送信周期(秒)                ：%d\n", ps_g_IOTCNINF->sndCycle);
			printf("\033[0K");	printf("送信データ保持期間          ：%03d 時間 （%d日%d時間）\n", ps_g_IOTCNINF->stockHour, ps_g_IOTCNINF->stockHour / 24, ps_g_IOTCNINF->stockHour % 24 );
			printf("\033[0K");	printf("------------------------------------------------------------------------------\n");
		}
		printf("\033[0K");
		printf("================================================================================\n");
		printf("\033[0K");
		printf("戻る:[q]                                                               (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;
		
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	return;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 計測情報表示
 *
 * 機能：
 *		引数 mKind で指定された装置種別の計測情報を表示する。
 *		指定された装置種別のうち実装する最小番号の装置を最初に表示する。表示対
 *		象の装置はキー入力により切替可能とする。
 *		１装置あたりの計測情報数が１画面に収まらない場合は、キー入力により表示
 *		する計測情報の切替を可能とする。
 *		表示する計測情報は、計測情報（設定値）と現在値、移動平均値とする。
 */
void	 func_display_tm( int mKind )
{
	int						soutiNumb;		/* 装置番号 */
	int						lineNumb = 0;	/* 回線番号 */
	int						soutiId = 0;	/* 装置ID */
	int						dispMode = 0;	/* 表示モード */
	int						offSet;			/* 表示開始位置用オフセット */
	int						maxPos;			/* 最大情報数 */
	int						maxSouti;		/* 最大装置数 */
	int						outLimit;		/* 表示可能行数 */
	int						loopCnt;		/* ループカウンタ */
	long long				calcData;		/* 演算データ */
	long long				pow10Data;		/* べき乗データ */
	long long				spow10Data;		/* べき乗データ */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	int						tmifLink;		/* 計測情報リンクポインタ */
	char					inKeyData;		/* キー入力値 */
	char					readByte;		/* キー入力バイト数 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;	/* 端末サイズ保存用 */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 計測情報の最大数を設定 */
	maxPos = ps_g_PVSYSINF->s_machineInfo[ mKind ].maxTm;
	/* 装置の最大数を設定 */
	maxSouti = ps_g_PVSYSINF->s_machineInfo[ mKind ].maxMachine;

	/* 最大装置数分繰り返す（実装装置の先頭を探す） */
	for( soutiNumb = 0 ; soutiNumb < maxSouti ; soutiNumb++ )
	{
		/* 実装の場合 */
		if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
			break;
	}
	/* 実装装置が無かった場合 */
	if( soutiNumb == maxSouti )
	{
		printf("%sは実装されていません\n", s_machineName[ mKind ].nameFull );
		sleep(5);
		return;
	}

	/* 所属回線番号と装置IDを取得 */
	func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );

	/* オフセットを設定 */
	offSet = 0;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 6 );

		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 表示対象が収集装置の場合 */
		if( mKind == 0 )
		{
			/* 装置種別と現在時刻の表示 */
			printf("= <%s (集約計測値)> ==============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
				s_machineName[ mKind ].nameShort,
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		}
		else
		{
			/* 装置種別、装置番号と現在時刻の表示 */
			printf("= <%s:%02d (回線:%02d,ID:%02d)> ========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
				s_machineName[ mKind ].nameShort,
				soutiNumb + 1,
				lineNumb,soutiId,
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		}

		if( dispMode == 0 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* 見出し１を表示 */
			printf("計測 実装 IoT  蓄積 計測 計測   保存  現在値         計測名称\n");


			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* 見出し２を表示 */
			printf("番号 FLAG 送信 番号 種別 単位__ 位置_ FL VALUE_____  _____________________\n");

			/* 装置種別に合わせて計測情報のリンクポインタ値を取得する */
			tmifLink = ps_g_PVSYSINF->s_machineInfo[ mKind ].tmIfLink + ps_g_PVSYSINF->s_machineInfo[ mKind ].maxTm * soutiNumb + offSet;

			/* 計測情報を画面に表示可能な行数分表示する */
			for( loopCnt = offSet ; ( loopCnt < maxPos ) && ( (loopCnt - offSet) < outLimit ) ; loopCnt++ )
			{
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");
	
				/* 計測番号（装置内）、実装/未実装、計測種別（瞬時/積算）を表示する */
				printf("%04d %4d %4d %4d %s ",
					loopCnt + 1,
					(ps_g_TMDATINF + tmifLink)->statFlag, 
					(ps_g_TMDATINF + tmifLink)->iotSndFlag, 
					(ps_g_TMDATINF + tmifLink)->storeNumb, 
					c_tmKindName[ (ps_g_TMDATINF + tmifLink)->tmKind ] );
				/* 計測単位を表示する */
				func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );
				/* 最新の取得値（計測フラグ、計測値）と移動平均値（計測フラグ、計測値）、計測情報名称を表示する */
				printf(" %05d ", tmifLink);
				if( (ps_g_TMDATINF + tmifLink)->statFlag == 0 )
				{
					printf("-- ----------  %s\n",(ps_g_TMDATINF + tmifLink)->tmName);
				}
				else
				{
					printf("%02X %10d  %s\n",
						(ps_g_TMSTSFIL + tmifLink)->avltmFlag,
						(ps_g_TMSTSFIL + tmifLink)->avltmVal,
						(ps_g_TMDATINF + tmifLink)->tmName );
				}
				/* 表示対象のリンクポインタを＋１ */
				tmifLink++;
			}
		}
		else
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* 見出し１を表示 */
			printf("TM  現在値     変換値          ベース値         乗  演算結果       単位   計測名称\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* 見出し２を表示 */
			printf("No_ __________ _______________ ________________ 率_ ______________ ______ ______________\n");

			/* 装置種別に合わせて計測情報のリンクポインタ値を取得する */
			tmifLink = ps_g_PVSYSINF->s_machineInfo[ mKind ].tmIfLink + ps_g_PVSYSINF->s_machineInfo[ mKind ].maxTm * soutiNumb + offSet;

			/* 計測情報を画面に表示可能な行数分表示する */
			for( loopCnt = offSet ; ( loopCnt < maxPos ) && ( (loopCnt - offSet) < outLimit ) ; loopCnt++ )
			{
				pow10Data = (long long)commfunc_pow10abs( (ps_g_TMDATINF + tmifLink)->expData );
				spow10Data = (long long)commfunc_pow10abs( (ps_g_TMDATINF + tmifLink)->expData + 3);
				calcData = (long long)((ps_g_TMSTSFIL + tmifLink)->tmVal);
				calcData *= (long long)((ps_g_TMDATINF + tmifLink)->convData);
				calcData += (long long)((ps_g_TMDATINF + tmifLink)->baseData);

				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");
	
				/* 計測番号（装置内）、変換値、ベース値、乗率、演算結果を表示する */
				printf("%03d %10d %15lld %16lld %3d ",
					loopCnt + 1,
					/* (ps_g_TMDATINF + tmifLink)->tmNumb, */
					(ps_g_TMSTSFIL + tmifLink)->tmVal,
					(long long)((ps_g_TMDATINF + tmifLink)->convData),
					(long long)((ps_g_TMDATINF + tmifLink)->baseData),
					(ps_g_TMDATINF + tmifLink)->expData );
					
				/* 演算結果を表示する */
				if( (ps_g_TMDATINF + tmifLink)->expData >= 0 )
				{
					printf("%10lld.000 ", calcData * pow10Data );
				}
				else
				{
					if( ((calcData / pow10Data) == 0) && ( calcData < 0 ) )
						printf("        -0." );
					else
						printf("%10lld.", calcData / pow10Data );
					if( calcData < 0 ) calcData *= -1;
					if( ((ps_g_TMDATINF + tmifLink)->expData + 3) >= 0 )
					{
						printf("%03lld ", ( calcData * spow10Data ) % 1000 );
					}
					else
					{
						printf("%03lld ", ( calcData / spow10Data ) % 1000 );
					}
				}
				/* 計測単位を表示する */
				func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );
				/* 最新の取得値（計測フラグ、計測値）と移動平均値（計測フラグ、計測値）、計測情報名称を表示する */
				printf(" %s\n", (ps_g_TMDATINF + tmifLink)->tmName );
				/* 表示対象のリンクポインタを＋１ */
				tmifLink++;
			}
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
	
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q]  表示切替:[m]  頁切替:[前:c / 次:v]  装置切替:[前:b / 次:n]  (5秒更新)");
		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 読み込みエラーの場合は処理を抜ける */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は処理を抜ける */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が表示切替の場合は表示モードを切り替える */
			if( (inKeyData == 'm') || (inKeyData == 'M') ) { dispMode = (dispMode + 1 ) % 2; }

			/* キー入力が装置番号の切替の場合は次の実装装置を検索する */
			if( (inKeyData == 'n') || (inKeyData == 'N' ) )
			{
				while( 1 )
				{
					/* 装置番号を＋１ */
					soutiNumb++;
					/* 装置番号が最大数を超過した場合は装置番号を0 に設定 */
					if( soutiNumb >= maxSouti ) soutiNumb = 0;

					/* 装置が実装の場合はループを抜ける */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
					{
						/* 所属回線番号と装置IDを取得 */
						func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );
						break;
					}
				}
			}
			if( (inKeyData == 'b') || (inKeyData == 'B' ) )
			{
				while( 1 )
				{
					/* 装置番号を－１ */
					soutiNumb--;
					/* 装置番号がマイナスの場合は装置最大数－１の値を設定 */
					if( soutiNumb < 0 ) soutiNumb = maxSouti - 1;

					/* 装置が実装の場合はループを抜ける */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
					{
						/* 所属回線番号と装置IDを取得 */
						func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );
						break;
					}
				}
			}
			/* キー入力が計測項目の画面切替の場合 */
			if( (inKeyData == 'v') || (inKeyData == 'V' ) )
			{
				if( (offSet + outLimit) < maxPos )
				{
					offSet += outLimit;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'c') || (inKeyData == 'C' ) )
			{
				offSet -= outLimit;
				if( offSet < 0 )
				{
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* キー入力が計測項目の１項目ずつの切替の場合 */
			if( inKeyData == '+' )
			{
				if( (offSet + 1) < maxPos )
				{
					offSet += 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( inKeyData == '-' )
			{
				if( (offSet - 1) >= 0 )
				{
					offSet -= 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 単位表示
 *
 * 機能：
 *		引数 uKind で指定された計測単位を表示する。
 */
void	 func_display_unit( short uKind )
{
	/* 計測単位を表示する */
	switch(  uKind )
	{
		case PV_TM_UKIND_WH:
			printf("Wh    ");
			break;
		case PV_TM_UKIND_KWH:
			printf("kWh   ");
			break;
		case PV_TM_UKIND_VARH:
			printf("varh  ");
			break;
		case PV_TM_UKIND_KVARH:
			printf("kvarh ");
			break;
		case PV_TM_UKIND_W:
			printf("W     ");
			break;
		case PV_TM_UKIND_KW:
			printf("kW    ");
			break;
		case PV_TM_UKIND_A:
			printf("A     ");
			break;
		case PV_TM_UKIND_KA:
			printf("kA    ");
			break;
		case PV_TM_UKIND_V:
			printf("V     ");
			break;
		case PV_TM_UKIND_KV:
			printf("kV    ");
			break;
		case PV_TM_UKIND_HZ:
			printf("Hz    ");
			break;
		case PV_TM_UKIND_C:
			printf("℃    ");
			break;
		case PV_TM_UKIND_KW_M2:
			printf("kW/m2 ");
			break;
		case PV_TM_UKIND_S:
			printf("秒    ");
			break;
		case PV_TM_UKIND_KAI:
			printf("回    ");
			break;
		case PV_TM_UKIND_KWH_KAI:
			printf("kWh/回");
			break;
		case PV_TM_UKIND_PERCENT:
			printf("%c     ", '%');
			break;
		case PV_TM_UKIND_MA:
			printf("mA    ");
			break;
		case PV_TM_UKIND_VRMS:
			printf("Vrms  ");
			break;
		case PV_TM_UKIND_ARMS:
			printf("Arms  ");
			break;
		case PV_TM_UKIND_KVAR:
			printf("kvar  ");
			break;
		case PV_TM_UKIND_KVA:
			printf("kVA   ");
			break;
		case PV_TM_UKIND_HOUR:
			printf("時    ");
			break;
		case PV_TM_UKIND_MIN:
			printf("分    ");
			break;
		case PV_TM_UKIND_WM2:
			printf("W/m2  ");
			break;
		case PV_TM_UKIND_MWH:
			printf("MWh   ");
			break;
		case PV_TM_UKIND_MW:
			printf("MW    ");
			break;
		case PV_TM_UKIND_MAH:
			printf("mAh   ");
			break;
		case PV_TM_UKIND_AH:
			printf("Ah    ");
			break;
		case PV_TM_UKIND_KHZ:
			printf("kHZ   ");
			break;
		case PV_TM_UKIND_MIN_1:
			printf("min-1 ");
			break;
		case PV_TM_UKIND_M3_S:
			printf("m3/s  ");
			break;
		case PV_TM_UKIND_CM:
			printf("cm    ");
			break;
		case PV_TM_UKIND_M:
			printf("m     ");
			break;
		case PV_TM_UKIND_MPA:
			printf("MPa   ");
			break;
		case PV_TM_UKIND_MVAR:	/* */
			printf("Mvar  ");
			break;
		case PV_TM_UKIND_MM:	/* */
			printf("mm    ");
			break;
		case PV_TM_UKIND_VAR:	/* */
			printf("var   ");
			break;
		case PV_TM_UKIND_VA:	/* */
			printf("VA    ");
			break;
		case PV_TM_UKIND_OHM:	/* */
			printf("Ω    ");
			break;
		case PV_TM_UKIND_NUM:	/* */
			printf("台    ");
			break;
		case PV_TM_UKIND_WS:	/* */
			printf("W/s   ");
			break;
		case PV_TM_UKIND_MV:	/* */
			printf("mV    ");
			break;
		default:
			printf("------"); 
			break;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 装置状態情報表示
 *
 * 機能：
 *		引数 mKind で指定された装置種別の装置状態情報を表示する。
 *		指定された装置種別のうち実装する最小番号の装置を最初に表示する。表示対
 *		象の装置はキー入力により切替可能とする。
 *		１装置あたりの装置状態情報数が１画面に収まらない場合は、キー入力により表
 *		示する装置状態情報の切替を可能とする。
 *		表示する装置状態情報は、装置状態情報（設定値）と現在状態とする。
 */
void	 func_display_sv( int mKind )
{
	int						soutiNumb;			/* 装置番号 */
	int						lineNumb = 0;		/* 回線番号 */
	int						soutiId = 0;		/* 装置ID */
	int						offSet;				/* オフセット値 */
	int						maxPos;				/* 最大装置情報数 */
	int						maxSouti;			/* 最大装置数 */
	int						outLimit;			/* 表示可能行数 */
	int						loopCnt;			/* ループカウンタ */
	struct tm				localTime;			/* 現在時刻 */
	time_t					nowTime;			/* 現在時刻 */
	int						svifLink;			/* 装置状態情報リンク情報 */
	int						svstLink;			/* 装置状態リンク情報 */
	int						svstBase;			/* 装置状態リンク情報（ベース値） */
	int						svNumb;				/* 状態番号 */
	char					inKeyData;			/* キー入力値 */
	char					readByte;			/* キー入力バイト数 */
	unsigned char			svstMask;			/* 機器状態取得用マスク */
	struct termio			ttyBackup;			/* 端末設定保存用 */
	struct termio			ttyChange;			/* 端末設定変更用 */
	struct winsize			termSize_OLD;		/* 端末サイズ保存用 */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 状態情報の最大数を設定 */
	maxPos = ps_g_PVSYSINF->s_machineInfo[ mKind ].maxSv;
	/* 装置の最大数を設定 */
	maxSouti = ps_g_PVSYSINF->s_machineInfo[ mKind ].maxMachine;

	/* 最大装置数分繰り返す（実装装置の先頭を探す） */
	for( soutiNumb = 0 ; soutiNumb < maxSouti ; soutiNumb++ )
	{
		/* 実装の場合 */
		if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
			break;
	}
	/* 実装装置が無かった場合 */
	if( soutiNumb == maxSouti )
	{
		printf("%sは実装されていません\n", s_machineName[ mKind ].nameFull );
		sleep(5);
		return;
	}

	/* 所属回線番号と装置IDを取得 */
	func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );

	/* オフセットを設定 */
	offSet = 0;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 6 );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別を表示 */
		if( mKind == PV_MKIND_PVSYS )
		{
			/* 装置と現在時刻を表示 */
			printf("= <%s> ===========================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
				s_machineName[ mKind ].nameShort,
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		}
		else
		{
			/* 装置番号と現在時刻を表示 */
			printf("= <%s:%02d (回線:%02d,ID:%02d)> ========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
				s_machineName[ mKind ].nameShort,
				soutiNumb + 1,
				lineNumb,soutiId,
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出し１を表示 */
		printf("状態 実装 IoT  蓄積 状変 情報  状態  現在    状態名称\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出し２を表示 */
		printf("番号 FLAG 送信 番号 MASK LINK_ LINK_ 状態___ _____________________________________________\n");

		/* 装置種別に合わせて情報のリンクポインタ値を取得する */
		svifLink = ps_g_PVSYSINF->s_machineInfo[ mKind ].svIfLink + ps_g_PVSYSINF->s_machineInfo[ mKind ].maxSv * soutiNumb + offSet;
		/* 装置種別に合わせて状態のリンクポインタ値を取得する */
		svstBase = 	ps_g_PVSYSINF->s_machineInfo[ mKind ].svStLink +
					((ps_g_PVSYSINF->s_machineInfo[ mKind ].maxSv - 1) / 8 + 1) * soutiNumb;

		/* １画面に表示可能な数だけ表示を行う */
		for( loopCnt = offSet ; ( loopCnt < maxPos ) && ( (loopCnt - offSet) < outLimit ) ; loopCnt++ )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* 状態番号（装置内）、実装/未実装、状態種別（マスク種別）、情報リンク、状態リンクの表示 */
			printf("%04d %4d %4d %4d %4d %05d %05d ",
				loopCnt + 1,
				(ps_g_SVDATINF + svifLink)->statFlag, 
				(ps_g_SVDATINF + svifLink)->iotSndFlag,
				(ps_g_SVDATINF + svifLink)->storeNumb,
				(ps_g_SVDATINF + svifLink)->svKind,
				svifLink,
				svstBase + loopCnt / 8 );
			/*	( (ps_g_SVDATINF + svifLink)->svNumb != 0)? svstBase + ((ps_g_SVDATINF + svifLink)->svNumb - 1) / 8 : 0 );	*/

			/* 装置状態取得のためのマスク値を設定(未実装の場合は、状態番号から算出する */
			if( (ps_g_SVDATINF + svifLink)->statFlag != 0 )
			{
				svstMask = (unsigned char)(0x80 >> (((ps_g_SVDATINF + svifLink)->svNumb - 1) % 8));
				svNumb = (ps_g_SVDATINF + svifLink)->svNumb;
			}
			else
			{
				svstMask = (unsigned char)(0x80 >> (loopCnt % 8));
				svNumb = loopCnt + 1;
			}
				
			/* 装置状態の取得と表示 */
			if( (ps_g_SVSTSFIL + svstBase + (svNumb - 1) / 8 )->svStat & svstMask )
				printf("[[ 1 ]] ");
			else
				printf("[[ 0 ]] ");
			/* 状態情報名称の表示 */
			printf("%s\n", (ps_g_SVDATINF + svifLink)->svName );

			/* 表示対象のリンクを＋１ */
			svifLink++;
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
	
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q]  頁切替:[前:c / 次:v]  装置切替:[前:b / 次:n]                (5秒更新)");
		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 入力エラーの場合は終了する */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は終了する */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が装置番号の切替の場合は次の実装装置番号を検索する */
			if( ((inKeyData == 'n') || (inKeyData == 'N' )) && (mKind != PV_MKIND_PVSYS) )
			{
				while( 1 )
				{
					/* 装置番号＋１ */
					soutiNumb++;
					/* 装置番号が装置最大数を超過した場合は 0 に設定 */
					if( soutiNumb >= maxSouti ) soutiNumb = 0;

					/* 装置が実装の場合はループを抜ける */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
					{
						/* 所属回線番号と装置IDを取得 */
						func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );
						break;
					}
				}
			}
			if( ((inKeyData == 'b') || (inKeyData == 'B' ) ) && ( mKind != PV_MKIND_PVSYS ) )
			{
				while( 1 )
				{
					/* 装置番号を－１ */
					soutiNumb--;
					/* 装置番号がマイナスの場合は装置最大数－１に設定 */
					if( soutiNumb < 0 ) soutiNumb = maxSouti - 1;

					/* 装置が実装の場合はループを抜ける */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
					{
						/* 所属回線番号と装置IDを取得 */
						func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );
						break;
					}
				}
			}
			/* キー入力が装置状態情報の１画面分の表示切り替えの場合 */
			if( (inKeyData == 'v') || (inKeyData == 'V' ) )
			{
				if( (offSet + outLimit) < maxPos )
				{
					offSet += outLimit;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'c') || (inKeyData == 'C' ) )
			{
				offSet -= outLimit;
				if( offSet < 0 )
				{
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* キー入力が装置状態情報の１行分の表示切り替えの場合 */
			if( inKeyData == '+' )
			{
				if( (offSet + 1) < maxPos )
				{
					offSet += 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( inKeyData == '-' )
			{
				if( (offSet - 1) >= 0 )
				{
					offSet -= 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 通信ステータス状態表示
 *
 * 機能：
 *		引数 mKind で指定された装置種別の通信ステータス状態情報を表示する。
 *		指定された装置種別のうち実装する最小番号の装置を最初に表示する。表示対
 *		象の装置はキー入力により切替可能とする。
 *		１装置あたりの通信ステータス状態情報数が１画面に収まらない場合は、キー入
 *		力により表示切り替えを行う
 */
void	 func_display_st( int mKind )
{
	int						soutiNumb;			/* 装置番号 */
	int						lineNumb = 0;		/* 回線番号 */
	int						soutiId = 0;		/* 装置ID */
	int						offSet;				/* 表示オフセット */
	int						maxSouti;			/* 最大装置数 */
	int						ststBase;			/* 通信ステータス状態リンク開始位置 */
	int						outLimit;			/* 表示可能行数 */
	int						loopCnt;			/* ループカウンタ */
	struct tm				localTime;			/* 現在時刻 */
	time_t					nowTime;			/* 現在時刻 */
	int						stifLink;			/* 通信ステータスリンク */
	char					inKeyData;			/* キー入力値 */
	char					readByte;			/* キー入力バイト数 */
	unsigned char			svstMask;			/* 状態マスク値 */
	struct termio			ttyBackup;			/* 端末設定保存用 */
	struct termio			ttyChange;			/* 端末設定変更用 */
	struct winsize			termSize_OLD;		/* 端末サイズ保存用 */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 装置の最大数を設定 */
	maxSouti = ps_g_PVSYSINF->s_machineInfo[ mKind ].maxMachine;

	/* 最大装置数分繰り返す（実装装置の先頭を探す） */
	for( soutiNumb = 0 ; soutiNumb < maxSouti ; soutiNumb++ )
	{
		/* 実装の場合 */
		if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
			break;
	}
	/* 実装装置が無かった場合 */
	if( soutiNumb == maxSouti )
	{
		printf("%sは実装されていません\n", s_machineName[ mKind ].nameFull );
		sleep(5);
		return;
	}

	/* 所属回線番号と装置IDを取得 */
	func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );

	/* オフセットを設定 */
	offSet = 0;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 6 );

		/* 端末サイズ（高さ）が変更された場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置番号と現在時刻を表示 */
		printf("= <%s:%02d (回線:%02d,ID:%02d)> ========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			s_machineName[ mKind ].nameShort,
			soutiNumb + 1,
			lineNumb,soutiId,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出し１を表示 */
		printf("状態 状態  現在    状態名称\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出し２を表示 */
		printf("番号 LINK_ 状態___ ______________________________________________________________\n");

		/* 表示開始リンクを設定 */
		stifLink = offSet;

		ststBase = ps_g_PVSYSINF->s_machineInfo[ mKind ].stStLink + soutiNumb * ((PV_MAX_STPOS - 1) / 8 + 1);

		/* １画面に表示可能な情報数分繰り返す */
		for( loopCnt = offSet ; ( loopCnt < PV_MAX_STPOS ) && ( (loopCnt - offSet) < outLimit ) ; loopCnt++ )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			/* 通信ステータス番号、状態リンクの表示 */
			printf("%03d  %05d ", stifLink + 1, ststBase + stifLink / 8);

			/* 通信ステータス状態取得用マスク値の設定 */
			svstMask = (unsigned char)(0x80 >> (stifLink % 8));

			/* 通信ステータス状態の取得と表示 */
			if( (ps_g_STSTSFIL + ststBase + stifLink / 8 )->stStat & svstMask )
				printf("[[ 1 ]] ");
			else
				printf("[[ 0 ]] ");

			/* 通信ステータス名称の表示 */
			printf("%s\n", (ps_g_STDATINF + stifLink)->stName );

			stifLink++;
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
	
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q]  頁切替:[前:c / 次:v]  装置切替:[前:b / 次:n]                (5秒更新)");
		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* キー入力がエラーの場合は終了する */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は終了する */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が装置番号の切替の場合は、次の実装装置を検索する */
			if( ((inKeyData == 'n') || (inKeyData == 'N' )) && (mKind != PV_MKIND_PVSYS) )
			{
				while( 1 )
				{
					/* 装置番号＋１ */
					soutiNumb++;
					/* 装置番号が装置最大数を超過した場合は０とする */
					if( soutiNumb >= maxSouti ) soutiNumb = 0;

					/* 装置が実装の場合はループを抜ける */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
					{
						/* 所属回線番号と装置IDを取得 */
						func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );
						break;
					}
				}
			}
			if( ((inKeyData == 'b') || (inKeyData == 'B' ) ) && ( mKind != PV_MKIND_PVSYS ) )
			{
				while( 1 )
				{
					/* 装置番号－１ */
					soutiNumb--;
					/* 装置番号がマイナスの場合は装置最大数－１に設定 */
					if( soutiNumb < 0 ) soutiNumb = maxSouti - 1;

					/* 装置が実装の場合はループを抜ける */
					if( commfunc_bitget( ps_g_PVSYSINF->s_machineInfo[ mKind ].statFlag, soutiNumb ) != 0 )
					{
						/* 所属回線番号と装置IDを取得 */
						func_line_id_get( mKind, soutiNumb, &lineNumb, &soutiId );
						break;
					}
				}
			}
			/* キー入力が１画面分の切替の場合 */
			if( (inKeyData == 'v') || (inKeyData == 'V' ) )
			{
				if( (offSet + outLimit) < PV_MAX_STPOS )
				{
					offSet += outLimit;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'c') || (inKeyData == 'C' ) )
			{
				offSet -= outLimit;
				if( offSet < 0 )
				{
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* キー入力が１行分の切替の場合 */
			if( inKeyData == '+' )
			{
				if( (offSet + 1) < PV_MAX_STPOS )
				{
					offSet += 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( inKeyData == '-' )
			{
				if( (offSet - 1) >= 0 )
				{
					offSet -= 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 集約計測情報表示
 *
 * 機能：
 *		集約計測情報を表示する。
 *      集約計測情報を１項目ずつ表示する。キー入力により表示項目および期間演算情
 *      報と表示内容を切り替える。
 *		初期状態は集約演算情報のうち実装する最小番号の情報を最初に表示する。
 */
void	 func_display_aggtminfo( )
{
	int						dispMode = 1;	/* 表示モード */
	int						calcTmSumNumb=0;/* 演算計測番号 */
	int						calcTmifLink;	/* 計測情報番号 */
	int						calcTmDifNumb;	/* 演算計測番号 */
	int						loopCnt1;		/* ループカウンタ */
	int						loopCnt2;		/* ループカウンタ */
	int						outLimit;		/* 表示可能行数 */
	long long				calcData;		/* 演算データ */
	long long				pow10Data;		/* べき乗データ */
	long long				spow10Data;		/* べき乗データ */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	int						tmifLink;		/* 計測情報リンクポインタ */
	char					inKeyData;		/* キー入力値 */
	char					readByte;		/* キー入力バイト数 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;	/* 端末サイズ保存用 */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 6 );

		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <%s (集約計測情報)> ============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			s_machineName[ 0 ].nameShort,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("集約計測INDEX  : [%03d] (001-%03d)\n", calcTmSumNumb + 1, PV_MAX_PVSYS_CTLTMPOS );

		calcTmifLink = ps_g_TMAGGINF[calcTmSumNumb].tmIfLink;
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------------------------------------------------------------------------------\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("演算計測番号   : [%03d]          計測種別       : [%s]\n",
			ps_g_TMAGGINF[calcTmSumNumb].tmNumb,
			c_tmKindName[ ps_g_TMDATINF[calcTmifLink].tmKind ] );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("演算計測種別   : [合計]演算     計測単位,乗率  : [");
		func_display_unit( ps_g_TMDATINF[calcTmifLink].unitKind );
		printf(",%3d]\n", ps_g_TMDATINF[calcTmifLink].expData );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("保存場所(LINK) : [%04d]         集約計測名称   : [%s]\n",
			ps_g_TMAGGINF[calcTmSumNumb].tmIfLink,
			ps_g_TMAGGINF[calcTmSumNumb].tmName );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("集約対象計測数 : [%02d]\n",
			ps_g_TMAGGINF[calcTmSumNumb].aggTmNum );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("集約演算結果   : [FL:%02X VL:%10d]\n",
			ps_g_TMSTSFIL[ps_g_TMAGGINF[calcTmSumNumb].tmIfLink].avltmFlag,
			ps_g_TMSTSFIL[ps_g_TMAGGINF[calcTmSumNumb].tmIfLink].avltmVal );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------------------------------------------------------------------------------\n");
		
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("No 集約演算対象__ LINK_ 単位,乗率___ 直値_________________ 計測名称_____________\n");
		
		for( loopCnt1 = 0 ; loopCnt1 < ps_g_TMAGGINF[calcTmSumNumb].aggTmNum ; loopCnt1++ )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("%02d [%s,%02d,%03d,%03d] %05d [",
				loopCnt1 + 1,
				(ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].plusminus == PV_DEF_PLUS)? "+" : "-",
				ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].soKind,
				ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].soNumb,
				ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmNumb,
				ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmIfLink );
			func_display_unit( ps_g_TMDATINF[ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmIfLink].unitKind );
			printf(",%3d] [FL:%02X VL:%10d] %s\n",
				ps_g_TMDATINF[ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmIfLink].expData,
				ps_g_TMSTSFIL[ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmIfLink].avltmFlag,
				ps_g_TMSTSFIL[ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmIfLink].avltmVal,
				ps_g_TMDATINF[ps_g_TMAGGINF[calcTmSumNumb].s_tmaggdata[loopCnt1].tmIfLink].tmName );
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
	
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q]              計測項目:[前:b / 次:n]                          (5秒更新)");
		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 読み込みエラーの場合は処理を抜ける */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は処理を抜ける */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が計測項目の画面切替の場合 */
			if( (inKeyData == 'n') || (inKeyData == 'N' ) )
			{
				if( calcTmSumNumb >= 0 )
				{
					while( 1 )
					{
						calcTmSumNumb++;
						if( calcTmSumNumb >= PV_MAX_PVSYS_CTLTMPOS )
							calcTmSumNumb = 0;
						if( ps_g_TMAGGINF[calcTmSumNumb].statFlag != 0 )
							break;
					}
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'b') || (inKeyData == 'B' ) )
			{
				if( calcTmSumNumb >= 0 )
				{
					while( 1 )
					{
						if( calcTmSumNumb == 0 )
							calcTmSumNumb = PV_MAX_PVSYS_CTLTMPOS - 1;
						else
							calcTmSumNumb--;
						if( ps_g_TMAGGINF[calcTmSumNumb].statFlag != 0 )
							break;
					}
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 集約状態情報表示
 *
 * 機能：
 *		集約状態情報を表示する。
 *      集約状態情報を１項目ずつ表示する。キー入力により表示項目および期間演算情
 *      報と表示内容を切り替える。
 *		初期状態は集約演算情報のうち実装する最小番号の情報を最初に表示する。
 */
void	 func_display_aggsvinfo( )
{
	int						dispMode = 0;	/* 表示モード */
	int						infoNumb=0;		/* 集約情報番号 */
	int						svifLink;		/* 状態情報リンク */
	int						svstLink;		/* 装置状態リンク情報 */
	int						svstBase;		/* 装置状態リンク情報（ベース値） */
	int						aggSvNum;			/* 集約情報数 */
	int						aggSvifLink;	/* 状態情報リンク */
	int						calcTmDifNumb;	/* 演算計測番号 */
	int						loopCnt1;		/* ループカウンタ */
	int						outLimit;		/* 表示可能行数 */
	long long				calcData;		/* 演算データ */
	long long				pow10Data;		/* べき乗データ */
	long long				spow10Data;		/* べき乗データ */
	struct tm				localTime;		/* 現在時刻 */
	unsigned char			svstMask;			/* 状態マスク値 */
	time_t					nowTime;		/* 現在時刻 */
	int						tmifLink;		/* 計測情報リンクポインタ */
	char					inKeyData;		/* キー入力値 */
	char					readByte;		/* キー入力バイト数 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;	/* 端末サイズ保存用 */
	struct st_svaggdata		*ps_svaggdata;	/* 集約対象 */
	int						offSet = 0;		/* 表示オフセット */
	int						svstVal;		/* 現在値 */
	char					svStat;			/* 現在値(状態リンク)バイト  */
	struct st_svaggstat		*ps_svaggstat;	/* 集約SV保存ポインタ */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 初期位置検索 */
	for( infoNumb = 0; infoNumb < PV_MAX_PVSYS_AGGSVPOS;  infoNumb++ )
	{
		/* 実装済みの集約SVがある場合 */
		if( ps_g_SVAGGINF[infoNumb].statFlag != 0 )
		{
			break;
		}
	};

	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 11 );

		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <%s (集約状態情報)> ============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			s_machineName[ 0 ].nameShort,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		if( infoNumb == PV_MAX_PVSYS_AGGSVPOS )
		{
			printf("\033[0K");
			printf("未設定\n");
		}
		else
		{
			aggSvNum = ps_g_SVAGGINF[infoNumb].aggSvNum;

			printf("集約状態INDEX  : [%03d] (001-%03d)\n", infoNumb + 1, PV_MAX_PVSYS_AGGSVPOS );

			svifLink = ps_g_SVAGGINF[infoNumb].svIfLink;
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("--------------------------------------------------------------------------------\n");

			/* 装置種別に合わせて状態のリンクポインタ値を取得する */
			svstBase = 	ps_g_PVSYSINF->s_machineInfo[ (ps_g_SVDATINF + svifLink)->soKind ].svStLink +
						((ps_g_PVSYSINF->s_machineInfo[ (ps_g_SVDATINF + svifLink)->soKind ].maxSv - 1) / 8 + 1) * ((ps_g_SVDATINF + svifLink)->soNumb-1);
			printf("\033[0K");
			printf("装置種別       : [%d:%s]    装置番号       : [%02d]\n", (ps_g_SVDATINF + svifLink)->soKind, s_machineName[(ps_g_SVDATINF + svifLink)->soKind].nameShort, (ps_g_SVDATINF + svifLink)->soNumb);
			printf("状態番号       : [%03d]           集約状態名称   : [%s]\n", (ps_g_SVDATINF + svifLink)->svNumb, (ps_g_SVDATINF + svifLink)->svName );
			printf("状態LINK       : [%05d]         集約演算子     : [%s]\n", (svstBase + ((ps_g_SVDATINF + svifLink)->svNumb - 1) / 8 ), ( ps_g_SVAGGINF[infoNumb].logicalOpe == PV_DEF_AND ? "AND" : "OR") );
			printf("集約対象状態数 : [%2d]            集約状態       : ", ps_g_SVAGGINF[infoNumb].aggSvNum);



			/* 装置状態取得のためのマスク値を設定 */
			svstMask = (unsigned char)(0x80 >> (((ps_g_SVDATINF + svifLink)->svNumb - 1) % 8));
			/* 装置状態の取得と表示 */
			if( (ps_g_SVSTSFIL + svstBase + ((ps_g_SVDATINF + svifLink)->svNumb - 1) / 8 )->svStat & svstMask )
				printf("[[ 1 ]] ");
			else
				printf("[[ 0 ]] ");
			printf("\n");

			printf("\033[0K");
			printf("--------------------------------------------------------------------------------\n");
			if( dispMode == 0 )
			{
				printf("\033[0K");
				printf("No  集約対象__ SV/ST 判定 On/Off(sec)  状態名称________________________   状態\n");
			}
			else
			{
				printf("\033[0K");
				printf("No  集約対象__ SV/ST 判定   検出時間_____________  確定時間_____________  状態\n");
			}

			/* 画面に表示可能な行数分表示する */
			for( loopCnt1 = offSet ; ( loopCnt1 < aggSvNum ) && ( (loopCnt1 - offSet) < outLimit ) ; loopCnt1++ )
			{
				ps_svaggdata = &ps_g_SVAGGINF[infoNumb].s_svaggdata[loopCnt1];
				ps_svaggstat = &ps_g_SVAGGFIL[infoNumb].s_svaggstat[loopCnt1];

				
				/* SV状態情報 */
				if( ps_svaggdata->svstKind == PV_DEF_SV )
				{
					func_display_cmn_strlenBlank( 34, (ps_g_SVDATINF + ps_svaggdata->svIfLink )->svName );

					/* 装置状態の取得と表示 */
					svStat = (ps_g_SVSTSFIL + ps_svaggdata->stPoint )->svStat;

				}
				/* ST状態情報 */
				else //if( ps_svaggdata->svstKind == PV_DEF_ST )
				{
					func_display_cmn_strlenBlank( 34, (ps_g_STDATINF + ps_svaggdata->svstNumb -1 )->stName );

					/* 通信ステータス状態の取得と表示 */
					svStat = (ps_g_STSTSFIL + ps_svaggdata->stPoint )->stStat;
				}
				printf("\033[0K");
				printf("%02d [%02d,%03d,%03d] [%s] [ %d ]", 
						loopCnt1+1, ps_svaggdata->soKind, ps_svaggdata->soNumb, ps_svaggdata->svstNumb, (ps_svaggdata->svstKind == 0 ? "SV" : "ST"),
						ps_svaggdata->svVal);
				if( dispMode == 0 )
				{
					printf("[%4d/%4d]  %s", ps_svaggdata->onCheckSec, ps_svaggdata->offCheckSec, c_strData);
				}
				else
				{
					printf(" ");
					if( ps_svaggstat->detectionTim.tv_sec == 0 )
					{
						printf("--/-- --:--:--.------  ");
											
					}
					else
					{
						localtime_r( &ps_svaggstat->detectionTim.tv_sec, &localTime );
						printf("%02d/%02d %02d:%02d:%02d.%06ld  ",
							localTime.tm_mon + 1,
							localTime.tm_mday,
							localTime.tm_hour,
							localTime.tm_min,
							localTime.tm_sec,
							(int)(ps_svaggstat->detectionTim.tv_usec));
					}
	
					if( ps_svaggstat->fixTim.tv_sec == 0 )
					{
						printf("--/-- --:--:--.------ ");
											
					}
					else
					{
						localtime_r( &ps_svaggstat->fixTim.tv_sec, &localTime );
						printf("%02d/%02d %02d:%02d:%02d.%06ld ",
							localTime.tm_mon + 1,
							localTime.tm_mday,
							localTime.tm_hour,
							localTime.tm_min,
							localTime.tm_sec,
							(int)(ps_svaggstat->fixTim.tv_usec));
					}
				}

				if( ps_svaggstat->svStat == 1 )
					printf("[[ 1 ]] ");
				else
					printf("[[ 0 ]] ");
				printf("\n");
			}
		}
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]  表示切替:[d]   集約項目:[前:b/次:n]   集約情報:[前:c/次:v]  (%d秒更新)", viewTime/10);

		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 読み込みエラーの場合は処理を抜ける */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は処理を抜ける */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が表示切替の場合は表示モードを切り替える */
			if( (inKeyData == 'd') || (inKeyData == 'D') )
			{
				dispMode = (dispMode + 1 ) % 2;
				/* 画面をクリア */
				printf("\033[2J");
			}

			/* キー入力が計測項目の画面切替の場合 */
			if( (inKeyData == 'v') || (inKeyData == 'V' ) )
			{
				if( (offSet + outLimit) < aggSvNum )
				{
					offSet += outLimit;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'c') || (inKeyData == 'C' ) )
			{
				offSet -= outLimit;
				if( offSet < 0 )
				{
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}

			/* キー入力が計測項目の画面切替の場合 */
			if( (inKeyData == 'n') || (inKeyData == 'N' ) )
			{
				offSet = 0;
				if( infoNumb >= 0 )
				{
					while( 1 )
					{
						infoNumb++;
						/* 終端まで行った場合は、現在のSVを表示 */
						if( infoNumb >= PV_MAX_PVSYS_AGGSVPOS )
							infoNumb = 0;
						if( ps_g_SVAGGINF[infoNumb].statFlag != 0 )
							break;
					}
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'b') || (inKeyData == 'B' ) )
			{
				offSet = 0;
				if( infoNumb >= 0 )
				{
					while( 1 )
					{
						/* 先頭まで行った場合は、現在のSVを表示 */
						if( infoNumb == 0 )
							infoNumb = PV_MAX_PVSYS_AGGSVPOS -1;
						else
							infoNumb--;
						
						if( ps_g_SVAGGINF[infoNumb].statFlag != 0 )
							break;
					}
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 計測監視情報表示
 *
 * 機能：
 *		計測監視情報を表示する。
 *      計測監視情報を１項目ずつ表示する。キー入力により表示項目および期間演算情
 *      報と表示内容を切り替える。
 *		初期状態は集約演算情報のうち実装する最小番号の情報を最初に表示する。
 */
void	 func_display_tmmoninfo( )
{
	int						dispMode = 0;	/* 表示モード */
	int						infoNumb=0;		/* 集約情報番号 */
	int						svifLink;		/* 状態情報リンク */
	int						svstLink;		/* 装置状態リンク情報 */
	int						svstBase;		/* 装置状態リンク情報（ベース値） */
	int						aggSvNum;		/* 集約情報数 */
	int						aggSvifLink;	/* 状態情報リンク */
	int						calcTmDifNumb;	/* 演算計測番号 */
	int						loopCnt1;		/* ループカウンタ */
	int						loopCnt2;		/* ループカウンタ */
	int						outLimit;		/* 表示可能行数 */
	long long				calcData;		/* 演算データ */
	long long				pow10Data;		/* べき乗データ */
	long long				spow10Data;		/* べき乗データ */
	struct tm				localTime;		/* 現在時刻 */
	unsigned char			svstMask;			/* 状態マスク値 */
	time_t					nowTime;		/* 現在時刻 */
	int						tmifLink;		/* 計測情報リンクポインタ */
	char					inKeyData;		/* キー入力値 */
	char					readByte;		/* キー入力バイト数 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;	/* 端末サイズ保存用 */
	struct st_svaggdata		*ps_svaggdata;	/* 集約対象 */
	int						offSet = 0;		/* 表示オフセット */
	int						svstVal;		/* 現在値 */
	char					svStat;			/* 現在値(状態リンク)バイト  */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 5 );

		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <%s (計測監視情報)> ============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			s_machineName[ 0 ].nameShort,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");

		if( dispMode == 0 )
		{
			printf("No|監視状態____ 名称________________  状態_\n");
			printf("\033[0K");

			/* 画面に表示可能な行数分表示する */
			for( loopCnt1 = offSet ; ( loopCnt1 < PV_MAX_PVSYS_MONSVPOS ) && ( (loopCnt1 - offSet) < outLimit ) ; loopCnt1++ )
			{
				if( ps_g_TMMONINF[loopCnt1].statFlag == 0 )
				{
					printf("%02d|[--,---,---] --------------------  -----\n", loopCnt1+1 );
				}
				else
				{
					svstBase = 	ps_g_PVSYSINF->s_machineInfo[ (ps_g_SVDATINF + svifLink)->soKind ].svStLink +
								((ps_g_PVSYSINF->s_machineInfo[ (ps_g_SVDATINF + svifLink)->soKind ].maxSv - 1) / 8 + 1) * ((ps_g_SVDATINF + svifLink)->soNumb-1);

					func_display_cmn_strlenBlank( 20, ps_g_TMMONINF[loopCnt1].svName );
					printf("%02d|[%02d,%03d,%03d] %s ", loopCnt1+1, ps_g_TMMONINF[loopCnt1].soKind, ps_g_TMMONINF[loopCnt1].soNumb, ps_g_TMMONINF[loopCnt1].svNumb, c_strData );
					svifLink = ps_g_TMMONINF[loopCnt1].svIfLink;
					/* 装置状態取得のためのマスク値を設定 */
					svstMask = (unsigned char)(0x80 >> (((ps_g_SVDATINF + svifLink)->svNumb - 1) % 8));
					/* 装置状態の取得と表示 */
					if( (ps_g_SVSTSFIL + svstBase + ((ps_g_SVDATINF + svifLink)->svNumb - 1) / 8 )->svStat & svstMask )
						printf("[[ 1 ]] ");
					else
						printf("[[ 0 ]] ");
					printf("\n");
				}
			}
		}
		else
		{
			printf("No|監視対象____ 名称________________ FL VALUE_____ | 基準値__ 誤差範囲 不感帯__\n");

			/* 画面に表示可能な行数分表示する */
			for( loopCnt1 = offSet ; ( loopCnt1 < PV_MAX_PVSYS_MONSVPOS ) && ( (loopCnt1 - offSet) < outLimit ) ; loopCnt1++ )
			{
				if( ps_g_TMMONINF[loopCnt1].statFlag == 0 )
				{
					printf("%02d|[--,---,---] --- ---------------- -- ---------- | -------- -------- --------\n", loopCnt1+1 );
				}
				else
				{
					func_display_cmn_strlenBlank( 20, ps_g_TMDATINF[ps_g_TMMONINF[loopCnt1].tmIfLink].tmName );
					printf("%02d|[%02d,%03d,%03d] %s %02X %10d | %8d %8d %8d\n", loopCnt1+1, ps_g_TMMONINF[loopCnt1].soKindMoni, ps_g_TMMONINF[loopCnt1].soNumbMoni, ps_g_TMMONINF[loopCnt1].tmNumbMoni, c_strData,
						ps_g_TMSTSFIL[ps_g_TMMONINF[loopCnt1].tmIfLink].avltmFlag, ps_g_TMSTSFIL[ps_g_TMMONINF[loopCnt1].tmIfLink].avltmVal, ps_g_TMMONINF[loopCnt1].baseVal, ps_g_TMMONINF[loopCnt1].gosa, ps_g_TMMONINF[loopCnt1].deadband );
				}
			}
		}
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
	
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q]       表示切替:[d]       頁切替:[前:b / 次:n]                (5秒更新)");
		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 読み込みエラーの場合は処理を抜ける */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は処理を抜ける */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が計測項目の画面切替の場合 */
			if( (inKeyData == 'n') || (inKeyData == 'N' ) )
			{
				if( (offSet + outLimit) < aggSvNum )
				{
					offSet += outLimit;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData == 'b') || (inKeyData == 'B' ) )
			{
				offSet -= outLimit;
				if( offSet < 0 )
				{
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}

			/* キー入力が表示切替の場合は表示モードを切り替える */
			if( (inKeyData == 'd') || (inKeyData == 'D') )
			{
				dispMode = (dispMode + 1 ) % 2;
				/* 画面をクリア */
				printf("\033[2J");
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * デマンド監視情報に関する設定情報表示
 *
 * 機能：
 *		デマンド監視情報に関する設定情報保存ファイルの状態表示を行う
 *
 */
void	func_display_demandinfo()
{
	char				workStr[32];	/* ワーク */
	int					offSet = 0;		/* オフセット */
	int					schNo = 0;		/* スケジュール番号 */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=6;		/* 表示不可行数 */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	int					tmifLink;		/* 計測情報リンクポインタ */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = 50;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <デマンド監視情報> ===================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );


		if( ps_g_DEMNDINF->statFlag == 0 )
		{
			printf("\033[0K");	printf(" 未設定 \n");
		}
		else
		{
			/* 装置種別に合わせて計測情報のリンクポインタ値を取得する */
			tmifLink = ps_g_PVSYSINF->s_machineInfo[ ps_g_DEMNDINF->soKind ].tmIfLink + ps_g_PVSYSINF->s_machineInfo[ ps_g_DEMNDINF->soKind ].maxTm * (ps_g_DEMNDINF->soNumb-1) + (ps_g_DEMNDINF->tmNumb-1);

			printf("\033[0K");	printf("装置種別     : %02d(%s)\n", ps_g_DEMNDINF->soKind, s_machineName[ ps_g_DEMNDINF->soKind ].nameShort );
			printf("\033[0K");	printf("装置番号     : %02d\n", ps_g_DEMNDINF->soNumb );
			printf("\033[0K");	printf("計測番号     : %3d\n", ps_g_DEMNDINF->tmNumb );
			printf("\033[0K");	printf("計測種別     : %s\n", c_tmKindName[ (ps_g_TMDATINF + tmifLink)->tmKind ] );
			printf("\033[0K");	printf("単位         : ");	func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );	printf("\n");
			printf("\033[0K");	printf("乗率         : %d\n", (ps_g_TMDATINF + tmifLink)->expData );
			printf("\033[0K");	printf("計測名称     : %s\n", (ps_g_TMDATINF + tmifLink)->tmName );
			printf("\033[0K");
			printf("--------------------------------------------------------------------------------\n");
			printf("\033[0K");	printf("開始時刻                : %d\n", ps_g_DEMNDINF->startUpTime );
			printf("\033[0K");	printf("契約電力(w)             : %d\n", ps_g_DEMNDINF->dmdContractPow );
			for( loopCnt1 = 0; loopCnt1 < 5; loopCnt1++ )
			{
				printf("\033[0K");	printf("デマンド監視Lv%d閾値(wh) : ", loopCnt1+1);
				if( ps_g_DEMNDINF->s_thresholdInf[loopCnt1].statFlag == 0 )
				{
					printf("未設定\n");
				}
				else
				{
					printf("%d\n", ps_g_DEMNDINF->s_thresholdInf[loopCnt1].thresholdVal );
				}
			}
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                                                              (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * CEMS制御情報表示
 *
 * 機能：
 *		CEMS制御情報保存ファイルの表示を行う
 *
 */
void	func_display_cemsdinfo()
{
	char				workStr[32];	/* ワーク */
	int					offSet = 0;		/* オフセット */
	int					schNo = 0;		/* スケジュール番号 */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=6;		/* 表示不可行数 */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <CEMS制御情報> =======================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
/* [20230217] Add St */
		printf("\033[0K");  printf("機能優先度----------------------------------------------------------------\n");
		printf("\033[0K");  printf("  価格連動制御                      ：%d\n", ps_g_CEMSPINF->arbitragePri);
		printf("\033[0K");  printf("  ピークカット                      ：%d\n", ps_g_CEMSPINF->peakCutPri);
		printf("\033[0K");  printf("  BCP                               ：%d\n", ps_g_CEMSPINF->bcpPri);
		printf("\033[0K");  printf("  需給調整市場                      ：%d\n", ps_g_CEMSPINF->balancePri);		/* [20240220] Add */
		printf("\033[0K");  printf("  容量市場                          ：%d\n", ps_g_CEMSPINF->capacityPri);		/* [20240220] Add */
		printf("\033[0K");  printf("制御機能------------------------------------------------------------------\n");
/* [20230217] Add Ed */
		printf("\033[0K");  printf("  BT充放電制御                      ：%s\n", (ps_g_CEMSPINF->btControl == 1 ) ? "有効" : "無効");
/* [20230301] Chg St */
		printf("\033[0K");  printf("     BT充電変化量(100ms/w)          ：%d\n", ps_g_CEMSPINF->btChangeAmtChg );
		printf("\033[0K");  printf("     BT放電変化量(100ms/w)          ：%d\n", ps_g_CEMSPINF->btChangeAmtDischg );
		if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].machineCnt !=  0 )
		{
			printf("\033[0K");  printf("     PV抑制変化量(100ms/w)          ：%s\n", ps_g_CEMSPINF->pvChangeAmtDown );
			printf("\033[0K");  printf("     PV発電変化量(100ms/w)          ：%d\n", ps_g_CEMSPINF->pvChangeAmtUp );
		}
/* [20230301] Chg Ed */
/* [20230217] Del St */
		//printf("\033[0K");  printf("  受電電力一定制御                  ：%s\n", (ps_g_CEMSPINF->rcvPowConst == 1 ) ? "有効" : "無効");
		//printf("\033[0K");  printf("  過充電回避制御                    ：%s\n", (ps_g_CEMSPINF->ovrChgAvoid == 1 ) ? "有効" : "無効");
		//printf("\033[0K");  printf("  デマンド制御                      ：%s\n", (ps_g_CEMSPINF->dmdControl == 1 ) ? "有効" : "無効");
/* [20230217] Del Ed */
		printf("\033[0K");  printf("設定値------------------------------------------------------------------\n");
		printf("\033[0K");  printf("  制御開始時間(秒)                  ：%4d\n", ps_g_CEMSPINF->ctlCountDown);
		printf("\033[0K");  printf("  シャットダウン待機時間(分)        ：%4d\n", ps_g_CEMSPINF->shutdownWait);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                                                              (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 受電電力一定制御情報表示
 *
 * 機能：
 *		受電電力一定制御情報保存ファイルの表示を行う
 *
 */
void	func_display_rcvpowerinfo()
{
	char				workStr[32];	/* ワーク */
	int					offSet = 0;		/* オフセット */
	int					schNo = 0;		/* スケジュール番号 */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=6;		/* 表示不可行数 */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = 50;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <受電電力一定制御情報> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		printf("\033[0K");  printf("  BCP用SOC (%)                   : %d\n", ps_g_RCVPWINF->BCPSoc );
		printf("\033[0K");  printf("  受電電力脱落負荷量 (w)         : %d\n", ps_g_RCVPWINF->dropOffLoadPower );
		printf("\033[0K");  printf("  制御不感帯 (W)                 : %d\n", ps_g_RCVPWINF->deadZone );		/* [20230220] Chg */
		printf("\033[0K");  printf("  放電再開SOC (%)                : %d\n", ( ps_g_RCVPWINF->BCPSoc + ps_g_RCVPWINF->dischgRestartSoC ) );
		printf("\033[0K");  printf("  充電再開SOC (%)                : %d\n", ( ps_g_BATTEINF->socUpper - ps_g_RCVPWINF->chgRestartSoC ) );
		printf("\033[0K");  printf("  受電電力平均時間 (ms)          : %d\n", ps_g_RCVPWINF->rcvPowAveTime );
		printf("\033[0K");  printf("  負荷電力平均時間 (ms)          : %d\n", ps_g_RCVPWINF->loadPowAveTime );
		printf("\033[0K");  printf("  受電電力目標値変化量 (w)       : %d\n", ps_g_RCVPWINF->rcvPowChangeAmt );
		printf("\033[0K");  printf("  デマンド閾値 (w)               : %d\n", ps_g_RCVPWINF->rcvPowMaxDemand );
		printf("\033[0K");  printf("  受電電力最低電力 (w)           : %d\n", ps_g_RCVPWINF->rcvPowMinDemand );
		printf("\033[0K");  printf("  SoC制御不感帯 (wh)             : %d\n", ps_g_RCVPWINF->deadZoneSoC );		/* [20240220] Add */
		printf("\033[0K");  printf("PV発電加算テーブル--------------------------------------------------------\n");
		printf("\033[0K");  printf(" No. | 負荷電力閾値 | PV発電加算値\n");
		for( loopCnt1 = 0; loopCnt1 < ps_g_RCVPWINF->pvAddPowInfNum; loopCnt1++ )
		{
			printf("\033[0K");  printf(" %2d  | %12d | %12d\n", loopCnt1+1, ps_g_RCVPWINF->s_pvAddPowInf[loopCnt1].loadPowLimit, ps_g_RCVPWINF->s_pvAddPowInf[loopCnt1].addPvPower);
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                                                              (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 過充電回避制御情報表示
 *
 * 機能：
 *		過充電回避制御情報保存ファイルの表示を行う
 *
 */
void	func_display_overchargeinfo()
{
	char				workStr[32];	/* ワーク */
	int					offSet = 0;		/* オフセット */
	int					schNo = 0;		/* スケジュール番号 */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=6;		/* 表示不可行数 */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = 50;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <過充電回避制御情報> =================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		printf("\033[0K");  printf("  脱落負荷量(w)          ：%8d\n", ps_g_OVRCHINF->dropOffLoadPower );
		printf("\033[0K");  printf("  PV発電再開SoC(%)       ：%3d\n", ps_g_OVRCHINF->pvRestartSoc );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                                                              (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/* [20251218] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 一次調整力パラメータ情報表示
 *
 * 機能：
 *		一次調整力パラメータ情報、一次調整力レポート蓄積項目情報の表示を行う
 *
 */
void	func_display_fcrparaminfo()
{
	char				workStr[32];	/* ワーク */
	int					offSet = 0;		/* オフセット */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=6;		/* 表示不可行数 */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	int					integers;		/* 整数部 */
	int					decimals;		/* 小数部 */
	int					tmifLink;		/* 計測情報リンクポインタ */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = 50;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <一次調整力パラメータ> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* 基準周波数が0の場合 */
		if( ps_g_FCRSTINF->baseFreq == 0 )
		{
			printf("未設定\n");
		}
		else
		{
			printf("\033[0K");  printf("  基準周波数 (Hz)        ：%4d.%03d\n", (ps_g_FCRSTINF->baseFreq/1000), (ps_g_FCRSTINF->baseFreq%1000));
			printf("\033[0K");  printf("  調停率(%)              ：%4d.%03d\n", (ps_g_FCRSTINF->variationRate/1000), (ps_g_FCRSTINF->variationRate%1000));
			printf("\033[0K");  printf("  不感帯(Hz)             ：%4d.%03d\n", (ps_g_FCRSTINF->deadBand/1000), (ps_g_FCRSTINF->deadBand%1000));
			printf("\033[0K");  printf("     上限                ：%4d.%03d\n", (ps_g_FCRSTINF->deadBandUpper/1000), (ps_g_FCRSTINF->deadBandUpper%1000));
			printf("\033[0K");  printf("     下限                ：%4d.%03d\n", (ps_g_FCRSTINF->deadBandLower/1000), (ps_g_FCRSTINF->deadBandLower%1000));
			printf("\033[0K");  printf("  異常判定周波数 (Hz)    ：%4d.%03d\n", (ps_g_FCRSTINF->freqDevtThrsld/1000), (ps_g_FCRSTINF->freqDevtThrsld%1000));
			printf("\033[0K");  printf("     上限                ：%4d.%03d\n", (ps_g_FCRSTINF->freqDevtThrsldUpper/1000), (ps_g_FCRSTINF->freqDevtThrsldUpper%1000));
			printf("\033[0K");  printf("     下限                ：%4d.%03d\n", (ps_g_FCRSTINF->freqDevtThrsldLower/1000), (ps_g_FCRSTINF->freqDevtThrsldLower%1000));
			printf("\033[0K");  printf("  異常時出力継続時間 (秒)：%4d\n", ps_g_FCRSTINF->faultDuration);
			printf("\033[0K");  printf("  定格出力 (w)           ：%9d\n", ps_g_FCRSTINF->ratedPower);
			printf("\033[0K");  printf("  放電時出力変化量 (w)   ：%9d\n", ps_g_FCRSTINF->dischgPowerChgAmount);
			printf("\033[0K");  printf("  充電時出力変化量 (w)   ：%9d\n", ps_g_FCRSTINF->chgPowerChgAmount);
			printf("\033[0K");  printf("  不感帯応動種別         ：%s\n", (ps_g_FCRSTINF->deadBandResType == PV_FCRST_DEADBAND_TYPE_NONE ) ? "不感帯なし" : (ps_g_FCRSTINF->deadBandResType == PV_FCRST_DEADBAND_TYPE_ONE ) ? "不感帯ありパターン1※応動の開始≠0w" : "不感帯ありパターン2※応動の開始＝0w");
			printf("\033[0K");  printf("  充電応動有無           ：%s\n", (ps_g_FCRSTINF->chgResponsiveEnabled == 1 ) ? "有効" : "無効");
			printf("\033[0K");  printf("--------------------------------------------------------------------------------\n");
			printf("\033[0K");  printf("■一次調整力レポート蓄積情報\n");
			printf("\033[0K");  printf("  No レポート対象 LINK_ 単位,乗率___ 直値_________________ 計測名称_____________\n");
		
			for( loopCnt1 = 0 ; loopCnt1 < ps_g_FCRRPINF->dataMax ; loopCnt1++ )
			{
				/* 装置種別に合わせて計測情報のリンクポインタ値を取得する */
				tmifLink = ps_g_PVSYSINF->s_machineInfo[ ps_g_FCRRPINF->s_fcrrpData[loopCnt1].mKind ].tmIfLink + ps_g_PVSYSINF->s_machineInfo[ ps_g_FCRRPINF->s_fcrrpData[loopCnt1].mKind ].maxTm * (ps_g_FCRRPINF->s_fcrrpData[loopCnt1].mNumb-1) + (ps_g_FCRRPINF->s_fcrrpData[loopCnt1].tmNumb-1);

				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");

				printf("  %02d [%02d,%03d,%03d] %05d [",
					ps_g_FCRRPINF->s_fcrrpData[loopCnt1].dataNumb,
					ps_g_FCRRPINF->s_fcrrpData[loopCnt1].mKind,
					ps_g_FCRRPINF->s_fcrrpData[loopCnt1].mNumb,
					ps_g_FCRRPINF->s_fcrrpData[loopCnt1].tmNumb,
					tmifLink );

				func_display_unit( ps_g_TMDATINF[tmifLink].unitKind );
				printf(",%3d] [FL:%02X VL:%10d] %s\n",
					ps_g_TMDATINF[tmifLink].expData,
					ps_g_TMSTSFIL[tmifLink].avltmFlag,
					ps_g_TMSTSFIL[tmifLink].avltmVal,
					ps_g_TMDATINF[tmifLink].tmName );
			}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
			

		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                                                              (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}
/* [20251218] Add Ed */


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 文字列全角/半角統一
 *
 * 機能：
 *		文字数と全角/半角の入力文字から文字数に合わせた文字列をc_strDataに格納する
 *
 */
void func_display_cmn_strlenBlank( int strSize, char *pc_strData)
{
	int			loopCnt1;
	int			strCnt = 0;
	int			viewSize;
	char		*pcInStr;
	char		*pcOutStr;
	int			charByte;

//printf("call IN : func_display_cmn_strlenBlank\n");
//printf("pcInStr(%s), strlen=%d :[", pc_strData, strlen(pc_strData));

	// 先頭ポインタを格納
	pcInStr = pc_strData;
	pcOutStr = c_strData;
	memset(c_strData, 0, sizeof(c_strData));


	// 文字数分ループ
	for( loopCnt1 = 0; loopCnt1 < strlen( pc_strData ); loopCnt1++ )
	{
		/* 終端なら抜ける */
		if( *pcInStr == 0 )
		{
			break;
		}

		/* 半角チェック */
		if( (( *pcInStr >= '0' ) && ( *pcInStr <= '9' )) ||
			(( *pcInStr >= 'a' ) && ( *pcInStr <= 'z' )) ||
			(( *pcInStr >= 'A' ) && ( *pcInStr <= 'Z' )) ||
			( *pcInStr == ')' ) ||
			( *pcInStr == '(' ) ||
			( *pcInStr == '&' ) ||
			( *pcInStr == ' ' ) ||
			( *pcInStr == '-' ) ||
			( *pcInStr == ':' ) ||
			( *pcInStr == '.' ) ||
			( *pcInStr == '/' ))
		{
			/* データバイト数 */
			charByte = 1;
			/* 表示文字数 */
			viewSize = 1;
		}
		/* 全角チェック */
		else
		{
			/* データバイト数 */
			charByte = 3;
			/* 表示文字数 */
			viewSize = 2;
		}


		/* 表示文字数を超えたら抜ける */
		if( (strCnt + viewSize ) > strSize )
		{
			break;
		}
		/* 文字データをコピー */
		memcpy(pcOutStr, pcInStr, charByte);
//printf("%d(%s) ", charByte, pcOutStr);

		/* ポインタを移動 */
		pcOutStr += charByte;
		pcInStr += charByte;
		strCnt += viewSize;
	}

//printf("], strCnt=%d", strCnt );
	/* 表示の残りを半角スペースで埋める */
	for( loopCnt1 = strCnt; loopCnt1 < strSize; loopCnt1 ++)
	{
		*pcOutStr = ' ';
		pcOutStr++;
	}
//printf("\n");

}




/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 制御情報共通表示
 *
 * 機能：
 *		制御情報の共通表示を行う
 *
 */
void func_display_ctrlinfo( struct st_CTLINF *ps_CTLINF )
{
	int			loopCnt1;
	char		*pcSvName;
	char		cSvName[16];
	
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[0K");
	printf("  |制御不可情報    |強| 設定情報                           |応動情報\n");
	printf("\033[0K");
	printf("No|[種,番,SvNo,Val]|制|[種,番,SvNo] 名称           VAL TIM |[種,番,SvNo] 名称             VAL TIM\n");

	// 制御数上限分ループ
	for( loopCnt1 = 0; loopCnt1 < PV_MAX_CTL_NUM; loopCnt1++ )
	{
		/* 設定制御がある場合 */
		if( ps_CTLINF[loopCnt1].statFlag == 1 )
		{
			printf("%2d|", loopCnt1+1);

			if( ps_CTLINF[loopCnt1].statFlagNotCtl == 1 )
			{
				printf("[%2d,%3d,%3d,%3d]|", ps_CTLINF[loopCnt1].soKindNotCtl, ps_CTLINF[loopCnt1].soNumbNotCtl, ps_CTLINF[loopCnt1].svNumbNotCtl, ps_CTLINF[loopCnt1].svValNotCtl );
			}
			else
			{
				printf("[--,---,---,---]|");
			
			}

			printf("\033[0K");
			func_display_cmn_strlenBlank(14, (ps_g_SVDATINF + ps_CTLINF[loopCnt1].svIfLinkCtl)->svName);
			printf("%2d|[%02d,%03d,%03d] %s %3d %3d |", 
				ps_CTLINF[loopCnt1].forceCtl, ps_CTLINF[loopCnt1].soKindCtl, ps_CTLINF[loopCnt1].soNumbCtl, ps_CTLINF[loopCnt1].svNumbCtl, c_strData, ps_CTLINF[loopCnt1].svValCtl, ps_CTLINF[loopCnt1].oneShot );

			if( ps_CTLINF[loopCnt1].statFlagRes == 1 )
			{
				func_display_cmn_strlenBlank(14, (ps_g_SVDATINF + ps_CTLINF[loopCnt1].svIfLinkRes)->svName);
				printf("[%2d,%3d,%3d] %s %3d %3d\n", ps_CTLINF[loopCnt1].soKindRes, ps_CTLINF[loopCnt1].soNumbRes, ps_CTLINF[loopCnt1].svNumbRes, c_strData, ps_CTLINF[loopCnt1].svValRes, ps_CTLINF[loopCnt1].timeOutRes );
			}
			else
			{
				printf("[--,---,---]\n");
			}
		}
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * モード切替時制御情報表示
 *
 * 機能：
 *		モード切替時制御情報の表示を行う
 *
 */
void func_display_modectrlinfo( void )
{
	char				workStr[32];	/* ワーク */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=4;		/* 表示不可行数 */
	char				inKeyData;		/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	int					ctlMode = 0;	/* 表示切替 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = GET_OUT_LIMIT(termSize.ws_row, noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <モード切替時制御情報> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );


		printf("\033[0K");
		printf("切替モード ：%s\n", c_ctlModeName[ ctlMode+1 ]);
		printf("\033[0K");
		printf("制御情報   ：%s\n", (ps_g_MDCTLINF[ctlMode].statFlag == 0)? "未設定" : "設定" );
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");
		func_display_ctrlinfo( ps_g_MDCTLINF[ctlMode].s_ctlinfo);
		printf("\033[0K");
		printf("================================================================================\n");
		printf("\033[0K");
		printf("戻る:[q]             モード切替:[前:c / 次:v]                          (5秒更新)");

		fflush(NULL);

		if( signal_flg ) break;

		memset( &inKeyData, 0, sizeof(inKeyData));
		
		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

		/* キー入力が計測項目の画面切替の場合 */
		if( (inKeyData == 'v') || (inKeyData == 'V' ) )
		{
			if( (ctlMode + 1) < PV_CTL_MAX )
			{
				ctlMode ++;
				/* 画面をクリア */
				printf("\033[2J");
			}
		}
		if( (inKeyData == 'c') || (inKeyData == 'C' ) )
		{
			ctlMode--;
			if( ctlMode < 0 )
			{
				ctlMode = 0;
			}
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* 終了処理 */
		if( (inKeyData == 'q') || (inKeyData == 'Q' ) )
		{

		}

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	return;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * シリアル通信ログ設定
 *
 * 機能：
 *		シリアル通信のログ出力設定を行う
 *
 */
void	func_display_logsetting()
{
	char				logChange[PV_MAX_COMLINE + 2];	/* 通信回線数 */
	char				logSave[PV_MAX_COMLINE + 2];	/* 通信回線数 */

	char				workName[13];	/* 文字列処理用 */
	int					loopCnt1;		/* ループカウンタ */
	int					menuNo;			/* メニュー番号 */
	int					awsRcvFlag = 0;	/* 指令情報受信有無 */
	int					iotSndFlag = 0;	/* IoTデータ情報送信先有無 */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 通信回線数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_COMLINE ; loopCnt1++ )
	{
		/* 管理情報ファイルのログ設定フラグをコピーする */
		logChange[loopCnt1] = ps_g_PVSYSINF->lineLogFlag[loopCnt1];
		logSave[loopCnt1]   = ps_g_PVSYSINF->lineLogFlag[loopCnt1];
	}
	/* 管理情報ファイルのログ設定フラグをコピーする */
	logChange[PV_MAX_COMLINE]     = ps_g_PVSYSINF->awsRcvLogFlag;
	logSave[PV_MAX_COMLINE]       = ps_g_PVSYSINF->awsRcvLogFlag;
	logChange[PV_MAX_COMLINE + 1] = ps_g_PVSYSINF->iotSndLogFlag;
	logSave[PV_MAX_COMLINE + 1]   = ps_g_PVSYSINF->iotSndLogFlag;

	for( loopCnt1 = PV_MAX_COMLINE; loopCnt1 < PV_MAX_PROC; loopCnt1++ )
	{
		/* EMSプロセスが実装されている場合 */
		if( strcmp( ps_g_PVSYSINF->s_procInfo[loopCnt1].procName, PV_PROCNAME_BUSINESS03 ) == 0 )
		{
			awsRcvFlag = 1;
		}

		/* IOTデータ共有プロセスが実装されている場合 */
		if( strcmp( ps_g_PVSYSINF->s_procInfo[loopCnt1].procName, PV_PROCNAME_BUSINESS04 ) == 0 )
		{
			iotSndFlag = 1;
		}
	}


	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出しの表示 */
		printf("= 通信ログ設定 =================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("回線番号 PROTOCOL       現在の設定              将来の設定                      \n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------+--------------+--------------------+--+--------------------------------\n");

		/* 通信回線数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < (PV_MAX_COMLINE + 2) ; loopCnt1++ )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			if( loopCnt1 < PV_MAX_COMLINE )
			{

				/* 通信回線が未実装の場合は次の通信回線を処理する */
				if( (ps_g_COMLIINF + loopCnt1)->statFlag == 0 ) continue;

				/* 通信回線に定義された通信プロトコル名を取得して表示する */
				memset( workName, 0, sizeof( workName ) );
				strncpy( workName,  (ps_g_COMLIINF + loopCnt1)->protoName, 12);
				printf("   %X.    %-12s   ", loopCnt1 + 1, workName );

				if( ps_g_PVSYSINF->lineLogFlag[loopCnt1] == PV_SERIAL_LOG_AUTO )
					printf("エラー発生時のみ出力");
				else
					printf("常時出力　　　　　　");
		
				if( ps_g_PVSYSINF->lineLogFlag[loopCnt1] == logChange[loopCnt1] )
					printf("    （変更なし）\n");
				else
				{
					if( logChange[loopCnt1] == PV_SERIAL_LOG_AUTO )
						printf(" -> エラー発生時のみ出力（未確定）\n");
					else
						printf(" -> 常時出力　　　　　　（未確定）\n");
				}
			}
			else
			{
				if( loopCnt1 == PV_MAX_COMLINE )
				{
					if( ( iotSndFlag ) || ( awsRcvFlag ) )
						printf("--------+--------------+--------------------+--+--------------------------------\n");
	
					if( awsRcvFlag )
					{

						/* カーソルの位置からその行の右端までクリア */
						printf("\033[0K");

//						printf("   A.  AWS指令データ    ");
//						if( ps_g_PVSYSINF->awsRcvLogFlag == PV_LOG_ON )
//							printf("常時出力            ");
//						else
//							printf("出力なし　　　　　　");
//			
//						if( ps_g_PVSYSINF->awsRcvLogFlag == logChange[loopCnt1] )
//							printf("    （変更なし）\n");
//						else
//						{
//							if( logChange[loopCnt1] == PV_LOG_ON )
//								printf(" -> 常時出力            （未確定）\n");
//							else
//								printf(" -> 出力なし　　　　　　（未確定）\n");
//						}
					}
				}
				else
				{
					if( iotSndFlag )
					{
						printf("   S.  IOTデータ送信    ");
						if( ps_g_PVSYSINF->iotSndLogFlag == PV_LOG_ON )
							printf("常時出力            ");
						else
							printf("出力なし　　　　　　");
			
						if( ps_g_PVSYSINF->iotSndLogFlag == logChange[loopCnt1] )
							printf("    （変更なし）\n");
						else
						{
							if( logChange[loopCnt1] == PV_LOG_ON )
								printf(" -> 常時出力            （未確定）\n");
							else
								printf(" -> 出力なし　　　　　　（未確定）\n");
						}
					}
				}
			}
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("注意：常時出力に設定する場合は、障害調査時かつ１回線のみの運用として下さい。\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る[q]  設定変更[番号]  設定変更の確定[w]  元に戻す[c]");
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が確定の場合 */
		if( (inKeyData[0] == 'w') || (inKeyData[0] == 'W') )
		{
			/* 通信回線数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_COMLINE ; loopCnt1++ )
			{
				/* 設定変更内容を管理情報ファイルのログ設定フラグに反映する */
				ps_g_PVSYSINF->lineLogFlag[loopCnt1] = logChange[loopCnt1];
			}
			ps_g_PVSYSINF->awsRcvLogFlag = logChange[PV_MAX_COMLINE];
			ps_g_PVSYSINF->iotSndLogFlag = logChange[PV_MAX_COMLINE + 1];
		}

		/* キー入力が戻すの場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			/* 通信回線数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_COMLINE ; loopCnt1++ )
			{
				/* メニュー開始時に保存しておいた内容を管理情報ファイルのログ設定フラグに反映する */
				ps_g_PVSYSINF->lineLogFlag[loopCnt1] = logSave[loopCnt1];
				logChange[loopCnt1] = logSave[loopCnt1];
			}
			ps_g_PVSYSINF->awsRcvLogFlag = logSave[PV_MAX_COMLINE];
			logChange[PV_MAX_COMLINE] = logSave[PV_MAX_COMLINE];
			ps_g_PVSYSINF->iotSndLogFlag = logSave[PV_MAX_COMLINE + 1];
			logChange[PV_MAX_COMLINE + 1] = logSave[PV_MAX_COMLINE + 1];
		}

		/* メニュー番号を取得 */
		if( (inKeyData[0] == 'a') || (inKeyData[0] == 'A') )
			menuNo = PV_MAX_COMLINE + 1;
		else if( (inKeyData[0] == 's') || (inKeyData[0] == 'S') )
			menuNo = PV_MAX_COMLINE + 2;
		else
			menuNo = atoi( inKeyData );

		/* メニュー番号が通信回線の範囲内 */
		if( (menuNo > 0 ) && ( menuNo < (PV_MAX_COMLINE + 1) ) )
		{
			/* 通信回線が実装の場合 */
			if( (ps_g_COMLIINF + menuNo - 1)->statFlag != 0 )
			{
				if( logChange[menuNo - 1] == PV_SERIAL_LOG_AUTO )
					logChange[menuNo - 1] = PV_SERIAL_LOG_ON;
				else
					logChange[menuNo - 1] = PV_SERIAL_LOG_AUTO;
			}
		}
		/* メニュー番号が通信回線の範囲内 */
		if( ( menuNo == (PV_MAX_COMLINE+1) ) || ( menuNo == (PV_MAX_COMLINE+2) ) )
		{
			if( logChange[menuNo - 1] == PV_LOG_ON )
				logChange[menuNo - 1] = PV_LOG_OFF;
			else
				logChange[menuNo - 1] = PV_LOG_ON;
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	
}

/* [20230224] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * オプション設定
 *
 * 機能：
 *		オプション設定を行う
 *
 */
void	func_display_setoption()
{
	char				flgChange01;		/* フラグ設定 */
	char				flgSave01;		/* フラグ設定 */
	char				flgChange02;		/* フラグ設定 */
	char				flgSave02;		/* フラグ設定 */

	char				workName[13];	/* 文字列処理用 */
	int					loopCnt1;		/* ループカウンタ */
	int					menuNo;			/* メニュー番号 */
	int					dtsndFlag = 0;	/* 他発電所向け情報送信先有無 */
	int					dtrcvFlag = 0;	/* 他発電所情報受信有無 */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 画面初期設定 */
	flgChange01 = ps_g_PVSYSINF->recFileCre;
	flgSave01 = ps_g_PVSYSINF->recFileCre;
	flgChange02 = ps_g_PVSYSINF->sslDataFlag;
	flgSave02 = ps_g_PVSYSINF->sslDataFlag;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出しの表示 */
		//printf("= 計測ファイル出力 =============================================================\n");
		printf("= オプション設定   =============================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("                              現在の設定              将来の設定                      \n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("-----------------------------+--------------------+--+--------------------------\n");

		printf("\033[0K");
		printf(" 1.計測・状態蓄積ファイル出力 ");
   
		if( ps_g_PVSYSINF->recFileCre == PV_SERIAL_WRITE_ON )
			printf("出力する            ");
		else
			printf("出力しない          ");

		if( ps_g_PVSYSINF->recFileCre == flgChange01 )
			printf("    （変更なし）\n");
		else
		{
			if( flgChange01 == PV_SERIAL_WRITE_ON )
				printf(" ->   出力する      （未確定）\n");
			else
				printf(" ->   出力しない    （未確定）\n");
		}

		printf("\033[0K");
		printf(" 2.SSL通信データファイル出力  ");
   
		if( ps_g_PVSYSINF->sslDataFlag == PV_SERIAL_WRITE_ON )
			printf("出力する            ");
		else
			printf("出力しない          ");

		if( ps_g_PVSYSINF->sslDataFlag == flgChange02 )
			printf("    （変更なし）\n");
		else
		{
			if( flgChange02 == PV_SERIAL_WRITE_ON )
				printf(" ->   出力する      （未確定）\n");
			else
				printf(" ->   出力しない    （未確定）\n");
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る[q]  設定変更[1]  設定変更の確定[w]  元に戻す[c]");
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

/* [Ver1.11] Add St */
		if( signal_flg ) break;
/* [Ver1.11] Add Ed */

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が確定の場合 */
		if( (inKeyData[0] == 'w') || (inKeyData[0] == 'W') )
		{
			/* 設定変更内容を管理情報ファイルに反映する */
			ps_g_PVSYSINF->recFileCre = flgChange01;
			ps_g_PVSYSINF->sslDataFlag = flgChange02;
		}

		/* キー入力が戻すの場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			ps_g_PVSYSINF->recFileCre = flgSave01;
			flgChange01 = flgSave01;

			ps_g_PVSYSINF->sslDataFlag = flgSave02;
			flgChange02 = flgSave02;
		}

		/* メニュー番号を取得 */
		if( inKeyData[0] == '1')
		{
			flgChange01 = (flgChange01+1) % 2;
		}
		if( inKeyData[0] == '2')
		{
			flgChange02 = (flgChange02+1) % 2;
		}

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	
}
/* [20230224] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 数値文字列チェック
 *
 * 機能：
 *		数値文字列のチェックを行う
 *
 */
int  decchk( char *decStr)
{
    int  retCode=0;
    int  len;
    int  loopCnt;

    /* １０進数チェック */
    len = strlen(decStr);
    for(loopCnt = 0, retCode = 0; loopCnt < len; loopCnt++)
    {
        if( (!(('0' <= decStr[loopCnt]) && (decStr[loopCnt] <= '9'))) &&
        	( '-' != decStr[loopCnt] ))
        {
            retCode = -1;
            break;
        }
    }

    return(retCode);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * PVマニュアル設定
 *
 * 機能：
 *		PVマニュアル設定を行う
 *
 */
void	func_display_pvManual( )
{
	time_t				nowTime;		/* 現在時刻 */
	struct tm			localTime;		/* 現在時刻 */
	char				inKeyData[128];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	int					pvSetPow;
	int					pvSetPer;

	/* PVマニュアルモード判定 */
	if( ps_g_PVSYSINF->pvManualMode == 0 )
	{
		return;
	}

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 画面をクリア */
		printf("\033[2J");
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 端末サイズを取得 */
		func_termsize_get();
		/* 端末サイズを保存 */
		termSize_OLD.ws_row = termSize.ws_row;

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= PVマニュアル設定 =====================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
		
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("\n本モードは、PV出力制御に必要なSVのチェックは行っていません。\n");
		printf("\033[0K");	printf("必ず、PV出力制御を実施しても問題ないかチェックしてください。\n\n");
		printf("\033[0K");	printf("本モードをOFFにする際は、必ず出力制御を「0」に設定してから行ってください。\n\n");
		printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("PV発電容量合計(w)           : %d\n", ps_g_EMCTLFIL->pvRatedPow );
		printf("\033[0K");	printf("PV出力制御目標値(w)         : %d\n", ps_g_EMCTLFIL->pvReqPow );
		printf("\033[0K");	printf("PV出力制御指示値(w)         : %d\n", ps_g_EMCTLFIL->pvSetPow );
		printf("\033[0K");	printf("PV出力制御指示値(%)         : %d.%02d\n", ps_g_EMCTLFIL->pvSetPer/100, ps_g_EMCTLFIL->pvSetPer%100 );
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]      制御:[0-9]");

		printf("\033[0K");	printf("\n最大発電可能電力(kW)を入力してください。\n");
		printf("\033[0K");	printf(">>");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 128 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		
		inKeyData[strlen(inKeyData)-1]=0;
		/* 数値文字列チェック */
		if( decchk(inKeyData) != 0 )
			continue;

		pvSetPow = atoi(inKeyData);
		pvSetPer = ((long long)pvSetPow * 1000 * 10000 / ps_g_EMCTLFIL->pvRatedPow);
		if( pvSetPer > 10000 )
			pvSetPer = 10000;

		printf("\n");
		printf("\033[0K");
		printf("================================================================================\n");
		printf("\033[0K");	printf("\n下記の設定をよろしいですか？(y/n)\n");
		printf("\033[0K");	printf("最大可能発電電力(kW) : %d\n", pvSetPow);
		printf("\033[0K");	printf("最大可能発電電力(%)  : %d.%02d\n", pvSetPer/100, pvSetPer%100 );
		printf("\033[0K");	printf(">>");
		

		/* 反映確認 */
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 128 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'y') || (inKeyData[0] == 'Y') )
		{
			/* 設定を行う */
			if( (pvSetPow*1000) > ps_g_EMCTLFIL->pvRatedPow )
				ps_g_EMCTLFIL->pvReqPow = ps_g_EMCTLFIL->pvRatedPow;
			else
				ps_g_EMCTLFIL->pvReqPow = (pvSetPow*1000);
		}
		fflush(NULL);
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * BTマニュアル設定
 *
 * 機能：
 *		BTマニュアル設定を行う
 *
 */
void	func_display_btManual( )
{
	time_t				nowTime;		/* 現在時刻 */
	struct tm			localTime;		/* 現在時刻 */
	char				inKeyData[128];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	int					bttSetPow;

	/* BTマニュアルモード判定 */
	if( ps_g_PVSYSINF->btManualMode == 0 )
	{
		return;
	}

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 画面をクリア */
		printf("\033[2J");
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 端末サイズを取得 */
		func_termsize_get();
		/* 端末サイズを保存 */
		termSize_OLD.ws_row = termSize.ws_row;

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= BTマニュアル設定 =====================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
		
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("\n本モードは、BT出力制御に必要なSVのチェックは行っていません。\n");
		printf("\033[0K");	printf("必ず、BT出力制御を実施しても問題ないかチェックしてください。\n\n");
		printf("\033[0K");	printf("本モードをOFFにする際は、必ず出力制御を「0」に設定してから行ってください。\n\n");
		printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("バッテリ充電可能電力(w)     : %d\n", ps_g_EMCTLFIL->avlChgPow );
		printf("\033[0K");	printf("バッテリ放電可能電力(w)     : %d\n", ps_g_EMCTLFIL->avlDischgPow );
		printf("\033[0K");	printf("バッテリ充放電電力目標値(w) : %d\n", ps_g_EMCTLFIL->bttReqPow );
		printf("\033[0K");	printf("バッテリ充放電電力指示値(w) : %d\n", ps_g_EMCTLFIL->bttSetPow );
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]      制御:[0-9]");

		printf("\033[0K");	printf("\n充放電電力(kW)を入力してください。\n");
		printf("\033[0K");	printf(">>");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 128 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		inKeyData[strlen(inKeyData)-1]=0;
		/* 数値文字列チェック */
		if( decchk(inKeyData) != 0 )
			continue;

		bttSetPow = atoi(inKeyData);

		printf("\033[0K");
		printf("================================================================================\n");
		printf("\033[0K");	printf("\n下記の設定をよろしいですか？(y/n)\n");
		printf("\033[0K");	printf("充放電電力(kW) : %d\n", bttSetPow);
		printf("\033[0K");	printf(">>");
		

		/* 反映確認 */
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 128 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'y') || (inKeyData[0] == 'Y') )
		{
			/* 設定を行う */
			ps_g_EMCTLFIL->bttReqPow = bttSetPow*1000;
		}
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * マニュアルモード切替設定
 *
 * 機能：
 *		マニュアルモードの切替設定を行う
 *
 */
void	func_display_ManualMode()
{
	char				modeName[2][96] = { "ＯＦＦ           ", "ＯＮ             "};
	char				pvflgChange;		/* フラグ設定 */
	char				pvflgSave;			/* フラグ設定 */
	char				btflgChange;		/* フラグ設定 */
	char				btflgSave;			/* フラグ設定 */

	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 画面初期設定 */
	pvflgChange = ps_g_PVSYSINF->pvManualMode;
	pvflgSave = ps_g_PVSYSINF->pvManualMode;
	btflgChange = ps_g_PVSYSINF->btManualMode;
	btflgSave = ps_g_PVSYSINF->btManualMode;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出しの表示 */
		printf("= 計測ファイル出力 =============================================================\n");
		printf("= マニュアルモード設定   =======================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("                        現在の設定              将来の設定                      \n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("-----------------------+-----------------+--+-----------------------------------\n");

		printf("\033[0K");
		printf(" ＰＶマニュアルモード   %s", modeName[ps_g_PVSYSINF->pvManualMode]);
		if( ps_g_PVSYSINF->pvManualMode == pvflgChange )
			printf("       （変更なし）\n");
		else
		{
			printf(" ->     %s（未確定）\n", modeName[pvflgChange]);
		}
		printf("\033[0K");
		printf(" ＢＴマニュアルモード   %s", modeName[ps_g_PVSYSINF->btManualMode]);
		if( ps_g_PVSYSINF->btManualMode == btflgChange )
			printf("       （変更なし）\n");
		else
		{
			printf(" ->     %s（未確定）\n", modeName[btflgChange]);
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る[q]        設定変更[PV(p) / BT(b)]       設定変更の確定[w]       元に戻す[c]");
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が確定の場合 */
		if( (inKeyData[0] == 'w') || (inKeyData[0] == 'W') )
		{
			/* OFFにする際は、要求値を0にする */
			if( ( ps_g_PVSYSINF->pvManualMode != pvflgChange ) &&
				( pvflgChange == 0 ) )
			{
				ps_g_EMCTLFIL->pvReqPow = 0;
			}

			/* ONにする場合 */
			if( ( ps_g_PVSYSINF->pvManualMode != pvflgChange ) &&
				( pvflgChange == 1 ) )
			{
				/* CEMSモードがONGRIDで、受電電力一定制御がOFFの場合 */
				if( (( ps_g_EMCTLFIL->cemsMode == PV_CTL_ONGRID )||
					 ( ps_g_EMCTLFIL->cemsMode == PV_CTL_ERR_ONGRID ) ) &&
					( ps_g_CEMSPINF->rcvPowConst == 0 ))
				{
					/* 指示値を０にする */
					ps_g_EMCTLFIL->pvReqPow = 0;
				}
				/* CEMSモードがOFFGRIDで、過充電回避制御がOFFの場合 */
				else if( (( ps_g_EMCTLFIL->cemsMode == PV_CTL_OFFGRID )||
						  ( ps_g_EMCTLFIL->cemsMode == PV_CTL_ERR_OFFGRID ) ) &&
						 ( ps_g_CEMSPINF->ovrChgAvoid == 0 ))
				{
					/* 指示値を０にする */
					ps_g_EMCTLFIL->pvReqPow = 0;
				}
				/* PV抑制制御がOFF */
				else if( ps_g_CEMSPINF->pvControl == 0 )
				{
					/* 指示値を０にする */
					ps_g_EMCTLFIL->pvReqPow = 0;
				}
			}
			

			/* OFFにする際は、要求値を0にする */
			if( ( ps_g_PVSYSINF->btManualMode != btflgChange ) &&
				( btflgChange == 0 ) )
			{
				ps_g_EMCTLFIL->bttReqPow = 0;
			}

			/* ONにする際、BT充放電制御がOFF、または、受電電力一定制御がOFFの場合 */
			if( ( ps_g_PVSYSINF->btManualMode != btflgChange ) &&
				( btflgChange == 1 ) &&
				( (ps_g_CEMSPINF->btControl == 0 ) ||
				  (ps_g_CEMSPINF->rcvPowConst == 0 )) )
			{
				/* 指示値を０にする */
				ps_g_EMCTLFIL->bttReqPow = 0;
			}
			


			/* 設定変更内容を管理情報ファイルに反映する */
			ps_g_PVSYSINF->pvManualMode = pvflgChange;
			pvflgSave = pvflgChange;
			ps_g_PVSYSINF->btManualMode = btflgChange;
			btflgSave = btflgChange;
		}

		/* キー入力が戻すの場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			ps_g_PVSYSINF->pvManualMode = pvflgSave;
			pvflgChange = pvflgSave;
			ps_g_PVSYSINF->btManualMode = btflgSave;
			btflgChange = btflgSave;
		}

		/* PV切替 */
		if( (inKeyData[0] == 'p') || (inKeyData[0] == 'P') )
		{
			pvflgChange = (pvflgChange+1) % 2;
		}

		/* BT切替 */
		if( (inKeyData[0] == 'b') || (inKeyData[0] == 'B') )
		{
			btflgChange = (btflgChange+1) % 2;
		}
	}
	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 画面更新時間設定
 *
 * 機能：
 *		画面更新時間の設定を行う
 *
 */
void	func_display_setupdatetime()
{
	int						keyByte;
	char					keyData[32];
	int						menuNumb;

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出しの表示 */
		//printf("= 計測ファイル出力 =============================================================\n");
		printf("= 画面更新時間設定   ===========================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("  現在の更新時刻      ： %2d秒\n", viewTime/10);

		printf("\n");
		printf("\033[0K");
		printf("  変更後の更新時刻を入力してください。（１～６０）\n");
		printf("  （※２５秒以上は正しく更新されない）\n");
		printf("\033[0K");
		printf("================================================================================\n");
		printf("  >> ");
		fflush( NULL );

		if( signal_flg ) break;

		memset( keyData, 0, sizeof( keyData ) );
		keyByte = read( 0, &keyData, 1024 );

		if(keyByte < 0 )
			/* 終了 */
			break;

		/* 入力値を数値化 */
		menuNumb = atoi( keyData );

		if( ( menuNumb >= 1 ) && ( menuNumb <= 60 ) )
		{
			viewTime = menuNumb * 10;
		}
		break;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 時刻設定
 *
 * 機能：
 *		時刻の設定を行う
 *
 */
void	func_display_timesetting()
{
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct tm			checkTime;		/* 最終変更時刻 */
	struct tm			localTime;		/* 現在時刻 */
	struct tm			setTime;		/* 変更時刻 */
	struct tm			tmpTime;		/* 計算用領域 */
	time_t				nowTime;		/* 現在時刻 */
	time_t				setTimeSec;		/* 設定時刻 */
	time_t				sabunSec;		/* 差分時刻 */
	char				setTimeStr[20];	/* 設定時刻文字列 */
	char				keyCnt=0;		/* 文字数 */
	char				errMsg[128];	/* エラー文字列 */
	char				command[128];	/* コマンド文字列 */
	char				result=0;		/* 処理結果 */
	uid_t				user_id;

	memset(errMsg, 0, sizeof(errMsg));
	memset(setTimeStr, 0, sizeof(setTimeStr));

	/* 起動ユーザＩＤを判定 */
	user_id = getuid();

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 見出しの表示 */
		printf("= 時刻設定   ===================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );
		localtime_r( &ps_g_PVSYSINF->lastSetTime, &checkTime );

		/* 日付が変わった場合*/
		if( ( localTime.tm_year != checkTime.tm_year ) ||
		    ( localTime.tm_mon  != checkTime.tm_mon )  ||
		    ( localTime.tm_mday != checkTime.tm_mday ) )
		{
			/* 最終時刻を保持 */
			ps_g_PVSYSINF->lastSetTime = nowTime;
			/* 1日の累積変更時刻をリセット */
			ps_g_PVSYSINF->dayTimeSum = 0;
		}

		/* rootユーザの場合 */
		if( user_id == 0 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("    現在時刻                            変更時刻    \n");
			printf("    %04d/%02d/%02d %02d:%02d:%02d       ＞＞      ",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec);
			printf("\033[0K");
			printf("%s\n", setTimeStr );

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("  %s\n", errMsg);

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("戻る[q]       全て削除[c]     １文字削除[b]          設定変更の確定[w]");
		}
		else
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("    権限がない為、変更できません。\n");
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			printf("戻る[q]");
		}
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が確定の場合 */
		if( (inKeyData[0] == 'w') || (inKeyData[0] == 'W') )
		{
			if( keyCnt > 18 )
			{
				memcpy(&tmpTime, &setTime, sizeof(setTime));
			    tmpTime.tm_year -= 1900;  /* 西暦年 - 1900 */
			    tmpTime.tm_mon -= 1;      /* 月 ( 1月＝0 ) */
			    setTimeSec = mktime(&tmpTime);

				sabunSec = setTimeSec - nowTime;

				if( ( (ps_g_PVSYSINF->dayTimeSum+sabunSec) >= (10*60) ) ||
				    ( (ps_g_PVSYSINF->dayTimeSum+sabunSec) <= (-10*60) ) )
				{
					sprintf(errMsg, "  １日に変更できる時間の範囲を超えています。");
					result = 2;
				}
				else
				{
					ps_g_PVSYSINF->dayTimeSum += sabunSec;
					sprintf(command, "date -s \"%s\"", setTimeStr);
   					setuid( 0 );
   					system(command);
					sprintf(errMsg, "  時刻の変更に成功しました。");
					keyCnt = 0;
					memset(setTimeStr, 0, sizeof(setTimeStr));
					result = 1;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
		else
		{
			/* キー入力が確定の場合 */
			if( (inKeyData[0] == 'b') || (inKeyData[0] == 'B') )
			{
				if( keyCnt > 0 )
				{
					if( ( keyCnt == 5 ) ||
					    ( keyCnt == 8 ) ||
					    ( keyCnt == 11 ) ||
					    ( keyCnt == 14 ) ||
					    ( keyCnt == 17 ) )
					{
						keyCnt--;
					}

					keyCnt--;

					memset(&setTimeStr[keyCnt], 0, sizeof(setTimeStr)-keyCnt);
				}
			}
			/* キー入力が戻すの場合 */
			else if( (inKeyData[0] >= '0') && (inKeyData[0] <= '9') )
			{
				if( keyCnt <= 18 )
				{
					setTimeStr[keyCnt]=inKeyData[0];
					keyCnt++;

					if( keyCnt == 4)
					{
						if( (atoi(&setTimeStr[0]) < 1970) || ( atoi(&setTimeStr[0]) > 2100))
						{
							memset(setTimeStr, 0, sizeof(setTimeStr));
							keyCnt = 0;
						}
						else
						{
							setTime.tm_year = atoi(&setTimeStr[0]);
							setTimeStr[keyCnt]='/';
							keyCnt++;
						}
					}
					else if( keyCnt == 7 )
					{
						if( (atoi(&setTimeStr[5]) < 1) || ( atoi(&setTimeStr[5]) > 12))
						{
							memset(&setTimeStr[5], 0, 2);
							keyCnt -= 2;
						}
						else
						{
							setTime.tm_mon = atoi(&setTimeStr[5]);
							setTimeStr[keyCnt]='/';
							keyCnt++;
						}
					}
					else if( keyCnt == 10 )
					{
						if( (atoi(&setTimeStr[8]) < 1) || ( atoi(&setTimeStr[8]) > func_display_get_days(setTime.tm_year, setTime.tm_mon)))
						{
							memset(&setTimeStr[8], 0, 2);
							keyCnt -= 2;
						}
						else
						{
							setTime.tm_mday = atoi(&setTimeStr[8]);
							setTimeStr[keyCnt]=' ';
							keyCnt++;
						}
					}
					else if( keyCnt == 13)
					{
						if( (atoi(&setTimeStr[11]) < 0) || ( atoi(&setTimeStr[11]) > 23))
						{
							memset(&setTimeStr[11], 0, 2);
							keyCnt -= 2;
						}
						else
						{
							setTime.tm_hour = atoi(&setTimeStr[11]);
							setTimeStr[keyCnt]=':';
							keyCnt++;
						}
					}
					else if( keyCnt == 16)
					{
						if( (atoi(&setTimeStr[14]) < 0) || ( atoi(&setTimeStr[14]) > 59))
						{
							memset(&setTimeStr[14], 0, 2);
							keyCnt -= 2;
						}
						else
						{
							setTime.tm_min = atoi(&setTimeStr[14]);
							setTimeStr[keyCnt]=':';
							keyCnt++;
						}
					}
					else if( keyCnt == 19)
					{
						if( (atoi(&setTimeStr[17]) < 0) || ( atoi(&setTimeStr[17]) > 59))
						{
							memset(&setTimeStr[17], 0, 2);
							keyCnt -= 2;
						}
						else
						{
							setTime.tm_sec = atoi(&setTimeStr[17]);
						}
					}
				}
			}
			/* キー入力が戻すの場合 */
			else if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
			{
				memset(setTimeStr, 0, sizeof(setTimeStr));
				keyCnt = 0;
			}

			if( ( result == 2 ) && ( keyCnt <= 18 ) )
			{
				memset(errMsg, 0, sizeof(errMsg));
				result = 0;
			}
			else if( ( result == 1 ) && ( keyCnt > 0 ) )
			{
				memset(errMsg, 0, sizeof(errMsg));
				result = 0;
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 制御状態表示
 *
 * 機能：
 *		リストデータ取得
 *
 */
int	get_list_data( char *filePath, int *list, int dataMax )
{
	FILE					*fp;							/* ファイルディスクリプタ */
	char					readBuff[128];					/* リードバッファ */
	int						dataNum=0;
    int  					retCode=0;

	/* 設定ファイルが存在しない場合 */
	if( access( filePath, R_OK ) != 0 )
	{
		return -1;
	}

	/* CSVファイルオープン */
	if( (fp = fopen(filePath, "r")) != NULL)
	{
		memset(list, 0, (sizeof(int) * dataMax ));
		/* 設定ファイルの終端まで繰り返す */
		while( fgets( readBuff, 128, fp) != NULL )
		{
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;

			/* パラメータ確認 */
			list[dataNum] = atoi(readBuff);

			/* データ数をインクリメント */
			dataNum++;

			/* 上限に到達した場合 */
			if( dataNum == dataMax )
				break;
		}
		retCode = dataNum;
		/* ファイルをクローズ */
		fclose(fp);
	}
	else
	{
		retCode = -2;
	}
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 制御状態表示
 *
 * 機能：
 *		制御状態保存ファイルの状態表示を行う
 *
 */
void	func_display_emctlinfo()
{
	time_t				nowTime;		/* 現在時刻 */
	struct tm			localTime;		/* 現在時刻 */
	char				workStr[64];	/* ワーク */
	int					maxCtlCnt = 0;	/* 最大装置数 */
	int					skipCnt;		/* 一画面に表示する装置数*/
	int					offSet = 0;		/* オフセット */
	int					loopCnt1;		/* ループカウンタ */
	int					loopCnt2;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	int					ctlfil=0;		/* 制御管理情報位置 */
	int					dispMode = 0;
	long long			pow10Data;		/* べき乗データ */
	long long			spow10Data;		/* べき乗データ */
	long long			calcData;		/* 演算データ */
	struct st_CTLMNG	*ps_ctlmng;		/* 制御情報 */
	int					svBase;		/* 装置状態リンク情報（ベース値） */
	unsigned char		svMask;			/* 状態マスク値 */
	int					svVal;		/* 現在値 */
	char				tmStr[32];
	char				svStr[32];
	int					viewTM[64] = {	 PV_SYS_TM_AVE_RCV_POW	/* [20251112] Chg */
										,PV_SYS_TM_RCV_POW
										,PV_SYS_TM_AVE_LOAD_POW
										,PV_SYS_TM_LOAD_POW
										,PV_SYS_TM_PLAN_RCV_POW
										,PV_SYS_TM_RCV_POW_TARGET
										,PV_SYS_TM_BATTE_SOC
										,PV_SYS_TM_BT_REMAIN_WH
										,PV_SYS_TM_BT_REQ_POW
										,PV_SYS_TM_BT_SET_POW
										,PV_SYS_TM_BATTE_POW
										,PV_SYS_TM_PLAN_REMAIN
										,PV_SYS_TM_CHG_LIMIT
										,PV_SYS_TM_DISCHG_LIMIT
										,PV_SYS_TM_SCH_PER
										,PV_SYS_TM_SCH_CHG_TIME
/* [20250807] Add St */
										,PV_SYS_TM_FCRCTL_BASE_FREQ
										,PV_SYS_TM_FCRCTL_FREQ_VARIATION_RATE
										,PV_SYS_TM_FCRCTL_DEAD_BAND
										,PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD
										,PV_SYS_TM_FCRCTL_ERR_FAULT_DURATION
										,PV_SYS_TM_FCRCTL_RATED_POWER
										,PV_SYS_TM_FCRCTL_DEAD_BAND_RES_TYPE
										,PV_SYS_TM_FCRCTL_CONTRACT_DLT_POW
/* [20260130] Del */
/* [20250807] Add Ed */
	};

	int					viewSV[64] = {	 PV_SYS_SV_BT_ONGRID_STATE	/* [20251112] Chg */
										,PV_SYS_SV_ONGRID_BT_CHECK
										,PV_SYS_SV_BT_OFFGRID_STATE
										,PV_SYS_SV_OFFGRID_BT_CHECK
										,PV_SYS_SV_BT_OPERATION
										,PV_SYS_SV_ONGRID_PLAN
										,PV_SYS_SV_RCVPWR_CTL
										,PV_SYS_SV_DEMAND_CTL
										,PV_SYS_SV_OPE_SOC_CTL
										,PV_SYS_SV_OPE_SUPPLY_DEMAND_SOC
										,PV_SYS_SV_OPE_SUPPLY_DEMAND
										,PV_SYS_SV_OPE_CAPACITY_MARKET_SOC
										,PV_SYS_SV_OPE_CAPACITY_MARKET
										,PV_SYS_SV_OPE_CAPACITY_MARKET_EXAM
										,PV_SYS_SV_BCP_USE
/* [20250807] Add St */
										,PV_SYS_SV_FCR_CTL
										,PV_SYS_SV_FCR_CTL_FREQ_ERR
										,PV_SYS_SV_REPORT_DATA_ACUM
										,PV_SYS_SV_FREQ_SIM_TEST
/* [20250807] Add Ed */
										,PV_SYS_SV_ERR_CMMON
										,PV_SYS_SV_ERR_OFFGRID
										,PV_SYS_SV_ERR_ONGRID
	};
	char				checkFile[256];
	int					viewMax;

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末サイズを取得 */
	func_termsize_get();
	/* 端末サイズを保存 */
	termSize_OLD.ws_row = termSize.ws_row;
	viewMax = (termSize.ws_row - 20 );

	strcpy( checkFile, PV_PATH_CSV );
	strcat( checkFile, "SYSDEBUG_SV_LIST" );
	get_list_data(checkFile, viewSV, (sizeof(viewSV)/sizeof(int)));

	strcpy( checkFile, PV_PATH_CSV );
	strcat( checkFile, "SYSDEBUG_TM_LIST" );
	get_list_data(checkFile, viewTM, (sizeof(viewTM)/sizeof(int)));

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= EM制御管理情報表示 ===================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("■制御情報----------------------------------------------------------------------\n");
		printf("\033[0K");  printf("優先度\n");
		printf("\033[0K");  printf("  価格連動制御              : %d          ｜   需給調整市場              : %d\n", ps_g_CEMSPINF->arbitragePri, ps_g_CEMSPINF->balancePri);
		printf("\033[0K");  printf("  ピークカット              : %d          ｜   容量市場                  : %d\n", ps_g_CEMSPINF->peakCutPri, ps_g_CEMSPINF->capacityPri);
		printf("\033[0K");  printf("  BCP                       : %d          ｜   (容量市場優先度)          : %s\n", ps_g_CEMSPINF->bcpPri, (ps_g_EMCTLFIL->cmExamPri == 1 ) ? "実効性テスト優先" : "実需給優先");		/* [20240910] Chg */
		printf("\033[0K");	printf("制御開始カウントダウン      : %d.%d\n", ps_g_EMCTLFIL->actCntdown/10, ps_g_EMCTLFIL->actCntdown%10);
		printf("\033[0K");	printf("制御モード                  : %s\n", c_ctlModeName[ps_g_EMCTLFIL->cemsMode+1]);
		printf("\033[0K");  printf("バッテリ充放電制御          : %s", (ps_g_CEMSPINF->btControl == 1 ) ? "有効" : "無効");
		printf("       ｜   BCP用SOC (%)              : %d\n", ps_g_RCVPWINF->BCPSoc );
/* [20250828] Chg St */
		if( ps_g_BATTEINF != NULL )
		{
			printf("\033[0K");	printf("バッテリ充放電電力目標値(w) : %8d", ps_g_EMCTLFIL->bttReqPow );
			printf("   ｜   放電禁止                  : %d\n", ps_g_BATTEINF->socLower );
			printf("\033[0K");	printf("バッテリ充放電電力指示値(w) : %8d", ps_g_EMCTLFIL->bttSetPow );
			printf("   ｜   充電禁止                  : %d\n", ps_g_BATTEINF->socUpper );
			printf("\033[0K");	printf("バッテリ充電可能電力(w)     : %8d", ps_g_EMCTLFIL->avlChgPow );
			printf("   ｜   放電再開SoCオフセット (%) : %d\n", ps_g_RCVPWINF->dischgRestartSoC );
			printf("\033[0K");	printf("バッテリ放電可能電力(w)     : %8d", ps_g_EMCTLFIL->avlDischgPow );
			printf("   ｜   充電再開SoCオフセット (%) : %d\n", ps_g_RCVPWINF->chgRestartSoC );
		}
/* [20250828] Chg Ed */
/* [20230217] Add St */
		if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].machineCnt !=  0 )
		{
/* [20230217] Add Ed */
		printf("\033[0K");	printf("  PV出力制御目標値(w)       : %d\n", ps_g_EMCTLFIL->pvReqPow );
		printf("\033[0K");	printf("  PV出力制御指示値(w)       : %d\n", ps_g_EMCTLFIL->pvSetPow );
		printf("\033[0K");	printf("  PV出力制御指示値(%)       : %d.%02d\n", (ps_g_EMCTLFIL->pvSetPer/100), (ps_g_EMCTLFIL->pvSetPer%100) );
		printf("\033[0K");	printf("  PV定格出力(w)             : %d\n", ps_g_EMCTLFIL->pvRatedPow );
		printf("\033[0K");	printf("  PV発電抑制                : %s\n", (ps_g_EMCTLFIL->pvSprFlg == 1) ? "ON" : "OFF" );
/* [20230217] Add St */
		}
/* [20230217] Add Ed */

		printf("\033[0K");	printf("S1:停止タイマ開始時刻       : ");
		if( ps_g_EMCTLFIL->s1_timer_sec == 0 )
			printf("--\n");
		else
		{
			nowTime = ps_g_EMCTLFIL->s1_timer_sec;
			localtime_r( &nowTime, &localTime );
			printf("%04d/%02d/%02d %02d:%02d:%02d\n",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		}
		printf("\033[0K");	printf("     タイムアウト時刻       : ");
		if( ps_g_EMCTLFIL->s1_timer_sec == 0 )
			printf("--\n");
		else
		{
			nowTime = ps_g_EMCTLFIL->s1_timer_sec + (ps_g_CEMSPINF->shutdownWait*60);
			localtime_r( &nowTime, &localTime );
			printf("%04d/%02d/%02d %02d:%02d:%02d",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );

			nowTime = time( NULL );
			if( ( nowTime - ps_g_EMCTLFIL->s1_timer_sec ) > (ps_g_CEMSPINF->shutdownWait*60) )
				printf(" （タイムアウト）\n");
			else
				printf(" （%5d）\n",  (ps_g_CEMSPINF->shutdownWait*60) - ( nowTime - ps_g_EMCTLFIL->s1_timer_sec ));
		}

		printf("--------------------------------------------------------------------------------\n");
		printf("■制御元計測情報/状態情報-------------------------------------------------------\n");
		printf(" 計測名称_____________ FL:VAL_______ 単位_ | 状態名称___________________ 状態___\n");

		for( loopCnt1 = 0; loopCnt1 < viewMax; loopCnt1++ )
		{
			printf("\033[0K");
			if( viewTM[loopCnt1] == 0 )
			{
				func_display_cmn_strlenBlank(42, " ");
				printf(" %s", c_strData);
			}
			else
			{
				func_display_cmn_strlenBlank(20, (ps_g_TMDATINF + viewTM[loopCnt1]-1)->tmName);

				pow10Data = (long long)commfunc_pow10abs( (ps_g_TMDATINF + viewTM[loopCnt1]-1)->expData );
				spow10Data = (long long)commfunc_pow10abs( (ps_g_TMDATINF + viewTM[loopCnt1]-1)->expData + 3);
				calcData = (long long)((ps_g_TMSTSFIL + viewTM[loopCnt1]-1)->avltmVal);
				calcData *= (long long)((ps_g_TMDATINF + viewTM[loopCnt1]-1)->convData);
				calcData += (long long)((ps_g_TMDATINF + viewTM[loopCnt1]-1)->baseData);

				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");
	
				printf(" %s  %02X:", c_strData, (ps_g_TMSTSFIL + viewTM[loopCnt1]-1)->avltmFlag, (ps_g_TMSTSFIL + viewTM[loopCnt1]-1)->avltmVal );

				/* 演算結果を表示する */
				if( (ps_g_TMDATINF + viewTM[loopCnt1]-1)->expData >= 0 )
				{
					printf("%10lld ", calcData * pow10Data );
				}
				else
				{
					if( ((calcData / pow10Data) == 0) && ( calcData < 0 ) )
						printf("    -0." );
					else
						printf("%6lld.", calcData / pow10Data );
					if( calcData < 0 ) calcData *= -1;
					if( ((ps_g_TMDATINF + viewTM[loopCnt1]-1)->expData + 3) >= 0 )
					{
						printf("%03lld ", ( calcData * spow10Data ) % 1000 );
					}
					else
					{
						printf("%03lld ", ( calcData / spow10Data ) % 1000 );
					}
				}
				/* 計測単位を表示する */
				func_display_unit( (ps_g_TMDATINF + viewTM[loopCnt1]-1)->unitKind );
			}
			if( viewSV[loopCnt1] == 0 )
			{
				printf("| \n");
			}
			else
			{
				func_display_cmn_strlenBlank(26, (ps_g_SVDATINF + viewSV[loopCnt1]-1)->svName);
				printf("| %s  ", c_strData);
				/* 装置状態取得のためのマスク値を設定 */
				svMask = (unsigned char)(0x80 >> (((ps_g_SVDATINF + viewSV[loopCnt1]-1)->svNumb - 1) % 8));
				/* 装置状態の取得と表示 */
				if( (ps_g_SVSTSFIL + ((ps_g_SVDATINF + viewSV[loopCnt1]-1)->svNumb - 1) / 8 )->svStat & svMask )
					printf("[[ 1 ]] ");
				else
					printf("[[ 0 ]] ");
				printf("\n");
			}
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                                                                 (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * EMS管理制御情報
 *
 * 機能：
 *		EMS管理制御情報ファイルの表示を行う
 *
 */
void	func_display_emsorderinfo()
{
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm			localTime;		/* 現在時刻 */
	struct tm			startTime;		/* 開始時刻 */
	struct tm			endTime;		/* 終了時刻 */
	time_t				nowTime;		/* 現在時刻 */
	int					loopCnt1;		/* ループカウント */
	int					opePos;		/* ループカウント */
	int					skipCnt;		/* 一画面に表示する装置数*/
	int					offSet = 0;		/* オフセット */
	int					viewPos;		/* 表示位置 */
	struct tm			nowEndTime;		/* 終了時刻 */
	struct tm			nextEndTime;	/* 終了時刻 */
	struct st_ORDERPARA	*ps_orderpara;
	int					dispMode = 0;	/* 表示切替 */
	int					viewMode = 0;	/* 表示切替 */
	int					orderMode = 0;	/* 指令切替 *//* [20240701] Chg */
	struct st_ORDERPARA	*ps_order;
	struct st_evtdata	*ps_evtdata;
	struct st_setdata	*ps_setdata;
	struct st_resinfo	*ps_resInfo;
	

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );


	skipCnt = (termSize.ws_row - 17 );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <ACシステム指令情報表示> =============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );


		printf("\033[0K");	printf("\n");

		if( dispMode == 0 )
		{
/* [20240701] Chg St */
			printf("\033[0K");	printf(" ACシステム制御指令情報--------------------------------------------------------------\n");
			printf("\033[0K");	printf("\n");
			switch( orderMode )
			{
				/* 三次①，② */
				case 0 :
					printf("\033[0K");	printf("  需給調整市場(三次調整力①,②)\n");
					ps_order = ps_g_EMCTLFIL->s_rr_para;
					ps_evtdata =  &ps_g_EMCTLFIL->s_evtdata_rr;
					ps_setdata =  &ps_g_EMCTLFIL->s_setdata_rr;
					break;
/* [20250807] Chg St */
				/* 一次 */
				case 1 :
					printf("\033[0K");	printf("  需給調整市場(一次調整力)\n");
					ps_order = ps_g_EMCTLFIL->s_fcr_para;
					ps_evtdata =  &ps_g_EMCTLFIL->s_evtdata_fcr;
					ps_setdata =  &ps_g_EMCTLFIL->s_setdata_fcr;
					break;
				/* 容量市場(実効性テスト) */
				case 2 :
					printf("\033[0K");	printf("  容量市場(実効性テスト)\n");
					ps_order = ps_g_EMCTLFIL->s_cm_exam_para;
					ps_evtdata =  &ps_g_EMCTLFIL->s_evtdata_cm_exam;
					ps_setdata =  &ps_g_EMCTLFIL->s_setdata_cm_exam;
					break;
				/* 容量市場(実需給) */
				case 3 :
					printf("\033[0K");	printf("  容量市場(実需給)\n");
					ps_order = ps_g_EMCTLFIL->s_cm_para;
					ps_evtdata =  &ps_g_EMCTLFIL->s_evtdata_cm;
					ps_setdata =  &ps_g_EMCTLFIL->s_setdata_cm;
					break;
/* [20250807] Chg Ed */
			}
			printf("\033[0K");	printf("\n");
			if( opePos == AWS_ORDER_NOW )
			{
				printf("\033[0K");	printf("  ◎現在   %s\n", (ps_setdata->actMode == AWS_MODE_OPE) ? "【 運用中 】" : "【 待機中 】" );
			}
			else
			{
				printf("\033[0K");	printf("  〇ストック[%d]\n", opePos);
			}

			ps_orderpara = &ps_order[opePos];

			if( ps_orderpara->eventId[0] != 0 )
			{
				/* 年月日時分秒を取得 */
				localtime_r( &ps_orderpara->eventEdTimeSec.tv_sec, &nowEndTime );
				printf("\033[0K");	printf("    イベントＩＤ     ：%-22s | 指示種別        ：%s\n", ps_orderpara->eventId, ps_orderpara->orderStr );
				printf("\033[0K");	printf("    イベント開始時刻 ：%-22s | 継続時間(%3s)   ：%s\n", ps_orderpara->eventTimeStr, ps_orderpara->timeUnit, ps_orderpara->duration );
				printf("\033[0K");	printf("    商品区分         ：" );
				if( strcmp( ps_orderpara->categoryStr, AWS_CATEGORY_REPLACEMENT_RESERVE ) == 0 )
				{
					printf("三次調整力①");
				}
				else if( strcmp( ps_orderpara->categoryStr, AWS_CATEGORY_REPLACEMENT_RESERVE_FOR_FIT ) == 0 )
				{
					printf("三次調整力②");
				}
				else if( strcmp( ps_orderpara->categoryStr, AWS_CATEGORY_SYNC_FREQUENCY_RESTORATION ) == 0 )
				{
					printf("二次調整力①");
				}
				else if( strcmp( ps_orderpara->categoryStr, AWS_CATEGORY_FREQUENCY_CONTAINMENT ) == 0 )
				{
					printf("二次調整力②");
				}
				else if( strcmp( ps_orderpara->categoryStr, AWS_CATEGORY_FREQUENCY_RESTORATION ) == 0 )
				{
					printf("一次調整力");
				}
				else if( strcmp( ps_orderpara->categoryStr, AWS_CATEGORY_CAPACITYMARKET ) == 0 )
				{
					if( ps_orderpara->cm_exam == 1 )
					{
						printf("容量市場(実効性テスト)");
					}
					else
					{
						printf("容量市場(実需給)");
					}
				}
				printf("\n");
			}
			else
			{
				printf("\033[0K");	printf("    なし\n");
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("\n");
			}
			printf("\033[0K");	printf("    ----------------------------------------------------------------------------\n");
			printf("\033[0K");	printf("    インターバル情報------------------------------------------------------------\n");
			printf("\033[0K");	printf("             | 継続時間 |  基準値  |  指令値  |                  |\n");
			printf("\033[0K");	printf("        区分 |  (%3s)   | (%6s) | (%6s) | 開始時刻         | 終了時刻\n", ps_orderpara->timeUnit, ps_orderpara->valueUnit, ps_orderpara->valueUnit );

			for( loopCnt1 = 0; loopCnt1 < skipCnt; loopCnt1++ )
			{
				viewPos = ( loopCnt1 + offSet );

				if( viewPos >= AWS_INTERVAL_MAX )
					break;

				printf("\033[0K");
				if( ps_orderpara->s_interval[viewPos].statFlag == 0 )
				{
							     printf("        %4d | -------- | -------- | -------- |                  |\n", viewPos);
				}
				else
				{
					/* 年月日時分秒を取得 */
					nowTime = ps_orderpara->s_interval[viewPos].startSec.tv_sec;
					localtime_r( &nowTime, &startTime );
					nowTime = ps_orderpara->s_interval[viewPos].endSec.tv_sec;
					localtime_r( &nowTime, &endTime );


					/* 運用中、且つ、現在面、且つ、interval位置が一致している場合 */
					if( ( ps_setdata->actMode == AWS_MODE_OPE) && 
						( opePos == AWS_ORDER_NOW ) && 
						( ps_evtdata->intervalPos == viewPos ) )
						printf("    ●");
					else
						printf("      ");
					
					printf("  %4d | %6d.%d | %8d | %8d | %02d-%02d %02d:%02d:%02d.%d | %02d-%02d %02d:%02d:%02d.%d\n", 
							viewPos, ps_orderpara->s_interval[viewPos].duration.tv_sec,ps_orderpara->s_interval[viewPos].duration.tv_usec/100000,
							ps_orderpara->s_interval[viewPos].base, ps_orderpara->s_interval[viewPos].value,
							startTime.tm_mon + 1, startTime.tm_mday, startTime.tm_hour, startTime.tm_min, startTime.tm_sec,ps_orderpara->s_interval[viewPos].startSec.tv_usec/100000,
							endTime.tm_mon + 1, endTime.tm_mday, endTime.tm_hour, endTime.tm_min, endTime.tm_sec,ps_orderpara->s_interval[viewPos].endSec.tv_usec/100000 );
				
				}
			}
			printf("\033[0K");
			printf("================================================================================\n");
			printf("\033[0K");
			printf("戻る:[q] 頁切替:[前:c/次:v] 指令切替:[前:b/次:n] 商品切替:[m] 通知切替:[d]  (5秒更新)");
		}
		else
		{
			if( viewMode == 0 )
			{
				printf("\033[0K");	printf("通知情報--------------------------------------------------------------------------------\n");
				printf("\033[0K");	printf(" 番号 | 送信状態 | イベントID                     |  種別    | 応答結果 | 応答メッセージ\n");
				printf("\033[0K");	printf("------+----------+--------------------------------+----------+----------+---------------\n");
				
				for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
				{
					ps_resInfo = &ps_g_EMCTLFIL->s_resinfo[loopCnt1];

					printf("\033[0K");	printf(" [%2d] | ", ps_resInfo->resId );
					switch( ps_resInfo->sndstatus )
					{
						case AWS_NOTIFY_ST_NON :
							printf("未設定   |                                |          |          |\n");
							continue;
							break;
						case AWS_NOTIFY_ST_WAIT :
							printf("送信待ち |");
							break;
						case AWS_NOTIFY_ST_COMP :
							printf("送信完了 |");
							break;
						case AWS_NOTIFY_ST_FAIL :
							printf("送信失敗 |");
							break;
					}
					printf(" %-30s | ", ps_resInfo->eventId );
					switch( ps_resInfo->messageKind )
					{
						case AWS_NOTIFY_RES :
							printf("指令応答");
							break;
						case AWS_NOTIFY_NOTIFY_S :
							printf("開始通知");
							break;
						case AWS_NOTIFY_NOTIFY_E :
							printf("終了通知");
							break;
					}
					printf(" |");
					if( ps_resInfo->messageKind == AWS_NOTIFY_RES )
						printf(" %2d(%s) | %s", ps_resInfo->resResut, ((ps_resInfo->resResut == 0 ) ? "正常" : "異常"), ps_resInfo->errMsg );
					else
						printf(" -------- | ------------" );
						
					printf("\n");
				}
			}
			else
			{
				printf("\033[0K");	printf("通知情報(送信順序)--------------------------------------------------------------\n");
				printf("\033[0K");	printf(" 順序 | 番号 | 送信状態 | イベントID                     |  種別    | 送信回数 \n");
				printf("\033[0K");	printf("------+------+----------+--------------------------------+----------+----------\n");
				for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
				{
					ps_resInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]];
					printf("\033[0K");	printf(" [%2d] | [%2d] | ", loopCnt1 + 1, ps_resInfo->resId );

					switch( ps_resInfo->sndstatus )
					{
						case AWS_NOTIFY_ST_NON :
							printf("未設定   |                                |          |\n");
							continue;
							break;
						case AWS_NOTIFY_ST_WAIT :
							printf("送信待ち |");
							break;
						case AWS_NOTIFY_ST_COMP :
							printf("送信完了 |");
							break;
						case AWS_NOTIFY_ST_FAIL :
							printf("送信失敗 |");
							break;
					}
					printf(" %-30s | ", ps_resInfo->eventId );
					switch( ps_resInfo->messageKind )
					{
						case AWS_NOTIFY_RES :
							printf("指令応答");
							break;
						case AWS_NOTIFY_NOTIFY_S :
							printf("開始通知");
							break;
						case AWS_NOTIFY_NOTIFY_E :
							printf("終了通知");
							break;
						default:
							printf("        ");
							break;
					}
					printf(" | %3d / ", ps_resInfo->resSndCnt );
					if( ps_resInfo->retryCnt == 0 )
						printf("--");
					else
						printf("%2d", ps_resInfo->retryCnt );

					printf("\n");
				}
			}
			printf("\033[0K");
			printf("================================================================================\n");
			printf("\033[0K");
			printf("戻る:[q]             表示切替:[m]             指令切替：[d]            (5秒更新)");
		}

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;


		/* キー入力が表示切替２の場合 */
		if( (inKeyData[0] == 'm') || (inKeyData[0] == 'M') )
		{
			if( dispMode == 0 )
			{
				orderMode = (orderMode + 1 ) % 4;	/* [20251209] 一次調整力対応 Chg */
				offSet = 0;
				//opePos = 0;
			}
			else
			{
				viewMode = ( viewMode + 1 ) % 2;
			}
		}
/* [20240701] Chg Ed */

		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			if( dispMode == 0 )
			{
				if( ( offSet - skipCnt )  >= 0 )
				{
					offSet -= skipCnt;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}

		/* キー入力が表示切替の場合 */
		if( (inKeyData[0] == 'd') || (inKeyData[0] == 'D') )
		{
			viewMode = 0;
			dispMode = ( dispMode + 1 ) % 2;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'v') || (inKeyData[0] == 'V') )
		{
			if( dispMode == 0 )
			{
				if( ( offSet + skipCnt ) < AWS_INTERVAL_MAX )
				{
					offSet += skipCnt;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}

		/* キー入力が前の場合 */
		if( (inKeyData[0] == 'b') || (inKeyData[0] == 'B') )
		{
			if( dispMode == 0 )
			{
				if( ( opePos - 1 ) >= 0 )
				{
					opePos--;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'n') || (inKeyData[0] == 'N') )
		{
			if( dispMode == 0 )
			{
				if( ( opePos + 1 ) < AWS_ORDER_STOCK )
				{
					opePos++;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	return;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 制御状態表示
 *
 * 機能：
 *		制御状態保存ファイルの状態表示を行う
 *
 */
void	func_display_pvctlinfo()
{
	time_t				nowTime;		/* 現在時刻 */
	struct tm			localTime;		/* 現在時刻 */
	char				workStr[64];	/* ワーク */
	int					maxCtlCnt = 0;	/* 最大装置数 */
	int					skipCnt;		/* 一画面に表示する装置数*/
	int					offSet = 0;		/* オフセット */

	int					loopCnt1;		/* ループカウンタ */
	int					loopCnt2;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct st_PVCTLFIL	*ps_pvctlfil;	/* 制御管理情報ポインタ */
	int					ctlfil=PV_CTL_TYPE_CLOUD;		/* 制御管理情報位置 */
	int					dispMode = 0;
	int					exit = 0;
	struct st_CTLMNG	*ps_ctlmng;		/* 制御情報 */


	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末サイズを取得 */
	func_termsize_get();
	/* 端末サイズを保存 */
	termSize_OLD.ws_row = termSize.ws_row;

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 制御管理情報ポインタに設定 */
		ps_pvctlfil = &ps_g_PVCTLFIL[ctlfil];
		
		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= 制御状態表示 =========================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");

		printf("\033[0K");
		if( ctlfil == PV_CTL_TYPE_MODESW )
			printf("■ 状態遷移時処理 --------------------------------------------------------------\n");
		else
			printf("■ 遠隔制御指示   --------------------------------------------------------------\n");

		if( exit == 0 )
		{
			if( ps_pvctlfil->ctlState == PV_CTL_STATE_OFF )
			{
				printf("\033[0K");
			 	printf("処理状態  ： 未設定\n");
			}
			else
			{
			 	if( ctlfil == PV_CTL_TYPE_MODESW )
			 	{
					printf("\033[0K");
					printf("\n");
					printf("\033[0K");
				 	printf("モード遷移： %s\n", c_ctlModeName[ps_pvctlfil->ctlMode+1] );
				}
				else
				{
					printf("\033[0K");
					printf("ファイル名：%s\n", ps_pvctlfil->filename);
					printf("\033[0K");
				 	printf("制御種別  ： %s\n", c_switchModeName[ps_pvctlfil->ctlMode] );
				}
				
				printf("\033[0K");
			 	printf("処理状態  ： %s\n", ( ps_pvctlfil->ctlState == PV_CTL_STATE_EXE ) ? "実行中" : ( ps_pvctlfil->ctlState == PV_CTL_STATE_OFF ) ? "未設定" : ( ps_pvctlfil->ctlState == PV_CTL_STATE_FORCE ) ? "強制終了" : "終了" );
				printf("\033[0K");
			 	printf("開始時間  ： ");
				if( ps_pvctlfil->startTime.tv_sec == 0 )
				{
					printf("--:--:--.------");
										
				}
				else
				{
					localtime_r( &ps_pvctlfil->startTime.tv_sec, &localTime );
					printf("%02d:%02d:%02d.%06ld",
						localTime.tm_hour,
						localTime.tm_min,
						localTime.tm_sec,
						ps_pvctlfil->startTime.tv_usec);
				}
				printf("\n");
				printf("\033[0K");
			 	printf("ユーザ番号： %d\n", ps_pvctlfil->userNo);
				printf("\033[0K");
			 	printf("制御数    ： %d\n", ps_pvctlfil->ctlNum);
				printf("\033[0K");
				printf("--------------------------------------------------------------------------------\n");

				/* 制御数がある場合 */
				if( ps_pvctlfil->ctlNum > 0 )
				{
					if( dispMode == 0 )
					{
						printf("  | 実  処理  |     設定制御情報   |    OneShot制御情報 |      応動確認情報\n");
						printf("No| 行  状態  |   終了時刻    結果 |   終了時刻    結果 |   終了時刻    結果 \n");
						for( loopCnt1 = 0; loopCnt1 < ps_pvctlfil->ctlNum; loopCnt1++ )
						{
							ps_ctlmng = &ps_pvctlfil->s_ctlmng[loopCnt1];
							
							printf("%2d| ", loopCnt1+1 );
							if( loopCnt1 == ps_pvctlfil->ctlIdx )
							{
								printf("● ");
							}
							else
							{
								printf("   ");
							}
							printf("%s", (( ps_ctlmng->ctlProc == PV_CTL_PROC_WAIT ) ? "待機中 " : (( ps_ctlmng->ctlProc == PV_CTL_PROC_SET ) ? "制御中 " : (( ps_ctlmng->ctlProc == PV_CTL_PROC_ONE ) ? "OneShot" : (( ps_ctlmng->ctlProc == PV_CTL_PROC_RES ) ? "応動待 " : " 完了  ")))));

							for( loopCnt2 = 0; loopCnt2 < PV_CTL_PROC_RES; loopCnt2++ )
							{
								func_cmn_setTimeStr( TIME_STR_PTN_4, &ps_ctlmng->s_ctlrec[loopCnt2].retTime, workStr );
								workStr[13]=0;
								printf("| %s %s ", workStr, ((ps_ctlmng->s_ctlrec[loopCnt2].retTime.tv_sec == 0 ) ? "----" : ( ps_ctlmng->s_ctlrec[loopCnt2].retCode == 0 ) ? "正常" : "異常" ));
							}
							printf("\n");
						}
					}
					else
					{
						func_display_ctrlinfo( ps_pvctlfil->s_ctlinfo);
					}
				}
			}
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る:[q]                表示切替:[d]       モード切替:[m]                (5秒更新)");
		}
		else
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			/* 見出しの表示 */
			printf("\n");
			printf("\n");
			printf("\n");
			printf("               制御処理を強制終了しますか？\n");
			printf("\n");
			printf("\n");
			printf("\n");
			printf("================================================================================\n");
			printf("戻る:[q]                     実行:[y]                    キャンセル:[etc/5秒後]");
		
		}
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が表示切替の場合 */
		if( (inKeyData[0] == 'd') || (inKeyData[0] == 'D') )
		{
			dispMode = ( dispMode + 1 ) % 2;
			/* 画面をクリア */
			printf("\033[2J");
		}
		/* キー入力が表示切替の場合 */
		if( (inKeyData[0] == 'm') || (inKeyData[0] == 'M') )
		{
			ctlfil = ( ctlfil + 1 ) % PV_CTL_TYPE_MAX;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* キー入力がYESの場合 */
		if( (inKeyData[0] == 'y') || (inKeyData[0] == 'Y') )
		{
			/* 強制終了画面の場合 */
			if( exit == 1 )
			{
				/* 制御終了処理 */
				commfunc_pvctl_stop(ctlfil, ps_g_SVSTSFIL, ps_g_SVMSKINF );
			}
		}

		/* 強制終了画面の解除 */
		if( exit == 1 )
		{
			exit = 0;
		}

		/* キー入力が初期化の場合 */
		if( (inKeyData[0] == 'f') || (inKeyData[0] == 'F') )
		{
			/* 初期化画面 */
			exit = 1;
			/* 画面をクリア */
			printf("\033[2J");
			/* カーソルの位置を高さ０、横０に移動 */
			printf("\033[%d;%dH",0,0);
		}

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * スケジュール管理情報表示
 *
 * 機能：
 *		スケジュール管理情報保存ファイルの状態表示を行う
 *
 */
void	func_display_pvschinfo()
{
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	char				workStr[32];	/* ワーク */
	int					loopCnt1;		/* ループカウンタ */
	int					loopCnt2;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	int					dispMode = 1;	/* 表示モード */
	char				buffSelect;		/* s_schinfoの０面/１面 */
	int					dataPos;		/* 現在のデータ位置 */
	int					viewPos;		/* 表示のデータ位置 */
	int					month;			/* 月頭のデータ位置 */
	int					monDays;		/* 月の日数 */
	int					yearDays;		/* 月頭の合計日数 */
	struct st_schdata	*p_schdata;
	int					retCode;
	int					colNum=3;
	int					rowNum=PV_SCH_DAY_TI_NUM/colNum;
	int					printPos;
	int					schAcsTime[10] = {76200,78000,79800,81600,83400,85200,600,2400,4200,6000};
	int					ntpAcsTime[10] = {75600,77400,79200,81000,82800,84600,0,1800,3600,5400};
	int					timeSpanS;
	int					timeSpanE;
	int					temp;

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末サイズを取得 */
	func_termsize_get();
	/* 端末サイズを保存 */
	termSize_OLD.ws_row = termSize.ws_row;

	viewPos = (ps_g_SCHCTLFIL->dataPos - (ps_g_SCHCTLFIL->dataPos % PV_SCH_DAY_TI_NUM));
	buffSelect = ps_g_SCHCTLFIL->buffSelect;

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}


		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );


		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");


		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュール管理情報> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		if( dispMode == 0 )
		{
/* [20251027] Add St */
			if( ps_g_SCHCTLFIL->statFlag == 0 )
			{
				printf("\033[0K");	printf("未設定\n");
			}
			else
/* [20251027] Add Ed */
			{
				/* 見出しの表示 */
				printf("\033[0K");	printf("発電所情報 ---------------------------------------------------------------------\n");
				printf("\033[0K");	printf("  電力会社仕様                    ：%02d[%s]\n", ps_g_SCHCTLFIL->powerGrid, ( ps_g_SCHCTLFIL->powerGrid < PV_SCH_POWERGRID_MIN || ( ps_g_SCHCTLFIL->powerGrid > PV_SCH_POWERGRID_MAX )) ? "範囲外" : c_powerGridName[ps_g_SCHCTLFIL->powerGrid-1] );	/* [20210617] Add */
				printf("\033[0K");	printf("  発電所ＩＤ                      ：%s\n", ps_g_SCHCTLFIL->plantId);
				printf("\033[0K");	printf("  出力変化時間                    ：%d分\n", ps_g_SCHCTLFIL->chgTime);
				printf("\033[0K");	printf("  スケジュール未登録時の制御値    : %d\n", ps_g_SCHCTLFIL->schNonPer );	/* [20210330] Add */
				printf("\033[0K");	printf("ダウンロード情報 ---------------------------------------------------------------\n");
				printf("\033[0K");	printf("  ＩＤ登録状態                    ：%s\n", ((ps_g_SCHCTLFIL->idRegist == 0xFF) ? "未取得" : ((ps_g_SCHCTLFIL->idRegist == 0) ? "未登録" : ((ps_g_SCHCTLFIL->idRegist == 1) ? "登録済み" : "通信異常"))));
				printf("\033[0K");	printf("  設定値異常                      ：%s\n", (ps_g_SCHCTLFIL->configErr == 0) ? "未発生" : "発生");
				printf("\033[0K");	printf("  スケジュールサーバ間通信異常    ：%s\n", (ps_g_SCHCTLFIL->getSchErr == 0) ? "未発生" : "発生");
				printf("\033[0K");	printf("  要求スケジュール区分            ：%s\n", (ps_g_SCHCTLFIL->s_dwlinfo.kubun[0]==0 ? "----" : ps_g_SCHCTLFIL->s_dwlinfo.kubun));
				printf("\033[0K");	printf("  ＨＴＴＰステータスコード        ："); 
									if( ps_g_SCHCTLFIL->s_dwlinfo.comErrCode == 0 )
										printf("----\n");
									else
										printf("%d(%s)\n",ps_g_SCHCTLFIL->s_dwlinfo.comErrCode, ps_g_SCHCTLFIL->s_dwlinfo.comErrMsg);
				printf("\033[0K");	printf("  応答エラーコード                ：%s\n", (ps_g_SCHCTLFIL->s_dwlinfo.appErrCode[0]==0 ? "----" : ps_g_SCHCTLFIL->s_dwlinfo.appErrCode));
				printf("\033[0K");	printf("  応答エラーメッセージ            ：%s\n", (ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg[0]==0 ? "----" : ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg));
				printf("\033[0K");	printf("  ダウンロードファイル            ：%s\n", (ps_g_SCHCTLFIL->s_dwlinfo.fileName[0]==0 ? "----" : ps_g_SCHCTLFIL->s_dwlinfo.fileName));
				printf("\033[0K");	printf("  更新スケジュールリトライ時間    ：%d分\n", ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin);
				printf("\033[0K");	printf("  更新スケジュール次回アクセス日時：");
				if( ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime == 0 )
				{
					printf("未設定\n");
				}
				else
				{
					localtime_r( &ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime, &localTime );
					printf("%04d/%02d/%02d %02d:%02d:%02d\n",
							localTime.tm_year + 1900,
							localTime.tm_mon + 1,
							localTime.tm_mday,
							localTime.tm_hour,
							localTime.tm_min,
							localTime.tm_sec );
				}
				printf("\033[0K");	printf("  固定スケジュール更新フラグ(現)  ：");
				if( ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr == 0xFF )
				{
					printf("未設定\n");
				}
				else
				{
					printf("%d\n", ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr);
				}

				printf("\033[0K");	printf("                            (次)  ：");
				if( ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext == 0xFF )
				{
					printf("未設定\n");
				}
				else
				{
					printf("%d\n", ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext);
				}
				printf("\033[0K");	printf("  固定スケジュール(月間)取得情報  ：");
				if( ps_g_SCHCTLFIL->s_dwlinfo.getMon == 0 )
				{
					printf("未設定\n");
				}
				else
				{
					printf("20%02d/%02d\n", ps_g_SCHCTLFIL->s_dwlinfo.getMon/100, ps_g_SCHCTLFIL->s_dwlinfo.getMon%100);
				}
				timeSpanS = schAcsTime[ps_g_SCHCTLFIL->chkSumNumb];
				timeSpanE = timeSpanS + (20*60)-1;
				printf("\033[0K");	printf("  固定スケジュール要求時間帯      ：%02d:%02d:%02d～%02d:%02d:%02d\n", 
					timeSpanS/3600, (timeSpanS%3600)/60, (timeSpanS%3600)%60, timeSpanE/3600, (timeSpanE%3600)/60, (timeSpanE%3600)%60);

				timeSpanS = ntpAcsTime[ps_g_SCHCTLFIL->chkSumNumb];
				timeSpanE = timeSpanS + (10*60)-1;
				printf("\033[0K");	printf("  時刻同期実施時間帯              ：%02d:%02d:%02d～%02d:%02d:%02d\n", 
					timeSpanS/3600, (timeSpanS%3600)/60, (timeSpanS%3600)%60, timeSpanE/3600, (timeSpanE%3600)/60, (timeSpanE%3600)%60);
			}
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る:[q]  表示切替:[m]");
		}
		else if( dispMode == 1 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			/* 見出しの表示 */
			printf("スケジュール制御情報------------------------------------------------------------\n");


			retCode = func_display_pvschdays( buffSelect, viewPos, &month, &yearDays, &monDays );
			if( retCode < 0 )
			{
				printf("\033[0K");
				printf("  未取得\n");
			}
			else
			{
				printf("\033[0K");
				printf("日付：%04d/%02d/%02d\n", ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear, month, ((viewPos / PV_SCH_DAY_TI_NUM)-yearDays+1));
				printf("\033[0K");
				printf("    時刻 種別 指令値 sync |    時刻 種別 指令値 sync |    時刻 種別 指令値 sync\n");
				printf("\033[0K");
				printf("--------------------------+--------------------------+--------------------------\n");
				for( loopCnt1=0; loopCnt1 < rowNum; loopCnt1++)
				{
					for( loopCnt2=0; loopCnt2 < colNum; loopCnt2++ )
					{
						printPos = (viewPos+loopCnt1)+(loopCnt2*rowNum);
						p_schdata = &ps_g_SCHCTLFIL->s_schinfo[buffSelect].s_schdata[printPos];

						printf(" %s%02d:%02d", (((printPos==ps_g_SCHCTLFIL->dataPos)&&(buffSelect==ps_g_SCHCTLFIL->buffSelect)) ? "●" : "  "), 
								((loopCnt1+(loopCnt2*rowNum))/2), (((loopCnt1+(loopCnt2*rowNum))%2)*30));

						if( p_schdata->schType == PV_SCH_TYPE_NONE )
						{
							printf("  --  ------  %s ", (p_schdata->update==1 ? "(*)" : "   "));	/* [20251120] Chg */
						}
						else
						{
							printf("  %s  %-6d  %s ", (p_schdata->schType==PV_SCH_TYPE_UPDATE ? "更" : "固"), p_schdata->data, (p_schdata->update==1 ? "(*)" : "   "));
						}

						if( (loopCnt2 + 1) < colNum )
							printf("|");
					}
					printf("\n");
				}
			}
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る:[q] 当日:[t] 日切替:[前:c/次:v] 月切替:[前:b/次:n] 表示切替:[m]  (5秒更新)");
		}
		else if( dispMode == 2 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			/* 見出しの表示 */
			printf("スケジュール情報初期化----------------------------------------------------------\n");
			printf("\n");
			printf("\n");
			printf("               スケジュール情報を初期化しますか？\n");
			printf("\n");
			printf("\n");
			printf("================================================================================\n");
			printf("戻る:[q]                     実行:[y]                    キャンセル:[etc/5秒後]");
		}

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		/* キー入力が表示切替の場合は表示モードを切り替える */
		if( (inKeyData[0] == 'm') || (inKeyData[0] == 'M') )
		{
			if( dispMode != 2 )
			{
				dispMode = (dispMode + 1 ) % 2;
				/* 画面をクリア */
				printf("\033[2J");
			}
		}

		/* キー入力が前の場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			if( dispMode == 1 )
			{
				/* 前日が年の切り替わりを判定 */
				if( (viewPos - PV_SCH_DAY_TI_NUM) < 0 )
				{
					if( ps_g_SCHCTLFIL->s_schinfo[(buffSelect+1) % PV_SCH_BUFF_NUM].schYear > 2000 )
					{
						buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;
						viewPos = ((365 + ((ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear % 4) == 0 ? 1 : 0)) - 1) * PV_SCH_DAY_TI_NUM; 
					}
				}
				else
				{
					viewPos -= PV_SCH_DAY_TI_NUM;
				}
			}
			/* 画面をクリア */
			printf("\033[2J");
		}
		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'v') || (inKeyData[0] == 'V') )
		{
			if( dispMode == 1 )
			{
				/* 翌日が年の切り替わりを判定 */
				if(( month == 12 ) &&
				   ( (viewPos + PV_SCH_DAY_TI_NUM) >= ((yearDays+monDays) * PV_SCH_DAY_TI_NUM)))
				{
					if( ps_g_SCHCTLFIL->s_schinfo[(buffSelect+1) % PV_SCH_BUFF_NUM].schYear > 2000 )
					{
						buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;
						viewPos = 0; 
					}
				}
				else
				{
					viewPos += PV_SCH_DAY_TI_NUM;
				}
			}
			/* 画面をクリア */
			printf("\033[2J");
		}
		
		/* キー入力が前の場合 */
		if( (inKeyData[0] == 'b') || (inKeyData[0] == 'B') )
		{
			if( dispMode == 1 )
			{
				/* 月の頭ではない場合 */
				if( viewPos != ( yearDays * PV_SCH_DAY_TI_NUM ))
				{
					/* 月の頭に移動する */
					viewPos = yearDays * PV_SCH_DAY_TI_NUM;
				}
				else
				{
					/* 1月の場合 */
					if( month == 1 )
					{
						if( ps_g_SCHCTLFIL->s_schinfo[(buffSelect+1) % PV_SCH_BUFF_NUM].schYear > 2000 )
						{
							buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;
							viewPos = ((365 + ((ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear % 4) == 0 ? 1 : 0)) - 31) * PV_SCH_DAY_TI_NUM; 
						}
					}
					else
					{
						viewPos = (yearDays - func_display_get_days(ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear, ((month+10)%12+1) )) * PV_SCH_DAY_TI_NUM;
					}
				}
				/* 翌日が年の切り替わりか */
				/* データ位置を更新(月から算出)-？？ */
				
			}
			/* 画面をクリア */
			printf("\033[2J");
		}
		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'n') || (inKeyData[0] == 'N') )
		{
			if( dispMode == 1 )
			{
				/* 12月の場合 */
				if( month == 12 )
				{
					if( ps_g_SCHCTLFIL->s_schinfo[(buffSelect+1) % PV_SCH_BUFF_NUM].schYear > 2000 )
					{
						buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;
						viewPos = 0; 
					}
					else
					{
						viewPos = (yearDays+monDays-1) * PV_SCH_DAY_TI_NUM;
					}
				}
				else
				{
					viewPos = (yearDays+monDays) * PV_SCH_DAY_TI_NUM;
				}
				/* 翌日が年の切り替わりか */
				/* データ位置を更新(月から算出)+？？ */
			}
			/* 画面をクリア */
			printf("\033[2J");
		}
		/* キー入力が当日の場合 */
		if( (inKeyData[0] == 't') || (inKeyData[0] == 'T') )
		{
			if( dispMode == 1 )
			{
				/* 現在時刻に戻す */
				viewPos = (ps_g_SCHCTLFIL->dataPos - (ps_g_SCHCTLFIL->dataPos % PV_SCH_DAY_TI_NUM));
				buffSelect = ps_g_SCHCTLFIL->buffSelect;
			}
		
		}

		/* キー入力がYESの場合 */
		if( (inKeyData[0] == 'y') || (inKeyData[0] == 'Y') )
		{
			/* 初期化画面の場合 */
			if( dispMode == 2 )
			{
				/* 設定値異常を解除する。 */
				ps_g_SCHCTLFIL->configErr = 0;
				/* スケジュール取得異常 */
				ps_g_SCHCTLFIL->getSchErr = 0;

				/* ダウンロード情報を初期化する。 */
				memset(&ps_g_SCHCTLFIL->s_dwlinfo, 0, sizeof(struct st_dwlinfo));
				ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = 0xFF;
				ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = 0xFF;
				ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = 5;	/* PV_UP_SCH_RTY_MIN_1STではなく直値なので注意 */

				/* スケジュール情報を初期化する。 */
				memset( &ps_g_SCHCTLFIL->s_schinfo, 0x00, sizeof(struct st_schinfo)*PV_SCH_BUFF_NUM);
				ps_g_SCHCTLFIL->buffSelect = 0;
				ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear = localTime.tm_year + 1900;
			}
		}

		/* 初期化画面の解除 */
		if( dispMode == 2 )
		{
			dispMode = 0;
		}

		/* キー入力が初期化の場合 */
		if( (inKeyData[0] == 'i') || (inKeyData[0] == 'I') )
		{
			/* 初期化画面 */
			dispMode = 2;
			/* 画面をクリア */
			printf("\033[2J");
			/* カーソルの位置を高さ０、横０に移動 */
			printf("\033[%d;%dH",0,0);
		}

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	

}

/* [20210128] Add St */
int	func_cmn_srchRcvPowPos( int gridIdx, int schDay )
{
	int			loopCnt1;
	int			retCode=-1;

	for( loopCnt1 = 0; loopCnt1 < ps_g_PWPLNFIL->powplndata[gridIdx].plnDayNum; loopCnt1++ )
	{
		if( schDay == ps_g_PWPLNFIL->powplndata[gridIdx].s_powDayPln[loopCnt1].plnDay )
		{
			retCode = loopCnt1;
			break;
		}
	}

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 受電電力計画値表示
 *
 * 機能：
 *		受電電力計画値保存ファイルの表示を行う
 *
 */
void	func_display_rcvpowplninfo()
{
	char					workStr[32];	/* ワーク */
	int						offSet = 0;		/* オフセット */
	int						rmtNo = 0;		/* スケジュール番号 */
	int						outLimit;		/* 表示可能行数 */
	int						noOut=6;		/* 表示不可行数 */
	int						maxPos;			/* 最大情報数 */
	int						loopCnt1;		/* ループカウンタ */
	int						loopCnt2;		/* ループカウンタ */
	char					inKeyData[2];	/* キー入力値 */
	char					readByte;		/* キー入力バイト数 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;	/* 端末サイズ保存 */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	struct st_powDayPln		powDayPln[PV_POWPLN_MAX_DAY]; /* 受電電力計画値(内部表示用) */
	int						srchDay;			/* 現在日 */
	int						rcvPowPos;		/* 受電電力計画該当位置 */
	time_t					srchDayTime;	/* 検索日時刻 */
	int						viewDay=0;		/* 表示日 */
	struct st_powDayPln		*ps_checkPowPln;
	int						viewPos=0;		/* 表示コマ */
	int						dispMode = 0;	/* 表示モード */
	int						gridMode = PV_POWPLAN_IDX_ONGRID;	/* 系統切替モード */
	int						dayPage=2;
	int						rowNum=48/dayPage;
	int						todayNum;
	int						nowPos;

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <需給計画> ===========================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
		localTime.tm_year + 1900,
		localTime.tm_mon + 1,
		localTime.tm_mday,
		localTime.tm_hour,
		localTime.tm_min,
		localTime.tm_sec );

		/* 現在日の数値、現在コマを算出 */
		todayNum = ((localTime.tm_year + 1900) * 10000) + (localTime.tm_mon + 1) *100 + localTime.tm_mday;
		nowPos = (localTime.tm_hour*2) + (localTime.tm_min/30);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		/* 見出しの表示 */
		if( ps_g_PWPLNFIL->powplndata[gridMode].plnDayNum <= 0 )
		{
			printf("需給計画------------------------------------------------------------------------\n");
			printf("\033[0K");
			printf("未設定\n");
		}
		else
		{
			printf("需給計画値(登録日数：%2d)-------------------------------------------------------\n", ps_g_PWPLNFIL->powplndata[gridMode].plnDayNum);
			ps_checkPowPln = &ps_g_PWPLNFIL->powplndata[gridMode].s_powDayPln[viewDay];
			printf("\033[0K");
			printf("日付：%04d/%02d/%02d\n", (ps_checkPowPln->plnDay/10000), ((ps_checkPowPln->plnDay/100)%100), (ps_checkPowPln->plnDay%100));
			printf("\033[0K");
/* [20251218] Chg St */
			if( dispMode == 0 )
			{
				printf("     時刻  計画 充放電電力 充電上限 放電上限 BCP使用 PeakCut\n");

			}
			else if( dispMode == 1 )
			{
/* [20251208] Chg St */
				printf("     時刻  受電目標値 残電力量( SoC)  ***商品区分*** 必要電力量( SoC) 約定△kW\n");
/* [20251208] Chg Ed */
			}
			printf("\033[0K");
			printf("-------------------------------------------------------------------------------\n");

			for( loopCnt1=viewPos; loopCnt1 < viewPos+rowNum; loopCnt1++)
			{
				/* 現在コマの場合 */
				if( ( ps_checkPowPln->plnDay == todayNum ) &&
					( nowPos == loopCnt1 ) )
				{
					printf(" ●");
				}
				else
				{
					printf("   ");
				}

				if( ps_checkPowPln->plnStat[loopCnt1] == 1 )
				{
					printf(" %02d:%02d  ", (loopCnt1/2), ((loopCnt1%2)*30));
				}
				else
				{
					printf("(%02d:%02d) ", (loopCnt1/2), ((loopCnt1%2)*30));
				}
				if( dispMode == 0 )
				{
					printf(" %s  ", (ps_checkPowPln->plnStat[loopCnt1] == 1) ? "○" : "×");
					printf("%10d ", ps_checkPowPln->bttPow[loopCnt1]);
					printf("%8d ", ps_checkPowPln->avlChgPow[loopCnt1]);
					printf("%8d ", ps_checkPowPln->avlDischgPow[loopCnt1]);
					printf("%s ", (ps_checkPowPln->useBcp[loopCnt1] == 1) ? "  可   " : " 不可  ");
					printf("%s ", (ps_checkPowPln->peakCutStat[loopCnt1] == 1) ? " 有効" : " 無効");
				}
				else if( dispMode == 1 )
				{
					printf("%10d ", ps_checkPowPln->rcvPow[loopCnt1]);
					printf("%8d", ps_checkPowPln->bttSoC[loopCnt1]);
					printf("(%2d.%d) ", ((unsigned long)ps_checkPowPln->bttSoC[loopCnt1] * 100 / ps_g_BATTEINF[0].batteCap), (((unsigned long)ps_checkPowPln->bttSoC[loopCnt1] * 1000 / ps_g_BATTEINF[0].batteCap)%10));
					printf(" ");
					if( ps_checkPowPln->acUse[loopCnt1] == 1)
					{
						printf("三次");
					}
					else
					{
						printf("----");
					}
					printf("/");
					if( ps_checkPowPln->capMktUse[loopCnt1] == 1)
					{
						printf("容量");
					}
					else
					{
						printf("----");
					}
					printf("/");
					if( ps_checkPowPln->fcrUse[loopCnt1] == 1)
					{
						printf("一次");
					}
					else
					{
						printf("----");
					}
					printf(" ");
					if( ps_checkPowPln->acSoC[loopCnt1] != 0 )
					{
						printf("%10d", ps_checkPowPln->acSoC[loopCnt1]);
						printf("(%2d.%d)", ((unsigned long)ps_checkPowPln->acSoC[loopCnt1] * 100 / ps_g_BATTEINF[0].batteCap), (((unsigned long)ps_checkPowPln->acSoC[loopCnt1] * 1000 / ps_g_BATTEINF[0].batteCap)%10));
					}
					else
					{
						printf("----------(--.-)");
					}
/* [20251208] Add St */
					printf(" %8d ", ps_checkPowPln->fcrDltKw[loopCnt1]);									/* 約定△kW */
/* [20251208] Add Ed */
				}
				printf("\n");
			}
/* [20251218] Chg Ed */
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("戻る:[q]                  日切替:[前:c/次:v]        表示切替:[d]      (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;



		/* キー入力が表示切替の場合は表示モードを切り替える */
		if( (inKeyData[0] == 'd') || (inKeyData[0] == 'D') )
		{
			if( dispMode != 2 )
			{
				dispMode = (dispMode + 1 ) % 2;
				/* 画面をクリア */
				printf("\033[2J");
			}
		}

		/* キー入力が前の場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			/* 1日の最初に到達した場合 */
			if( viewPos == 0 )
			{
				/* 登録の初日でない場合 */
				if( viewDay > 0 )
				{
					viewDay--;
					viewPos = 48 - rowNum;
				}
			}
			else
			{
				viewPos -= rowNum;
			}
			/* 画面をクリア */
			printf("\033[2J");
		}
		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'v') || (inKeyData[0] == 'V') )
		{
			/* 1日の最後に到達した場合 */
			if( ( viewPos + rowNum ) == 48 )
			{
				/* 登録の最終日でない場合 */
				if( (viewDay+1) < ps_g_PWPLNFIL->powplndata[gridMode].plnDayNum )
				{
					viewDay++;
					viewPos = 0;
				}
			}
			else
			{
				viewPos += rowNum;
			}
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* キー入力が当日の場合 */
		if( (inKeyData[0] == 't') || (inKeyData[0] == 'T') )
		{
			viewDay = 0;
			viewPos = 0;
		}

	}
	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}
/* [20210128] Add Ed */

void	func_display_pvctldmdinfo()
{
	char				workStr[32];	/* ワーク */
	int					offSet = 0;		/* オフセット */
	int					dmdNo = 0;		/* デマンド監視番号 */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=19;		/* 表示不可行数 */
	int					maxPos;			/* 最大情報数 */
	int						tmifLink;		/* 計測情報リンクポインタ */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	int					dispMode = 0x00;	/* 表示モード */

	/* 表示情報の最大数を設定 */
	maxPos = PV_MAX_DMD_THAD_LV;


	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = 50;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <デマンド制御情報> ===================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		/* 読み込みフラグがOFFの場合 */
		if( ps_g_DEMNDINF->statFlag != 1 )
		{
				printf("\033[0K");
				printf("デマンド情報表示 ------------------------------------------------------------\n");
				printf("  監視状態      ：未設定\n");
				printf("  計測異常      ：------\n");
				printf("  監視開始時刻  ：-- 分\n");
				printf("  演算単位      ：------, 演算乗率      ：--\n");
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");
				printf("================================================================================\n");
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");
				printf("戻る:[q]                                                              (5秒更新)");
		}
		else
		{
#if 0
			
			printf("\033[0K");
			printf("デマンド情報表示 ------------------------------------------------------------\n");
			printf("  監視状態      ：%s\n", (ps_g_DEMNDFIL->moniFlg == 1 ? "動作中" : "停止中"));
			printf("\033[0K");
			printf("  計測異常      ：%s\n", (ps_g_DEMNDFIL->tmAbnFlag == 1 ? "発生" : "未発生"));
			printf("  監視開始時刻  ：%2d 分\n", ps_g_DEMNDINF->startUpTime);
			printf("  演算単位      ：");	func_display_unit( ps_g_DEMNDINF->unitKind );
			printf(", 演算乗率      ：%2d\n", ps_g_DEMNDINF->expData);
			printf("--------------------------------------------------------------------------------\n");
			printf(" デマンド計測情報 --------------------------------------------------------------\n");
			printf("    装 装 計測 計測 計測   乗 デマンド演算時                 名称\n");
			printf("    種,番,番号 種別 単位__ 率 FL:VALUE_____ (演算結果______) ________________\n");
			/* 装置種別に合わせて計測情報のリンクポインタ値を取得する */
			tmifLink = ps_g_PVSYSINF->s_machineInfo[ ps_g_DEMNDINF->soKind ].tmIfLink + 
						ps_g_PVSYSINF->s_machineInfo[ ps_g_DEMNDINF->soKind ].maxTm * (ps_g_DEMNDINF->soNumb-1) + (ps_g_DEMNDINF->tmNumb-1);

			printf("時限[-,--,---] ---- ------ -- --:%10d (%14.3f) デマンド時限計測値\n", ps_g_DEMNDFIL->tmValPow0, 
							func_cmn_ConvEnzan(ps_g_DEMNDFIL->tmValPow0, (ps_g_TMDATINF + tmifLink)->expData, (ps_g_TMDATINF + tmifLink)->convData, (ps_g_TMDATINF + tmifLink)->baseData));
			printf("補正[-,--,---] ---- ------ -- --:%10d (%14.3f) 監視電力加算値\n", ps_g_DEMNDFIL->tmValOffset, 
							func_cmn_ConvEnzan(ps_g_DEMNDFIL->tmValOffset, (ps_g_TMDATINF + tmifLink)->expData, (ps_g_TMDATINF + tmifLink)->convData, (ps_g_TMDATINF + tmifLink)->baseData));
			/* 監視対象の現在計測情報 */
			printf("監視[%d,%02d,%03d] %s ",
					ps_g_DEMNDINF->soKind,
					ps_g_DEMNDINF->soNumb,
					ps_g_DEMNDINF->tmNumb,
					c_tmKindName[ (ps_g_TMDATINF + tmifLink)->tmKind ] );
				func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );
				printf(" %2d %02X:%10d (%14.3f) %s\n",
					(ps_g_TMDATINF + tmifLink)->expData,
					ps_g_TMSTSFIL[tmifLink].tmFlag,
					ps_g_TMSTSFIL[tmifLink].tmVal,
					func_cmn_ConvEnzan(ps_g_TMSTSFIL[tmifLink].tmVal, (ps_g_TMDATINF + tmifLink)->expData, (ps_g_TMDATINF + tmifLink)->convData, (ps_g_TMDATINF + tmifLink)->baseData),
					(ps_g_TMDATINF + tmifLink)->tmName );
			/* 現在デマンド電力 */
			tmifLink = PV_SYS_TM_DMD_CUR_POW -1;
			printf("現在[%d,%02d,%03d] %s ", PV_MKIND_PVSYS, 1, PV_SYS_TM_DMD_CUR_POW, 
					c_tmKindName[ (ps_g_TMDATINF + tmifLink)->tmKind ]);
			func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );
				printf(" %2d %02X:%10d (%14.3f) %s\n",
					(ps_g_TMDATINF + tmifLink)->expData,
					ps_g_TMSTSFIL[tmifLink].tmFlag,
					ps_g_TMSTSFIL[tmifLink].tmVal,
					func_cmn_ConvEnzan(ps_g_TMSTSFIL[tmifLink].tmVal, (ps_g_TMDATINF + tmifLink)->expData, (ps_g_TMDATINF + tmifLink)->convData, (ps_g_TMDATINF + tmifLink)->baseData),
					(ps_g_TMDATINF + tmifLink)->tmName );

			/* 予測デマンド電力 */
			tmifLink = PV_SYS_TM_DMD_EST_POW - 1;
			printf("予測[%d,%02d,%03d] %s ", PV_MKIND_PVSYS, 1, PV_SYS_TM_DMD_EST_POW, 
					c_tmKindName[ (ps_g_TMDATINF + tmifLink)->tmKind ]);
			func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );
				printf(" %2d %02X:%10d (%14.3f) %s\n",
					(ps_g_TMDATINF + tmifLink)->expData,
					ps_g_TMSTSFIL[tmifLink].tmFlag,
					ps_g_TMSTSFIL[tmifLink].tmVal,
					func_cmn_ConvEnzan(ps_g_TMSTSFIL[tmifLink].tmVal, (ps_g_TMDATINF + tmifLink)->expData, (ps_g_TMDATINF + tmifLink)->convData, (ps_g_TMDATINF + tmifLink)->baseData),
					(ps_g_TMDATINF + tmifLink)->tmName );

			/* 予測デマンド電力最大 */
			tmifLink = PV_SYS_TM_DMD_EST_POW_MAX - 1;
			printf("最大[%d,%02d,%03d] %s ", PV_MKIND_PVSYS, 1, PV_SYS_TM_DMD_EST_POW_MAX, 
					c_tmKindName[ (ps_g_TMDATINF + tmifLink)->tmKind ]);
			func_display_unit( (ps_g_TMDATINF + tmifLink)->unitKind );
				printf(" %2d %02X:%10d (%14.3f) %s\n",
					(ps_g_TMDATINF + tmifLink)->expData,
					ps_g_TMSTSFIL[tmifLink].tmFlag,
					ps_g_TMSTSFIL[tmifLink].tmVal,
					func_cmn_ConvEnzan(ps_g_TMSTSFIL[tmifLink].tmVal, (ps_g_TMDATINF + tmifLink)->expData, (ps_g_TMDATINF + tmifLink)->convData, (ps_g_TMDATINF + tmifLink)->baseData),
					(ps_g_TMDATINF + tmifLink)->tmName );

			printf("--------------------------------------------------------------------------------\n");
			printf("デマンド閾値情報 --------------------------------------------------------------\n");

			/* 見出し２を表示 */
			printf("\033[0K");	printf(" No  閾値(W)___ | 監視名称______________  超過 | 監視名称______________  超過\n");
			/* 制御設備情報を画面に表示可能な行数分表示する */
			for( loopCnt = 0; loopCnt < PV_MAX_DMD_THAD_LV; loopCnt++ )
			{
				if( ps_g_DEMNDINF->s_thresholdInf[loopCnt].statFlag == 1 )
				{
					func_display_cmn_strlenBlank( 22, ps_g_SVDATINF[PV_SYS_SV_DMD_CUR_LV1 + loopCnt -1].svName );

					printf("\033[0K");
					printf(" %02d  %10d | %s   %s  |",
							loopCnt+1,
							ps_g_DEMNDINF->s_thresholdInf[loopCnt].thresholdVal,
							c_strData,
							(ps_g_DEMNDFIL->curLvOverFlg[loopCnt] == 0) ? "無" : "有");

					func_display_cmn_strlenBlank( 22, ps_g_SVDATINF[PV_SYS_SV_DMD_EST_LV1 + loopCnt -1].svName );
					printf(" %s   %s    \n",
							c_strData,
							(ps_g_DEMNDFIL->estLvOverFlg[loopCnt] == 0) ? "無" : "有");
				}
				else
				{
					printf("\033[0K");
					printf(" %02d  ---------- | ----------------------  ---- | ----------------------  ---- \n",loopCnt + 1 );
				}
			}
#endif
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る:[q]                頁切替:[前:c / 次:v]                          (5秒更新)");
		}

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;
#if 0
		/* キー入力が表示切替の場合は表示モードを切り替える */
		if( (inKeyData[0] == 'm') || (inKeyData[0] == 'M') )
		{
			dispMode = (dispMode + 1 ) % 2;
			if( dispMode == 0 )
			{
				noOut = 19;
				maxPos = PV_MAX_DMD_THAD_LV;
				
			}
			else
			{
				noOut = 13;
				maxPos = PV_MAX_CTRL_INFO;
			}
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'c') || (inKeyData[0] == 'C') )
		{
			if( ( offSet - outLimit )  >= 0 )
			{
				offSet -= outLimit;
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* 画面をクリア */
			printf("\033[2J");
		}
		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'v') || (inKeyData[0] == 'V') )
		{
			if( ( offSet + outLimit ) < maxPos )
			{
				offSet += outLimit;
				/* 画面をクリア */
				printf("\033[2J");
			}
		}
		/* キー入力が前の場合 */
		if( (inKeyData[0] == 'b') || (inKeyData[0] == 'B') )
		{
			if( dispMode == 1 )
			{
				dmdNo = (dmdNo + PV_MAX_DMD_THAD_LV -1) % PV_MAX_DMD_THAD_LV;
				/* 画面をクリア */
				printf("\033[2J");
			
			}
		}
		/* キー入力が次の場合 */
		if( (inKeyData[0] == 'n') || (inKeyData[0] == 'N') )
		{
			if( dispMode == 1 )
			{
				dmdNo = (dmdNo + 1) % PV_MAX_DMD_THAD_LV;
				/* 画面をクリア */
				printf("\033[2J");
			
			}
		}
#endif
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 画面サイズ設定
 *
 * 機能：
 *		端末のサイズ（桁、行）を設定する。
 *
 */
void	func_displaysize_set()
{
	char					keyData[1024];
	int						keyByte;
	int						menuNumb;
	struct winsize			termSize_OLD;	/* 端末サイズ保存 */

	/* ターミナルサイズを保存 */
	termSize_OLD.ws_col = termSize.ws_col;
	termSize_OLD.ws_row = termSize.ws_row;

	while( 1 )
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("= 画面サイズ設定 ===============================================================\n");
	
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("現在の設定：[%03d]桁 ｘ [%03d]行\n", termSize.ws_col, termSize.ws_row );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("現在使用している端末の画面サイズにあわせて 桁数 と 行数 を設定して下さい。\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("実際の端末サイズより大きい値を設定した場合は画面表示が乱れます。\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("（桁数は 79 以下、行数は 23 以下の値は指定できません）\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("桁数を入力してください（改行のみの場合は変更しない）[%03d] >> ", termSize.ws_col );
		fflush( NULL );

		if( signal_flg ) break;

		memset( keyData, 0, sizeof( keyData ) );
		keyByte = read( 0, &keyData, 1024 );

		if(keyByte < 0 )
			/* ツールを終了 */
			return;

		/* 入力値を数値化 */
		menuNumb = atoi( keyData );

		if( ( menuNumb >= 80 ) && ( menuNumb <= 256 ) )termSize.ws_col = menuNumb;
		
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("行数を入力してください（改行のみの場合は変更しない）[%03d] >> ", termSize.ws_row );
		fflush( NULL );

		if( signal_flg ) break;

		memset( keyData, 0, sizeof( keyData ) );
		keyByte = read( 0, &keyData, 1024 );

		if(keyByte < 0 )
			/* ツールを終了 */
			return;

		/* 入力値を数値化 */
		menuNumb = atoi( keyData );
		/* 行数が24以上の値の場合は更新する */
		if( ( menuNumb >= 24 ) && ( menuNumb <= 256 ) ) termSize.ws_row = menuNumb;

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("--------------------------------------------------------------------------------\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("画面サイズを [%03d]桁 x [%03d]行 に変更しますか？ (y/Y/n/N) >> ", termSize.ws_col, termSize.ws_row );
		fflush( NULL );

		if( signal_flg ) break;

		memset( keyData, 0, sizeof( keyData ) );
		keyByte = read( 0, &keyData, 1024 );

		if(keyByte < 0 )
			/* ツールを終了 */
			return;

		/* 入力がＹＥＳの場合 */
		if( ( keyData[0] == 'y' ) || ( keyData[0] == 'Y' ) )
		{
			/* ターミナルサイズを変更 */
			ioctl(STDOUT_FILENO, TIOCSWINSZ, &termSize );

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("画面サイズを [%03d]桁 x [%03d]行 に変更します。\n", termSize.ws_col, termSize.ws_row );
			sleep( 2 );
		}
		else
		{
			termSize.ws_col = termSize_OLD.ws_col;
			termSize.ws_row = termSize_OLD.ws_row;
		}

		break;
	}
}

void func_mesasge_view_wait()
{
	char					inKeyData[2];	/* キー入力値 */

	fflush(NULL);
	memset( inKeyData, 0, sizeof(inKeyData));
	read( 0, inKeyData, 1 );
}

void *func_tmlog_thread( void *args )
{

	FILE					*fp;						/* ファイルディスクリプタ */
	int						dirPos;
	int						loopCnt;
	struct tm				localTime;		/* 現在時刻 */
	char					writeData[1024];			/* ファイルパス */
	struct timeval			nowTime;					/* 現在時刻 */
	struct timespec			sleepTm;				/* nanosleep用 */
	long					waitTime_us;
	struct tm				timeData;


	/* 現在時刻を取得 */
	gettimeofday( &nowTime, NULL );
	/* 年月日時分秒を取得 */
	localtime_r( &nowTime.tv_sec, &localTime );

	/* 出力先ディレクトリがない場合 */
	if( s_tmlogInfo.outputDir[0]==0 )
	{
		for( loopCnt=0; loopCnt<strlen(s_tmlogInfo.inputFilePath); loopCnt++)
		{
			if( s_tmlogInfo.inputFilePath[loopCnt] == '/' )
				dirPos = loopCnt+1;
		}
		memcpy(s_tmlogInfo.outputFilePath, s_tmlogInfo.inputFilePath, dirPos );
	}
	else
	{
		memcpy(s_tmlogInfo.outputFilePath, s_tmlogInfo.outputDir, sizeof(s_tmlogInfo.outputDir) );
	}

	strcat(s_tmlogInfo.outputFilePath, "tmlog");
	sprintf(&s_tmlogInfo.outputFilePath[strlen(s_tmlogInfo.outputFilePath)], "%04d%02d%02d%02d%02d%02d",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
	strcat(s_tmlogInfo.outputFilePath, ".csv");


	if( (fp = fopen(s_tmlogInfo.outputFilePath, "w")) == NULL)
	{
		printf("ファイルオープンエラー\n");
	}
	else
	{
		sprintf(writeData, "日時");
		for(loopCnt=0; loopCnt < DEBUG_TM_LOG_MAX; loopCnt++ )
		{
			if( s_tmlogInfo.s_tminfo[loopCnt].statFlg != 0 )
			{
				sprintf(&writeData[strlen(writeData)], ",%s", ps_g_TMDATINF[s_tmlogInfo.s_tminfo[loopCnt].tmIfLink].tmName);
			}
			else
			{
				sprintf(&writeData[strlen(writeData)], ",");
			}
		}
		fprintf(fp, "%s\n", writeData);

		sleepTm.tv_sec = 0;
		waitTime_us = s_tmlogInfo.cycleTim*1000;

		s_tmlogInfo.status = 1;
		/* スレッド停止フラグがＯＦＦの間は処理を続ける */
		while( s_tmlogInfo.status != 2 )
		{
			gettimeofday( &nowTime, NULL );

			sleepTm.tv_nsec = ((nowTime.tv_usec + waitTime_us) / waitTime_us) * waitTime_us * 1000;
			sleepTm.tv_nsec -= (nowTime.tv_usec * 1000);

			/* 待機 */
			nanosleep( &sleepTm, NULL);

			gettimeofday( &nowTime, NULL );
			nowTime.tv_usec -= (nowTime.tv_usec % waitTime_us);

			localtime_r( &nowTime.tv_sec, &timeData );
			sprintf(writeData, "%04d/%02d/%02d %02d:%02d:%02d.%03d", 
					timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday,
					timeData.tm_hour, timeData.tm_min, timeData.tm_sec, (nowTime.tv_usec/1000));

			for(loopCnt=0; loopCnt < DEBUG_TM_LOG_MAX; loopCnt++ )
			{
				if( ( s_tmlogInfo.s_tminfo[loopCnt].statFlg != 0 ) && (ps_g_TMSTSFIL[s_tmlogInfo.s_tminfo[loopCnt].tmIfLink].tmFlag == PV_TMFLG_NORMAL ))
				{
					sprintf(&writeData[strlen(writeData)], ",%d", ps_g_TMSTSFIL[s_tmlogInfo.s_tminfo[loopCnt].tmIfLink ].tmVal);
				}
				else
				{
					sprintf(&writeData[strlen(writeData)], ",");
				}
			}
			fprintf(fp, "%s\n", writeData);
		}
        fclose(fp);
	}
	return( NULL );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶ分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列をカンマで区切り、ps_sepStr に格納する。
 * 戻り値：
 * 		カンマで区切られたデー多数を返す。
 */
int func_dbg_sep_data( char *pc_data, struct st_dbg_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_sepStr, 0, sizeof(struct st_dbg_sepStr) );

	pt = pc_data;
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		if( *pt == ',' )
		{
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
 * 計測ログ取得
 * -----------------------------------------------------------------------------
 * 収集装置 計測ログ取得
 *
 * 機能：
 *		収集装置内の指定した計測情報を取得し、出力する
 *
 */
void func_tmlogsetting()
{
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;	/* 端末サイズ保存 */
	int						outLimit;		/* 表示可能行数 */
	int						noOut=6;		/* 表示不可行数 */
	char					keyData[1024];
	int						keyByte;
	FILE					*fp;						/* ファイルディスクリプタ */
	int						loopCnt;
	int						tmInfoCnt=0;
	char					inputPath[256];
	int						retCode = 0;
	time_t					nowTime;		/* 現在時刻 */
	struct tm				localTime;		/* 現在時刻 */
	struct st_tminfo 		tminfo[DEBUG_TM_LOG_MAX];
	char					readByte;		/* キー入力バイト数 */
	struct st_dbg_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						workData;					/* 数値変換用 */
	int						soKind;						/* 装置種別 */
	int						soNumb;						/* 装置番号 */
	int						tmNumb;						/* 計測番号 */
	int						tmIfLink;					/* 計測情報リンク */
	int						cycleTim;
	int						menuNumb;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* 変更用設定にコピー */
		ttyChange = ttyBackup;

		/* 端末のサイズを取得 */
		func_termsize_get();
		termSize_OLD.ws_row = termSize.ws_col;

		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <計測ログ取得> =======================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );

		printf("--------------------------------------------------------------------------------\n");
		printf("   1.計測ログ取得設定\n");
		printf("   2.計測ログ取得周期設定\n");
		printf("   3.計測ログ取得開始/停止\n");
		printf("  99.終了\n");
		printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");
		printf("================================================================================\n");
		printf("番号を選択してください（改行のみの場合はメニューを再表示） >> ");


		fflush(NULL);

		memset( keyData, 0, sizeof( keyData ) );

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		keyByte = read( 0, &keyData, 1024 );
		/* 入力値を数値化 */
		menuNumb = atoi( keyData );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if(keyByte < 0 ) break;


		/* キー入力が終了の場合 */
		if( menuNumb == 99 )	break;

		if( menuNumb == 1)
		{
			if( s_tmlogInfo.status != 0 )
			{
				printf("\033[0K");
				printf("計測情報取得中です。終了してから実施してください。\n");
				func_mesasge_view_wait();
			}
			else
			{
				/* ターミナルの設定を元に戻す */
				ioctl( 0, TCSETAF, &ttyBackup );

				tmInfoCnt=0;
				printf("\n");
				printf("計測情報設定のファイルパスを入力して下さい\n");
				printf("  >>");
				fflush( NULL );

				if( signal_flg ) break;

				memset( keyData, 0, sizeof( keyData ) );
				keyByte = read( 0, &keyData, 1024 );


				memset(inputPath, 0, sizeof(inputPath));
				memcpy(inputPath, keyData, keyByte-1);

				if( (fp = fopen(inputPath, "rb")) == NULL)
				{
					printf("ファイルのオープンに失敗しました。\n");
					func_mesasge_view_wait();
				}
				else
				{
					retCode = 0;
					memset(tminfo, 0, sizeof(tminfo));

					/* CSVファイルの終端まで繰り返す */
					while( fgets( readBuff, 4096, fp) != NULL )
					{
						/* コメント行はスキップ */
						if( readBuff[0] == '#' ) continue;
						if( strlen( readBuff ) < 1) continue;

						/* CSVデータの分割 */
						csvCount = func_dbg_sep_data( readBuff, &s_sepStr );
						if( csvCount != 3 )
						{
							printf("データ数が合っていません。\n");
							func_mesasge_view_wait();
							retCode = -1;
							break;
						}

						/* 装置種別の取得 */
						soKind = atoi( s_sepStr.data[0] );
						if( ( soKind < PV_MKIND_PVSYS ) || ( soKind > PV_MKIND_LAST ) )
						{
							printf("装置種別が範囲外です\n");
							func_mesasge_view_wait();
							retCode = -1;
							break;
						}
						/* 装置番号の取得 */
						soNumb = atoi( s_sepStr.data[1] );
						if( (soNumb < 1) || (soNumb > ps_g_PVSYSINF->s_machineInfo[soKind].machineCnt) )
						{
							printf("装置番号が範囲外です\n");
							func_mesasge_view_wait();
							retCode = -1;
							break;
						}
						/* 計測番号の取得 */
						tmNumb = atoi( s_sepStr.data[2] );
						if( (tmNumb < 1) || (tmNumb > ps_g_PVSYSINF->s_machineInfo[soKind].maxTm) )
						{
							printf("計測番号が範囲外です\n");
							func_mesasge_view_wait();
							retCode = -1;
							break;
						}
						/* 計測情報のリンクを取得 */
						tmIfLink = ps_g_PVSYSINF->s_machineInfo[soKind].tmIfLink + ps_g_PVSYSINF->s_machineInfo[soKind].maxTm * (soNumb - 1) + tmNumb - 1;
						if( ps_g_TMDATINF[tmIfLink].statFlag == 0 )
						{
							printf("計測値が実装されていません\n");
							func_mesasge_view_wait();
							retCode = -1;
							break;
						}

						tminfo[tmInfoCnt].statFlg = 1;
						tminfo[tmInfoCnt].tmIfLink = tmIfLink;
						tminfo[tmInfoCnt].soKind = soKind;
						tminfo[tmInfoCnt].soNumb = soNumb;
						tminfo[tmInfoCnt].tmNumb = tmNumb;
						tmInfoCnt++;
					}
					/* 正常終了の場合 */
					if( retCode == 0 )
					{
						memcpy(s_tmlogInfo.inputFilePath, inputPath, sizeof(inputPath));
						memcpy(s_tmlogInfo.s_tminfo, tminfo, sizeof(s_tmlogInfo.s_tminfo));
					}
				}
			}
		}
		else if( menuNumb == 2)
		{
			if( s_tmlogInfo.status != 0 )
			{
				printf("\033[0K");
				printf("計測情報取得中です。終了してから実施してください。\n");
				func_mesasge_view_wait();
			}
			else
			{
				printf("\n");
				printf("\033[0K");
				/* ターミナルの設定を元に戻す */
				ioctl( 0, TCSETAF, &ttyBackup );
				printf("\033[0K");
				printf("収集周期(100ms単位)を設定して下さい。（ 現在：%4d ms）->", s_tmlogInfo.cycleTim);
				fflush( NULL );
				memset( keyData, 0, sizeof( keyData ) );
				keyByte = read( 0, &keyData, 1024 );
				printf("\n");
				printf("\033[0K");
				if(keyByte > 2 )
				{
					/* 入力値を数値化 */
					cycleTim = atoi( keyData );
					if( ( cycleTim < 100 ) || ( ( cycleTim % 100) != 0 ))
					{
						printf("\033[0K");
						printf("入力データが正しくありません\n");
						func_mesasge_view_wait();
					}
					else
					{
						s_tmlogInfo.cycleTim = cycleTim;
					}
				}
			}
		}
		else if( menuNumb == 3)
		{

			if( ( s_tmlogInfo.inputFilePath[0] == 0 ) || ( s_tmlogInfo.cycleTim == 0 ))
			{
				printf("\033[0K");
				if( s_tmlogInfo.inputFilePath[0] == 0 )
					printf("計測情報が未設定です\n");
				else
					printf("収集周期が未設定です\n");
				func_mesasge_view_wait();
			}
			else
			{
				while(1)
				{
					/* 画面をクリア */
					printf("\033[2J");
					/* カーソルの位置を高さ０、横０に移動 */
					printf("\033[%d;%dH",0,0);
					/* 現在時刻を取得 */
					nowTime = time( NULL );
					/* 年月日時分秒を取得 */
					localtime_r( &nowTime, &localTime );
					/* カーソルの位置からその行の右端までクリア */
					printf("\033[0K");
					printf("= <計測ログ取得> =======================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
							localTime.tm_year + 1900,
							localTime.tm_mon + 1,
							localTime.tm_mday,
							localTime.tm_hour,
							localTime.tm_min,
							localTime.tm_sec );
					printf("\033[0K");

					printf("計測情報------------------------------------------------------------------------\n");
					printf("\033[0K");	printf("管理 装 装  計測  保存   現在値(直値)    計測名称\n");
					printf("\033[0K");	printf("番号 種 番  番号  位置   FL VALUE_____   _____________________\n");
					for(loopCnt=0; loopCnt < DEBUG_TM_LOG_MAX; loopCnt++ )
					{
						if( s_tmlogInfo.s_tminfo[loopCnt].statFlg != 0 )
						{
							printf(" %02d [%02d,%03d,%04d] %05d [%02X %10ld]  %s\n", loopCnt + 1, 
								s_tmlogInfo.s_tminfo[loopCnt].soKind,
								s_tmlogInfo.s_tminfo[loopCnt].soNumb,
								s_tmlogInfo.s_tminfo[loopCnt].tmNumb,
								s_tmlogInfo.s_tminfo[loopCnt].tmIfLink,
								(ps_g_TMSTSFIL + s_tmlogInfo.s_tminfo[loopCnt].tmIfLink)->tmFlag,
								(ps_g_TMSTSFIL + s_tmlogInfo.s_tminfo[loopCnt].tmIfLink)->tmVal,
								ps_g_TMDATINF[s_tmlogInfo.s_tminfo[loopCnt].tmIfLink].tmName);
						}
					}
					printf("--------------------------------------------------------------------------------\n");
					printf("収集周期   ： %4d ms\n", s_tmlogInfo.cycleTim );
					printf("--------------------------------------------------------------------------------\n");
					printf(" ステータス：[%s]\n", ((s_tmlogInfo.status == 0) ? "停止  " : (s_tmlogInfo.status == 1) ? "取得中" : "停止中"));
					printf("   1.開始\n");
					printf("   2.停止\n");
					printf("  99.戻る\n");
					printf("--------------------------------------------------------------------------------\n");
					if( ( s_tmlogInfo.status == 0 ) && (s_tmlogInfo.outputFilePath[0] != 0 ) )
					{
						printf("最終保存ファイル：%s\n", s_tmlogInfo.outputFilePath);
						printf("--------------------------------------------------------------------------------\n");
					}
					printf("番号を選択してください（改行のみの場合はメニューを再表示） >> ");

					fflush(NULL);
					if( signal_flg ) break;

					memset( keyData, 0, sizeof( keyData ) );
					keyByte = read( 0, &keyData, 1024 );

					/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
					if( keyByte < 0 ) break;

					/* 入力値を数値化 */
					menuNumb = atoi( keyData );
					
					/* キー入力が終了の場合 */
					if( menuNumb == 99 ) break;

					if( menuNumb == 1)
					{
						/* ファイルチェック用スレッドを生成 */
						pthread_create( &s_tmlogInfo.threadID, NULL, func_tmlog_thread, NULL );

					}
					if( menuNumb == 2)
					{
						s_tmlogInfo.status = 2;
						pthread_join( s_tmlogInfo.threadID, NULL );
						s_tmlogInfo.status = 0;
					}
				}
			}
		}
	}
	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 収集装置シャットダウン／リブート設定
 *
 * 機能：
 *		収集装置のシャットダウン／リブート指示を行う
 *
 */
void	func_pvmode_change()
{
	char				pvmodeName[3][96] = { "（何もしない）", "　シャットダウン　", "　リブート　" };
	char				onldatName[2][96] = { "（何もしない）", "　初期化する　" };
	char				ftpdatName[2][96] = { "（何もしない）", "　全て削除する　" };
	char				checkFile1[256];
	char				checkFile2[256];
	char				execCmd[256];
	int					stopMode = 0;
	int					pvmodeNumb = 0;
	int					onldatNumb = 0;
	int					ftpdatNumb = 0;
	int					checkFlg = 0;
	char				workName[13];	/* 文字列処理用 */
	int					menuNo;			/* メニュー番号 */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */

	/* 装置シャットダウン用ファイルパスの設定 */
	strcpy( checkFile1, PV_PATH_DAT );
	strcat( checkFile1, PV_FNAME_PV_SHUTDOWN );
	/* 装置リブート用ファイルパスの設定 */
	strcpy( checkFile2, PV_PATH_DAT );
	strcat( checkFile2, PV_FNAME_PV_REBOOT );

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 画面をクリア */
	printf("\033[2J");

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 管理プロセス（NKMANAGE）の動作状態を表示 */
        if( func_nkmanage_pidget() > 0 )
		{
			/* 収集装置のシャットダウンが設定されている場合 */
			if( access( checkFile1, R_OK ) == 0 )
			{
				pvmodeNumb = 1;
				stopMode = 1;
			}
			/* 収集装置のリブートが設定されている場合 */
			else if( access( checkFile2, R_OK ) == 0 )
			{
				pvmodeNumb = 2;
				stopMode = 1;
			}
		}
		else
		{
			stopMode = 2;
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		/* 見出しの表示 */
		printf("= シャットダウン／リブート設定 =================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		if( stopMode == 1 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			if( pvmodeNumb == 1 )
				printf("現在  シャットダウン  準備中です\n");
			else
				printf("現在  リブート  準備中です\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る[q]                                                                        ");
		}
		else if( stopMode == 2 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("現在  シャットダウン／リブート  設定ができない状態です。\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る[q]                                                                        ");
		}
		else
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("収集装置の運転状態\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("--------------------------------------------------------------------------------\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf(" [m]  運転状態の変更                         : %s\n", pvmodeName[pvmodeNumb]);

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

#if 0
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("シャットダウンまたはリブート時のデータ処理\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("--------------------------------------------------------------------------------\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf(" [1]  収集装置が保持する状態・計測値データ   : %s\n", onldatName[onldatNumb]);

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf(" [2]  モニタリングサーバへの未送信データ     : %s\n", ftpdatName[ftpdatNumb]);

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

#endif
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("--------------------------------------------------------------------------------\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			if( pvmodeNumb == 1 )
				printf(" 注意事項：遠隔でシャットダウンを実施した場合、装置の再起動には現地において\n");
			else if( pvmodeNumb == 2 )
				printf(" 注意事項：リブートによる再起動は、３～１０分程度の時間を要します。このため\n");
			else
				printf(" 注意事項：なし\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			if( pvmodeNumb == 1 )
				printf("           電源のＯＦＦ／ＯＮ作業が必要となります。（遠隔では再起動できません）\n");
			else if( pvmodeNumb == 2 )
				printf("           再起動処理が５分周期を跨ぐ場合はデータ欠落が発生する可能があります。\n");
			else
				printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			if( checkFlg )
				printf(" ### 実行確認 ###  収集装置の%sを実行します。よろしければ [y]\n", pvmodeName[pvmodeNumb] );
			else
				printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("戻る[q]                                       設定変更[指定キー]  処理の実行[r]");
		}

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		if( stopMode == 0 )
		{
			/* キー入力がモード変更の場合 */
			if( (inKeyData[0] == 'm') || (inKeyData[0] == 'M') )
			{
				checkFlg = 0;
				pvmodeNumb++;
				if( pvmodeNumb > 2 ) pvmodeNumb = 0;
				if( pvmodeNumb == 0 )
				{
					onldatNumb = 0;
					ftpdatNumb = 0;
				}
			}
#if 0
			/* キー入力が状態データ初期化変更の場合 */
			if( inKeyData[0] == '1') 
			{
				checkFlg = 0;
				if( pvmodeNumb != 0 )
				{
					onldatNumb++;
					if( onldatNumb > 1 ) onldatNumb = 0;
				}
			}
			/* キー入力が送信データ初期化変更の場合 */
			if( inKeyData[0] == '2') 
			{
				checkFlg = 0;
				if( pvmodeNumb != 0 )
				{
					ftpdatNumb++;
					if( ftpdatNumb > 1 ) ftpdatNumb = 0;
				}
			}
#endif
			/* キー入力が実行の場合 */
			if( (inKeyData[0] == 'r') || (inKeyData[0] == 'R') )
			{
				if( pvmodeNumb != 0 )
					checkFlg = 1;
				else
					checkFlg = 0;
			}
			/* キー入力が実行確認の場合 */
			if( (inKeyData[0] == 'y') || (inKeyData[0] == 'Y') )
			{
				/* 実行確認キー入力待ちの場合 */
				if( checkFlg )
				{
					/* 手動停止扱いを設定 */
					commfunc_manage_manualStopSet( 1 );

					/* 停止モードにあわせてファイルを設定 */
					if( pvmodeNumb == 1 )
						sprintf(execCmd, "/usr/bin/touch %s", checkFile1 );
					else
						sprintf(execCmd, "/usr/bin/touch %s", checkFile2 );
					system( execCmd );
	
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
			if( (inKeyData[0] == 'n') || (inKeyData[0] == 'N') )
				checkFlg = 0;
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * スケジュール管理情報表示
 *
 * 機能：
 *		引数の月の開始位置と月の日数を返却
 *
 */
int func_display_pvschdays( int buffSelect, int dataPos, int *month, int *yearDays, int *monDays )
{
	char		month_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
	int			loopCnt;
	int			day_sum=0;
	
	if( ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear < 2000 )
	{
		return -1;
	}

	if( (ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear % 4 ) == 0 )
		month_days[1]++;

	for( loopCnt = 0; loopCnt < 12; loopCnt++ )
	{
		if( dataPos < ((day_sum + month_days[loopCnt]) * PV_SCH_DAY_TI_NUM) )
		{
			break;
		}
		day_sum += month_days[loopCnt];
	}

	*month = loopCnt+1;
	*yearDays = day_sum;
	*monDays = month_days[loopCnt];

	return 0;
}


int func_display_get_days( int year, int month )
{
	/*							1  2  3  4  5  6  7  8  9  10 11 12 */
	char		month_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};

	if( (year % 4 ) == 0 )
	{
	    if((year % 100 ) != 0 )
	    {
			month_days[1]++;
		}
		else
		{
	    	if((year % 400 ) == 0 )
	    	{
				month_days[1]++;
			}
		}

	
	}

	return month_days[month-1];
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * システムステータス表示
 *
 * 機能：
 *		簡易システムステータス情報の表示を行う
 *
 */
void	func_display_syste_status_en()
{
	char				workStr[32];	/* ワーク */
	int					outLimit;		/* 表示可能行数 */
	int					noOut=20;		/* 表示不可行数 */
	int					tmifLink;		/* 計測情報リンクポインタ */
	int					loopCnt;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	int					page_idx=0;		/* ページ切替 */
	int 				soKind;			/* 装置種別 */
	int 				soNumb;			/* 装置番号 */
	struct st_EMCTLFIL	*pst_EMCTLFIL;	/* エネルギーマネジメント管理ファイルポインタ */
	double				tmpVal;

	int					soutiNumb;		/* 装置番号 */
	int					ststBase;			/* 通信ステータス状態リンク開始位置 */
	int					abnormalFlg;	/* 異常フラグ */
	int					stifLink;			/* 通信ステータスリンク */
	int					svifLink;			/* 装置状態情報リンク情報 */
	int					svstLink;			/* 装置状態リンク情報 */
	int					svstBase;			/* 装置状態リンク情報（ベース値） */
	unsigned char		svstMask;			/* 状態マスク値 */

	/* 画面をクリア */
	printf("\033[2J");
	/* カーソルの位置を高さ０、横０に移動 */
	printf("\033[%d;%dH",0,0);

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = GET_OUT_LIMIT(termSize.ws_row, noOut );

		/* 端末のサイズ（高さ）が変超されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */

		printf("= <NK-CEMS System status summary> =====================[ %04d-%02d-%02dT%02d:%02d:%02dZ ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\033[0K");
		printf("System status-------------------------------------------------------------------\n");
		printf("\033[0K");
		printf("  System status              : ");
		abnormalFlg = 0;

		/* 装置種別に合わせて情報のリンクポインタ値を取得する */
		svifLink = ps_g_PVSYSINF->s_machineInfo[ PV_MKIND_PVSYS ].svIfLink;
		/* 装置種別に合わせて状態のリンクポインタ値を取得する */
		svstBase = 	ps_g_PVSYSINF->s_machineInfo[ PV_MKIND_PVSYS ].svStLink;

		/* 1機器の全通信ステータスを取得 */
		for( loopCnt = 0; loopCnt < PV_MAX_PVSYS_SVPOS; loopCnt++ )
		{
			/* 装置異常、メール送信異常～縮退運転、Web-API 上り通信異常～は、異常としない */
			if( ((ps_g_SVDATINF + svifLink)->svNumb == PV_SYS_SV_SYSRUN) ||
				 ((ps_g_SVDATINF + svifLink)->svNumb <= PV_SYS_SV_SHUKUTAI))
				continue;

			/* 装置状態取得のためのマスク値を設定 */
			svstMask = (unsigned char)(0x80 >> (((ps_g_SVDATINF + svifLink)->svNumb - 1) % 8));
			/* 装置状態の取得と表示 */
			if( (ps_g_SVSTSFIL + svstBase + ((ps_g_SVDATINF + svifLink)->svNumb - 1) / 8 )->svStat & svstMask )
				abnormalFlg = 1;

			svifLink++;
		}
		if( abnormalFlg == 0 )
			printf("Good\n");
		else
			printf("Bad\n");
		

		printf("\033[0K");
		printf("Communication status------------------------------------------------------------\n");
		printf("\033[0K");

		/* 装置種別が未実装の場合は次の装置の処理を行う */
		if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_BATTE].machineCnt != 0 )
		{
			abnormalFlg = 0;
			for( soutiNumb = 0; soutiNumb < ps_g_PVSYSINF->s_machineInfo[PV_MKIND_BATTE].maxMachine; soutiNumb ++)
			{
	  			if( (ps_g_BATTEINF + soutiNumb)->statFlag == 0 )
					continue;

				ststBase = ps_g_PVSYSINF->s_machineInfo[ PV_MKIND_BATTE ].stStLink + soutiNumb * ((PV_MAX_STPOS - 1) / 8 + 1);

				/* 1機器の全通信ステータスを取得 */
				for( loopCnt = 0; loopCnt < PV_MAX_STPOS; loopCnt++ )
				{
					/* 通信ステータス状態取得用マスク値の設定 */
					svstMask = (unsigned char)(0x80 >> (loopCnt % 8));

					if( (ps_g_STSTSFIL + ststBase + loopCnt / 8 )->stStat & svstMask )
						abnormalFlg = 1;
				}
			}
			printf("\033[0K");
			printf("  Battery system             : ");
			if( abnormalFlg == 0 )
				printf("Good\n");
			else
				printf("Bad\n");
			
		}


		/* 装置種別が未実装の場合は次の装置の処理を行う */
		if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_MLTMT].machineCnt != 0 )
		{
			for( soutiNumb = 0; soutiNumb < ps_g_PVSYSINF->s_machineInfo[PV_MKIND_MLTMT].maxMachine; soutiNumb ++)
			{
				abnormalFlg = 0;
	  			if( (ps_g_MLTMTINF + soutiNumb)->statFlag == 0 )
					continue;

				ststBase = ps_g_PVSYSINF->s_machineInfo[ PV_MKIND_MLTMT ].stStLink + soutiNumb * ((PV_MAX_STPOS - 1) / 8 + 1);

				/* 1機器の全通信ステータスを取得 */
				for( loopCnt = 0; loopCnt < PV_MAX_STPOS; loopCnt++ )
				{
					/* 通信ステータス状態取得用マスク値の設定 */
					svstMask = (unsigned char)(0x80 >> (loopCnt % 8));

					if( (ps_g_STSTSFIL + ststBase + loopCnt / 8 )->stStat & svstMask )
						abnormalFlg = 1;
				}
				printf("\033[0K");
				printf("  Power meter %d              : ", soutiNumb+1);
				if( abnormalFlg == 0 )
					printf("Good\n");
				else
					printf("Bad\n");
			}
			
		}
		printf("================================================================================\n");
		printf("Quit[q]                                                      (Redraw after 5sec)");
		/* カーソルの位置からその行の右端までクリア */
		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	return;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 収集装置シャットダウン設定
 *
 * 機能：
 *		収集装置のシャットダウン指示を行う
 *
 */
void	func_pvmode_shutdown_en( int mode)
{
	char				checkFile1[256];
	char				checkFile2[256];
	char				execCmd[256];
	int					stopMode = 0;
	int					pvmodeNumb = 0;
	int					onldatNumb = 0;
	int					ftpdatNumb = 0;
	int					checkFlg = 0;
	char				workName[13];	/* 文字列処理用 */
	int					menuNo;			/* メニュー番号 */
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */

	/* 装置シャットダウン用ファイルパスの設定 */
	strcpy( checkFile1, PV_PATH_DAT );
	strcat( checkFile1, PV_FNAME_PV_SHUTDOWN );
	/* 装置リブート用ファイルパスの設定 */
	strcpy( checkFile2, PV_PATH_DAT );
	strcat( checkFile2, PV_FNAME_PV_REBOOT );

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 画面をクリア */
	printf("\033[2J");

	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 管理プロセス（NKMANAGE）の動作状態を表示 */
        if( func_nkmanage_pidget() > 0 )
		{
			/* 収集装置のシャットダウンが設定されている場合 */
			if( access( checkFile1, R_OK ) == 0 )
			{
				pvmodeNumb = 1;
				stopMode = 1;
			}
			/* 収集装置のリブートが設定されている場合 */
			else if( access( checkFile2, R_OK ) == 0 )
			{
				pvmodeNumb = 2;
				stopMode = 1;
			}
		}
		else
		{
			stopMode = 2;
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		/* 見出しの表示 */
		if( mode == 0 )
			printf("= Shutdown =====================================================================\n");
		else
			printf("= Reboot =======================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("\n");

		if( stopMode == 1 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");

			if( pvmodeNumb == 1 )
				printf("   Currently preparing to shutdown\n");
			else
				printf("   Currently preparing to reboot\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("Quit[q]                                                                        ");
		}
		else if( stopMode == 2 )
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			if( mode == 0 )
				printf("   Shutdown is not available now\n");
			else
				printf("   Reboot is not available now\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("Quit[q]                                                                        ");
		}
		else
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf(" ### Execution confirmation ###\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			if( mode == 0 )
				printf("   Perform a shutdown of the device. \n");
			else
				printf("   Perform a reboot of the device. \n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("   Is it okay? [y]\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf(" \n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("================================================================================\n");

			/* カーソルの位置からその行の右端までクリア */
			printf("\033[0K");
			printf("Quit[q]                                                             Execution[y]");
		}

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		if( stopMode == 0 )
		{
			/* キー入力が実行確認の場合 */
			if( (inKeyData[0] == 'y') || (inKeyData[0] == 'Y') )
			{
				pvmodeNumb = 1;
				/* 手動停止扱いを設定 */
				commfunc_manage_manualStopSet( 1 );

				/* 停止モードにあわせてファイルを設定 */
				if( mode == 0 )
					sprintf(execCmd, "/usr/bin/touch %s", checkFile1 );
				else
					sprintf(execCmd, "/usr/bin/touch %s", checkFile2 );
				
				system( execCmd );

				/* 画面をクリア */
				printf("\033[2J");
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 演算結果取得
 *
 * 機能
 *		演算結果を算出して返却する
 *
 */
double func_cmn_ConvEnzan( int tmVal, short expData, long long convData, long long baseData )
{
	long long	calcData;		/* 演算データ */
	long long	pow10Data;		/* べき乗データ */
	double		calcEnzan;

	pow10Data = (long long)commfunc_pow10abs( expData );
	calcData = (long long)tmVal;
	calcData *= (long long)convData;
	calcData += (long long)baseData;

	if( expData >= 0 )
	{
		calcEnzan = (double)calcData * pow10Data;
	}
	else
	{
		calcEnzan = (double)calcData / pow10Data;
	}

	return calcEnzan;
}

void func_cmn_setTimeStr( int cnvPtn, struct timeval *cnvtime, char *cnvStr )
{
	struct tm			localTime;		/* 時刻形式 */

	localtime_r( &cnvtime->tv_sec, &localTime );
	
	/* 年～秒 */
	if( cnvPtn == TIME_STR_PTN_1 )
	{
		if( cnvtime->tv_sec == 0 )
		{
			sprintf(cnvStr, "----/--/-- --:--:--");
		}
		else
		{
			sprintf(cnvStr, "%04d/%02d/%02d %02d:%02d:%02d",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		}
	}
	/* 年～マイクロ秒 */
	else if( cnvPtn == TIME_STR_PTN_2 )
	{
		if( cnvtime->tv_sec == 0 )
		{
			sprintf(cnvStr, "----/--/-- --:--:--.------");
		}
		else
		{
			sprintf(cnvStr, "%04d/%02d/%02d %02d:%02d:%02d.%06ld",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				cnvtime->tv_usec );
		}
	}
	/* 月～マイクロ秒 */
	else if( cnvPtn == TIME_STR_PTN_3 )
	{
		if( cnvtime->tv_sec == 0 )
		{
			sprintf(cnvStr, "--/-- --:--:--.------");
		}
		else
		{
			sprintf(cnvStr, "%02d/%02d %02d:%02d:%02d.%06ld",
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				cnvtime->tv_usec );
		}
	}
	/* 時～マイクロ秒 */
	else //if( cnvPtn == TIME_STR_PTN_4 )
	{
		if( cnvtime->tv_sec == 0 )
		{
			sprintf(cnvStr, "--:--:--.------");
		}
		else
		{
			sprintf(cnvStr, "%02d:%02d:%02d.%06ld",
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				cnvtime->tv_usec );
		}
	}
}
 	
/* [20250807] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * 模擬周波数設定
 *
 * 機能：
 *		模擬周波数の計画データ(CSVファイル)を読み込み、
 *		開始時刻、約定△kWを設定すると、計画に従って
 *		模擬周波数で一次調整力制御を実行する。
 */
void func_display_freqsim_set( void )
{
	int							dispMode = 0;		/* 表示モード */
	int							offSet = 0;			/* 表示オフセット */
	int							loopCnt1;			/* ループカウンタ */
	int							loopCnt2;			/* ループカウンタ */
	int							loopCnt3;			/* ループカウンタ */
	int							outLimit;			/* 表示可能行数 */
	struct tm					localTime;			/* 現在時刻 */
	time_t						nowTime;			/* 現在時刻 */
	struct timeval				s_nowTime;			/* 現在時刻 */
	char						inKeyData;			/* キー入力値 */
	char						readByte;			/* キー入力バイト数 */
	struct termio				ttyBackup;			/* 端末設定保存用 */
	struct termio				ttyChange;			/* 端末設定変更用 */
	struct winsize				termSize_OLD;		/* 端末サイズ保存用 */
	char						enzanStr[32];		/* 演算結果格納 */
	char						errMsg[128];		/* エラー文字列 */
	int							errCode = 0;		/* エラー種別 */
	int							msgCode = 0;		/* エラー種別 */
	char						filePath[128];		/* 模擬周波数ファイルパス *//* [20251211] ファイルパス整形 Chg */
	FILE						*fp;				/* ファイルディスクリプタ */
	char						readBuff[4096];		/* CSVファイル読み込みバッファ */
	int							csvCount;			/* CSVデータ数（デリミタ分割数） */
	int							csvOffset;			/* CSVデータオフセット */
	int							csvdata = 0;		/* データ */
	int							syoriCount = 0;		/* CSV処理カウンタ */
	int							startTime = 0;		/*  */
	struct st_sepStr			s_sepStr;			/* CSVデータ分割格納 */
	struct tm					s_stTime_tm;		/* 開始時刻 */
	struct timeval				s_stTime_timeval;	/* 開始時刻 */
	struct tm					work_tm;			/* 開始時刻操作領域 */
	int							fcrDltKw = 0;		/* 約定△kW */
	char						inputStr[20];		/* 入力文字列 */
	char						workStr[5];			/* 文字列操作領域 */
	int							inputErr = 0;		/* 入力エラー */
	int							workDuration;		/* 経過時間操作領域 */
	struct st_fcrFreqSimData	s_fcr_freq_sim_data;/* 模擬周波数データ */
	int							first_flg = 0;		/* 乗数を取得 *//* [20251211] 模擬周波数読み込み時の乗数保持 */

	sprintf(filePath, "%s%s", PV_PATH_EXT, FCR_FREQ_SIM_FILENAME );/* [20251211] 模擬周波数ファイルパス整形 Add */
	memset(errMsg, 0, sizeof(errMsg));
	memset(&s_fcr_freq_sim_data, 0, sizeof(s_fcr_freq_sim_data));

	/* CSVファイルオープン */
	if( ( fp = fopen( filePath, "r" ) ) != NULL )
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			csvOffset=0;
			/* CSV処理カウンタ＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（5文字以下）はスキップ */
			if( strlen( readBuff ) < 5) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );
			/* CSVデータ数が不正の場合 */
			if( csvCount != 2)
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 2). File[%s] Line[%d] DATACNT[%d]", filePath, syoriCount, csvCount );
				msgCode = -2;
				errCode = -2;
				break;
			}
/* [20251211] 系統周波数の元になる周波数の乗数の取得 Chg St */
			if( first_flg == 0 )
			{
				first_flg = 1;
				csvOffset++;
				csvdata = atoi( s_sepStr.data[csvOffset] );
				if( csvdata == -3 || csvdata == -2 || csvdata == -1 || csvdata == 0 )
				{
					/* 乗数を設定 */
					s_fcr_freq_sim_data.freqMeasurementNum = csvdata;
				}
				else
				{
					/* 乗数の値が不正 */
					sprintf( errMsg, "CSV-Format Error (Data is not freqMeasurementNum(%d)). csvdata, File[%s] Line[%d] DATACNT[%d]", filePath, 1, csvCount );
					msgCode = -2;
					errCode = -2;
					break;
				}
			}
			else
			{
				csvdata = atoi( s_sepStr.data[csvOffset] );
				/* 継続時間が0より小さい場合 */
				if( csvdata < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Data is < 0). File[%s] Line[%d] DATA[%d]", filePath, syoriCount, csvOffset+1 );
					msgCode = -2;
					errCode = -2;
					break;
				}
				s_fcr_freq_sim_data.s_fcr_freq_sim_list[s_fcr_freq_sim_data.simDataCount].duration = csvdata;
				csvOffset++;
				csvdata = atoi( s_sepStr.data[csvOffset] );
				/* 周波数が0より小さい場合 */
				if( csvdata < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Data is < 0). File[%s] Line[%d] DATA[%d]", filePath, syoriCount, csvOffset+1 );
					msgCode = -2;
					errCode = -2;
					break;
				}
				s_fcr_freq_sim_data.s_fcr_freq_sim_list[s_fcr_freq_sim_data.simDataCount].freqValue = csvdata;
				s_fcr_freq_sim_data.simDataCount++;
			}
/* [20251211] 系統周波数の元になる周波数の乗数の取得 Chg Ed */
		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "CSV-Format Error (file not found)." );
		msgCode = -1;
		errCode = -1;
	}

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力を５秒まつ */
//	ttyChange.c_cc[VMIN] = 0;
//	ttyChange.c_cc[VTIME] = viewTime;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 12 );

		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <%s (模擬周波数設定)> ==========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			s_machineName[ 0 ].nameShort,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("模擬周波数ファイル  : [%s] \n", FCR_FREQ_SIM_FILENAME );/* [20251211] 模擬周波数ファイルパス整形、乗数表記 Chg */
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

/* [20251211] 乗数表記 Chg St */
		if( dispMode == 0 )
		{
			if( s_fcr_freq_sim_data.simDataCount != 0 )
			{
				printf("表示内容：メモリ        乗数：%2d\n", s_fcr_freq_sim_data.freqMeasurementNum );
			}
			else
			{
				printf("表示内容：メモリ        乗数：--\n" );
			}
		}
		else
		{
			if( ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount != 0 )
			{
				printf("表示内容：EMCTLFIL      乗数：%2d\n", ps_g_EMCTLFIL->s_fcr_freq_sim_data.freqMeasurementNum );
			}
			else
			{
				printf("表示内容：EMCTLFIL      乗数：--\n" );
			}
		}
/* [20251211] 乗数表記 Chg Ed */

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------------------------------------------------------------------------------\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		if( dispMode == 0 )
		{
			switch(msgCode)
			{
			case 0:
				printf("読込成功しました。\n");
				break;
			case 1:
				printf("模擬周波数を登録しました。\n");
				break;
			case 2:
				printf("模擬周波数を削除しました。\n");
				break;
			case 3:
				printf("操作を中止しました。\n");
				break;
			case 4:
				printf("開始時刻、約定△kWを入力しました。\n");
				break;
			case -1:
				printf("模擬周波数ファイルがありません。\n");
				break;
			case -2:
				printf("模擬周波数ファイルが不正です。\n");
				break;
			case -3:
				printf("開始時刻の入力受付失敗しました。\n");
				break;
			case -4:
				printf("約定△kWの入力受付失敗しました。\n");
				break;
			case -5:
				printf("開始時刻を設定してください。\n");
				break;
			case -6:
				printf("開始時刻は未来の時刻を設定してください。\n");
				break;
			case -7:
				printf("約定△kWを設定してください。\n");
				break;
			case -8:
				printf("約定△kWは1000000以上で設定してください。\n");
				break;
			case -9:
				printf("模擬周波数ファイルがありません。\n");
				break;
			}
		}
		else
		{
			if( ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount == 0 )
			{
				printf("登録データなし\n");
			}
			else
			{
				printf("登録データ\n");
			}
		}
				
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("%s\n",errMsg);

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		
		if( dispMode == 0 )
		{
			if( errCode == 0 && startTime != 0 )
			{
				printf("開始時刻 %04d/%02d/%02d %02d:%02d:%02d    約定△kW %10d\n",
					s_stTime_tm.tm_year,
					s_stTime_tm.tm_mon,
					s_stTime_tm.tm_mday,
					s_stTime_tm.tm_hour,
					s_stTime_tm.tm_min,
					s_stTime_tm.tm_sec,
					s_fcr_freq_sim_data.fcrDltKw);
			}
			else
			{
				printf("開始時刻 ----/--/-- --:--:--    約定△kW ----------\n");
			}
		}
		else
		{
			if( ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount != 0 )
			{
				localtime_r( &ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_stTime_timeval.tv_sec, &work_tm );
				printf("開始時刻 %04d/%02d/%02d %02d:%02d:%02d    約定△kW %10d\n",
					work_tm.tm_year + 1900,
					work_tm.tm_mon + 1,
					work_tm.tm_mday,
					work_tm.tm_hour,
					work_tm.tm_min,
					work_tm.tm_sec,
					ps_g_EMCTLFIL->s_fcr_freq_sim_data.fcrDltKw);
			}
			else
			{
				printf("開始時刻 ----/--/-- --:--:--    約定△kW ----------\n");
			}
		}
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("--------------------------------------------------------------------------------\n");
		
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("No__開始時刻_______________継続時間_______周波数_______________________\n");

		if( dispMode == 0 )
		{
			for( loopCnt1 = offSet; (( loopCnt1 < s_fcr_freq_sim_data.simDataCount ) && ( (loopCnt1 - offSet) < outLimit )); loopCnt1++ )
			{
				/* カーソルの位置からその行の右端までクリア */
				printf("\033[0K");

				if( errCode == 0 )
				{
					if( startTime != 0 )
					{
						localtime_r( &s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].s_stTime_timeval.tv_sec, &work_tm );
						printf("%03d %04d/%02d/%02d %02d:%02d:%02d",
							loopCnt1 + 1,
							work_tm.tm_year + 1900,
							work_tm.tm_mon + 1,
							work_tm.tm_mday,
							work_tm.tm_hour,
							work_tm.tm_min,
							work_tm.tm_sec);
						printf("   |  %4d秒       %02d.%03dHz\n",
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].duration,
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue/1000,
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue%1000);
						/* カーソルの位置からその行の右端までクリア */
						printf("\033[0K");
					}
					else
					{
						printf("--- ----/--/-- --:--:--");
						printf("   |  %4d秒       %02d.%03dHz\n",
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].duration,
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue/1000,
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue%1000);
						/* カーソルの位置からその行の右端までクリア */
						printf("\033[0K");
					}
				}
				else
				{
					printf("--- ----/--/-- --:--:--");
					printf("   |  ----秒       --.---Hz\n");
					/* カーソルの位置からその行の右端までクリア */
					printf("\033[0K");
				}
			}
		}
		else
		{
			for( loopCnt1 = offSet; (( loopCnt1 < ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount ) && ( (loopCnt1 - offSet) < outLimit )); loopCnt1++ )
			{
				if( ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount != 0 )
				{
					localtime_r( &ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].s_stTime_timeval.tv_sec, &work_tm );
					printf("%03d %04d/%02d/%02d %02d:%02d:%02d",
						loopCnt1 + 1,
						work_tm.tm_year + 1900,
						work_tm.tm_mon + 1,
						work_tm.tm_mday,
						work_tm.tm_hour,
						work_tm.tm_min,
						work_tm.tm_sec);
					printf("   |  %4d秒       %02d.%03dHz\n",
						ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].duration,
						ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue/1000,
						ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue%1000);
					/* カーソルの位置からその行の右端までクリア */
					printf("\033[0K");
				}
				else
				{
					printf("--- ----/--/-- --:--:--");
					printf("   |  ----秒       --.---Hz\n");
					/* カーソルの位置からその行の右端までクリア */
					printf("\033[0K");
				}
			}
		}

		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		if( errCode != 0)
		{
			printf("戻る:[q] 表示切替:[m]                           削除:[d]   (%d秒更新)", viewTime/10);
		}
		else if( startTime == 0 )
		{
			printf("戻る:[q] 表示切替:[m] 開始時刻設定:[t]          削除:[d]   (%d秒更新)", viewTime/10);
		}
		else
		{
			printf("戻る:[q] 表示切替:[m] 開始時刻設定:[t] 登録:[s] 削除:[d]   (%d秒更新)", viewTime/10);
		}

		fflush(NULL);

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 読み込みエラーの場合は処理を抜ける */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は処理を抜ける */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が表示切替の場合は表示モードを切り替える */
			if( (inKeyData == 'm') || (inKeyData == 'M') )
			{
				dispMode = (dispMode + 1 ) % 2;
				/* 画面をクリア */
				printf("\033[2J");
			}

			/* 開始時刻の設定 */
			if( errCode == 0 && ( (inKeyData == 't') || (inKeyData == 'T' ) ) )
			{
				offSet = 0;
				/* 画面をクリア */
				printf("\033[2J");
				/* カーソルの位置を高さ０、横０に移動 */
				printf("\033[%d;%dH",0,0);
				printf("\n開始時刻を入力してください(数字12桁 yyyymmddhhmm)\n");
				fflush(NULL);
				inputErr = 0;
				memset(inputStr, 0, sizeof(inputStr));
				for(loopCnt3 = 0; loopCnt3 < 12; loopCnt3++)
				{
					if( signal_flg ) break;

					/* キーボード入力を待つ */
					readByte = read( 0, &inKeyData, 1 );
					if( readByte != 0 )
					{
						/* キー入力が終了の場合は処理を抜ける */
						if( (inKeyData == '1') || (inKeyData == '2') || (inKeyData == '3') || (inKeyData == '4') ||
							(inKeyData == '5') || (inKeyData == '6') || (inKeyData == '7') || (inKeyData == '8') ||
							(inKeyData == '9') || (inKeyData == '0') )
						{
							printf("%c",inKeyData);
							inputStr[loopCnt3] = inKeyData;
							fflush(NULL);
						}
						else
						{
							inputErr = 1;
							break;
						}
					}
					else
					{
						inputErr = 1;
						break;
					}
				}
				if( loopCnt3 == 12 && inputErr == 0 )
				{
					strncpy(workStr, &inputStr[0], 4);
					s_stTime_tm.tm_year = atoi(workStr);
					memset(workStr, 0, sizeof(workStr));
					strncpy(workStr, &inputStr[4], 2);
					s_stTime_tm.tm_mon = atoi(workStr);
					strncpy(workStr, &inputStr[6], 2);
					s_stTime_tm.tm_mday = atoi(workStr);
					strncpy(workStr, &inputStr[8], 2);
					s_stTime_tm.tm_hour = atoi(workStr);
					strncpy(workStr, &inputStr[10], 2);
					s_stTime_tm.tm_min = atoi(workStr);
					strncpy(workStr, "00", 2);
					s_stTime_tm.tm_sec = atoi(workStr);
					memset(&s_fcr_freq_sim_data.s_stTime_timeval, 0, sizeof(s_fcr_freq_sim_data.s_stTime_timeval));

					/* 開始時刻を秒で取得 */
					s_fcr_freq_sim_data.s_stTime_timeval.tv_sec = func_cmn_mktime(
						s_stTime_tm.tm_year, s_stTime_tm.tm_mon, s_stTime_tm.tm_mday,
						s_stTime_tm.tm_hour, s_stTime_tm.tm_min, s_stTime_tm.tm_sec );

					printf("\n約定△kWを入力してください(数字10桁以下)\n");
					fflush(NULL);

					memset(inputStr, 0, sizeof(inputStr));
					for(loopCnt3 = 0; loopCnt3 < 10; loopCnt3++)
					{
						if( signal_flg ) break;

						/* キーボード入力を待つ */
						readByte = read( 0, &inKeyData, 1 );
						if( readByte != 0 )
						{
							/* キー入力が終了の場合は処理を抜ける */
							if( (inKeyData == '1') || (inKeyData == '2') || (inKeyData == '3') || (inKeyData == '4') ||
								(inKeyData == '5') || (inKeyData == '6') || (inKeyData == '7') || (inKeyData == '8') ||
								(inKeyData == '9') || (inKeyData == '0') )
							{
								printf("%c",inKeyData);
								inputStr[loopCnt3] = inKeyData;
								fflush(NULL);
							}
							else if(inKeyData == '\n')
							{
								break;
							}
							else
							{
								inputErr = 1;
								break;
							}
						}
						else
						{
							inputErr = 1;
							break;
						}
						
					}
					if( loopCnt3<=10 && inputErr == 0 )
					{
						s_fcr_freq_sim_data.fcrDltKw = atoi(inputStr);
						workDuration = 0;
						for( loopCnt3 = 0; loopCnt3 < s_fcr_freq_sim_data.simDataCount; loopCnt3++ )
						{
							/* 経過時間を加算して各開始時間を算出 */
							s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].s_stTime_timeval.tv_sec =
							s_fcr_freq_sim_data.s_stTime_timeval.tv_sec + workDuration;
							workDuration += s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].duration;
						}
						msgCode = 4;
						startTime = 1;
					}
					else
					{
						msgCode = -4;
						memset(&s_stTime_tm, 0, sizeof(s_stTime_tm));
						memset(&s_fcr_freq_sim_data.s_stTime_timeval, 0, sizeof(s_fcr_freq_sim_data.s_stTime_timeval));
					}
				}
				else
				{
					msgCode = -3;
				}
				offSet = 0;
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* 模擬周波数の保存 */
			if( errCode == 0 && ( (inKeyData == 's') || (inKeyData == 'S' ) ) )
			{
				/* 積算秒を取得 */
				gettimeofday( &s_nowTime, NULL );
				if( s_fcr_freq_sim_data.s_stTime_timeval.tv_sec == 0)
				{
					msgCode = -5;
				}
				else if( s_nowTime.tv_sec > s_fcr_freq_sim_data.s_stTime_timeval.tv_sec )
				{
					msgCode = -6;
				}
				else if( s_fcr_freq_sim_data.fcrDltKw == 0 )
				{
					msgCode = -7;
				}
				else if( s_fcr_freq_sim_data.fcrDltKw < 1000000 )
				{
					msgCode = -8;
				}
				else
				{
					/* 画面をクリア */
					printf("\033[2J");
					/* カーソルの位置を高さ０、横０に移動 */
					printf("\033[%d;%dH",0,0);
					printf("\n模擬周波数を登録しますか？はい(y) いいえ(n)\n");
					fflush(NULL);
					if( signal_flg ) break;

					/* キーボード入力を待つ */
					readByte = read( 0, &inKeyData, 1 );
					if( readByte != 0 )
					{
						/* キー入力が終了の場合は処理を抜ける */
						if( (inKeyData == 'y') || (inKeyData == 'Y') )
						{
							memcpy( &ps_g_EMCTLFIL->s_fcr_freq_sim_data, &s_fcr_freq_sim_data, sizeof(s_fcr_freq_sim_data) );
#ifdef DEBUG_FCR_FREQ_SIM
							printf("simDataCount[%d] fcrDltKw[%d] ",
								ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount,
								ps_g_EMCTLFIL->s_fcr_freq_sim_data.fcrDltKw
							);
							localtime_r( &ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_stTime_timeval.tv_sec, &work_tm );
							printf("StartTime %04d/%02d/%02d %02d:%02d:%02d\n",
								work_tm.tm_year + 1900,
								work_tm.tm_mon + 1,
								work_tm.tm_mday,
								work_tm.tm_hour,
								work_tm.tm_min,
								work_tm.tm_sec);
							fflush(NULL);
							for( loopCnt3 = 0; loopCnt3< ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount; loopCnt3++ )
							{
								localtime_r( &ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].s_stTime_timeval.tv_sec, &work_tm );
								printf("%03d %04d/%02d/%02d %02d:%02d:%02d %d秒 %d.%03dHz tv_sec[%d]\n",
									loopCnt3 + 1,
									work_tm.tm_year + 1900,
									work_tm.tm_mon + 1,
									work_tm.tm_mday,
									work_tm.tm_hour,
									work_tm.tm_min,
									work_tm.tm_sec,
									ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].duration,
									ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].freqValue / 1000,
									ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].freqValue % 1000,
									ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt3].s_stTime_timeval.tv_sec
								);
								fflush(NULL);
							}
							/* キーボード入力を待つ */
							readByte = read( 0, &inKeyData, 1 );
#endif /* DEBUG_FCR_FREQ_SIM */
							msgCode = 1;
						}
						else
						{
							msgCode = 3;
						}
					}
				}
				offSet = 0;
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* 模擬周波数の削除 */
			if( (inKeyData == 'd') || (inKeyData == 'D' ) )
			{
				/* 画面をクリア */
				printf("\033[2J");
				/* カーソルの位置を高さ０、横０に移動 */
				printf("\033[%d;%dH",0,0);
				printf("\n模擬周波数を削除しますか？はい(y) いいえ(n)\n");
				fflush(NULL);
				if( signal_flg ) break;

				/* キーボード入力を待つ */
				readByte = read( 0, &inKeyData, 1 );
				if( readByte != 0 )
				{
					/* キー入力が終了の場合は処理を抜ける */
					if( (inKeyData == 'y') || (inKeyData == 'Y') )
					{
						memset( &ps_g_EMCTLFIL->s_fcr_freq_sim_data, 0, sizeof( ps_g_EMCTLFIL->s_fcr_freq_sim_data ) );
						msgCode = 2;
					}
					else
					{
						msgCode = 3;
					}
				}
				offSet = 0;
				/* 画面をクリア */
				printf("\033[2J");
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶ分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列をカンマで区切り、ps_sepStr に格納する。
 * 戻り値：
 * 		カンマで区切られたデー多数を返す。
 */
int func_sep_data( char *pc_data, struct st_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_sepStr, 0, sizeof(struct st_sepStr) );

	pt = pc_data;
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		if( *pt == ',' )
		{
			ps_sepStr->data[idx1][idx2] = 0;
			idx2 = 0;
			idx1++;
			continue;
		}
		ps_sepStr->data[idx1][idx2] = *pt;
		idx2++;
	}
	ps_sepStr->data[idx1][idx2] = 0;
//	if(idx2) idx1++;
	if((idx1==0)&&(idx2))
		 idx1++;
	else if(idx1>0)
		idx1++;

	return( idx1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * UNIXタイム返却
 *
 * 機能：
 * 		年月日からUNIXタイムを返却する
 * 戻り値：
 * 		UNIXタイム
 */
time_t func_cmn_mktime( int year, int month, int day, int hour, int min, int sec)
{
    time_t r_time;
    struct tm mk_time;

    memset(&mk_time, 0, sizeof(mk_time));

    mk_time.tm_year = year - 1900;  /* 西暦年 - 1900 */
    mk_time.tm_mon = month - 1;     /* 月 ( 1月＝0 ) */
    mk_time.tm_mday = day;      	/* 日 */
    mk_time.tm_hour = hour;     	/* 時 */
    mk_time.tm_min = min;       	/* 分 */
    mk_time.tm_sec = sec;      		/* 秒 */
    mk_time.tm_isdst = -1;    		/* サマータイムフラグ */

    r_time = mktime(&mk_time);

    return r_time;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力の最大出力値を設定
 *
 * 機能：
 * 		最大出力を約定△kW⇔定格出力で切り替える
 * 戻り値：
 * 		なし
 */
void func_display_freq_maxpow_set( void )
{
	struct tm					localTime;			/* 現在時刻 */
	time_t						nowTime;			/* 現在時刻 */
	char						inKeyData;			/* キー入力値 */
	char						readByte;			/* キー入力バイト数 */
	struct termio				ttyBackup;			/* 端末設定保存用 */
	struct termio				ttyChange;			/* 端末設定変更用 */
	struct winsize				termSize_OLD;		/* 端末サイズ保存用 */
	int							outLimit;			/* 表示可能行数 */
	int							setType;			/* 設定Type */

	/* 画面をクリア */
	printf("\033[2J");

	/* 端末のサイズを取得 */
	func_termsize_get();
	termSize_OLD.ws_row = termSize.ws_col;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 設定Typeの取得 */
	setType = ps_g_EMCTLFIL->fcrCalcDataType;
	/* 終了まで繰り返す */
	while( 1 )
	{
		/* 端末のサイズを取得 */
		func_termsize_get();
		outLimit = (termSize.ws_row - 12 );

		/* 端末のサイズ（高さ）が変更されている場合 */
		if(termSize_OLD.ws_col != termSize.ws_col )
		{
			/* 端末サイズを保存 */
			termSize_OLD.ws_col = termSize.ws_col;
			/* 画面をクリア */
			printf("\033[2J");
		}

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		/* 装置種別と現在時刻の表示 */
		printf("= <%s (一次調整力 最大出力)> =====================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			s_machineName[ 0 ].nameShort,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		printf("\033[0K");
		printf("------------------------------------------------------+--+----------------------\n");
		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");
		printf("出力演算に使用する最大出力の元となる情報    現在の設定     将来の設定\n");
		printf("\033[0K");
		printf("------------------------------------------------------+--+----------------------\n");
		printf("\033[0K");

		if( ps_g_EMCTLFIL->fcrCalcDataType == PV_FCR_CALC_DATA_TYPE_DLTPOW )
		{
			if( ps_g_EMCTLFIL->fcrCalcDataType == setType )
			{
				printf("一次調整力の出力算出に使用する最大値データ：約定△kW       変更なし\n");
			}
			else
			{
				printf("一次調整力の出力算出に使用する最大値データ：約定△kW   ->  定格出力\n");
			}
		}
		else
		{
			if( ps_g_EMCTLFIL->fcrCalcDataType == setType )
			{
				printf("一次調整力の出力算出に使用する最大値データ：定格出力       変更なし\n");
			}
			else
			{
				printf("一次調整力の出力算出に使用する最大値データ：定格出力   ->  約定△kW\n");
			}
		}

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("------------------------------------------------------+--+----------------------\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("================================================================================\n");

		/* カーソルの位置からその行の右端までクリア */
		printf("\033[0K");

		printf("戻る:[q] 設定変更:[c] 設定変更の確定:[w]                 (%d秒更新)", viewTime/10);

		fflush(NULL);
		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );
		if( readByte != 0 )
		{
			/* 読み込みエラーの場合は処理を抜ける */
			if( readByte < 0 ) break;

			/* キー入力が終了の場合は処理を抜ける */
			if( (inKeyData == 'q') || (inKeyData == 'Q') ) break;

			/* キー入力が設定変更の場合は、設定を切り替える */
			if( (inKeyData == 'c') || (inKeyData == 'C') )
			{
				setType = (setType + 1 ) % 2;
				/* 画面をクリア */
				printf("\033[2J");
			}

			/* キー入力が確定の場合は、設定を更新する */
			if( (inKeyData == 'w') || (inKeyData == 'W') )
			{
				if( ps_g_EMCTLFIL->fcrCalcDataType != setType )
				{
					ps_g_EMCTLFIL->fcrCalcDataType = setType;
					/* 画面をクリア */
					printf("\033[2J");
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
}
/* [20250807] Add Ed */

