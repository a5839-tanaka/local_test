/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20250807] 2025.08.07
 * ・一次調整力レポートデータ送信対応
 * [20230831] 2023.08.31
 * ・一度に送信する最大ファイル数を8→64に変更
 * ・停止時にリトライ送信中は送信しないように変更
 * [20230131] 2023.01.31
 * ・遠隔制御、設定変更ファイルをダウンロード中に通信異常が発生し、ファイルが
 *   壊れていた場合の対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * モニタリングサーバ送信プロセス
 *
 * 主な機能
 * 		（１）通信機能
 * 		１分周期でシステムでファインに定義されたディレクトリに格納されている規程
 * 		の名称のファイルをモニタリングサーバ情報ファイル（PVSVRINF.dat）に登録さ
 * 		れている情報に従い、ＳＦＴＰを使用して送信する。
 *
 * 		（２）ファイル保存機能
 * 		ＳＦＴＰによる送信ができなかったファイルについて、一定期間保存する。
 * 		保存したファイルは送信契機の度に古いものから優先して送信を行う。
 * 		一定期間送信ができなかったファイルは破棄する。
 * 		保持期間はPVSVRINF.datファイルの設定に従う。（最長7日まで1時間単位で指定）
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PV_MAIN_MODULE

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvcomm.h"
#include "pvctldata.h"

#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：ＳＦＴＰ設定情報表示 */
#define	PV_DEBUG_DT2								/* デバッグフラグ： */
#define	PV_DEBUG_DT3								/* デバッグフラグ： */
#endif

#define	PV_OWN_NAME		"NKFTPSND"					/* 自プログラム名 */

#define	PV_FTP_FILES	64							/* 一度に送信する最大ファイル数 *//* [20230831] */
#define	PV_FTP_ERR_MAX	1180						/* 連続送信失敗時間（19分40秒） */
#define PV_FTP_FILES_STD	2						/* 一度に送信する標準ファイル数 */
#define	PV_STOP_SND_CNT		3						/* 停止時に送信する回数 */

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_PVSVRINF		*ps_g_PVSVRINF;				/* 管理ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_CTLJHFIL		*ps_g_CTLJHFIL;				/* 制御状変蓄積ファイルポインタ */

													/* 使用ファイルインデックス */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_PVSVRINF, (void **)&ps_g_PVSVRINF, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
    {   PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
    {   PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
    {   PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
    {   PV_FILENO_CTLJHFIL, (void **)&ps_g_CTLJHFIL, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0 }
};

static char				pv_own_name[128];			/* 自プロセス情報 */
static char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int				limitFiles = PV_FTP_FILES_STD;/* 一度に送信するファイル数 */
static time_t			sendChkTime;				/* 送信確認時間 */
static char				curdir[128];				/* カレントディレクトリPath */

/* 内部関数定義 */
int	func_pvserver_ftpsend();
int	func_choice_dirname( const struct dirent *dir );
int	func_choice_filename( const struct dirent * );
int	func_choice_filename_CMD( const struct dirent * );
int	func_choice_filename_CTL( const struct dirent * );
int	func_abnormal_exit( int );
int func_file_StrCheck( char *, char * );	/* [20230131] Add */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モニタリングサーバ送信処理
 *
 * 機能：
 * 		１分周期に下記処理を行う
 * 		システムデファイン PV_PATH_FTP に定義されたディレクトリに格納されている
 * 		規程の名称のファイルをモニタリングサーバ情報ファイル（PVSVRINF.dat）に登
 * 		録されている情報に従い、ＳＦＴＰを使用して送信する。
 * 		規程ファイル名は 26文字の以下の名称ファイル
 * 		XXXXXXXXYYYYMMDDHHmm.DAT
 * 		　XXXXXXXX　：　収集装置ID名称（8文字）
 * 		　YYYY　　　：　西暦（4文字）
 * 		　MM　　　　：　月（2文字）
 * 		　DD　　　　：　日（2文字）
 * 		　HH　　　　：　時（2文字）
 * 		　mm　　　　：　分（2文字）
 * 		　ss　　　　：　秒（2文字）
 * 		　.DAT　　　：　拡張子（4文字）
 * 		ファイルが複数存在する場合は古いファイルを優先して送信する。
 * 		送信先のサーバのIPアドレスおよびポート番号、アクセスユーザ名はモニタリン
 * 		グサーバ情報ファイル（PVSVRINF.dat）に登録されているものを使用する。
 *
 * 		（２）ファイル保存機能
 * 		ＳＦＴＰによる送信ができなかったファイルについて、一定期間保存する。
 * 		保存したファイルは送信契機の度に古いものから優先して送信を行う。
 * 		一定期間送信ができなかったファイルは破棄する。
 * 		保持期間はPVSVRINF.datファイルの設定に従う。（最長7日まで1時間単位で指定）
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int main(int argc, char *argv[])
{
	int						procNo;						/* プログラム番号 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						selectSvr = 0;				/* サーバ切替用 */
	int						stopFlag = 0;				/* 停止通知受信済みフラグ */

#ifdef PV_DEBUG
	struct timeval			time1;
	struct timeval			time2;
#endif

	/* プロセスの優先度を他の業務プロセスよりも低く設定する(nice = 5) */
	setpriority( PRIO_PROCESS, 0, 5 );

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
		printf("NKFTPSND : File Open Error\n");
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_SET );

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_RESET );

	/* 送信確認時刻を初期化 */
	sendChkTime = time( NULL );

	/* 停止通知受信済みフラグリセット */
	stopFlag = 0;

	while( 1 )
	{
		/* 5秒周期でアクセスする待機する */
		if( commfunc_wait( 5, procNo ) < 0 )
		{
#ifdef PV_DEBUG
			printf("NKFTPSND : PROCESS STOP COMMAND RECV (PROC:%d)\n", procNo );
#endif
			/* 停止通知受信済みフラグをインクリメント */
			stopFlag++;

			/* 停止チェックはウォッチドックのチェック(PV_WDOG_CYCLE秒間隔)で行っている為、ここでの待機は不要 */
			/* repoeditより起動した場合、ファイルが作成されておらず、次のwaitでも同じ秒の為、 */
			/* 即時にタイムアウトしてしまうので、必ず1秒待つ */
			sleep( 1 );
		}


#ifdef PV_DEBUG
		gettimeofday( &time1, NULL );
		printf("NKFTPSND : ST:%d.%06d\n",time1.tv_sec, time1.tv_usec );
#endif

		/* サーバへの送信処理を実施	*/
		retCode = func_pvserver_ftpsend( &selectSvr );

#ifdef PV_DEBUG
		gettimeofday( &time2, NULL );
		printf("NKFTPSND : ED:%d.%06d\n",time2.tv_sec, time2.tv_usec );
#endif
		/* 停止通知受信済フラグが設定されている場合 */
		/* 　停止時送信回数分に到達、または、 */
		/* 　送信ファイル数がリトライ送信(最小より多い)の場合、または、  */
		/* 　送信失敗時  */
		if( ( stopFlag > 0 ) &&
			( ( stopFlag >= PV_STOP_SND_CNT ) ||
			  ( limitFiles > PV_FTP_FILES_STD ) ||		/* [20230831] Add */
			  ( retCode < 0 )) )
		{
			/* 処理を抜ける */
			break;
		}
	}

	/* 装置状態状変：アップデートファイル検出をOFF */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_PROC_FILE_FOUND, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DATA_FILE_FOUND, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

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
 * モニタリングサーバ送信処理
 *
 * 機能：
 * 		システムデファイン PV_PATH_FTP に定義されたディレクトリに格納されている
 * 		規程の名称のファイルをモニタリングサーバ情報ファイル（PVSVRINF.dat）に登
 * 		録されている情報に従い、ＳＦＴＰを使用して送信する。
 * 		規程ファイル名は 26文字の以下の名称ファイル
 * 		XXXXXXXXYYYYMMDDHHmmss.DAT
 * 		　XXXXXXXX　：　収集装置ID名称（8文字）
 * 		　YYYY　　　：　西暦（4文字）
 * 		　MM　　　　：　月（2文字）
 * 		　DD　　　　：　日（2文字）
 * 		　HH　　　　：　時（2文字）
 * 		　mm　　　　：　分（2文字）
 * 		　ss　　　　：　秒（2文字）
 * 		　.DAT　　　：　拡張子（4文字）
 *
 * 		ＳＦＴＰの送信はバッチファイルを生成し、system() 関数を使用して SFTP を
 * 		起動する。
 * 		バッチファイルには以下の処理を記載する。
 * 		・送信先ディレクトリへの移動
 * 		・送信ファイルを名称変更して送信
 * 		・送信完了の場合は名称を正規のものに修正
 * 		・送信完了のファイルをローカルディレクトリから削除
 *
 * 		引数 *selectSvr の数値は 0 から 9 の範囲の値で受け渡される。SFTP送信が
 * 		失敗する度に +1 され、値が 10 になった場合は 0 にリセットされる。
 * 		値が 5 から 9 の場合にモニタリングサーバ情報にサブのIPアドレスが登録さ
 * 		れている場合は、サブのサーバに対して情報を送信する。
 *
 * 備考：
 * 		SFTPをバッチファイルで処理した場合、処理途中で問題が発生した場合はバッ
 * 		チ処理が終了する。
 * 		バッチ処理が異常終了した場合はsystem()関数はエラー値を返す
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	func_pvserver_ftpsend( int *selectSvr )
{
	FILE			*fp;							/* SFTPバッチファイルポインタ */
	FILE			*fp_list;						/* リストファイルポインタ */
	struct dirent	**fileList;						/* ディレクトリ内ファイルリスト用ポインタ */
	struct dirent	**fileList2;					/* ディレクトリリスト用ポインタ */
	struct dirent	**fileListDwn;					/* ディレクトリ内ファイルリスト用ポインタ */
	struct tm		dayTime;						/* 日付情報 */
	time_t			nowTime;						/* 現在時刻 */
	time_t			befTime;						/* 送信前時刻 */
	time_t			aftTime;						/* 送信後時刻 */
	int				fileSize;						/* ファイルサイズ */
	int				rebootFlag;						/* リブート（データ・プログラム更新あり）フラグ */
	int				retCode=0;						/* 関数戻り値 */
	int				retCode1;						/* 関数戻り値 */
	int				retCode2;						/* 関数戻り値 */
	char			batchFile[128];					/* SFTPバッチファイル名 */
	char			logFile[128];					/* SFTPログファイル名 */
	char			logOldFile[128];				/* SFTPログファイル名 */
	char			tmpFile1[128];					/* 一時ファイル名 */
	char			tmpFile2[128];					/* 一時ファイル名 */
	char			sftpCommand[512];				/* SSHコマンド */
	int				loopCnt1;						/* ループカウンタ */
	int				svrSelect = 0;					/* サーバ選択 */
	int				sendFiles = 0;					/* 送信ファイル数 */
	char			listFile[128];					/* リストファイル名 */
	char			tmpBuff[128];					/* 一時バッファ */
	char			readBuff[128];					/* ファイル名読み込みバッファ */

	/* ファイルリスト名の設定 */
	strcpy( listFile, PV_PATH_FTP );
	strcat( listFile, PV_FNAME_DWLFILE_LIST );
	/* SFTPバッチファイル名の設定 */
	strcpy( batchFile, PV_PATH_FTP );
	strcat( batchFile, PV_FNAME_SFTP_BAT );
	/* SFTPログファイル名の設定 */
	strcpy( logFile, PV_PATH_LOG );
	strcat( logFile, PV_FNAME_SFTP_LOG );
	strcat( logFile, ".log" );
	/* SFTPログファイル名の設定 */
	strcpy( logOldFile, PV_PATH_LOG );
	strcat( logOldFile, PV_FNAME_SFTP_LOG );
	strcat( logOldFile, "_1.log" );

	/* 一度に送信するファイル数のチェック */
	if( limitFiles < PV_FTP_FILES_STD ) limitFiles = PV_FTP_FILES_STD;

	/* 送信先サーバの決定 */
	if( (*selectSvr > 4) && (strnlen(ps_g_PVSVRINF->s_svrinfo[1].ipAddr,31) > 0) )
	{ 
		/* 送信先サーバを予備機に設定 */
		svrSelect = 1;
	}
	/* 送信先サーバが未設定の場合 */
	if( strnlen(ps_g_PVSVRINF->s_svrinfo[svrSelect].ipAddr, 31) == 0 )
		return( 0 );


	/* バッチファイル名が存在する場合 */
	if( access( batchFile, R_OK | W_OK ) == 0 )
	{
		/* 既存のバッチファイルを削除 */
		unlink( batchFile );
	}

	/* バッチファイルを生成 */
	if( (fp = fopen( batchFile, "w" )) == NULL )
	{
		return( -1 );
	}

	/* リストファイルが存在する場合 */
	if( access( listFile, R_OK ) == 0 )
	{
		if( (fp_list = fopen( listFile, "r" )) == NULL )
		{
			return( -1 );
		}
		/* データアップデートファイルの格納場所に移動するコマンドの登録 */
		fprintf( fp, "-cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].dataPath );
#ifdef PV_DEBUG
		printf( "-cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].dataPath );
#endif
		/* リストファイルの終端まで繰り返す */
		while( fgets( readBuff, 128, fp_list) != NULL )
		{
			/* バッチファイルにコマンドを追加 */
			fprintf( fp, "%s", readBuff );
		}
		/* バッチファイルを閉じる */
		fclose( fp_list );
	}

	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	/* （一覧作成時に func_choice_dirname フィルタを使用し、アルファベット順にソートする） */
	retCode1 = scandir( PV_PATH_FTP, &fileList2, func_choice_dirname, alphasort );
	if( retCode1 < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "FTP Send Dir List Get Error" );
		return( -1 );
	}
	/* 処理対象ファイルが存在する場合 */
	if( retCode1 > 0 )
	{
		/* サーバ送信用ファイルディレクトリを取得する */
		memset(curdir, 0, sizeof(curdir));
		strcpy( curdir, PV_PATH_FTP );
		strcat( curdir, fileList2[0]->d_name);
		strcat( curdir, "/");

		/* ファイル数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < retCode1 ; loopCnt1++ )
		{
			/* リソースを開放 */
			free( fileList2[loopCnt1] );
		}

		/* サーバ送信用ファイルディレクトリ内のファイル一覧を取得する */
		/* （一覧作成時に func_choice_filename フィルタを使用し、アルファベット順にソートする） */
		retCode2 = scandir( curdir, &fileList, func_choice_filename, alphasort );
		if( retCode2 < 0)
		{
			commfunc_syslog_msgset( pv_own_name, "FTP Send File List Get Error" );
			return( -1 );
		}
		/* 処理対象ファイルが存在する場合 */
		if( retCode2 > 0 )
		{
			/* 現在の未送信ファイル数が一度に送信するファイル数よりも少ない場合 */
			if( limitFiles > retCode2 )
			{
				/* 一度に送信するファイル数を調整 */
				limitFiles = retCode2;
			}

			/* バッチファイルにコマンドを追加 */

			/* ローカルの時刻を記録（SFTP接続完了の時刻として扱う） */
			fprintf( fp, "!date\n" );

			/* ローカルディレクトリを送信用ファイルの格納ディレクトリに移動するコマンドの登録 */
			fprintf( fp, "lcd %s\n", curdir );
			/* サーバ側の送信用ディレクトリに移動するコマンドを登録 */
			fprintf( fp, "cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].sendPath );
#ifdef PV_DEBUG
			printf("lcd %s\n", curdir );
			printf("cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].sendPath );
#endif

			/* ファイル数分繰り返す（一度に送信できるファイル数の範囲内で実施） */
			for( loopCnt1 = 0 ; loopCnt1 < retCode2 ; loopCnt1++ )
			{
#ifdef PV_DEBUG
				printf("NKFTPSND : SFTP SEND FILE-No:%02d [ %s ] ", loopCnt1+1, fileList[ loopCnt1 ]->d_name );
#endif
				/* 送信対象ファイルが一度に送信可能なファイル数以内の場合 */
				if( ( loopCnt1 < limitFiles ) && ( loopCnt1 < PV_FTP_FILES ) )
				{
					/* ワークファイル名生成 */
					memset( tmpFile1, 0, sizeof(tmpFile1) );
					strncpy( tmpFile1, fileList[ loopCnt1 ]->d_name, (strlen(fileList[ loopCnt1 ]->d_name)-4) );
					/* バッチファイルにコマンドを追加 */
					/* ワークファイル名で送信するコマンドの登録 */
					fprintf( fp, "put %s %s.TMP\n", fileList[ loopCnt1 ]->d_name, tmpFile1 );
					/* ワークファイル名を本来のファイル名に変更するコマンドの登録 */
					fprintf( fp, "rename  %s.TMP %s\n", tmpFile1, fileList[ loopCnt1 ]->d_name );
					/* 送信完了ファイルをローカルディレクトリから削除するコマンドの登録 */
					fprintf( fp, "!rm -f %s%s\n", curdir, fileList[ loopCnt1 ]->d_name );
					/* 送信ファイル数を＋１ */
					sendFiles++;
#ifdef PV_DEBUG
					printf(" [CHECK]\n");
				}
				else
				{
					printf("\n");
#endif
				}

				/* リソースを開放 */
				free( fileList[ loopCnt1 ]);
			}

			/* ローカルディレクトリをダウンロードファイル格納場所に移動するコマンドの登録 */
			fprintf( fp, "lcd %s\n", PV_PATH_DWL );

			/* これ以降のバッチ処理はエラーが発生しても処理を継続するようにする（コマンドの前に'-'をつける） */

#ifdef PV_DEBUG
			printf( "lcd %s\n", PV_PATH_DWL );
			printf( "-cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].dataPath );
#endif
			/* データアップデートファイルの格納場所に移動するコマンドの登録 */
			fprintf( fp, "-cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].dataPath );
			/* データアップデートファイルのダウンロードを行うコマンドの登録 */
			fprintf( fp, "-get %s_DATA_UPDATE.tgz %s_DATA_UPDATE.tgz_tmp\n", ps_g_PVSYSINF->siteIdName, ps_g_PVSYSINF->siteIdName );
			/* ダウンロードしたファイル名を修正するコマンドの登録 */
			fprintf( fp, "-!mv -f %s_DATA_UPDATE.tgz_tmp %s_DATA_UPDATE.tgz\n", ps_g_PVSYSINF->siteIdName, ps_g_PVSYSINF->siteIdName );
			/* サーバ上のデータアップデートファイルの削除を行うコマンドの登録 */
			fprintf( fp, "-rm %s_DATA_UPDATE.tgz\n", ps_g_PVSYSINF->siteIdName );

			/* 遠隔制御ファイルのダウンロードを行うコマンドの登録 */
			fprintf( fp, "-get %s*%s.DAT\n", ps_g_PVSYSINF->siteIdName, PV_FNAME_REMOTECTL );
#ifdef PV_DEBUG
			printf("-get %s*%s.DAT\n", ps_g_PVSYSINF->siteIdName, PV_FNAME_REMOTECTL );
#endif
//			/* MicroGrid計画情報ファイルのダウンロードを行うコマンドの登録 */
//			fprintf( fp, "-get %s*%s.DAT\n", ps_g_PVSYSINF->siteIdName, PV_FNAME_MGPLNINF );
//#ifdef PV_DEBUG
//			printf("-get %s*%s.DAT\n", ps_g_PVSYSINF->siteIdName, PV_FNAME_MGPLNINF );
//#endif
			/* 設定変更ファイルのダウンロードを行うコマンドの登録 */
			fprintf( fp, "-get %s*%s.DAT\n", ps_g_PVSYSINF->siteIdName, PV_FNAME_UPDATECNF );
#ifdef PV_DEBUG
			printf("-get %s*%s.DAT\n", ps_g_PVSYSINF->siteIdName, PV_FNAME_UPDATECNF );
#endif
			/* プログラムアップデートファイルの格納場所に移動するコマンドの登録 */
			fprintf( fp, "-cd %s\n", ps_g_PVSVRINF->s_svrinfo[svrSelect].procPath );
			/* プログラムアップデートファイルのダウンロードを行うコマンドの登録 */
			fprintf( fp, "-get %s_PROC_UPDATE.tgz %s_PROC_UPDATE.tgz_tmp\n", ps_g_PVSYSINF->siteIdName, ps_g_PVSYSINF->siteIdName );
			/* ダウンロードしたファイル名を修正するコマンドの登録 */
			fprintf( fp, "-!mv -f %s_PROC_UPDATE.tgz_tmp %s_PROC_UPDATE.tgz\n", ps_g_PVSYSINF->siteIdName, ps_g_PVSYSINF->siteIdName );
			/* サーバ上のプログラムアップデートファイルの削除を行うコマンドの登録 */
			fprintf( fp, "-rm %s_PROC_UPDATE.tgz\n", ps_g_PVSYSINF->siteIdName );
			/* 遠隔コマンドファイルのダウンロードを行うコマンドの登録 */
			fprintf( fp, "-get %s_COMMAND_*\n", ps_g_PVSYSINF->siteIdName );
			/* 遠隔コマンドファイルの削除を行うコマンドの登録 */
			fprintf( fp, "-rm %s_COMMAND_*\n", ps_g_PVSYSINF->siteIdName );

			/* ローカルの時刻を記録（SFTP接続終了の時刻として扱う） */
			fprintf( fp, "!date\n" );

			fprintf( fp, "quit\n" );
		}
		/* リソースを開放 */
		free( fileList );
	}
	/* リソースを開放 */
	free( fileList2 );

	/* バッチファイルを閉じる */
	fclose( fp );

	/* 転送対象ファイルが存在した場合 */
	if( ( retCode1 > 0 ) && ( retCode2 > 0 ))
	{
		/* ログディレクトリが存在する場合 */
		if( access( PV_PATH_LOG, R_OK | W_OK | X_OK ) == 0 )
		{
			/* 現在時刻を取得 */
			nowTime = time( NULL );
			/* 年月日を取得 */
			localtime_r( &nowTime, &dayTime );
			/* ログファイルを追加モードでOPENする */
			if( ( fp = fopen( logFile, "a" )) != NULL )
			{
				/* ファイルサイズを取得 */
				fileSize = ftell( fp );
				/* ファイルサイズが1MByteを超過している場合 */
				if( fileSize > 1000000 )
				{
					/* ファイルをクローズ */
					fclose( fp );
					/* ログファイル名が存在し読み書き可能な場合 */
					if( access( logOldFile, R_OK | W_OK ) == 0 )
					{
						/* 既存のログファイルを削除 */
						unlink( logOldFile );
					}
					/* ログファイルを旧ファイル名に変更 */
					rename( logFile, logOldFile );
					/* ログファイルを追加モードでOPENする */
					fp = fopen( logFile, "a" );
				}
				/* 現在時刻を記録する */
				fprintf( fp, "===== SFTP CONNECT START [ %04d/%02d/%02d/ %02d:%02d:%02d ] =====\n",
					dayTime.tm_year + 1900, dayTime.tm_mon + 1, dayTime.tm_mday,
					dayTime.tm_hour, dayTime.tm_min, dayTime.tm_sec );
				/* ファイルをクローズ */
				fclose( fp );
			}
				
		}
		else
		{
			/* ログファイル名を /dev/nullに変更 */
			sprintf( logFile, "/dev/null" );
		}

		/* SFTPコマンドの生成 */
		/* ・バッチファイルの指定 */
		/* ・ポート番号の指定 */
		/* ・接続ユーザ名の指定 */
		/* ・ホストキーの保存確認を行わない */

		sprintf( sftpCommand, "sftp -C -b%s -oPort=%d -oIdentityFile=~/.ssh/id_rsa.PVSERVER_%d -oUser=%s -oStrictHostKeyChecking=no %s >>%s 2>&1",
			batchFile,
			ps_g_PVSVRINF->s_svrinfo[svrSelect].portNumb,
			svrSelect + 1,
			ps_g_PVSVRINF->s_svrinfo[svrSelect].loginName,
			ps_g_PVSVRINF->s_svrinfo[svrSelect].ipAddr,
			logFile );

#ifdef PV_DEBUG
		printf("NKFTPSND : SFTP COMMAND [ %s ]\n", sftpCommand );
#endif

		/* 送信開始時刻の保存 */
		befTime = time( NULL );

		/* SYSTEMコマンドによりSFTPを実施 */
		retCode = system( sftpCommand );

		/* バッチファイルを削除 */
		unlink( batchFile );

		/* 送信終了時刻の保存 */
		aftTime = time( NULL );

		/* SYSTEMコマンド異常終了の場合 */
		if( retCode != 0 )
		{
			/* SYSTEM関数の終了コードを取得 */
			if( retCode == -1 )
			{
				sprintf( errMsg, "SFTP ERROR !! RET[%d] SERVER-No[%d]", retCode, svrSelect + 1 );
			}
			else if( WIFEXITED(retCode) )
			{
				retCode = WEXITSTATUS(retCode);
				sprintf( errMsg, "SFTP ERROR !! RET[%d] SERVER-No[%d]", retCode, svrSelect + 1 );
			}
			else if( WIFSIGNALED(retCode) )
			{
				retCode = WTERMSIG( retCode );
				sprintf( errMsg, "SFTP ERROR !! RET(SIGNo)[%d] SERVER-No[%d]", retCode, svrSelect + 1 );
			}
			else
			{
				sprintf( errMsg, "SFTP ERROR !! RET(OTHER)[%d] SERVER-No[%d]", retCode, svrSelect + 1 );
			}

			/* システムログを収集 */
			commfunc_syslog_msgset( pv_own_name, errMsg );

			/* サーバ選択変数を＋１ */
			*selectSvr = (*selectSvr + 1) % 10;

			/* SFTP送信状態にエラーを設定 */
			commfunc_manage_sftpSndStsSet( PV_FLG_STS_SET, sendFiles );

			/* 送信確認時間が２０分を経過した場合 */
			if( time( NULL ) > (sendChkTime + PV_FTP_ERR_MAX ) )
			{
				/* カウンタをリセット */
				sendChkTime = time( NULL );
				/* 管理プロセスにルータの再起動を要求する */
				commfunc_manage_routerResetSet( 1 );
			}

			/* 異常終了を設定 */
			retCode = -1;
		}
		else
		{
			/* 送信確認時間を更新 */
			sendChkTime = aftTime;

			/* 送信にかかった時間を算出 */
			aftTime -= befTime;
#ifdef PV_DEBUG
			printf("NKFTPSND : SEND TIME:%d\n", aftTime );
#endif
			/* 送信にかかった時間がプログラムのウォッチドッグ時間の 1/4 より短い場合 */
			if( aftTime < ((PV_WDOG_CYCLE * PV_WDOG_LMTCNT) / 4) )
			{
				/* 一度に送信するファイル数が最大送信数以下の場合 */
				if( limitFiles <= PV_FTP_FILES )
				{
					/* 一度に送信するファイル数を２倍に設定 */
					limitFiles *= 2;
#ifdef PV_DEBUG
			printf("NKFTPSND : SEND LIMIT FILES UP [%d]\n", limitFiles );
#endif
				}
			}
			/* 送信にかかった時間がプログラムのウォッチドッグ時間の 1/2 より長い場合 */
			else if( aftTime > ((PV_WDOG_CYCLE * PV_WDOG_LMTCNT) / 2) )
			{
				/* 一度に送信するファイル数を 1/2 に設定 */
				limitFiles /= 2;
				/* 補正が必要な場合は補正を行う */
				if( limitFiles < 1 ) limitFiles = 1;
			}

			/* SFTP送信状態に平常（リセット）を設定 */
			commfunc_manage_sftpSndStsSet( PV_FLG_STS_RESET, sendFiles );

			/* バッチファイル名が存在する場合 */
			if( access( listFile, R_OK | W_OK ) == 0 )
			{
				/* 既存のバッチファイルを削除 */
				unlink( listFile );
			}
		}


		rebootFlag = 0;

		/* アップデートファイル（データ）がダウンロードされているか確認する */
		sprintf( tmpFile1, "%s%s_DATA_UPDATE.tgz", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName );
		if( access( tmpFile1, R_OK | W_OK ) == 0 )
		{
			/* 装置状態状変：アップデートファイル検出 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DATA_FILE_FOUND, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

			/* ファイルをリネームする */
			sprintf( tmpFile2, "%sDATA_UPDATE_NEW.tgz", PV_PATH_DWL );
			rename( tmpFile1, tmpFile2 );
			rebootFlag = 1;
			/* システムログを収集 */
			sprintf( errMsg, "***** DATA UPDATE FILE FOUND !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
		/* アップデートファイル（プログラム）がダウンロードされているか確認する */
		sprintf( tmpFile1, "%s%s_PROC_UPDATE.tgz", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName );
		if( access( tmpFile1, R_OK | W_OK ) == 0 )
		{
			/* 装置状態状変：アップデートファイル検出 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_PROC_FILE_FOUND, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

			/* ファイルをリネームする */
			sprintf( tmpFile2, "%sPROC_UPDATE_NEW.tgz", PV_PATH_DWL );
			rename( tmpFile1, tmpFile2 );
			rebootFlag = 1;
			/* システムログを収集 */
			sprintf( errMsg, "***** PROGRAM UPDATE FILE FOUND !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}

		/* 遠隔データ送信ロック解除ファイルがダウンロードされているか確認する */
		sprintf( tmpFile1, "%s%s_COMMAND_DATASEND", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName );
		if( access( tmpFile1, R_OK | W_OK ) == 0 )
		{
			/* ファイルを削除する */
			unlink( tmpFile1 );
			/* データ送信ロックファイルを削除する */
			sprintf( tmpFile1, "%s%s", PV_PATH_DAT, PV_FNAME_PV_DTSDLOCK );
			unlink( tmpFile1 );
			/* システムログを収集 */
			sprintf( errMsg, "***** DATASEND(UNLOCK DATA SEND) COMMAND FILE FOUND !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
		/* 遠隔コマンド（サイレント設定）ファイルがダウンロードされているか確認する */
		sprintf( tmpFile1, "%s%s_COMMAND_SILENTMODE", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName );
		if( access( tmpFile1, R_OK | W_OK ) == 0 )
		{
			/* ファイルをリネームする */
			sprintf( tmpFile2, "%s%s", PV_PATH_DAT, PV_FNAME_PV_SILENTRB );
			rename( tmpFile1, tmpFile2 );
			/* システムログを収集 */
			sprintf( errMsg, "***** SILENT MODE COMMAND FILE FOUND !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			/* 管理プロセスがシャットダウン・再起動フラグより先に認識する時間を確保する */
			sleep( 1 );
		}
		/* 遠隔シャットダウンファイルがダウンロードされているか確認する */
		sprintf( tmpFile1, "%s%s_COMMAND_SHUTDOWN", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName );
		if( access( tmpFile1, R_OK | W_OK ) == 0 )
		{
			/* 装置状態状変：シャットダウン予約開始 */
			commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_SET, PV_MKIND_PVSYS, 1, PV_SYS_SV_SHUTDOWN_RSV, 1, 0, PV_SYS_SV_SHUTDOWN_RSV-1 );

			/* ファイルをリネームする */
			sprintf( tmpFile2, "%s%s", PV_PATH_DAT, PV_FNAME_PV_SHUTDOWN );
			rename( tmpFile1, tmpFile2 );
			/* システムログを収集 */
			sprintf( errMsg, "***** SHUTDOWN COMMAND FILE FOUND !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
		/* 遠隔リブートファイルがダウンロードされているか確認する */
		sprintf( tmpFile1, "%s%s_COMMAND_REBOOT", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName );
		if( access( tmpFile1, R_OK | W_OK ) == 0 )
		{
			/* ファイルを削除する */
			unlink( tmpFile1 );
			/* リブート対象フラグをセットする */
			rebootFlag = 1;
			/* システムログを収集 */
			sprintf( errMsg, "***** REBOOT COMMAND FILE FOUND !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
		/* リブート対象のファイルがダウンロードされている場合 */
		if( rebootFlag )
		{
			/* 装置状態状変：再起動開始 */
			commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_SET, PV_MKIND_PVSYS, 1, PV_SYS_SV_REBOOT_ST, 1, 0, PV_SYS_SV_REBOOT_ST-1 );

			/* システムログを収集 */
			sprintf( errMsg, "***** REBOOT REQUEST SET !! *****" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			/* 装置再起動フラグを設定する */
			sprintf( sftpCommand, "/usr/bin/touch %s%s", PV_PATH_DAT, PV_FNAME_PV_REBOOT );
			system( sftpCommand );
		}
		rebootFlag = 0;
	
		/* ダウンロードフォルダに残された遠隔操作コマンドファイル一覧を取得する */
		/* （一覧作成時に func_choice_filename_CMD フィルタを使用し、アルファベット順にソートする） */
		retCode1 = scandir( PV_PATH_DWL, &fileList, func_choice_filename_CMD, alphasort );
		if( retCode1 == -1 )
		{
			commfunc_syslog_msgset( pv_own_name, "Remote Command File List Get Error" );
			return( -1 );
		}
		/* 処理対象ファイルが存在する場合 */
		if( retCode1 > 0 )
		{
			/* ファイル数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < retCode1 ; loopCnt1++ )
			{
				/* 削除ファイルパスを設定 */
				sprintf( tmpFile1, "%s%s", PV_PATH_DWL, fileList[ loopCnt1 ]->d_name );
				/* ファイルを削除 */
				unlink( tmpFile1 );

#ifdef PV_DEBUG
				printf("NKFTPSND : Delete Remote Command FILE-No:%02d [ %s ]\n", loopCnt1+1, tmpFile1 );
#endif

				/* リソースを開放 */
				free( fileList[ loopCnt1 ]);
			}
		}
		/* リソースを開放 */
		free( fileList );
		/* ダウンロードフォルダに格納した制御指示ファイルファイル一覧を取得する */
		/* （一覧作成時に func_choice_filename_CTL フィルタを使用し、アルファベット順にソートする） */
		retCode1 = scandir( PV_PATH_DWL, &fileList, func_choice_filename_CTL, alphasort );
		if( retCode1 == -1 )
		{
			commfunc_syslog_msgset( pv_own_name, "Remote Command File List Get Error" );
			return( -1 );
		}
		/* 処理対象ファイルが存在する場合 */
		if( retCode1 > 0 )
		{
			/* リストファイルをオープン(追記) */
			if( (fp = fopen( listFile, "a" )) == NULL )
			{
				return( -1 );
			}
		
			/* ファイル数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < retCode1 ; loopCnt1++ )
			{
				/* サーバ上の指示ファイルの削除を行うコマンドの登録 */
				fprintf( fp, "-rm %s\n", fileList[ loopCnt1 ]->d_name );
#ifdef PV_DEBUG_DT2
				printf("-rm %s\n", fileList[ loopCnt1 ]->d_name );
#endif
				/* 一時バッファにファイル名(拡張子除く)を設定 */
				memset(tmpBuff, 0, sizeof(tmpBuff));
				strncpy( tmpBuff, fileList[ loopCnt1 ]->d_name, (strlen(fileList[ loopCnt1 ]->d_name)-4) );

				/* 変更前ファイルパスを設定 */
				sprintf( tmpFile1, "%s%s", PV_PATH_DWL, fileList[ loopCnt1 ]->d_name );

				/* 変更後ファイルパスを設定 */
				sprintf( tmpFile2, "%s%s%s", PV_PATH_DWL, tmpBuff, ".csv" );

				rename( tmpFile1, tmpFile2 );
#ifdef PV_DEBUG_DT2
				printf("NKFTPSND : RenameFile FILE-No:%02d [%s]->[%s]\n", loopCnt1+1, tmpFile1, tmpFile2 );
#endif
				/* リソースを開放 */
				free( fileList[ loopCnt1 ]);
			}
			/* バッチファイルを閉じる */
			fclose( fp );
		}
		/* リソースを開放 */
		free( fileList );
	}

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（送信データ用）
 *
 * 機能：
 * 		func_pvserver_ftpsend()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名から収集装置ＩＤの一致するものや、フ
 * 		ァイル名が正規のものだけを処理する。
 * 		ファイル名から一定期間経過したものかを判断し、一定期間を経過したものは
 * 		削除を行う。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_filename( const struct dirent *dir )
{
	time_t					secData;					/* 積算秒 */
	struct tm				timeData;					/* 時刻情報 */
	char					checkName[256];				/* 確認用ファイル名 */
	char					deleteName[256];			/* 削除ファイル名 */
	char					prefixStr[6];

	/* 現在時刻（積算秒）の取得 */
	secData = time( NULL );
	/* 現在時刻に保持期間を反映 */
	secData -= ps_g_PVSVRINF->stockHour * 3600;
	/* 保持期間の時刻を年月日時分を取得 */
	localtime_r( &secData, &timeData );
	/* 比較用ファイル名を生成 */
	sprintf( checkName, "%s%04d%02d%02d%02d%02d%02d",
		ps_g_PVSYSINF->siteIdName,
		timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec);

/* [20250807] Chg St */
	/* 送信ファイル名が２６、３１、３３文字以外の場合は処理対象外のため除外	*/
	if( ( strnlen( dir->d_name, 32 ) != 26 ) &&
		( strnlen( dir->d_name, 32 ) != 31 ) &&
		( strnlen( dir->d_name, 34 ) != 33 ) )
/* [20250807] Chg Ed */
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}
	else
	{
		/* 31文字の場合、prefixが合っているかチェック */
		if( strnlen( dir->d_name, 32 ) == 31 )
		{
			/* Prefixが一致しない場合は、祖より対象外 */
			if( (strncmp( &dir->d_name[22], PV_FNAME_TMREC_PREFIX, strlen(PV_FNAME_TMREC_PREFIX) ) != 0 ) &&
			    (strncmp( &dir->d_name[22], PV_FNAME_SVREC_PREFIX, strlen(PV_FNAME_SVREC_PREFIX) ) != 0 ) )
			{
#ifdef PV_DEBUG_DT1
				printf("NKFTPSND : FILE NAME ERROR PREFIX : ( %s )\n", dir->d_name );
#endif
				return( 0 );
			}
		}
/* [20250807] Add St */
		/* 33文字の場合、prefixが合っているかチェック */
		else if( strnlen( dir->d_name, 34 ) == 33 )
		{
			/* Prefixが一致しない場合は、祖より対象外 */
			if( (strncmp( &dir->d_name[22], PV_FNAME_FCRREPO_PREFIX, strlen(PV_FNAME_FCRREPO_PREFIX) ) != 0 ) )
			{
#ifdef PV_DEBUG_DT1
				printf("NKFTPSND : FILE NAME ERROR FCRREPO PREFIX : ( %s )\n", dir->d_name );
#endif
				return( 0 );
			}
		}
/* [20250807] Add Ed */
	}

	/* 送信ファイル名の先頭８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
	if( strncmp( dir->d_name, ps_g_PVSYSINF->siteIdName, 8 ) != 0 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : SITE ID ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 送信ファイル名の最後の４文字が .DAT 以外は処理対象外 */
	if( strncmp( &dir->d_name[strlen(dir->d_name)-4], ".DAT", 4 ) != 0 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 保存期間を超過したファイルは処理対象外 */
	if( strncmp( dir->d_name, checkName, 22 ) < 0 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : STOCK OUT FILE [ %s ] < [ %s ]\n", dir->d_name, checkName );
#endif
		/* 保存期間を超過したファイルを削除 */
		strcpy( deleteName, curdir );
		strcat( deleteName, "/");
		strcat( deleteName, dir->d_name);
		unlink( deleteName );
		return( 0 );
	}

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（指示ファイル用）
 *
 * 機能：
 * 		func_()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名から「収集装置ＩＤ_COMMAND_*」に一致
 *		するファイル名だけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_filename_CMD( const struct dirent *dir )
{
	/* 確認ファイル名が１７文字以外の場合は処理対象外のため除外	*/
	if( strnlen( dir->d_name, 17 ) < 17 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : COMMAND FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 確認ファイル名の先頭８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
	if( strncmp( dir->d_name, ps_g_PVSYSINF->siteIdName, 8 ) != 0 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : SITE ID ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 確認ファイル名の９バイト目から９文字が _COMMAND_ 以外は処理対象外 */
	if( strncmp( &dir->d_name[8], "_COMMAND_", 9 ) != 0 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : COMMAND FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 処理対象 */
	return( 1 );
}

/* [20230131] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（文字検索）
 *
 * 機能：
 * 		ファイル内の文字列を検索し、あった場合、なかった場合、異常を返す。
 * 		あった場合は、その行を削除したファイルに書き直す。
 * 戻り値：
 * 	   -1：異常終了
 * 		0：正常処理（検索なし）
 * 		1：正常処理（検索対象あり）
 */
int func_file_StrCheck( char *filePath, char *endStr )
{
	int		loopCnt1;
	int		lineCnt = 0;  			/* 行数 */
	char	fileData[100][512];		/* 計画(3種×連系/自立×14日) + 設定変更 */
	char	readBuff[512];			/* CSVファイル読み込みバッファ */
	FILE	*fp;					/* SFTPバッチファイルポインタ */
	int		findFlg=0;				/* 検索結果 */
	int		err_no;

	/* 領域初期化 */
	memset(fileData, 0, sizeof(fileData));

	/* ファイルを読み込みで開く */
	if( (fp = fopen( filePath, "r" )) == NULL )
	{
		err_no = errno;
		sprintf(errMsg, "[%s] read fopen Error", filePath);
		commfunc_syslog_msgset( pv_own_name, errMsg );

		sprintf(errMsg, "errno:%d. error string by strerror: %s", err_no, strerror(err_no) );
		commfunc_syslog_msgset( pv_own_name, errMsg );

		return( -1 );
	}

	while( fgets( fileData[lineCnt], sizeof(fileData[lineCnt]), fp) != NULL )
	{
		//printf("%s", fileData[lineCnt] );
		/* 検索文字列が一致した場合 */
		if( strncmp(fileData[lineCnt], endStr, strlen(endStr)) == 0 )
		{
			findFlg = 1;
		}
		else
		{
			lineCnt++;
		}
	}
	/* ファイルを閉じる */
	fclose(fp);

	/* ファイル削除 */
	if( unlink( filePath ) < 0 )
	{
		err_no = errno;
		sprintf(errMsg, "[%s] unlink Error", filePath);
		commfunc_syslog_msgset( pv_own_name, errMsg );

		sprintf(errMsg, "errno:%d. error string by strerror: %s", err_no, strerror(err_no) );
		commfunc_syslog_msgset( pv_own_name, errMsg );

		return( -3);
	}

	/* ファイルを書き込みで開く */
	if( (fp = fopen( filePath, "w" )) == NULL )
	{
		err_no = errno;
		sprintf(errMsg, "[%s] write fopen Error", filePath);
		commfunc_syslog_msgset( pv_own_name, errMsg );

		sprintf(errMsg, "errno:%d. error string by strerror: %s", err_no, strerror(err_no) );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		
		return( -2 );
	}

	/* 検索文字を除いた行を書き込み */
	for( loopCnt1=0; loopCnt1 < lineCnt; loopCnt1++ )
	{
		fputs(fileData[loopCnt1], fp);
	}

	/* ファイルを閉じる */
	fclose(fp);

	return findFlg;
}
/* [20230131] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（制御指示ファイル用）
 *
 * 機能：
 * 		func_()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名から「制御指示ファイル」に一致
 *		するファイル名だけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_filename_CTL( const struct dirent *dir )
{
	char		checkName[128];		/* 確認用ファイル名 */
/* [20230131] Add St */
	int			retCode;
	char		filePath[128];
	FILE		*fp;							/* SFTPバッチファイルポインタ */
/* [20230131] Add Ed */

	/* 確認ファイル名の先頭８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
	if( strncmp( dir->d_name, ps_g_PVSYSINF->siteIdName, 8 ) != 0 )
	{
#ifdef PV_DEBUG_DT2
		printf("NKFTPSND : SITE ID ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}
	/* 確認ファイル名の２１バイト目から１３文字が RTCTLINF.DAT,UPDATECNF.DAT 以外は処理対象外 */
	sprintf( checkName, "%s%s", PV_FNAME_REMOTECTL, ".DAT");
	if( strncmp( &dir->d_name[20], checkName, 14 ) != 0 )
	{
		sprintf( checkName, "%s%s", PV_FNAME_UPDATECNF, ".DAT");
		if( strncmp( &dir->d_name[20], checkName, 14 ) != 0 )
		{
#ifdef PV_DEBUG_DT2
			printf("NKFTPSND : CTL FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
			return( 0 );
		}
	}

/* [20230131] Add St */
	/* ファイルパスの設定 */
	strcpy( filePath, PV_PATH_DWL );
	strcat( filePath, dir->d_name );

	/* ファイル内文字検索 */
	retCode = func_file_StrCheck( filePath, "END");
#ifdef PV_DEBUG_DT2
	printf("NKFTPSND : func_file_StrCheck : %d : ( %s )\n", retCode, dir->d_name );
#endif

	/* 異常終了、もしくは、検索文字なしの場合 */
	if( retCode <= 0 )
	{
		/* ファイル削除 */
		unlink( filePath );

		sprintf(errMsg, "[%s] is data Error(%d)", dir->d_name, retCode );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return( 0 );
	}
/* [20230131] Add Ed */

	/* 処理対象 */
	return( 1 );
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

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（送信データ用）
 *
 * 機能：
 * 		func_pvserver_ftpsend()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるフォルダ名から送信対象のファイルの有無を判断し、
 * 		ファイルがあるものだけを処理する。
 * 		フォルダの日付が一定期間経過したものかを判断し、
 * 		一定期間を経過したフォルダはフォルダ内のファイルを含め削除を行う。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_dirname( const struct dirent *dir )
{
	time_t					secData;					/* 積算秒 */
	struct tm				timeData;					/* 時刻情報 */
	char					checkName[256];				/* 確認用ファイル名 */
	struct dirent			**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int						retCode;					/* 関数戻り値 */
	int						loopCnt1;					/* ループカウンタ */
	char					deldir[256];				/* 一時ディレクトリ名*/
	char					delfile[256];				/* 一時ファイル名 */
	int						filenum;					/* ファイル数 */
	char					today[16];					/* 確認用ファイル名 */

	/* 現在時刻（積算秒）の取得 */
	secData = time( NULL );
	localtime_r( &secData, &timeData );
	/* 現在日時のフォルダ名を生成 */
	sprintf( today, "%04d%02d%02d", timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday);

	/* 現在時刻（積算秒）の取得 */
	secData = time( NULL );
	/* 現在時刻に保持期間を反映 */
	secData -=  ps_g_PVSVRINF->stockHour * 60 * 60;
	/* 保持期間の時刻を年月日時分を取得 */
	localtime_r( &secData, &timeData );
	/* 比較用フォルダ名を生成 */
	sprintf( checkName, "%04d%02d%02d", timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday);

	/* 名前が9文字以外（年月日＝８）以外の場合は除外	*/
	if( strnlen( dir->d_name, 9 ) != 8 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : DIR NAME ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 拡張子がある場合は除外 */
	if( strstr( dir->d_name, "." ) != NULL )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : EXTENSION EXIST ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* ディレクトリに残されたファイルの一覧を取得する */
	filenum = 0;
	strcpy( deldir, PV_PATH_FTP );
	strncat( deldir, dir->d_name, 8 );
	retCode = scandir( deldir, &fileList, NULL, alphasort );
	if( retCode < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "FTP Send Dir List Get Error" );
		return( 0 );
	}

	/* 保存期間(日付)を超過したディレクトリは削除対象 */
	if( strncmp( dir->d_name, checkName, 8 ) < 0 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : STOCK OUT DELETE DIR [ %s ] < [ %s ]\n", dir->d_name, checkName );
#endif

		/* 対象ファイルが存在する場合 */
		if( retCode > 0 )
		{
			/* ファイル数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < retCode ; loopCnt1++ )
			{
				/* カレントディレクトリ「.」は、削除対象外 */
				/* 親ディレクトリ「..」は、削除対象外 */
				if( (strcmp(".", fileList[ loopCnt1 ]->d_name) == 0) ||
					(strcmp("..", fileList[ loopCnt1 ]->d_name) == 0))
            	{
					/* 対象外 */
				}
				else
				{
					/* ファイルの削除を実施  */
					strcpy( delfile, deldir );
					strcat( delfile, "/");
					strcat( delfile, fileList[ loopCnt1 ]->d_name);
					unlink( delfile );
#ifdef PV_DEBUG_DT1
					printf("NKFTPSND : DELETE FILE IN THE DELETE DIR [ %s ]\n", fileList[ loopCnt1 ]->d_name );
#endif
				}

				/* リソースを開放 */
				free( fileList[ loopCnt1 ]);
			}
		}
		/* リソースを開放 */
		free( fileList );
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : DELETE DIR [ %s ]\n", deldir );
#endif
		/* ディレクトリの削除を実施 */
		rmdir( deldir );
		return( 0 );
	}
	else
	{
		/* 対象ファイルが存在する場合 */
		if( retCode > 0 )
		{
			/* ファイル数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < retCode ; loopCnt1++ )
			{
				/* カレントディレクトリ「.」は、削除対象外 */
				/* 親ディレクトリ「..」は、削除対象外 */
				if( (strcmp(".", fileList[ loopCnt1 ]->d_name) == 0) ||
					(strcmp("..", fileList[ loopCnt1 ]->d_name) == 0))
	        	{
					/* 対象外 */
				}
				/* 送信ファイル名の収集装置ＩＤと一致しないファイルは削除 */
				else if(strncmp( fileList[ loopCnt1 ]->d_name, ps_g_PVSYSINF->siteIdName, 8 ) != 0 )
				{
					/* ファイルの削除を実施  */
					strcpy( delfile, deldir );
					strcat( delfile, "/");
					strcat( delfile, fileList[ loopCnt1 ]->d_name);
					unlink( delfile );
#ifdef PV_DEBUG_DT1
					printf("NKFTPSND : DELETE FILE IN THE DELETE DIR [ %s ]\n", fileList[ loopCnt1 ]->d_name );
#endif
				}
/* [20250807] Chg St */
				/* 現在日ではないディレクトリの場合、且つ */
				/*   ファイル名が２６、３１、３３文字ではない、または、 */
				/*   拡張子が「.DAT」ではない場合 */
				else if( ( strncmp( dir->d_name, today, 8 ) != 0 ) &&
					     ((( strnlen( fileList[ loopCnt1 ]->d_name, 32 ) != 26 ) &&
					       ( strnlen( fileList[ loopCnt1 ]->d_name, 32 ) != 31 ) &&
					       ( strnlen( fileList[ loopCnt1 ]->d_name, 34 ) != 33 )) ||
					      ( strncmp( &fileList[ loopCnt1 ]->d_name[strlen( fileList[ loopCnt1 ]->d_name )-4], ".DAT", 4 ) != 0 )))
/* [20250807] Chg Ed */
				{
					/* ファイルの削除を実施（現在日の場合、ファイルの作成中の可能性もあるため）  */
					strcpy( delfile, deldir );
					strcat( delfile, "/");
					strcat( delfile, fileList[ loopCnt1 ]->d_name);
					unlink( delfile );
#ifdef PV_DEBUG_DT1
					printf("NKFTPSND : DELETE FILE IN THE DELETE DIR [ %s ]\n", fileList[ loopCnt1 ]->d_name );
#endif
				}
				else
				{
					/* 対象ファイル数をインクリメント */
					filenum++;
				}

				/* リソースを開放 */
				free( fileList[ loopCnt1 ]);
			}
		}
		/* リソースを開放 */
		free( fileList );

		/* 対象ファイルがない、且つ、現在日ではないディレクトリは削除 */
		if(( strncmp( dir->d_name, today, 8 ) != 0 ) &&
		   ( filenum == 0 ))
		{
#ifdef PV_DEBUG_DT1
			printf("NKFTPSND : DELETE DIR [ %s ]\n", deldir );
#endif
			/* ディレクトリの削除を実施 */
			rmdir( deldir );
			return( 0 );
		}
	}

#ifdef PV_DEBUG_DT1
	/* 送信対象 */
	printf("NKFTPSND : SEND DIR [ %s ]\n", dir->d_name );
#endif

	/* 処理対象 */
	return( 1 );
}

