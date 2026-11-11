/*
   TAB STOP SIZE = 4
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "embase.h"
#include "embase_mon.h"

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
#if 0
#endif
#define DEBUG_1
#define DEBUG_NOPAGE
#define DEBUG_ERR
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/

#define EMBASE_OWN_NAME			"EMBASE_MONI"					/* 自プログラム名 */

struct st_dspStsName {
	char		nameStr[5][256];								/* 状態名称 */
};

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
/* プロセス内グローバル */

struct st_dspStsName	s_nameDevice = {						/* 装置名称 */
	"不明", "１系", "２系", "－－", "－－"
};
struct st_dspStsName	s_nameSysMod = {						/* ２系列モード名称 */
	"不明", "準備", "停止", "待機", "運用"
};
struct st_dspStsName	s_nameNetDev = {						/* ネットワークＩ／Ｆ状態名称 */
	"正常", "異常", "不明", "－－", "－－"
};
struct st_dspStsName	s_nameLifChk = {						/* 死活監視状態名称 */
	"正常", "異常", "－－", "－－", "－－"
};

struct winsize			termSize;					/* 端末の画面サイズ */
struct winsize			termSize_OLD;					/* 端末の画面サイズ */

/* 内部関数の定義 */
void func_termsize_get();
int	func_get_pid( char * );

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状態表示処理
 *
 * 機能：
 * 		自装置の系情報、死活監視状態、自系および相手系の装置モード、ネットワーク
 *		I/F状態を標準出力に表示する
 * 戻り値：
 * 		なし
 */
int main()
{
	FILE					*fp;						/* ファイルポインタ */
	char					cmdStr[1024];				/* コマンド文字列 */
	char					drbdData[1024];				/* ＤＲＢＤ状態表示 */
	int						retCode;					/* 戻り値 */
	int						msgCnt;						/* ログ表示行数 */
	int						drbdCnt;					/* ＤＲＢＤ状態表示行数 */
	int						waitTime = 1;				/* キー入力待ち時間 */
	int						pidEmsystem;				/* 管理プロセスPID */
	int						logFlg = 1;					/* ログ表示フラグ */
	int						drbdFlg = 1;				/* ＤＲＢＤ状態表示フラグ */
	int						modTranTime = 0;			/* 遷移待ち時間 */
	int						liveFlg = 0;				/* 動作フラグ */
	char					inKeyData;					/* キー入力値 */
	char					readByte;					/* キー入力バイト数 */
	time_t					workTime;					/* 時刻情報 */
	time_t					recvTime;					/* 時刻情報 */
	struct tm				t_st1;						/* 時刻情報 */
	struct tm				t_st2;						/* 時刻情報 */
	struct termio			ttyBackup;					/* 端末設定保存用 */
	struct termio			ttyChange;					/* 端末設定変更用 */


	/* 動作環境構築 */
	if( (retCode = embase_func_startup( 0 )) != 0 )
	{
		printf("%s:main() embase_func_startup ERROR err:%d\n", EMBASE_OWN_NAME, retCode );
		exit( EXIT_FAILURE );
	}

	/* 現在のターミナル設定を退避 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;

	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON );
	/* キー入力待ち設定 */
	ttyChange.c_cc[VMIN] = 0;
	ttyChange.c_cc[VTIME] = waitTime * 10;
	/* 設定変更を有効にする */
	ioctl( 0, TCSETAF, &ttyChange );

	/* 停止条件となるまで繰り返す */
	while( 1 )
	{
		/* ターミナルサイズの取得 */
		func_termsize_get();

		/* メッセージ表示行数の設定 */
		msgCnt = termSize.ws_row - 16;
		if( msgCnt < 0 ) msgCnt = 0;
		
		/* 現在時刻情報の取得 */
		workTime = time( NULL );
		localtime_r( &workTime, &t_st1 );

		modTranTime = (int)(embase_func_get_mode_tran_time() - time( NULL ));
		if( modTranTime < 0 ) modTranTime = 0;

		/* 最終受信時刻情報の取得 */
		recvTime = embase_func_get_lastrecv_time();
		localtime_r( &recvTime, &t_st2 );

		/* 画面をクリア */
		printf("\033[2J");
		/* カーソルの位置を高さ０、横０に移動 */
		printf("\033[%d;%dH",0,0);

		pidEmsystem = func_get_pid( EMBASE_PROCNAME_EMSYSTEM );

		printf("=======================================================< %04d/%02d/%02d %02d:%02d:%02d >=\n",
			t_st1.tm_year + 1900, t_st1.tm_mon + 1, t_st1.tm_mday, t_st1.tm_hour, t_st1.tm_min, t_st1.tm_sec );

		if( pidEmsystem != -1 )
		{
			printf(" 装置管理プロセス[ %s ] : \033[42m動作中\033[0m (pid:%d)\n", EMBASE_PROCNAME_EMSYSTEM, pidEmsystem );
		}
		else
		{
			printf(" 装置管理プロセス[ %s ] : \033[41m停止中\033[0m\n", EMBASE_PROCNAME_EMSYSTEM );
			/* 以降の文字表示を反転 */
			printf("\033[047m");
		}

		printf(" 自装置番号      [ %s ( %s -> %s ) ]   遷移確認待[ %d ]\n",
			s_nameDevice.nameStr[ embase_func_get_kei_info() ], s_nameSysMod.nameStr[ embase_func_get_my_mode() ],
			s_nameSysMod.nameStr[ embase_func_get_newSysMode() ], modTranTime );
		printf(" 死活監視    S-IF[ %s ] I-IF[ %s ]     最終受信時刻[ %04d/%02d/%02d %02d:%02d:%02d ]\n",
			s_nameNetDev.nameStr[ embase_func_get_lifchk_service() ], s_nameNetDev.nameStr[ embase_func_get_lifchk_internal() ],
			t_st2.tm_year + 1900, t_st2.tm_mon + 1, t_st2.tm_mday, t_st2.tm_hour, t_st2.tm_min, t_st2.tm_sec );
		printf("-<１系>--------------------------------+-<２系>--------------------------------\n");
		printf(" 装置モード      [ %s ]              | 装置モード      [ %s ]\n",
			s_nameSysMod.nameStr[ embase_func_get_1kei_mode() ], s_nameSysMod.nameStr[ embase_func_get_2kei_mode() ] );
		printf(" Network-I/F S-IF[ %s ] I-IF[ %s ] | Network-I/F S-IF[ %s ] I-IF[ %s ]\n",
			s_nameNetDev.nameStr[ embase_func_get_1kei_net_service() ], s_nameNetDev.nameStr[ embase_func_get_1kei_net_internal() ],
			s_nameNetDev.nameStr[ embase_func_get_2kei_net_service() ], s_nameNetDev.nameStr[ embase_func_get_2kei_net_internal() ]);
		printf("---------------------------------------+---------------------------------------\n");
		printf(" ＤＲＢＤロール状態   : [ %s / %s ]  （自系/他系）\n",
			s_drbdRoleTable[ embase_func_get_drbd_my_role() ].Name,  s_drbdRoleTable[ embase_func_get_drbd_par_role() ].Name );
		printf(" ＤＲＢＤディスク状態 : [ %s / %s ]  （自系/他系）\n",
			s_drbdDstateTable[ embase_func_get_drbd_my_dstate() ].Name,  s_drbdDstateTable[ embase_func_get_drbd_par_dstate() ].Name );
		printf(" ＤＲＢＤ接続状態     : [ %s ]  （自系）\n",
			s_drbdCstateTable[ embase_func_get_drbd_my_cstate() ].Name );
		printf("-------------------------------------------------------------------------------\n");
		printf(" 運用モード 最終稼働時刻[ %d / %d ] 正常稼働時間[ %d / %d ]\n",
			embase_func_get_my_operate_last_time(), embase_func_get_par_operate_last_time(), 
			embase_func_get_my_operate_total_time(), embase_func_get_par_operate_total_time() );

		if( pidEmsystem == -1 )
			printf("\033[0m");

		printf("==================================================== d:drbd / l:log / q:quit ==\n");

		drbdCnt = 0;
		if( drbdFlg )
		{
			/* ＤＲＢＤ状態をコマンドを実行してパイプから取得 */
			if( (fp = popen( "/usr/bin/cat /proc/drbd 2> /dev/null", "r" )) != NULL )
			{
				while( fgets( drbdData, 1024, fp ) != NULL )
				{
					printf("%s", drbdData );
					drbdCnt += 1;
				}
				pclose( fp );
			}
			printf("===============================================================================\n");
			drbdCnt += 1;
		}

		if( logFlg )
		{
			/* ログファイルの後方を表示 */
			sprintf(cmdStr, "/usr/bin/tail -%d %s%s.%s 2> /dev/null",
				( 0 < (msgCnt - drbdCnt) )? msgCnt - drbdCnt : 0, EMBASE_FPATH_LOG, EMBASE_FNAME_EMSYSLOG, EMBASE_FNEXT_EMSYSLOG );
			system(cmdStr);
		}

		/* 画面表示をフラッシュ */
		fflush(NULL);

		/* キーボード入力を待つ */
		readByte = read( 0, &inKeyData, 1 );

		if( readByte != 0 )
		{
			/* キー入力異常時（端末クローズ等） */
			if( readByte < 0 ) break;
			/* 終了のキー入力の場合処理を抜ける */
			if( (inKeyData == 'q' ) || (inKeyData == 'Q') ) break;
			if( inKeyData == '+' )
			{
				if( waitTime < 5 )
				{
					waitTime++;
					ttyChange.c_cc[VMIN] = 0;
					ttyChange.c_cc[VTIME] = waitTime * 10;
					/* 設定変更を有効にする */
					ioctl( 0, TCSETAF, &ttyChange );
				}
			}
			if( inKeyData == '-' )
			{
				if( waitTime > 1 )
				{
					waitTime--;
					ttyChange.c_cc[VMIN] = 0;
					ttyChange.c_cc[VTIME] = waitTime * 10;
					/* 設定変更を有効にする */
					ioctl( 0, TCSETAF, &ttyChange );
				}
			}
			if( ( inKeyData == 'l' ) || ( inKeyData == 'L' ) )
			{
				logFlg += 1;
				logFlg %= 2;
			}
			if( ( inKeyData == 'd' ) || ( inKeyData == 'D' ) )
			{
				drbdFlg += 1;
				drbdFlg %= 2;
			}
		}
	}

	/* ターミナルの設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	/* 動作環境破棄 */
	embase_func_finish();

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
int	func_get_pid( char *c_procName )
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
		/* パイプを閉じる */
		pclose( fp );
	
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
 * 端末サイズ取得
 *
 * 機能：
 *		動作している端末のサイズ（縦・横）を取得する。
 *		サイズが 80x24 以下の場合は、80x24 として扱う。
 */
void func_termsize_get()
{
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &termSize );
	/* 端末サイズが取得できなかった場合は暫定的に 80x24 とする */
	if( termSize.ws_col == 0 ) termSize.ws_col = 80;
	if( termSize.ws_row == 0 ) termSize.ws_row = 24;
}

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
