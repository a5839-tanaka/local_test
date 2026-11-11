/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * バーチャルプラントシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
 */
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス情報ファイル生成処理
 *
 * 機能：
 * 		IOTCNINF.csv ファイルから通信ステータス情報ファイル（IOTCNINF.dat）を生
 * 		成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_IOTCNINF_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_Dat;						/* ファイルディスクリプタ（通信ステータス情報） */
	int					fd_Msk;						/* ファイルディスクリプタ（通信ステータス状変マスク情報） */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePathDat[1024];		/* DATファイルパス（通信ステータス情報） */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					csvOffset;					/* CSVデータオフセット */
	int					resorceType;				/* リソース種別 */
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_IOTCNINF	*ps_IOTCNINF;				/* 通信ステータス情報ポインタ */
	int					portNumb;					/* ポート番号 */
	int					sndCycle;					/* 送信周期 */
	int					sndRetry;					/* リトライ回数 */
	int					stockHour;					/* データ保存時間 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathDat, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_IOTCNINF );
	strcat(datFilePathDat, PV_FNAME_IOTCNINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePathDat, ".dat");

	/* DATファイル生成 */
	fd_Dat = open( datFilePathDat, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_Dat, sizeof(struct st_IOTCNINF) * PV_MAX_STPOS ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_IOTCNINF = (struct st_IOTCNINF *)mmap( NULL, sizeof(struct st_IOTCNINF) * PV_MAX_STPOS, PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_IOTCNINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_IOTCNINF, 0, sizeof(struct st_IOTCNINF));

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
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
			if( csvCount != 9)
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 8). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* リソース種別の取得 */
			resorceType = atoi( s_sepStr.data[csvOffset] );
			if( ( resorceType < AWS_RESORCE_THERMAL_POWER ) || ( resorceType > AWS_RESORCE_MAX ) )
			{
				sprintf( errMsg, "CSV-Format Error (resorceType(%d) is range over). File[%s] Line[%d] DAT[%d]", resorceType, csvFilePath, syoriCount, csvOffset+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error resorceType", csvFilePath );
				retCode = -1;
				break;
			}
			csvOffset++;

			/* ホストアドレスの取得 */
			if( ( strlen(s_sepStr.data[csvOffset]) == 0 ) || (strlen(s_sepStr.data[csvOffset]) > 256 ) )
			{
				sprintf( errMsg, "CSV-Format Error (hostAddress is Empty or length over 256). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error hostAddress", csvFilePath );
				retCode = -1;
				break;
			}
			strncpy( ps_IOTCNINF->hostAddress, s_sepStr.data[csvOffset], 256);
			csvOffset++;

			/* ポート番号の取得 */
			portNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( portNumb < 1)
			{
				sprintf( errMsg, "CSV-Format Error (portNumb). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error portNumb", csvFilePath );
				retCode = -1;
				break;
			}


			/* デバイスIDの取得 */
			if( strlen(s_sepStr.data[csvOffset]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (deviceId is Empty). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error deviceId", csvFilePath );
				retCode = -1;
				break;
			}
			strncpy( ps_IOTCNINF->deviceId, s_sepStr.data[csvOffset], 32);
			csvOffset++;

			/* 受信トピック名の取得 */
			if( ( strlen(s_sepStr.data[csvOffset]) == 0 ) ||
			    ( strlen(s_sepStr.data[csvOffset]) >= 64 ) )
			{
				sprintf( errMsg, "CSV-Format Error (rcvTopicName is Empty). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error rcvTopicName", csvFilePath );
				retCode = -1;
				break;
			}
			strncpy( ps_IOTCNINF->rcvTopicName, s_sepStr.data[csvOffset], 64);
			csvOffset++;

			/* リトライ回数の取得 */
			sndRetry = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (sndRetry < 1) || (sndRetry > 10) )
			{
				sprintf( errMsg, "CSV-Format Error (sndRetry). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error sndRetry", csvFilePath );
				retCode = -1;
				break;
			}

			/* 計測・状態情報送信トピック名の取得 */
			memset( ps_IOTCNINF->sndTopicName, 0x00, sizeof(ps_IOTCNINF->sndTopicName) );
			if( strlen(s_sepStr.data[csvOffset]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Warning (sndTopicName is Empty). File[%s]", csvFilePath );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				csvOffset++;
				csvOffset++;
				csvOffset++;
			}
			else if ( strlen(s_sepStr.data[csvOffset]) >= 64 )
			{
				sprintf( errMsg, "CSV-Format Error (sndTopicName is Length Over). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error sndTopicName", csvFilePath );
				retCode = -1;
				break;
			}
			else
			{
				strncpy( ps_IOTCNINF->sndTopicName, s_sepStr.data[csvOffset], 64);
				csvOffset++;

				/* 送信周期の取得 */
				sndCycle = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (sndCycle < 1) || (sndCycle > 60) || (( 60 % sndCycle ) != 0 ))
				{
					sprintf( errMsg, "CSV-Format Error (sndCycle). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					printf("FILEMAKE : InputFile[%s] CSV-Data error sndCycle", csvFilePath );
					retCode = -1;
					break;
				}

				/* ファイル保存時間の取得 */
				stockHour = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (stockHour < 1 ) || ( stockHour > PV_MAX_FILE_STOCK_HOUR ) )
				{
					sprintf( errMsg, "CSV-Format Error (Save Hour). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					printf("FILEMAKE : InputFile[%s] CSV-Data error Save Hour", csvFilePath );
					retCode = -1;
					break;
				}
			}

#if 0
			printf("IOTCNINF : portNumb:%d, hostAddress:%s\n", 
				ps_IOTCNINF->portNumb,
				ps_IOTCNINF->hostAddress);

			printf("IOTCNINF : deviceId:%s, rcvTopicName:%s, sndTopicName:%s, sndRetry:%d, stockHour:%d\n", 
				ps_IOTCNINF->deviceId,
				ps_IOTCNINF->rcvTopicName,
				ps_IOTCNINF->sndTopicName,
				ps_IOTCNINF->sndRetry,
				ps_IOTCNINF->stockHour);
#endif

			ps_IOTCNINF->statFlag = 1;
			ps_IOTCNINF->portNumb = portNumb;
			ps_IOTCNINF->sndRetry = sndRetry;
			ps_IOTCNINF->sndCycle = sndCycle;
			ps_IOTCNINF->stockHour = stockHour;
			ps_IOTCNINF->resorceType = resorceType;

		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}
	/* メモリマップを解除 */
	munmap( ps_IOTCNINF, sizeof(struct st_IOTCNINF));
	/* DATファイルをクローズ */
	close( fd_Dat );
	/* 終了 */
	return( retCode );	
}

