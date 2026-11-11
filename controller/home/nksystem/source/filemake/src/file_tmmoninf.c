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
 * 合計演算情報ファイル生成処理
 *
 * 機能：
 * 		TMMONINF.csv ファイルから合計演算情報ファイル（TMMONINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_TMMONINF_Create( struct st_linkPoint *ps_linkPoint, struct st_machineDef *ps_machineDef )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_mon;						/* ファイルディスクリプタ */
	int					fd_sv;						/* ファイルディスクリプタ */
	int					fd_tm;						/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePathMon[1024];		/* DATファイルパス */
	char				datFilePathSv[1024];		/* DATファイルパス */
	char				datFilePathTm[1024];		/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvOffset=0;
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_TMMONINF	*ps_TMMONINF;				/* 計測監視情報ポインタ */
	struct st_TMDATINF	*ps_TMDATINF;				/* 計測情報ポインタ */
	struct st_SVDATINF	*ps_SVDATINF;				/* 状態情報ポインタ */

	int					mKind;						/* 装置種別 */
	int					mNumb;						/* 装置番号 */
	int					svNumb;						/* 状態番号 */
	int					statFlag;					/* 実装/未実装 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	int					svIfLink;					/* 状態情報リンク */
	int					tmIfLink;					/* 計測情報リンク */
	int					tmMonIdx;					/* 合計演算リンク */
	int					soKindMoni;					/* 装置種別（演算元） */
	int					soNumbMoni;					/* 装置番号（演算元） */
	int					tmNumbMoni;					/* 状態番号（演算元） */
	int					svOffset;					/* 開始SV番号 */
	int					moniType;					/* 監視種別 */
	int					baseVal;					/* 基準値 */
	int					gosa;						/* 誤差範囲 */
	int					deadband;					/* 不感帯 */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathMon, PV_PATH_DAT );
	strcpy(datFilePathSv, PV_PATH_DAT );
	strcpy(datFilePathTm, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_TMMONINF );
	strcat(datFilePathMon, PV_FNAME_TMMONINF );
	strcat(datFilePathSv, PV_FNAME_SVDATINF );
	strcat(datFilePathTm, PV_FNAME_TMDATINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePathMon, ".dat");
	strcat(datFilePathSv, ".dat");
	strcat(datFilePathTm, ".dat");

	svOffset = ( PV_MAX_PVSYS_CTLSVPOS );

	/* DATファイル生成 */
	fd_mon = open( datFilePathMon, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_mon == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathMon );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_mon, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathMon );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_mon );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMMONINF = (struct st_TMMONINF *)mmap( NULL, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mon, 0 );
	if( ps_TMMONINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathMon );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_mon );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_TMMONINF, 0, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS );

	/* 状態情報ファイル */

	/* DATファイルを開く */
	fd_sv = open( datFilePathSv, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_sv == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathSv );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_TMMONINF, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS );
		close( fd_mon );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_SVDATINF = (struct st_SVDATINF *)mmap( NULL, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_sv, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathSv );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_TMMONINF, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS );
		close( fd_mon );
		close( fd_sv );
		retCode = -1;
		return( retCode );
	}

	/* DATファイルを開く */
	fd_tm = open( datFilePathTm, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_tm == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathTm );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );

		munmap( ps_TMMONINF, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS );
		munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA);
		close( fd_mon );
		close( fd_sv );

		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMDATINF = (struct st_TMDATINF *)mmap( NULL, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_tm, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathSv );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		munmap( ps_TMMONINF, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS );
		munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA);
		close( fd_mon );
		close( fd_sv );
		close( fd_tm );
		retCode = -1;
		return( retCode );
	}

	/* 計測監視情報の CSVファイルが存在しない場合 */
	if( access( csvFilePath, R_OK ) != 0 )
	{
		/* エラーメッセージの出力 */
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE" , errMsg );
		
		/* メモリマップを解除 */
		munmap( ps_TMMONINF, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS );
		munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA );
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
		/* DATファイルをクローズ */
		close( fd_mon );
		close( fd_sv );
		close( fd_tm );
		/* 終了 */
		return( 0 );
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

			/* CSVデータ数が不正の場合 */
			if( csvCount != 12 )
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA is Not 11). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置種別の取得 */
			mKind = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( mKind != PV_MKIND_PVSYS )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 装置番号の取得 */
			mNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if(mNumb != 1)
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 状態番号の取得 */
			svNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (svNumb <  ( svOffset + 1)) || (svNumb > ( svOffset + PV_MAX_PVSYS_MONSVPOS)) )
			{
				sprintf( errMsg, "CSV-Format Error (SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 実装/未実装の取得 */
			statFlag = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (statFlag != 0) && (statFlag != 1) )
			{
				sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 未実装の場合は次の情報を処理 */
			if( statFlag == 0 ) continue;


			/* リンク情報を取得 */
			svIfLink = ps_linkPoint[mKind].svIfLink + ps_machineDef[mKind].maxSvNumb * (mNumb - 1) + svNumb - 1;
			/* リンク情報を取得 */
			tmMonIdx = svNumb - svOffset -1;

			/* 装置種別の設定 */
			ps_TMMONINF[tmMonIdx].soKind   = mKind;
			/* 装置番号の設定 */
			ps_TMMONINF[tmMonIdx].soNumb   = mNumb;
			/* 状態番号の設定 */
			ps_TMMONINF[tmMonIdx].svNumb   = svNumb;
			/* 実装/未実装の設定 */
			ps_TMMONINF[tmMonIdx].statFlag = statFlag;
			/* リンク情報の設定 */
			ps_TMMONINF[tmMonIdx].svIfLink = svIfLink;

			/* 状態名称の取得 */
			if( strlen(s_sepStr.data[csvOffset]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 応動状態名称の設定 */
			strncpy( ps_TMMONINF[tmMonIdx].svName, s_sepStr.data[csvOffset], 72);
			csvOffset++;

			/* 状態情報ファイルに設定 */
			ps_SVDATINF[svIfLink].statFlag = statFlag;
			ps_SVDATINF[svIfLink].soKind   = mKind;
			ps_SVDATINF[svIfLink].soNumb   = mNumb;
			ps_SVDATINF[svIfLink].svNumb   = svNumb;
			strncpy( ps_SVDATINF[svIfLink].svName, ps_TMMONINF[tmMonIdx].svName, 72);



			/* 装置種別の取得 */
			soKindMoni = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( ( soKindMoni < PV_MKIND_PVSYS ) || ( soKindMoni > PV_MKIND_LAST ) )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置番号の取得 */
			soNumbMoni = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (soNumbMoni < 1) || (soNumbMoni > ps_machineDef[soKindMoni].maxMNumb) )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 計測番号の取得 */
			tmNumbMoni = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			/* 装置番号の範囲確認 */
			if( (tmNumbMoni < 1) || (tmNumbMoni > ps_machineDef[soKindMoni].maxTmNumb) )
			{
				sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 演算対象の計測情報のリンクを取得 */
			tmIfLink = ps_linkPoint[soKindMoni].tmIfLink + ps_machineDef[soKindMoni].maxTmNumb * (soNumbMoni - 1) + tmNumbMoni - 1;

			/* 演算元の計測情報が未実装の場合 */
			if( ps_TMDATINF[tmIfLink].statFlag == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Non-Exist TM Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 監視種別の取得 */
			moniType = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( ( moniType != PV_MONI_TYPE_RANGE ) && 
				( moniType != PV_MONI_TYPE_ABOVE ) && 
				( moniType != PV_MONI_TYPE_BELOW ) )
			{
				sprintf( errMsg, "CSV-Format Error (monitor Type). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 監視基準値の取得 */
			baseVal = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;

			/* 誤差範囲の取得 */
			gosa = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;

			/* 誤差範囲の取得 */
			deadband = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;


			/* 装置種別の設定 */
			ps_TMMONINF[tmMonIdx].soKindMoni = soKindMoni;
			/* 装置番号の設定 */
			ps_TMMONINF[tmMonIdx].soNumbMoni = soNumbMoni;
			/* 計測番号の設定 */
			ps_TMMONINF[tmMonIdx].tmNumbMoni = tmNumbMoni;
			/* リンク情報の設定 */
			ps_TMMONINF[tmMonIdx].tmIfLink = tmIfLink;
			/* 監視種別の設定 */
			ps_TMMONINF[tmMonIdx].moniType = moniType;
			/* 監視基準値の設定 */
			ps_TMMONINF[tmMonIdx].baseVal = baseVal;
			/* 誤差範囲の設定 */
			ps_TMMONINF[tmMonIdx].gosa = gosa;
			/* 誤差範囲の設定 */
			ps_TMMONINF[tmMonIdx].deadband = deadband;
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
	munmap( ps_TMMONINF, sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS);
	munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA);
	munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
	/* DATファイルをクローズ */
	close( fd_mon );
	close( fd_sv );
	close( fd_tm );
	/* 終了 */
	return( retCode );	
}

