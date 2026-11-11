/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20210617] 2021.06.17
 * ・太陽光(高圧)スケジュール機能 北陸電力対応(電力会社毎の処理分岐)
 * [20210330] 2021.03.30
 * ・特高CDT出力制御対応(東北電力)
 * ・スケジュール未登録時の出力制御値(%)を追加
 * [20200814] 2020.08.14
 * ・特高CDT出力制御対応(九州電力)
 * [20190627] 2019.06.27
 * ・NTPサーバのFQDNの文字数の上限を変更
 * [20161201] 2016.12.01
 * ・出力制御機能に対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
 */
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"

/* 内部関数定義 */
int	func_cmn_string_check( char *, int );
int	func_cmn_get_mac_address( char *strPoint );

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
int	func_PGCONINF_Create( )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd;							/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					strCheck;					/* 文字列確認 */
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	struct st_PGCONINF	*ps_PGCONINF;				/* ファイルポインタ */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	char				*ptr=NULL;					/* 文字列ポインタ */
	char				skipFlg=0;					/* スキップフラグ */
	char				powerGrid[4];				/* 電力会社識別子 *//* [20210330] Add */
	int					fileSize = 0;				/* ファイルサイズ */
	int					dataCount;					/* データカウンタ */
	int					outCtlType;					/* 出力制御種別 */

	memset( &s_sepStr, 0, sizeof(struct st_sepStr));

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_PGCONINF );
	strcat(datFilePath, PV_FNAME_PGCONINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePath, ".dat");

	fileSize = sizeof(struct st_PGCONINF) * PV_SCH_TYPE_MAX;

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
	if( ftruncate( fd, fileSize ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_PGCONINF = (struct st_PGCONINF *)mmap( NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_PGCONINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの中身をALL0クリア */
	memset( ps_PGCONINF, 0, fileSize );

	/* 電力会社向け情報の CSVファイルが存在しない場合 */
	if( access( csvFilePath, R_OK ) != 0 )
	{
		/* エラーメッセージの出力 */
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE" , errMsg );
		
		/* メモリマップを解除 */
		munmap( ps_PGCONINF, fileSize );
		/* DATファイルをクローズ */
		close( fd );
		/* 終了 */
		return( retCode );
	}

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			dataCount=0;
			/* CSV処理カウンタ＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（発電所ID+MACアドレス+カラム数 文字以下）はスキップ */
			if( strlen( readBuff ) < (26+12+9)) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );

			/* CSVデータ数が不正の場合 */
			if( csvCount != 9 )
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 9). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 出力制御種別を取得 */
			workData = atol( s_sepStr.data[dataCount] );
			if( (workData < PV_SCH_TYPE_BATTERY ) || ( workData >= PV_SCH_TYPE_MAX ) )
			{
				sprintf( errMsg, "CSV-Format Error (out control type Unknown(%d)) . File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			outCtlType = workData;
			ps_PGCONINF[outCtlType].outCtlType = workData;
			dataCount++;


			/* 発電所ＩＤ名称の取得 */
			if( strlen(s_sepStr.data[dataCount]) != 26 )
			{
				sprintf( errMsg, "CSV-Format Error (Power Plant ID Name Error). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 数値以外の文字が指定されている場合 */
			strCheck = func_cmn_string_check( s_sepStr.data[dataCount], 27 );
			if( ( strCheck == -1 ) || (strCheck != 0x0001) )
			{
				sprintf( errMsg, "CSV-Format Error (Power Plant ID exist no Number ) . File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			else
			{
				/* 電力会社識別子をコピー */
				memset(powerGrid, 0, sizeof(powerGrid) );
				memcpy(powerGrid, s_sepStr.data[dataCount], 2 );

				/* 電力会社情報を取得 */
				workData = atol( powerGrid );
				if( (workData < PV_SCH_POWERGRID_MIN ) || ( workData > PV_SCH_POWERGRID_MAX ) )
				{
					sprintf( errMsg, "CSV-Format Error (Power Grid Kind Unknown(%02d)) . File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, dataCount+1 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				ps_PGCONINF[outCtlType].powerGrid = workData;
			}
			strncpy( ps_PGCONINF[outCtlType].plantId, s_sepStr.data[dataCount], 52);
			dataCount++;

			/* ＭＡＣアドレス名称の取得 */
			if( strlen(s_sepStr.data[dataCount]) != 12 )
			{
				sprintf( errMsg, "CSV-Format Error (Mac Address Error). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 数値、16進の大文字以外の文字が指定されている場合 */
			strCheck = func_cmn_string_check( s_sepStr.data[dataCount], 13 );
			if( ( strCheck == -1 ) || (strCheck & ~(int)0x0011) )
			{
				sprintf( errMsg, "CSV-Format Error (MAC address is not hexadecimal or Characters is not uppercase ) . File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PGCONINF[outCtlType].macAddr, s_sepStr.data[dataCount], 13);
			dataCount++;

			strCheck = func_cmn_get_mac_address(ps_PGCONINF[outCtlType].macAddr);
			if( strCheck == -1 )
			{
				sprintf( errMsg, "CSV-Format Error (Get MAC address Error ) . File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* スケジュール情報配信サーバＵＲＬ */
			if( strlen(s_sepStr.data[dataCount]) > 127 )
			{
				sprintf( errMsg, "CSV-Format Error (URL) Length Long. File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( ps_PGCONINF[outCtlType].ipAddr, s_sepStr.data[dataCount], 128);
			dataCount++;

			/* スケジュール情報配信サーバＳＳＨポート番号の取得 */
			workData = atol( s_sepStr.data[dataCount] );
			if( workData == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (SSH Port). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF[outCtlType].portNumb = workData;
			dataCount++;

			/* ＮＴＰサーバＩＰアドレスの取得（orホスト名） */
			if( strlen(s_sepStr.data[dataCount]) >= 128 )
			{
				sprintf( errMsg, "CSV-Format Error (IP Address or HostName) Length is Long. File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			else if( strlen(s_sepStr.data[dataCount]) > 0 )
			{
				/* 数値・英文字・ハイフン・アンダースコア・ピリオド以外の文字が指定されている場合 */
				strCheck = func_cmn_string_check( s_sepStr.data[dataCount], 128 );
				if( strCheck == -1 )
				{
					sprintf( errMsg, "CSV-Format Error (IP Address) Unknown Charactor. File[%s] Line[%d] DAT[%d]=[%s]", csvFilePath, syoriCount, dataCount+1, s_sepStr.data[4] );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* IPアドレス指定（数値とピリオドのみ）で文字列長が16文字以上の場合 */
				if( ( strCheck == ((0x0001|0x0002)) ) && ( strlen(s_sepStr.data[dataCount]) > 15 ) )
				{
					sprintf( errMsg, "CSV-Format Error Length is Long. File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				strncpy( ps_PGCONINF[outCtlType].ntpIpAddr, s_sepStr.data[dataCount], 128);
			}
			else
			{
				sprintf( errMsg, "CSV-Format Error (IP Address or HostName) Length is short. File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			dataCount++;

			/* ＮＴＰサーバＳＳＨポート番号の取得 */
			workData = atol( s_sepStr.data[dataCount] );
			if( workData == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (SSH Port). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF[outCtlType].ntpPortNumb = workData;
			dataCount++;

			/* 出力変化時間の取得 */
			workData = atol( s_sepStr.data[dataCount] );
			if( (workData < 5 ) || ( workData > 10 ) )
			{
				sprintf( errMsg, "CSV-Format Error (Outoput Change Time). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF[outCtlType].chgTime = workData;
			dataCount++;

			/* スケジュール未登録時の出力制御値の取得 */
			workData = atol( s_sepStr.data[dataCount] );
			if( (workData < 0 ) || ( workData > 100 ) )
			{
				sprintf( errMsg, "CSV-Format Error (schdule none percent). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, dataCount+1 );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_PGCONINF[outCtlType].schNonPer = workData;
			dataCount++;

			ps_PGCONINF[outCtlType].statFlag = 1;

			retCode = 1;
		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		/* DATファイルが存在する場合 */
		if( access( datFilePath, R_OK ) == 0 )
		{
			unlink( datFilePath );
		}
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}

	/* メモリマップを解除 */
	munmap( ps_PGCONINF, fileSize );
	/* DATファイルをクローズ */
	close( fd );
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
int	func_cmn_string_check( char *strPoint, int strlenMax )
{
	int					chkLength;					/* データ確認長 */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	/* 指定ポインタの文字列長を確認する */
	if( ( chkLength = strnlen( strPoint, strlenMax+1 ) ) >= strlenMax )
	{
		sprintf( errMsg, "CSV-Format Error (IP Address or HostName) Length Over (%d).", chkLength );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		return( -1 );
	}

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
			sprintf( errMsg, "CSV-Format Error (IP Address) Unknown Charactor[%c].", (strPoint + loopCnt) );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
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

/* [CENTOS] Chg St */
	/* コマンドを実行してntpqコマンドの結果をパイプから取得 */
	if( (fp = popen( "/sbin/ip a", "r" )) == NULL )
	{
		commfunc_syslog_msgset( "FILEMAKE", "ERROR: ifconfig Command Error" );
		retCode = -1;
	}
	else
	{
		retCode = -1;
		/* 最初の１行を取り出し */
		while( fgets( checkData, 512, fp ) != NULL )
		{
		    if ((ptr = strstr(checkData, "ether")) != NULL )
			{
				ptr+=6;
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
				retCode = 0;
				break;
			}
		}
		if( retCode != 0 )
		{
			commfunc_syslog_msgset( "FILEMAKE", "ERROR: ifconfig Command [HWaddr] is Not Found" );
		}
/* [CENTOS] Chg Ed */
		/* パイプを閉じる */
		pclose( fp );
	}
	return retCode;
}

