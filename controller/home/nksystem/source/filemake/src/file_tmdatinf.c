/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 *------------------------------------------------------------------------------
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
 * 計測情報ファイル生成処理
 *
 * 機能：
 * 		TMDATINF.csv ファイルから計測情報ファイル（TMDATINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_TMDATINF_Create( struct st_linkPoint *ps_linkPoint, struct st_machineDef *ps_machineDef )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_dat;						/* ファイルディスクリプタ */
	int					fd_hst;					/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				tmdatFilePath[1024];		/* DATファイルパス */
	char				tmhstFilePath[1024];		/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_TMDATINF	*ps_TMDATINF;				/* 計測情報ポインタ */
	struct st_TMRECINF	*ps_TMRECINF;				/* 計測履歴情報ポインタ */
	int					mKind;						/* 装置種別 */
	int					mNumb;						/* 装置番号 */
	int					tmNumb;						/* 計測番号 */
	int					workData;					/* 数値変換用 */
	long long			workDataLong;				/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	int					maxTmNumb;					/* 最大計測数 */
	int					maxMNumb;					/* 最大装置数 */
	int					linkBase;					/* 計測情報基本リンク */
	int					tmIfLink;					/* 計測情報リンク */
	int					statFlag;					/* 実装/未実装 */
	int					storeNumb;					/* 蓄積番号 */
	char				setFlag[PV_MAX_TM_HISTORY];	/* 設定済みフラグ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(tmdatFilePath, PV_PATH_DAT );
	strcpy(tmhstFilePath, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_TMDATINF );
	strcat(tmdatFilePath, PV_FNAME_TMDATINF );
	strcat(tmhstFilePath, PV_FNAME_TMRECINF );
	strcat(csvFilePath, ".csv");
	strcat(tmdatFilePath, ".dat");
	strcat(tmhstFilePath, ".dat");

	/* DATファイル生成 */
	fd_dat = open( tmdatFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", tmdatFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	fd_hst = open( tmhstFilePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_hst == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", tmhstFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_dat, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", tmdatFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_dat );
		retCode = -1;
		return( retCode );
	}
	if( ftruncate( fd_hst, sizeof(struct st_TMRECINF) * PV_MAX_TM_HISTORY ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", tmdatFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_dat );
		close( fd_hst );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMDATINF = (struct st_TMDATINF *)mmap( NULL, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dat, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", tmdatFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMRECINF = (struct st_TMRECINF *)mmap( NULL, sizeof(struct st_TMRECINF) * PV_MAX_TM_HISTORY, PROT_READ | PROT_WRITE, MAP_SHARED, fd_hst, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", tmdatFilePath );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_TMDATINF, 0, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA );
	/* ファイルの内容を0クリア */
	memset( ps_TMRECINF, 0, sizeof(struct st_TMRECINF) * PV_MAX_TM_HISTORY );

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			/* CSV処理カウンタを＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（9文字以下）はスキップ */
			if( strlen( readBuff ) < 10) continue;
			/* CSVデータの分割 */
			csvCount = func_sep_data( readBuff, &s_sepStr );
			/* CSVデータ数が不正の場合 */
			if( csvCount != 12 )		/* [20230529] Chg */
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 12). File[%s] line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
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
			/* 計測番号の取得 */
			tmNumb = atoi( s_sepStr.data[2] );
			if( (tmNumb < 1) || (tmNumb > PV_MAX_TMDATA) )
			{
				sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置の最大情報数の取得 */
			maxMNumb  = ps_machineDef[mKind].maxMNumb;
			maxTmNumb = ps_machineDef[mKind].maxTmNumb;
			linkBase  = ps_linkPoint[mKind].tmIfLink + ps_machineDef[mKind].maxTmNumb * (mNumb - 1);

			/* 装置番号が超過している場合 */
			if( mNumb > maxMNumb )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[2] mNumb(%d) > maxMNumb(%d)", csvFilePath, syoriCount, mNumb, maxMNumb );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 計測番号が超過している場合 */
			if( tmNumb > maxTmNumb )
			{
				sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[3] tmNumb(%d) > maxTmNumb(%d)", csvFilePath, syoriCount, tmNumb, maxTmNumb );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置が未実装の場合 */
			if( ps_linkPoint[mKind].statFlag == 0 )
				continue;

			/* 計測情報の保存ポインタを積算 */
			tmIfLink = linkBase + tmNumb - 1;

			/* 実装/未実装の取得 */
			statFlag = atoi( s_sepStr.data[3] );
			if( (statFlag != 0) && (statFlag != 1) )
			{
				sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] Line[%d] DAT[4]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_TMDATINF + tmIfLink)->statFlag = statFlag;
			/* 所属装置種別の取得 */
			(ps_TMDATINF + tmIfLink)->soKind = mKind;
			/* 所属装置番号の取得 */
			(ps_TMDATINF + tmIfLink)->soNumb = mNumb;
			/* 装置内計測番号の取得 */
			(ps_TMDATINF + tmIfLink)->tmNumb = tmNumb;
			/* 計測種別の取得 */
			workData = atoi( s_sepStr.data[4] );
			if( (workData != PV_TM_KIND_VALUE) && (workData != PV_TM_KIND_COUNT) && (workData != PV_TM_KIND_AVERG) )
			{
				sprintf( errMsg, "CSV-Format Error (TM Kind). File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_TMDATINF + tmIfLink)->tmKind = workData;
			/* 乗率（冪乗）の取得 */
			workData = atoi( s_sepStr.data[5] );
			if( (workData < -20) || (workData > 20) )
			{
				sprintf( errMsg, "CSV-Format Error (Exp Data). File[%s] Line[%d] DAT[6]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_TMDATINF + tmIfLink)->expData = workData;
			/* 変換値の取得 */
			workDataLong = (long long)atoll( s_sepStr.data[6] );
			if( workDataLong == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Convert). File[%s] Line[%d] DAT[7]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_TMDATINF + tmIfLink)->convData = workDataLong;
			/* ベース値の取得 */
			workDataLong = atoll( s_sepStr.data[7] );
			(ps_TMDATINF + tmIfLink)->baseData = workDataLong;
			/* 計測単位の取得 */
			workData = atoi( s_sepStr.data[8] );
			if( (workData < 0 ) || (workData > PV_TM_UKIND_LAST) )
			{
				sprintf( errMsg, "CSV-Format Error (Unit Kind Data). File[%s] Line[%d] DAT[9]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_TMDATINF + tmIfLink)->unitKind = workData;
			/* 計測名称の取得 */
			if( strlen(s_sepStr.data[9]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] Line[%d] DAT[10]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			strncpy( (ps_TMDATINF + tmIfLink)->tmName, s_sepStr.data[9], 72);

			/* 蓄積番号の取得 */
			storeNumb = atoi( s_sepStr.data[10] );
			if( ( storeNumb < 0 ) || ( storeNumb > PV_MAX_TM_HISTORY ) )
			{
				sprintf( errMsg, "CSV-Format Error (storeNumb). File[%s] Line[%d] DAT[11]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			if( storeNumb > 0 )
			{
				/* 重複定義の確認 */
				if( setFlag[storeNumb - 1] == 1 )
				{
					sprintf( errMsg, "CSV-Format Error (storeNumb Dupulication Error). File[%s] LINE[%d] DAT[11]", csvFilePath, syoriCount );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				setFlag[storeNumb - 1] = 1;

				/* 計測履歴情報を設定 */
				ps_TMRECINF[storeNumb -1].statFlag = 1;
				ps_TMRECINF[storeNumb -1].soKind = mKind;
				ps_TMRECINF[storeNumb -1].soNumb = mNumb;
				ps_TMRECINF[storeNumb -1].tmNumb = tmNumb;
				ps_TMRECINF[storeNumb -1].tmIfLinkPos = tmIfLink;
			}
			(ps_TMDATINF + tmIfLink)->storeNumb = storeNumb;

/* [20230529] Add St */
			/* IoT送信有無の取得 */
			statFlag = atoi( s_sepStr.data[11] );
			if( (statFlag != 0) && (statFlag != 1) )
			{
				sprintf( errMsg, "CSV-Format Error (IoT send Flag Error). File[%s] Line[%d] DAT[12]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			(ps_TMDATINF + tmIfLink)->iotSndFlag = statFlag;
/* [20230529] Add Ed */
			
#if 0
			printf("TMDATINF : LINK[%4d] FL:%2d, SK:%2d, SN:%2d, TN:%3d, KD:%2d, EX:%2d, CN:%lld, BS:%lld, UN:%2d, ST:%2d, NM:%s\n",
				tmIfLink,
				(ps_TMDATINF + tmIfLink)->statFlag,
				(ps_TMDATINF + tmIfLink)->soKind,
				(ps_TMDATINF + tmIfLink)->soNumb,
				(ps_TMDATINF + tmIfLink)->tmNumb,
				(ps_TMDATINF + tmIfLink)->tmKind,
				(ps_TMDATINF + tmIfLink)->expData,
				(ps_TMDATINF + tmIfLink)->convData,
				(ps_TMDATINF + tmIfLink)->baseData,
				(ps_TMDATINF + tmIfLink)->unitKind,
				(ps_TMDATINF + tmIfLink)->storeFlg,
				(ps_TMDATINF + tmIfLink)->tmName);
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
	munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
	munmap( ps_TMRECINF, sizeof(struct st_TMRECINF) * PV_MAX_TM_HISTORY);
	/* DATファイルをクローズ */
	close( fd_dat );
	close( fd_hst );
	/* 終了 */
	return( retCode );	
}

