/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20210802] 2021.08.02
 * ・JET認証試験用 固定スケジュール機能
 * [20210730] 2021.07.30
 * ・模擬サーバエビデンス用一括表示追加
 * [20210607] 2021.06.07
 * ・SSLデータ出力有無初期設定チェック追加
 * [20210330] 2021.03.30
 * ・特高CDT出力制御対応(東北電力)
 * [20190809] 2019.08.09
 * ・スケジュール配信サーバの通信ログ提出用のスケジュールデータ解析機能をメニュー(23)に追加
 * [20181116] 2018.11.16
 * ・スケジュールバッファ面を切り替える機能をメニュー(73)に追加
 * ・更新スケジュール作成時における次回アクセス日時のオフセットを秒単位に変更
 * [20180605] 2018.06.05
 * ・固定スケジュールの更新フラグを変更する機能をメニュー(72）に追加
 * [20180604] 2018.06.04
 * ・SSLの通信データをファイルに出力するデバッグ機能をメニュー(22)に追加
 *   スケジュールファイルダウンロード(21)の通信データをファイルに出力
 * [20170411] 2017.04.11
 * ・ＭＡＣアドレス、配信サーバＵＲＬの変更機能を追加
 * ・スケジュールファイル反映機能にＩＤ登録結果も対応するように修正
 * [20170310] 初版
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * スケジュールファイル設定ツール
 *
 * 機能：
 * 	以下の機能を提供する。
 *	・スケジュールファイルを収集装置へ設定する
 *	・スケジュールファイルを解析する
 *	
 */
#include <sys/types.h>

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <termio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvudp.h"
#include "pvcomm.h"

#define	OWN_VERSION	"1.00"							/* バージョン  */

#define	OWN_PROC_NAME	"SCHDEBUG"					/* 自プロセス名 */

#define	PV_SYSDEBUG_PCS_CTL	0						/* パワーコンディショナの制御機能を実装した場合に1を設定 */

#define PV_CWD_PATH_SIZE			512
#define PV_SCH_TI_MIN				30
#define PV_SCH_TI_SEC				(PV_SCH_TI_MIN*60)
#define PV_UP_SCH_RTY_MIN_1ST		5		/* 更新スケジュールリトライ時間(初期起動時) */
#define PV_UP_SCH_RTY_MIN			30		/* 更新スケジュールリトライ時間(通常時) */
#define PV_SCH_FMT_NO_ID			301		/* フォーマットNo（ID登録結果） */
#define PV_SCH_FMT_NO_YEAR			201		/* フォーマットNo（年間） */
#define PV_SCH_FMT_NO_MONTH			202		/* フォーマットNo（月間） */
#define PV_SCH_FMT_NO_UPDATE		203		/* フォーマットNo（更新） */
#define PV_SCH_FMT_NO_ERR			999		/* フォーマットNo（更新） */
#define	PV_ERR_PATTERN				11
#define PV_GET_YEAR_DAYS(year)		( 365 + ((year % 400)==0 ? 1 : ((year % 100)==0 ? 0 : ((year % 4)==0 ? 1 : 0))))
#define	PV_INPUT_MSG				"キーを入力して下さい。"
#define	PV_INPUT_ANYKEY_MSG			"任意のキー押下でメニューに戻ります"
#define	PV_INPUT_ONLY				"INPUTONLY"
#define	PV_SSL_READ_SIZE			1024

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_PGCONINF		*ps_g_PGCONINF;				/* 電力会社向け情報ファイル */
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL;			/* スケジュール管理情報ファイル */
struct st_fileIndex     s_fileIndex[] =				/* ファイル定義テーブル（必須ファイル） */
{
    {   PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
    {   PV_FILENO_PGCONINF, (void **)&ps_g_PGCONINF, 0, 0, 0 },
    {   PV_FILENO_SCHCTLFIL,(void **)&ps_g_SCHCTLFIL,0, 0, 0 },
    {   0,                                     NULL, 0, 0, 0 }
};

struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};

struct st_blockdata{
	char				numByte[6];
	char				data[20065];		// 6+(10+26+12+5+1488+2)*13
	char				chksumByte[2];
}s_blockdata;

struct st_datptn{
	char				num;
	char				data[256];
/* [20210802] Add St */
	char				dataEnd;
	char				montCnt;
/* [20210802] Add Ed */
}s_datptn;


struct st_SCHCONF {
	char				plantId[27];				/*  */
	char				yobi1;						/*  */
	char				updateFlg;					/*  */
	char				checksum;					/*  */
	char				yobi2;						/*  */
	int					nextDateOffset;				/*  */
	char				saveDir[128];				/*  */
	char				yobi3[3];					/*  */
	char				setType;					/*  */
	struct st_datptn	dataPtn[2];					/*  */
};


struct st_PGCONINF		g_PGCONINF;				/* 電力会社向け情報ファイル */

char c_tmKindName[4][16] = { "----", "平均", "積算", "直値" };

													/* ファイル定義テーブル */
struct st_fileIndex		s_fileIndex2[PV_MAX_MKIND];

struct winsize			termSize;					/* 端末の画面サイズ */

static int				mainte_flg = 0;				/* モニタ専用フラグ */
static int				pvctl_flg = 0;				/* 制御機能ありフラグ */
static int				pvsch_flg = 0;				/* スケジュール管理機能ありフラグ */
static int				pvsch_dat = 0;

static int				signal_flg = 0;				/* シグナル受信フラグ */

struct tm					g_timeData;					/* 時刻情報 */
time_t						g_Time;					/* 現在積算秒 */
char						g_timeStr[15];			/**/
int							g_blockbyte;
struct st_blockdata			g_blokdata;
char						g_binblockdata[1560];
static char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static char 			schFileErr[14][128];		/* スケジュールファイルエラー表示 */
static char				g_curDir[PV_CWD_PATH_SIZE];
struct st_SCHCONF			g_st_SCHCONF;				/* ポインタ */
struct st_SCHCONF			*g_ps_SCHCONF;				/* ポインタ */
static int					g_patten2Cnt;
static const char			strType[3][128] = {"ランダム", "１日","任意"};
static struct tm 			g_data_next;
static char  			sslBuff[PV_SSL_READ_SIZE];
static char  			*p_sslBuff;
static int   			sslReadSize;
static SSL   			*ssl;

struct st_block_data{
	struct tm 	timeData;
	char		schId[11];
	char		plantId[27];
	char		regist;						/*  */
	char		checkSum;
	short		dataNum;
	char		data[1488];
};

struct st_sch_data{
	char					fileName[128];
	short					fileType;
	char					updateFlg;
	struct tm 				nextTime;
	char					blockNum;
	struct st_block_data	blockData[13];
};

struct st_sch_data		schData;
char				g_plantId[27];				/*  */
char 				g_SchFileName[64];


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
void	func_line_id_get( int, int, int *, int * );
void	func_display_machineinfo( int, int );
void	func_display_proc();
void	func_display_line();
void	func_display_pvsvrinf();
void	func_display_pgconinf();
void	func_display_tm( int );
void	func_display_unit( short );
void	func_display_sv( int );
void	func_display_st( int );
void	func_display_calctminfo( );
void	func_display_pvdspinfo( );
void	func_display_anothersnd( );
void	func_display_logsetting( );
void	func_display_createtmfile( );
void	func_display_timesetting( );
void	func_display_pvctlinfo();
void	func_display_pvschinfo();
void	func_display_pvctlruninfo();
void	func_johenmsg_out( int );
void	func_syslog_out( int );
void	func_termsize_get();
void	func_displaysize_set();
void	func_pvmode_change();
int 	func_display_pvschdays( int, int, int *, int *, int * );
int		func_display_get_days( int, int );
int func_schdata_set( void );
int func_cmn_FileFormatCheck2( const char *, int );
int func_cmn_set_tim( void );

/* 外部関数定義 */
extern	long long commfunc_pow10abs( short );
int pvct_cmn_str2byte(char *strData, char *byteData, char strLen);
void func_cmn_set_data_str( struct tm *timData, char *setStr, char setByte );
void func_cmn_create_data(struct tm *sTim, int dataNum, char checkSum);
int pvct_cmn_num2byte(int num, int dataNum, char *strData);
int pvct_cmn_byte2str(char *byteData, char *strData, int byteNum);
int func_cmn_offset_sec( struct tm *sTim, int tm_year, int tm_month );
int func_cmn_FileFormatCheck1( const char *FilePath, int type);
int func_cmn_bin2Num(char *strData, int byteNum);
int func_cmn_time_check( struct tm *timData );
int func_schfile_data_set(int buffSelect, int dataPos, int dataNum, unsigned char *ptr_data, int fmtNo );
int func_schfile_data_check(struct tm *timeData, int dataNum, unsigned char *ptr_data, int fmtNo );
int	func_cmn_string_check( char *strPoint, char strlength );
int func_cmn_get_data_pos( short year, char month, char day, char hour, char min);
time_t func_cmn_mktime( short year, char month, char day, char hour, char min, char sec);
void func_plantid_set( void );
void func_macaddr_set( void );
void func_serverurl_set( void );
int func_schfile_analize( int skipFlg, char *filePath );
int func_schfile_set( void );
void func_display_pvschinfo2( int fileType);
void func_idregist_file( void );
void func_fixed_schedule_year( void );
void func_fixed_schedule_month( void );
void func_update_schedule( void );
void func_schedule_error( void );
void func_cmn_KeyInput( char *msg );
void func_cmn_set_blockdata( int type, struct tm *sTim, int dataNum, char checkSum);
int func_updateflg_get( void );
void func_data_pattern_set( void );
int func_data_pattern_check( void );
void func_data_pattern_view( void );
int func_sep_data( char *pc_data, struct st_sepStr *ps_sepStr );
int func_ssl_readmain( char *fileType );
int func_ssl_read( char **p_restBuff );
int func_ssl_fgets( char **buff );
int func_ssl_getStatus( void );
int func_ssl_getResHeader( void );
int func_ssl_getResBody( void );
int func_ssl_filedownload( void );
int func_schFileDownload( void );
int func_ssl_commdata_fileout( void );
int func_schfile_report( void );
int func_chg_updateFlg( void );
int func_chg_updateTime( void );
int func_chg_buffSelect( void );
int func_chg_buffYear( void );
/* 内部関数定義 */
int	func_PGCONINF_Set( );
int	func_cmn_string_check2( char *, char );
int	func_cmn_get_mac_address( char *strPoint );
int func_ssl_data_file_write( char *preFix, char *p_buff, int length );
int func_ssl_data_file_close( void );


static char					g_keyData[1024];
static int					g_keyByte;
static FILE					*sslfp=NULL;				/* ファイルディスクリプタ */
static int					ssl_comm_output=0;


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
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
	int						pvmanagePid;
	int						mutexCreate = 0;
	uid_t					user_id;
	char					datFilePath[1024];			/* DATファイルパス */
	char					tmpFile[1024];				/* 設定ファイルパス *//* [20210607] Add */

	struct sigaction		sigact;

	memset( &sigact, 0, sizeof(sigact) );
	sigemptyset( &sigact.sa_mask );
	sigaddset( &sigact.sa_mask, SIGUSR1 );
	sigact.sa_handler  = func_signal_recv;
	sigact.sa_flags    = SA_RESETHAND;
	sigact.sa_restorer = 0;


	if( sigaction( SIGUSR1, &sigact, NULL ) != 0 )
	{
		perror("SYSDEBUG: sigacion");
		exit( 1 );
	}
	

	/* 起動ユーザＩＤを判定 */
	user_id = getuid();
	/* root または nksystem, nkuser, pvsystem ユーザ以外の場合 */
	if( (user_id != 0) && (user_id != 500) && (user_id != 501) && (user_id != 502) )
	{
		/* メンテナンス専用ユーザフラグＯＮ */
		mainte_flg = 1;
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

	memset(&g_st_SCHCONF, 0, sizeof(g_st_SCHCONF));
	g_ps_SCHCONF = &g_st_SCHCONF;

	/* 使用ファイルをオープンしてＭＭＡＰ化 */
	retCode = commfunc_file_open( s_fileIndex );
	if(retCode < 0)
	{
		printf("%s : File Resource Open Error!\n", OWN_PROC_NAME );
		if( func_PGCONINF_Set() < 0 )
		{
			memset(&g_PGCONINF, 0x00, sizeof(g_PGCONINF));
		}
		else
		{
			memcpy( g_ps_SCHCONF->plantId, g_PGCONINF.plantId, sizeof(g_ps_SCHCONF->plantId));
		}
	}
	else
	{
		pvsch_dat = 1;
		memcpy(&g_PGCONINF, ps_g_PGCONINF, sizeof(g_PGCONINF));

		/* ファイルのオープンに成功した場合 */
		if( ps_g_SCHCTLFIL->plantId[0] != 0 )
		{
			memcpy( g_ps_SCHCONF->plantId, ps_g_SCHCTLFIL->plantId, sizeof(g_plantId));
		}
		else if( g_PGCONINF.plantId[0] != 0 )
		{
			memcpy( g_ps_SCHCONF->plantId, g_PGCONINF.plantId, sizeof(g_plantId));
		}
		
		
	}

	memset(g_curDir, 0, sizeof(g_curDir));
	getcwd(g_curDir, PV_CWD_PATH_SIZE);
	g_curDir[strlen(g_curDir)]='/';

	g_ps_SCHCONF->setType = 2;
	g_ps_SCHCONF->dataPtn[1].num = 7;
	g_ps_SCHCONF->dataPtn[1].data[0] = 100;
	g_ps_SCHCONF->dataPtn[1].data[1] = 0;
	g_ps_SCHCONF->dataPtn[1].data[2] = 50;
	g_ps_SCHCONF->dataPtn[1].data[3] = 100;
	g_ps_SCHCONF->dataPtn[1].data[4] = 50;
	g_ps_SCHCONF->dataPtn[1].data[5] = 0;
	g_ps_SCHCONF->dataPtn[1].data[6] = 100;
	g_ps_SCHCONF->dataPtn[1].dataEnd = -1;	/* [20210802] Add */

/* [20210607] Add St */
	/* SSLデータ出力設定ファイルがある場合 */
	sprintf( tmpFile, "%sSSL_FILE_OUT", PV_PATH_CSV );
	if( access( tmpFile, R_OK | W_OK ) == 0 )
	{
		/* ファイル出力をONにする */
		ssl_comm_output = 1;
	}
/* [20210607] Add Ed */


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 画面をクリア */
		printf("\033[2J");
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);
	
		printf("= 収集装置スケジュール管理ツール ============================== [Version:%s] =\n", OWN_VERSION );
		printf("--------------------------------------------------------------------------------\n");
		printf("   1.ＩＤ登録結果\n");
		printf("   2.更新スケジュール作成\n");
		printf("   3.固定スケジュール（月間）作成\n");
		printf("   4.固定スケジュール（年間）作成\n");
		printf("   5.エラーファイル作成\n");
		printf("--------------------------------------------------------------------------------\n");
		printf("  10.発電所ＩＤ設定\n");
/* [20170411] Add St */
		printf("  11.ＭＡＣアドレス設定\n");
		printf("  12.配信サーバＵＲＬ設定\n");
/* [20170411] Add Ed */
		printf("  15.データパターン設定\n");
		printf("--------------------------------------------------------------------------------\n");
		printf("  20.スケジュールファイル表示\n");
		printf("  21.スケジュールファイルダウンロード\n");
		printf("  22.SSL通信データ出力設定\n");
		printf("  23.スケジュール配信サーバ通信レポート提出用ファイル解析\n");
		printf("--------------------------------------------------------------------------------\n");
		if( pvsch_dat )
		{
			printf("  70.スケジュールファイル反映\n");
			printf("  71.スケジュール管理情報表示\n");
			printf("  72.固定スケジュール更新フラグ変更\n");
			printf("  73.更新スケジュール次回アクセス日時変更\n");
			printf("  74.スケジュールバッファ面切替（不具合確認用）\n");
			printf("  75.(欠番)スケジュールバッファ面(年)強制変更\n");
			printf("  76.スケジュール設定\n");
			printf("--------------------------------------------------------------------------------\n");
		}
		printf("  99.終了\n");
		printf("================================================================================\n");
		printf("番号を選択してください（改行のみの場合はメニューを再表示） >> ");
		fflush( NULL );

		if( signal_flg ) break;

		memset( keyData, 0, sizeof( keyData ) );
		keyByte = read( 0, &keyData, 1024 );

		if(keyByte < 0 )
			/* ツールを終了 */
			break;

		/* 入力値を数値化 */
		menuNumb = atoi( keyData );

		if (menuNumb == 99 )
		{
			/* ツールを終了 */
			break;
		}
		else if( menuNumb == 1 )
		{
			func_idregist_file();
		}
		else if( menuNumb == 2 )
		{
			func_update_schedule();
		}
		else if( menuNumb == 3 )
		{
			func_fixed_schedule_month();
		}
		else if( menuNumb == 4 )
		{
			func_fixed_schedule_year();
		}
		else if( menuNumb == 5 )
		{
			func_schedule_error();
		}
		else if( menuNumb == 10 )
		{
			func_plantid_set();
		}
/* [20170411] Add St */
		else if( menuNumb == 11 )
		{
			func_macaddr_set();
		}
		else if( menuNumb == 12 )
		{
			func_serverurl_set();
		}
/* [20170411] Add Ed */
		else if( menuNumb == 15 )
		{
			func_data_pattern_set();
		}
		else if( menuNumb == 20 )
		{
			func_schfile_analize( 0, NULL);
		}
		else if( menuNumb == 21 )
		{
			func_schFileDownload();
		}
		else if( menuNumb == 22 )
		{
			func_ssl_commdata_fileout();
		}
/* [20190809] Add St */
		else if( menuNumb == 23 )
		{
			func_schfile_report();
		}
/* [20190809] Add Ed */
		else if( ( menuNumb == 70 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			func_schfile_set();
		}
		else if( ( menuNumb == 71 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			/* スケジュール情報表示 */
			func_display_pvschinfo( );
		}
		else if( ( menuNumb == 72 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			func_chg_updateFlg();
		}
		else if( ( menuNumb == 73 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			func_chg_updateTime();
		}
		else if( ( menuNumb == 74 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			func_chg_buffSelect();
		}
		else if( ( menuNumb == 75 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			//func_chg_buffYear();
		}
		else if( ( menuNumb == 76 ) && ( pvsch_dat ) && ( mainte_flg == 0 ) )
		{
			func_schdata_set();
		}
		else if( menuNumb == 81 )
		{
			/* 画面サイズ設定 */
			func_displaysize_set( );
		}
	}
	
	/* ミューテックス領域を生成していた場合 */
	if( mutexCreate )
	{
		/* ミューテックス領域を削除 */
		commfunc_mutex_destroy();
	}

	if( signal_flg )
	{
		/* 終了メッセージ表示 */
		printf("\n\n");
		printf("==========================================\n");
		printf(" 収集装置の停止または再起動処理を行うため \n");
		printf(" オンラインモニタを強制的に終了しました \n");
		printf("==========================================\n");
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
	ttyChange.c_cc[VTIME] = 50;
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
			/* 見出しの表示 */
			printf("\033[0K");	printf("ダウンロード情報 ---------------------------------------------------------------\n");
			printf("\033[0K");	printf("  発電所ＩＤ                      ：%s\n", ps_g_SCHCTLFIL->plantId);
			printf("\033[0K");	printf("  出力変化時間                    ：%d分\n", ps_g_SCHCTLFIL->chgTime);
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
			timeSpanS = schAcsTime[ps_g_SCHCTLFIL->chkSumNumb];
			timeSpanE = timeSpanS + (20*60)-1;
			printf("\033[0K");	printf("  固定スケジュール要求時間帯      ：%02d:%02d:%02d～%02d:%02d:%02d\n", 
				timeSpanS/3600, (timeSpanS%3600)/60, (timeSpanS%3600)%60, timeSpanE/3600, (timeSpanE%3600)/60, (timeSpanE%3600)%60);

			timeSpanS = ntpAcsTime[ps_g_SCHCTLFIL->chkSumNumb];
			timeSpanE = timeSpanS + (10*60)-1;
			printf("\033[0K");	printf("  時刻同期実施時間帯              ：%02d:%02d:%02d～%02d:%02d:%02d\n", 
				timeSpanS/3600, (timeSpanS%3600)/60, (timeSpanS%3600)%60, timeSpanE/3600, (timeSpanE%3600)/60, (timeSpanE%3600)%60);
			
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
				printf("    時刻  種別 指令値(％) |    時刻  種別 指令値(％) |    時刻  種別 指令値(％) \n");
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
							printf("   --   --------  ");
						}
						else
						{
							printf("   %s   %-10d", (p_schdata->schType==PV_SCH_TYPE_UPDATE ? "更" : "固"), p_schdata->data);
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
				temp = ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin;
				memset(&ps_g_SCHCTLFIL->s_dwlinfo, 0, sizeof(struct st_dwlinfo));
				ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = 0xFF;
				ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = 0xFF;
				ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = temp;

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


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフォーマットチェック
 *
 * 機能：
 * 		スケジュールファイルのフォーマットをチェックする
 * 戻り値：
 * 		 0：正常
 * 		-1：異常(システム異常)
 * 		-2：異常(ファイルサイズ異常)
 * 		-3：異常(発電所ＩＤ不正、チェックサム異常)
 */
int func_cmn_FileFormatCheck1( const char *FilePath, int fmtNo)
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						dataBlock;
	int						fileSize;
	int						checkSize=0;
	unsigned  char			buff[20065+1];
	int						loopCnt;
	int						loopCnt2;
	unsigned  char			*p_buff;
	char					plantId[27];
	int						dataNum;
	int						checksum;
	int						updateFlg;
	int						checksumNum;
	int						retCode = 0;
	int						blockErr;
	unsigned  char			*ptr_data;
	struct tm				timeData;		/* 時刻情報 */
	struct tm				nextTimeData;		/* 時刻情報 */
	time_t					upSchReqTime;

	//printf("START\n");

	//printf("FilePath=[%s]\n", FilePath);

	/* 対象スケジュールファイルオープン */
	if( (fp = fopen(FilePath, "rb")) == NULL)
	{
		sprintf( schFileErr[0], "ファイルのオープンに失敗しました。");
		//sprintf( errMsg, "File[%s] Open error", FilePath );
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//printf("%s\n", errMsg);
		return( -1 );
	}

	/* ファイル読み込み */
	fileSize = fread( buff, sizeof( unsigned char ), sizeof(buff), fp );

	/* 最低限のファイルサイズの確認 */
	/* データ部のデータ数: 6 byte */
	if( fileSize <= 6 )
	{
		sprintf( schFileErr[0], "ファイルの読み込みに失敗しました。");
		//sprintf( errMsg, "file is not enough size[%s:%d]", FilePath, fileSize);
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//printf("%s\n", errMsg);
		fclose(fp);
		return( -2 );
	}


	p_buff = &buff[0];

	/* データ部のデータ数を取得 */
	dataBlock = func_cmn_bin2Num(p_buff, 6);
	p_buff+=6;
	checkSize +=6;
	//printf("dataBlock=[%d]\n", dataBlock);

/* [20170411] Add St */
	if( fmtNo == PV_SCH_FMT_NO_ID )
	{
		/* 発電所IDチェック */
		memset(plantId, 0, sizeof(plantId));
		for( loopCnt2=0; loopCnt2 < 26; loopCnt2++ )
		{
			sprintf(&plantId[loopCnt2], "%d", *p_buff);
			p_buff++;
		}
		/* 発電所IDのチェック */
		if( strncmp( &plantId[0], ps_g_PGCONINF->plantId, 26 ) != 0 )
		{
			sprintf( schFileErr[0], "異なる発電所ＩＤです。（%s）", plantId);
			retCode = -1;
		}
		else
		{
			if( *p_buff == 0 )
			{
				/* ID登録済みに変更する */
				ps_g_SCHCTLFIL->idRegist = 1;
			}
			else
			{
				/* ID未登録に変更する */
				ps_g_SCHCTLFIL->idRegist = 0;
			}
			retCode = 0;
		}
	}
/* [20170411] Add Ed */
	else
	{
		for( loopCnt=0; loopCnt < dataBlock; loopCnt++)
		{
			sprintf( schFileErr[loopCnt+1], "ファイルのデータが不足しています。");
			/* ブロック毎のエラー情報を初期化 */
			blockErr = 0;

			/* 固定オフセット（53 byte） */
			/* ┣スケジュールID    :10 byte */
			/* ┣発電所ID          :26 byte */
			/* ┣制御年月日        :12 byte */
			/* ┗出力制御率データ数: 5 byte */
			checkSize += 53;
			if( fileSize < checkSize )
			{
				//sprintf( errMsg, "DataBlock[%d] is not enough1 data size.[%d]<[%d]", loopCnt+1, fileSize, checkSize);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				retCode = -2;
				break;
			}

			/* スケジュールＩＤは意識しない */
			p_buff+=10;		/* スケジュールID */

			/* 発電所IDチェック */
			memset(plantId, 0, sizeof(plantId));
			for( loopCnt2=0; loopCnt2 < 26; loopCnt2++ )
			{
				sprintf(&plantId[loopCnt2], "%d", *p_buff);
				p_buff++;
			}
			/* 発電所IDのチェック */
			if( strncmp( &plantId[0], ps_g_PGCONINF->plantId, 26 ) != 0 )
			{
				sprintf( schFileErr[loopCnt+1], "異なる発電所ＩＤです。（%s）", plantId);
				//sprintf( errMsg, "DataBlock[%d] is plantId different=[%s]", loopCnt+1, plantId);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				//blockErr = -1;
			}

			/* 制御年月日を取得 */
			timeData.tm_year = func_cmn_bin2Num(p_buff, 4);		p_buff+=4;
			timeData.tm_mon  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_mday = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_hour = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_min  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_sec  = 0;

			/* 確認ファイル名の生成日時のチェック */
			if( func_cmn_time_check(&timeData) < 0 )
			{
				sprintf( schFileErr[loopCnt+1], "制御年月日が正しくありません。（%04d/%02d/%02d %02d:%02d %02d）", 
							timeData.tm_year, timeData.tm_mon, timeData.tm_mday,
							timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
				//sprintf( errMsg, "schedule date error [%04d/%02d/%02d %02d:%02d %02d]",
				//			timeData.tm_year, timeData.tm_mon, timeData.tm_mday,
				//			timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				blockErr = -2;
			}


			/* 制御データ数を取得 */
			dataNum = func_cmn_bin2Num(p_buff, 5);
			p_buff+=5;

			/* 出力データ以降のファイルサイズチェック */
			/* ┣データ部          :dataNum byte */
			/* ┣チェックサム      : 2 byte */
			/* ┣更新フラグ      ※: 1 byte */
			/* ┗次回アクセス日時※:14 byte */
			/*  ※更新スケジュールのみ*/
			checkSize += dataNum + 2;
			if( fmtNo == PV_SCH_FMT_NO_UPDATE )
			{
				checkSize += 15;
			}
			if( fileSize < checkSize )
			{
				sprintf( schFileErr[loopCnt+1], "DataBlock[%d] is not enough2 data size.[%d]<[%d]", loopCnt+1, fileSize, checkSize);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				retCode = -2;
				break;
			}

			//printf("DataStartTime=[%04d%02d%02d %02d:%02d], DataNum=[%d]\n", 
			//	timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, dataNum);

			/* 出力データのチェックサムを算出 */
			ptr_data = p_buff;
			checksum = 0;
			for( loopCnt2=0; loopCnt2 < dataNum; loopCnt2++ )
			{
				checksum += *p_buff;
				p_buff++;
			}

			/* 更新スケジュールの場合は、更新フラグを取得 */
			if( fmtNo == PV_SCH_FMT_NO_UPDATE )
			{
				updateFlg = *p_buff;
				p_buff++;
			}

			/* チェックサム取得 */
			checksumNum = func_cmn_bin2Num(p_buff, 2);
			p_buff+=2;

			/* 出力データのチェックサムを判定 */
			if( ( checksum % (timeData.tm_mon + timeData.tm_mday)) != checksumNum )
			{
				sprintf( schFileErr[loopCnt+1], "チェックサムが異常です。（期待：[%d] != 格納：[%d]）", 
					checksumNum,  (checksum % (timeData.tm_mon + timeData.tm_mday)));
				//sprintf( errMsg, "check sum different [%d] != [%d], date[%04d/%02d/%02d %02d:%02d]", 
				//	checksumNum,  (checksum % (timeData.tm_mon + timeData.tm_mday)), 
				//	timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				blockErr = -3;
			}

			/* 更新スケジュールの場合は、次回アクセス日時を取得 */
			if( fmtNo == PV_SCH_FMT_NO_UPDATE )
			{
				/* 次回アクセス日時を取得 */
				nextTimeData.tm_year = func_cmn_bin2Num(p_buff, 4);		p_buff+=4;
				nextTimeData.tm_mon  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_mday = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_hour = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_min  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_sec  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;

				/* 確認ファイル名の生成日時のチェック */
				if( func_cmn_time_check(&nextTimeData) < 0 )
				{
					sprintf( schFileErr[loopCnt+1], "次回アクセス日時が正しくありません。（%04d/%02d/%02d %02d:%02d %02d）", 
								nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday,
								nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
					//sprintf( errMsg, "update time error [%04d/%02d/%02d %02d:%02d %02d]",
					//			nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday,
					//			nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
					//commfunc_syslog_msgset( pv_own_name, errMsg );
					//printf("%s\n", errMsg);
					blockErr = -2;
				}
				else
				{
					/* 時刻取得共通関数 */
					upSchReqTime = func_cmn_mktime(nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday, 
												nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
				
				}
			}

			/* ブロックのフォーマットチェック  */
			if( blockErr != 0 )
			{
				/* 戻り値にエラーを設定 */
				retCode = -3;
			}
			else
			{
				sprintf( schFileErr[loopCnt+1], "正常データです。");
				/* 正常の場合は、スケジュールデータを反映する */
				func_schfile_data_check(&timeData, dataNum, ptr_data, fmtNo);
			}
		}
	}

	/* 更新スケジュールは、次回アクセス日時と更新フラグを反映する */
	if( ( retCode == 0 ) && ( fmtNo == PV_SCH_FMT_NO_UPDATE ) )
	{
		//printf("updateFlg=[%d]\n", updateFlg);
		//printf("NextAccessTime=[%04d%02d%02d %02d:%02d], DataNum=[%d]\n", 
		//	nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday, nextTimeData.tm_hour, nextTimeData.tm_min, dataNum);
		ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime = upSchReqTime;
		ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = updateFlg;
	}

	fclose(fp);

	//printf("END\n");
	
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス初期化処理
 *
 * 機能：
 * 		プロセスの初期化処理を行う。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int func_cmn_bin2Num(char *strData, int byteNum)
{
	int						loopCnt;
	int						dataNum;
	int						powNum = 1;

	dataNum=0;
	for( loopCnt = byteNum-1; loopCnt >= 0; loopCnt-- )
	{
		dataNum += (strData[loopCnt] * powNum);
		powNum *= 10;
	}
	return dataNum;
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
time_t func_cmn_mktime( short year, char month, char day, char hour, char min, char sec)
{
    time_t r_time;
    struct tm mk_time;

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
 * 指定時刻のデータ格納位置を返却
 *
 * 機能：
 * 		指定した年に対して、月日時分に対する格納位置を返却する
 * 戻り値：
 * 		データ位置
 */
int func_cmn_get_data_pos( short year, char month, char day, char hour, char min)
{
    time_t 	s_time;
    time_t 	e_time;
    int	   	diff_time;
    int		data_num;

	s_time = func_cmn_mktime(year, 1, 1, 0, 0, 0);
	e_time = func_cmn_mktime(year, month, day, hour, min, 0);

	diff_time = e_time - s_time;
	data_num = (int)(diff_time/PV_SCH_TI_SEC);

    return data_num;
}



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 文字列判定処理
 *
 * 機能：
 * 		引数 strPointer で指定されたアドレスに格納されている文字列を確認し、内
 *		容毎に以下の値を返す。
 * 戻り値：
 *		 0x0001：数字が含まれている場合
 *		 0x0002：ピリオドが含まれている場合
 *		 0x0004：ハイフン又は、アンダースコアが含まれている場合
 *		 0x0008：小文字アルファベットが含まれている場合
 *		 0x0010：大文字アルファベットが含まれている場合
 *		 0x0100：アルファベットが16進数（A-F,a-f）以外が含まれている場合
 *		-1：上記以外の文字（空白、記号、タブ、全角など）が含まれている場合
 *		    または文字列長が引数（strlenMax）による指定文字以上の場合
 */
int	func_cmn_string_check( char *strPoint, char strlength )
{
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */

	/* 文字列長がstrlength以外の場合は異常	*/
	if( strnlen( strPoint, strlength+1 ) < strlength )
		return( -1 );

	/* 文字列の最後まで確認する */
	for( loopCnt = 0 ; loopCnt < strlength ; loopCnt++ )
	{
		/* ハイフンまたはアンダースコアの場合 */
		if( (*(strPoint + loopCnt) == '-') || (*(strPoint + loopCnt) == '_') )
		{
			retCode |= 0x0004;
		}
		/* アルファベット（小文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'a') && (*(strPoint + loopCnt) <= 'z') )
		{
			retCode |= 0x0008;
			/* 16進数の文字以外の場合 */
			if(*(strPoint + loopCnt) > 'f')
			{
				retCode |= 0x1000;
			}
		}
		/* アルファベット（大文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'A') && (*(strPoint + loopCnt) <= 'Z') )
		{
			retCode |= 0x0010;
			/* 16進数の文字以外の場合 */
			if(*(strPoint + loopCnt) > 'F')
			{
				retCode |= 0x1000;
			}
		}
		/* 数値の場合 */
		else if( (*(strPoint + loopCnt) >= '0') && (*(strPoint + loopCnt) <= '9') )
		{
			retCode |= 0x0001;
		}
		/* ピリオドの場合 */
		else if((*(strPoint + loopCnt) == '.'))
		{
			retCode |= 0x0002;
		}
		else
		{
			return( -1 );
		}
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイルのデータを反映する
 *
 * 機能：
 * 		スケジュールファイルのデータを管理情報に反映する
 * 戻り値：
 * 		-1：全て範囲外
 * 		 0：全て範囲内
 * 		 1：一部範囲外
 */
int func_schfile_data_check(struct tm *timeData, int dataNum, unsigned char *ptr_data, int fmtNo )
{
	int						buffSelect;
	int						setDataNum;
	int						dataPos;
	int						retCode = 0;

	//printf("START\n");

	/* スケジュール管理情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_SCHCTLFIL );

	/* データをコピーする */
	/* 書き込みバッファ面取得 */
	/* スケジュールデータが今年の場合 */
	if( ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear == timeData->tm_year )
	{
		buffSelect = ps_g_SCHCTLFIL->buffSelect;
	}
	/* スケジュールデータが翌年の場合 */
	else if( (ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear + 1) == timeData->tm_year )
	{
		/* バッファを切り換える */
		buffSelect = (ps_g_SCHCTLFIL->buffSelect+1) % PV_SCH_BUFF_NUM;

		/* バッファの開始年がスケジュール年と異なる場合 */
		if( ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear != timeData->tm_year)
		{
			/* バッファを初期化する */
			memset( &ps_g_SCHCTLFIL->s_schinfo[buffSelect], 0x00, sizeof(struct st_schinfo));
			ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear = timeData->tm_year;
		}
	}
	else
	{
		/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );
		//printf("set schedule year[%d] not found. \n", timeData->tm_year);
		return( -1 );
	}

	/* バッファ面に対する書き込み位置を取得 */
	dataPos = func_cmn_get_data_pos(timeData->tm_year, timeData->tm_mon, timeData->tm_mday, timeData->tm_hour, timeData->tm_min);

	// 2016/12/31 23:50 => 17567
	// 2016 データ数    => 17568(↑は0はじまりだから）
	/* バッファ面への書き込み可能数を算出 */
	if( (dataPos + dataNum) > (PV_GET_YEAR_DAYS(timeData->tm_year) * PV_SCH_DAY_TI_NUM))
	{
		setDataNum = (PV_GET_YEAR_DAYS(timeData->tm_year) * PV_SCH_DAY_TI_NUM) - dataPos;
	}
	else
	{
		setDataNum = dataNum;
	}

	/* データ書き込み共通関数 */
	func_schfile_data_set(buffSelect, dataPos, setDataNum, ptr_data, fmtNo);
	ptr_data += setDataNum;


	/* 書き込みデータが残っている場合 */
	if( dataNum != setDataNum )
	{
		
		/* 書き込み面がSelect面と同じ場合 */
		if( buffSelect == ps_g_SCHCTLFIL->buffSelect )
		{
			/* 2面目のバッファ面に切り換えて設定 */
			buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;

			/* バッファの開始年がスケジュール年と異なる場合 */
			if( ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear != timeData->tm_year+1)
			{
				/* バッファを初期化する */
				memset( &ps_g_SCHCTLFIL->s_schinfo[buffSelect], 0x00, sizeof(struct st_schinfo));
				/* バッファ面の年を設定する */
				ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear = timeData->tm_year+1;
			}

			/* 残データの書き込みデータ数を補正(2面を超えることがある為） */
			if( (dataNum - setDataNum) > (PV_GET_YEAR_DAYS(timeData->tm_year+1) * PV_SCH_DAY_TI_NUM) )
			{
				/* 1年分のデータを設定 */
				setDataNum = PV_GET_YEAR_DAYS(timeData->tm_year+1) * PV_SCH_DAY_TI_NUM;
				retCode = 1;
			}
			else
			{
				/* 全てのデータを設定 */
				setDataNum = dataNum - setDataNum;
			}

			/* データ書き込み共通関数 */
			func_schfile_data_set(buffSelect, 0, setDataNum, ptr_data, fmtNo);
		}
		else
		{
			retCode = 1;
		}
	}
	/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );

	//printf("END\n");
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイルのデータを反映する
 *
 * 機能：
 * 		スケジュールファイルのデータを管理情報に反映する
 * 戻り値：
 */
int func_schfile_data_set(int buffSelect, int dataPos, int dataNum, unsigned char *ptr_data, int fmtNo )
{
	int        			schTypeBorder;
	int        			setSchType;
	int					loopCnt;
	struct st_schdata	*ptr_schData;
	signed char			*p_data;

	//printf("START\n");

	//printf("(buffSelect,dataPos,dataNum,fmtNo)=[%d,%d,%d,%d]\n", buffSelect, dataPos, dataNum, fmtNo);

	p_data = (signed char *)ptr_data;
	
	/* フォーマットNoによる上書き境界種別と更新種別を設定 */
	switch( fmtNo )
	{
		case PV_SCH_FMT_NO_UPDATE :
			schTypeBorder = PV_SCH_TYPE_UPDATE;
			setSchType = PV_SCH_TYPE_UPDATE;
			break;
		case PV_SCH_FMT_NO_YEAR :
		case PV_SCH_FMT_NO_MONTH :
		default :
			schTypeBorder = PV_SCH_TYPE_FIXED;
			setSchType = PV_SCH_TYPE_FIXED;
			break;
	}

	/* 書き込み開始ポインタを設定 */
	ptr_schData = &ps_g_SCHCTLFIL->s_schinfo[buffSelect].s_schdata[dataPos];

	/* データ数分繰り返す */
	for( loopCnt = 0; loopCnt < dataNum; loopCnt++ )
	{
		/* 該当スケジュールの更新判定 */
		if( ptr_schData->schType <= schTypeBorder )
		{
			/* 該当スケジュールを更新 */
			ptr_schData->schType = setSchType;

			/* 出力制御率の範囲チェック */
			if( *p_data < 0 )
			{
				/* 0より小さい場合は、0に丸めこむ */
				ptr_schData->data = 0;
			}
			else if( *p_data > 100 )
			{
				/* 100より大きい場合は、100に丸めこむ */
				ptr_schData->data = 100;
			}
			else
			{
				ptr_schData->data = *p_data;
			}

		}
		p_data++;
		ptr_schData++;
	}

	//printf("END\n");

	return 0;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻情報チェック
 *
 * 機能：
 * 		引数 timData で指定された時刻情報の整合性をチェックする
 * 戻り値：
 *		 0：正常
 *		-1：異常
 */
int func_cmn_time_check( struct tm *timData )
{
	/* 年のチェック */
	if( timData->tm_year <= 2000 )
	{
		return( -1 );
	}

	/* 月のチェック */
	if( (timData->tm_mon < 1 ) || (timData->tm_mon > 12 ) )
	{
		return( -1 );
	}

	/* 日のチェック */
	if( (timData->tm_mday < 1 ) || (timData->tm_mday > 31 ) )
	{
		return( -1 );
	}

	/* 時のチェック */
	if( (timData->tm_hour < 0 ) || (timData->tm_hour > 23 ) )
	{
		return( -1 );
	}

	/* 分のチェック */
	if( (timData->tm_min < 0 ) || (timData->tm_min > 59 ) )
	{
		return( -1 );
	}

	/* 秒のチェック */
	if( (timData->tm_sec < 0 ) || (timData->tm_sec > 59 ) )
	{
		return( -1 );
	}
	return 0;
}

int func_schfile_analize( int skipFlg, char *filePath )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	char					temp[256];
	int						loopCnt;
    int 					type;
	char 					FilePath[256];
	char					*namePos;
	char					inputPath[256];
	int						retCode = 0;
	time_t					nowTime;		/* 現在時刻 */
	struct tm				localTime;		/* 現在時刻 */

	memset(schFileErr, 0x00, sizeof(schFileErr));

	g_patten2Cnt = 0;
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


		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュールファイル内容表示> =======================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		if( skipFlg == 0 )
		{
			printf("\n");
			printf("  内容表示するスケジュールファイルのパスを入力して下さい\n");
			printf("  >>");
			fflush( NULL );
			memset( g_keyData, 0, sizeof( g_keyData ) );
			g_keyByte = read( 0, &g_keyData, 1024 );

			memset(inputPath, 0, sizeof(inputPath));
			memcpy(inputPath, g_keyData, g_keyByte-1);

		}
		else
		{
			memset(inputPath, 0, sizeof(inputPath));
			memcpy(inputPath, filePath, sizeof(inputPath));
		}

		memset(FilePath, 0, sizeof(FilePath));
		memcpy(FilePath, inputPath, sizeof(FilePath));

		/* 確認ファイル名が55文字以外の場合は処理対象外のため除外	*/
		if( strlen( inputPath ) < 55 )
		{
			sprintf( schFileErr[0], "有効なファイルパスではありません。");
			retCode = -1;
			break;
		}
		/* 確認ファイル名の拡張子のチェック */
		if( strncmp( &inputPath[strlen( inputPath ) - 5], ".data", 5 ) != 0 )
		{
			sprintf( schFileErr[0], "拡張子が正しくありません。");
			retCode = -1;
			break;
		}

		if( access( inputPath, R_OK ) != 0 )
		{
			sprintf( schFileErr[0], "有効なファイルパスではありません。");
			retCode = -1;
			break;
		
		}

		/* ファイルパスの後ろ55文字（スケジュールファイル名）に移動 */
		namePos = &FilePath[strlen(FilePath)-55];

		memset(temp, 0, sizeof(temp));
		memcpy(temp, namePos, 3);

		if(strncmp(temp, "ERR", 3) == 0)
		{
			type = PV_SCH_FMT_NO_ERR;
		}
		else
		{
			type = atoi(temp);
		}


		memset(&schData, 0x00, sizeof(schData));
		memcpy(schData.fileName, namePos, 56);
		schData.fileType = type;

		retCode = func_cmn_FileFormatCheck2(inputPath, type);

		break;
	}

	if( retCode == 0 )
	{
		func_display_pvschinfo2(type);
	}
	else
	{
		printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		for( loopCnt = 0; loopCnt < 14; loopCnt++)
		{
			if( schFileErr[loopCnt][0] != 0 )
			{
				printf("\033[0K");
				if( loopCnt == 0 )
					printf("  %s\n", schFileErr[loopCnt]);
				else
					printf("    [%d] %s\n", loopCnt, schFileErr[loopCnt]);
			}
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf(" スケジュールファイルの内容表示に失敗しました。 \n");
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("================================================================================\n");

		func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	}

	return 0;
}
	
int func_cmn_getSchData( time_t orderTime, struct st_schdata *pst_schData )
{
	int						retCode=-1;
	time_t					nowTime;		/* 現在時刻 */
	struct tm				localTime;		/* 現在時刻 */
	char					buffSelect;		/* s_schinfoの０面/１面 */
	time_t 					newYearTime;			/* 1/1 0:00の積算秒 */
    int						diff_time;				/* 1/1 0:00からの差分(秒) */
    int						data_pos;				/* データ位置 */
	int						loopCnt;

	if( orderTime == 0 )
	{
		/* 現在時刻を取得 */
		nowTime = time( NULL );
	}
	else
	{
		nowTime = orderTime;
	}
	/* 年月日時分秒を取得 */
	localtime_r( &nowTime, &localTime );

	/* バッファ面を検索 */
	for( loopCnt = 0; loopCnt < PV_SCH_BUFF_NUM; loopCnt++ )
	{
		if( ps_g_SCHCTLFIL->s_schinfo[loopCnt].schYear == (localTime.tm_year+1900) )
		{
			break;
		}
	}

	/* 指定時刻のバッファ面がある場合 */
	if( loopCnt != PV_SCH_BUFF_NUM )
	{
	    localTime.tm_mon = 0;     /* 月 ( 1月＝0 ) */
	    localTime.tm_mday = 1;    /* 日 */
	    localTime.tm_hour = 0;    /* 時 */
	    localTime.tm_min = 0;     /* 分 */
	    localTime.tm_sec = 0;     /* 秒 */

		/* 1/1 0:00の積算秒を取得 */
	    newYearTime = mktime(&localTime);

		/* 現在時刻との差分を算出 */
		diff_time = nowTime - newYearTime;

		/* スケジュールデータ位置を算出 */
		data_pos = (int)(diff_time/PV_SCH_TI_SEC);

		/* 指定時刻のスケジュールを参照する */
		pst_schData = &ps_g_SCHCTLFIL->s_schinfo[loopCnt].s_schdata[data_pos];

		retCode = 0;
	}

	return retCode;
}

int func_schdata_set( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	char					temp[256];
	int						loopCnt;
    int 					type;
	char					inputPath[256];
	int						retCode = 0;
	time_t					nowTime;		/* 現在時刻 */
	struct tm				localTime;		/* 現在時刻 */
	int						schdata;
	time_t					offsetKoma=0;
	char					buffSelect;		/* s_schinfoの０面/１面 */
	struct st_schdata		*pst_schData = NULL;			/* スケジュールデータポインタ */
	time_t 					newYearTime;			/* 1/1 0:00の積算秒 */
    int						diff_time;				/* 1/1 0:00からの差分(秒) */
    int						data_pos;				/* データ位置 */

	memset(schFileErr, 0x00, sizeof(schFileErr));
	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 現在時刻を取得 */
		nowTime = time( NULL );
		nowTime -= (nowTime % PV_SCH_TI_SEC);
		/* 年月日時分秒を取得 */
		localtime_r( &nowTime, &localTime );

		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュールデータ更新> =====================================================\n");

		printf("\n");
		printf("  現在コマから更新する未来のコマを指定してください。(q：キャンセル)\n");
		printf("  [ %04d/%02d/%02d %02d:%02d:%02d ]\n", 
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("  >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if( g_keyData[0] == 'q')
		{
			break;
		}
		offsetKoma = atoi(g_keyData);


		printf("\n");
		printf("  現在時刻の出力制御値を指定してください。(q：キャンセル)\n");
		printf("  ※0～100以外はスケジュールなし\n");
		printf("  >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if( g_keyData[0] == 'q')
		{
			break;
		}
		schdata = atoi(g_keyData);

		/* 年月日時分秒を取得 */
		nowTime += ( offsetKoma * PV_SCH_TI_SEC );
		localtime_r( &nowTime, &localTime );

		printf("\n");

		if( offsetKoma == 0 )
		{
			printf("  現在");
		}
		else
		{
			printf("  [%04d/%02d/%02d %02d:%02d]",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min);
		}

		if( ( schdata >= 0 ) && ( schdata <= 100 ) )
		{
			printf("  の出力制御値を（%d）を反映しますか？（ｙ／ｎ）\n", schdata);
		}
		else
		{
			printf("  の出力制御値を「スケジュールなし」にしますか？（ｙ／ｎ）\n");
		}
		printf("  >>");
		func_cmn_KeyInput(NULL);

		if( g_keyData[0] != 'y')
		{
			printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
			printf("\033[0K");	printf("\n");
			printf("\033[0K");	printf("  スケジュールデータの反映をキャンセルしました。 \n");
		}
		else
		{
			/* スケジュール管理情報保存ファイルのミューテックスをロック */
			commfunc_mutex_lock( PV_FILENO_SCHCTLFIL );

			/* バッファ面を検索 */
			for( loopCnt = 0; loopCnt < PV_SCH_BUFF_NUM; loopCnt++ )
			{
				if( ps_g_SCHCTLFIL->s_schinfo[loopCnt].schYear == (localTime.tm_year+1900) )
				{
					break;
				}
			}

			/* 指定時刻のバッファ面がある場合 */
			if( loopCnt != PV_SCH_BUFF_NUM )
			{
			    localTime.tm_mon = 0;     /* 月 ( 1月＝0 ) */
			    localTime.tm_mday = 1;    /* 日 */
			    localTime.tm_hour = 0;    /* 時 */
			    localTime.tm_min = 0;     /* 分 */
			    localTime.tm_sec = 0;     /* 秒 */

				/* 1/1 0:00の積算秒を取得 */
			    newYearTime = mktime(&localTime);

				/* 現在時刻との差分を算出 */
				diff_time = nowTime - newYearTime;

				/* スケジュールデータ位置を算出 */
				data_pos = (int)(diff_time/PV_SCH_TI_SEC);

				/* 指定時刻のスケジュールを参照する */
				pst_schData = &ps_g_SCHCTLFIL->s_schinfo[loopCnt].s_schdata[data_pos];

				if( ( schdata >= 0 ) && ( schdata <= 100 ) )
				{
					/* スケジュール種別を更新データ(固定)に設定 */
					pst_schData->schType = PV_SCH_TYPE_UPDATE;

					/* スケジュール値を更新する */
					if( pst_schData->data != schdata )
					{
						pst_schData->update = 1;
					}
					pst_schData->data = schdata;
				}
				else
				{
					/* スケジュール種別をなしに設定 */
					pst_schData->schType = PV_SCH_TYPE_NONE;
				}

				printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("  スケジュールデータを反映しました。 \n");
			}
			else
			{
				printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("  スケジュールデータの更新に失敗しました。\n");
				
			}

			/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
			commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );
		}

		break;
	}

	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	return 0;
}



int func_schfile_set( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	char					temp[256];
	int						loopCnt;
    int 					type;
	char 					FilePath[256];
	char					*namePos;
	char					inputPath[256];
	int						retCode = 0;
	time_t					nowTime;		/* 現在時刻 */
	struct tm				localTime;		/* 現在時刻 */

	memset(schFileErr, 0x00, sizeof(schFileErr));
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


		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュールファイル反映> ===================================================\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

	
		printf("\n");
		printf("  反映するスケジュールファイルのパスを入力して下さい\n");
		printf("  >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		memset(inputPath, 0, sizeof(inputPath));
		memcpy(inputPath, g_keyData, g_keyByte-1);

		memset(FilePath, 0, sizeof(FilePath));
		memcpy(FilePath, inputPath, sizeof(FilePath));

		/* 確認ファイル名が55文字以外の場合は処理対象外のため除外	*/
		if( strlen( inputPath ) < 55 )
		{
			sprintf( schFileErr[0], "有効なファイルパスではありません。");
			retCode = -1;
			break;
		}
		/* 確認ファイル名の拡張子のチェック */
		if( strncmp( &inputPath[strlen( inputPath ) - 5], ".data", 5 ) != 0 )
		{
			sprintf( schFileErr[0], "拡張子が正しくありません。");
			retCode = -1;
			break;
		}

		if( access( inputPath, R_OK ) != 0 )
		{
			sprintf( schFileErr[0], "有効なファイルパスではありません。");
			retCode = -1;
			break;
		
		}

		for( loopCnt=0; loopCnt<strlen(FilePath); loopCnt++)
		{
			if( FilePath[loopCnt] == '/' )
				namePos = &FilePath[loopCnt+1];
		}

		/* ファイルの先頭に装置IDが付与されている場合 */
		if( namePos[3] != '_' )
		{
			namePos+=9;
		}

		memset(temp, 0, sizeof(temp));
		memcpy(temp, namePos, 3);

		if(strncmp(temp, "ERR", 3) == 0)
		{
			sprintf( schFileErr[0], "有効なスケジュールファイルではありません。");
			retCode = -1;
			break;
		}

		type = atoi(temp);


		printf("\n");
		printf("  スケジュールファイルを反映しますか？（ｙ／ｎ）\n");
		printf("  >>");
		func_cmn_KeyInput(NULL);

		if( (g_keyByte == 2 ) && (g_keyData[0] != 'y') )
		{
			retCode = 1;
			break;
		}

		retCode = func_cmn_FileFormatCheck1(inputPath, type);

		break;
	}

	/* 正常終了の場合 */
	if( retCode == 0 )
	{
		if( type == PV_SCH_FMT_NO_YEAR )
		{
			/* 次回更新フラグを現在に反映する */
			ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext;
		}
		else if( type == PV_SCH_FMT_NO_UPDATE )
		{
			/* 更新スケジュールリトライ時間を通常時に変更 */
			ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = PV_UP_SCH_RTY_MIN;
		}

		printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("  スケジュールファイルの反映に成功しました。 \n");
	}
	else if( retCode == 1 )
	{
		printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  スケジュールファイルの反映をキャンセルしました。 \n");
	}
	else
	{
		printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		for( loopCnt =0; loopCnt < 15; loopCnt++ )
		{
			if( schFileErr[loopCnt][0] != 0 )
			{
				printf("\033[0K");	printf("  [%d]:%s\n", loopCnt, schFileErr[loopCnt]);
			}
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  スケジュールファイルの反映に失敗しました。 \n");
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");

	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	return 0;
}


int func_cmn_FileFormatCheck2( const char *FilePath, int fmtNo)
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						dataBlock;
	int						fileSize;
	int						checkSize=0;
	unsigned  char			buff[20065+1];
	int						loopCnt;
	int						loopCnt2;
	unsigned  char			*p_buff;
	char					plantId[27];
	int						dataNum;
	int						checksum;
	int						updateFlg;
	int						checksumNum;
	int						retCode = 0;
	int						blockErr;
	unsigned  char			*ptr_data;
	struct tm				timeData;		/* 時刻情報 */
	struct tm				nextTimeData;		/* 時刻情報 */
	time_t					upSchReqTime;

	if( fmtNo == PV_SCH_FMT_NO_ERR )
	{
		return 0;
	}
	/* 対象スケジュールファイルオープン */
	if( (fp = fopen(FilePath, "rb")) == NULL)
	{
		sprintf( schFileErr[0], "ファイルのオープンに失敗しました。");
		//sprintf( errMsg, "File[%s] Open error", FilePath );
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//printf("%s\n", errMsg);
		return( -1 );
	}

	/* ファイル読み込み */
	fileSize = fread( buff, sizeof( unsigned char ), sizeof(buff), fp );

	/* 最低限のファイルサイズの確認 */
	/* データ部のデータ数: 6 byte */
	if( fileSize <= 6 )
	{
		sprintf( schFileErr[0], "ファイルの読み込みに失敗しました。");
		//sprintf( errMsg, "file is not enough size[%s:%d]", FilePath, fileSize);
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//printf("%s\n", errMsg);
		fclose(fp);
		return( -2 );
	}


	p_buff = &buff[0];

	/* データ部のデータ数を取得 */
	dataBlock = func_cmn_bin2Num(p_buff, 6);
	schData.blockNum = dataBlock;
	p_buff+=6;
	checkSize +=6;
	//printf("dataBlock=[%d]\n", dataBlock);

	if( fmtNo == PV_SCH_FMT_NO_ID )
	{
		/* 発電所IDチェック */
		memset(plantId, 0, sizeof(plantId));
		for( loopCnt2=0; loopCnt2 < 26; loopCnt2++ )
		{
			sprintf(&plantId[loopCnt2], "%d", *p_buff);
			p_buff++;
		}
		/* 発電所IDのチェック */
		if( strncmp( &plantId[0], g_ps_SCHCONF->plantId, 26 ) != 0 )
		{
			sprintf( schFileErr[1], "異なる発電所ＩＤです。（%s）", plantId);
		}
		else
		{
			sprintf( schFileErr[1], "正常データです。");
		}
		memcpy(schData.blockData[0].plantId, plantId, sizeof(plantId));
		schData.blockData[0].regist = *p_buff;
		p_buff++;
		retCode = 0;
	}
	else
	{
		for( loopCnt=0; loopCnt < dataBlock; loopCnt++)
		{
			sprintf( schFileErr[loopCnt+1], "正常データです。");
			/* ブロック毎のエラー情報を初期化 */
			blockErr = 0;

			/* 固定オフセット（53 byte） */
			/* ┣スケジュールID    :10 byte */
			/* ┣発電所ID          :26 byte */
			/* ┣制御年月日        :12 byte */
			/* ┗出力制御率データ数: 5 byte */
			checkSize += 53;
			if( fileSize < checkSize )
			{
				//sprintf( errMsg, "DataBlock[%d] is not enough1 data size.[%d]<[%d]", loopCnt+1, fileSize, checkSize);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				retCode = -2;
				sprintf( schFileErr[loopCnt+1], "ファイルのデータが不足しています。");
				break;
			}

			/* スケジュールＩＤ取得 */
			for( loopCnt2=0; loopCnt2 < 10; loopCnt2++ )
			{
				sprintf(&schData.blockData[loopCnt].schId[loopCnt2], "%d", *p_buff);
				p_buff++;
			}

			/* 発電所IDチェック */
			memset(plantId, 0, sizeof(plantId));
			for( loopCnt2=0; loopCnt2 < 26; loopCnt2++ )
			{
				sprintf(&plantId[loopCnt2], "%d", *p_buff);
				p_buff++;
			}
			memcpy(schData.blockData[loopCnt].plantId, plantId, sizeof(plantId));
			/* 発電所IDのチェック */
			if( strncmp( &plantId[0], g_ps_SCHCONF->plantId, 26 ) != 0 )
			{
				sprintf( schFileErr[loopCnt+1], "異なる発電所ＩＤです。（%s）", plantId);
				//sprintf( errMsg, "DataBlock[%d] is plantId different=[%s]", loopCnt+1, plantId);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				blockErr = -1;
			}

			/* 制御年月日を取得 */
			timeData.tm_year = func_cmn_bin2Num(p_buff, 4);		p_buff+=4;
			timeData.tm_mon  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_mday = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_hour = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_min  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			timeData.tm_sec  = 0;

			schData.blockData[loopCnt].timeData = timeData;

			/* 確認ファイル名の生成日時のチェック */
			if( func_cmn_time_check(&timeData) < 0 )
			{
				sprintf( schFileErr[loopCnt+1], "制御年月日が正しくありません。（%04d/%02d/%02d %02d:%02d %02d）", 
							timeData.tm_year, timeData.tm_mon, timeData.tm_mday,
							timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
				//sprintf( errMsg, "schedule date error [%04d/%02d/%02d %02d:%02d %02d]",
				//			timeData.tm_year, timeData.tm_mon, timeData.tm_mday,
				//			timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				blockErr = -2;
			}


			/* 制御データ数を取得 */
			dataNum = func_cmn_bin2Num(p_buff, 5);
			p_buff+=5;
			schData.blockData[loopCnt].dataNum = dataNum;

			/* 出力データ以降のファイルサイズチェック */
			/* ┣データ部          :dataNum byte */
			/* ┣チェックサム      : 2 byte */
			/* ┣更新フラグ      ※: 1 byte */
			/* ┗次回アクセス日時※:14 byte */
			/*  ※更新スケジュールのみ*/
			checkSize += dataNum + 2;
			if( fmtNo == PV_SCH_FMT_NO_UPDATE )
			{
				checkSize += 15;
			}
			if( fileSize < checkSize )
			{
				//sprintf( errMsg, "DataBlock[%d] is not enough2 data size.[%d]<[%d]", loopCnt+1, fileSize, checkSize);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				sprintf( schFileErr[loopCnt+1], "ファイルのデータが不足しています。");
				retCode = -2;
				break;
			}

			//printf("DataStartTime=[%04d%02d%02d %02d:%02d], DataNum=[%d]\n", 
			//	timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, dataNum);

			/* 出力データのチェックサムを算出 */
			ptr_data = p_buff;
			checksum = 0;
			for( loopCnt2=0; loopCnt2 < dataNum; loopCnt2++ )
			{
				schData.blockData[loopCnt].data[loopCnt2] = *((char *)p_buff);
				checksum += *p_buff;
				p_buff++;
			}

			/* 更新スケジュールの場合は、更新フラグを取得 */
			if( fmtNo == PV_SCH_FMT_NO_UPDATE )
			{
				schData.updateFlg = *p_buff;
				p_buff++;
			}

			/* チェックサム取得 */
			checksumNum = func_cmn_bin2Num(p_buff, 2);
			p_buff+=2;
			schData.blockData[loopCnt].checkSum = checksumNum;

			/* 出力データのチェックサムを判定 */
			if( ( checksum % (timeData.tm_mon + timeData.tm_mday)) != checksumNum )
			{
				sprintf( schFileErr[loopCnt+1], "チェックサムが異常です。（期待：[%d] != 格納：[%d]）", 
					checksumNum,  (checksum % (timeData.tm_mon + timeData.tm_mday)));
				//sprintf( errMsg, "check sum different [%d] != [%d], date[%04d/%02d/%02d %02d:%02d]", 
				//	checksumNum,  (checksum % (timeData.tm_mon + timeData.tm_mday)), 
				//	timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min);
				//commfunc_syslog_msgset( pv_own_name, errMsg );
				//printf("%s\n", errMsg);
				blockErr = -3;
			}
			else
			{
				sprintf( &schFileErr[loopCnt+1][strlen(schFileErr[loopCnt+1])], "チェックサム[%d]", checksumNum);
			}

			/* 更新スケジュールの場合は、次回アクセス日時を取得 */
			if( fmtNo == PV_SCH_FMT_NO_UPDATE )
			{
				/* 次回アクセス日時を取得 */
				nextTimeData.tm_year = func_cmn_bin2Num(p_buff, 4);		p_buff+=4;
				nextTimeData.tm_mon  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_mday = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_hour = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_min  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				nextTimeData.tm_sec  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
				schData.nextTime = nextTimeData;

				/* 確認ファイル名の生成日時のチェック */
				if( func_cmn_time_check(&nextTimeData) < 0 )
				{
					sprintf( schFileErr[loopCnt+1], "次回アクセス日時が正しくありません。（%04d/%02d/%02d %02d:%02d %02d）", 
								nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday,
								nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
					//sprintf( errMsg, "update time error [%04d/%02d/%02d %02d:%02d %02d]",
					//			nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday,
					//			nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
					//commfunc_syslog_msgset( pv_own_name, errMsg );
					//printf("%s\n", errMsg);
					blockErr = -2;
				}
				else
				{
					/* 時刻取得共通関数 */
					upSchReqTime = func_cmn_mktime(nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday, 
												nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
				
				}
			}

			/* ブロックのフォーマットチェック  */
			if( blockErr != 0 )
			{
				/* 戻り値にエラーを設定 */
				//retCode = -3;
			}
		}
	}

	fclose(fp);

	//printf("END\n");
	
	return retCode;
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
void	func_display_pvschinfo2( int fileType )
{
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	int					offSet = 0;		/* オフセット */
	char				workStr[32];	/* ワーク */
	int					loopEnd;		/* ループカウンタ */
	int					loopCnt1;		/* ループカウンタ */
	int					loopCnt2;		/* ループカウンタ */
	int					loopCnt3;		/* ループカウンタ */
	int					loopNum;
	char				inKeyData[2];	/* キー入力値 */
	char				readByte;		/* キー入力バイト数 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	struct winsize		termSize_OLD;			/* 端末サイズ保存 */
	int					dispMode = 0;	/* 表示モード */
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
	int					blockPage=0;
	int					dataPage=0;
    time_t 				mk_tim;
    struct tm 			mk_time;
	int					outLimit;		/* 表示可能行数 */
	int					noOut=12;		/* 表示不可行数 */
	char				buff[256];
	FILE				*fp;						/* ファイルディスクリプタ */

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
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 端末サイズを取得 */
	func_termsize_get();
	/* 端末サイズを保存 */
	termSize_OLD.ws_row = termSize.ws_row;

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
		printf("= <スケジュールファイル内容表示> =================================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		printf("\033[0K");
		printf("ファイル種別      ：%s\n", (fileType == PV_SCH_FMT_NO_YEAR ? "固定スケジュール(年間)" : (fileType == PV_SCH_FMT_NO_UPDATE ? "更新スケジュール" : (fileType == PV_SCH_FMT_NO_MONTH ? "固定スケジュール(月間)" : (fileType == PV_SCH_FMT_NO_ID ? "ＩＤ登録結果" : "エラーファイル")))));
		printf("ファイル名        ：%s\n", schData.fileName);

		printf("----------------------------------------------------------------------------------------------------------\n");
		if( schData.fileType == PV_SCH_FMT_NO_ERR )
		{
			/* 対象スケジュールファイルオープン */
			sprintf(buff, "%s%s", g_curDir, schData.fileName);
			if( (fp = fopen(buff, "rb")) == NULL)
			{
				printf("ファイルのオープンに失敗しました。");
			}
			else
			{
				/* ファイル読み込み */
				memset(buff, 0, sizeof(buff));
				fread( buff, sizeof( unsigned char ), sizeof(buff), fp );
				/* エラーコードとメッセージを区切る */
				buff[5]=0;
				fclose(fp);

				/* エラーコードを設定 */
				printf("エラーコード      ：%s\n", &buff[0]);
				printf("エラーメッセージ  ：%s\n", &buff[6]);
			}
			printf("==========================================================================================================\n");
			printf("戻る:[q]");
		}
		else if( schData.fileType == PV_SCH_FMT_NO_ID )
		{
			printf("\033[0K");
			printf("ブロック番号      ：1\n");
			printf("データ整合性      ：%s\n", schFileErr[1]);
			printf("発電所ＩＤ        ：%s\n", schData.blockData[0].plantId);
			printf("ＩＤ登録結果      ：%d(%s)\n", schData.blockData[0].regist, ((schData.blockData[0].regist==0) ? "ID登録済み" : "ID未登録" ));
			printf("==========================================================================================================\n");
			printf("戻る:[q]");
		}
		else
		{
/* [20210730] Add St */
			if( dispMode == 1 )
			{
				loopCnt1 = 0;
				loopEnd = schData.blockNum;
				outLimit = ( 365 * 2 ) + 1;
				offSet = 0;
			}
			else
			{
				loopCnt1 = blockPage;
				loopEnd = blockPage+1;
				outLimit = (termSize.ws_row - noOut );
			}
/* [20210730] Add Ed */

/* [20210730] Chg St */
			for( ; loopCnt1 < loopEnd; loopCnt1++ )
			{
				printf("\033[0K");
				printf("ブロック番号      ：%2d                          | データ整合性      ：%s\n",loopCnt1+1, schFileErr[loopCnt1+1]);
				printf("\033[0K");
				if( schData.fileType == PV_SCH_FMT_NO_UPDATE )
				{
					outLimit--;
					printf("次回アクセス日時  ：%04d/%02d/%02d %02d:%02d:%02d         | 更新フラグ        ：%d\n",
							schData.nextTime.tm_year,
							schData.nextTime.tm_mon,
							schData.nextTime.tm_mday,
							schData.nextTime.tm_hour,
							schData.nextTime.tm_min,
							schData.nextTime.tm_sec,
							schData.updateFlg );
				}
				printf("\033[0K");
				printf("発電所ＩＤ        ：%s", schData.blockData[loopCnt1].plantId);
				printf("  | 制御年月日時分    ：%04d/%02d/%02d %02d:%02d:%02d\n", 
							schData.blockData[loopCnt1].timeData.tm_year,
							schData.blockData[loopCnt1].timeData.tm_mon,
							schData.blockData[loopCnt1].timeData.tm_mday,
							schData.blockData[loopCnt1].timeData.tm_hour,
							schData.blockData[loopCnt1].timeData.tm_min,
							schData.blockData[loopCnt1].timeData.tm_sec);
				printf("\033[0K");
				printf("スケジュールＩＤ  ：%10s                  | 出力制御率データ数：%d\n", schData.blockData[loopCnt1].schId, schData.blockData[loopCnt1].dataNum);

				printf("\033[0K");
				printf("----------------------------------------------------------------------------------------------------------\n");
				
				
				printf("\033[0K");
				printf("日付 AM/PM| 00:00 | 01:00 | 02:00 | 03:00 | 04:00 | 05:00 | 06:00 | 07:00 | 08:00 | 09:00 | 10:00 | 11:00 ");

				mk_tim = func_cmn_mktime(schData.blockData[loopCnt1].timeData.tm_year, 
											schData.blockData[loopCnt1].timeData.tm_mon, 
											schData.blockData[loopCnt1].timeData.tm_mday, 
											schData.blockData[loopCnt1].timeData.tm_hour, 
											schData.blockData[loopCnt1].timeData.tm_min, 0);

				/* 表示開始日時刻 */
				mk_tim += (offSet * PV_SCH_TI_SEC);

				// 表示可能なライン数が表示可能日時
				localtime_r( &mk_tim, &mk_time );
			    mk_time.tm_year += 1900;
			    mk_time.tm_mon += 1;
				viewPos = 24 - (((mk_time.tm_hour%12)*2)+(mk_time.tm_min/30));
				viewPos += ((outLimit-1) * 24);
				if( (offSet +  viewPos ) > schData.blockData[loopCnt1].dataNum )
				{
					viewPos = schData.blockData[loopCnt1].dataNum - offSet;
				}

				for( loopCnt2=offSet; loopCnt2 < (offSet+viewPos); loopCnt2++ )
				{
					if( (loopCnt2 == 0) || (((mk_time.tm_hour%12) == 0 ) && (mk_time.tm_min == 0 )))
					{
						printf("\n%02d/%02d %s  |", mk_time.tm_mon, mk_time.tm_mday, ((mk_time.tm_hour/12)==0 ? "AM" : "PM"));

						if( loopCnt2 == 0 )
						{
							loopNum = ((mk_time.tm_hour%12)*2)+(mk_time.tm_min/30);
							for( loopCnt3 = 0; loopCnt3 < loopNum; loopCnt3++)
							{
								if( (loopCnt3 % 2 ) == 1 )
									printf("   |");
								else
									printf("   ,");
							}
						}
					}
					else
					{
						if( mk_time.tm_min == 0 )
							printf("|");
						else
							printf(",");
					}

					printf("%3d", schData.blockData[loopCnt1].data[loopCnt2]);
					mk_tim += PV_SCH_TI_SEC;
					localtime_r( &mk_tim, &mk_time );
				    mk_time.tm_year += 1900;
				    mk_time.tm_mon += 1;
				}

				if(((mk_time.tm_hour%12) == 0 ) && (mk_time.tm_min == 0 ))
					printf("\n");
				else
				{
					loopNum = 24 - (((mk_time.tm_hour%12)*2)+(mk_time.tm_min/30));
					for( loopCnt3 = 0; loopCnt3 < loopNum; loopCnt3++)
					{
						if( ((loopCnt3 + (mk_time.tm_min/30)) % 2 ) == 0 )
							printf("|   ");
						else
							printf(",   ");
					}
					printf("\n");
				}

				printf("==========================================================================================================\n");
			}
			if( dispMode == 1 )
			{
				printf("戻る:[q]           画面内表示:[m]");
			}
			else
			{
				printf("戻る:[q]           一括表示:[m]           日切替:[前:c/次:v]           月切替:[前:b/次:n]");
			}
/* [20210730] Chg Ed */
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

		if( ( schData.fileType != PV_SCH_FMT_NO_ERR ) && ( schData.fileType != PV_SCH_FMT_NO_ID ))
		{
			
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
				if( dispMode == 0 )
				{
					/* ブロックの先頭より前になる場合 */
					if( (offSet - (outLimit*24)) < 0 )
					{
						offSet = 0;
					}
					else
					{
						offSet -= (outLimit*24);
					}
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* キー入力が次の場合 */
			if( (inKeyData[0] == 'v') || (inKeyData[0] == 'V') )
			{
				if( dispMode == 0 )
				{
					/* ブロックの終端の場合 */
					if( (offSet + viewPos ) < schData.blockData[blockPage].dataNum )
					{
						offSet += viewPos;
					}
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			
			/* キー入力が前の場合 */
			if( (inKeyData[0] == 'b') || (inKeyData[0] == 'B') )
			{
				if( dispMode == 0 )
				{
					blockPage = (blockPage + 12) % schData.blockNum;
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
			/* キー入力が次の場合 */
			if( (inKeyData[0] == 'n') || (inKeyData[0] == 'N') )
			{
				if( dispMode == 0 )
				{
					blockPage = (blockPage + 1) % schData.blockNum;
					offSet = 0;
				}
				/* 画面をクリア */
				printf("\033[2J");
			}
		}

	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	

}


void func_update_schedule( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	char 					tmpDate[13];
	int						retCode = 0;
	int						funcRet;
	char					filePath[256];
	char					fileName[256];
	struct tm 				data_start;
	struct tm 				data_end;
	struct tm 				data_next;
	int						offset;
	int						dataNum;
	char					tmp[4];
	char					blockNum[7];
	int						loopCnt;
	int						offsetsec;
	time_t					end_time;
	int						checkSum;
	char					checkSumErr[64];
	int						step=0;
	int						nextTime=1;
	time_t					nowTime;		/* 現在時刻 */
	struct tm				localTime;		/* 現在時刻 */

	g_patten2Cnt = 0;
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


		/* 装置種別と現在時刻の表示 */
		printf("= <更新スケジュール作成> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		///////////////////////////////////////////
		//	203：更新スケジュール
		func_cmn_set_tim();

		if( g_ps_SCHCONF->plantId[0] == 0 )
		{
			sprintf( errMsg, "発電所ＩＤが設定されていません。");
			retCode = -1;
			break;
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("    ・発電所ＩＤ          :%s\n", g_ps_SCHCONF->plantId);
		if( step > 0 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・制御開始日時        :%04d/%02d/%02d %02d:%02d\n",
											data_start.tm_year, data_start.tm_mon, data_start.tm_mday,
											data_start.tm_hour, data_start.tm_min );
		}
		if( step > 1 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・スケジュールデータ数:%d (～%04d/%02d/%02d %02d:%02d)\n", dataNum, 
											data_end.tm_year, data_end.tm_mon, data_end.tm_mday,
											data_end.tm_hour, data_end.tm_min );
		}
		if( step > 2 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・次回アクセス日時    :%04d/%02d/%02d %02d:%02d:%02d\n", 
											g_data_next.tm_year, g_data_next.tm_mon, g_data_next.tm_mday,
											g_data_next.tm_hour, g_data_next.tm_min, g_data_next.tm_sec );
		}
		if( step > 3 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・更新フラグ          :%d\n", g_ps_SCHCONF->updateFlg);
		}
		if( step > 4 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・データパターン      :%s\n", strType[g_ps_SCHCONF->setType]);
				func_data_pattern_view();
		}
		switch( step )
		{
			case 0 :	/* 制御開始日時 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・制御開始日時を指定して下さい。<例>2016年11月5日13:30＝＞201611051330\n");
				printf("\033[0K");	printf("      ※ 4桁以下の場合は、現在時刻からの30分オフセットとする<例>5＝＞2.5時間後\n");
				printf("\033[0K");	printf("      ※ 改行のみの場合は現在時刻からとする\n");
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				if( g_keyByte == 1 )
				{
					memcpy(&data_start, &g_timeData, sizeof(g_timeData));
					data_start.tm_min  -= (data_start.tm_min % PV_SCH_TI_MIN);
				}
				else if( g_keyByte < 5 )
				{
					offsetsec = atoi( g_keyData );
					offsetsec *= 60*30;
					g_Time += offsetsec;

					localtime_r( &g_Time, &data_start );

					data_start.tm_year += 1900;
					data_start.tm_mon+= 1;
					data_start.tm_min  -= (data_start.tm_min % PV_SCH_TI_MIN);

				}
				else if( g_keyByte != 13 )
				{
					sprintf( errMsg, "入力データが正しくありません");
					retCode = -1;
					break;
				}
				else
				{
					memcpy(tmpDate, g_keyData, sizeof(tmpDate));
					data_start.tm_min  = atoi(&tmpDate[10]);	tmpDate[10]=0;
					data_start.tm_min -= (data_start.tm_min % PV_SCH_TI_MIN);
					data_start.tm_hour = atoi(&tmpDate[8]);	tmpDate[8]=0;
					data_start.tm_mday = atoi(&tmpDate[6]);	tmpDate[6]=0;
					data_start.tm_mon  = atoi(&tmpDate[4]);	tmpDate[4]=0;
					data_start.tm_year = atoi(&tmpDate[0]);	tmpDate[0]=0;
					data_start.tm_sec  = 0;
				}
				data_start.tm_sec  = 0;
				step++;
				break;


			case 1 :	/* スケジュールデータ数 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・%d単位のスケジュールデータ数を入力して下さい。\n", PV_SCH_TI_MIN);
				printf("\033[0K");	printf("      ※ 推奨範囲は１～３３６（※７日分）\n");
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				dataNum = atoi( g_keyData );

				if( (dataNum < 1))
				{
					sprintf( errMsg, "作成可能範囲外です。");
					retCode = -1;
					break;
				}
				end_time = func_cmn_mktime(data_start.tm_year, data_start.tm_mon, data_start.tm_mday, data_start.tm_hour, data_start.tm_min, 0);
				end_time += (dataNum-1) * PV_SCH_TI_SEC;
				localtime_r(&end_time, &data_end);
				data_end.tm_year += 1900;
				data_end.tm_mon+= 1;
			
				memset(g_timeStr, 0, sizeof(g_timeStr));
				func_cmn_set_data_str(&g_timeData, g_timeStr, 14);

				step++;
				break;

			case 2 :	/* 次回アクセス日時 */
				localtime_r(&end_time, &data_next);
				data_next.tm_year += 1900;
				data_next.tm_mon+= 1;
				
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・次回アクセス日時(%04d/%02d/%02d %02d:%02d)のオフセット(±秒)を指定して下さい。\n",
											data_next.tm_year, data_next.tm_mon, data_next.tm_mday,
											data_next.tm_hour, data_next.tm_min );
				printf("\033[0K");	printf("      ※ 参考：%6d：  5分\n", 5*60);
				printf("\033[0K");	printf("      ※     ：%6d： 10分\n", 10*60);
				printf("\033[0K");	printf("      ※     ：%6d：  1時\n", 60*60);
				printf("\033[0K");	printf("      ※     ：%6d： 24時\n", 24*60*60);
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				offset = atoi( g_keyData );

				end_time += (offset);
				localtime_r(&end_time, &g_data_next);
				g_data_next.tm_year += 1900;
				g_data_next.tm_mon+= 1;

				step++;
				break;


			case 3 :	/* 更新フラグ */
				printf("\033[0K");	printf("\n");
				if( func_updateflg_get() < 0 )
				{
					retCode = -1;
					break;
				}
				step++;
				break;

			case 4 :	/* データパターン */
				printf("\033[0K");	printf("\n");

				funcRet = func_data_pattern_check();
				if( funcRet == 0 )
				{
					step++;
					continue;
				}

				break;

			case 5 :
				sprintf(fileName, "%03d_0000_%s_%s.data", PV_SCH_FMT_NO_UPDATE, g_ps_SCHCONF->plantId, g_timeStr);
				sprintf(filePath, "%s%s", g_curDir, fileName);

				if( (fp = fopen(filePath, "wb")) == NULL)
				{
					sprintf( errMsg, "ファイルが開けません。");
					retCode = -1;
					return;
				}

				memset(&blockNum, 0, sizeof(blockNum));
				pvct_cmn_num2byte(1, 6, blockNum);
				fwrite( blockNum, sizeof( unsigned char ), 6, fp );

				func_cmn_set_blockdata(PV_SCH_FMT_NO_UPDATE, &data_start, dataNum, 0);
				fwrite( g_binblockdata, sizeof( unsigned char ), g_blockbyte, fp );

				fclose(fp);
				step++;
				break;
			default :
				break;
		}

		if( (retCode == -1) || (step == 6 ))
			break;
	}

	/*  */
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  更新スケジュールファイルを以下に作成しました。\n");
		printf("\033[0K");	printf("    ディレクトリ：%s\n", g_curDir);
		printf("\033[0K");	printf("    ファイル名  ：%s\n", fileName);
	}
	else
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  更新スケジュールファイルの作成に失敗しました。\n");
	}
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);

}

void func_fixed_schedule_year( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						retCode = 0;
	int						funcRet;
	char					filePath[256];
	char					fileName[256];
	struct tm 				data_start;
	int						dataNum;
	char					tmp[4];
	char					blockNum[7];
	int						loopCnt;
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;			/* 端末サイズ保存 */
	int						step=0;


	g_patten2Cnt = 0;
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


		/* 装置種別と現在時刻の表示 */
		printf("= <固定スケジュール作成> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		///////////////////////////////////////////
		//	201：年間固定スケジュール
		func_cmn_set_tim();

		if( g_ps_SCHCONF->plantId[0] == 0 )
		{
			sprintf( errMsg, "発電所ＩＤが設定されていません。");
			retCode = -1;
			break;
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("    ・発電所ＩＤ          :%s\n", g_ps_SCHCONF->plantId);
		if( step > 0 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・制御開始月          :%04d/%02d\n", data_start.tm_year, data_start.tm_mon);
		}
		if( step > 1 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・更新フラグ          :%d(999%d)\n", g_ps_SCHCONF->updateFlg, g_ps_SCHCONF->updateFlg);
		}
		if( step > 2 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・データパターン      :%s\n", strType[g_ps_SCHCONF->setType]);
				func_data_pattern_view();
		}
		
		switch( step )
		{
			case 0 :
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・制御開始月を入力して下さい。<例>2016年11月＝＞1611\n");
				printf("\033[0K");	printf("     （改行のみの場合は、現在月とする）\n");
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				if( g_keyByte == 1 )
				{
					memcpy(&data_start, &g_timeData, sizeof(g_timeData));
					data_start.tm_hour = 0;
					data_start.tm_min = 0;
					data_start.tm_sec = 0;
					step++;
				}
				else if( g_keyByte != 5 )
				{
					sprintf( errMsg, "入力データが正しくありません");
					retCode = -1;
					break;
				}
				else
				{
					memset(&data_start, 0, sizeof(data_start));
					memcpy(tmp, g_keyData, 4);
					data_start.tm_mon = atoi(&tmp[2]);
					tmp[2]=0;
					data_start.tm_year = 2000 + atoi(&tmp[0]);
					data_start.tm_mday = 1;
					data_start.tm_min = 0;
					data_start.tm_sec = 0;
					step++;
				}
				break;

			case 1 :
				printf("\033[0K");	printf("\n");
				if( func_updateflg_get() < 0 )
				{
					retCode = -1;
					break;
				}
				step++;

				break;
			case 2 :
				printf("\033[0K");	printf("\n");

				funcRet = func_data_pattern_check();
				if( funcRet == 0 )
				{
/* [20210802] Add St */
					g_ps_SCHCONF->dataPtn[1].dataEnd = -1;
					if( g_ps_SCHCONF->setType == 2 )
					{
						printf("最終データのみ変更する場合は、入力してください。（未入力は指定なし）\n");
						printf("※JET認証試験用 年間スケジュールのオプション機能です。）\n");
						printf("     >>");
						fflush( NULL );
						func_cmn_KeyInput(NULL);
						if( g_keyByte > 1 )
						{
							g_ps_SCHCONF->dataPtn[1].dataEnd = atoi( g_keyData );
						}
					}
/* [20210802] Add Ed */
					step++;
					continue;
				}

				break;

			case 3 :

				memset(g_timeStr, 0, sizeof(g_timeStr));
				func_cmn_set_data_str(&g_timeData, g_timeStr, 14);
				sprintf(fileName, "%03d_999%d_%s_%s.data", PV_SCH_FMT_NO_YEAR, g_ps_SCHCONF->updateFlg, g_ps_SCHCONF->plantId, g_timeStr);
				sprintf(filePath, "%s%s", g_curDir, fileName);

				if( (fp = fopen(filePath, "wb")) == NULL)
				{
					sprintf( errMsg, "ファイルが開けません。  [%s]", fileName);
					retCode = -1;
					return;
				}

				memset(&blockNum, 0, sizeof(blockNum));
				pvct_cmn_num2byte(13, 6, blockNum);
				fwrite( blockNum, sizeof( unsigned char ), 6, fp );

				/* 13か月分作成 */
				for( loopCnt = 0; loopCnt < 13; loopCnt++)
				{
					g_ps_SCHCONF->dataPtn[1].montCnt = loopCnt+1;	/* [20210802] Add */
					dataNum = func_cmn_offset_sec(&data_start, 0, 1) / PV_SCH_TI_SEC;
					func_cmn_set_blockdata(PV_SCH_FMT_NO_MONTH, &data_start, dataNum, 0);
					fwrite( g_binblockdata, sizeof( unsigned char ), g_blockbyte, fp );

					data_start.tm_year += (data_start.tm_mon ) / 12;
					data_start.tm_mon = (data_start.tm_mon % 12 ) + 1;
				}
				g_ps_SCHCONF->dataPtn[1].dataEnd = -1;	/* [20210802] Add */
				fclose(fp);
				step++;
				break;
			default :
				break;
		}


		if( (retCode == -1) || (step == 4 ))
			break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  固定スケジュールファイル（年間）を以下に作成しました。\n");
		printf("\033[0K");	printf("    ディレクトリ：%s\n", g_curDir);
		printf("\033[0K");	printf("    ファイル名  ：%s\n", fileName);
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  固定スケジュールファイルの作成に失敗しました。\n");
	}
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
}


void func_fixed_schedule_month( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						retCode = 0;
	int						funcRet;
	char					filePath[256];
	char					fileName[256];
	struct tm 				data_start;
	int						dataNum;
	char					tmp[4];
	char					blockNum[7];
	int						loopCnt;
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	struct termio			ttyBackup;		/* 端末設定保存用 */
	struct termio			ttyChange;		/* 端末設定変更用 */
	struct winsize			termSize_OLD;			/* 端末サイズ保存 */
	int						step=0;


	g_patten2Cnt = 0;
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


		/* 装置種別と現在時刻の表示 */
		printf("= <固定スケジュール作成> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		///////////////////////////////////////////
		//	202：月間固定スケジュール
		func_cmn_set_tim();

		if( g_ps_SCHCONF->plantId[0] == 0 )
		{
			sprintf( errMsg, "発電所ＩＤが設定されていません。");
			retCode = -1;
			break;
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("    ・発電所ＩＤ          :%s\n", g_ps_SCHCONF->plantId);

		if( step > 0 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・作成月              :%04d/%02d\n", data_start.tm_year, data_start.tm_mon);
		}
		if( step > 1 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・データパターン      :%s\n", strType[g_ps_SCHCONF->setType]);
				func_data_pattern_view();
		}
		
		switch( step )
		{
			case 0 :
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・作成月を入力して下さい。<例>2016年11月＝＞1611\n");
				printf("\033[0K");	printf("     （改行のみの場合は、現在月とする）\n");
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				if( g_keyByte == 1 )
				{
					memcpy(&data_start, &g_timeData, sizeof(g_timeData));
					data_start.tm_hour = 0;
					data_start.tm_min = 0;
					data_start.tm_sec = 0;
					step++;
				}
				else if( g_keyByte != 5 )
				{
					sprintf( errMsg, "入力データが正しくありません");
					retCode = -1;
					break;
				}
				else
				{
					memset(&data_start, 0, sizeof(data_start));
					memcpy(tmp, g_keyData, 4);
					data_start.tm_mon = atoi(&tmp[2]);
					tmp[2]=0;
					data_start.tm_year = 2000 + atoi(&tmp[0]);
					data_start.tm_mday = 1;
					data_start.tm_min = 0;
					data_start.tm_sec = 0;
					step++;
				}
				break;

			case 1 :
				printf("\033[0K");	printf("\n");

				funcRet = func_data_pattern_check();
				if( funcRet == 0 )
				{
					step++;
					continue;
				}

				break;

			case 2 :

				memset(g_timeStr, 0, sizeof(g_timeStr));
				func_cmn_set_data_str(&g_timeData, g_timeStr, 14);
				sprintf(fileName, "%03d_%02d%02d_%s_%s.data", PV_SCH_FMT_NO_MONTH, (data_start.tm_year%100), data_start.tm_mon, g_ps_SCHCONF->plantId, g_timeStr);
				sprintf(filePath, "%s%s", g_curDir, fileName);

				if( (fp = fopen(filePath, "wb")) == NULL)
				{
					sprintf( errMsg, "ファイルが開けません。  [%s]", fileName);
					retCode = -1;
					return;
				}

				memset(&blockNum, 0, sizeof(blockNum));
				pvct_cmn_num2byte(1, 6, blockNum);
				fwrite( blockNum, sizeof( unsigned char ), 6, fp );

				dataNum = func_cmn_offset_sec(&data_start, 0, 1) / PV_SCH_TI_SEC;
				func_cmn_set_blockdata(PV_SCH_FMT_NO_MONTH, &data_start, dataNum, 0);
				fwrite( g_binblockdata, sizeof( unsigned char ), g_blockbyte, fp );

				fclose(fp);

				step++;
				break;
			default :
				break;
		}


		if( (retCode == -1) || (step == 3 ))
			break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  固定スケジュールファイル（月間）を以下に作成しました。\n");
		printf("\033[0K");	printf("    ディレクトリ：%s\n", g_curDir);
		printf("\033[0K");	printf("    ファイル名  ：%s\n", fileName);
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  固定スケジュールファイルの作成に失敗しました。\n");
	}
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
}


void func_idregist_file( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						retCode = 0;
	char					filePath[256];
	char					fileName[256];
	char					id_ret;

	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 装置種別と現在時刻の表示 */
		printf("= <ＩＤ登録結果ファイル作成> =============================================================\n");

		///////////////////////////////////////////
		//	アプリケーションエラー
		func_cmn_set_tim();
		
		if( g_ps_SCHCONF->plantId[0] == 0 )
		{
			sprintf( errMsg, "発電所ＩＤが設定されていません。");
			retCode = -1;
			break;
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("    ・発電所ＩＤ          :%s\n", g_ps_SCHCONF->plantId);

		printf("\033[0K");	printf("\n");
		printf("    ・ＩＤ登録結果を選択して下さい。\n");
		printf("        ０：ＩＤ登録済み\n");
		printf("        １：ＩＤ未登録\n");
		printf("      >>");
		func_cmn_KeyInput(NULL);

		if( g_keyByte != 2 )
		{
			sprintf(errMsg, "入力データが正しくありません");
			retCode = -1;
			break;
		}
		else
		{
			id_ret = atoi( g_keyData );
			if( (id_ret != 0 ) && ( id_ret != 1 ) )
			{
				sprintf(errMsg, "入力データが正しくありません");
				retCode = -1;
				break;
			}
		}

		memset(g_timeStr, 0, sizeof(g_timeStr));
		func_cmn_set_data_str(&g_timeData, g_timeStr, 14);

		sprintf(fileName, "%03d_8888_%s_%s.data", PV_SCH_FMT_NO_ID, g_ps_SCHCONF->plantId, g_timeStr);
		sprintf(filePath, "%s%s", g_curDir, fileName);

		if( (fp = fopen(filePath, "wb")) == NULL)
		{
			sprintf(errMsg, "ファイルの作成に失敗しました。");
			retCode = -1;
			break;
		}

		memset(g_binblockdata, 0, sizeof(g_binblockdata));
		g_blockbyte=0;
		pvct_cmn_str2byte("000001", &g_binblockdata[g_blockbyte], 6);
		g_blockbyte+=6;
		pvct_cmn_str2byte(g_ps_SCHCONF->plantId, &g_binblockdata[g_blockbyte], 26);
		g_blockbyte+=26;
		g_binblockdata[g_blockbyte] = id_ret;
		g_blockbyte++;
		
		fwrite( g_binblockdata, sizeof( unsigned char ), g_blockbyte, fp );

		fclose(fp);

		break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  ＩＤ登録結果ファイルを以下に作成しました。\n");
		printf("\033[0K");	printf("    ディレクトリ：%s\n", g_curDir);
		printf("\033[0K");	printf("    ファイル名  ：%s\n", fileName);
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  ＩＤ登録結果ファイルの作成に失敗しました。\n");
	}
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");

	func_cmn_KeyInput(PV_INPUT_MSG);
}

void func_schedule_error( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						retCode = 0;
	char					filePath[256];
	char					fileName[256];
	int						err_num;
	int						step=0;
	char					schkbn[5];
	int						loopCnt;
	char					err_code[PV_ERR_PATTERN][256]={
											 {"E1001 スケジュール区分は４桁で設定してください。"}
											,{"E1002 スケジュール区分に半角数字以外が設定されています。"}
											,{"E1003 スケジュール区分の指定に誤りがあります。"}
											,{"E1006 発電所ＩＤは２６桁で設定してください。"}
											,{"E1007 発電所ＩＤに半角数字以外が設定されています。"}
											,{"E1008 ＭＡＣアドレスは１２桁で設定してください。"}
											,{"E1009 ＭＡＣアドレスに半角英数字以外が設定されています。"}
											,{"E1010 ＭＡＣアドレスの半角英数字は大文字で設定してください。"}
											,{"E0001 配信する固定スケジュール（年間）が存在しません。"}
											,{"E0002 配信する固定スケジュール（月間）が存在しません。"}
											,{"E0003 配信する更新スケジュールが存在しません。"}
	};


	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 装置種別と現在時刻の表示 */
		printf("= <エラーファイル作成> ===================================================================\n");

		///////////////////////////////////////////
		//	アプリケーションエラー
		func_cmn_set_tim();
		
		if( g_ps_SCHCONF->plantId[0] == 0 )
		{
			sprintf( errMsg, "発電所ＩＤが設定されていません。");
			retCode = -1;
			break;
		}
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("    ・発電所ＩＤ          :%s\n", g_ps_SCHCONF->plantId);

		if( step > 0 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・スケジュール区分    :%s\n", schkbn);
		}
		if( step > 1 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・エラーメッセージ    :%s\n", err_code[err_num-1]);
		}

		switch( step )
		{
			case 0 :
				printf("\033[0K");	printf("\n");
				printf("    ・スケジュール区分を入力して下さい。(半角)\n");
				printf("      <例>９９９Ｘ：固定スケジュール（年間）\n");
				printf("          ＹＹＭＭ：固定スケジュール（月間）\n");
				printf("          ００００：更新スケジュール\n");
				printf("          ８８８８：ＩＤ登録確認\n");
				printf("      >>");
				func_cmn_KeyInput(NULL);

				if( g_keyByte != 5 )
				{
					sprintf(errMsg, "入力データが正しくありません");
					retCode = -1;
					break;
				}
				else
				{
					memset(schkbn, 0, sizeof(schkbn));
					memcpy(schkbn, g_keyData, 4);
				}
				step++;
				break;

			case 1 :
				printf("\033[0K");	printf("\n");
				printf("    ・エラーメッセージを選択して下さい。\n");
				for( loopCnt = 0; loopCnt < PV_ERR_PATTERN; loopCnt++ )
				{
					printf("        [%2d]：%s\n", loopCnt+1, err_code[loopCnt]);
				}
				printf("      >>");
				func_cmn_KeyInput(NULL);
				err_num = atoi( g_keyData );
				if( (err_num < 1) || ( err_num > PV_ERR_PATTERN))
				{
					sprintf(errMsg, "入力データが正しくありません");
					retCode = -1;
					break;
				}
				step++;
				break;

			case 2 :
				memset(g_timeStr, 0, sizeof(g_timeStr));
				func_cmn_set_data_str(&g_timeData, g_timeStr, 14);
				sprintf(fileName, "ERR_%s_%s_%s.data", schkbn, g_ps_SCHCONF->plantId, g_timeStr);
				sprintf(filePath, "%s%s", g_curDir, fileName);

				if( (fp = fopen(filePath, "wb")) == NULL)
				{
					sprintf(errMsg, "ファイルの作成に失敗しました。");
					retCode = -1;
					break;
				}

				
				memset(g_binblockdata, 0, sizeof(g_binblockdata));
				g_blockbyte=0;
				memcpy(g_binblockdata, err_code[err_num-1], 128);
				fwrite( g_binblockdata, sizeof( unsigned char ), 128, fp );

				fclose(fp);
				step++;
				break;
		}
		if( (retCode == -1) || (step == 3 ))
			break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  エラーファイルを以下に作成しました。\n");
		printf("\033[0K");	printf("    ディレクトリ：%s\n", g_curDir);
		printf("\033[0K");	printf("    ファイル名  ：%s\n", fileName);
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  エラーファイルの作成に失敗しました。\n");
	}
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");

	func_cmn_KeyInput(PV_INPUT_MSG);
}


void func_plantid_set( void )
{
	FILE				*fp;						/* ファイルポインタ */
	int					retCode=0;

	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);


		/* 装置種別と現在時刻の表示 */
		printf("= <発電所ＩＤ設定> =============================================================\n");

		printf("\n");
		printf("       現在の発電所ＩＤ：");
		if( g_ps_SCHCONF->plantId[0] == 0 )
		{
			printf("未設定\n");
		}
		else
		{
			printf("%s\n", g_ps_SCHCONF->plantId);
		}

		printf("\n");
		printf("       新しいＩＤ（２６文字の数字）を設定して下さい。\n");
		printf("       >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if(g_keyByte != 27 )
		{
			sprintf( errMsg, "ＩＤが２６文字ではありません");
			retCode = -1;
			break;
		}

		memset(g_ps_SCHCONF->plantId, 0, sizeof(g_ps_SCHCONF->plantId));
		sprintf(g_ps_SCHCONF->plantId, "%s", g_keyData);
		g_ps_SCHCONF->plantId[26]=0;
		break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  発電所ＩＤを変更しました。\n");
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  発電所ＩＤの変更に失敗しました。\n");
	}
	
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	return;
}

/* [20170411] Add St */
void func_macaddr_set( void )
{
	FILE				*fp;						/* ファイルポインタ */
	int					retCode=0;

	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);


		/* 装置種別と現在時刻の表示 */
		printf("= <ＭＡＣアドレス設定> =========================================================\n");

		printf("\n");
		printf("       現在のＭＡＣアドレス：");
		if( g_PGCONINF.macAddr[0] == 0 )
		{
			printf("未設定\n");
		}
		else
		{
			printf("%s\n", g_PGCONINF.macAddr);
		}

		printf("\n");
		printf("       新しいＩＤ（１２文字の大文字英数字）を設定して下さい。\n");
		printf("       >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if(g_keyByte != 13 )
		{
			sprintf( errMsg, "英数字が１２文字ではありません");
			retCode = -1;
			break;
		}

		memset(g_PGCONINF.macAddr, 0, sizeof(g_PGCONINF.macAddr));
		sprintf(g_PGCONINF.macAddr, "%s", g_keyData);
		g_PGCONINF.macAddr[12]=0;
		break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  ＭＡＣアドレスを変更しました。\n");
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  ＭＡＣアドレスの変更に失敗しました。\n");
	}
	
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	return;
}

void func_serverurl_set( void )
{
	FILE				*fp;						/* ファイルポインタ */
	int					retCode=0;

	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);


		/* 装置種別と現在時刻の表示 */
		printf("= <配信サーバＵＲＬ> ===========================================================\n");

		printf("\n");
		printf("       現在の配信サーバＵＲＬ：");
		if( g_PGCONINF.ipAddr[0] == 0 )
		{
			printf("未設定\n");
		}
		else
		{
			printf("%s\n", g_PGCONINF.ipAddr);
		}

		printf("\n");
		printf("       新しい配信サーバＵＲＬを設定して下さい。\n");
		printf("       >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if(g_keyByte == 1 )
		{
			sprintf( errMsg, "配信サーバーＵＲＬが設定されていません。");
			retCode = -1;
			break;
		}

		memset(g_PGCONINF.ipAddr, 0, sizeof(g_PGCONINF.ipAddr));
		sprintf(g_PGCONINF.ipAddr, "%s", g_keyData);
		g_PGCONINF.ipAddr[g_keyByte-1]=0;
		break;
	}

	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
	if( retCode == 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  配信サーバＵＲＬを変更しました。\n");
	}
	else
	{
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  配信サーバＵＲＬの変更に失敗しました。\n");
	}
	
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	return;
}
/* [20170411] Add Ed */

void func_cmn_KeyInput( char *msg )
{
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	memset( g_keyData, 0, sizeof( g_keyData ) );

	if( ( msg != NULL ) &&
		( (void *)msg != (void *)&PV_INPUT_ONLY) )
		printf("%s\n", msg);
	fflush( NULL );

	if( ((void *)msg == (void *)&PV_INPUT_ANYKEY_MSG) ||
		((void *)msg == (void *)&PV_INPUT_ONLY) )
	{
		/* 現在のターミナル設定を退避 */
		ioctl( 0, TCGETA, &ttyBackup );

		/* 変更用設定にコピー */
		ttyChange = ttyBackup;
		/* ローカルエコーをやめる */
		ttyChange.c_lflag &= ~(ECHO | ICANON );

		/* 設定変更を有効にする */
		ioctl( 0, TCSETAF, &ttyChange );

		g_keyByte = read( 0, &g_keyData, 1 );

		/* ターミナルの設定を元に戻す */
		ioctl( 0, TCSETAF, &ttyBackup );
	}
	else
	{
		g_keyByte = read( 0, &g_keyData, 1024 );
	}
}

void func_cmn_set_blockdata( int type, struct tm *sTim, int dataNum, char checkSum)
{
    time_t mk_tim;
    struct tm mk_time;
	char	tmpDate[15];
	int		loopCnt;

	memset(g_binblockdata, 0, sizeof(g_binblockdata));
	g_blockbyte=0;

	/* スケジュールID（制御時間から設定） */
	func_cmn_set_data_str(sTim, tmpDate, 14);
	pvct_cmn_str2byte(&tmpDate[2], &g_binblockdata[g_blockbyte], 10);
	g_blockbyte+=10;
	/* 発電所ID */
	pvct_cmn_str2byte(g_ps_SCHCONF->plantId, &g_binblockdata[g_blockbyte], 26);
	g_blockbyte+=26;
	/* 制御年月日時分 */
	pvct_cmn_str2byte(tmpDate, &g_binblockdata[g_blockbyte], 12);
	g_blockbyte+=12;

	/* データ数、出力制御率、チェックサムをグローバルに保持） */
	func_cmn_create_data(sTim, dataNum, checkSum);

	/* データ数 */
	memcpy(&g_binblockdata[g_blockbyte], g_blokdata.numByte, 5);
	g_blockbyte+=5;
	/* 出力制御率 */
	memcpy(&g_binblockdata[g_blockbyte], g_blokdata.data, dataNum);
	g_blockbyte+=dataNum;

	/* 更新スケジュールの場合、更新フラグ */
	if( type == PV_SCH_FMT_NO_UPDATE )
	{
		g_binblockdata[g_blockbyte] = g_ps_SCHCONF->updateFlg;
		g_blockbyte += 1;
	}
	
	/* チェックサム */
	memcpy(&g_binblockdata[g_blockbyte], g_blokdata.chksumByte, 2);
	g_blockbyte += 2;

	/* 更新スケジュールの場合、次回アクセス日時 */
	if( type == PV_SCH_FMT_NO_UPDATE )
	{
		func_cmn_set_data_str(&g_data_next, tmpDate, 14);
		pvct_cmn_str2byte(&tmpDate[0], &g_binblockdata[g_blockbyte], 14);

		g_blockbyte += 14;
	}

	return;
}

int func_cmn_set_tim( void )
{
	
	/* 積算秒を取得 */
	g_Time = time( NULL );
	/* 時刻情報に変換 */
	localtime_r( &g_Time, &g_timeData );

	g_timeData.tm_year += 1900;
	g_timeData.tm_mon+= 1;

//	printf("now[%d%02d%02d %02d:%02d %02d]\n",
//			g_timeData.tm_year,g_timeData.tm_mon,g_timeData.tm_mday,
//			g_timeData.tm_hour, g_timeData.tm_min, g_timeData.tm_sec);

	return(0);
}

void func_cmn_set_data_str( struct tm *timData, char *setStr, char setByte )
{
	char 		tmpStr[15];
	int			loopCnt;

	sprintf(tmpStr, "%04d%02d%02d%02d%02d%02d",
			timData->tm_year,timData->tm_mon,timData->tm_mday,
			timData->tm_hour, timData->tm_min, timData->tm_sec);

	for( loopCnt = 0; loopCnt < setByte; loopCnt++)
		setStr[loopCnt] = tmpStr[loopCnt];
}


int pvct_cmn_num2byte(int num, int dataNum, char *strData)
{
	int						loopCnt;
	int						strLen ;

	strLen = strlen(strData);

	for( loopCnt = dataNum-1; loopCnt >= 0; loopCnt-- )
	{
		strData[loopCnt] = num%10;
		num /= 10;
	}
	return 0;
}

int func_cmn_offset_sec( struct tm *sTim, int tm_year, int tm_month )
{
    struct tm tmptime;
    time_t s_time;
    time_t e_time;
    int    diff;

	memcpy(&tmptime, sTim, sizeof(tmptime));
	tmptime.tm_year -= 1900;
    tmptime.tm_mon -= 1;
    tmptime.tm_isdst = -1;    		/* サマータイムフラグ */
    s_time = mktime(&tmptime);

	memcpy(&tmptime, sTim, sizeof(tmptime));
	tmptime.tm_year  += tm_year + ((tmptime.tm_mon + tm_month ) / 12);
    tmptime.tm_mon   = ((tmptime.tm_mon + tm_month ) % 12);;
    tmptime.tm_isdst = -1;    		/* サマータイムフラグ */
	tmptime.tm_year -= 1900;
    tmptime.tm_mon -= 1;
    tmptime.tm_isdst = -1;    		/* サマータイムフラグ */

    e_time = mktime(&tmptime);

	diff = e_time - s_time;

    return diff;
}


void func_cmn_create_data(struct tm *sTim, int dataNum, char checkSum)
{
	int		setData;
	int		loopCnt;
	int		chksum=0;
	int		Type1Offset=0;
	
	memset(&g_blokdata, 0, sizeof(g_blokdata));

	Type1Offset = (sTim->tm_hour * 2)+(sTim->tm_min/PV_SCH_TI_MIN);

	pvct_cmn_num2byte(dataNum, 5, g_blokdata.numByte);
	for(loopCnt=0; loopCnt < dataNum; loopCnt++)
	{
		if( g_ps_SCHCONF->setType == 0 )
		{
			setData = rand() % 101;
		}
		else if( g_ps_SCHCONF->setType == 1 )
		{
			setData = g_ps_SCHCONF->dataPtn[0].data[(loopCnt+Type1Offset)%48];
		}
		else if( g_ps_SCHCONF->setType == 2 )
		{
			setData = g_ps_SCHCONF->dataPtn[1].data[g_patten2Cnt];
			g_patten2Cnt = (g_patten2Cnt+1) % g_ps_SCHCONF->dataPtn[1].num;
/* [20210802] Add St */
			if( ( g_ps_SCHCONF->dataPtn[1].montCnt == 13 ) &&
				( (loopCnt +1 ) == dataNum ) &&
				( g_ps_SCHCONF->dataPtn[1].dataEnd >= 0 ) )
			{
				setData = g_ps_SCHCONF->dataPtn[1].dataEnd;
			}
/* [20210802] Add Ed */
		}
		g_blokdata.data[loopCnt] = setData;
		chksum += g_blokdata.data[loopCnt];
	}
	chksum = (chksum%(sTim->tm_mon+sTim->tm_mday));
	if( checkSum != 0 )
	{
		chksum += (rand() % 98)+1;
	}
	g_blokdata.chksumByte[0] = chksum/10;
	g_blokdata.chksumByte[1] = chksum%10;

	return;
}

int pvct_cmn_str2byte(char *strData, char *byteData, char strLen)
{
	int						loopCnt;
	int						retCode = 0;

	for( loopCnt = 0; loopCnt < strLen; loopCnt++ )
	{
		if( ( strData[loopCnt] >= '0' ) && ( strData[loopCnt] <= '9' ) )
		{
			byteData[loopCnt] = strData[loopCnt] - '0';
		}
		else
		{
			printf("pvct_cmn_str2byte:ERROR!!!!\n");
			retCode = -1;
			break;
		}
	}
	return retCode;
}


int func_updateflg_get( void )
{
	int					retCode=0;
	int					dataNum;

	while(1)
	{
		printf("    ・更新フラグを入力して下さい。\n");
		printf("      >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if(g_keyByte != 2 )
		{
			sprintf( errMsg, "設定値が正しくありません");
			retCode = -1;
			break;
		}

		dataNum = atoi(g_keyData);
		if( (dataNum < 0 ) || ( dataNum >= 10 ))
		{
			sprintf( errMsg, "設定値が範囲外です。");
			retCode = -1;
			break;
		}
		else
		{
			g_ps_SCHCONF->updateFlg = dataNum;
			retCode = dataNum;
		}
		break;
	}

	return retCode;
}

void func_data_pattern_set( void )
{
	FILE				*fp;						/* ファイルポインタ */
	int					retCode=0;
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int					dataCnt;
	int					loopCnt;
	int					setType;
	int					dataPer;
	struct st_datptn	s_dataPtn;

	memset(&s_dataPtn, 0, sizeof(s_dataPtn));
	
	while(1)
	{
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);


		/* 装置種別と現在時刻の表示 */
		printf("= <データパターン設定> =========================================================\n");
		printf("\n");
		printf("    現在のデータパターン      :%s\n", strType[g_ps_SCHCONF->setType]);
		func_data_pattern_view();
		printf("\n");
		printf("\n");
		printf("    データパターンを指定して下さい。\n");
		printf("      ０：%s\n", strType[0]);
		printf("      １：%s\n", strType[1]);
		printf("      ２：%s\n", strType[2]);
		printf("      >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if(g_keyByte != 2 )
		{
			sprintf( errMsg, "設定値が正しくありません");
			retCode = -1;
			break;
		}

		setType = atoi(g_keyData);
		if( (setType < 0 ) || ( setType >= 3 ))
		{
			sprintf( errMsg, "設定値が範囲外です。");
			retCode = -1;
			break;
		}
		else
		{
			printf("\n");
			if( ( setType == 1 ) || ( setType == 2 ) )
			{
				if( setType == 1 )
				{
					printf("    1日のデータパターンを0:00から設定して下さい。\n");
					printf("    （データ数：48、区切り文字「,」）\n");
				}
				else
				{
					printf("    変更する場合は、設定データデータパターンを入力して下さい。\n");
					printf("    （データ数：最大256、区切り文字「,」）\n");
				}

				printf("    >>");
				fflush( NULL );
				memset( g_keyData, 0, sizeof( g_keyData ) );
				g_keyByte = read( 0, &g_keyData, 1024 );

				dataCnt = 0;
				if( g_keyByte > 1 )
				{
					dataCnt = func_sep_data( g_keyData, &s_sepStr );

					if( setType == 1 )
					{
						if( dataCnt != 48 )
						{
							sprintf( errMsg, "データ数が正しくありません。");
							retCode = -1;
						}
					}
					else
					{
						if( dataCnt > 256 )
						{
							sprintf( errMsg, "データ数が上限を超えています。");
							retCode = -1;
						}
					}

					if( retCode == 0 )
					{
						s_dataPtn.num = dataCnt;
						for( loopCnt = 0; loopCnt < s_dataPtn.num; loopCnt++ )
						{
							dataPer = atoi(s_sepStr.data[loopCnt]);
							if( ( dataPer < 0 ) || ( dataPer > 100 ) )
							{
								sprintf( errMsg, "不正なデータがあります。");
								retCode = -1;
								break;
							}
							s_dataPtn.data[loopCnt] = dataPer;
						}
					}

					if( retCode == 0 )
					{
						memcpy(&g_ps_SCHCONF->dataPtn[setType-1], &s_dataPtn, sizeof(s_dataPtn));
					}
				}

				if( dataCnt == 0 )
				{
					sprintf( errMsg, "データが設定されていません。");
					retCode = -1;
				}
			}
		}
		break;
	}

	printf("\n");
	if( retCode == 0 )
	{
		g_ps_SCHCONF->setType = setType;
		/* 画面をクリア */
		printf("\033[2J");

		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		/* 装置種別と現在時刻の表示 */
		printf("= <データパターン設定> ===================================================\n");
		printf("\n");
		printf("  データパターンを[%s]に変更しました。\n", strType[g_ps_SCHCONF->setType]);
		func_data_pattern_view();
		printf("\n");
	}
	else
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("  %s\n", errMsg);
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  データパターンの変更に失敗しました。\n");
	}
	
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);
	return;
}


int func_data_pattern_check( void )
{
	FILE				*fp;						/* ファイルポインタ */
	int					retCode=0;
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int					dataCnt;
	int					loopCnt1;
	int					setType;
	int					dataPer;
	struct st_datptn	s_dataPtn;

	memset(&s_dataPtn, 0, sizeof(s_dataPtn));
	
	while(1)
	{
		printf("    ・データパターンを変更しますか？（ｙ／ｎ）\n");
		printf("     （現在の設定[%s]）\n", strType[g_ps_SCHCONF->setType]);
		func_data_pattern_view();
		printf("         >>");
		fflush( NULL );
		memset( g_keyData, 0, sizeof( g_keyData ) );
		g_keyByte = read( 0, &g_keyData, 1024 );

		if( g_keyData[0] == 'y')
		{
			retCode = 1;
			func_data_pattern_set();
		}
		else if( g_keyData[0] == 'n')
		{
			retCode = 0;
		}
		else
		{
			retCode = 1;
			
		}
		break;
	}

	return retCode;
}

void func_data_pattern_view ( void )
{
	int					loopCnt1;
	if( g_ps_SCHCONF->setType == 1 )
	{
		printf("\033[0K");
		printf("     AM/PM| 00:00 | 01:00 | 02:00 | 03:00 | 04:00 | 05:00 | 06:00 | 07:00 | 08:00 | 09:00 | 10:00 | 11:00 ");
		for( loopCnt1 = 0; loopCnt1 < 48; loopCnt1++ )
		{
			if( (loopCnt1%24) == 0 )
			{
				printf("\033[0K");
				printf("\n      %s  ", (loopCnt1/24)==0 ? "AM" : "PM");
			}
			if( (loopCnt1 % 2 ) == 0 )
				printf("|");
			else
				printf(",");
			printf("%3d", g_ps_SCHCONF->dataPtn[g_ps_SCHCONF->setType-1].data[loopCnt1]);
			
		}
		printf("\033[0K");
		printf("\n");
	}
	else if( g_ps_SCHCONF->setType == 2 )
	{
		printf("\033[0K");
		for( loopCnt1 = 0; loopCnt1 < g_ps_SCHCONF->dataPtn[g_ps_SCHCONF->setType-1].num; loopCnt1++ )
		{
			if( (loopCnt1%24) == 0 )
			{
				printf("\033[0K");
				printf("      ");
			}
			else
			{
				printf(",");
			}
			printf("%3d", g_ps_SCHCONF->dataPtn[g_ps_SCHCONF->setType-1].data[loopCnt1]);
		}
		printf("\033[0K");
		printf("\n");
	}

	return;
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
	if(idx2) idx1++;

	return( idx1 );
}


int func_ssl_readmain( char *fileType )
{
    int                  sockfd;
    int                  body;
    int                  read_size;
    char                *pt;
    char                 msg[1024];
    char                 host[1024];
    char                 path[1024];
    struct sockaddr_in  s_sndAddr;
    struct sockaddr_in   server;
    struct servent      *service;
    struct hostent      *servhost;
    SSL_CTX             *sslCtx;
    long				sslVeryErr;
	int					sslret = -1;
	char				prefix[32];

	if( g_ps_SCHCONF->plantId[0] == 0 )
	{
		sprintf( errMsg, "発電所ＩＤが設定されていません。");
	}
	else if( g_PGCONINF.macAddr[0] == 0 )
	{
		sprintf( errMsg, "ＭＡＣアドレスが設定されていません。");
	}
	else if( g_PGCONINF.ipAddr[0] == 0 )
	{
		sprintf( errMsg, "配信サーバーＵＲＬが設定されていません。");
	}
	else
	{
	    memset(host, 0, sizeof(host));
	    memset(path, 0, sizeof(path));

	    /* ＵＲＬからホスト名・パス取得 */
	    /* ホスト名を取得 */
	    sscanf(g_PGCONINF.ipAddr, "https://%s", host);

	    /* パスを取得 */
	    if((pt = strchr(host, '/')) != NULL)
	    {
	        strcpy(path, pt);
	        *pt = 0;
	    }

	    /* ＩＰアドレス取得 */
	    if( (servhost = gethostbyname(host)) == NULL )
	    {
			sprintf( errMsg, "ＩＰアドレスの取得に失敗しました。(%d) ", errno );
			//sprintf( errMsg, "ERROR gethostbyname(%s) Errno(%d)", host, errno);
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			return( -1 );
		}

	    memset(&s_sndAddr, 0, sizeof(s_sndAddr));
	    memcpy(&s_sndAddr.sin_addr, servhost->h_addr, servhost->h_length);
	    s_sndAddr.sin_family = AF_INET;

	    /* HTTPSのポート番号設定 */
	    s_sndAddr.sin_port = htons( g_PGCONINF.portNumb );

	    /* ソケット生成 */
	    sockfd = socket(AF_INET, SOCK_STREAM, 0); 

	    if( sockfd < 0 )
	    {
			sprintf( errMsg, "ソケット生成に失敗しました。(%d)", errno );
			//sprintf( errMsg, "ERROR TCP Socket Error (%s) Errno(%d)\n", host, errno );
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			return-1;
		}

	    /* コネクト */
	    if( connect(sockfd, (struct sockaddr *)&s_sndAddr, sizeof(s_sndAddr)) == -1 )
		{
			/* ソケットをクローズ */
			close( sockfd );
			sprintf( errMsg, "ソケットの接続に失敗しました。(%d)", errno );
			//sprintf( errMsg, "ERROR TCP Device CONNECT Error (%s:%d) Errno(%d)\n",
			//	host, s_sndAddr.sin_port, errno );
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			return( -1 );
		}
	 
	    /* エラー文字列の解放 */
	    SSL_load_error_strings();

		/* ライブラリを初期化 */
	    (void)SSL_library_init();

	    /* プロトコル指定(SSLv2、SSLv3、TLSv1) */
	    sslCtx = SSL_CTX_new(SSLv23_client_method());
	    if( sslCtx == NULL )
	    {
			close( sockfd );
			sprintf( errMsg, "プロトコルの指定に失敗しました。(%d)", errno );
			//sprintf( errMsg, "ERROR SSL_CTX_new Error(null)");
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			return( -1 );
		}

		/* SSLオブジェクトの生成 */
	    ssl = SSL_new(sslCtx);
	    if( ssl == NULL )
	    {
			sprintf( errMsg, "ＳＳＬオブジェクト生成に失敗しました。" );
			//sprintf( errMsg, "ERROR SSL_new Error(null)");
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			close( sockfd );
		    SSL_CTX_free(sslCtx);
			return( -1 );
			
		}

		/* SSLオブジェクトとファイルディスクリプタを接続 */
	    sslret = SSL_set_fd(ssl, sockfd);
	    if( sslret != 1 )
	    {
			sprintf( errMsg, "ＳＳＬ接続に失敗しました。(%s)\n", ERR_reason_error_string(sslret) );
			//sprintf( errMsg, "ERROR SSL_set_fd (%s)\n", ERR_reason_error_string(sslret) );
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			close( sockfd );
		    SSL_CTX_free(sslCtx);
		    SSL_free(ssl); 
			return( -1 );
		}

		/* SSLの接続 */
	    sslret = SSL_connect(ssl);
	    if( sslret != 1 )
	    {
			sprintf( errMsg, "ＳＳＬ接続に失敗しました。ret(%d), errno(%d)\n", sslret, SSL_get_error(ssl, sslret) );
			//sprintf( errMsg, "ERROR SSL_connect ret(%d), errno(%d)\n", sslret, SSL_get_error(ssl, sslret) );
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			close( sockfd );
		    SSL_CTX_free(sslCtx);
		    SSL_free(ssl); 
			return( -1 );
		}

	    /* HTTPリクエストメッセージ生成 */
	    sprintf(msg, "POST %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Length: 84\r\ncontent-type: application/x-www-form-urlencoded\r\n\r\npower_plant_id=%s&mac_address=%s&schedule_kbn=%s\r\n", 
	    		path, host, g_ps_SCHCONF->plantId, g_PGCONINF.macAddr, fileType );

		if( ssl_comm_output == 1 )
		{
			sprintf(prefix, "req_%s", fileType);
			func_ssl_data_file_write( prefix, msg, strlen(msg));
			func_ssl_data_file_close( );
		}

	    /* リクエスト送信 */
	    sslret = SSL_write(ssl, msg, strlen(msg));
	    if( sslret < 0 )
	    {
			sprintf( errMsg, "ＳＳＬリクエスト送信に失敗しました。ret(%d), errno(%d)\n", sslret, SSL_get_error(ssl, sslret) );
			//sprintf( errMsg, "ERROR SSL_write ret(%d), errno(%d)\n", sslret, SSL_get_error(ssl, sslret) );
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			close( sockfd );
		    SSL_CTX_free(sslCtx);
		    SSL_shutdown(ssl); 
		    SSL_free(ssl); 
			return( -1 );
		}

		if( ssl_comm_output == 1 )
		{
			sprintf(prefix, "res_%s", fileType);
			func_ssl_data_file_write( prefix, NULL, 0);
		}

		/* スケジュールファイルダウンロード */
		sslret = func_ssl_filedownload();

		if( ssl_comm_output == 1 )
		{
			func_ssl_data_file_close( );
		}

	    /* SSL後処理 */
	    SSL_shutdown(ssl); 
	    SSL_free(ssl); 
	    SSL_CTX_free(sslCtx);
	    ERR_free_strings();

	    close(sockfd);
	}

    return(sslret);
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 指定時刻のデータ格納位置を返却
 *
 * 機能：
 * 		指定した年に対して、月日時分に対する格納位置を返却する
 * 戻り値：
 * 		  0 : 正常終了
 * 	 正の数 : 検索失敗（最後に検索した行の先頭位置）
 * 	 負の数 : 
 */
int func_ssl_read( char **p_restBuff )
{
	int reqSize;
	int readPos;
	int readSize;

	if( *p_restBuff != NULL )
	{
		readPos = sslReadSize - (*p_restBuff - sslBuff);
		reqSize = (PV_SSL_READ_SIZE-1) - readPos;
		memcpy(sslBuff, *p_restBuff, readPos);
	}
	else
	{
		reqSize = PV_SSL_READ_SIZE -1;
		readPos = 0;

	}

	/* SSLのデータを取得 */
	readSize = SSL_read(ssl, &sslBuff[readPos], reqSize);
	if( readSize < 0)
	{
		sprintf( errMsg, "データが足りません。");
	}
	if( ssl_comm_output == 1 )
	{
		func_ssl_data_file_write( NULL, &sslBuff[readPos], readSize);
	}
	sslReadSize = readPos+readSize;
	sslBuff[sslReadSize] = 0;
	*p_restBuff = sslBuff;

	return sslReadSize;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 指定時刻のデータ格納位置を返却
 *
 * 機能：
 * 		指定した年に対して、月日時分に対する格納位置を返却する
 * 戻り値：
 * 		  0 : 正常終了
 * 	 正の数 : 検索失敗（最後に検索した行の先頭位置）
 * 	 負の数 : 
 */
int func_ssl_fgets( char **buff )
{
	int		readSize;
	int 	readFlg = 0;
	int		retCode;
	char	*tmp;

	while( 1 )
	{
		/* 読み込みバッファポインタがNULL以外の場合 */
		if( p_sslBuff != NULL )
		{
			/* 改行コード（CR+LF(\r\n)）を検索 */
			if((tmp = strstr(p_sslBuff, "\r\n")) != NULL)
			{
				/* 見つかった場合、\r\nを0に置換し、行の先頭アドレスと、データサイズ(\r\n含む)を返却 */
				*tmp = 0;
				*tmp++;
				*tmp = 0;
				*tmp++;
				*buff = p_sslBuff;
				retCode = (tmp - p_sslBuff);
				p_sslBuff = tmp;
				break;
			}
            /* 改行コード（CR+LF(\r\n)）を検索 */
            else if((tmp = strstr(p_sslBuff, "\n")) != NULL)
            {
                    /* 見つかった場合、\r\nを0に置換し、行の先頭アドレスと、データサイズ(\r\n含む)を返却 */
                    *tmp = 0;
                    *tmp++;
                    *buff = p_sslBuff;
                    retCode = (tmp - p_sslBuff);
                    p_sslBuff = tmp;
                    break;
            }
			else
			{
				/* 先頭から検索して見つからなかった場合 */
				if( p_sslBuff == sslBuff )
				{
					sprintf( errMsg, "データ解析エラー");
					//printf("SSL_read 1 Line data size Over");
					retCode = -1;
					break;
				}
			}
			
		}

		/* データを読み込む */
		readSize = func_ssl_read(&p_sslBuff);

		/* ファイル終端、又は、異常の場合 */
		if( readSize <= 0 )
		{
			/* 終了する */
			retCode = readSize;
			p_sslBuff = NULL;
			break;
		}
		else
		{
			/* 先頭位置を設定する */
			p_sslBuff = sslBuff;
		}
	}

	return retCode;
}

//int func_ssl_getStatus()
int func_ssl_getStatus( void )
{
	char	*p_buff;
	char	*p_tmp1;
	char	*p_tmp2;
	int		retCode;
	int		lineLen;
	int		stsCode=-1;
	char	stsCodeStr[32];

	lineLen = func_ssl_fgets(&p_buff);
	if( lineLen <= 0 )
	{
		retCode = -1;
		return retCode;
	}

	if(strncmp(p_buff, "HTTP/", 5) != 0)
	{
		sprintf( errMsg, "HTTPヘッダ解析エラー1");
		//printf("HTTP Status Line Error1 [%s]\n", p_buff);
		retCode = -1;
		return retCode;
	}
	
	/* 空白を検索 */
	if((p_tmp1 = strstr(p_buff, " ")) == NULL)
	{
		sprintf( errMsg, "HTTPヘッダ解析エラー2");
		//printf("HTTP Status Line Error2 [%s]\n", p_buff);
		retCode = -1;
		return retCode;
	}

	p_tmp1++;
	/* 次の空白を検索 */
	if((p_tmp2 = strstr(p_tmp1, " ")) == NULL)
	{
		sprintf( errMsg, "HTTPヘッダ解析エラー3");
		//printf("HTTP Status Line Error3 [%s]\n", p_buff);
		retCode = -1;
		return retCode;
	}

	/* ステータスコードを取得 */
	*p_tmp2 = 0;
	stsCode = atoi(p_tmp1);
	*p_tmp2++;
	memset(stsCodeStr,0,sizeof(stsCodeStr));
	strncpy(stsCodeStr,p_tmp2, sizeof(stsCodeStr)-1);

	switch( (int)(stsCode/100) )
	{
		case 2:
			retCode = 0;
			break;
		case 4:
			/* 設定値異常 */
		case 1:
		case 3:
		case 5:
		default :
			retCode = -1;
			sprintf( errMsg, "ＨＴＴＰステータスコード(%04d:%s)", stsCode, stsCodeStr);
			break;
	}

	return retCode;
}

int func_ssl_getResHeader( void )
{
	char	*p_buff;
	int		retCode;
	int		lineLen;

	while( 1 )
	{
		/* 1行データを取得する */
		lineLen = func_ssl_fgets(&p_buff);
		if( lineLen <= 0 )
		{
			retCode = -1;
			return retCode;
		}
		/* 先頭が改行の場合 */
		if( p_buff[0] == 0 )
		{
			/* 終了する */
			retCode = 0;
			break;
		}
	}
	return 0;
}


int func_ssl_getResBody( void )
{
	int 	retCode;
	int 	fileSize = 0;
	char 	*p_buff;
	char 	filePath[128];
	char	*p_tmp1;
	char	*p_tmp2;
	int		readSize;
	int		totalSize=0;
	FILE	*fp;						/* ファイルディスクリプタ */

	memset(g_SchFileName, 0, sizeof(g_SchFileName));

	while( 1 )
	{
		/* 1行データを取得する */
		readSize = func_ssl_fgets(&p_buff);
		if( readSize <= 0 )
		{
			retCode = -1;
			break;
		}

		/* ファイル名を検索 */
		if((p_tmp1 = strstr(p_buff, "Content-Disposition: ")) != NULL)
		{
			if((p_tmp1 = strstr(p_buff, "filename=")) != NULL)
			{
				p_tmp1+=9;

				if( strlen(p_tmp1) >= sizeof(g_SchFileName))
				{
					sprintf( errMsg, "ダウンロードするファイルがありません");
				}
				else
				{
					/* ステータスコードを取得 */
					strcpy(g_SchFileName, p_tmp1);
				}
			}
		}

		/* ファイルサイズを検索 */
		if((p_tmp1 = strstr(p_buff, "Content-Length: ")) != NULL)
		{
			p_tmp1+=16;
			fileSize = atoi(p_tmp1);
		}

		/* 先頭が改行の場合 */
		if( p_buff[0] == 0 )
		{
			p_buff += 2;
			break;
		}
	}

	/* ファイル名、ファイルサイズが取得できた場合 */
	if(( fileSize > 0 ) && ( g_SchFileName[0] != 0 ))
	{
		sprintf(filePath, "%s%s", g_curDir, g_SchFileName);

		if( (fp = fopen(filePath, "wb")) == NULL)
		{
			retCode = -1;
			sprintf( errMsg, "ファイルの作成に失敗しました。");
		}
		else
		{
			while( 1 )
			{
				/* バッファポインタが設定されている場合 */
				if( p_buff != NULL )
				{
					/* 残りのデータを読み込む */
					readSize = sslReadSize - (p_buff - sslBuff);
				}
				else
				{
					readSize = func_ssl_read(&p_buff);

					if( readSize <= 0 )
					{
						retCode = -1;
						break;
					}
				}

				/* ファイルサイズまでのデータを書き込み */
				if( (totalSize + readSize) > fileSize )
				{
					/* ファイルサイズを超える場合、補正して書き込む */
					fwrite( p_buff, sizeof( unsigned char ), (fileSize-totalSize), fp );
					/* 書き込みサイズを更新 */
					totalSize = fileSize;
					break;
				}
				else
				{
					/* ファイルサイズを超えない場合、全て書き込む */
					fwrite( p_buff, sizeof( unsigned char ), readSize, fp );
					/* 書き込みサイズを更新 */
					totalSize += readSize;
				}
				p_buff = NULL;
			}
			/* ファイルクローズ */
			fclose(fp);

			/* 書き込みデータサイズ判定 */
			if( totalSize == fileSize )
			{
				/* 正常終了 */
				retCode = 0;
			}
			else
			{
				/* 異常終了 */
				//printf("fileSize Error (%d) != (%d)\n", fileSize, totalSize);
				sprintf( errMsg, "ファイルサイズと実データが一致しません。");
				retCode = -1;
			}
		}
	}
	else
	{
		//printf("fileSize(%d) or fileName(%s) Error\n", fileSize, g_SchFileName);
		sprintf( errMsg, "ファイルサイズ、又は、ファイル名の取得に失敗しました。");
		/* 異常終了 */
		retCode = -1;
	}

	return retCode;
}

int func_ssl_filedownload( void )
{
    int retCode;

	/* SSL関連初期化 */
	p_sslBuff = NULL;
	sslReadSize = 0;

	/* レスポンスステータス取得 */
	retCode = func_ssl_getStatus();
	if( retCode < 0 )
	{
		return retCode;
	}

	/* レスポンスヘッダ取得 */
	retCode = func_ssl_getResHeader();
	if( retCode < 0 )
	{
		return retCode;
	}

	/* レスポンスボディ取得 */
	retCode = func_ssl_getResBody();
	if( retCode < 0 )
	{
		return retCode;
	}

	return retCode;
}


int func_schFileDownload( void )
{
	FILE				*fp;						/* ファイルポインタ */
	int					retCode=0;
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int					dataCnt;
	int					loopCnt;
	int					setType;
	int					dataPer;
	char				fileType[5];
	struct st_datptn	s_dataPtn;
	int					reqFlg=0;
	char				dwlMsg[13][PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	char				title[13][PV_MSG_SIZE_LOG] = {
							"[r]ＩＤ登録結果    ", 
							"[u]更新スケジュール", 
							"[m]月間スケジュール", 
							"[0]年間スケジュール", 
							"[1]年間スケジュール", 
							"[2]年間スケジュール", 
							"[3]年間スケジュール", 
							"[4]年間スケジュール", 
							"[5]年間スケジュール", 
							"[6]年間スケジュール", 
							"[7]年間スケジュール", 
							"[8]年間スケジュール", 
							"[9]年間スケジュール"};
	char				fileName[13][64];	/* エラーメッセージ */
	struct tm				localTime;		/* 現在時刻 */
	time_t					nowTime;		/* 現在時刻 */
	int					fileNum;
	char				tmp[256];
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	int					dispMode=0;
	int					dwlStart=0;
	int					dwlEnd=0;

	memset(&s_dataPtn, 0, sizeof(s_dataPtn));
	memset(&dwlMsg, 0x00, sizeof(dwlMsg));
	memset(&fileName, 0x00, sizeof(fileName));

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );

	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );

	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

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


		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュールファイルダウンロード> ==========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\n");
		printf("  発電所ＩＤ          :%s\n", g_ps_SCHCONF->plantId);
		printf("  ＭＡＣアドレス      :%s\n", g_PGCONINF.macAddr);
		printf("  配信サーバーＵＲＬ  :%s\n", g_PGCONINF.ipAddr);
		printf("--------------------------------------------------------------------------------------\n");
		for( loopCnt = 0; loopCnt < 13; loopCnt++)
		{
			memset(fileType, 0x00, sizeof(fileType));
			switch(loopCnt)
			{
				case 0 :
					sprintf(fileType, "%04d", 8888);
					break;
				case 1 :
					sprintf(fileType, "%04d", 0000);
					break;
				case 2 :
					sprintf(fileType, "%02d%02d", (localTime.tm_year%100), (localTime.tm_mon + 1)%100);
					break;
				default :
					sprintf(fileType, "999%d", (loopCnt-3));
					break;
			}
			sprintf(tmp, "    %s(%s):", title[loopCnt], fileType);
			printf("\033[0K");
			printf("%s", tmp);
			
			if( dwlMsg[loopCnt][0] != 0 )
			{
				printf("%s", dwlMsg[loopCnt]);
			}
			else if( fileName[loopCnt][0] != 0 )
			{
				printf("%s", fileName[loopCnt]);
			}
			else
			{
				printf("未実施");
			}
			printf("\n");
		}
		printf("======================================================================================\n");
		if( dispMode == 0 )
		{
			printf("ダウンロードするスケジュールファイルを指定してください。\n");
			printf("戻る:[q]  スケジュールファイル表示画面切替:[d]\n");
			printf("全スケジュール:[a]  ID登録結果:[r]  スケジュール（更新:[u] 月間:[m] 年間:[0～9]）\n");
			printf("    >>");
			fflush( NULL );
			g_keyByte = read( 0, &g_keyData, 1 );

			dwlStart = -1;
			dwlEnd = -1;
			if( g_keyData[0] == 'q')
			{
				break;
			}
			if( g_keyData[0] == 'd')
			{
				dispMode = (dispMode + 1) % 2;
			}
			else if( g_keyData[0] == 'a')
			{
				memset(&dwlMsg, 0x00, sizeof(dwlMsg));
				memset(&fileName, 0x00, sizeof(fileName));
				dwlStart = 0;
				dwlEnd = 13;
			}
			else
			{
				fileNum = -1;
				if( g_keyData[0] == 'r')
				{
					fileNum = 0;
				}
				else if( g_keyData[0] == 'u')
				{
					fileNum = 1;
				}
				else if( g_keyData[0] == 'm')
				{
					fileNum = 2;
				}
				else if( (g_keyData[0] >= '0') && (g_keyData[0] <= '9'))
				{
					fileNum = atoi(g_keyData) + 3;
				}

				if( fileNum != -1 )
				{
					memset(&dwlMsg[fileNum], 0x00, sizeof(dwlMsg[fileNum]));
					memset(&fileName[fileNum], 0x00, sizeof(fileName[fileNum]));
					dwlStart = fileNum;
					dwlEnd = fileNum+1;
				}
			}

			if( ( dwlStart >= 0 ) && ( dwlEnd >= 0 ) )
			{
				printf("\n    ダウンロード中・・・・・\n");
				for( loopCnt = dwlStart; loopCnt < dwlEnd; loopCnt++)
				{
					memset(fileType, 0x00, sizeof(fileType));
					switch(loopCnt)
					{
						case 0 :
							sprintf(fileType, "%04d", 8888);
							break;
						case 1 :
							sprintf(fileType, "%04d", 0000);
							break;
						case 2 :
							sprintf(fileType, "%02d%02d", (localTime.tm_year%100), (localTime.tm_mon + 1)%100);
							break;
						default :
							sprintf(fileType, "999%d", (loopCnt-3));
							break;
					}

					retCode = func_ssl_readmain(fileType);

					/* カーソルの位置からその行の右端までクリア */
					if( retCode < 0 )
					{
						memcpy(dwlMsg[loopCnt], errMsg, sizeof(dwlMsg[loopCnt]));
						printf("%s", dwlMsg[loopCnt]);

					}
					else
					{
						memcpy(fileName[loopCnt], g_SchFileName, sizeof(fileName[loopCnt]));
						printf("%s", fileName[loopCnt]);
					}
				}
			}
		}
		else
		{
			/* カーソルの位置からその行の右端までクリア */
			printf("戻る:[q] DWL画面切替:[d] ＩＤ登録結果:[r] スケジュール（更新:[u] 月間:[m] 年間:[0～9]）");
			memset( g_keyData, 0, sizeof( g_keyData ) );
			fflush( NULL );
			g_keyByte = read( 0, &g_keyData, 1 );

			fileNum = -1;
			if( g_keyData[0] == 'd')
			{
				dispMode = (dispMode + 1) % 2;
			}
			else
			{
				if( g_keyData[0] == 'q')
				{
					break;
				}
				else if( g_keyData[0] == 'r')
				{
					fileNum = 0;
				}
				else if( g_keyData[0] == 'u')
				{
					fileNum = 1;
				}
				else if( g_keyData[0] == 'm')
				{
					fileNum = 2;
				}
				else if( (g_keyData[0] >= '0') && (g_keyData[0] <= '9'))
				{
					fileNum = atoi(g_keyData) + 3;
				}

				if( (fileNum >= 0 ) && (fileName[fileNum][0] != 0 ))
				{
					/* 画面をクリア */
					printf("\033[2J");

					/* カーソルの位置を高さ０、横０に移動 */
					printf("\033[%d;%dH",0,0);
					sprintf(tmp, "%s%s", g_curDir, fileName[fileNum]);
					func_schfile_analize( 1, tmp);
				}
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	return 0;
}

int func_ssl_commdata_fileout( void )
{
	int					retCode=0;
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */


	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );

	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );

	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

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


		/* 装置種別と現在時刻の表示 */
		printf("= <ＳＳＬ通信ファイル出力設定> ================================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\n");
		printf("\n");
		printf("  出力設定          :%s\n", ((ssl_comm_output==0) ? "出力しない" : "出力する" ));
		printf("\n");
		printf("\n");
		printf("======================================================================================\n");
		printf("戻る:[q]    設定変更:[d]");
		fflush( NULL );
		g_keyByte = read( 0, &g_keyData, 1 );

		if( g_keyData[0] == 'q')
		{
			break;
		}
		if( g_keyData[0] == 'd')
		{
			ssl_comm_output = (ssl_comm_output + 1) % 2;
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	return 0;
}

int func_chg_updateFlg( void )
{
	int					retCode=0;
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	int					dispMode=0;


	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );

	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );

	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

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


		/* 装置種別と現在時刻の表示 */
		printf("= <固定スケジュール更新フラグ変更> ============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\n");
		printf("\n");
		printf("  現在の更新フラグ          :%d\n", ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr);
		printf("  次回の更新フラグ          :%d\n", ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext);
		printf("\n");
		if( dispMode == 0 )
			printf("現在の更新フラグが変更可能です。\n");
		else
			printf("次回の更新フラグが変更可能です。\n");
		printf("======================================================================================\n");
		printf("戻る:[q]    更新対象切替：[d]       更新フラグ:[0-9]");
		fflush( NULL );
		g_keyByte = read( 0, &g_keyData, 1 );

		if( g_keyData[0] == 'q')
		{
			break;
		}
		if( g_keyData[0] == 'd')
		{
			dispMode = (dispMode + 1) % 2;
		}
		if( ( g_keyData[0] >= '0') && ( g_keyData[0] <= '9') )
		{
			if( dispMode == 0 )
				ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = g_keyData[0] - 0x30;
			else
				ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = g_keyData[0] - 0x30;
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	return 0;
}

int func_chg_updateTime( void )
{
	int					retCode=0;
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	int					dispMode=0;
	time_t				chgTime;

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


		/* 装置種別と現在時刻の表示 */
		printf("= <更新スケジュール次回アクセス時刻変更> ======================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\n");
		printf("\n");

		localtime_r( &ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime, &localTime );
		printf("  現在の次回アクセス時刻     :%04d/%02d/%02d %02d:%02d:%02d\n",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		printf("\n");
		printf("--------------------------------------------------------------------------------------\n");
		printf("上記アクセス時刻からの変更秒を指定してください。\n");
		printf(" ≪例≫ ± 5分 : ± 300\n");
		printf("        ±10分 : ± 600\n");
		printf("        ±30分 : ±1800\n");
		printf(" >>");
		memset( g_keyData, 0, sizeof( g_keyData ) );
		fflush( NULL );
		g_keyByte = read( 0, &g_keyData, 1024 );
		chgTime = ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime + atoi(g_keyData);

		localtime_r( &chgTime, &localTime );

		printf(" 以下の時刻に変更しますか？（ｙ／ｎ）\n  [%04d/%02d/%02d %02d:%02d:%02d]\n",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec );
		printf(" >>");
//		fflush( NULL );
//		memset( g_keyData, 0, sizeof( g_keyData ) );
//		g_keyByte = read( 0, &g_keyData, 1 );
		func_cmn_KeyInput(PV_INPUT_ONLY);

		if( g_keyData[0] == 'y')
		{
			ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime = chgTime;
		}
		else if( g_keyData[0] == 'n')
		{
			retCode = 0;
		}
		break;
	}


	return 0;
}

int func_chg_buffYear( void )
{
	int					retCode=0;
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	int					dispMode=0;
	int					buffYearNow[PV_SCH_BUFF_NUM];
	int					buffYear[PV_SCH_BUFF_NUM];
	int					buffSelect;
	int					loopCnt;
	int					*p_buffYear;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );

	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );

	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 設定情報取得 */
	for( loopCnt = 0; loopCnt < PV_SCH_BUFF_NUM; loopCnt )
	{
		buffYear[loopCnt] = ps_g_SCHCTLFIL->s_schinfo[loopCnt].schYear;
		buffYearNow[loopCnt] = ps_g_SCHCTLFIL->s_schinfo[loopCnt].schYear;
	}
	buffSelect = ps_g_SCHCTLFIL->buffSelect;

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

		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュールバッファ面 強制年変更> =========================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\n");
		printf("\n");
		printf("  現在のスケジュールバッファ面(%d)の年数   :  %4d  -> ", buffSelect, buffYearNow[buffSelect]);
		if( buffYear[buffSelect] != buffYearNow[loopCnt] )
			printf("(%4d)\n", buffYear[buffSelect]);
		else
			printf(" %4d \n", buffYear[buffSelect]);

		printf("  次  のスケジュールバッファ面(%d)の年数   :  %4d  -> ", (buffSelect+1) % PV_SCH_BUFF_NUM, buffYearNow[(buffSelect+1) % PV_SCH_BUFF_NUM]);
		if( buffYear[buffSelect] != buffYearNow[loopCnt] )
			printf("(%4d)\n", buffYear[(buffSelect+1) % PV_SCH_BUFF_NUM]);
		else
			printf(" %4d \n", buffYear[(buffSelect+1) % PV_SCH_BUFF_NUM]);
		printf("\n");
		if( dispMode == 0 )
		{
			printf("現在のバッファ面の年が変更可能です。\n");
			p_buffYear = &buffYear[buffSelect];
		}
		else
		{
			printf("次  のバッファ面の年が変更可能です。\n");
			p_buffYear = &buffYear[(buffSelect+1) % PV_SCH_BUFF_NUM];
		}
		printf("======================================================================================\n");
		printf("戻る:[q]   変更対象切替：[d]    リセット:[0]    増加：[+]    減少：[-]    確定：[w]");
		fflush( NULL );
		g_keyByte = read( 0, &g_keyData, 1 );

		if( g_keyData[0] == 'q')
		{
			break;
		}
		if( g_keyData[0] == 'd')
		{
			dispMode = (dispMode + 1) % 2;
		}
		if( g_keyData[0] == '0')
		{
			*p_buffYear = 0;
		}
		if( g_keyData[0] == '+')
		{
			if( *p_buffYear < 9999 )
			*p_buffYear = *p_buffYear+1;
		}
		if( g_keyData[0] == '-')
		{
			if( *p_buffYear > 0 )
			*p_buffYear = *p_buffYear-1;
		}
		if( g_keyData[0] == 'w')
		{
			for( loopCnt = 0; loopCnt < PV_SCH_BUFF_NUM; loopCnt )
			{
				ps_g_SCHCTLFIL->s_schinfo[loopCnt].schYear = buffYear[loopCnt];
				buffYearNow[loopCnt] = buffYear[loopCnt];
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	return 0;
}

int func_chg_buffSelect( void )
{
	int					retCode=0;
	struct tm			localTime;		/* 現在時刻 */
	time_t				nowTime;		/* 現在時刻 */
	struct termio		ttyBackup;		/* 端末設定保存用 */
	struct termio		ttyChange;		/* 端末設定変更用 */
	int					buffSelect;

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );

	/* 変更用設定にコピー */
	ttyChange = ttyBackup;
	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );

	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 設定情報取得 */
	buffSelect = ps_g_SCHCTLFIL->buffSelect;

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

		/* 装置種別と現在時刻の表示 */
		printf("= <スケジュールバッファ面 切替> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("\n");
		printf("\n");
		printf("  現在のスケジュールバッファ面番号：%d  (登録年：%4d)\n", buffSelect, ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear);
		printf("  次  のスケジュールバッファ面番号：%d  (登録年：%4d)\n", (buffSelect+1) % PV_SCH_BUFF_NUM, ps_g_SCHCTLFIL->s_schinfo[(buffSelect+1) % PV_SCH_BUFF_NUM].schYear );
		printf("\n");
		printf("\n");
		printf("======================================================================================\n");
		printf("戻る:[q]          バッファ面切替：[d]");
		fflush( NULL );
		g_keyByte = read( 0, &g_keyData, 1 );

		if( g_keyData[0] == 'q')
		{
			break;
		}
		if( g_keyData[0] == 'd')
		{
			buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;
			ps_g_SCHCTLFIL->buffSelect = buffSelect;
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	return 0;
}
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 電力会社向け情報ファイル生成処理
 *
 * 機能：
 * 		PGCONINF.csv ファイルから電力会社向け情報ファイル（PGCONINF.dat）
 * 		を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_PGCONINF_Set( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd;							/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					strCheck;					/* 文字列確認 */
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	struct st_PGCONINF	*ps_PGCONINF;				/* ファイルポインタ */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	ps_PGCONINF = &g_PGCONINF;
	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcat(csvFilePath, PV_FNAME_PGCONINF );
	strcat(csvFilePath, ".csv");

	/* ファイルの中身をALL0クリア */
	memset( ps_PGCONINF, 0, sizeof(struct st_PGCONINF) );

	/* 電力会社向け情報の CSVファイルが存在しない場合 */
	if( access( csvFilePath, R_OK ) != 0 )
	{
		/* 終了 */
		return( 0 );
	}

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			/* CSV処理カウンタ＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（1文字以下）はスキップ */
			if( strlen( readBuff ) < 1) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );

			/* CSVデータ数が不正の場合 */
			if( ( csvCount != 7) && (csvCount != 8) )	/* [20210330] Chg */
			{
				//sprintf( errMsg, "CSV-Format Error (Data is not 7). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 発電所ＩＤ名称の取得 */
			if( strlen(s_sepStr.data[0]) != 26 )
			{
				//sprintf( errMsg, "CSV-Format Error (Power Plant ID Name Error). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 数値以外の文字が指定されている場合 */
			strCheck = func_cmn_string_check2( s_sepStr.data[0], 27 );
			if( ( strCheck == -1 ) || (strCheck != 0x0001) )
			{
				//sprintf( errMsg, "CSV-Format Error (Power Plant ID exist no Number ) . File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PGCONINF->plantId, s_sepStr.data[0], 52);


			/* ＭＡＣアドレス名称の取得 */
			if( strlen(s_sepStr.data[1]) != 12 )
			{
				//sprintf( errMsg, "CSV-Format Error (Mac Address Error). File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 数値、16進の大文字以外の文字が指定されている場合 */
			strCheck = func_cmn_string_check2( s_sepStr.data[1], 13 );
			if( ( strCheck == -1 ) || (strCheck & ~(int)0x0011) )
			{
				//sprintf( errMsg, "CSV-Format Error (MAC address is not hexadecimal or Characters is not uppercase ) . File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PGCONINF->macAddr, s_sepStr.data[1], 13);

			strCheck = func_cmn_get_mac_address(ps_PGCONINF->macAddr);
			if( strCheck == -1 )
			{
				//sprintf( errMsg, "CSV-Format Error (Get MAC address Error ) . File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* スケジュール情報配信サーバＵＲＬ */
			if( strlen(s_sepStr.data[2]) > 127 )
			{
				//sprintf( errMsg, "CSV-Format Error (URL) Length Long. File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PGCONINF->ipAddr, s_sepStr.data[2], 128);

			/* スケジュール情報配信サーバＳＳＨポート番号の取得 */
			workData = atol( s_sepStr.data[3] );
			if( workData == 0 )
			{
				//sprintf( errMsg, "CSV-Format Error (SSH Port). File[%s] Line[%d] DAT[4]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF->portNumb = workData;


			/* ＮＴＰサーバＩＰアドレスの取得（orホスト名） */
			if( ( strlen(s_sepStr.data[4]) > 31 ) || ( strlen(s_sepStr.data[4]) < 7 ) )
			{
				//sprintf( errMsg, "CSV-Format Error (IP Address) Length Short or Long. File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 数値・英文字・ハイフン・アンダースコア・ピリオド以外の文字が指定されている場合 */
			strCheck = func_cmn_string_check2( s_sepStr.data[4], 128 );
			if( strCheck == -1 )
			{
				//sprintf( errMsg, "CSV-Format Error (IP Address) Unknown Charactor. File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* IPアドレス指定（数値とピリオドのみ）で文字列長が16文字以上の場合 */
			if( ( strCheck == ((0x0001|0x0002)) ) && ( strlen(s_sepStr.data[4]) > 15 ) )
			{
				//sprintf( errMsg, "CSV-Format Error (IP Address) Length Short or Long. File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PGCONINF->ntpIpAddr, s_sepStr.data[4], 128);

			/* ＮＴＰサーバＳＳＨポート番号の取得 */
			workData = atol( s_sepStr.data[5] );
			if( workData == 0 )
			{
				//sprintf( errMsg, "CSV-Format Error (SSH Port). File[%s] Line[%d] DAT[6]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF->ntpPortNumb = workData;

			/* 出力変化時間の取得 */
			workData = atol( s_sepStr.data[6] );
			if( (workData < 5 ) || ( workData > 10 ) )
			{
				//sprintf( errMsg, "CSV-Format Error (Outoput Change Time). File[%s] Line[%d] DAT[7]", csvFilePath, syoriCount );
				//commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF->chgTime = workData;

			retCode = 1;
		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}

	/* 終了 */
	return( retCode );	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 電力会社向け情報ファイル生成処理
 *
 * 機能：
 * 		引数 strPointer で指定されたアドレスに格納されている文字列を確認し、内
 *		容毎に以下の値を返す。
 * 戻り値：
 *		 0x0001：数字が含まれている場合
 *		 0x0002：ピリオドが含まれている場合
 *		 0x0004：ハイフン又は、アンダースコアが含まれている場合
 *		 0x0008：小文字アルファベットが含まれている場合
 *		 0x0010：大文字アルファベットが含まれている場合
 *		 0x0100：アルファベットが16進数（A-F,a-f）以外が含まれている場合
 *		-1：上記以外の文字（空白、記号、タブ、全角など）が含まれている場合
 *		    または文字列長が引数（strlenMax）による指定文字以上の場合
 */
int	func_cmn_string_check2( char *strPoint, char strlenMax )
{
	int					chkLength;					/* データ確認長 */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */

	/* 指定ポインタの文字列長を確認する */
	if( ( chkLength = strnlen( strPoint, strlenMax+1 ) ) >= strlenMax )
		return( -1 );

	/* 文字列の最後まで確認する */
	for( loopCnt = 0 ; loopCnt < chkLength ; loopCnt++ )
	{
		/* ハイフンまたはアンダースコアの場合 */
		if( (*(strPoint + loopCnt) == '-') || (*(strPoint + loopCnt) == '_') )
		{
			retCode |= 0x0004;
		}
		/* アルファベット（小文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'a') && (*(strPoint + loopCnt) <= 'z') )
		{
			retCode |= 0x0008;
			/* 16進数の文字以外の場合 */
			if(*(strPoint + loopCnt) > 'f')
			{
				retCode |= 0x1000;
			}
		}
		/* アルファベット（大文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'A') && (*(strPoint + loopCnt) <= 'Z') )
		{
			retCode |= 0x0010;
			/* 16進数の文字以外の場合 */
			if(*(strPoint + loopCnt) > 'F')
			{
				retCode |= 0x1000;
			}
		}
		/* 数値の場合 */
		else if( (*(strPoint + loopCnt) >= '0') && (*(strPoint + loopCnt) <= '9') )
		{
			retCode |= 0x0001;
		}
		/* ピリオドの場合 */
		else if((*(strPoint + loopCnt) == '.'))
		{
			retCode |= 0x0002;
		}
		else
		{
			return( -1 );
		}
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 電力会社向け情報ファイル生成処理
 *
 * 機能：
 * 		引数 strPointer が全て'0'の場合、収集装置のＭＡＣアドレスを格納する。
 * 戻り値：
 *		 0：正常
 *		-1：異常
 */
int	func_cmn_get_mac_address( char *strPoint )
{
	FILE					*fp;
	char					*ptr;
	int						retCode = 0;
	char					checkData[512];
	int						loopCnt;
	char					mac[13]={"000000000000"};

	/* ＭＡＣアドレス取得チェック */
	if( strncmp( &mac[0], strPoint, 12 ) != 0 )
	{
		return retCode;
	}

	/* 領域を初期化  */
	memset(strPoint, 0, 13);

	/* コマンドを実行してntpqコマンドの結果をパイプから取得 */
	if( (fp = popen( "LANG=C /sbin/ifconfig", "r" )) == NULL )
	{
		//commfunc_syslog_msgset( "FILEMAKE", "ERROR: ifconfig Command Error" );
		retCode = -1;
	}
	else
	{
		/* 最初の１行を取り出し */
		fgets( checkData, 512, fp );

	    if ((ptr = strstr(checkData, "HWaddr")) != NULL )
		{
			ptr+=7;
			for( loopCnt = 0; loopCnt < 6; loopCnt++)
			{
				strncpy(&strPoint[loopCnt*2], ptr, 2);
				ptr+=3;
			}

			for( loopCnt = 0; loopCnt < 12; loopCnt++)
			{
				/* アルファベット（小文字）の場合 */
				if(( strPoint[loopCnt] >= 'a') && (strPoint[loopCnt] <= 'f') )
				{
					strPoint[loopCnt] -= 0x20;
				}
			}
		}
		else
		{
			//commfunc_syslog_msgset( "FILEMAKE", "ERROR: ifconfig Command [HWaddr] is Not Found" );
			retCode = -1;
		}
		/* パイプを閉じる */
		pclose( fp );
	}
	return retCode;
}

int func_ssl_data_file_write( char *preFix, char *p_buff, int length )
{
	struct tm				timeData;					/* 時刻情報 */
	struct timeval			timeVal;					/* 現在積算秒 */
	char					filePath[512];				/* 出力ファイルパス */
	int						retCode = 0;				/* 終了コード */

	if( sslfp == NULL )
	{
		/* 積算秒を取得 */
		gettimeofday(&timeVal, NULL);
		/* 時刻情報に変換 */
		localtime_r( &timeVal.tv_sec, &timeData );

		timeData.tm_year += 1900;
		timeData.tm_mon+= 1;

		sprintf(filePath, "%04d%02d%02d%02d%02d%02d%03d_%s.bin", 
			timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, (timeVal.tv_usec/1000 ), preFix);

		/* 保存ファイルをオープン */
		if( (sslfp = fopen(filePath, "wb")) == NULL)
		{
			retCode = -1;
		}
		else
		{
			retCode = 0;
		}
	}

	if( ( sslfp != NULL ) && ( p_buff != NULL ) && ( length > 0 ))
	{
		fwrite( p_buff, sizeof( unsigned char ), length, sslfp );
	}

	return retCode;

}

int func_ssl_data_file_close( void )
{
	if( sslfp != NULL )
	{
		/* ファイルクローズ */
		fclose(sslfp);

		/* 初期化 */
		sslfp = NULL;
	}

}

/* [20190809] Add St */
int func_schfile_report( void )
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						dataBlock;
	char					temp[256];
	int						fileSize;
	unsigned  char			buff[20065+1];
	int						loopCnt;
	int						loopCnt2;
	int						loopCnt3;
	char					*p_buff;
	int						year;
	int						month;
	int						day;
	int						hour;
	int						min;
	int						sec;
	int						dataNum;
	int						checksum;
	int						updateFlg;
    time_t 					mk_tim;
    struct tm 				mk_time;
    int 					type;
	char 					FilePath[256];
	char 					fileName[256];
	char					*namePos;
	char					*path;
	char					inputPath[256];
	char					dirPath[256];
	char					typeStr[100];
	char					checkSumRet[32];
	char					monthData[8*32*48];
	char					sameFlg;
	char					sameCheck;
	char					gaiyo[256];
	char					plant_id[32];
	char					kubun[8];
	char					sch_id[16];
	int						days;

	printf("電力会社へのレポートを出力するスケジュールファイルのパスを入力して下さい\n");
	printf("※ファイル名のみの場合は[/home/nksystem/DWLDATA/]を参照\n");
	printf(">>");
	fflush( NULL );
	memset( g_keyData, 0, sizeof( g_keyData ) );
	g_keyByte = read( 0, &g_keyData, 1024 );

	memset(inputPath, 0, sizeof(inputPath));
	memcpy(inputPath, g_keyData, g_keyByte-1);

	if((path = strstr(inputPath, "/")) != NULL)
	{
		memcpy(FilePath, inputPath, sizeof(FilePath));
	}
	else
	{
		sprintf(FilePath, "%s%s", "/home/nksystem/DWLDATA/", inputPath);
	}

	//ファイル名の取得
	memcpy( fileName, &FilePath[strlen(FilePath)-55], 55 );

	//発電所IDの取得
	memset( plant_id, 0, sizeof(plant_id));
	memcpy( plant_id, &FilePath[strlen(FilePath)-46], 26 );

	//要求区分の取得
	memset( kubun, 0, sizeof(kubun));
	memcpy( kubun, &FilePath[strlen(FilePath)-51], 4 );
	
	
	if( (fp = fopen(FilePath, "rb")) == NULL)
	{
		printf("OPEN ERR\n");
		return -1;
	}

	fileSize = fread( buff, sizeof( unsigned char ), sizeof(buff), fp );

	if( fileSize <= 0 )
	{
		printf("fileSize ERR[%d]\n", fileSize);
		return -1;
	}


	/* ファイルパスの後ろ55文字（スケジュールファイル名）に移動 */
	namePos = &FilePath[strlen(FilePath)-55];

	memset(temp, 0, sizeof(temp));
	memcpy(temp, namePos, 3);

	if(strncmp(temp, "ERR", 3) == 0)
	{
		printf("==========================================\n");
		printf("フォーマット    ：≪アプリケーションエラー≫\n");
		printf("エラーメッセージ：%s\n", buff );
		printf("==========================================\n");
	}
	else
	{
		type = atoi(temp);

		switch( type )
		{
			case PV_SCH_FMT_NO_ID :				/* フォーマットNo（ID登録結果） */
				sprintf(typeStr, "ID登録確認結果");
				break;
			case PV_SCH_FMT_NO_YEAR :			/* フォーマットNo（年間） */
				sprintf(typeStr, "固定スケジュール");
				break;
			case PV_SCH_FMT_NO_MONTH :			/* フォーマットNo（月間） */
				sprintf(typeStr, "固定スケジュール");
				break;
			case PV_SCH_FMT_NO_UPDATE :			/* フォーマットNo（更新） */
				sprintf(typeStr, "更新スケジュール");
				break;
			default :
			printf("サポート外のファイルです。\n");
			return -1;
		}

		printf("-------------------------------------\n");
		printf("発電所ID：%s\n", plant_id);
		printf("要求区分：%s", kubun);
		switch( type )
		{
			case PV_SCH_FMT_NO_ID :				/* フォーマットNo（ID登録結果） */
				printf("（ID 登録確認）\n");
				break;
			case PV_SCH_FMT_NO_YEAR :			/* フォーマットNo（年間） */
				printf("（固定スケジュール：更新フラグ%s）\n", &kubun[3]);
				break;
			case PV_SCH_FMT_NO_MONTH :			/* フォーマットNo（月間） */
				printf("（固定スケジュール）\n");
				break;
			case PV_SCH_FMT_NO_UPDATE :			/* フォーマットNo（更新） */
				printf("（更新スケジュール）\n");
				break;
		}
		printf("-------------------------------------\n\n");
		printf("-------------------------------------\n");


		p_buff = &buff[0];
		dataBlock = func_cmn_bin2Num(p_buff, 6);
		p_buff+=6;

		printf("ファイル名：%s（%s：黄色部分）\n", fileName, typeStr );
		printf("ファイル実体部分：水色\n");
		printf("        データ数：%d\n", dataBlock );

		if( type == PV_SCH_FMT_NO_ID )
		{
			pvct_cmn_byte2str(p_buff, temp, 26);
			printf("        発電所ID：%s\n", temp);
			p_buff+=26;		/* 発電所ID */
			printf("        登録確認結果：%d（%s）\n", *p_buff, ((*p_buff==0) ? "登録済" : "未登録" ));
			p_buff++;
		}
		else
		{
			for( loopCnt=0; loopCnt < dataBlock; loopCnt++)
			{
				pvct_cmn_byte2str(p_buff, sch_id, 10);
				p_buff+=10;		/* スケジュールID */
				pvct_cmn_byte2str(p_buff, plant_id, 26);
				p_buff+=26;		/* 発電所ID */

				year = func_cmn_bin2Num(p_buff, 4);
				p_buff+=4;
				month = func_cmn_bin2Num(p_buff, 2);
				p_buff+=2;
				day = func_cmn_bin2Num(p_buff, 2);
				p_buff+=2;
				hour = func_cmn_bin2Num(p_buff, 2);
				p_buff+=2;
				min = func_cmn_bin2Num(p_buff, 2);
				p_buff+=2;
				if( loopCnt == 0 )
					sprintf(gaiyo, "%04d/%02d/%02d %02d:%02d", year, month, day, hour, min);
				dataNum = func_cmn_bin2Num(p_buff, 5);
				p_buff+=5;
				mk_tim = func_cmn_mktime(year, month, day, hour, min, 0);

				if( loopCnt == 0 )
				{
					printf("        スケジュールID ：%s\n", sch_id);
					printf("        発電所ID：%s\n", plant_id);
					if( dataBlock > 1 )
					{
						printf("        制御年月日時分：%04d/%02d/%02d %02d:%02d ～ ", year, month, day, hour, min);
						days = func_display_get_days( year+((month+dataBlock)/12), (month+dataBlock-1)%12 );
						printf("%04d/%02d/%02d %02d:%02d\n", year+((month+dataBlock)/12), (month+dataBlock-1)%12, days, 24, 0);
						printf("        ※詳細は、添付資料2.1.③.c.3 参照\n");
						printf("        ※詳細は、添付資料2.2.③.c.3 参照\n\n");
						sprintf(&gaiyo[strlen(gaiyo)], "～%04d/%02d/%02d %02d:%02d",  year+((month+dataBlock)/12), (month+dataBlock-1)%12, days, 24, 0);
						

						printf("        データ数：%d\n", dataBlock );
						printf("        スケジュールID ：%s\n", sch_id);
						printf("        発電所ID：%s\n\n", plant_id);
					}
				}

				printf("        制御年月日時分：%04d/%02d/%02d %02d:%02d\n", year, month, day, hour, min);
				printf("        出力制御率データ数：%d\n", dataNum);
				checksum = 0;
				memset(monthData, 0, sizeof(monthData));
				sameFlg = 0;
				if( dataNum >= (48*28) )
				{
					sameFlg = 1;
				}
				for( loopCnt2=0; loopCnt2 < dataNum; loopCnt2++ )
				{
					if( loopCnt2 == 0 )
					{
						sameCheck = *p_buff;
					}
					else if( sameFlg == 1 )
					{
						if( sameCheck != *p_buff )
						{
							sameFlg = 0;
						}
					}
					sprintf(&monthData[strlen(monthData)], "%d%%", *p_buff);
					if( loopCnt2+1 != dataNum )
					{
						sprintf(&monthData[strlen(monthData)], "、");
						if( (loopCnt2+1) % 12 == 0 )
						{
							sprintf(&monthData[strlen(monthData)], "\n                    ");
						}
					}
					checksum += *p_buff;
					p_buff++;
					mk_tim += PV_SCH_TI_SEC;
				}
				if( sameFlg == 1 )
					printf("        出力制御率：%d%%、%d%%、・・・、%d%% （全て %d%%）\n", sameCheck, sameCheck, sameCheck, sameCheck);
				else
					printf("        出力制御率：%s\n", monthData);

				if( type == 203 )
				{
					updateFlg = *p_buff;
					printf("        固定スケジュール更新フラグ：%d\n", updateFlg);
					p_buff++;
				}

				printf("        チェックサム：%d%d\n", *p_buff, *(p_buff+1));
				p_buff+=2;

				if( type == 203 )
				{
					/* 次回アクセス日時 */
					year = func_cmn_bin2Num(p_buff, 4);
					p_buff+=4;
					month = func_cmn_bin2Num(p_buff, 2);
					p_buff+=2;
					day = func_cmn_bin2Num(p_buff, 2);
					p_buff+=2;
					hour = func_cmn_bin2Num(p_buff, 2);
					p_buff+=2;
					min = func_cmn_bin2Num(p_buff, 2);
					p_buff+=2;
					sec = func_cmn_bin2Num(p_buff, 2);
					p_buff+=2;
					printf("        次回アクセス日時：%04d/%02d/%02d %02d:%02d:%02d\n", year, month, day, hour, min, sec);
				}
				printf("\n");
			}
		}
		printf("-------------------------------------\n\n");
		printf("==========================================\n");
		switch( type )
		{
			case PV_SCH_FMT_NO_ID :				/* フォーマットNo（ID登録結果） */
				break;
			case PV_SCH_FMT_NO_YEAR :			/* フォーマットNo（年間） */
				printf("■WireSharkのパケット解析資料で使用\n\n");
				printf("概要：%s\n\n", gaiyo);
				printf("==========================================\n");
				break;
			case PV_SCH_FMT_NO_MONTH :			/* フォーマットNo（月間） */
				break;
			case PV_SCH_FMT_NO_UPDATE :			/* フォーマットNo（更新） */
				printf("■WireSharkのパケット解析資料で使用\n\n");
				printf("概要：%s～、データ数：%d\n", gaiyo, dataNum);
				printf("固定スケジュール更新フラグ：%d\n", updateFlg);
				printf("次回アクセス日時：%04d/%02d/%02d %02d:%02d:%02d\n\n", year, month, day, hour, min, sec);
				printf("==========================================\n");
				break;
		}

	}
	func_cmn_KeyInput(PV_INPUT_MSG);

	return 0;
}
int pvct_cmn_byte2str(char *byteData, char *strData, int byteNum)
{
	int						loopCnt;

	for( loopCnt = 0; loopCnt < byteNum; loopCnt++ )
	{
		sprintf(&strData[loopCnt], "%d", byteData[loopCnt]);
	}
	strData[loopCnt]=0;
	
	return 0;
}
/* [20190809] Add Ed */
