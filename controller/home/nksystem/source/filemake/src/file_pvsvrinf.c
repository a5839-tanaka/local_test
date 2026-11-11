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
 * MicroGridシステム
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

int	func_string_check( char * );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モニタリングサーバ情報ファイル生成処理
 *
 * 機能：
 * 		PVSVRINF.csv ファイルからモニタリングサーバ情報ファイル（PVSVRINF.dat）
 * 		を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_PVSVRINF_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd;							/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */

	int					saveHour = 1;				/* ファイル保存時間（初期値） */
	int					svrKind = 0;				/* サーバ情報種別 */
	int					strCheck;					/* 文字列確認 */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_PVSVRINF	*ps_PVSVRINF;				/* ファイルポインタ */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_PVSVRINF );
	strcat(datFilePath, PV_FNAME_PVSVRINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePath, ".dat");

	/* DATファイル生成 */
	fd = open( datFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd, sizeof(struct st_PVSVRINF) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PVSVRINF = (struct st_PVSVRINF *)mmap( NULL, sizeof(struct st_PVSVRINF), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_PVSVRINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの中身をALL0クリア */
	memset( ps_PVSVRINF, 0, sizeof(struct st_PVSVRINF) );

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
			if( csvCount == 1)
			{
				/* ファイル保存時間の取得 */
				workData = atol( s_sepStr.data[0] );
				if( (workData < 1 ) || ( workData > PV_MAX_FILE_STOCK_HOUR ) )
				{
					sprintf( errMsg, "CSV-Format Error (Save Hour). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				ps_PVSVRINF->stockHour = workData;
				retCode = 1;
				continue;
			}
			/* CSVデータ数が不正の場合 */
			if( csvCount != 7)
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 7). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* サーバ情報種別の取得 */
			workData = atol( s_sepStr.data[0] );
			if( (workData != 1 ) && ( workData != 2 ) )
			{
				sprintf( errMsg, "CSV-Format Error (Server Number). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			svrKind = workData;
			/* MicroGridシステムサーバＩＰアドレスの取得（orホスト名） */
			if( ( strlen(s_sepStr.data[1]) > 31 ) || ( strlen(s_sepStr.data[1]) < 7 ) )
			{
				sprintf( errMsg, "CSV-Format Error (IP Address) Length Short or Long. File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 数値・英文字・ハイフン・アンダースコア・ピリオド以外の文字が指定されている場合 */
			strCheck = func_string_check( s_sepStr.data[1] );
			if( strCheck == -1 )
			{
				sprintf( errMsg, "CSV-Format Error (IP Address) Unknown Charactor. File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* IPアドレス指定（数値とピリオドのみ）で文字列長が16文字以上の場合 */
			if( ( strCheck == 1 ) && ( strlen(s_sepStr.data[1]) > 15 ) )
			{
				sprintf( errMsg, "CSV-Format Error (IP Address) Length Short or Long. File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PVSVRINF->s_svrinfo[svrKind - 1].ipAddr, s_sepStr.data[1], 32);
			/* MicroGridシステムサーバＳＳＨポート番号の取得 */
			workData = atol( s_sepStr.data[2] );
			if( workData == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (SSH Port). File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PVSVRINF->s_svrinfo[svrKind - 1].portNumb = workData;
			/* MicroGridシステムサーバログインユーザ名の取得 */
			if( ( strlen(s_sepStr.data[3]) > 20 ) || ( strlen(s_sepStr.data[3]) < 1 ) )
			{
				sprintf( errMsg, "CSV-Format Error (User Name). File[%s] Line[%d] DAT[4]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PVSVRINF->s_svrinfo[svrKind - 1].loginName, s_sepStr.data[3], 20);
			/* MicroGridシステムサーバ送信先パスの取得 */
			if( ( strlen(s_sepStr.data[4]) > 127 ) || ( strlen(s_sepStr.data[4]) < 1 ) )
			{
				sprintf( errMsg, "CSV-Format Error (Data Send Path). File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PVSVRINF->s_svrinfo[svrKind - 1].sendPath, s_sepStr.data[4], 127);
			/* MicroGridシステムサーバプログラムアップデート情報取得パスの取得 */
			if( ( strlen(s_sepStr.data[5]) > 127 ) || ( strlen(s_sepStr.data[5]) < 1 ) )
			{
				sprintf( errMsg, "CSV-Format Error (Program Update Path). File[%s] Line[%d] DAT[6]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PVSVRINF->s_svrinfo[svrKind - 1].procPath, s_sepStr.data[5], 127);
			/* MicroGridシステムサーバデータアップデート情報取得パスの取得 */
			if( ( strlen(s_sepStr.data[6]) > 127 ) || ( strlen(s_sepStr.data[6]) < 1 ) )
			{
				sprintf( errMsg, "CSV-Format Error (Data Update Path). File[%s] Line[%d] DAT[7]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PVSVRINF->s_svrinfo[svrKind - 1].dataPath, s_sepStr.data[6], 127);

			retCode = 1;
		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "File[%s] Open error", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
	}
	/* メモリマップを解除 */
	munmap( ps_PVSVRINF, sizeof(struct st_PVSVRINF) );
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モニタリングサーバ情報ファイル生成処理
 *
 * 機能：
 * 		引数 strPointer で指定されたアドレスに格納されている文字列を確認し、内
 *		容毎に以下の値を返す。
 * 戻り値：
 *		 1：数字,ピリオドのみで構成されている場合
 *		 2：数字,英文字,ハイフン,アンダースコア,ピリオドで構成されている場合
 *		-1：上記以外の文字（空白、記号、タブなど）が含まれている場合
 *		    または文字列長が33文字以上の場合
 */
int	func_string_check( char *strPoint )
{
	int					chkLength;					/* データ確認長 */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */

	/* 指定ポインタの文字列長を確認する */
	if( ( chkLength = strnlen( strPoint, 33 ) ) >= 32 )
		return( -1 );

	/* 文字列の最後まで確認する */
	for( loopCnt = 0 ; loopCnt < chkLength ; loopCnt++ )
	{
		/* ハイフンまたはアンダースコアの場合 */
		if( (*(strPoint + loopCnt) == '-') || (*(strPoint + loopCnt) == '_') )
		{
			retCode = 2;
		}
		/* アルファベット（小文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'a') && (*(strPoint + loopCnt) <= 'z') )
		{
			retCode = 2;
		}
		/* アルファベット（大文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'A') && (*(strPoint + loopCnt) <= 'Z') )
		{
			retCode = 2;
		}
		/* 数値またはピリオドの場合 */
		else if( ( (*(strPoint + loopCnt) >= '0') && (*(strPoint + loopCnt) <= '9') ) || ((*(strPoint + loopCnt) == '.')) )
		{
			if( retCode != 2 ) retCode = 1;
		}
		else
		{
			return( -1 );
		}
	}
	return( retCode );
}
