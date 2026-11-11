/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251120] 2025.11.20 Ph5品証指摘No.37対応
 * ・スケジュールなし(-1)がスケジュール管理情報に更新されない問題の対応
 * [20251117] 2025.11.17 Ph5品証指摘No.20対応
 * ・通信異常にて計測情報が欠測ならない不具合の対応
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 出力制御ユニット用ＰＬＣ通信プロセス
 * （通信プロトコル１：ＭｏｄｂｕｓＴＣＰ通信タイプ）
 *
 * 主な機能
 * 		（１）通信機能
 * 		起動時に指定された通信回線番号に接続されたＰＬＣと通信し
 * 		計測値と装置状態の情報を収集する。
 * 		情報収集は「４８５通信回線情報ファイル（COMLIINF.dat）」と「ＰＬＣ装置情
 *		報ファイル（PLCFAINF.dat）」の設定を元に行う。
 * 		通信は、前述の設定ファイルの内容に従い定周期に行う。
 *
 * 		（２）情報の取得および管理
 * 		収集した情報は、計測情報の場合は「計測値保存ファイル（TMSTSFIL.dat）」に
 * 		行い、状態情報は「装置状態ファイル（SVSTSFIL.dat）」に行う、なお、ＰＬＣ
 * 		との通信時に発生した通信の状態は
 *		「通信ステータス状態ファイル（STSTSFIL.dat）」に保存する。
 * 		なお、計測値に関しては瞬時値の他に単純移動平均（過去１分間分）の演算を行
 * 		い、瞬時値と同様に「計測状態ファイル（TMSTSFIL.dat）」に保存する。
 *
 * 		（３）状変処理
 * 		装置状態および通信ステータスに変化が生じた場合は、状態変化情報を状変情報
 * 		として「状変蓄積ファイル（JOHENFIL.dat）」に蓄積する。
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvctldata.h"
#include "pvcomm.h"

#include "schplc1.h"

/* デバッグフラグ */
#if 0
#define	PV_DEBUG_SERIAL
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_SV									/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_TM									/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_DATA_CHECK							/* デバッグフラグ：スケジュールデータチェック表示 */
#endif

#define	PV_OWN_NAME			"SCHPLC1"				/* 自プログラム名 */

/* ４８５回線で使用するプロトコル名 */
#define	PV_COMPROTOCOL		PV_COMPRO_SCHPLC1		/* 出力制御ユニットタイプ１（MODBUS-RTU） */

/* ＭＭＡＰアクセス用ポインタ変数 */
struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* ４８５通信回線情報ファイルポインタ */
struct st_PLCFAINF		*ps_g_PLCFAINF;				/* ＰＬＣ情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */
struct st_PVCTLFIL      *ps_g_PVCTLFIL;             /* 制御状態保存ファイルポインタ [20180712] Add */
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL;				/* スケジュール管理情報ファイルポインタ */

/* ファイルインデックス（本プロセスで使用するファイルの登録） */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_PLCFAINF, (void **)&ps_g_PLCFAINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{   PV_FILENO_PVCTLFIL, (void **)&ps_g_PVCTLFIL, 0, 0, 0 }, /* [20180712] Add */
	{	PV_FILENO_SCHCTLFIL, (void **)&ps_g_SCHCTLFIL, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0,}
};

static char							pv_own_name[128];			/* 自プロセス情報 */
static char							errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int							plcMaxCnt = 0;				/* 処理対象バッテリー数 */
static struct st_serialinf			s_serial[PV_MAX_PLCFA];		/* シリアル回線情報 */
static struct st_stdDevInfo			s_plcInfo[PV_MAX_PLCFA];	/* バッテリー情報格納領域 */
static struct st_plcData			s_plcData[PV_MAX_PLCFA];	/* 計測情報（瞬時値格納用） */
static struct st_threadArg			s_threadArg[PV_MAX_PLCFA];	/* スレッド管理情報 */
static struct st_plcAvlData			s_plcAvlData[PV_MAX_PLCFA];	/* 計測情報（移動平均演算用） */
static int							threadStop = 0;				/* スレッドストップフラグ */
static struct st_plcSchDataCheck	s_plcSchData[PV_MAX_PLCFA];	/* スケジュールデータ管理 *//* [20251027] Add */
/* 内部関数定義 */
void	*func_plcOpen_thread( void *args );
int		func_plcData_recv( struct st_serialinf *, int, int, int, int, int, unsigned char *, struct st_serialLog * );
int		func_plcData_get( int, int, int, int, struct st_plcData *, unsigned char * );
int		func_abnormal_exit( int );


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 出力制御ユニット蓄電池制御用ＰＬＣ通信プロセス主処理
 *
 * 機能：
 * 		引数で指定された通信回線番号を元に４８５通信回線情報ファイル
 * 		（COMLIINF.dat）から通信回線の設定情報を取得する。
 * 		引数で指定された通信回線番号を元にＰＬＣ情報ファイル
 * 		（PLCFAINF.dat）から通信回線番号が一致する装置の情報を取得する。
 * 		ファイルに設定された周期でＰＬＣと通信を行い、情報を取得し
 * 		て編集結果を指定されたファイルに保存する。
 * 		管理プロセスから停止指示が出た場合はプロセスを終了する。
 *
 */
int main(int argc, char *argv[])
{
														/* ＰＬＣへの送信コマンドベース */
	char					lineNo;						/* 通信回線番号 */
	char					ip4Data[12];				/* ＩＰアドレス第四オクテット */
	char					*charWorkPt;				/* ワークポインタ */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	pthread_t				threadID[PV_MAX_PLCFA];		/* スレッドＩＤ */
	int						requestCnt = 0;				/* 合計要求回数 */
	int						minDataMax;					/* 移動平均データ最大数 */
	int						plcNumb;					/* 処理中のＰＬＣ番号 */
	int						threadJoinCnt;				/* スレッド終了カウント */
	time_t					fileTime=0;					/* ファイル更新時刻 */
	char					extdataPathSchSync[128];			/* 外部データパス */
	char					extdataPathNonPer[128];		/* 外部データパス(スケジュールなし時の抑制値) */
	int						retFileCode;				/* 関数戻り値格納用 */
	char					getStr[128];				/* 取得文字列 */

	/* プロセス起動時の引数から通信回線番号を取得 */
	if( argc != 2 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("Usage : %s lineno\n", argv[0]);
		return( -1 );
	}
	else
	{
		/* 通信回線番号を取得 */
		lineNo = (char)atoi(argv[1]);
		/* 通信回線番号が既定値範囲外の場合 */
		if( (lineNo < 1) || (lineNo > PV_MAX_COMLINE) )
		{
			printf("Usage : %s lineno\n", argv[0]);
			return( -1 );
		}
	}

	/* 自プロセス情報を設定 */
	sprintf( pv_own_name, "%s(%02d)", PV_OWN_NAME, lineNo );

	/* ファイルをオープンしてＭＭＡＰ化する */
	retCode = commfunc_file_open( s_fileIndex );
	if( retCode < 0 )
	{
		printf("%s : File Open Error\n", pv_own_name);
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_SET );

	/* シリアル回線情報を初期化 */
	memset( s_serial, 0, sizeof(struct st_serialinf) );
	/* データ格納領域を初期化 */
	memset( &s_plcData, 0, sizeof(s_plcData));
	/* スレッド管理情報を初期化 */
	memset( s_threadArg, 0, sizeof(s_threadArg));
	/* スケジュール管理情報初期化 */
	memset( s_plcSchData, 0, sizeof(s_plcSchData));	/* [20251027] Add */

	/* 通信回線情報を取得 */
	if( (retCode = commfunc_serial_infoget( lineNo, &s_serial[0], ps_g_COMLIINF, PV_COMPROTOCOL )) != 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [485LINLIF.csv] Information Get Error Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

	/* ＰＬＣ情報を取得 */
	if( (plcMaxCnt = commfunc_devInfo_get( PV_MKIND_PLCFA, lineNo, s_plcTypeName, s_plcInfo, &requestCnt, NULL, NULL, ps_g_PLCFAINF )) <= 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [PLCFAINF.csv] Not Exist Target Machine of Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

#ifdef PV_DEBUG
	printf("%s : PLCMAXCNT : %d\n", pv_own_name, plcMaxCnt );
#endif

	/* シリアル情報にあるデバイス名（IPアドレス）の第4オクテットデータを消去する */
	if( ( charWorkPt = strrchr( s_serial[0].targetName, '.' ) ) != NULL )
	{
		charWorkPt++;
		memset( charWorkPt, 0, 10);
	}

	/* 複数パワコン対応処理 */
	for( loopCnt1 = 1 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
	{
		/* シリアル回線情報をコピーする */
		memcpy( &s_serial[loopCnt1], &s_serial[0], sizeof(struct st_serialinf) );
	}

	/* 複数対応処理 */
	for( loopCnt1 = 0 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
	{
		/* IPアドレスの第4オクテットを装置IDを使用して設定 */
		sprintf( ip4Data, "%d", s_plcInfo[ loopCnt1 ].idNumb );
		strcat( s_serial[loopCnt1].targetName, ip4Data );

#ifdef PV_DEBUG
	printf("%s : PLC:%02d IP-ADDRESS:%s\n", pv_own_name, s_plcInfo[loopCnt1].devNumb, s_serial[loopCnt1].targetName );
#endif
		/* スレッド管理情報を設定 */
		s_threadArg[loopCnt1].indexNumb = loopCnt1;

	}

	/* 収集周期の調整と移動平均算出用データ数の取得 */
	commfunc_serial_cycletmchk( &s_serial[0], &minDataMax, ps_g_COMLIINF );

	/* 移動平均用データの初期化 */
	commfunc_devAvlData_init( PV_MKIND_PLCFA, minDataMax, s_plcAvlData );

	/* 移動平均用データから積算型計測値を復元する */
	retCode = commfunc_tmstsfil_get( PV_MKIND_PLCFA, plcMaxCnt, PV_SCHPLC1_MAXTM, s_plcInfo, s_plcData, s_plcAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );

	/* ＰＬＣの装置状態を復元する */
	for( loopCnt1 = 0 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
	{
		/* ＰＬＣ番号の取得 */
		plcNumb = s_plcInfo[loopCnt1].devNumb;
		/* ＰＬＣの装置状態を復元する */
		commfunc_sv_get( PV_MKIND_PLCFA, plcNumb, PV_MAX_PLCFA_SVPOS, s_plcData[loopCnt1].plcStatus, ps_g_SVSTSFIL );

		/* 外部データファイルを設定 */
		sprintf(extdataPathSchSync, "%s%s_%d.txt", PV_PATH_EXT, PV_OWN_NAME, plcNumb );

		/* 設定ファイルのチェック */
		retCode = commfunc_file_info_get( extdataPathSchSync, &s_plcSchData[loopCnt1].fileTime, "SYNC_CNT", getStr );

		if( retCode == COM_PARAM_GET_OK )
		{
			s_plcSchData[loopCnt1].checkNum = atoi(getStr);
		}
		else
		{
			s_plcSchData[loopCnt1].checkNum = PV_SCHPLC1_SYNC_NUM;
		}
		sprintf( errMsg, "Idx[%d] : checkNum=%d, fileTime=%lu", loopCnt1+1, s_plcSchData[loopCnt1].checkNum, s_plcSchData[loopCnt1].fileTime);
#ifdef PV_DEBUG_DATA_CHECK
		printf("%s : %s\n", pv_own_name, errMsg);
#endif
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_RESET );

	/* 当該通信回線に所属するバッテリーの台数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
	{

#ifdef PV_DEBUG
		printf("%s : PLC[%02d] OPEN-Thread EXECUTE\n", pv_own_name, s_plcInfo[loopCnt1].devNumb );
#endif
		/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
		s_threadArg[loopCnt1].stFlg = 1;
		if( pthread_create( &threadID[loopCnt1], NULL, func_plcOpen_thread, &s_threadArg[loopCnt1] ) != 0 )
		{

#ifdef PV_DEBUG
printf("%s : PLC[%02d] OPEN-Thread EXECUTE Error\n", pv_own_name, s_plcInfo[loopCnt1].devNumb );
#endif
			sprintf( errMsg, "Thread Execute Error" );
			commfunc_syslog_msgset( pv_own_name, errMsg );

			/* プロセス異常終了処理 */
			func_abnormal_exit( lineNo );
			return( -1 );
		}
	}
/* [20251120] Add St */
	/* 外部データファイルを設定 */
	sprintf(extdataPathNonPer, "%s%s.txt", PV_PATH_EXT, PV_OWN_NAME );

	/* スケジュール未登録時の出力制御値(%)を100%で更新 */
	ps_g_SCHCTLFIL[PV_SCH_TYPE_BATTERY].schNonPer = 100;
/* [20251120] Add Ed */


	while( 1 )
	{
/* [20251120] Add St */
		/* 設定ファイルのチェック */
		retCode = commfunc_file_info_get( extdataPathNonPer, &fileTime, "SCH_NON_PER", getStr );

		if( retCode == COM_PARAM_GET_OK )
		{
			/* スケジュール未登録時の出力制御値(%)を更新 */
			ps_g_SCHCTLFIL[PV_SCH_TYPE_BATTERY].schNonPer = atoi(getStr);
			sprintf( errMsg, "schNonPer=%d, fileTime=%lu", ps_g_SCHCTLFIL[PV_SCH_TYPE_BATTERY].schNonPer, fileTime);
#ifdef PV_DEBUG_DATA_CHECK
			printf("%s : %s\n", pv_own_name, errMsg);
#endif
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
/* [20251120] Add Ed */

		/* 当該通信回線に所属するバッテリーの台数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
		{
			/* 設定ファイルのチェック */
			retCode = commfunc_file_info_get( extdataPathSchSync, &s_plcSchData[loopCnt1].fileTime, "SYNC_CNT", getStr );
			/* ファイル更新があった場合 */
			if( retCode == COM_PARAM_GET_OK )
			{
				s_plcSchData[loopCnt1].checkNum = atoi(getStr);
				sprintf( errMsg, "Idx[%d] : checkNum=%d, fileTime=%lu", loopCnt1+1, s_plcSchData[loopCnt1].checkNum, s_plcSchData[loopCnt1].fileTime);
#ifdef PV_DEBUG_DATA_CHECK
				printf("%s : %s\n", pv_own_name, errMsg);
#endif
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
		}

		/* 収集時刻になるまで待機する */
		if( commfunc_mswait( s_serial[0].cycleTime, lineNo ) < 0 )
		{
#ifdef PV_DEBUG
			printf("%s : PROCESS STOP COMMAND RECV (LINE:%d)\n", pv_own_name, lineNo );
#endif
			break;
		}
	}

	/* スレッドの終了を待つ（3秒ｘ5回） */
	threadStop = 1;
	for(loopCnt2 = 0 ; loopCnt2 < 5; loopCnt2++)
	{
		threadJoinCnt = 0;
		/* 当該通信回線に所属するマルチメータの台数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
		{
			/* スレッドが終了していない場合 */
			if( threadID[loopCnt1] != 0 )
			{
				/* スレッドの終了を待つ */
				if( pthread_join( threadID[loopCnt1], NULL ) == 0 )
				{
					threadID[loopCnt1] = 0;
					threadJoinCnt++;
				}
			}
			else
			{
				threadJoinCnt++;
			}
		}
		/* 全てのスレッドの終了を回収した場合 */
		if( threadJoinCnt == plcMaxCnt )
		{
			break;
		}
		sleep(3);
	}

	/* スレッドが終了していない場合 */
	if( threadJoinCnt != plcMaxCnt )
	{
		/* 強制終了を行う */
		/* 当該通信回線に所属するマルチメータの台数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < plcMaxCnt ; loopCnt1++ )
		{
			/* スレッドが終了していない場合 */
			if( threadID[loopCnt1] != 0 )
			{
				sprintf( errMsg, "pthread_detach (tid=%d)", threadID[loopCnt1]	 );
				/* スレッドのリソースをデタッチする */
				retCode = pthread_detach( threadID[loopCnt1] );
				if( retCode != 0 )
				{
					sprintf( errMsg, "pthread_detach Error(%d)", retCode );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
			}
		}
	}

	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_RESET );
	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * バッテリーＴＣＰ／ＩＰ通信接続処理
 *
 * 機能：
 *		引数 args で指定された情報内に格納されているバッテリーに対
 *		して、接続を行う（オープンからコネクトまで）
 *		実際の接続処理は共通関数 commfunc_serial_open() を使用する。
 *		
 */
void *func_plcOpen_thread( void *args )
{
														/* ＰＬＣへの送信コマンドベース */
	unsigned  char				sendBuff[32];				/* 送信バッファ */
	int							dataLength;					/* 送信データ長 */
	unsigned char				*p_sendBuff;
	unsigned char				readBuff[1024];				/* 受信バッファ */
	int							openRetCode;				/* シリアルポートオープン結果 */
	int							retCode;					/* 関数戻り値格納用 */
	int							retCheck;					/* 整合性チェック結果 *//* [20251117] Add */
	int							loopCnt1;					/* ループカウンタ */
	int							loopCnt2;					/* ループカウンタ */
	int							loopCnt3;					/* ループカウンタ */
	int							plcNumb;					/* 処理中のＰＬＣ番号 */
	int							idNumb;						/* 処理中のＰＬＣ装置ＩＤ */
	struct st_serialLog			s_serialLog;				/* シリアル通信ログ格納領域 */
	struct timespec				sleepTm;					/* 送信待ちタイマ */
	struct st_plcPowSchSdInf	*ps_plcSdInf;				/* 送信情報ポインタ */
	int							workIdxNumb;				/* 処理インデックス番号 */
	struct st_threadArg			*ps_threadArg;				/* スレッド引数処理用 */
	struct timeval				nowTime;					/* 現在時刻 */
	struct tm					timeData;					/* 時刻情報 */
	int							tranID;						/* トランザクションID */
	struct st_plcData			plcData;					/* 計測情報 */

	ps_threadArg = (struct st_threadArg *)args;

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));

	/* 処理インデックス番号の取得 */
	workIdxNumb = ps_threadArg->indexNumb;

	/* パワコン番号の取得 */
	plcNumb = s_plcInfo[workIdxNumb].devNumb;

	/* 装置状態をコピー */
	memcpy(s_plcData[workIdxNumb].plcStatus, plcData.plcStatus, sizeof(s_plcData[workIdxNumb].plcStatus));

#ifdef PV_DEBUG
	printf("%s : IDX[%02d] PLC[%02d] OPEN-Thread START\n", pv_own_name, workIdxNumb, plcNumb );
#endif


	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = s_serial[workIdxNumb].sndTime / 1000;
	sleepTm.tv_nsec = (s_serial[workIdxNumb].sndTime % 1000) * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( threadStop == 0 )
	{
		/* シリアルポートをオープン */
		openRetCode = commfunc_serial_open( &s_serial[workIdxNumb] );
		if( openRetCode >= 0 )
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（復帰） */
			commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST14_LANERR, 0,
			    (ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );

			/* シリアルポートステータスを更新 */
			s_plcData[workIdxNumb].serialStatus = openRetCode;

		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST14_LANERR, 1,
			    (ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );

#ifdef PV_DEBUG
		printf("%s : IDX[%02d] PLC[%02d] Thread OPEN ERROR!!\n", pv_own_name, workIdxNumb, plcNumb );
#endif
			/* シリアルポートステータスを更新 */
			s_plcData[workIdxNumb].serialStatus = openRetCode;

			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 5 );
		}

		/* 要求実施回数を初期化 */
		s_serial[workIdxNumb].requestCount = 0;

		/* 処理タイプの設定 */
		if( s_plcInfo[workIdxNumb].devType == PV_SCHPLC1_TYPE1 )
		{
			/* 送信情報を処理タイプ１に設定 */
			ps_plcSdInf = s_type1SdInf;
		}
		else
		{
			/* 送信情報を処理タイプ１に設定 */
			ps_plcSdInf = s_type1SdInf;
		}

		/* 戻り値初期化 */
		retCode = 0;

		/* 複数回に分けて情報を収集する */
		for( loopCnt2 = 0 ; ps_plcSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
		{
#ifdef PV_DEBUG
			printf("%s : LOOP:%02d PLCNO:%02d PLCID-No:%02d REQUEST:%d OPN-STS:%d\n", pv_own_name, workIdxNumb+1, plcNumb, idNumb, loopCnt2+1, s_serial[workIdxNumb].status );
#endif

			/* 送信バッファの設定 */
			memset( sendBuff, 0, sizeof( sendBuff ));
			p_sendBuff = &sendBuff[0];

			/* 通信状態が正常の場合、且つ、前回の通信が正常の場合 */
			if( ( s_serial[workIdxNumb].status == PV_STAT_COM_CONNECT ) &&
				( retCode == 0 ) )
			{
				/* 送信バッファの初期化 */
				dataLength = ps_plcSdInf[loopCnt2].dataLength;				/* 送信データ長 */
				p_sendBuff[0] = idNumb;										/* トランザクションID（装置ID） */
				p_sendBuff[1] = loopCnt2+1;									/* トランザクションID（要求回数） */
				p_sendBuff[2] = 0;											/* プロトコルID（MODBUS=0を使用） */
				p_sendBuff[3] = 0;											/* プロトコルID（MODBUS=0を使用） */
				p_sendBuff[4] = (dataLength - 6) / 0x100;					/* データ長設定 */
				p_sendBuff[5] = (dataLength - 6) % 0x100;					/* データ長設定 */
				p_sendBuff[6] = 0x01;										/* 送信先局番 */
				p_sendBuff[7] = ps_plcSdInf[loopCnt2].funcCode;
				tranID = (p_sendBuff[0] * 0x100) + p_sendBuff[1];

				switch( ps_plcSdInf[loopCnt2].funcCode )
				{
					case PV_SCHPLC1_FUNC_03 :
						p_sendBuff[8] = ps_plcSdInf[loopCnt2].startAdU;		/* 開始アドレス */
						p_sendBuff[9] = ps_plcSdInf[loopCnt2].startAdL;		/* 開始アドレス */
						p_sendBuff[10] = ps_plcSdInf[loopCnt2].dataCntU;	/* レジスタ数/コイル数 */
						p_sendBuff[11] = ps_plcSdInf[loopCnt2].dataCntL;	/* レジスタ数/コイル数 */
						p_sendBuff[12] = p_sendBuff[11] * 2;
						break;

					default :
						continue;
						break;
				}

				/* 送信待ち処理 */
				nanosleep( &sleepTm, NULL);

				/* 処理結果の初期値を異常に設定 */
				retCode = -1;

				/* 要求実施回数の上限まで繰り返す */
				/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
				while( s_serial[workIdxNumb].requestCount < PV_SERIAL_REQUEST_MAX )
				{
					/* シリアル通信ログ領域を初期化 */
					s_serialLog.dataCnt = 0;

					/* ＰＬＣに要求を送信 */
					retCode = commfunc_serial_send( &s_serial[workIdxNumb], p_sendBuff, dataLength, &s_serialLog );
					if( retCode < 0 )
					{
						sprintf( errMsg, "TCP Port Write Error! RET:%d num/id:%02d/%02d (%s)",
							retCode, plcNumb, idNumb, s_serial[workIdxNumb].targetName );
						commfunc_syslog_msgset( pv_own_name, errMsg );

						/* データ送信異常（送信不可異常）発生 */
						/* 状変（通信ステータス状変）発生 */
						commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST17_NOTSENDERR, 1,
							(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );

						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* データ送信異常（送信不可異常）復帰 */
						/* 状変（通信ステータス状変）復帰 */
						commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST17_NOTSENDERR, 0,
							(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
					}
	
					/* データ処理バッファをクリア */
					memset(readBuff, 0, sizeof(readBuff));
	
					/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
					retCode = func_plcData_recv(	&s_serial[workIdxNumb], tranID, plcNumb, idNumb,
													ps_plcSdInf[loopCnt2].funcCode, ps_plcSdInf[loopCnt2].dataCntL,
													readBuff, &s_serialLog );
					if( retCode == 0 )
					{
						/* 正常データ受信完了 */
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_AUTO, &s_serial[workIdxNumb], &s_serialLog );
						/* 通信ステータス状変：全ステータス（復帰） */
						commfunc_johen_resetstat( PV_MKIND_PLCFA, plcNumb,
							(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );

						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_ON, &s_serial[workIdxNumb], &s_serialLog );
						/* タイムアウト以外の場合は再要求不要のため処理を抜ける */
						if( retCode != PV_SERIAL_ERRTIMEOUT )
						{
							break;
						}
					}
				}

			}
			else
			{
#ifdef PV_DEBUG_SERIAL
				printf("%s : SERIAL POERT NOT-OPENED STATUS[%d] stFlg[%d]\n", pv_own_name, s_serial[workIdxNumb].status, s_threadArg[workIdxNumb].stFlg );
#endif
				/* シリアルポートが異常の場合 */
				retCode = -1;
			}

			/* 読み込み処理のみ計測情報を格納する */
			if( ps_plcSdInf[loopCnt2].funcCode == PV_SCHPLC1_FUNC_03 )  /* [20251117] Chg */
			{
				/* データ処理バッファから装置状態と計測値を抽出 */
				func_plcData_get( workIdxNumb, loopCnt2 + 1, plcNumb, retCode, &plcData, readBuff);
				/* 装置状態情報は反映 */
				memcpy(s_plcData[workIdxNumb].plcStatus, plcData.plcStatus, sizeof(s_plcData[workIdxNumb].plcStatus));
			}
		}

/* [20251117] Cht St */
		/* 整合性チェック結果初期化 */
		retCheck = 0;
		/* データ整合性チェック */
		for( loopCnt1 = 0; loopCnt1 < PV_SCHPLC1_MAXTM; loopCnt1++ )
		{
			/* 計測情報が一致しているかチェック */
			if( ( s_plcSchData[workIdxNumb].plcData.tmValData[loopCnt1] != plcData.tmValData[loopCnt1] ) ||
				( s_plcSchData[workIdxNumb].plcData.tmFlgData[loopCnt1] != plcData.tmFlgData[loopCnt1] ) )
			{
				retCheck = -1;
				sprintf(errMsg, "Idx[%d] : tmNumb[%d] preTmData(%d,%d), nowTmData(%d,%d)", workIdxNumb, loopCnt1+1,
					s_plcSchData[workIdxNumb].plcData.tmFlgData[loopCnt1], s_plcSchData[workIdxNumb].plcData.tmValData[loopCnt1],
					plcData.tmFlgData[loopCnt1], plcData.tmValData[loopCnt1] );
#ifdef PV_DEBUG_DATA_CHECK
				printf("%s : %s\n", pv_own_name, errMsg);
#endif
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
			/* 前回データとして退避 */
			s_plcSchData[workIdxNumb].plcData.tmValData[loopCnt1] = plcData.tmValData[loopCnt1];
			s_plcSchData[workIdxNumb].plcData.tmFlgData[loopCnt1] = plcData.tmFlgData[loopCnt1];
		}

		/* データ整合性チェックが一致している */
		if( retCheck == 0 )
		{
			/* 整合性チェックインクリメント */
			s_plcSchData[workIdxNumb].checkCnt++;
#ifdef PV_DEBUG_DATA_CHECK
			printf("%s : Idx[%d] : checkCnt=%d, checkNum=%d\n", pv_own_name, workIdxNumb, s_plcSchData[workIdxNumb].checkCnt, s_plcSchData[workIdxNumb].checkNum );
#endif

			/* 整合性チェック回数に到達した場合 */
			if( s_plcSchData[workIdxNumb].checkCnt >= s_plcSchData[workIdxNumb].checkNum )
			{
#ifdef PV_DEBUG_DATA_CHECK
				printf("%s : Idx[%d] : tmDataCopy\n", pv_own_name, workIdxNumb );
#endif
				/* チェック回数をリセット */
				s_plcSchData[workIdxNumb].checkCnt = 0;

				/* 計測フラグ、計測値をコピーする */
				memcpy(s_plcData[workIdxNumb].tmFlgData, s_plcSchData[workIdxNumb].plcData.tmFlgData, sizeof(s_plcData[workIdxNumb].tmFlgData));
				memcpy(s_plcData[workIdxNumb].tmValData, s_plcSchData[workIdxNumb].plcData.tmValData, sizeof(s_plcData[workIdxNumb].tmValData));

				/* 計測種別毎の保持 */
				(void)commfunc_devTmData_set( PV_MKIND_PLCFA, plcNumb, PV_MAX_PLCFA_TMPOS, &s_plcData[workIdxNumb], &s_plcAvlData[workIdxNumb], ps_g_TMDATINF );

				/* 計測情報を計測値格納ファイルに保存 */
				(void)commfunc_tmstsfil_devset( PV_MKIND_PLCFA, plcNumb, PV_MAX_PLCFA_TMPOS, &s_plcData[workIdxNumb], &s_plcAvlData[workIdxNumb], ps_g_TMDATINF, ps_g_TMSTSFIL );

				/* 通信処理が正常終了の場合 */
				if( retCode == 0 )
				{
					/* 現在時刻取得 */
					gettimeofday(&nowTime, NULL);

					/* 要求数分ループ */
					for( loopCnt1 = 0; loopCnt1 < PV_SCHPLC1_MAX_REQ; loopCnt1++ )
					{
						/* スケジュールデータ登録 */
						commfunc_set_sch_data( PV_SCH_TYPE_BATTERY, nowTime.tv_sec, &s_plcData[workIdxNumb].tmValData[48*loopCnt1]);

						/* 日付を1日ずらす */
						nowTime.tv_sec += 24*60*60;
					}
				}
			}
		}
		else
		{
			/* チェック回数をリセット */
			s_plcSchData[workIdxNumb].checkCnt = 0;
#ifdef PV_DEBUG_DATA_CHECK
			printf("%s : Idx[%d] : dataCheckError -> checkCntReset(%d)\n", pv_own_name, workIdxNumb, s_plcSchData[workIdxNumb] );
#endif
		}
/* [20251117] Cht Ed */

	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &s_serial[workIdxNumb] );

	/* スレッド終了フラグＯＮ */
	ps_threadArg->edFlg = 1;

#ifdef PV_DEBUG
	printf("%s : IDX[%02d] PLC[%02d] OPEN-Thread END\n", pv_own_name, workIdxNumb, plcNumb );
#endif

	return( NULL );

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ受信処理
 *
 * 機能：
 * 		シリアルポートからデータを受信し、引数 dataBuff に指定されたアドレスへ
 * 		受信データを保存する。
 * 		データ受信する際に、受信データが引数 targetID で指定された装置IDからの
 * 		データであるかチェックし、IDが異なるデータは破棄する。
 * 		引数 ps_serial 内で設定されている受信タイムアウトまで受信データを待つ。
 * 		データが正常に受信できない場合は、通信ステータス状態を更新する。
 * 戻り値：
 * 		 0：正常処理
 * 		0>：異常処理（シリアルポートアクセス不可、データ未受信等）
 *
 */
int	func_plcData_recv(
	struct st_serialinf *ps_serial,
	int tranID,
	int plcNumb,
	int targetID,
	int reqFunc,
	int reqCnt,
	unsigned char *dataBuff,
	struct st_serialLog *ps_serialLog )
{
	fd_set			rfds;					/* ファイルディスクリプタ集合 */
	unsigned char	readBuff[1024];			/* リードバッファ */
	char			flag_GOD = 0;			/* 受信完了フラグ */
	char			*workPoint;				/* データ格納バッファワークポインタ */
	int				crcData = 0;			/* CRCチェック */
	int				readCnt = 0;			/* データ受信バイト数 */
	int				totalLength = 0;		/* 受信データ量 */
	int				dataLength = 0;			/* 受信データ長 */
	int				checkLength = 0;		/* 受信データ長（期待値） */
	struct timeval	timedata;				/* 現在時刻取得用 */
	struct timeval	timeout;				/* 応答タイムアウト時刻 */
	int				retCode;				/* 関数戻り値 */
	int				loopCnt;				/* ループカウンタ */
	unsigned char	crcHi;					/* CRC上位データ */
	unsigned char	crcLo;					/* CRC下位位データ */


#ifdef PV_DEBUG
	printf("%s : Data-Recv TargetID: %d  START\n", pv_own_name, targetID);
#endif

	/* ファンクションコードから受信データサイズを決定する（期待値） */
	switch( reqFunc )
	{
		case PV_SCHPLC1_FUNC_03:
			checkLength = reqCnt * 2 + 9;
			break;

		default:
			return -1;
			break;
	}

	/* データバッファのアドレスをコピー */
	workPoint = dataBuff;

	/* 現在時刻の取得 */
	gettimeofday(&timedata, NULL);

	/* 応答タイムアウト時刻の設定 */
	timeout.tv_usec = timedata.tv_usec + (ps_serial->rcvTime % 1000) * 1000;
	if(timeout.tv_usec >= 1000000)
	{
		timeout.tv_sec  = timedata.tv_sec + 1 + (ps_serial->rcvTime / 1000);
		timeout.tv_usec %= 1000000;
	}
	else
	{
		timeout.tv_sec  = timedata.tv_sec + (ps_serial->rcvTime / 1000);
	}

	/* 以下の条件を満たすまでシリアルポートからデータを読み込む */
	/* ・受信予定のデータをすべて受信 */
	/* ・タイムアウト */
	readCnt = 0;
	while( (flag_GOD == 0) && (readCnt >= 0) )
	{
		/* シリアルポートからデータ読み出し */
		readCnt = commfunc_serial_recv(ps_serial, readBuff, &timeout, ps_serialLog );
		if( readCnt < 0 )
		{
			switch(readCnt)
			{
				case PV_SERIAL_ERRNOTREADY :
					sprintf( errMsg, "Serial Port Not Open! Numb:%02d Id:%02d (%s)", plcNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRNOTREADY );
					break;
				case PV_SERIAL_ERRTIMEOUT :
					sprintf( errMsg, "Read Timeout! DataLen:%d Numb:%02d Id:%02d (%s)", totalLength, plcNumb, targetID, ps_serial->targetName);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 要求実施回数を＋１ */
					ps_serial->requestCount += 1;
					/* 要求実施回数が上限に達している場合 */
					if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
					{
						/* 通信ステータス状変：通信不応答異常（発生） */
						commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST01_NONRESPONSE, 1,
							(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", plcNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRACCESS );
					break;
				default:
					break;
			}
			/* シリアルポート異常 */
			return( PV_SERIAL_ERROTHER );
		}
		totalLength += readCnt;

#ifdef PV_DEBUG_SERIAL
printf("%s : FD:%d : RECV:%3dByte : ", pv_own_name, ps_serial->fd_sockfd, readCnt );
#endif
		/* 受信データ数分繰り返す */
		for(loopCnt = 0 ; loopCnt < readCnt ; loopCnt++)
		{
#ifdef PV_DEBUG_SERIAL
printf("[%02X]", readBuff[loopCnt] );
#endif
			/* データバッファに受信データを格納 */
			*workPoint = readBuff[loopCnt];
			workPoint++;
			dataLength++;
			/* 9バイト分のデータを受信したら先頭データのチェックを行う */
			if( dataLength == 9 )
			{
				/* トランザクションIDの確認（データフレーム毎に固有の値を使用） */
				if( ( *dataBuff != (unsigned char)(tranID / 0x100) ) || ( *(dataBuff + 1) != (unsigned char)(tranID % 0x100) ) )		/* [20210407] Chg */
				{
#ifdef PV_DEBUG_SERIAL
printf("<TRAN-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv TransactionID Error! Numb:%02d Expect[00][%02X],Result[%02X][%02X]", plcNumb, tranID, *dataBuff, *(dataBuff+1) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* ファンクションコードが不一致の場合 */
				else if ( *(dataBuff+7) != (unsigned char)reqFunc ) 
				{
					sprintf( errMsg, "Data-Recv function Error! Numb:%02d Expect[%02X],Result[%02X]", plcNumb, reqFunc, *(dataBuff+7) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 例外レスポンスの場合 */
					if( (*(dataBuff+7) & (unsigned char)0x80) == (unsigned char)0x80 )
					{
						/* 受信データ長（期待値）を３バイトに設定 */
						checkLength = 9;
					}
					else
					{
						/* 受信データの初期化 */
						dataLength = 0;
					}
				}
				/* データ長をチェック */
				else if( ( (*(dataBuff+4)*256) + *(dataBuff+5) + 6 ) != checkLength )
				{
					sprintf( errMsg, "Data-Recv msgLength Error! Numb:%02d Expect[%02X],Result[%02X]", plcNumb, checkLength, ((*(dataBuff+4)*256) + *(dataBuff+5)) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
					break;
				}
			}
			/* 受信データが規程の長さに達した場合 */
			if( dataLength >= checkLength)
			{
#ifdef PV_DEBUG_SERIAL
printf("Data-Recv Compleat!!\n");
#endif
				/* データ受信完了フラグ設定 */
				flag_GOD = 1;
				/* read処理から抜ける */
				break;
			}
		}
#ifdef PV_DEBUG_SERIAL
printf("\n");
#endif
	}
	/* データ受信完了フラグがセットされている？ */
	if( flag_GOD == 1 )
	{
		/* 受信データが例外レスポンスの場合（ */
		if( checkLength == 9 )
		{
#ifdef PV_DEBUG
			printf("BATTPLC1 : Data-Poling Error\n");
#endif
			/* 例外コードが不正ファンクションの場合 */
			if( *(dataBuff+8) == (unsigned char)0x01 )
			{
				/* 通信ステータス状変：下りコマンド異常（発生） */
				commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST19_DWCOMMANDERR, 1,
					(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			if( *(dataBuff+8) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST22_DWPARAMERR, 1,
					(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ値の場合 */
			if( *(dataBuff+8) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST23_DWDATAERR, 1,
					(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが スレーブデバイス異常の場合 */
			if( *(dataBuff+8) == (unsigned char)0x04 )
			{
				/* 通信ステータス状変：下りデバイス異常（発生） */
				commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST16_DEVICEERROR, 1,
					(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST20_DWFORMATERR, 1,
					(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
		}
	}
	else
	{
		/* 要求実施回数を＋１ */
		ps_serial->requestCount += 1;
		/* 要求実施回数が上限に達している場合 */
		if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
		{
			/* 通信ステータス状変：通信不応答異常（発生） */
			commfunc_johen_stat( PV_MKIND_PLCFA, plcNumb, PV_ST01_NONRESPONSE, 1,
				(ps_g_PLCFAINF+plcNumb-1)->stStLink, ps_g_STSTSFIL );
		}
		/* タイムアウト */
		return( PV_SERIAL_ERRTIMEOUT );
	}
	/* 正常受信完了 */
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測情報取得処理
 *
 * 機能：
 * 		引数 plcNumb で指定されたＰＬＣについて以下の処理を行う。
 * 		引数 dataBuff で指定された受信バッファの内容から装置状態と計測値を取得する。
 * 		受信結果 resultCode が異常の場合は、計測値を０、計測ステータスを欠測に設定
 * 		する。
 * 		受信結果 resultCode が正常な場合は受信バッファのASCIIデータから装置状態と
 * 		計測値を取得し、計測ステータスを生成する。
 * 		装置状態が取得できた場合は、状態更新処理を行う。
 * 戻り値：
 * 		0：正常処理
 */
int	func_plcData_get( int workIdxNumb, int reqCnt, int plcNumb, int resultCode, struct st_plcData *ps_plcData, unsigned char *dataBuff)
{
	union {												/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char			dataByte[4];			/* データ格納用領域 */
		short int				data16bit;				/* 16bitデータ取得用 */
		unsigned short int		data16bitu;				/* 16bitデータ取得用（符号無し） */				
		int						data32bit;				/* 32bitデータ取得用 */
	} u_change;
	unsigned char				bitMask;				/* ビットマスク */
	int							tmNumb;					/* 計測情報番号 */
	int							codNumb;				/* コード番号 */
	int							stsNumb;				/* 状態番号 */
	int							workPoint;				/* 文字列開始ポインタ */
	int							loopCnt1;				/* ループカウンタ */
	int							loopCnt2;				/* ループカウンタ */
	int							loopCnt3;				/* ループカウンタ */
	int							tmIfLinkBase;			/* 計測情報ファイルリンク */
	int							svStLinkBase;			/* 状態情報ファイルリンク */
	int							svIfLinkBase;			/* 装置状態情報ファイルリンク */
	struct st_plcPowSchSvInf	*ps_plcSvInf;			/* 装置状態情報取得情報 */
	struct st_plcPowSchTmInf	*ps_plcTmInf;			/* 計測値用情報取得情報 */
	struct st_devTmRange		*ps_plcTmRange;			/* 計測範囲 */
	time_t						now_time;				/* 現在時刻 */
	int							tmVal;					/* 計測値 */
	int 						plcType;				/* 処理タイプ */
	unsigned char				tmStatus[2];			/* 計測ステータス */	

	plcType = s_plcInfo[workIdxNumb].devType;

	/* 格納位置を取得 */
	tmIfLinkBase = ps_g_PLCFAINF[plcNumb - 1].tmIfLink;
	svStLinkBase = ps_g_PLCFAINF[plcNumb - 1].svStLink;
	svIfLinkBase = ps_g_PLCFAINF[plcNumb - 1].svIfLink;

#ifdef PV_DEBUG_DT1
	printf("%s : PCS[ %02d ] TYPE[%d] REQ[%d] COD[%d] LINK[TMST:%6d, SVST:%6d, SVIF:%6d]\n", pv_own_name,
		plcNumb, plcType, reqCnt, resultCode, tmIfLinkBase, svStLinkBase, svIfLinkBase );
#endif

	/* 処理タイプ１の場合 */
	if( plcType == PV_SCHPLC1_TYPE1 )
	{
		ps_plcSvInf   = s_type1SvInf;
		ps_plcTmInf   = s_type1TmInf;
		ps_plcTmRange = s_type1TmRange;
	}
	else
	{
		ps_plcSvInf   = s_type1SvInf;
		ps_plcTmInf   = s_type1TmInf;
		ps_plcTmRange = s_type1TmRange;
	}
	
	/* ------------------ */
	/* 装置状態情報の取得 */
	/* ------------------ */

	/* 受信処理結果が正常の場合のみ装置状態を取得 */
	if( resultCode == 0 )
	{
		/* ストッパーまで繰り返す */
		for( loopCnt1 = 0 ; ps_plcSvInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
		{
			/* 取得情報の要求回数が一致しない場合は次のデータへ */
			if( reqCnt != ps_plcSvInf[loopCnt1].reqCnt )
				continue;

			/* 処理タイプがビットタイプの場合 */
			if( ps_plcSvInf[loopCnt1].svType == PV_SVTYPE_BIT )
			{
#ifdef PV_DEBUG_SV
				printf("BATTPLC1 : BIT-TYPE\n" );
#endif
				/* 受信バッファの指定位置から指定バイト数分の状態をコピー */
				for( loopCnt2 = 0 ; loopCnt2 < ps_plcSvInf[loopCnt1].dataSize ; loopCnt2++ )
				{
					/* 状態を取得 */
					ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] = dataBuff[ ps_plcSvInf[loopCnt1].buffLink + loopCnt2 ];
#ifdef PV_DEBUG_SV
					printf("%s : SV-STS[%2d] : %02X\n", pv_own_name, ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2, ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] );
#endif
				}
			}
			/* 処理タイプがビットタイプ（８ビット逆順）の場合 */
			else if( ps_plcSvInf[loopCnt1].svType == PV_SVTYPE_08R )
			{
#ifdef PV_DEBUG_SV
				printf("BATTPLC1 : BIT(REV)-TYPE\n" );
#endif
				/* 受信バッファの指定位置から指定バイト数分の状態をコピー */
				for( loopCnt2 = 0 ; loopCnt2 < ps_plcSvInf[loopCnt1].dataSize ; loopCnt2++ )
				{
					/* 状態を初期化 */
					ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] = 0;
					/* ビットの並びをＭＳＢとＬＳＢを入れ替える（８ビット） */
					for( bitMask = 1, loopCnt3 = 0 ; loopCnt3 < 8 ; loopCnt3++, bitMask<<=1 )
					{
						/* 状態を設定 */
						if( dataBuff[ ps_plcSvInf[loopCnt1].buffLink + loopCnt2 ] & bitMask )
							ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] |= (unsigned char)(0x80 >> loopCnt3);
					}
#ifdef PV_DEBUG_SV
					printf("%s : SV-STS[%2d] : %02X\n", pv_own_name, ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2, ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] );
#endif
				}
			}
			/* 処理タイプがビットタイプ（１６ビット逆順）の場合 */
			else if( ps_plcSvInf[loopCnt1].svType == PV_SVTYPE_16R )
			{
#ifdef PV_DEBUG_SV
				printf("BATTPLC1 : 16BIT(REV)-TYPE\n" );
#endif
				/* 受信バッファの指定位置から指定バイト数分の状態をコピー */
				for( loopCnt2 = 0 ; loopCnt2 < ps_plcSvInf[loopCnt1].dataSize ; loopCnt2 += 2 )
				{
					/* 状態を初期化 */
					ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] = 0;
					ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 + 1 ] = 0;
					/* ビットの並びをＭＳＢとＬＳＢを入れ替える（１６ビット） */
					for( bitMask = 1, loopCnt3 = 0 ; loopCnt3 < 8 ; loopCnt3++, bitMask<<=1 )
					{
						/* 状態を設定 */
						if( dataBuff[ ps_plcSvInf[loopCnt1].buffLink + loopCnt2 ] & bitMask )
							ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 + 1] |= (unsigned char)(0x80 >> loopCnt3);
						if( dataBuff[ ps_plcSvInf[loopCnt1].buffLink + loopCnt2 + 1 ] & bitMask )
							ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] |= (unsigned char)(0x80 >> loopCnt3);
					}
#ifdef PV_DEBUG_SV
					printf("%s : SV-STS[%2d] : %02X\n", pv_own_name,
						ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2, ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] );
					printf("%s : SV-STS[%2d] : %02X\n", pv_own_name,
						ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 + 1, ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 + 1 ] );
#endif
				}
			}
			/* 処理タイプが数値タイプの場合 */
			else if( ps_plcSvInf[loopCnt1].svType == PV_SVTYPE_NUM )
			{
				/* 状態・コード番号の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
				for( loopCnt2 = 1 ; loopCnt2 <= ps_plcSvInf[loopCnt1].dataSize ; loopCnt2++ )
					u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_plcSvInf[loopCnt1].buffLink + ps_plcSvInf[loopCnt1].dataSize - loopCnt2];

				/* サイズが２バイトサイズの場合 */
				if( ps_plcSvInf[loopCnt1].dataSize == 2 )
				{
					/* 16bitサイズのデータとして数値を取得 */
					codNumb = u_change.data16bit;
				}
				/* サイズが４バイトサイズの場合 */
				else if( ps_plcSvInf[loopCnt1].dataSize == 4 )
				{
					/* 32bitサイズのデータとして数値を取得 */
					codNumb = u_change.data32bit;
				}
				else
				{
					/* 8bitサイズのデータとして数値を取得 */
					codNumb = u_change.dataByte[0];
				}

				if( ps_plcSvInf[loopCnt1].iniSize != 0 )
				{
					/* 状態を初期化 */
					memset( &ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink ], 0, ps_plcSvInf[loopCnt1].iniSize );
				}

				/* コード番号から状態番号を取得 */
				for( stsNumb = -1, loopCnt2 = 0 ; loopCnt2 <= 9999 ; loopCnt2++ )
				{
					/* 変換テーブルのコード番号がストッパーの場合 */
					if( ps_plcSvInf[loopCnt1].ps_stsCodTblPt[loopCnt2].codNumb == -1 )
					{
						/* 状態番号を設定 */
						stsNumb = ps_plcSvInf[loopCnt1].ps_stsCodTblPt[loopCnt2].stsNumb;
						break;
					}
					/* 変換テーブルのコード番号が一致する場合 */
					if( ps_plcSvInf[loopCnt1].ps_stsCodTblPt[loopCnt2].codNumb == codNumb )
					{
						/* 状態番号を設定 */
						stsNumb = ps_plcSvInf[loopCnt1].ps_stsCodTblPt[loopCnt2].stsNumb;
						break;
					}
				}

#ifdef PV_DEBUG_SV
				printf("%s : NUM-TYPE CODE[ %d ] SV-NUM[ %d ]\n", pv_own_name, codNumb, stsNumb );
#endif

				/* 状態番号が１以上の場合 */
				if( stsNumb > 0 )
				{
					/* 状態を更新 */
					ps_plcData->plcStatus[ (stsNumb - 1) / 8 ] |= (unsigned char)0x80 >> ((stsNumb - 1) % 8);
				}
				/* コード番号に該当する状態番号が見つからなかった場合 */
				else if( stsNumb == -1 )
				{
					sprintf( errMsg, "SV Status Number Not Found !! PCS-No[%02d] Type[%d] Buff[%d] Size[%d] NUMBER[%d]",
						plcNumb, plcType, ps_plcSvInf[loopCnt1].buffLink, ps_plcSvInf[loopCnt1].dataSize, codNumb );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
#ifdef PV_DEBUG_SV
				for( loopCnt2 = 0 ; loopCnt2 < ps_plcSvInf[loopCnt1].iniSize ; loopCnt2++ )
				{
					printf("%s : SV-STS[%2d] : %02X\n", pv_own_name,
						ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2, ps_plcData->plcStatus[ ps_plcSvInf[loopCnt1].pcsSvLink + loopCnt2 ] );
				}
#endif
			}
		}

		/* 装置状態情報の更新（ビットステータス形式で更新） */
		commfunc_johen_sv( PV_MKIND_PLCFA, plcNumb, PV_MAX_PLCFA_SVPOS, ps_plcData->plcStatus,
			svIfLinkBase, svStLinkBase, 
			ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	/* ------------------------------------ */
	/* 通常形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */

	/* ストッパーまで繰り返す */
	for( loopCnt1 = 0 ; ps_plcTmInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
	{
		/* 取得情報の要求回数が一致しない場合は次のデータへ */
		if( reqCnt != ps_plcTmInf[loopCnt1].reqCnt )
			continue;

		/* 計測情報番号を取得 */
		tmNumb = ps_plcTmInf[loopCnt1].tmNumb - 1;

		/* 受信結果が異常 または 計測情報が未実装の場合 */
		if( ( resultCode != 0 ) || ( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 ) )
		{
			/* 未実装の場合 */
			if( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 )
			{
				/* 計測値を０に設定 */
				ps_plcData->tmValData[tmNumb] = 0;
				/* 計測ステータスをリセット */
				ps_plcData->tmFlgData[tmNumb] = 0;
			}
			else
			{
				/* 計測ステータスに欠測を設定 */
				ps_plcData->tmFlgData[tmNumb] = PV_TMFLG_DROPTM;
			}

#ifdef PV_DEBUG_TM
		printf("%s : TM[%2d] ERR FLG:VAL[%Xh:%9d]  NAME:[%s]\n", pv_own_name,
			tmNumb+1, ps_plcData->tmFlgData[tmNumb], ps_plcData->tmValData[tmNumb], ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
			continue;
		}
		/* 受信結果正常 かつ 実装の場合 */
		else
		{
			/* 変換用領域の初期化 */
			memset( &u_change, 0, sizeof(u_change));

			/* 計測値の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
			for( loopCnt2 = 1 ; loopCnt2 <= ps_plcTmInf[loopCnt1].dataSize ; loopCnt2++ )
				u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_plcTmInf[loopCnt1].buffLink + ps_plcTmInf[loopCnt1].dataSize - loopCnt2];

			/* 計測値のサイズが２バイトサイズの場合 */
			if( ps_plcTmInf[loopCnt1].dataSize == 2 )
			{
				/* 符号付きの場合 */
				if( ps_plcTmInf[loopCnt1].signedFlg == 1 )
				{
					/* 符号あり16bitサイズのデータとして計測値を取得 */
					tmVal = u_change.data16bit;
				}
				else
				{
					/* 符号無し16bitサイズのデータとして計測値を取得 */
					tmVal = u_change.data16bitu;
				}
			}
			/* 計測値のサイズが４バイトサイズの場合 */
			else if( ps_plcTmInf[loopCnt1].dataSize == 4 )
			{
				/* 符号あり32bitサイズのデータとして計測値を取得 */
				tmVal = u_change.data32bit;
			}


			/* 計測値が既定値の範囲内か？ */
			if( ( ps_plcTmRange[tmNumb].tmLower > tmVal ) ||
				( ps_plcTmRange[tmNumb].tmUpper < tmVal ) )
			{
				/* 計測異常２をセット */
				/* 計測フラグ->計測値の順で更新する */
				ps_plcData->tmFlgData[tmNumb] = PV_TMFLG_TMERR2;
				ps_plcData->tmValData[tmNumb] = tmVal;
			}
			else
			{
				/* 計測フラグを正常にセット */
				/* 計測値->計測フラグの順で更新する */
				ps_plcData->tmValData[tmNumb] = tmVal;
				ps_plcData->tmFlgData[tmNumb] = PV_TMFLG_NORMAL;
			}
		}

#ifdef PV_DEBUG_TM
		printf("%s : TM[%2d] NOR FLG:VAL[%Xh:%9d]  NAME:[%s]\n", pv_own_name,
			tmNumb+1, ps_plcData->tmFlgData[tmNumb], ps_plcData->tmValData[tmNumb], ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
	}

	/* ------------------------------------ */
	/* 特殊形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */
	/* 該当する計測値なし */

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス異常終了処理
 *
 * 機能：
 * 		プロセスの異常終了をシステム監視ファイルに登録し、ファイルリソースを開放
 * 		する。
 * 戻り値：
 * 		0：正常処理
 */
int	func_abnormal_exit( int lineNo )
{
		/* プロセス異常中フラグを設定 */
		commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_SET );
		/* プロセス動作中フラグを解除 */
		commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_RESET );
		/* ファイルをオープンしてＭＭＡＰ化する */
		commfunc_file_close( s_fileIndex );

		return( 0 );
}
