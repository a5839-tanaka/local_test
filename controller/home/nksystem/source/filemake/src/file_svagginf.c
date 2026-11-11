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
 * 		SVAGGINF.csv ファイルから合計演算情報ファイル（SVAGGINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_SVAGGINF_Create( struct st_linkPoint *ps_linkPoint, struct st_machineDef *ps_machineDef )
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

	struct st_SVAGGINF	*ps_SVAGGINF;				/* 合計演算情報ポインタ */
	struct st_SVDATINF	*ps_SVDATINF;				/* 状態情報ポインタ */

	int					mKind;						/* 装置種別 */
	int					mNumb;						/* 装置番号 */
	int					svNumb;						/* 状態番号 */
	int					statFlag;					/* 実装/未実装 */
	int					tmKind;						/* 計測種別 */
	int					expData;					/* 乗率 */
	int					unitKind;					/* 単位種別 */
	long long			convData;					/* 変換値 */
	long long			baseData;					/* ベース値 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	int					svIfLink;					/* 状態情報リンク */
	int					svAggLink;					/* 合計演算リンク */
	int					sumsvIfLink;				/* 状態情報リンク */
	int					stPoint;					/* 通信/状態情報リンク */
	int					svstMask;					/* 状態情報リンク情報ビットデータ */
	int					summKind;					/* 装置種別（演算元） */
	int					summNumb;					/* 装置番号（演算元） */
	int					sumsvstNumb;					/* 状態番号（演算元） */
	int					sumMaxCnt;					/* 合計演算元情報数 */
	int					logicalOpe;					/* 論理演算子 */
	int					svstkind;					/* 対象状態情報 */
	int					svVal;						/* 状態 */
	int					svOffset;					/* 開始SV番号 */
	int					csvOffset = 0;				/* CSV処理オフセット */
	int					onCheckSec;						/* ON時判定時間 */
	int					offCheckSec;					/* OFF時判定時間 */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcpy(datFilePathAGG, PV_PATH_DAT );
	strcpy(datFilePathDAT, PV_PATH_DAT );
	strcat(csvFilePath, PV_FNAME_SVAGGINF );
	strcat(datFilePathAGG, PV_FNAME_SVAGGINF );
	strcat(datFilePathDAT, PV_FNAME_SVDATINF );
	strcat(csvFilePath, ".csv");
	strcat(datFilePathAGG, ".dat");
	strcat(datFilePathDAT, ".dat");

	svOffset = ( PV_MAX_PVSYS_CTLSVPOS + PV_MAX_PVSYS_MONSVPOS );

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
	if( ftruncate( fd_sum, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS ) == -1 )
	{
		sprintf( errMsg, "File[%s] ftruncate error", datFilePathAGG );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_sum );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_SVAGGINF = (struct st_SVAGGINF *)mmap( NULL, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS, PROT_READ | PROT_WRITE, MAP_SHARED, fd_sum, 0 );
	if( ps_SVAGGINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathAGG );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_sum );
		retCode = -1;
		return( retCode );
	}

	/* ファイルの内容を0クリア */
	memset( ps_SVAGGINF, 0, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS );

	/* 状態情報ファイル */

	/* DATファイルを開く */
	fd_dat = open( datFilePathDAT, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fd_dat == -1 )
	{
		sprintf( errMsg, "File[%s] Open error", datFilePathDAT );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_sum );
		munmap( ps_SVAGGINF, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS );
		retCode = -1;
		return( retCode );
	}

	/* ファイルをメモリマップ */
	ps_SVDATINF = (struct st_SVDATINF *)mmap( NULL, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA, PROT_READ | PROT_WRITE, MAP_SHARED, fd_dat, 0 );
	if( ps_SVDATINF == MAP_FAILED)
	{
		sprintf( errMsg, "File[%s] MMAP error", datFilePathDAT );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		close( fd_sum );
		close( fd_dat );
		munmap( ps_SVAGGINF, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS );
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
		munmap( ps_SVAGGINF, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS );
		munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA );
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
			if( (csvCount < 7) || (csvCount > (7+(PV_MAX_AGG_SVCNT*7)) ))
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT_1). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* CSVデータ数のチェック */
			chkData = ( csvCount - 7 ) % 7;
			if( chkData != 0 )
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT_2). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvCount );
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


			/* 合計演算対象情報数の取得 */
			sumMaxCnt = atoi( s_sepStr.data[6] );
			if( (sumMaxCnt < 1) && (sumMaxCnt > PV_MAX_AGG_SVCNT) )
			{
				sprintf( errMsg, "CSV-Format Error (SV Number_1(%d)). File[%s] Line[%d] DAT[7]", sumMaxCnt, csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* CSVデータ数が不正の場合 */
			if( csvCount < (sumMaxCnt * 5 + 7))
			{
				sprintf( errMsg, "CSV-Format Error (SEP-DATA CNT_3). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
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
			/* 状態番号の取得 */
			svNumb = atoi( s_sepStr.data[2] );
			if( (svNumb <  ( svOffset + 1)) || (svNumb > ( svOffset + PV_MAX_PVSYS_AGGSVPOS)) )
			{
				sprintf( errMsg, "CSV-Format Error (SV Number_2). File[%s] Line[%d] DAT[3]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 状態名称の取得 */
			if( strlen(s_sepStr.data[4]) == 0 )
			{
				sprintf( errMsg, "CSV-Format Error (Name is Empty). File[%s] Line[%d] DAT[5]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}

			/* 論理演算子の取得 */
			logicalOpe = atoi( s_sepStr.data[5] );
			if( (logicalOpe != PV_DEF_AND) && (logicalOpe != PV_DEF_OR) )
			{
				sprintf( errMsg, "CSV-Format Error (logicalOpe). File[%s] Line[%d] DAT[6]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}


			/* リンク情報を取得 */
			svIfLink = ps_linkPoint[mKind].svIfLink + ps_machineDef[mKind].maxSvNumb * (mNumb - 1) + svNumb - 1;
			/* リンク情報を取得 */
			svAggLink = svNumb - svOffset - 1;

			/* 装置種別の設定 */
			ps_SVAGGINF[svAggLink].soKind   = mKind;
			/* 装置番号の設定 */
			ps_SVAGGINF[svAggLink].soNumb   = mNumb;
			/* 状態番号の設定 */
			ps_SVAGGINF[svAggLink].svNumb   = svNumb;
			/* 実装/未実装の設定 */
			ps_SVAGGINF[svAggLink].statFlag = statFlag;
			/* リンク情報の設定 */
			ps_SVAGGINF[svAggLink].svIfLink = svIfLink;
			/* 計測名称の設定 */
			strncpy( ps_SVAGGINF[svAggLink].svName, s_sepStr.data[4], 72);
			/* 合計演算対象情報数の設定 */
			ps_SVAGGINF[svAggLink].aggSvNum = sumMaxCnt;
			/* 論理演算子 */
			ps_SVAGGINF[svAggLink].logicalOpe = logicalOpe;

			/* 状態情報ファイルに設定 */
			ps_SVDATINF[svIfLink].statFlag = statFlag;
			ps_SVDATINF[svIfLink].soKind   = mKind;
			ps_SVDATINF[svIfLink].soNumb   = mNumb;
			ps_SVDATINF[svIfLink].svNumb   = svNumb;
			strncpy( ps_SVDATINF[svIfLink].svName, s_sepStr.data[4], 72);

			/* 合計演算の対象情報数分繰り返す */
			for( loopCnt1 = 0 ; (loopCnt1 < sumMaxCnt) & (retCode == 0) ; loopCnt1++ )
			{
				csvOffset = loopCnt1 * 7 + 7;
				/* 装置種別の取得 */
				summKind = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( ( summKind < PV_MKIND_PVSYS ) || ( summKind > PV_MKIND_LAST ) )
				{
					sprintf( errMsg, "CSV-Format Error (Target Machine Kind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}
				/* 装置番号の取得 */
				summNumb = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (summNumb < 1) || (summNumb > ps_machineDef[summKind].maxMNumb) )
				{
					sprintf( errMsg, "CSV-Format Error (Target Machine Number). File[%s] Line[%d] DAT[%d] summNumb[%d] summNumbMax[%d]", csvFilePath, syoriCount, csvOffset, summNumb, ps_machineDef[summKind].maxMNumb );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 状態種別の取得 */
				svstkind = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (svstkind != PV_DEF_SV) && (svstkind != PV_DEF_ST) )
				{
					sprintf( errMsg, "CSV-Format Error (Target svstkind). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* 状態番号の取得 */
				sumsvstNumb = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;

				if( svstkind == PV_DEF_SV )
				{
					if( summKind == PV_MKIND_PVSYS )
					{
						/* 統合コントローラの場合 */
						/* SVの範囲確認 */
						if( (sumsvstNumb < 1) || (sumsvstNumb > ps_machineDef[summKind].maxSvNumb) || ( svNumb == sumsvstNumb ) )
						{
							/* SV範囲外、自分自身の場合エラーにする */
							sprintf( errMsg, "CSV-Format Error (Target SV Number_1) (sumsvstNumb,svNumb,maxSvNumb)=(%d,%d,%d). File[%s] Line[%d] DAT[%d]", sumsvstNumb, svNumb, ps_machineDef[summKind].maxSvNumb, csvFilePath, syoriCount, loopCnt1 * 5 + 11 );
							commfunc_syslog_msgset( "FILEMAKE", errMsg );
							retCode = -1;
							break;
						}
					}
					else
					{
						/* 装置番号の範囲確認 */
						if( (sumsvstNumb < 1) || (sumsvstNumb > ps_machineDef[summKind].maxSvNumb) )
						{
							sprintf( errMsg, "CSV-Format Error (Target SV Number_2). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 5 + 11 );
							commfunc_syslog_msgset( "FILEMAKE", errMsg );
							retCode = -1;
							break;
						}
					}

					/* 演算対象の状態情報のリンクを取得 */
					sumsvIfLink = ps_linkPoint[summKind].svIfLink + ps_machineDef[summKind].maxSvNumb * (summNumb - 1) + sumsvstNumb - 1;

					/* 演算元の状態情報が未実装の場合 */
					if( ps_SVDATINF[sumsvIfLink].statFlag == 0 )
					{
						sprintf( errMsg, "CSV-Format Error (Target Non-Exist SV Number). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 5 + 11 );
						commfunc_syslog_msgset( "FILEMAKE", errMsg );
						retCode = -1;
						break;
					}
					stPoint = ps_linkPoint[summKind].svStLink + ((ps_machineDef[summKind].maxSvNumb - 1) / 8 + 1) * (summNumb-1);
					stPoint += ((sumsvstNumb - 1) / 8);
					svstMask = 0x80 >> ((sumsvstNumb - 1) % 8);
				}
				else //if( svstkind == PV_DEF_ST )
				{
					if( summKind == PV_MKIND_PVSYS )
					{
						sprintf( errMsg, "CSV-Format Error (Target ST Number_1). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 5 + 11 );
						commfunc_syslog_msgset( "FILEMAKE", errMsg );
						retCode = -1;
						break;
					}

					/* 装置番号の範囲確認 */
					if( (sumsvstNumb < 1) || (sumsvstNumb > PV_MAX_STPOS) )
					{
						sprintf( errMsg, "CSV-Format Error (Target ST Number_2). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, loopCnt1 * 5 + 11 );
						commfunc_syslog_msgset( "FILEMAKE", errMsg );
						retCode = -1;
						break;
					}
					stPoint = ps_linkPoint[summKind].stStLink + ((PV_MAX_STPOS - 1) / 8 + 1) * (summNumb-1);
					stPoint += ((sumsvstNumb - 1) / 8);
					svstMask = 0x80 >> ((sumsvstNumb - 1) % 8);
				}

				/* 状態の取得 */
				svVal = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( (svVal != 0) && (svVal != 1) )
				{
					sprintf( errMsg, "CSV-Format Error (Target svVal). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* ON時判定時間の取得 */
				onCheckSec = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( onCheckSec < 0  )
				{
					sprintf( errMsg, "CSV-Format Error (Target onCheckSec). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}

				/* OFF時判定時間の取得 */
				offCheckSec = atoi( s_sepStr.data[csvOffset] );
				csvOffset++;
				if( offCheckSec < 0  )
				{
					sprintf( errMsg, "CSV-Format Error (Target offCheckSec). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
					commfunc_syslog_msgset( "FILEMAKE", errMsg );
					retCode = -1;
					break;
				}



				/* 装置種別の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].soKind = summKind;
				/* 装置番号の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].soNumb = summNumb;
				/* SV/STの設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svstKind = svstkind;
				/* 状態番号の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svstNumb = sumsvstNumb;
				/* 状態の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svVal = svVal;
				/* 状態情報リンク情報 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svIfLink = sumsvIfLink;
				/* リンク情報の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].stPoint = stPoint;
				/* リンク情報の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svstMask = svstMask;
				/* ON時判定時間の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].onCheckSec = onCheckSec;
				/* OFF時判定時間の設定 */
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].offCheckSec = offCheckSec;

#if 0
			printf("SVAGGINF : SUMINFO[%2d] FL:%2d, SK:%2d, SN:%2d, TN:%3d\n",
				loopCnt1+1,
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svIfLink,
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].soKind,
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].soNumb,
				ps_SVAGGINF[svAggLink].s_svaggdata[loopCnt1].svNumb);
#endif

			}
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
	munmap( ps_SVAGGINF, sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS);
	munmap( ps_SVDATINF, sizeof(struct st_SVDATINF) * PV_MAX_SVDATA);
	/* DATファイルをクローズ */
	close( fd_sum );
	close( fd_dat );
	/* 終了 */
	return( retCode );	
}

