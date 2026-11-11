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

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvudp.h"
#include "pvcomm.h"

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_fileIndex     s_fileIndex[] =				/* ファイル定義テーブル（必須ファイル） */
{
    {   PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
    {   0,                                     NULL, 0, 0, 0 }
};

struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};

int func_sep_data( char *, struct st_sepStr * );
int	conffile_copy( char *inputPath, char *outputPath, int dateFlg );

static int signal_flg = 0;
static char *gId;
static char copyEndFile[256];				/* チェックファイル名 */
static char copyEndFileBatt[256];				/* チェックファイル名 */

#define	PV_PATH_DWL			"/home/nksystem/DWLDATA/"	/* ダウンロードファイル格納ディレクトリ */
#define	PV_PATH_TOOL		"/home/nksystem/tool/"	/* ダウンロードファイル格納ディレクトリ */
#define PV_FNAME_UPDATECNF		"UPDATECNF"			/* 設定変更ファイル */

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
	char					tempFilePath[1024];			/* CSVファイルパス */
	char					systemCmd[1024];			/* CSVファイルパス */
	char					confFileManage[1024];			/* CSVファイルパス */
	char					copyFileName[1024];			/* CSVファイルパス */
	char					todayStr[1024];			/* CSVファイルパス */
	int						copyFileNameNumb;
	char					readBuff[1024];
	int						retCode=0;
	struct timeval			nowTime;					/* 現在時刻 */
	char					checkName[128];		/* 確認用ファイル名 */
	char					checkFile[1024];
	char					extension[5];			/* 拡張子 */
	FILE			*fp;
	struct tm				localTime;				/* 現在時刻 */
	int						mutexCreate = 0;
	char					siteIdName[16];		/* 収集装置ID */
	int						dateFlg = 0;

	/* ミューテックス領域をアタッチ */
	if( commfunc_mutex_attach() != 0 )
	{
		/* ミューテックス領域を作成 */
		if( commfunc_mutex_create() != 0 )
		{
			printf("File Resource Create Error!\n");
			return( -1 );
		}
		/* ミューテックス作成フラグ設定 */
		mutexCreate = 1;
	}

	/* 使用ファイルをオープンしてＭＭＡＰ化 */
	retCode = commfunc_file_open( s_fileIndex );
	if(retCode < 0)
	{
		printf("File Resource Open Error!\n" );
		return( -1 );
	}

	/* 収集装置IDをコピー */
	memcpy(siteIdName, ps_g_PVSYSINF->siteIdName, sizeof(siteIdName));
	/* 設定変更ファイル通し番号コピー */
	memset( copyFileName, 0, sizeof(copyFileName));
	memcpy(copyFileName, &ps_g_PVSYSINF->updateConfFile[8], 12 );

	/* 使用ファイルをクローズ */
	commfunc_file_close( s_fileIndex );

	memset( copyEndFile, '0', sizeof(copyEndFile));
	memset( copyEndFileBatt, '0', sizeof(copyEndFileBatt));
	memset( updateTime, 0, sizeof(updateTime));
	
	memset( &sigact, 0, sizeof(sigact) );
	sigemptyset( &sigact.sa_mask );
	sigaddset( &sigact.sa_mask, SIGUSR1 );
	sigact.sa_handler  = func_signal_recv;
	sigact.sa_flags    = SA_RESETHAND;
	sigact.sa_restorer = 0;

	//if( argc < 2 )
	//{
		printf("--------------------------------------------------------------------------------------\n");
		printf("本ツールは、以下の機能を有します。\n");
		printf("(1).引数のアップデートファイルをコピーし、リネームする。\n");
		printf("(2).引数をファイル名だけにすると、PWPINFを現在日に置換しま。\n");
		printf("(3).引数をファイル名 [nochange]にすると、PWPINFを日付は変更しません。\n");
		printf("--------------------------------------------------------------------------------------\n");
	//	printf("使用方法(引数の説明)は以下の通り。\n");
	//	printf("  arg[1] : 使用するアップデートファイル（先頭8文字は収集装置IDであること）\n");
	//	printf("--------------------------------------------------------------------------------------\n");
	//	return(0);
	//}


	/* 引数がファイル名だけの場合 */
	if( argc == 2 )
	{
		/* 時刻更新フラグをON */
		dateFlg = 1;
	}
	else if( argc == 3 )
	{
		if( 0 == strcmp( "nochange", argv[2] ) )
		{
			/* 時刻更新フラグをON */
			dateFlg = 0;
		}
		else
		{
			/* 時刻更新フラグをON */
			dateFlg = 1;
		}
		
	}
//	printf("PID=%d\n", getpid());

	sprintf(checkFile, "%s%s", PV_PATH_DWL, argv[1] );

	if( strstr( argv[1], ".csv" ) != NULL )
	{
		sprintf(extension, "csv");
	}
	else if( strstr( argv[1], ".DAT" ) != NULL )
	{
		sprintf(extension, "DAT");
	}
	else
	{
		printf("最初の拡張子を.csv/.DATにしてください\n");
		return 0;
	}

	/* 確認ファイル名の２１バイト目から９文字が UPDATECNF 以外は処理対象外 */
	if( strncmp( &argv[1][20], PV_FNAME_UPDATECNF, strlen(PV_FNAME_UPDATECNF) ) != 0 )
	{
		printf("[%s]は設定変更指示ファイルではありません。\n", argv[1], PV_PATH_DWL );
		return 0;
	}

	/* 管理ファイルが存在しない場合 */
	if( access( checkFile, R_OK ) != 0 )
	{
		printf("[%s]が%sのディレクトリになりません。\n", argv[1], PV_PATH_DWL );
		return 0;
	}

#if 0
	sprintf( confFileManage, "%s%s", PV_PATH_TOOL, PV_FNAME_UPDATECNF );
	memset( copyFileName, 0, sizeof(copyFileName));

	/* 管理ファイルが存在する場合 */
	if( access( confFileManage, R_OK ) == 0 )
	{
		/* 管理ファイルオープン */
		if( (fp = fopen(confFileManage, "rw")) != NULL)
		{
			/* CSVファイルの終端まで繰り返す */
			if( fgets( readBuff, 4096, fp) != NULL )
			{
				sprintf(copyFileName, "%s", readBuff);
			}
			else
			{
				printf("Error1\n");
				retCode = -1;
			}
			/* ファイルをクローズする */
			fclose( fp );
		}
		else
		{
			printf("Error2\n");
			retCode = -1;
		}
	}

	if( retCode == -1 )
	{
		printf("ファイルのオープンに失敗しました。\n[%s]\n", confFileManage );
		return 0;
	}
#endif
	/* 年月日時分秒を取得 */
	gettimeofday( &nowTime, NULL );
	localtime_r( &nowTime.tv_sec, &localTime );
	sprintf(todayStr, "%04d%02d%02d0001",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday);

	if( ( strlen(copyFileName) == 0 ) ||
	    ( strncmp( copyFileName, todayStr, 8 ) < 0 ) )
	{
		sprintf(copyFileName, "%s", todayStr);
	}
	else
	{
		copyFileNameNumb = atoi(&copyFileName[8]);
		copyFileNameNumb++;
		sprintf( &copyFileName[8], "%04d", copyFileNameNumb);
	}

	sprintf(copyFilePath, "%s%s%s%s.%s", PV_PATH_DWL, siteIdName, copyFileName, PV_FNAME_UPDATECNF, extension );
	sprintf(tempFilePath, "%s%s%s%s.tmp", PV_PATH_DWL, siteIdName, copyFileName, PV_FNAME_UPDATECNF );

	//sprintf(systemCmd, "cp %s %s", checkFile, tempFilePath);
	//system( systemCmd );

	conffile_copy(checkFile, tempFilePath, dateFlg );

	/* ログファイルを旧ファイル名に変更 */
	rename( tempFilePath, copyFilePath );
	printf("%s -> %s\n", checkFile, copyFilePath );

#if 0
	/* 管理ファイルオープン */
	if( (fp = fopen(confFileManage, "wb")) == NULL)
	{
		printf("Error3\n");
		retCode = -1;
	}
	/* 終端情報を出力 */
	fprintf( fp, "%s\n", copyFileName );
//	printf( "[%s]\n", copyFileName);
	/* ファイルをクローズする */
	fclose( fp );
#endif

	return 0;

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



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル情報読み込み
 *
 * 機能：
 * 		夏時間情報を読み込み、更新する
 * 戻り値：
 * 	    0：正常
 * 	   -1：異常
 */
int	conffile_copy( char *inputPath, char *outputPath, int dateFlg )
{
	FILE					*fp;							/* ファイルポインタ */
	FILE					*fpOut;							/* ファイルポインタ */
	char					readBuff[4096];					/* ファイル読み込みバッファ */
	int						sepCount;						/* データ数（デリミタ分割数） */
	int						retCode = 0;					/* 戻り値 */
	struct st_sepStr		s_sepStr;						/* データ分割格納 */
	int						syoriCount = 0;					/* 処理カウンタ */
	int						dataCount = 0;					/* データ数 */
	int						loopCnt1 = 0;
	time_t					nowTime;					/* 現在時刻 */
	struct tm				localTime;					/* 現在時刻 */
	char					convData[256];				/* 変換データ */
	char					dateStr[32];					/* 時刻文字列 */

	nowTime = time( NULL );

	/* CSVファイルオープン */
	if( (fp = fopen(inputPath, "r")) != NULL)
	{
		/* CSVファイルオープン */
		if( (fpOut = fopen(outputPath, "w")) != NULL)
		{
			/* CSVファイルの終端まで繰り返す */
			while( fgets( readBuff, 4096, fp) != NULL )
			{
				/* CSV処理カウンタを＋１ */
				syoriCount++;
				/* コメント行はスキップ */
				if( readBuff[0] == '#' )
				{
					continue;
				}

				/* 未入力行はスキップ */
				if( strlen( readBuff ) == 0 ) continue;
				
				/* データの分割 */
				sepCount = func_sep_data( readBuff, &s_sepStr );

				/* データ数が0の場合はスキップ */
				if( sepCount == 0 ) continue;

				/* 充放電計画の場合 */
			    if( ( strncmp( s_sepStr.data[0], "PWPLNINF", 8 ) == 0 ) &&
				    ( dateFlg > 0 ) )
			    {
					/* 種別が需給計画有無の場合 */
					if( atoi(s_sepStr.data[2]) == 0 )
					{
						localtime_r( &nowTime, &localTime );
						sprintf(dateStr, "%04d%02d%02d", 
							localTime.tm_year + 1900,
							localTime.tm_mon + 1,
							localTime.tm_mday);
						/* 時刻を翌日にする */
						nowTime += (24*60*60);
					}

					for( loopCnt1=0; loopCnt1 < sepCount; loopCnt1++ )
					{
						if( loopCnt1 != 0 )
						{
							fprintf( fpOut, ","); 
						}

						if( loopCnt1 == 1 )
						{
							fprintf( fpOut, "%s", dateStr); 
						}
						else
						{
							fprintf( fpOut, "%s", s_sepStr.data[loopCnt1]); 
						}
					}
					fprintf( fpOut, "\n"); 
				}
				else
				{
					/* そのままコピーする */
					fprintf( fpOut, "%s", readBuff );
				}
			}
			fclose(fpOut);
		}
		else
		{
			printf("出力ファイルのオープンに失敗しました\n");
			retCode = -1;
		
		}
		fclose(fp);
	}
	else
	{
		retCode = -1;
	
	}

	/* 終了 */
	return( retCode );	
}

