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
#include <stdlib.h>

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
 * 		TMAGGINF.csv ファイルから合計演算情報ファイル（TMAGGINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_TMAGGINF_Create( struct st_linkPoint *ps_linkPoint, struct st_machineDef *ps_machineDef )
{
	FILE				*fp;						/* ファイルポインタ */
	int					fd_sum;						/* ファイルディスクリプタ */
	int					fd_dat;						/* ファイルディスクリプタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	char				datFilePathAGG[1024];		/* DATファイルパス */
	char				datFilePathDAT[1024];		/* DATファイルパス */
	char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					chkData;					/* チェック用変数 */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */

	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */

	struct st_TMAGGINF	*ps_TMAGGINF;				/* 合計演算情報ポインタ */
	struct st_TMDATINF	*ps_TMDATINF;				/* 計測情報ポインタ */

	int					mKind;						/* 装置種別 */
	int					mNumb;						/* 装置番号 */
	int					tmNumb;						/* 計測番号 */
	int					statFlag;					/* 実装/未実装 */
	int					tmKind;						/* 計測種別 */
	int					expData;					/* 乗率 */
	int					unitKind;					/* 単位種別 */
	long long			convData;					/* 変換値 */
	long long			baseData;					/* ベース値 */
	int					expMin;						/* 乗率最小値 */
	int 				unitMin;					/* 単位最小 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	int					tmIfLink;					/* 計測情報リンク */
	int					tmSumLink;					/* 合計演算リンク */
	int					sumtmIfLink;				/* 計測情報リンク */
	int					summKind;					/* 装置種別（演算元） */
	int					summNumb;					/* 装置番号（演算元） */
	int					sumtmNumb;					/* 計測番号（演算元） */
	int					sumMaxCnt;					/* 合計演算元情報数 */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathAGG, PV_PATH_DAT );
	strcpy(datFilePathDAT, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_TMAGGINF );
	strcat(datFilePathAGG, PV_FNAME_TMAGGINF );
	strcat(datFilePathDAT, PV_FNAME_TMDATINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePathAGG, ".dat");
	strcat(datFilePathDAT, ".dat");

	/* 合計演算情報ファイル */

	/* DATファイル生成 */
	fd_sum = open( datFilePathAGG, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_sum == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathAGG );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( fd_sum, sizeof(struct st_TMAGGINF) * PV_MAX_PVSYS_AGGTMPOS ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathAGG );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_sum );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMAGGINF = (struct st_TMAGGINF *)mmap( NULL, sizeof(struct st_TMAGGINF) * PV_MAX_PVSYS_AGGTMPOS, PROT_READ | PROT_WRITE, MAP_SHARED, fd_sum, 0 );
	if( ps_TMAGGINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathAGG );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_TMAGGINF, 0, sizeof(struct st_TMAGGINF) * PV_MAX_PVSYS_AGGTMPOS );

	/* 計測情報ファイル */

	/* DATファイルを開く */
	fd_dat = open( datFilePathDAT, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathDAT );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_TMDATINF = (struct st_TMDATINF *)mmap( NULL, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dat, 0 );
	if( ps_TMDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDAT );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
		return( retCode );
	}

	/* 合計演算情報の CSVファイルが存在しない場合 */
	if( access( csvFilePath, R_OK ) != 0 )
	{
		/* エラーメッセージの出力 */
		sprintf( errMsg, "Information: File[%s] Not Found", csvFilePath );
		commfunc_syslog_msgset( "FILEMAKE" , errMsg );
		
		/* メモリマップを解除 */
		munmap( ps_TMAGGINF, sizeof(struct st_TMAGGINF) * PV_MAX_PVSYS_AGGTMPOS );
		munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA );
		/* DATファイルをクローズ */
		close( fd_sum );
		close( fd_dat );
		/* 終了 */
		return( 0 );
	}

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
			if( (csvCount < 11) || (csvCount > 211) )
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* CSVデータ数のチェック */
			chkData = ( csvCount - 11 ) % 4;
			if( chkData != 0 )
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 合計演算対象情報数の取得 */
			sumMaxCnt = atoi( s_sepStr.data[10] );
			if( (sumMaxCnt < 1) && (sumMaxCnt > PV_MAX_AGG_TMCNT) )
			{
				sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[11]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* CSVデータ数が不正の場合 */
			if( csvCount < (sumMaxCnt * 4 + 11))
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 装置種別の取得 */
			mKind = atoi( s_sepStr.data[0] );
			if( mKind != PV_MKIND_PVSYS )
			{
				sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[1]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 装置番号の取得 */
			mNumb = atoi( s_sepStr.data[1] );
			if(mNumb != 1)
			{
				sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[2]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 計測番号の取得 */
			tmNumb = atoi( s_sepStr.data[2] );
			if( (tmNumb <  (PV_MAX_PVSYS_CTLTMPOS + 1)) || (tmNumb > (PV_MAX_PVSYS_AGGTMPOS+PV_MAX_PVSYS_CTLTMPOS)) )
			{
				sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 実装/未実装の取得 */
			statFlag = atoi( s_sepStr.data[3] );
			if( (statFlag != 0) && (statFlag != 1) )
			{
				sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] Line[%d] DAT[4]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 未実装の場合は次の情報を処理 */
			if( statFlag == 0 ) continue;

			/* 計測種別の取得 */
			tmKind = atoi( s_sepStr.data[4] );
			if( (tmKind != PV_TM_KIND_VALUE) && (tmKind != PV_TM_KIND_COUNT) && (tmKind != PV_TM_KIND_AVERG) )
			{
				sprintf( errMsg, "CSV-Format Error (TM Kind). File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 乗率（冪乗）の取得 */
			expData = atoi( s_sepStr.data[5] );
			if( (expData < -20) || (expData > 20) )
			{
				sprintf( errMsg, "CSV-Format Error (Exp Data). File[%s] Line[%d] DAT[6]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 変換値の取得 */
			convData = (long long)atoll( s_sepStr.data[6] );
			if( convData == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Convert Data). File[%s] Line[%d] DAT[7]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* ベース値の取得 */
			baseData = (long long)atoll( s_sepStr.data[7] );
			/* 計測単位の取得 */
			unitKind = atoi( s_sepStr.data[8] );
			if( (unitKind < 0) || (unitKind > PV_TM_UKIND_LAST) )
			{
				sprintf( errMsg, "CSV-Format Error (Unit Kind Data). File[%s] Line[%d] DAT[9]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			/* 計測名称の取得 */
			if( strlen(s_sepStr.data[9]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] Line[%d] DAT[10]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* リンク情報を取得 */
			tmIfLink = ps_linkPoint[mKind].tmIfLink + ps_machineDef[mKind].maxTmNumb * (mNumb - 1) + tmNumb - 1;
			/* リンク情報を取得 */
			tmSumLink = tmNumb - PV_MAX_PVSYS_CTLTMPOS - 1;

			/* 装置種別の設定 */
			ps_TMAGGINF[tmSumLink].soKind   = mKind;
			/* 装置番号の設定 */
			ps_TMAGGINF[tmSumLink].soNumb   = mNumb;
			/* 計測番号の設定 */
			ps_TMAGGINF[tmSumLink].tmNumb   = tmNumb;
			/* 実装/未実装の設定 */
			ps_TMAGGINF[tmSumLink].statFlag = statFlag;
			/* リンク情報の設定 */
			ps_TMAGGINF[tmSumLink].tmIfLink = tmIfLink;
			/* 計測名称の設定 */
			strncpy( ps_TMAGGINF[tmSumLink].tmName, s_sepStr.data[9], 72);
			/* 合計演算対象情報数の設定 */
			ps_TMAGGINF[tmSumLink].aggTmNum = sumMaxCnt;
			/* 調整用変換値の設定 */
			ps_TMAGGINF[tmSumLink].choseiConv = convData;
			/* 調整用ベース値の設定 */
			ps_TMAGGINF[tmSumLink].choseiBase = baseData;
#if 0
printf("TMAGGINF : LINK[%4d] FL:%2d, SK:%2d, SN:%2d, TN:%3d, KD:%2d, EX:%2d, CN:%d, BS:%d, UN:%2d, NM:%s\n",
	tmSumLink,
	ps_TMAGGINF[tmSumLink].statFlag,
	ps_TMAGGINF[tmSumLink].soKind,
	ps_TMAGGINF[tmSumLink].soNumb,
	ps_TMAGGINF[tmSumLink].tmNumb,
	tmKind,
	expData,
	convData,
	baseData,
	unitKind,
	ps_TMAGGINF[tmSumLink].tmName);
printf("TMDATINF : LINK[%4d] FL:%2d, SK:%2d, SN:%2d, TN:%3d, KD:%2d, EX:%2d, CN:%d, BS:%d, UN:%2d, NM:%s\n",
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
	(ps_TMDATINF + tmIfLink)->tmName);
#endif
			/* 計測情報ファイルに設定 */
			ps_TMDATINF[tmIfLink].statFlag = statFlag;
			ps_TMDATINF[tmIfLink].soKind   = mKind;
			ps_TMDATINF[tmIfLink].soNumb   = mNumb;
			ps_TMDATINF[tmIfLink].tmNumb   = tmNumb;
			ps_TMDATINF[tmIfLink].tmKind   = tmKind;
			ps_TMDATINF[tmIfLink].expData  = expData;
			ps_TMDATINF[tmIfLink].unitKind = unitKind;
			ps_TMDATINF[tmIfLink].convData = convData;
			ps_TMDATINF[tmIfLink].baseData = baseData;
			strncpy( ps_TMDATINF[tmIfLink].tmName, s_sepStr.data[9], 72);

			/* 乗率最小および単位最小を設定 */
			expMin  =  999999999;
			unitMin =  999999999;

/* [20250807] Chg St */
			/* 合計演算結果の単位確認 */
			if( (retCode = commfunc_sumInfChk( unitKind, unitKind, expData, &expMin, &unitMin, sumMaxCnt )) != 0 )
/* [20250807] Chg Ed */
			{
				sprintf( errMsg, "CSV-Format Error (Unit Kind Data). File[%s] Line[%d] DAT[9]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
#if 0
			printf("TMAGGINF : LINK[%2d] sumMaxCnt:%2d\n",
				tmSumLink,
				sumMaxCnt);
#endif

			/* 合計演算の対象情報数分繰り返す */
			for( loopCnt1 = 0 ; (loopCnt1 < sumMaxCnt) & (retCode == 0) ; loopCnt1++ )
			{
				/* 装置種別の取得 */
				summKind = atoi( s_sepStr.data[loopCnt1 * 4 + 12] );
				if( ( summKind < PV_MKIND_PVSYS ) || ( summKind > PV_MKIND_LAST ) )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 13 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 装置番号の取得 */
				summNumb = atoi( s_sepStr.data[loopCnt1 * 4 + 13] );
				if( (summNumb < 1) || (summNumb > ps_machineDef[summKind].maxMNumb) )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 14 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 計測番号の取得 */
				sumtmNumb = atoi( s_sepStr.data[loopCnt1 * 4 + 14] );
				if( summKind == PV_MKIND_PVSYS )
				{
					/* 合計演算の計測番号は合計演算の計測番号を演算元として指定可能だが自計測番号以下であること */
					if( (sumtmNumb < 1) || (sumtmNumb >= tmNumb) )
					{
						sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 15 );
						commfunc_syslog_msgset( "FILEMAKE", errMsg );
						retCode = -1;
						break;
					}
				}
				else
				{
					/* 装置番号の範囲確認 */
					if( (sumtmNumb < 1) || (sumtmNumb > ps_machineDef[summKind].maxTmNumb) )
					{
						sprintf( errMsg, "CSV-Format Error (TM Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 15 );
						commfunc_syslog_msgset( "FILEMAKE", errMsg );
						retCode = -1;
						break;
					}
				}

				/* 演算対象の計測情報のリンクを取得 */
				sumtmIfLink = ps_linkPoint[summKind].tmIfLink + ps_machineDef[summKind].maxTmNumb * (summNumb - 1) + sumtmNumb - 1;
				/* 演算元の計測情報が未実装の場合 */
				if( ps_TMDATINF[sumtmIfLink].statFlag == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Non-Exist TM Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 15 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 計測種別が異なる場合 */
				if( ps_TMDATINF[sumtmIfLink].tmKind != tmKind )
				{
					sprintf( errMsg, "CSV-Format Error (Different TM kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 15 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

/* [20250807] Chg St */
				/* 単位種別の確認 */
				if( (retCode = commfunc_sumInfChk( unitKind, ps_TMDATINF[sumtmIfLink].unitKind, ps_TMDATINF[sumtmIfLink].expData, &expMin, &unitMin, sumMaxCnt )) != 0 )
/* [20250807] Chg Ed */
				{
					sprintf( errMsg, "CSV-Format Error (Unit kind). File[%s] Line[%d] DAT[%d] RET[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 15, retCode );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 加減算情報の取得 */
				if( s_sepStr.data[loopCnt1 * 4 + 11][0] == '+' )
				{
					/* 加減算情報の設定 */
					ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].plusminus = PV_DEF_PLUS;
				}
				else if( s_sepStr.data[loopCnt1 * 4 + 11][0] == '-' )
				{
					/* 加減算情報の設定 */
					ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].plusminus = PV_DEF_MINUS;
				}
				else
				{
					sprintf( errMsg, "CSV-Format Error (Plus/Minus). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 4 + 12 );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* リンク情報の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].tmIfLink = sumtmIfLink;
				/* 装置種別の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].soKind = summKind;
				/* 装置番号の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].soNumb = summNumb;
				/* 計測番号の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].tmNumb = sumtmNumb;
				/* 調整値（変換値）の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].choseiConv = ps_TMDATINF[sumtmIfLink].convData;
				/* 調整値（ベース値）の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].choseiBase = ps_TMDATINF[sumtmIfLink].baseData;

#if 0
			printf("TMAGGINF : SUMINFO[%2d] FL:%2d, SK:%2d, SN:%2d, TN:%3d\n",
				loopCnt1+1,
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].tmIfLink,
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].soKind,
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].soNumb,
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].tmNumb);
#endif

			}

			/* 合計演算結果の乗率調整値の設定 */
			ps_TMAGGINF[tmSumLink].choseiExp  = commfunc_pow10abs( expData - expMin ) ;

			/* 単位種別による乗率を求める */
			expData = commfunc_chkUChosei( unitKind ) - unitMin;

			/* 合計演算結果の単位調整値の設定 */
			ps_TMAGGINF[tmSumLink].choseiUnit = commfunc_pow10abs( expData );

			/* 合計演算の対象情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < sumMaxCnt ; loopCnt1++ )
			{
				/* リンク情報の取得 */
				sumtmIfLink = ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].tmIfLink;

				/* 合計演算対象の乗率と全計測値の乗率の差分を調整 */
				expData  = ps_TMDATINF[sumtmIfLink].expData - expMin;

				/* 調整値（乗率）の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].choseiExp = commfunc_pow10abs( expData );
				
				/* 単位種別による乗率を求める */
				expData = commfunc_chkUChosei( ps_TMDATINF[sumtmIfLink].unitKind ) - unitMin;

				/* 調整値（単位）の設定 */
				ps_TMAGGINF[tmSumLink].s_tmaggdata[loopCnt1].choseiUnit = commfunc_pow10abs( expData );

			}

#if 0
			printf("TMAGGINF : LINK[%4d] FL:%2d, SK:%2d, SN:%2d, TN:%3d, KD:%2d, EX:%2d, CN:%d, BS:%d, UN:%2d, NM:%s\n",
				tmIfLink,
				(ps_TMAGGINF + tmIfLink)->statFlag,
				(ps_TMAGGINF + tmIfLink)->soKind,
				(ps_TMAGGINF + tmIfLink)->soNumb,
				(ps_TMAGGINF + tmIfLink)->tmNumb,
				(ps_TMAGGINF + tmIfLink)->tmKind,
				(ps_TMAGGINF + tmIfLink)->expData,
				(ps_TMAGGINF + tmIfLink)->convData,
				(ps_TMAGGINF + tmIfLink)->baseData,
				(ps_TMAGGINF + tmIfLink)->unitKind,
				(ps_TMAGGINF + tmIfLink)->tmName);
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
	munmap( ps_TMAGGINF, sizeof(struct st_TMAGGINF) * PV_MAX_PVSYS_AGGTMPOS);
	munmap( ps_TMDATINF, sizeof(struct st_TMDATINF) * PV_MAX_TMDATA);
	/* DATファイルをクローズ */
	close( fd_sum );
	close( fd_dat );
	/* 終了 */
	return( retCode );	
}

