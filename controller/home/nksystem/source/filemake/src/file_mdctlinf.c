/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 *------------------------------------------------------------------------------
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モード切替時制御情報ファイル生成処理
 *
 * 機能：
 * 		MDCTLINF.csv ファイルからモード切替時制御情報ファイル（MDCTLINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_MDCTLINF_Create( struct st_linkPoint *ps_linkPoint, struct st_machineDef *ps_machineDef )
{
	FILE				*fp;									/* ファイルポインタ */
	int					fd_md;									/* ファイルディスクリプタ */
	int					fd_dat;									/* ファイルディスクリプタ */
	char				readBuff[4096];							/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];						/* CSVファイルパス */
	char				datFilePathMD[1024];					/* DATファイルパス */
	char				datFilePathDAT[1024];					/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];				/* エラーメッセージ */
	int					csvCount;								/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;							/* 戻り値 */
	struct st_sepStr	s_sepStr;								/* CSVデータ分割格納 */
	struct st_MDCTLINF	*ps_MDCTLINF;							/* モード切替時制御情報ポインタ */
	struct st_SVDATINF	*ps_SVDATINF;							/* 状態情報ポインタ */
	int					workData;								/* 数値変換用 */
	int					force;									/* 強制制御 */
	int					mKind;									/* 装置種別 */
	int					mNumb;									/* 装置番号 */
	int					svNumb;									/* 状態番号 */
	int					svVal;									/* 状態 */
	int					statFlag;								/* 実装/未実装 */
	int					setMode;								/* 制御モード */
	int					ctlNo;									/* 制御番号 */
	int					syoriCount = 0;							/* CSV処理カウンタ */
	int					csvOffset = 0;							/* CSV処理オフセット */
	int					svIfLink;								/* 状態情報リンク */
	int					oneShot;								/* ワンショット維持時間 */
	int					statFlagRes;							/* 応動監視実装/未実装 */
	int					soKindRes;								/* 応動所属装置種別 */
	int					soNumbRes;								/* 応動所属装置番号 */
	int					svNumbRes;								/* 応動装置内計測番号 */
	int					svIfLinkRes;							/* 状態情報リンク */
	int					svValRes;								/* 応動方向 */
	int					timeOutRes;								/* 応動時間 */
	int					statFlagNotCtl;							/* 制御不可SV条件 実装/未実装 */
	int					soKindNotCtl;							/* 制御不可SV条件 制御所属装置種別 */
	int					soNumbNotCtl;							/* 制御不可SV条件 制御所属装置番号 */
	int					svNumbNotCtl;							/* 制御不可SV条件 制御装置内計測番号 */
	int					svIfLinkNotCtl;							/* 制御不可SV条件 状態情報リンク情報 */
	int					svValNotCtl;							/* 制御不可SV条件 操作方向 */


	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathMD, PV_PATH_DAT );
	strcpy(datFilePathDAT, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_MDCTLINF );
	strcat(datFilePathMD, PV_FNAME_MDCTLINF );
	strcat(datFilePathDAT, PV_FNAME_SVDATINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePathMD, ".dat");
	strcat(datFilePathDAT, ".dat");

	/* DATファイル生成 */
	fd_md = open( datFilePathMD, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_md == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathMD );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_md, sizeof(struct st_MDCTLINF) * PV_CTL_MAX ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathMD );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_md );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_MDCTLINF = (struct st_MDCTLINF *)mmap( NULL, sizeof(struct st_MDCTLINF) * PV_CTL_MAX, PROT_READ | PROT_WRITE, MAP_SHARED, fd_md, 0 );
	if( ps_MDCTLINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathMD );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_md );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_MDCTLINF, 0, sizeof(struct st_MDCTLINF) * PV_CTL_MAX );

	/* 状態情報ファイル */

	/* DATファイルを開く */
	fd_dat = open( datFilePathDAT, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathDAT );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_md );
		munmap( ps_MDCTLINF, sizeof(struct st_MDCTLINF) * PV_CTL_MAX );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_SVDATINF = (struct st_SVDATINF *)mmap( NULL, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dat, 0 );
	if( ps_SVDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDAT );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_md );
		close( fd_dat );
		munmap( ps_MDCTLINF, sizeof(struct st_MDCTLINF) * PV_CTL_MAX );
		retCode = -1;
		return( retCode );
	}


	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			csvOffset = 0;
			/* CSV処理カウンタを＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（9文字以下）はスキップ */
			if( strlen( readBuff ) < 10) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );

			/* CSVデータ数のチェック */
			if( csvCount != 19 )
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 制御モードの取得 */
			setMode = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (setMode <= 0) && (setMode > PV_CTL_MAX) )
			{
				sprintf( errMsg, "CSV-Format Error (setMode Error). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 制御番号の取得 */
			ctlNo = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (ctlNo <= 0) && (ctlNo > PV_MAX_CTL_NUM) )
			{
				sprintf( errMsg, "CSV-Format Error (ctlNo Error). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 重複チェック */
			if( ps_MDCTLINF[setMode-1].s_ctlinfo[ctlNo-1].statFlag == 1 )
			{
				sprintf( errMsg, "CSV-Format Error (control Number Duplication). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 制御不可SV条件実装／未実装の取得 */
			statFlagNotCtl = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( statFlagNotCtl < 0 )
			{
				sprintf( errMsg, "CSV-Format Error (not cntrol statFlag). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 制御不可SV条件実装の場合 */
			if( statFlagNotCtl == 1 )
			{
				/* 制御不可SV条件の装置種別の取得 */
				soKindNotCtl = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( ( soKindNotCtl < PV_MKIND_PVSYS ) || ( soKindNotCtl > PV_MKIND_LAST ) )
				{
					sprintf( errMsg, "CSV-Format Error (not cntrol Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 制御不可SV条件装置番号の取得 */
				soNumbNotCtl = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (soNumbNotCtl < 1) || (soNumbNotCtl > ps_machineDef[soKindNotCtl].maxMNumb) )
				{
					sprintf( errMsg, "CSV-Format Error (not cntrol Machine Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 制御不可SV条件状態番号の取得 */
				svNumbNotCtl = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (svNumbNotCtl < 1) || (svNumbNotCtl > ps_machineDef[soKindNotCtl].maxSvNumb) )
				{
					sprintf( errMsg, "CSV-Format Error (not cntrol SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 制御対象の状態情報のリンクを取得 */
				svIfLinkNotCtl = ps_linkPoint[soKindNotCtl].svIfLink + ps_machineDef[soKindNotCtl].maxSvNumb * (soNumbNotCtl - 1) + svNumbNotCtl - 1;

				/* 制御不可SV条件対象の状態情報が未実装の場合 */
				if( ps_SVDATINF[svIfLinkNotCtl].statFlag == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Non-Exist not cntrol SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 制御不可SV条件操作方向の取得 */
				svValNotCtl = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (svValNotCtl != 1) && (svValNotCtl != 0 ) )
				{
					sprintf( errMsg, "CSV-Format Error (not cntrol SV Value). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
			}
			else
			{
				/* 制御不可SV条件装置種別の設定 */
				csvOffset++;
				soKindNotCtl = 0;
				/* 制御不可SV条件装置番号の設定 */
				csvOffset++;
				soNumbNotCtl = 0;
				/* 制御不可SV条件状態番号の設定 */
				csvOffset++;
				svNumbNotCtl = 0;
				/* 制御不可SV条件操作方向の設定 */
				csvOffset++;
				svValNotCtl = 0;
			}

			/* 強制制御の取得 */
			force = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (force != 0 ) && (force != 1 ) )
			{
				sprintf( errMsg, "CSV-Format Error (forceCtl). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置種別の取得 */
			mKind = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( ( mKind < PV_MKIND_PVSYS ) || ( mKind > PV_MKIND_LAST ) )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置番号の取得 */
			mNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (mNumb < 1) || (mNumb > ps_machineDef[mKind].maxMNumb) )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 状態番号の取得 */
			svNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (svNumb < 1) || (svNumb > ps_machineDef[mKind].maxSvNumb) )
			{
				sprintf( errMsg, "CSV-Format Error (SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 制御対象の状態情報のリンクを取得 */
			svIfLink = ps_linkPoint[mKind].svIfLink + ps_machineDef[mKind].maxSvNumb * (mNumb - 1) + svNumb - 1;

			/* 制御対象の状態情報が未実装の場合 */
			if( ps_SVDATINF[svIfLink].statFlag == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Non-Exist SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 操作方向の取得 */
			svVal = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (svVal != 1) && (svVal != 0 ) )
			{
				sprintf( errMsg, "CSV-Format Error (SV Value). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* ワンショット維持時間の取得 */
			oneShot = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( oneShot < 0 )
			{
				sprintf( errMsg, "CSV-Format Error (one shot). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 応動実装／未実装の取得 */
			statFlagRes = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( statFlagRes < 0 )
			{
				sprintf( errMsg, "CSV-Format Error (response statFlag). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 応動実装の場合 */
			if( statFlagRes == 1 )
			{
				/* 応動の装置種別の取得 */
				soKindRes = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( ( soKindRes < PV_MKIND_PVSYS ) || ( soKindRes > PV_MKIND_LAST ) )
				{
					sprintf( errMsg, "CSV-Format Error (response Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 応動装置番号の取得 */
				soNumbRes = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (soNumbRes < 1) || (soNumbRes > ps_machineDef[soKindRes].maxMNumb) )
				{
					sprintf( errMsg, "CSV-Format Error (response Machine Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 応動状態番号の取得 */
				svNumbRes = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (svNumbRes < 1) || (svNumbRes > ps_machineDef[soKindRes].maxSvNumb) )
				{
					sprintf( errMsg, "CSV-Format Error (response SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 制御対象の状態情報のリンクを取得 */
				svIfLinkRes = ps_linkPoint[soKindRes].svIfLink + ps_machineDef[soKindRes].maxSvNumb * (soNumbRes - 1) + svNumbRes - 1;

				/* 応動対象の状態情報が未実装の場合 */
				if( ps_SVDATINF[svIfLinkRes].statFlag == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Non-Exist response SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 応動操作方向の取得 */
				svValRes = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (svValRes != 1) && (svValRes != 0 ) )
				{
					sprintf( errMsg, "CSV-Format Error (response SV Value). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 応動時間の取得 */
				timeOutRes = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( timeOutRes < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (response Time Out). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
			}
			else
			{
				/* 応動装置種別の設定 */
				soKindRes = 0;
				/* 応動装置番号の設定 */
				soNumbRes = 0;
				/* 応動状態番号の設定 */
				svNumbRes = 0;
				/* 応動操作方向の設定 */
				svValRes = 0;
				/* 応動時間の設定 */
				timeOutRes = 0;
			}

			/* 実装／未実装の設定 */
			ps_MDCTLINF[setMode -1].statFlag = 1;
			/* 制御モードの設定 */
			ps_MDCTLINF[setMode -1].ctlMode = setMode;
			/* 制御数をインクメント */
			ps_MDCTLINF[setMode -1].ctlNum++;

			/********** 制御不可SV条件情報の設定 **********/
			/* 実装／未実装の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].statFlagNotCtl = statFlagNotCtl;
			/* 強制制御の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].soKindNotCtl = soKindNotCtl;
			/* 装置番号の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].soNumbNotCtl = soNumbNotCtl;
			/* 状態番号の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svNumbNotCtl = svNumbNotCtl;
			/* 状態情報リンクの設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svIfLinkNotCtl = svIfLinkNotCtl;
			/* 操作方向の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svValNotCtl = svValNotCtl;


			/********** 制御情報の設定 **********/
			/* 実装／未実装の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].statFlag = 1;
			/* 強制制御の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].forceCtl = force;
			/* 装置種別の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].soKindCtl = mKind;
			/* 装置番号の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].soNumbCtl = mNumb;
			/* 状態番号の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svNumbCtl = svNumb;
			/* 状態情報リンクの設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svIfLinkCtl = svIfLink;
			/* 操作方向の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svValCtl = svVal;
			/* リンク情報の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].oneShot = oneShot;


			/********** 応動情報の設定 **********/
			/* 実装／未実装の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].statFlagRes = statFlagRes;
			/* 応動装置種別の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].soKindRes = soKindRes;
			/* 応動装置番号の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].soNumbRes = soNumbRes;
			/* 応動状態番号の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svNumbRes = svNumbRes;
			/* 状態情報リンクの設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svIfLinkRes = svIfLinkRes;
			/* 応動操作方向の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].svValRes = svValRes;
			/* 応動時間の設定 */
			ps_MDCTLINF[setMode -1].s_ctlinfo[ctlNo -1].timeOutRes = timeOutRes;

		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}
	/* メモリマップを解除 */
	munmap( ps_MDCTLINF, sizeof(struct st_MDCTLINF) * PV_CTL_MAX);
	munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA);
	/* DATファイルをクローズ */
	close( fd_md );
	close( fd_dat );
	/* 終了 */
	return( retCode );	
}

