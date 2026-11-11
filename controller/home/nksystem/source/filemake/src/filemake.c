/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 *   関連ファイルを追加
 * [20210128] 2020.01.28
 * ・受電電力計画値機能追加
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * オンラインファイル生成処理
 *
 * 主な機能：
 * 		ＣＳＶ形式のファイルからプロセスが使用するオンライン形式のファイルを生成
 * 		する。
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

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#include "filemake.h"

char	errMsg[PV_MSG_SIZE_LOG];							/* エラーメッセージ用 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインファイル生成処理主処理
 *
 * 機能：
 * 		ＣＳＶファイルからオンラインファイルを生成する各処理を呼び出す。
 */
int main()
{
	char					pvsyslogFile[1024];				/* ログファイルパス */
	int						fdLog;							/* ログファイルディスクリプタ */
	int						lineExist[PV_MAX_COMLINE];		/* 通信回線実装情報 */
	int						retCode;						/* 関数戻り値 */
	int						loopKind;						/* ループカウンタ */

	struct st_PVSYSLOG		*ps_PVSYSLOG;					/* ログファイルＭＭＡＰポインタ */

	struct st_linkPoint 	s_linkPoint;					/* 情報ファイルリンク位置保存 */
	struct st_linkPoint		s_allLinkPoint[PV_MAX_MKIND];	/* 情報ファイルリンク位置保存 */
	struct st_machineDef	s_machineDef[] =				/* 各装置後との最大機器、計測数定義 */
	{
		{	PV_MAX_PVSYS, PV_MAX_PVSYS_TMPOS, PV_MAX_PVSYS_SVPOS, ""					},	/* 00:収集装置本体 */
		{	PV_MAX_BATTE, PV_MAX_BATTE_TMPOS, PV_MAX_BATTE_SVPOS, PV_FNAME_BATTEINF		},	/* 01:バッテリー */
		{	PV_MAX_POWCN, PV_MAX_POWCN_TMPOS, PV_MAX_POWCN_SVPOS, PV_FNAME_POWCNINF		},	/* 02:パワーコンディショナ */
		{	PV_MAX_MLTMT, PV_MAX_MLTMT_TMPOS, PV_MAX_MLTMT_SVPOS, PV_FNAME_MLTMTINF		},	/* 03:マルチメータ */
		{	PV_MAX_REMIO, PV_MAX_REMIO_TMPOS, PV_MAX_REMIO_SVPOS, PV_FNAME_REMIOINF		},	/* 04:汎用リモートＩＯ装置 [20130412] Add */
		{	PV_MAX_PLCFA, PV_MAX_PLCFA_TMPOS, PV_MAX_PLCFA_SVPOS, PV_FNAME_PLCFAINF		},	/* 05:ＰＬＣ装置 [20150202] Add */
		{			   0, 				   0,                  0, ""					},	/* 06:予約 */
		{			   0, 				   0,                  0, ""					},	/* 07:予約 */
		{			   0, 				   0,                  0, ""					},	/* 08:予約 */
		{			   0, 				   0,                  0, ""					},	/* 09:予約 */
		{	-1,           -1,                 -1,                 ""					}	/* xx:終端 */
	};

	/* ファイルアクセス用ミューテックスをアタッチ */
	commfunc_mutex_attach();

	/* 状態・情報ポインタを初期化 */
	memset( &s_linkPoint, 0, sizeof(s_linkPoint));
	memset( &s_allLinkPoint, 0, sizeof(s_allLinkPoint));

	/* システムログファイルパスの展開 */
	strcpy(pvsyslogFile, PV_PATH_DAT );
	strcat(pvsyslogFile, PV_FNAME_PVSYSLOG );
	strcat(pvsyslogFile, ".dat");

	/* システムログファイルを開く */
	fdLog = open( pvsyslogFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	if( fdLog == -1 )
	{
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* ファイルをメモリマップ */
	ps_PVSYSLOG = (struct st_PVSYSLOG *)mmap( NULL, sizeof(struct st_PVSYSLOG), PROT_READ | PROT_WRITE, MAP_SHARED, fdLog, 0 );
	if( ps_PVSYSLOG == MAP_FAILED)
	{
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* グローバルアクセス用アドレス設定 */
	ps_gpt_PVSYSLOG = (struct st_PVSYSLOG *)ps_PVSYSLOG;

	/* 通信回線情報ファイル作成を実施 */
	if( (retCode = func_COMLIINF_Create( lineExist )) <= 0 )
	{
		if( retCode == 0 )
		{
			commfunc_syslog_msgset( "FILEMAKE", "No Data (COMLINE-DATA)" );
		}
		else if( retCode < 0 )
		{
			commfunc_syslog_msgset( "FILEMAKE", "File Create Error COMLIINF.csv -> COMLIINF.dat" );
			/* ファイルアクセス用ミューテックスをデタッチ */
			commfunc_mutex_detach();
			return( -1 );
		}
	}

	/* 収集装置を実装に設定 */
	s_allLinkPoint[PV_MKIND_PVSYS].statCnt    = 1;
	s_allLinkPoint[PV_MKIND_PVSYS].maxMachine = s_machineDef[PV_MKIND_PVSYS].maxMNumb;
	s_allLinkPoint[PV_MKIND_PVSYS].maxTm      = s_machineDef[PV_MKIND_PVSYS].maxTmNumb;
	s_allLinkPoint[PV_MKIND_PVSYS].maxSv      = s_machineDef[PV_MKIND_PVSYS].maxSvNumb;
	s_allLinkPoint[PV_MKIND_PVSYS].statFlag[0] = (unsigned char)0x80;

	/* 次の装置向けの情報として状態・情報ポインタを設定 */
	s_linkPoint.tmIfLink = s_machineDef[PV_MKIND_PVSYS].maxTmNumb;
	s_linkPoint.stStLink = 0;
	s_linkPoint.svIfLink = s_machineDef[PV_MKIND_PVSYS].maxSvNumb;
	s_linkPoint.svStLink = (s_machineDef[PV_MKIND_PVSYS].maxSvNumb - 1) / 8 + 1;

#if 0 
	printf("    PVSYS RESULT : %2d , POINT : TM[I:%06d] , ST[S:%06d] , SV[I:%06d/S:%06d]\n",
		s_allLinkPoint[PV_MKIND_PVSYS].statCnt,
		s_allLinkPoint[PV_MKIND_PVSYS].tmIfLink, s_allLinkPoint[PV_MKIND_PVSYS].stStLink,
		s_allLinkPoint[PV_MKIND_PVSYS].svIfLink, s_allLinkPoint[PV_MKIND_PVSYS].svStLink );
#endif

	/* 装置種別毎の情報設定 */
	for( loopKind = 1 ; loopKind <= PV_MKIND_LAST ; loopKind++ )
	{
		/* 状態・情報ポインタを設定 */
		s_allLinkPoint[loopKind].maxMachine = s_machineDef[loopKind].maxMNumb;
		s_allLinkPoint[loopKind].maxTm      = s_machineDef[loopKind].maxTmNumb;
		s_allLinkPoint[loopKind].maxSv      = s_machineDef[loopKind].maxSvNumb;
		s_allLinkPoint[loopKind].tmIfLink   = s_linkPoint.tmIfLink;
		s_allLinkPoint[loopKind].stStLink   = s_linkPoint.stStLink;
		s_allLinkPoint[loopKind].svIfLink   = s_linkPoint.svIfLink;
		s_allLinkPoint[loopKind].svStLink   = s_linkPoint.svStLink;
		/* 実装/未実装情報はクリア */
		memset( s_linkPoint.statFlag, 0, sizeof(s_linkPoint.statFlag));
		/* 装置種別毎の情報ファイルを生成 */
		if( loopKind == PV_MKIND_BATTE )
		{
			/* 装置種別:バッテリー情報ファイルの生成 */
			retCode = func_BATTEINF_Create( lineExist, &s_linkPoint );
		}
		else if( loopKind == PV_MKIND_POWCN )
		{
			/* 装置種別:パワーコンディショナ情報ファイルの生成 */
			retCode = func_POWCNINF_Create( lineExist, &s_linkPoint );
		}
		else if( loopKind == PV_MKIND_MLTMT )
		{
			/* 装置種別:マルチメータ装置情報ファイルの生成 */
			retCode = func_MLTMTINF_Create( lineExist, &s_linkPoint );
		}
		else if( loopKind == PV_MKIND_REMIO )
		{
			/* 装置種別:汎用リモートＩＯ装置情報ファイルの生成 */
			retCode = func_REMIOINF_Create( lineExist, &s_linkPoint );
		}
		else if( loopKind == PV_MKIND_PLCFA )
		{
			/* 装置種別:ＰＬＣ装置情報ファイルの生成 */
			retCode = func_PLCFAINF_Create( lineExist, &s_linkPoint );
		}
		else
		{
			retCode == 0;
		}

		/* 生成失敗の場合 */
		if( retCode == -1 )
		{
			sprintf( errMsg, "File %s.dat Create Error", s_machineDef[loopKind].fileName );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			/* ファイルアクセス用ミューテックスをデタッチ */
			commfunc_mutex_detach();
			return( -1 );
		}
		/* 未実装の場合 */
		else if( retCode == 0 )
		{
			memset( &s_allLinkPoint[loopKind], 0, sizeof(struct st_linkPoint) );
		}
		/* 実装の場合 */
		else
		{
			/* 機器数を格納 */
			s_allLinkPoint[loopKind].statCnt = retCode;
			/* 実装/未実装情報をコピーする */
			memcpy( s_allLinkPoint[loopKind].statFlag, s_linkPoint.statFlag, sizeof(s_linkPoint.statFlag) );
		}
#if 0
		printf("    MKIND[%d]  RESULT : %2d , POINT : TM[I:%06d] , ST[S:%06d] , SV[I:%06d/S:%06d]\n",
			loopKind,
			s_allLinkPoint[loopKind].statCnt,
			s_allLinkPoint[loopKind].tmIfLink, s_allLinkPoint[loopKind].stStLink,
			s_allLinkPoint[loopKind].svIfLink, s_allLinkPoint[loopKind].svStLink );
#endif
	}

#if 0 
	printf("    LAST  RESULT :      POINT : TM[I:%06d] , ST[S:%06d] , SV[I:%06d/S:%06d]\n",
		s_linkPoint.tmIfLink, s_linkPoint.stStLink,
		s_linkPoint.svIfLink, s_linkPoint.svStLink );
#endif

	/* 各種ポインタがシステムの最大数を超過している場合 */
	if( ( s_linkPoint.tmIfLink > PV_MAX_TMDATA ) ||
		( s_linkPoint.svIfLink > PV_MAX_SVDATA ) ||
		( s_linkPoint.svStLink > ((PV_MAX_SVDATA-1)/8+1) ) ||
		( s_linkPoint.stStLink > ((PV_MAX_STDATA-1)/8+1) ) )
	{
		sprintf( errMsg, "ERROR: LINK Pointer Limit Over (TMIF[%d/%d] SVIF[%d/%d] SVST[%d/%d] STST[%d/%d])",
			s_linkPoint.tmIfLink, PV_MAX_TMDATA, s_linkPoint.svIfLink, PV_MAX_SVDATA,
			s_linkPoint.svStLink,  ((PV_MAX_SVDATA-1)/8+1), s_linkPoint.stStLink, ((PV_MAX_STDATA-1)/8+1) ); 

		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}


	/*==========================================*/
	/* ＴＭ情報・ＳＶ情報・ＳＴ情報ファイル生成 */
	/*==========================================*/
	/* 計測情報ファイル作成を実施 */
	if( (retCode = func_TMDATINF_Create( s_allLinkPoint, s_machineDef )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File TMDATINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 装置状態情報・装置状態情報状変マスク情報ファイル作成を実施 */
	if( (retCode = func_SVDATINF_Create( s_allLinkPoint, s_machineDef )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File SVDATINF.dat SVMSKINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 集約計測情報ファイル作成を実施 */
	if( (retCode = func_TMAGGINF_Create( s_allLinkPoint, s_machineDef )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File TMAGGINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 集約状態情報ファイル作成を実施 */
	if( (retCode = func_SVAGGINF_Create( s_allLinkPoint, s_machineDef )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File SVAGGINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* システム管理ファイル作成を実施 */
	if( (retCode = func_PVSYSINF_Create( s_allLinkPoint )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File PVSYSINF.dat Create Error"  );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* TM監視情報ファイル作成を実施 */
	if( (retCode = func_TMMONINF_Create( s_allLinkPoint, s_machineDef )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File TMMONINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* モニタリングサーバ情報ファイル作成を実施 */
	if( (retCode = func_PVSVRINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File PVSVRINF.dat Create Error"  );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

/* [20230529] Add St */
	/* IOT設定情報ファイル作成を実施 */
	if( (retCode = func_IOTCNINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File IOTCNINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20230529] Add Ed */

	/* 通信ステータス情報・通信ステータス情報状変マスク情報ファイル作成を実施 */
	if( (retCode = func_STDATINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File STDATINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* CEMS制御情報ファイル作成を実施 */
	if( (retCode = func_CEMSPINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File CEMSPINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* デマンド監視情報ファイル作成を実施 */
	if( (retCode = func_DEMNDINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File DEMNDINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 過充電回避制御情報ファイル作成を実施 */
	if( (retCode = func_OVRCHINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File OVRCHINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 受電電力一定制御情報ファイル作成を実施 */
	if( (retCode = func_RCVPWINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File RCVPWINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* モード切替時制御情報ファイル作成を実施 */
	if( (retCode = func_MDCTLINF_Create( s_allLinkPoint, s_machineDef )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File MDCTLINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

/* [20230529] Add St */
	/* 電力会社向け情報ファイル作成を実施 */
	if( (retCode = func_PGCONINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File PGCONINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20230529] Add Ed */

/* [20250807] Add St */
	/* 一次調整力制御パラメータ設定情報ファイル作成を実施 */
	if( (retCode = func_FCRSTINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File FCRSTINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
	
	/* 一次調整力レポート蓄積項目情報ファイル作成を実施 */
	if( (retCode = func_FCRRPINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File FCRRPINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20250807] Add Ed */

	/*==================*/
	/* 状態ファイル生成 */
	/*==================*/

	/* 計測情報保存保ファイル */
	if( (retCode = func_TMSTSFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File TMSTSFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 通信ステータス状態保存ファイル */
	if( (retCode = func_STSTSFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File STSTSFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 装置状態保存ファイル */
	if( (retCode = func_SVSTSFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File SVSTSFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 状変蓄積ファイル */
	if( (retCode = func_JOHENFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File JOHENFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 制御状変蓄積ファイル */
	if( (retCode = func_CTLJHFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File CTLJHFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 計測値蓄積ファイル */
	if( (retCode = func_TMSTOFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File TMSTOFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 計測情報蓄積ファイル */
	if( (retCode = func_TMRECFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File TMRECFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 状態情報蓄積ファイル */
	if( (retCode = func_SVRECFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File SVRECFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

/* [20260515] Add St */
	/* 実績データ蓄積ファイル */
	if( (retCode = func_RPRECFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File RPRECFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20260515] Add Ed */

	/* 集約状態保存ファイル */
	if( (retCode = func_SVAGGFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File SVAGGFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 制御状態保存ファイル */
	if( (retCode = func_PVCTLFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File PVCTLFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* 遠隔制御情報保存ファイル */
	if( (retCode = func_RTCTLINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File RTCTLINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

/* [20210128] Add St */
	/* 受電電力計画値情報ファイル作成を実施 */
	if( (retCode = func_PWPLNINF_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File PWPLNINF.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20210128] Add Ed */

	/* デマンド制御保存ファイル */
	if( (retCode = func_DEMNDFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File DEMNDFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

	/* エネルギーマネジメント管理ファイル */
	if( (retCode = func_EMCTLFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File EMCTLFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}

/* [20210128] Add St */
	/* 受電電力計画値保存ファイル作成を実施 */
	if( (retCode = func_PWPLNFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File PWPLNFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20210128] Add Ed */

/* [20230529] Add St */
	/* スケジュール管理情報保存ファイル */
	if( (retCode = func_SCHCTLFIL_Create( )) < 0 )
	{
		commfunc_syslog_msgset( "FILEMAKE", "File SCHCTLFIL.dat Create Error" );
		/* ファイルアクセス用ミューテックスをデタッチ */
		commfunc_mutex_detach();
		return( -1 );
	}
/* [20230529] Add St */

	/* メモリマップを解除 */
	munmap( ps_PVSYSLOG, sizeof(struct st_PVSYSLOG) );
	/* システムログファイルをクローズ */
	close( fdLog );

	/* グローバルアクセス用アドレス解除 */
	ps_gpt_PVSYSLOG = NULL;

	/* ファイルアクセス用ミューテックスをデタッチ */
	commfunc_mutex_detach();

	return( 0 );
}

