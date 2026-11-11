/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260625] 2026.06.25
* ・通信回線情報ファイルの送信待ち時間上限を変更(500→30000)
 * [20251027] 2025.10.27
 * ・出力制御ユニット用ＰＬＣ（ModbusTCP）に対応
 * [20250421] 2025.04.21
 * ・Tesla社製蓄電池 Controls-V2に対応
 *
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
#include "pvserial.h"

#include "filemake.h"

/* モジュール内グローバル変数定義 */
struct st_typeName		s_g_protocolName[] = {		/* 通信プロトコル */
/*           +---------------------------------------- プロトコル名称                                               */
/*           |               +------------------------ 利用可能なデバイスタイプの最小値                             */
/*           |               |  +--------------------- 利用可能なデバイスタイプの最大値                             */
/*           |               |  |                       1:TTYデバイス / 2:LAN-485変換器 / 3:直接TCP接続             */
/*           |               |  |                       4:UDP接続(送信元ポートに返送) / 5:UDP接続(指定ポートに返送) */
/*           |               |  |                                                                                   */
	{ PV_COMPRO_SMA4       	, 3, 3 },				/* ＳＭＡ製パワーコンディショナ（Data Manager M） ModbusTCP通信 */
	{ PV_COMPRO_TAKEMULTIM2	, 1, 2 },				/* タケモトデンキ製マルチメーター（RS485:ModbusRTU）プロトコル */
	{ PV_COMPRO_TESLA1     	, 3, 3 },				/* TESLA製リチウムイオン電池（ModbusTCP通信方式） */
	{ PV_COMPRO_TESLA2     	, 3, 3 },				/* TESLA製リチウムイオン電池 Controls-v2（ModbusTCP通信方式） *//* [20250421] Add */
	{ PV_COMPRO_CEC1       	, 2, 2 },				/* 中央電子社製 接点伝送装置プロトコル（SMNP通信） */
	{ PV_COMPRO_NKIOPLC1   	, 3, 3 },				/* 収集装置外部Ｉ／Ｏ用ＰＬＣプロトコル */
	{ PV_COMPRO_HUAWEI2    	, 2, 2 },				/* ＨＵＡＷＥＩパワコン専用プロトコル（SmartLogger経由）（ModbusTCP通信方式） */
	{ PV_COMPRO_SANKEN2    	, 2, 2 },				/* サンケン電気パワコン専用プロトコル（SmartLogger経由）（ModbusTCP通信方式） */
	{ PV_COMPRO_MITSUMULTIM1, 1, 2 },				/* 三菱電機製EcoMonitor（RS485:ModbusRTU）プロトコル */
	{ PV_COMPRO_COMPOWAYF1  , 1, 2 },				/* オムロンＫＭ１専用プロトコル（Compoway/F） */
	{ PV_COMPRO_SCHNEIDER1  , 3, 3 },				/* シュナイダー製マルチメータ（ModbusTCP通信方式） *//* [20230529] Add */
	{ PV_COMPRO_SCHPLC1     , 3, 3 },				/* 出力制御ユニット用ＰＬＣ（ModbusTCP） *//* [20251027] Add */
	{ PV_COMPRO_PPMSMA1     , 3, 3 },				/* SMA製PPM（ModbusTCP） *//* [20251101] Add */
	{ PV_FILEMAKE_STOPPER   , 0, 0 }					/* ストッパー */
};

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信回線情報ファイル生成
 *
 * 機能：
 * 		COMLIINF.csv ファイルから通信回線情報ファイル（COMLIINF.dat）を生
 * 		成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_COMLIINF_Create( int *lineExist )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd;							/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePath[1024];			/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					loopCnt;					/* ループカウンタ */
	int					retCode = 0;				/* 戻り値 */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_COMLIINF	*ps_COMLIINF;				/* 通信回線情報ポインタ */
	int					lineNumb;					/* 通信回線番号 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSVファイル処理行数 */
	int					nonSerial = 0;				/* 非シリアルデバイス */
	char				setFlag[PV_MAX_COMLINE];	/* 設定済みフラグ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_COMLIINF );
	strcat(datFilePath, PV_FNAME_COMLIINF );
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
	if( ftruncate( fd, sizeof(struct st_COMLIINF) * PV_MAX_COMLINE) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_COMLIINF = (struct st_COMLIINF *)mmap( NULL, sizeof(struct st_COMLIINF) * PV_MAX_COMLINE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( ps_COMLIINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* 通信回線実装情報の初期化 */
	for( loopCnt = 0 ; loopCnt < PV_MAX_COMLINE ; loopCnt++ )
	{
		*(lineExist + loopCnt) = 0;
		setFlag[loopCnt] = 0;
	}

	/* ファイルの中身をALL0クリア */
	memset( ps_COMLIINF, 0, sizeof(struct st_COMLIINF) * PV_MAX_COMLINE );

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			/* 処理行数を＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（9文字以下）はスキップ */
			if( strlen( readBuff ) < 10) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );
			/* CSVデータ数が不正の場合 */
			if( csvCount != 13)
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 13). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 通信回線番号の取得 */
			lineNumb = atoi( s_sepStr.data[0] );
			if( (lineNumb < 1) || (lineNumb > PV_MAX_COMLINE) )
			{
				sprintf( errMsg, "CSV-Format Error (Line Number Error). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 実装/未実装の取得 */
			workData = atoi( s_sepStr.data[1] );
			/* 未実装の場合 */
			if( workData == 0 ) continue;
			/* 実装以外の場合 */
			if( workData != 1 )
			{
				sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] LINE[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_COMLIINF + lineNumb - 1)->statFlag = workData;

			/* 重複定義の確認 */
			if( setFlag[lineNumb - 1] == 1 )
			{
				sprintf( errMsg, "CSV-Format Error (Line Number Dupulication Error). File[%s] LINE[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			setFlag[lineNumb - 1] = 1;

			/* プロトコル名称の取得 */
			if( strlen(s_sepStr.data[2]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Unknown Protocol Name). File[%s] LINE[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* プロトコルが見つかるかストッパーまで繰り返す */
			for( loopCnt = 0 ; strncmp( s_g_protocolName[loopCnt].typeName, PV_FILEMAKE_STOPPER, 12 ) != 0 ; loopCnt++ )
			{
				/* プロトコルが一致した場合 */
				if( strncmp( s_g_protocolName[loopCnt].typeName, s_sepStr.data[2], 12 ) == 0 )
					break;
			}
			if( strncmp( s_g_protocolName[loopCnt].typeName, PV_FILEMAKE_STOPPER, 12 ) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Unknown Protocol Type).  File[%s] LINE[%d] DAT[3](%s)", csvFilePath, syoriCount, s_sepStr.data[2] );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( (ps_COMLIINF + lineNumb - 1)->protoName, s_sepStr.data[2], 12);
			/* デバイスタイプの取得 */
			workData = atoi( s_sepStr.data[3] );
			if( (workData != PV_STYPE_TTY) && (workData != PV_STYPE_TCP) && (workData != PV_STYPE_DIRTCP) &&
			    (workData != PV_STYPE_UDPDIR) && (workData != PV_STYPE_UDPOTH) )
			{
				sprintf( errMsg, "CSV-Format Error (Unknown Device Type). File[%s] LINE[%d] DAT[4]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_COMLIINF + lineNumb - 1)->deviceType = workData;
			/* デバイスタイプが通信プロトコルと一致しない場合 */
			if( ( workData < s_g_protocolName[loopCnt].minID ) || ( workData > s_g_protocolName[loopCnt].maxID ) )
			{
				sprintf( errMsg, "CSV-Format Error (Device Type Discrepancy). File[%s] LINE[%d] DAT[4]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* デバイスタイプがシリアルデバイスの場合 */
			if( workData == 1 )
				nonSerial = 0;
			else
				nonSerial = 1;

			/* ターゲット名称の取得 */
			if( strlen(s_sepStr.data[4]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Device Name is Empty). File[%s] LINE[%d] DAT[5]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( (ps_COMLIINF + lineNumb - 1)->targetName, s_sepStr.data[4], 20);
			/* ポート番号の取得 */
			workData = atoi( s_sepStr.data[5] );
			(ps_COMLIINF + lineNumb - 1)->portNumb = workData;

			/* ポート番号が必要なデバイスタイプでポート番号が未設定の場合 */
			if( (workData == 0) && 
			    ( ((ps_COMLIINF + lineNumb - 1)->deviceType == PV_STYPE_TCP) ||
			      ((ps_COMLIINF + lineNumb - 1)->deviceType == PV_STYPE_DIRTCP) ||
			      ((ps_COMLIINF + lineNumb - 1)->deviceType == PV_STYPE_UDPDIR) ||
			      ((ps_COMLIINF + lineNumb - 1)->deviceType == PV_STYPE_UDPOTH) ) )
			{
				sprintf( errMsg, "CSV-Format Error (Port Number is 0). File[%s] LINE[%d] DAT[6]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* ボーレートの取得 */
			workData = atoi( s_sepStr.data[6] );
			(ps_COMLIINF + lineNumb - 1)->baudRate = workData;
			/* ボーレートをＵＤＰ受信ポート番号として使用する場合にポート番号（ボーレート）が未設定の場合 */
			if( (workData == 0) && ( (ps_COMLIINF + lineNumb - 1)->deviceType == PV_STYPE_UDPOTH ) )
			{
				sprintf( errMsg, "CSV-Format Error (Baudrate [UDP Port] Number is 0). File[%s] LINE[%d] DAT[7]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* データサイズの取得 */
			workData = atoi( s_sepStr.data[7] );
			if( (workData != 7) && (workData != 8) )
			{
				if( !( ( workData == 0 ) && ( nonSerial == 1 ) ) )
				{
					sprintf( errMsg, "CSV-Format Error (Data Size Error). File[%s] LINE[%d] DAT[8]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
			}
			(ps_COMLIINF + lineNumb - 1)->dataSize = workData;
			/* パリティの取得 */
			workData = atoi( s_sepStr.data[8] );
			if( (workData != 0) && (workData != 1) && (workData != 2) )
			{
				sprintf( errMsg, "CSV-Format Error (Unknown Parity Type). File[%s] LINE[%d] DAT[9]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_COMLIINF + lineNumb - 1)->parity = workData;
			/* ストップビットの取得 */
			workData = atoi( s_sepStr.data[9] );
			if( (workData != 1) && (workData != 2) )
			{
				if( !( ( workData == 0 ) && ( nonSerial == 1 ) ) )
				{
					sprintf( errMsg, "CSV-Format Error (Unknown Stopbit). File[%s] LINE[%d] DAT[10]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
			}
			(ps_COMLIINF + lineNumb - 1)->stopBit = workData;
			/* 応答待ちタイマの取得 */
			workData = atoi( s_sepStr.data[10] );
			if( (workData < 0) || (workData > 1000) )
			{
				sprintf( errMsg, "CSV-Format Error (Receive Timer). File[%s] LINE[%d] DAT[11]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_COMLIINF + lineNumb - 1)->rcvTime = workData;
			/* 送信待ちタイマの取得 */
			workData = atoi( s_sepStr.data[11] );
			if( (workData < 0) || (workData > 30000) )	/* [20260625] 送信待ち時間上限変更(500→30000) */
			{
				sprintf( errMsg, "CSV-Format Error (Send Timer). File[%s] LINE[%d] DAT[12]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_COMLIINF + lineNumb - 1)->sndTime = workData;
			/* 収集周期の取得 */
			workData = atoi( s_sepStr.data[12] );
			if( (workData < 1) || (workData > 10000) )
			{
				sprintf( errMsg, "CSV-Format Error (Cycle Timer). File[%s] LINE[%d] DAT[13]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_COMLIINF + lineNumb - 1)->cycleTime = workData;

			/* 通信回線実装情報を設定 */
			*(lineExist + lineNumb - 1) = 1;

			/* 戻り値に回線情報ありを設定 */
			retCode = 1;
		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "File[%s] Open error", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = 1;
	}
	/* メモリマップを解除 */
	munmap( ps_COMLIINF, sizeof(struct st_COMLIINF) * PV_MAX_COMLINE);
	/* DATファイルをクローズ */
	close( fd );
	/* 終了 */
	return( retCode );	
}

