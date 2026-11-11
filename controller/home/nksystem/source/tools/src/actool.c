/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20230830] 初版
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

#define	OWN_PROC_NAME	"ACTOOL"					/* 自プロセス名 */
#define	PV_INPUT_MSG				"キーを入力して下さい。"
#define	PV_INPUT_ANYKEY_MSG			"任意のキー押下でメニューに戻ります"
#define	PV_INPUT_ONLY				"INPUTONLY"
#define PV_CWD_PATH_SIZE			512


struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_EMCTLFIL		*ps_g_EMCTLFIL;				/* エネルギーマネジメント管理ファイル */
struct st_fileIndex     s_fileIndex[] =				/* ファイル定義テーブル（必須ファイル） */
{
    {   PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
    {   PV_FILENO_EMCTLFIL, (void **)&ps_g_EMCTLFIL, 0, 0, 0 },
    {   0,                                     NULL, 0, 0, 0 }
};

struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};

struct st_datptn{
	char				num;
	char				data[256];
	char				dataEnd;
	char				montCnt;
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


struct winsize			termSize;					/* 端末の画面サイズ */
static int				mainte_flg = 0;				/* モニタ専用フラグ */
static int				signal_flg = 0;				/* シグナル受信フラグ */
struct tm				g_timeData;					/* 時刻情報 */
struct timeval			g_Time;					/* 現在積算秒 */
static char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static char				g_curDir[PV_CWD_PATH_SIZE];


/* シグナル受信時の処理 */
void	func_signal_recv( int signum )
{
	/* シグナル受信フラグセット */
	signal_flg = 1;
}

/* 関数定義 */
void	func_termsize_get();
void	func_displaysize_set();
int		func_display_get_days( int, int );
void 	func_display_emsorderinfo( void );
void 	func_order_regist( void );
void 	func_notify_delete( void );

int 	pvct_cmn_str2byte(char *strData, char *byteData, char strLen);
int 	func_cmn_bin2Num(char *strData, int byteNum);
int 	func_cmn_time_check( struct tm *timData );
time_t 	func_cmn_mktime( short year, char month, char day, char hour, char min, char sec);
void 	func_cmn_KeyInput( char *msg );
int 	func_sep_data( char *pc_data, struct st_sepStr *ps_sepStr );
int 	func_cmn_set_tim( void );

/* 外部関数定義 */
extern	long long commfunc_pow10abs( short );


static char					g_keyData[1024];
static int					g_keyByte;
static FILE					*sslfp=NULL;				/* ファイルディスクリプタ */
static int					ssl_comm_output=0;


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 
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
	/* 使用ファイルをオープンしてＭＭＡＰ化 */
	retCode = commfunc_file_open( s_fileIndex );
	if(retCode < 0)
	{
		printf("%s : File Resource Open Error!\n", OWN_PROC_NAME );
		return( -1 );
	}

	memset(g_curDir, 0, sizeof(g_curDir));
	getcwd(g_curDir, PV_CWD_PATH_SIZE);
	g_curDir[strlen(g_curDir)]='/';


	/* 終了するまで繰り返す */
	while( 1 )
	{
		/* 画面をクリア */
		printf("\033[2J");
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);
	
		printf("= ACシステム管理ツール ============================== [Version:%s] =\n", OWN_VERSION );
		printf("--------------------------------------------------------------------------------\n");
		printf("   1.指令情報表示（サポート終了）\n");
		printf("   2.指令情報登録\n");
		printf("   3.通知情報削除\n");
		printf("--------------------------------------------------------------------------------\n");
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
			func_display_emsorderinfo();
		}
		else if( menuNumb == 2 )
		{
			func_order_regist();
		}
		else if( menuNumb == 3 )
		{
			func_notify_delete();
		}
		else if( menuNumb == 4 )
		{
		}
		else if( menuNumb == 5 )
		{
		}
		else if( menuNumb == 10 )
		{
		}
/* [20170411] Add St */
		else if( menuNumb == 11 )
		{
		}
		else if( menuNumb == 12 )
		{
		}
/* [20170411] Add Ed */
		else if( menuNumb == 15 )
		{
		}
		else if( menuNumb == 20 )
		{
		}
		else if( menuNumb == 21 )
		{
		}
		else if( menuNumb == 22 )
		{
		}
/* [20190809] Add St */
		else if( menuNumb == 23 )
		{
		}
/* [20190809] Add Ed */
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

	/* 使用ファイルをクローズ */
	commfunc_file_close( s_fileIndex );


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




void func_order_regist( void )
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
	struct st_ORDERPARA		orderpara;
	int						dataNum;
	int						baseNum=0;
	int						valueNum;
	struct tm 				data_conv;
	struct timeval			eventTime;
	int						category=0;
	struct st_ORDERPARA		*ps_orderpara;

	memset(&orderpara, 0, sizeof(orderpara));

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
		printf("= <ストックイベント設定> ===============================[ %04d/%02d/%02d %02d:%02d:%02d ]=\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );

		///////////////////////////////////////////
		//	203：更新スケジュール
		func_cmn_set_tim();

		sprintf(orderpara.eventId, "%04d%02d%02d_%02d%02d%02d",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec);

		/* 指令種別 */
		sprintf(orderpara.orderStr, AWS_ORDER_TYPE_AC_STR);
		orderpara.orderType = AWS_ORDER_TYPE_AC;

		/* デバイスID */
		sprintf(orderpara.deviceId, ps_g_EMCTLFIL->deviceId, sizeof(ps_g_EMCTLFIL->deviceId));

		/* 時間単位 */
		sprintf(orderpara.timeUnit, "min");
		orderpara.timeConv = 60;
		
		/* 指令値単位 */
		sprintf(orderpara.valueUnit, "kW");
		orderpara.valueConv = 1000;

		sprintf(orderpara.timeStamp, "%04d-%02d-%02d %02d:%02d:%02d",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec);



		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("    ・デバイスＩＤ          :%s\n", orderpara.deviceId);
		if( step > 0 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・商品区分      :(%d)%s\n",orderpara.category, orderpara.categoryStr);
		}
		if( step > 1 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・イベント開始日時    :%s\n",orderpara.eventTimeStr);
		}
		if( step > 2 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・継続時間            :%d (%s)\n", dataNum, orderpara.timeUnit);
		}
		if( step > 3 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・ベース値            :%d\n", baseNum);
		}
		if( step > 4 )
		{
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・Value値             :%d\n", valueNum);
		}
		switch( step )
		{
			case 0 :	/* カテゴリ値 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・カテゴリを入力してください。\n");
				printf("\033[0K");	printf("        1：一次調整力(未対応)\n");
				printf("\033[0K");	printf("        2：二次調整力(1)(未対応)\n");
				printf("\033[0K");	printf("        3：二次調整力(2)(未対応)\n");
				printf("\033[0K");	printf("        4：三次調整力(1)\n");
				printf("\033[0K");	printf("        5：三次調整力(2)\n");
				printf("\033[0K");	printf("        6：調整力公募(非使用)\n");
				printf("\033[0K");	printf("        7：容量市場(実需給)\n");
				printf("\033[0K");	printf("        8：容量市場(実効性テスト)\n");
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				category = atoi( g_keyData );

				switch(category)
				{
					case 4 :
						sprintf(orderpara.categoryStr, "%s", AWS_CATEGORY_REPLACEMENT_RESERVE);
						orderpara.category = AWS_CATEGORY_RR;
						break;
					case 5 :
						sprintf(orderpara.categoryStr, "%s", AWS_CATEGORY_REPLACEMENT_RESERVE_FOR_FIT);
						orderpara.category = AWS_CATEGORY_RR;
						break;

					case 7 :
						sprintf(orderpara.categoryStr, "%s", AWS_CATEGORY_CAPACITYMARKET);
						orderpara.category = AWS_CATEGORY_CM;
						/* 実需給指令 */
						orderpara.cm_exam = 0;
						break;

					case 8 :
						sprintf(orderpara.categoryStr, "%s", AWS_CATEGORY_CAPACITYMARKET);
						orderpara.category = AWS_CATEGORY_CM;
						/* 実効性テスト指令 */
						orderpara.cm_exam = 1;
						break;
					case 1 :
					case 2 :
					case 3 :
					case 6 :
					default:
						sprintf( errMsg, "入力データが正しくありません");
						retCode = -1;
						break;

				}

				step++;
				break;

			case 1 :	/* 制御開始日時 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・制御開始日時を指定して下さい。<例>2022年11月5日13:30＝＞202211051330\n");
				printf("\033[0K");	printf("      ※ 4桁以下の場合は、現在時刻からの分オフセットとする<例>5＝＞5分後\n");
				printf("\033[0K");	printf("      ※ 改行のみの場合は現在時刻からとする\n");
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				if( g_keyByte == 1 )
				{
					eventTime = g_Time;
					localtime_r( &eventTime.tv_sec, &data_start );
					memcpy(&data_start, &g_timeData, sizeof(g_timeData));
				}
				else if( g_keyByte < 5 )
				{
					offsetsec = atoi( g_keyData );
					offsetsec *= 60;
					eventTime.tv_sec = g_Time.tv_sec + offsetsec;
					eventTime.tv_sec -= (eventTime.tv_sec % 60);

					localtime_r( &eventTime.tv_sec, &data_start );

					data_start.tm_year += 1900;
					data_start.tm_mon+= 1;
					
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
					data_start.tm_hour = atoi(&tmpDate[8]);	tmpDate[8]=0;
					data_start.tm_mday = atoi(&tmpDate[6]);	tmpDate[6]=0;
					data_start.tm_mon  = atoi(&tmpDate[4]);	tmpDate[4]=0;
					data_start.tm_year = atoi(&tmpDate[0]);	tmpDate[0]=0;
					data_start.tm_sec  = 0;

					data_conv = data_start;
					data_conv.tm_year -= 1900;
					data_conv.tm_mon -= 1;
					eventTime.tv_sec = mktime(&data_conv);
					eventTime.tv_usec = 0;
				}
				data_start.tm_sec  = 0;
				sprintf(orderpara.eventTimeStr, "%04d/%02d/%02d %02d:%02d",
							data_start.tm_year, data_start.tm_mon, data_start.tm_mday,
							data_start.tm_hour, data_start.tm_min );

				/* イベント開始時刻(sec) */
				orderpara.eventStTimeSec = eventTime;
				step++;
				break;


			case 2 :	/* 継続時間 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・イベントの継続時間(%s)数を入力して下さい。\n", orderpara.timeUnit);
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				dataNum = atoi( g_keyData );

				if( (dataNum < 0))
				{
					sprintf( errMsg, "作成可能範囲外です。");
					retCode = -1;
					break;
				}
				/* 継続時間 */
				sprintf(orderpara.duration, "%d%s", dataNum, orderpara.timeUnit);
				/* イベント終了時間 */
				orderpara.eventEdTimeSec.tv_sec = orderpara.eventStTimeSec.tv_sec + dataNum*orderpara.timeConv;

				step++;
				break;


			case 3 :	/* ベース値 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・ベース値(%s)を入力してください。\n", orderpara.valueUnit);
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				baseNum = atoi( g_keyData );

				step++;
				break;


			case 4 :	/* Value値 */
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("    ・Value値(%s)を入力してください。\n", orderpara.valueUnit);
				printf("\033[0K");	printf("      >>");
				func_cmn_KeyInput(NULL);

				valueNum = atoi( g_keyData );

				step++;
				break;

			case 5 :
				orderpara.s_interval[0].statFlag = 1;
				orderpara.s_interval[0].duration.tv_sec = dataNum;
				orderpara.s_interval[0].value = valueNum;
				orderpara.s_interval[0].base = baseNum;
				orderpara.s_interval[0].startSec = orderpara.eventStTimeSec;
				orderpara.s_interval[0].endSec = orderpara.eventEdTimeSec;


				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("\n");
				printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
				printf("\033[0K");	printf("    イベントＩＤ     ：%s\n", orderpara.eventId );
				printf("\033[0K");	printf("    指示種別         ：%s\n", orderpara.orderStr );
				printf("\033[0K");	printf("    商品区分         ：(%d)%s\n", orderpara.category, orderpara.categoryStr );
				printf("\033[0K");	printf("    イベント開始時刻 ：%s\n", orderpara.eventTimeStr);
				printf("\033[0K");	printf("    継続時間         ：%d(%s)\n", orderpara.s_interval[0].duration.tv_sec, orderpara.timeUnit);
				printf("\033[0K");	printf("    基準値           ：%d\n", orderpara.s_interval[0].base);
				printf("\033[0K");	printf("    指令値           ：%d\n", orderpara.s_interval[0].value);
				printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
				printf("\033[0K");	printf("  上記の指令情報を登録しますか？（ｙ／ｎ）\n");
				printf("\033[0K");	printf("  >>");
				func_cmn_KeyInput(NULL);

				if( (g_keyByte == 2 ) && (g_keyData[0] == 'y') )
				{
					retCode = 1;
				}
				else
				{
					retCode = 0;
				}
				step++;
				break;
			default :
				break;
		}
		if( (retCode == -1) || (step == 6 ))
			break;
	}

	/*  */
	if( retCode == 1 )
	{
		switch(category)
		{
			/*  4：三次調整力(1) */
			/*  5：三次調整力(2) */
			case 4 :
			case 5 :
				ps_orderpara = &ps_g_EMCTLFIL->s_rr_para[AWS_ORDER_NEXT];
				break;

			/*  7：容量市場(実需給) */
			case 7 :
				ps_orderpara = &ps_g_EMCTLFIL->s_cm_para[AWS_ORDER_NEXT];
				break;

			/*  8：容量市場(実効性テスト) */
			case 8 :
				ps_orderpara = &ps_g_EMCTLFIL->s_cm_exam_para[AWS_ORDER_NEXT];
				break;
		}
		memcpy(ps_orderpara, &orderpara, sizeof(orderpara));
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  指令情報を登録しました。\n");
	}
	else if( retCode != 0 )
	{
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
		printf("\033[0K");	printf("\n");
		printf("\033[0K");	printf("  指令情報を登録に失敗しました。\n");
	}
	printf("\033[0K");	printf("\n");
	printf("\033[0K");	printf("================================================================================\n");


	func_cmn_KeyInput(PV_INPUT_ANYKEY_MSG);

}



void	func_notify_delete()
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
	int					loopCnt2;		/* ループカウント */
	int					skipCnt;		/* 一画面に表示する装置数*/
	int					offSet = 0;		/* オフセット */
	int					viewPos;		/* 表示位置 */
	struct tm			nowEndTime;		/* 終了時刻 */
	struct tm			nextEndTime;	/* 終了時刻 */
	struct st_ORDERPARA	*ps_orderpara;
	int					dispMode = 0;	/* 表示切替 */
	int					delNum = 0;
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
	ttyChange.c_cc[VTIME] = 5;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );


	skipCnt = ((termSize.ws_row - 28 ) / 2);

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
		printf("= <通知情報削除> ===============================================================\n");

		printf("\033[0K");	printf("通知情報------------------------------------------------------------------------\n");
		printf("\033[0K");	printf(" 番号 | 送信状態 | イベントID |  種別    | 送信回数 | 応答結果 | 応答メッセージ\n");
		printf("\033[0K");	printf("------+----------+------------+----------+----------+----------+----------------\n");
		
		for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
		{
			printf("\033[0K");	printf(" [%2d] | ", ps_g_EMCTLFIL->s_resinfo[loopCnt1].resId );
			switch( ps_g_EMCTLFIL->s_resinfo[loopCnt1].sndstatus )
			{
				case AWS_NOTIFY_ST_NON :
					printf("未設定   |            |          |          |          |\n");
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
			printf(" %10s | ", ps_g_EMCTLFIL->s_resinfo[loopCnt1].eventId );
			switch( ps_g_EMCTLFIL->s_resinfo[loopCnt1].messageKind )
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
			printf(" | %3d / ", ps_g_EMCTLFIL->s_resinfo[loopCnt1].resSndCnt );
			if( ps_g_EMCTLFIL->s_resinfo[loopCnt1].retryCnt == 0 )
				printf("--");
			else
				printf("%2d", ps_g_EMCTLFIL->s_resinfo[loopCnt1].retryCnt );

			printf(" |");
			if( ps_g_EMCTLFIL->s_resinfo[loopCnt1].messageKind == AWS_NOTIFY_RES )
				printf(" %2d(%s) | %s", ps_g_EMCTLFIL->s_resinfo[loopCnt1].resResut, ((ps_g_EMCTLFIL->s_resinfo[loopCnt1].resResut == 0 ) ? "正常" : "異常"), ps_g_EMCTLFIL->s_resinfo[loopCnt1].errMsg );
			else
				printf(" -------- | ------------" );
				
			printf("\n");
		}
		printf("\033[0K");
		printf("================================================================================\n");
		printf("\033[0K");
		printf("戻る:[q]                         削除番号：[1-0]※0=10                   (5秒更新)");

		fflush(NULL);

		memset( inKeyData, 0, sizeof(inKeyData));

		if( signal_flg ) break;

		/* キーボード入力を待つ */
		readByte = read( 0, inKeyData, 1 );

		/* 何かキー入力が行われたか、入力エラーの場合は処理を抜ける */
		if( readByte < 0 ) break;

		/* キー入力が終了の場合 */
		if( (inKeyData[0] == 'q') || (inKeyData[0] == 'Q') ) break;

		if( ( inKeyData[0] >= '0' ) && ( inKeyData[0] <= '9' ) )
		{
			inKeyData[1]=0;
			delNum = atoi( inKeyData );
			if( delNum == 0 )
			{
				delNum = 9;
			}
			else
			{
				delNum--;
			}
			ps_g_EMCTLFIL->s_resinfo[delNum].sndstatus = AWS_NOTIFY_ST_NON;
	
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
	return;
}



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
	int					loopCnt2;		/* ループカウント */
	int					skipCnt;		/* 一画面に表示する装置数*/
	int					offSet = 0;		/* オフセット */
	int					viewPos;		/* 表示位置 */
	struct tm			nowEndTime;		/* 終了時刻 */
	struct tm			nextEndTime;	/* 終了時刻 */
	struct st_ORDERPARA	*ps_orderpara;
	int					dispMode = 0;	/* 表示切替 */
	int					viewMode = 0;	/* 表示切替 */
	struct st_resinfo	*ps_resInfo;
	
#if 0
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
	ttyChange.c_cc[VTIME] = 5;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );


	skipCnt = ((termSize.ws_row - 28 ) / 2);

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
			printf("\033[0K");	printf(" イベント出力抑制指令情報------------------------------------------------------------\n");
			printf("\033[0K");	printf("  ■運用状態\n");
			printf("\033[0K");	printf("               %s\n", (ps_g_EMCTLFIL->s_setdata.actMode == AWS_MODE_OPE) ? "【 運用中 】" : "【 待機中 】" );

			for( loopCnt2 = 0; loopCnt2 < AWS_ORDER_STOCK; loopCnt2++ )
			{
				if( loopCnt2 == AWS_ORDER_NOW )
				{
					printf("\033[0K");	printf("  ◎現在\n");
				}
				else
				{
					printf("\033[0K");	printf("  〇次回\n");
				}

				ps_orderpara = &ps_g_EMCTLFIL->s_orderpara[loopCnt2];

				if( ps_orderpara->eventId[0] != 0 )
				{
					/* 年月日時分秒を取得 */
					localtime_r( &ps_orderpara->eventEdTimeSec.tv_sec, &nowEndTime );
					printf("\033[0K");	printf("    イベントＩＤ     ：%-22s | 指示種別        ：%s\n", ps_orderpara->eventId, ps_orderpara->orderStr );
					printf("\033[0K");	printf("    イベント開始時刻 ：%-22s | 継続時間(%3s)   ：%s\n", ps_orderpara->eventTimeStr, ps_orderpara->timeUnit, ps_orderpara->duration );
				}
				else
				{
					printf("\033[0K");	printf("    なし\n");
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
						if( (ps_g_EMCTLFIL->s_setdata.actMode == AWS_MODE_OPE) && 
							( loopCnt2 == AWS_ORDER_NOW ) && 
							( ps_g_EMCTLFIL->s_evtdata.intervalPos == viewPos ) )
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
				if( loopCnt2 == 0 )
				{
					printf("\033[0K");	printf("--------------------------------------------------------------------------------\n");
				}

			}
			printf("\033[0K");
			printf("================================================================================\n");
			printf("\033[0K");
			printf("戻る:[q]   頁切替:[前:c / 次:v]  指令/通知切替：[d]                    (5秒更新)");
		}
		else
		{
			if( viewMode == 0 )
			{
				printf("\033[0K");	printf("通知情報------------------------------------------------------------------------\n");
				printf("\033[0K");	printf(" 番号 | 送信状態 | イベントID |  種別    | 送信回数 | 応答結果 | 応答メッセージ\n");
				printf("\033[0K");	printf("------+----------+------------+----------+----------+----------+----------------\n");
				
				for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
				{
					printf("\033[0K");	printf(" [%2d] | ", ps_g_EMCTLFIL->s_resinfo[loopCnt1].resId );
					switch( ps_g_EMCTLFIL->s_resinfo[loopCnt1].sndstatus )
					{
						case AWS_NOTIFY_ST_NON :
							printf("未設定   |            |          |          |          |\n");
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
					printf(" %10s | ", ps_g_EMCTLFIL->s_resinfo[loopCnt1].eventId );
					switch( ps_g_EMCTLFIL->s_resinfo[loopCnt1].messageKind )
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
					printf(" | %3d / ", ps_g_EMCTLFIL->s_resinfo[loopCnt1].resSndCnt );
					if( ps_g_EMCTLFIL->s_resinfo[loopCnt1].retryCnt == 0 )
						printf("--");
					else
						printf("%2d", ps_g_EMCTLFIL->s_resinfo[loopCnt1].retryCnt );

					printf(" |");
					if( ps_g_EMCTLFIL->s_resinfo[loopCnt1].messageKind == AWS_NOTIFY_RES )
						printf(" %2d(%s) | %s", ps_g_EMCTLFIL->s_resinfo[loopCnt1].resResut, ((ps_g_EMCTLFIL->s_resinfo[loopCnt1].resResut == 0 ) ? "正常" : "異常"), ps_g_EMCTLFIL->s_resinfo[loopCnt1].errMsg );
					else
						printf(" -------- | ------------" );
						
					printf("\n");
				}
			}
			else
			{
				printf("\033[0K");	printf("通知情報(送信順序)--------------------------------------------------------------\n");
				printf("\033[0K");	printf(" 順序 | 番号 | 送信状態 | イベントID |  種別    | 送信回数 | 応答結果 |\n");
				printf("\033[0K");	printf("------+------+----------+------------+----------+----------+----------+\n");
				for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
				{
					ps_resInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]];
					printf("\033[0K");	printf(" [%2d] | [%2d] | ", loopCnt1 + 1, ps_resInfo->resId );

					switch( ps_resInfo->sndstatus )
					{
						case AWS_NOTIFY_ST_NON :
							printf("未設定   |            |          |\n");
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
					printf(" %10s | ", ps_resInfo->eventId );
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

					printf(" |");
					if( ps_resInfo->messageKind == AWS_NOTIFY_RES )
						printf(" %2d(%s) |", ps_resInfo->resResut, ((ps_resInfo->resResut == 0 ) ? "正常" : "異常") );
					else
						printf(" -------- |" );
					printf("\n");
				}
			}
			printf("\033[0K");
			printf("================================================================================\n");
			printf("\033[0K");
			printf("戻る:[q]                         指令/通知切替：[d]                    (5秒更新)");
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
			if( dispMode == 1 )
			{
				viewMode = ( viewMode + 1 ) % 2;
			}
		}

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
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );
#endif
	return;
}




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

int func_cmn_set_tim( void )
{
	
	/* 積算秒を取得 */
	gettimeofday( &g_Time, NULL );

	/* 時刻情報に変換 */
	localtime_r( &g_Time.tv_sec, &g_timeData );

	g_timeData.tm_year += 1900;
	g_timeData.tm_mon+= 1;

//	printf("now[%d%02d%02d %02d:%02d %02d]\n",
//			g_timeData.tm_year,g_timeData.tm_mon,g_timeData.tm_mday,
//			g_timeData.tm_hour, g_timeData.tm_min, g_timeData.tm_sec);

	return(0);
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




