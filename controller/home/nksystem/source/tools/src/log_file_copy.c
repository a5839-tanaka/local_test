#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>	/* for mmap */
#include <sys/select.h>
#include <pwd.h>		/* for getpwuid */
#include <time.h>
#include <fcntl.h>		/* for open */
#include <termios.h>
#include <stdlib.h>		/* for atoi */
#include <unistd.h>		/* for ftruncate */
#include <string.h>		/* for memset, memcpy, strlen, strnlen */
#include <stdio.h>		/* for fopen , fgets */
#include <dirent.h>		/* for scandir */
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

int	func_choice_file( const struct dirent * );
int	func_file_copy( char *  );

static int signal_flg = 0;
static char *gId;
static char copyEndFile[256];				/* チェックファイル名 */
static char copyEndFileBatt[256];				/* チェックファイル名 */


/* シグナル受信時の処理 */
void	func_signal_recv( int signum )
{
	/* シグナル受信フラグセット */
	signal_flg = 1;
}

time_t 	updateTime[10];
int main(int argc, char *argv[])
{

	char					logFilePath[1024];			/* CSVファイルパス */
	static struct stat		statBuff;					/* ファイル状態 */
	struct tm				timeData;					/* 時刻情報 */
	char					command[256];				/* 確認用ファイル名 */
	struct sigaction		sigact;
	int						loopCnt;
	char					copyFilePath[1024];			/* CSVファイルパス */

	memset( copyEndFile, '0', sizeof(copyEndFile));
	memset( copyEndFileBatt, '0', sizeof(copyEndFileBatt));
	memset( updateTime, 0, sizeof(updateTime));
	
	memset( &sigact, 0, sizeof(sigact) );
	sigemptyset( &sigact.sa_mask );
	sigaddset( &sigact.sa_mask, SIGUSR1 );
	sigact.sa_handler  = func_signal_recv;
	sigact.sa_flags    = SA_RESETHAND;
	sigact.sa_restorer = 0;

	if( argc < 2 )
	{
		printf("--------------------------------------------------------------------------------------\n");
		printf("本ツールは、以下の機能を有します。\n");
		printf("(1).収集データファイル/バッテリ制御情報ファイルをローカルディレクトリにコピーする。\n");
		printf("(2).logディレクトリのログファイルをローカルディレクトリにコピーする。\n");
		printf("    ログファイルは、更新中のファイル名を入力しないでください。\n");
		printf("    (入力可能なファイルは、pvsys_log_1.logやserial-line01_1.logなど)\n");
		printf("--------------------------------------------------------------------------------------\n");
		printf("使用方法(引数の説明)は以下の通り。\n");
		printf("  arg[1] : 収集装置ID\n");
		printf("  arg[2] : /home/nksystem/logのログファイル名1\n");
		printf("  arg[3] : /home/nksystem/logのログファイル名2\n");
		printf("  ....\n");
		printf("  arg[9] : /home/nksystem/logのログファイル名3\n");
		printf("--------------------------------------------------------------------------------------\n");
		return(0);
	}

	printf("PID=%d\n", getpid());
	printf("argv[1]=%s：収集装置IDの収集データファイルをコピーします。\n", argv[1]);

	for( loopCnt = 2; loopCnt < argc; loopCnt++)
	{
		printf("argv[%d]=%s：/home/nksystem/log/%sをコピーします。\n", loopCnt, argv[loopCnt], argv[loopCnt]);
	}
	gId = argv[1];


	if( sigaction( SIGUSR1, &sigact, NULL ) != 0 )
	{
		printf("SYSDEBUG: sigacion");
		exit( 1 );
	}

	while(1)
	{
		sleep(1);

		if( signal_flg == 1 )
		{
			printf("receive exit!\n");
			break;
		}
		

		for( loopCnt = 2; loopCnt < argc; loopCnt++)
		{
			/* ファイルパスの展開 */
			strcpy(logFilePath, "/home/nksystem/log/" );
			strcat(logFilePath, argv[loopCnt]);

			/* ファイルが存在しているか確認 */
			if( access( logFilePath, R_OK | W_OK ) == 0 )
			{
				/* ファイル状態の取得に失敗 */
				if( stat( logFilePath, &statBuff ) != 0 )
				{
					printf("stat Error\n"); 
				}
				else
				{
					/* ファイルが変更されている場合 */
					if( updateTime[loopCnt] != statBuff.st_mtime )
					{
						/* 最終更新時刻を保存 */
						updateTime[loopCnt] = statBuff.st_mtime;

						localtime_r( &updateTime[loopCnt], &timeData );
						sprintf(copyFilePath, "%04d%02d%02d%02d%02d%02d%s", 
							timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, argv[loopCnt] );
						sprintf( command, "cp %s ./%s", logFilePath, copyFilePath);
						system( command );
						printf("%sをコピーしました。\n", copyFilePath);
					}
				}
			}
		}
		func_file_copy(gId);
	}

	return 0;

}

int	func_choice_file( const struct dirent *dir )
{
	char					extPos;

	struct tm		timeData;					/* 時刻情報 */
	time_t			Time;						/* 現在積算秒 */
	char			*p_endFile;

	/* 積算秒を取得 */
	Time = time( NULL );
	/* 時刻情報に変換 */
	localtime_r( &Time, &timeData );

	/* 送信ファイル名が２４、３２文字以外の場合は処理対象外のため除外	*/
	if( strnlen( dir->d_name, 25 ) == 24 )
	{
		extPos = 20;
		p_endFile = copyEndFile;
	}
	else if( strnlen( dir->d_name, 33 ) == 32 )
	{
		extPos = 28;
		p_endFile = copyEndFileBatt;
	}
	else
	{
		return( 0 );
	}

	/* ファイル名の１２文字目からの８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
	if( strncmp( &dir->d_name[12], gId, 8 ) != 0 )
	{
		return(0);
	}

	/* 送信ファイル名の最後の４文字が .DAT 以外は処理対象外 */
	if( strncmp( &dir->d_name[extPos], ".DAT", 4 ) != 0 )
	{
		return( 0 );
	}

	/* 最後に送ったファイルより過去のファイルは対象外 */
	if( strncmp( dir->d_name, p_endFile, 12 ) <= 0 )
	{
		return( 0 );
	}


	/* 処理対象 */
	return( 1 );
}

int	func_file_copy( char *id )
{
	struct dirent	**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int				retCode=-1;					/* 関数戻り値 */
	int				loopCnt1;					/* ループカウンタ */
	int				loopCnt2;
	char			checkFile[256];				/* チェックファイル名 */
	int				filenum;					/* ファイル数 */
	char			path[256];
	FILE			*fp;						/* ファイルディスクリプタ */
	int				fileSize;
	char			plantId[27];
	struct tm		timeData;					/* 時刻情報 */
	time_t			Time;						/* 現在積算秒 */
	char					command[256];				/* 確認用ファイル名 */

	if( gId == NULL )
		return 0;
	
	/* 積算秒を取得 */
	Time = time( NULL );
	/* 時刻情報に変換 */
	localtime_r( &Time, &timeData );


	sprintf(path, "/home/nksystem/FTPDATA/%04d%02d%02d/", timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday);

	/* スケジュール制御ファイルのリストを取得 */
	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	retCode = scandir( path, &fileList, func_choice_file, alphasort );
	if( retCode < 0 )
	{
		return( -1 );
	}
	/* ファイル数を設定 */
	filenum = retCode;

	/* ファイル数が0より大きい場合 */
	if( filenum > 0 )
	{
		/* ファイルリストを昇順で繰り返す */
		for( loopCnt1 = 0; loopCnt1 < filenum; loopCnt1++ )
		{
			/* ファイルを解析する */
			memset( checkFile, 0x00, sizeof(checkFile));
			sprintf( checkFile, "%s%s", path, fileList[loopCnt1]->d_name);

			sprintf( command, "cp -p %s ./%s", checkFile, fileList[loopCnt1]->d_name);
			system( command );
			printf("%sをコピーしました。\n", fileList[loopCnt1]->d_name);
			/* 最終ファイル日時を保持 */
			if( strnlen( fileList[loopCnt1]->d_name, 25 ) == 24 )
			{
				memcpy(copyEndFile, fileList[loopCnt1]->d_name, 12);
			}
			else if( strnlen( fileList[loopCnt1]->d_name, 33 ) == 32 )
			{
				memcpy(copyEndFileBatt, fileList[loopCnt1]->d_name, 12);
			}

			/* リソースを開放 */
			free( fileList[loopCnt1] );
		}
	}
	/* リソースを開放 */
	free( fileList );


	return( retCode );
}
