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


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態情報ファイル および 装置状態情報状変マスクファイル 生成処理
 *
 * 機能：
 * 		SVDATINF.csv ファイルから装置状態情報ファイル（SVDATINF.dat）を生成する。
 * 		SVDATINF.csv ファイルから装置状態状変マスクファイル（SVMSKINF.dat）を生
 * 		成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_SVDATINF_Create( struct st_linkPoint *ps_linkPoint, struct st_machineDef *ps_machineDef )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_Dat;						/* ファイルディスクリプタ（装置状態情報） */
	int					fd_Msk;						/* ファイルディスクリプタ（装置状態状変マスク情報） */
	int					fd_Hst;						/* ファイルディスクリプタ（装置状態履歴情報） */
	char				readBuff[1024];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[512];			/* CSVファイルパス */
	char				datFilePathDat[512];		/* DATファイルパス（装置状態情報） */
	char				datFilePathMsk[512];		/* DATファイルパス（装置状態状変マスク情報） */
	char				datFilePathHst[512];		/* DATファイルパス（装置状態履歴情報） */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_SVDATINF	*ps_SVDATINF;				/* 装置状態情報ポインタ */
	struct st_SVMSKINF	*ps_SVMSKINF;				/* 装置状態状変マスク情報ポインタ */
	struct st_SVRECINF	*ps_SVRECINF;				/* 装置状態履歴情報ポインタ */
	int					mKind;						/* 装置種別 */
	int					mNumb;						/* 装置番号 */
	int					svNumb;						/* 計測番号 */
	int					workData;					/* 数値変換用 */

	int					maxSvNumb;					/* 最大装置状態数 */
	int					maxMNumb;					/* 最大装置数 */
	int					linkBaseSvIf;				/* 装置状態情報基本リンク */
	int					linkBaseSvSt;				/* 装置状態情報基本リンク */
	int					svIfLink;					/* 装置状態情報リンク */
	int					svStLink;					/* 装置状態情報リンク */
	int					svStBit;					/* オフセット */
	int					svStOffset;					/* オフセット */
	int					storeNumb;					/* 蓄積番号 */
	char				setFlag[PV_MAX_SV_HISTORY];	/* 設定済みフラグ */

	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathDat, PV_PATH_DAT );
	strcpy(datFilePathMsk, PV_PATH_DAT );
	strcpy(datFilePathHst, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_SVDATINF );
	strcat(datFilePathDat, PV_FNAME_SVDATINF );
	strcat(datFilePathMsk, PV_FNAME_SVMSKINF );
	strcat(datFilePathHst, PV_FNAME_SVRECINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePathDat, ".dat");
	strcat(datFilePathMsk, ".dat");
	strcat(datFilePathHst, ".dat");


	/* DATファイル生成 */
	fd_Dat = open( datFilePathDat, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	fd_Msk = open( datFilePathMsk, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Msk == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathMsk );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	fd_Hst = open( datFilePathHst, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_Hst == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathHst );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの長さを調整 */
	if( ftruncate( fd_Dat, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		close( fd_Msk );
		close( fd_Hst );
		retCode = -1;
		return( retCode );
	}
	if( ftruncate( fd_Msk, sizeof(struct st_SVMSKINF) * ((PV_MAX_SVDATA-1) / 8 + 1) ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathMsk );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		close( fd_Msk );
		close( fd_Hst );
		retCode = -1;
		return( retCode );
	}
	if( ftruncate( fd_Hst, sizeof(struct st_SVRECINF) * PV_MAX_SV_HISTORY ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathHst );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		close( fd_Msk );
		close( fd_Hst );
		retCode = -1;
		return( retCode );
	}


	/* ファイルをメモリマップ */
	ps_SVDATINF = (struct st_SVDATINF *)mmap( NULL, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_Dat, 0 );
	if( ps_SVDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDat );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		close( fd_Msk );
		close( fd_Hst );
		retCode = -1;
		return( retCode );
	}
	ps_SVMSKINF = (struct st_SVMSKINF *)mmap( NULL, sizeof(struct st_SVMSKINF) * ((PV_MAX_SVDATA-1) / 8 + 1), PROT_READ | PROT_WRITE, MAP_SHARED, fd_Msk, 0 );
	if( ps_SVMSKINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathMsk );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		close( fd_Msk );
		close( fd_Hst );
		retCode = -1;
		return( retCode );
	}
	ps_SVRECINF = (struct st_SVRECINF *)mmap( NULL, sizeof(struct st_SVRECINF) * PV_MAX_SV_HISTORY, PROT_READ | PROT_WRITE, MAP_SHARED, fd_Hst, 0 );
	if( ps_SVRECINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathHst );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_Dat );
		close( fd_Msk );
		close( fd_Hst );
		retCode = -1;
		return( retCode );
	}


	/* ファイルの内容を0クリア */
	memset( ps_SVDATINF, 0, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA );
	/* ファイルの内容を0xFFクリア */
	memset( ps_SVMSKINF, 0xFF, sizeof(struct st_SVMSKINF) * ((PV_MAX_SVDATA-1) / 8 + 1) );
	/* ファイルの内容を0クリア */
	memset( ps_SVRECINF, 0, sizeof(struct st_SVRECINF) * PV_MAX_SV_HISTORY );

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 512, fp) != NULL )
		{
			/* CSV処理カウンタ＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（6文字以下）はスキップ */
			if( strlen( readBuff ) < 6) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );
			/* CSVデータ数が不正の場合 */
			if( csvCount != 8)	/* [20230529] Chg */
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 8). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 装置種別の取得 */
			mKind = atoi( s_sepStr.data[0] );
			if( (mKind < 0) || (mKind > PV_MKIND_LAST) )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 装置が実装されていない場合はスキップ */
			if( ps_linkPoint[mKind].statCnt == 0 ) continue;
			/* 装置番号の取得 */
			mNumb = atoi( s_sepStr.data[1] );
			if(mNumb < 1)
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 状態番号の取得 */
			svNumb = atoi( s_sepStr.data[2] );
			if( (svNumb < 1) || (svNumb > PV_MAX_SVDATA) )
			{
				sprintf( errMsg, "CSV-Format Error (POS Number). File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 最大情報の取得 */
			maxMNumb     = ps_machineDef[mKind].maxMNumb;
			maxSvNumb    = ps_machineDef[mKind].maxSvNumb;
			svStOffset   = (ps_machineDef[mKind].maxSvNumb - 1) / 8 + 1;
			linkBaseSvIf = ps_linkPoint[mKind].svIfLink + ps_machineDef[mKind].maxSvNumb * (mNumb - 1);
			linkBaseSvSt = ps_linkPoint[mKind].svStLink + svStOffset * (mNumb - 1);

			/* 装置番号が最大番号を超過した場合 */
			if( mNumb > maxMNumb )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 状態番号が最大番号を超過した場合 */
			if( svNumb > maxSvNumb )
			{
				sprintf( errMsg, "CSV-Format Error (POS Number). File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置が未実装の場合 */
			if( ps_linkPoint[mKind].statFlag == 0 )
				continue;

			/* 装置状態情報の格納位置取得 */
			svIfLink = linkBaseSvIf + svNumb - 1;
			/* 装置状態状変マスク情報の格納位置取得 */
			svStLink = linkBaseSvSt + (svNumb - 1) / 8;
			svStBit  = (svNumb - 1) % 8;

			/* 実装/未実装の取得 */
			workData = atoi( s_sepStr.data[3] );
			if( (workData != 0) && (workData != 1) )
			{
				sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] Line[%d] DAT[4]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_SVDATINF + svIfLink)->statFlag = workData;
			/* 所属装置種別の取得 */
			(ps_SVDATINF + svIfLink)->soKind = mKind;
			/* 所属装置番号の取得 */
			(ps_SVDATINF + svIfLink)->soNumb = mNumb;
			/* 装置内装置状態番号の取得 */
			(ps_SVDATINF + svIfLink)->svNumb = svNumb;
			/* 状変種別の取得 */
			workData = atoi( s_sepStr.data[4] );
			if( (workData != PV_SV_KIND_NORMAL) && (workData != PV_SV_KIND_JOMASK) )
			{
				sprintf( errMsg, "CSV-Format Error (SV Mask). File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_SVDATINF + svIfLink)->svKind = workData;

			/* 状変マスクの設定 */
			if( ((ps_SVDATINF + svIfLink)->statFlag == 1) && (workData == PV_SV_KIND_NORMAL) )
			{
				(ps_SVMSKINF + svStLink)->svMsk &= ~((unsigned char)0x80 >> svStBit);
			}

			/* 装置状態名称の取得 */
			if( strlen(s_sepStr.data[5]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] Line[%d] DAT[6]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( (ps_SVDATINF + svIfLink)->svName, s_sepStr.data[5], 72);

			/* 蓄積番号の取得 */
			storeNumb = atoi( s_sepStr.data[6] );
			if( ( storeNumb < 0 ) || ( storeNumb > PV_MAX_SV_HISTORY ) )
			{
				sprintf( errMsg, "CSV-Format Error (storeNumb). File[%s] Line[%d] DAT[7]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			if( storeNumb > 0 )
			{
				/* 重複定義の確認 */
				if( setFlag[storeNumb - 1] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (storeNumb Dupulication Error). File[%s] LINE[%d] DAT[7]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlag[storeNumb - 1] = 1;

				/* 状態履歴情報を設定 */
				ps_SVRECINF[storeNumb -1].statFlag = 1;
				ps_SVRECINF[storeNumb -1].soKind = mKind;
				ps_SVRECINF[storeNumb -1].soNumb = mNumb;
				ps_SVRECINF[storeNumb -1].svNumb = svNumb;
				ps_SVRECINF[storeNumb -1].svStLinkPos = svStLink;
				ps_SVRECINF[storeNumb -1].stBit = (0x80 >> svStBit);
			}
			(ps_SVDATINF + svIfLink)->storeNumb = storeNumb;

/* [20230529] Add St */
			/* IoT送信有無の取得 */
			workData = atoi( s_sepStr.data[7] );
			if( (workData != 0) && (workData != 1) )
			{
				sprintf( errMsg, "CSV-Format Error (IoT send Flag Error). File[%s] Line[%d] DAT[8]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_SVDATINF + svIfLink)->iotSndFlag = workData;
/* [20230529] Add Ed */

#if 0
			printf("SVDATINF : SOKD:%02d, SONO:%02d, SVNO:%03d, LINK[%4d], MSK:%02X, LINK[%4d] FL:%02d, SK:%2d, SN:%2d, SV:%3d, JK:%2d, NM:%s\n",
				mKind, mNumb, svNumb,
				svStLink, (unsigned char)0x80 >> svStBit,
				svIfLink,
				(ps_SVDATINF + svIfLink)->statFlag,
				(ps_SVDATINF + svIfLink)->soKind,
				(ps_SVDATINF + svIfLink)->soNumb,
				(ps_SVDATINF + svIfLink)->svNumb,
				(ps_SVDATINF + svIfLink)->svKind,
				(ps_SVDATINF + svIfLink)->svName);
#endif
				
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
	munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA);
	munmap( ps_SVMSKINF, sizeof(struct st_SVMSKINF) * ((PV_MAX_SVDATA-1) / 8 + 1) );
	munmap( ps_SVRECINF, sizeof(struct st_SVRECINF) * PV_MAX_SV_HISTORY);
	/* DATファイルをクローズ */
	close( fd_Dat );
	close( fd_Msk );
	close( fd_Hst );
	/* 終了 */
	return( retCode );	
}

