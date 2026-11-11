/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251008] 2025.10.08
 * ・ハートビート設定を追加
 * [20251006] 2025.10.06
 * ・周波数Droop機能を使用しない為、取得しない情報の追加
 * [20250731] 2025.07.31
 * ・出力抑制対応
 * [20250421] 2025.04.21
 * ・RJ [20250206]版の通信プロセスをベースに新規作成
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * -----------------------------------------------------------------------------
 * Ｔｅｓｌａ製リチウムイオン電池通信プロセス
 * （通信プロトコル１：ＭｏｄｂｕｓＴＣＰ通信タイプ）
 *
 * 主な機能
 * 		（１）通信機能
 * 		起動時に指定された通信回線番号に接続されたＴｅｓｌａ製の下記バッテリー
 *		と通信し、計測値と装置状態の情報を収集する。
 * 		情報収集は「通信回線情報ファイル（COMLIINF.dat）」と「バッテリー情報ファ
 * 		イル（BATTEINF.dat）」の設定を元に行う。
 * 		通信は、前述の設定ファイルの内容に従い定周期に行う。
 *
 * 		（２）情報の取得および管理
 * 		収集した情報は、計測情報の場合は「計測値保存ファイル（TMSTSFIL.dat）」に
 * 		行い、状態情報は「装置状態ファイル（SVSTSFIL.dat）」に行う、なお、バッテ
 * 		リーとの通信時に発生した通信の状態は「通信ステータス状態ファイル
 * 		（STSTSFIL.dat）」に保存する。
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
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvctldata.h"

#include "btttesl2.h"

/* デバッグフラグ */
#if 0
#define	PV_DEBUG_SERIAL
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_SV									/* デバッグフラグ：受信データ解析結果表示(SV) */
#define	PV_DEBUG_TM									/* デバッグフラグ：受信データ解析結果表示(TM) */
#define	PV_DEBUG_DT3								/* デバッグフラグ：移動平均ストックデータ表示 */
#define	PV_DEBUG_DT4								/* デバッグフラグ：制御処理確認 */
#define PV_CHECK_DROP								/* デバッグフラグ：収取周期と計測値取得時間から欠測かを表示 */
#define PV_DEBUG_CTL								/* デバッグフラグ：制御 */
#define PV_DEBUG_FACT								/* デバッグフラグ：力率算出 */
#define PV_DEBUG_BMT								/* デバッグフラグ：バッテリーメータ用スレッド */
#define PV_DEBUG_FREQ								/* デバッグフラグ：周波数 */
#define PV_DEBUG_FREQ_DROOP							/* デバッグフラグ：周波数ドループ */
#endif

#define	PV_OWN_NAME			"BTTTESLA2"				/* 自プログラム名 */

/* 通信回線で使用するプロトコル名 */
#define	PV_COMPROTOCOL		PV_COMPRO_TESLA2		/* Ｔｅｓｌａ通信プロトコル（MODBUS-TCP） */

/* ＭＭＡＰアクセス用ポインタ変数 */
struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* 通信回線情報ファイルポインタ */
struct st_BATTEINF		*ps_g_BATTEINF;				/* バッテリー情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
//struct st_TMRECFIL		*ps_g_TMRECFIL;				/* 蓄積ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVCTLFIL		*ps_g_PVCTLFIL;				/* 制御状態保存ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL;		/* スケジュール管理情報ファイルポインタ [20230823] Add */

/* ファイルインデックス（本プロセスで使用するファイルの登録） */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
//	{	PV_FILENO_TMRECFIL, (void **)&ps_g_TMRECFIL, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_BATTEINF, (void **)&ps_g_BATTEINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVCTLFIL, (void **)&ps_g_PVCTLFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	PV_FILENO_SCHCTLFIL, (void **)&ps_g_SCHCTLFIL, 0, 0, 0 },	/* [20230823] Add */
	{	0,	                NULL,                    0, 0, 0,}
};

static char						pv_own_name[128];			/* 自プロセス情報 */
static char						errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int						bttMaxCnt = 0;				/* 処理対象バッテリー数 */
static struct st_serialinf		s_serial[PV_MAX_BATTE];		/* シリアル回線情報 */
static struct st_serialinf		s_bttMtSerial[PV_MAX_BATTE];	/* シリアル回線情報(周波数取得) *//* [20241017] Add */
static struct st_stdDevInfo		s_bttInfo[PV_MAX_BATTE];	/* バッテリー情報格納領域 */
static struct st_bttData		s_bttData[PV_MAX_BATTE];	/* 計測情報（瞬時値格納用） */
static struct st_threadArg		s_threadArg[PV_MAX_BATTE];	/* スレッド管理情報 */
static struct st_threadArg		s_bttMtThreadArg[PV_MAX_BATTE];	/* スレッド管理情報(周波数取得) *//* [20241017] Add */
static struct st_bttAvlData		s_bttAvlData[PV_MAX_BATTE];	/* 計測情報（移動平均演算用） */
static int						threadStop = 0;				/* スレッドストップフラグ */
static struct st_bttFactTm		s_bttFactTm[PV_MAX_BATTE];	/* 力率計測情報（移動平均演算用） *//* [20200824] Add */
static struct st_extraParam 	s_bttExtraParam[PV_MAX_BATTE];	/* 力率設定値 *//* [20230721] Add */
static char						rdwt_thred_stList[PV_MAX_STPOS];	/* 読込書込通信ST *//* [20241017] Add */
static char						read_thred_stList[PV_MAX_STPOS];	/* バッテリメータ読込通信ST *//* [20241017] Add */
static char						join_stList[PV_MAX_STPOS];			/* 統合通信ST *//* [20241017] Add */

/* 内部関数定義 */
void	*func_bttOpen_thread( void *args );
int		func_bttData_recv( struct st_serialinf *, int, int, int, struct st_bttTeslaSdInf *, unsigned char *, unsigned char *, struct st_serialLog *, int );
int		func_bttData_get( int, int, int, int, struct st_bttData *, unsigned char * );
int		func_abnormal_exit( int );
void	func_set_soc( int, struct st_bttData *);
int 	func_get_extraParam( char *, time_t *, struct st_extraParam *  );	/* [20230721] Add */

/* [20241017] Add St */
void	*func_bttOpen_bttMtThread( void *args );				/* 周波数取得用スレッド */
void	func_btt_johen_stat( int, int, int, int );				/* 通信ステータス状変処理（単独処理） */
void	func_btt_johen_resetstat( int, int );					/* 通信ステータス状変処理（全通信ステータス復帰） */
/* [20241017] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * Ｔｅｓｌａ製バッテリー通信プロセス主処理
 *
 * 機能：
 * 		引数で指定された通信回線番号を元に通信回線情報ファイル
 * 		（COMLIINF.dat）から通信回線の設定情報を取得する。
 * 		引数で指定された通信回線番号を元にバッテリー情報ファイル
 * 		（BATTEINF.dat）から通信回線番号が一致する装置の情報を取得する。
 * 		ファイルに設定された周期でバッテリーと通信を行い、情報を取得し
 * 		て編集結果を指定されたファイルに保存する。
 * 		管理プロセスから停止指示が出た場合はプロセスを終了する。
 *
 */
int main(int argc, char *argv[])
{
	char					lineNo;						/* 通信回線番号 */
	char					ip4Data[12];				/* ＩＰアドレス第四オクテット */
	char					*charWorkPt;				/* ワークポインタ */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	pthread_t				threadID[PV_MAX_BATTE];		/* スレッドＩＤ */
	pthread_t				bttMtThreadID[PV_MAX_BATTE];	/* 周波数取得スレッドＩＤ *//* [20241017] Add */
	int						requestCnt = 0;				/* 合計要求回数 */
	int						bttNumb;					/* 処理中のバッテリー番号 */
	int						minDataMax;					/* 移動平均データ最大数 */
	int						timeOut = 0;				/* タイムアウト値 */
	struct timeval			nowTime;					/* 現在時刻 */
	struct timeval 			wrkTime;					/* 比較時刻 */
	int						dropFlg;					/* 欠測フラグ */
	struct st_bttData		bttData[PV_MAX_BATTE];		/* 計測情報 */
	long long				stepPow;
	

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
		printf("%s : File Open Error\n", PV_OWN_NAME);
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_SET );

	/* シリアル回線情報を初期化 */
	memset( s_serial, 0, sizeof(s_serial) );
	memset( s_bttMtSerial, 0, sizeof(s_bttMtSerial) );/* [20241017] Add */
	/* データ格納領域を初期化 */
	memset( &s_bttData, 0, sizeof(s_bttData));
	/* スレッド管理情報を初期化 */
	memset( s_threadArg, 0, sizeof(s_threadArg));
	memset( s_bttMtThreadArg, 0, sizeof(s_bttMtThreadArg));/* [20241017] Add */
	/* 力率設定値情報を初期化 */
	memset( s_bttExtraParam, 0, sizeof(s_bttExtraParam));

	/* 通信回線情報を取得 */
	if( (retCode = commfunc_serial_infoget( lineNo, &s_serial[0], ps_g_COMLIINF, PV_COMPROTOCOL )) != 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [COMLINLIF.csv] Information Get Error Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}
/* [20241017] Add St */
	else
	{
		memcpy( &s_bttMtSerial[0], &s_serial[0], sizeof(struct st_serialinf) );
	}
/* [20241017] Add Ed */

	/* バッテリー情報を取得 */
	if( (bttMaxCnt = commfunc_devInfo_get( PV_MKIND_BATTE, lineNo, s_bttTypeName, s_bttInfo, &requestCnt, NULL, NULL, ps_g_BATTEINF )) <= 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [BATTEINF.csv] Not Exist Target Machine of Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

#ifdef PV_DEBUG
	printf("%s : PCSMAXCNT : %d\n", pv_own_name, bttMaxCnt );
#endif

	/* シリアル情報にあるデバイス名（IPアドレス）の第4オクテットデータを消去する */
	if( ( charWorkPt = strrchr( s_serial[0].targetName, '.' ) ) != NULL )
	{
		charWorkPt++;
		memset( charWorkPt, 0, 10);
		memcpy( &s_bttMtSerial[0].targetName, &s_serial[0].targetName, sizeof(s_bttMtSerial[0].targetName) );/* [20241017] Add */
	}

	/* 複数パワコン対応処理 */
	for( loopCnt1 = 1 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{
		/* シリアル回線情報をコピーする */
		memcpy( &s_serial[loopCnt1], &s_serial[0], sizeof(struct st_serialinf) );
		memcpy( &s_bttMtSerial[loopCnt1], &s_serial[0], sizeof(struct st_serialinf) );/* [20241017] Add */
	}

	/* 複数パワコン対応処理 */
	for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{
		/* IPアドレスの第4オクテットを装置IDを使用して設定 */
		sprintf( ip4Data, "%d", s_bttInfo[ loopCnt1 ].idNumb );
		strcat( s_serial[loopCnt1].targetName, ip4Data );
		strcat( s_bttMtSerial[loopCnt1].targetName, ip4Data );/* [20241017] Add */

#ifdef PV_DEBUG
	printf("%s : PCS:%02d IP-ADDRESS:%s\n", pv_own_name, s_bttInfo[loopCnt1].devNumb, s_serial[loopCnt1].targetName );
#endif
		/* スレッド管理情報を設定 */
		s_threadArg[loopCnt1].indexNumb = loopCnt1;
		/* 周波数スレッド管理情報を設定 */
		s_bttMtThreadArg[loopCnt1].indexNumb = loopCnt1;/* [20241017] Add */

	}

	/* データ格納領域に欠測を設定 */
	for( loopCnt1 = 0; loopCnt1 < bttMaxCnt; loopCnt1++ )
	{
		for( loopCnt2 = 0; loopCnt2 < PV_MAX_BATTE_TMPOS; loopCnt2++ )
		{
			s_bttData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
		}
/* [20200824] Add St */

		/* 力率計測情報を初期化 */
		/* 1分間の取得周期を算出 */
		s_bttFactTm[loopCnt1].index = 0;
		s_bttFactTm[loopCnt1].minDataNum = 60000 / s_serial[0].cycleTime;

		/* 計測ステータスを欠測に設定 */
		for( loopCnt2 = 0; loopCnt2 < s_bttFactTm[loopCnt1].minDataNum; loopCnt2++ )
		{
			s_bttFactTm[loopCnt1].s_tmData[loopCnt2].tmFlg = PV_TMFLG_DROPTM;
		}
/* [20200824] Add Ed */
	}

	/* 収集周期の調整と移動平均算出用データ数の取得 */
	commfunc_serial_cycletmchk( &s_serial[0], &minDataMax, ps_g_COMLIINF );

	/* 移動平均用データの初期化 */
	commfunc_devAvlData_init( PV_MKIND_BATTE, minDataMax, s_bttAvlData );

	/* 移動平均用データから積算型計測値を復元する */
	retCode = commfunc_tmstsfil_get( PV_MKIND_BATTE, bttMaxCnt, PV_BTTTESLA2_MAXTM, s_bttInfo, s_bttData, s_bttAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );


	/* バッテリーの装置状態を復元 */
	for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{
		/* バッテリーの装置状態を復元する */
		commfunc_sv_get( PV_MKIND_BATTE, s_bttInfo[loopCnt1].devNumb, PV_MAX_BATTE_SVPOS, s_bttData[loopCnt1].bttStatus, ps_g_SVSTSFIL );
	}

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_RESET );

	/* 当該通信回線に所属するバッテリーの台数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{

#ifdef PV_DEBUG
		printf("%s : PCS[%02d] OPEN-Thread EXECUTE\n", pv_own_name, s_bttInfo[loopCnt1].devNumb );
#endif
		/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
		s_threadArg[loopCnt1].stFlg = 1;
		s_threadArg[loopCnt1].threadNumb = 0;/* [20241017] Add Ed */
		s_threadArg[loopCnt1].lineNo = lineNo;/* [20241017] Add Ed */
		if( pthread_create( &threadID[loopCnt1], NULL, func_bttOpen_thread, &s_threadArg[loopCnt1] ) != 0 )
		{

#ifdef PV_DEBUG
printf("%s : PCS[%02d] OPEN-Thread EXECUTE Error\n", pv_own_name, s_bttInfo[loopCnt1].devNumb );
#endif
			sprintf( errMsg, "Thread Execute Error" );
			commfunc_syslog_msgset( pv_own_name, errMsg );

			/* プロセス異常終了処理 */
			func_abnormal_exit( lineNo );
			return( -1 );
		}

/* [20241017] Add St */
		/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
		s_bttMtThreadArg[loopCnt1].stFlg = 1;
		s_bttMtThreadArg[loopCnt1].threadNumb = 1;
		s_bttMtThreadArg[loopCnt1].lineNo = lineNo;
		if( pthread_create( &bttMtThreadID[loopCnt1], NULL, func_bttOpen_bttMtThread, &s_bttMtThreadArg[loopCnt1] ) != 0 )
		{
#ifdef PV_DEBUG
printf("%s : PCS[%02d] OPEN-BttMtThread EXECUTE Error\n", pv_own_name, s_bttInfo[loopCnt1].devNumb );
#endif
			sprintf( errMsg, "bttMtThread Execute Error" );
			commfunc_syslog_msgset( pv_own_name, errMsg );

			/* プロセス異常終了処理 */
			func_abnormal_exit( lineNo );
			return( -1 );
		}
/* [20241017] Add Ed */
	}

	while( 1 )
	{
		/* 収集時刻になるまで待機する */
		if( commfunc_mswait( s_serial[0].cycleTime, lineNo ) < 0 )

		{
#ifdef PV_DEBUG
			printf("%s : PROCESS STOP COMMAND RECV (LINE:%d)\n", pv_own_name, lineNo );
#endif
			/* 処理ループを抜ける */
			break;
		}

/* [20200824] Add St */
		/* 当該通信回線に所属するバッテリーの台数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
		{
			/* 周期毎にIndexを更新する */
			if( ( s_bttFactTm[loopCnt1].index + 1 ) >= s_bttFactTm[loopCnt1].minDataNum )
			{
				s_bttFactTm[loopCnt1].index = 0;
			}
			else
			{
				s_bttFactTm[loopCnt1].index++;
			}
			s_bttFactTm[loopCnt1].s_tmData[s_bttFactTm[loopCnt1].index].tmFlg = PV_TMFLG_DROPTM;
		}
/* [20200824] Add Ed */

	}

	/* 収集周期 * 5 秒 を待つ(PCSの発電停止時間分) */
	sleep( s_serial[0].cycleTime / 200 );

	// ★複数スレッドの終了について検討する必要がある。
	/* スレッドの終了を待つ */
	threadStop = 1;
	pthread_join( threadID[0], NULL );
/* [20241017] Add St */
	pthread_join( bttMtThreadID[0], NULL );
/* [20241017] Add Ed */

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
void *func_bttOpen_thread( void *args )
{
/* [20241017] Add St */
	union {												/* ビッグエンディアン－リトルエンディアン変換 */
		int					data32bit;					/* 32bitデータ取得用 */
		float				data32f;					/* 32bitデータ取得用 */
	} u_change;
/* [20241017] Add Ed */
	unsigned  char			*p_sendBuff;				/* 送信バッファポンタ */
	unsigned  char			sendBuff_r[12] = {
								0x00,0x00,				/* トランザクションID（本装置では固定的に0を使用） */
								0x00,0x00,				/* プロトコルID（MODBUS=0を使用） */
								0x00,0x06,				/* データ長 */
								0x01,					/* 送信先局番（固定値0x01を使用） */
								0x00,					/* ファンクションコード（保持レジスタ読出） */
								0x00,0x00,				/* 開始アドレス */
								0x00,0x00				/* レジスタの数 */
							};
	unsigned  char			sendBuff_w[35] = {
								0x00,0x00,				/* [00-01]:トランザクションID（本装置では固定的に0を使用） */
								0x00,0x00,				/* [02-03]:プロトコルID（MODBUS=0を使用） */
								0x00,0x0F,				/* [04-05]:データ長(可変) */
								0x01,					/* [06]   :送信先局番（固定値0x01を使用） */
								0x00,					/* [07]   :ファンクションコード（保持レジスタ読出） */
								0x00,0x00,				/* [08-09]:開始アドレス */
								0x00,0x00,				/* [10-11]:レジスタの数 */
								0x08,					/* [12]   :以降のデータサイズ(可変) */
								0x00,0x00,0x00,0x00,	/* 充放電電力/制御値 */
								0xAA,0x55,				/* ハートビートパターン */
								0x00,0x3C,				/* ハートビートタイムアウト(60sec固定) */
								0x00,0x00,0x00,0x00,	/* [21-24] */
								0x00,0x00,0x00,0x00,	/* [25-28] */
								0x00,0x00,0x00,0x00,	/* [29-32] */
								0x00,0x00				/* [33-34] */
							};
	unsigned char			heartBeatPtn[2] = {			/* ハートビートパターン(0xAA55 or 0x55AA) */
								0xAA,0x55
							};
	int						directPower=0;				/* 充放電電力 */
	int						dataLength;					/* 送信データ長 */
	unsigned char			readBuff[4096];				/* 受信バッファ */
	char					lineNo;						/* 通信回線番号 */
	int						openRetCode = -1;			/* シリアルポートオープン結果 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */

	int						bttNumb;					/* 処理中のバッテリー番号 */
	int						idNumb;						/* 処理中のバッテリー装置ＩＤ */
	struct st_serialLog		s_serialLog;				/* シリアル通信ログ格納領域 */
	struct timespec			sleepTm;					/* 送信待ちタイマ */
	struct st_bttTeslaSdInf	*ps_bttSdInf;				/* 送信情報ポインタ */
	int						workIdxNumb;				/* 処理インデックス番号 */
	struct st_threadArg		*ps_threadArg;				/* スレッド引数処理用 */
	struct timeval			nowTime;					/* 現在時刻 */
	int						reqKind = 0;				/* 要求種別フラグ */
	int						lastRet=0;					/* 前回送信結果 */
	int						svBttCtlNow;				/* 前回制御権On/Off */
	int						svBttCtlPre=0;				/* 制御権SVフラグ */
	int						svGridStatNow = 0;			/* 連系SVOn/Off */
	int						svGridStatPre=0;			/* 前回連系SVフラグ */
	int						setPower;					/* 充放電電力値 */
	int						threadStop_local;			/* 停止フラグ(thread用) */
	int						onGridSvNumb;				/* 連系状態SV番号 *//* [20230529] Add */
/* [20230721] Add St */
	time_t					fileTime=0;					/* ファイル更新時刻 */
	char					extdataPath[128];			/* 外部データパス */
	int						retFileCode;				/* 関数戻り値格納用 */
/* [20230721] Add Ed */
	int						chgTime;					/* 出力変化時間 *//* [20230823] Add */
	int						tranID;						/* トランザクションID *//* [20241111] Add */
	int						threadNumb;					/* スレッド番号 *//* [20241017] Add */
/* [20241017] Add St */
	int						svSelfEndCtl=0;				/* 自端制御動作中 */
	int						svSelfEndCtlFreqErr=0;		/* 自端制御周波数異常動作中 */
	int						svBttSysOffReq=0;			/* 蓄電システム バッテリOFF要求 */
	int						droopSt;					/* 周波数ドループ有効/無効 */
	float					floatWork;					/* float型変換作業用 */
	long					freqRegRate;				/* 調定率 */
	long long				freqRegRateExpNum;			/* 調定率を戻すべき10のべき乗 */
	long					ratedOutput;				/* 定格電力 *//* [20250204] Chg */
	long					maxChgPow;					/* 最大充電電力 */
	long					maxDisChgPow;				/* 最大放電電力 */
/* [20241017] Add Ed */
	int						rampRate;					/* ランプレート値 *//* [20250731] Add */
	int						actPowTmNumb;				/* 有効電力計測番号 *//* [20250731] Add */
	int						heatbeatCnt=0;				/* ハートビートカウント *//* [20251008] Add */

	ps_threadArg = (struct st_threadArg *)args;

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));


	/* 処理インデックス番号の取得 */
	workIdxNumb = ps_threadArg->indexNumb;

	/* パワコン番号の取得 */
	bttNumb = s_bttInfo[workIdxNumb].devNumb;
	/* バッテリー装置IDの取得 */
	idNumb  = s_bttInfo[workIdxNumb].idNumb ;
	/* スレッド番号の取得 */
	threadNumb = ps_threadArg->threadNumb;	/* [20241017] Add */

	/* 外部データファイルを設定 */
	sprintf(extdataPath, "%s%s_%d.txt", PV_PATH_EXT, PV_OWN_NAME, bttNumb );

	/* 設定パラメータ初期値設定 */
	s_bttExtraParam[workIdxNumb].statFlag = 0;
	s_bttExtraParam[workIdxNumb].droopTimeResponse = 1;
	/* 設定ファイルのチェック*/
	retFileCode = func_get_extraParam( extdataPath, &fileTime, &s_bttExtraParam[workIdxNumb] );
	if( retFileCode != COM_PARAM_GET_OK )
	{
		sprintf( errMsg, "commfunc_file_info_get RET=%d. (%s)", retFileCode, extdataPath );
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}
	else
	{
		sprintf( errMsg, "extra file read. (%s)", extdataPath );
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}
	fileTime = 0;

#ifdef PV_DEBUG
	printf("%s : IDX[%02d] PCS[%02d] OPEN-Thread START\n", pv_own_name, workIdxNumb, bttNumb );
#endif

	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = s_serial[workIdxNumb].sndTime / 1000;
	sleepTm.tv_nsec = (s_serial[workIdxNumb].sndTime % 1000) * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( 1 )
	{
		/* 停止フラグを保持(ループの最初から停止フラグの情報を使用するようにここで保持する) */
		threadStop_local = threadStop;

		/* ポートがオープンしていない場合 */
		if( s_serial[workIdxNumb].status != PV_STAT_COM_CONNECT )
		{
			/* 要求種別を「通信復帰時」に変更 */
			reqKind = PV_BTTTESLA2_REQ_CB;
		}

		/* シリアルポートをオープン */
		openRetCode = commfunc_serial_open( &s_serial[workIdxNumb] );
		if( openRetCode >= 0 )
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（復帰） */
			func_btt_johen_stat( threadNumb, PV_ST14_LANERR, bttNumb, 0 );	/* [20241017] Chg */

			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 0;

		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			func_btt_johen_stat( threadNumb, PV_ST14_LANERR, bttNumb, 1 );	/* [20241017] Chg */

#ifdef PV_DEBUG
		printf("%s : IDX[%02d] PCS[%02d] Thread OPEN ERROR!!\n", pv_own_name, workIdxNumb, bttNumb );
#endif
			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 1;
			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 5 );
		}

		/* 要求実施回数を初期化 */
		s_serial[workIdxNumb].requestCount = 0;

		/* 処理タイプの設定 */
		if( s_bttInfo[workIdxNumb].devType == PV_BTTTESLA2_TYPE1 )
		{
			/* 送信情報を処理タイプ１に設定 */
			ps_bttSdInf = s_type1SdInf;
			onGridSvNumb = PV_BTTTESLA2_SV_ONGLID;		/* [20230922] Chg */
		}
		else
		{
			sprintf( errMsg, "devType not support.(%d)", s_bttInfo[workIdxNumb].devType );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			/* プロセス異常終了処理 */
			func_abnormal_exit( lineNo );	
			return( NULL );
		}

		/* 状態取得：バッテリ操作権限 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_OPERATION, &svBttCtlNow, ps_g_SVSTSFIL);

		/* 戻り値初期化 */
		retCode = 0;
		/* 複数回に分けて情報を収集する */
		for( loopCnt2 = 0 ; ps_bttSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
		{
#ifdef PV_DEBUG
			printf("%s : LOOP:%02d BTTNO:%02d BTTID-No:%02d REQUEST:%d OPN-STS:%d, retCode=%d\n", PV_OWN_NAME, workIdxNumb+1, bttNumb, idNumb, loopCnt2+1, s_serial[workIdxNumb].status, retCode );
#endif
/* [20241017] Add St */
			/* 要求毎の設定 */
			if ( ps_bttSdInf[loopCnt2].addrType == CMD_GET_BATTERY_METER )
			{
				/* バッテリメータの情報は、専用スレッドで処理するためスキップ */
				continue;
			}
/* [20241017] Add Ed */
			/* 送信バッファの設定 */
			if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_03 )
			{
				p_sendBuff = &sendBuff_r[0];
				
			}
			else
			{
				p_sendBuff = &sendBuff_w[0];
			}

			/* 送信待ち処理 */
			nanosleep( &sleepTm, NULL);


			/* 通信状態が正常の場合、且つ、前回処理が正常の場合 */
			if( ( s_serial[workIdxNumb].status == PV_STAT_COM_CONNECT ) &&
				( retCode == 0 ))
			{
#ifdef PV_DEBUG_SERIAL
				printf("%s : SERIAL POERT OPENED\n", PV_OWN_NAME);
#endif
/* [20241111] Add St */
				p_sendBuff[0]  = idNumb;
				p_sendBuff[1]  = loopCnt2 + 1;
/* [20241111] Add Ed */
				/* 送信バッファにファンクションと取得開始アドレス、取得データ数を設定 */
				if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_06 )
				{
					p_sendBuff[5] = ps_bttSdInf[loopCnt2].dataLength - 6;
					p_sendBuff[7] = ps_bttSdInf[loopCnt2].funcCode;
					p_sendBuff[8] = ps_bttSdInf[loopCnt2].startAdU;
					p_sendBuff[9] = ps_bttSdInf[loopCnt2].startAdL;
				}
				else
				{
					p_sendBuff[5] = ps_bttSdInf[loopCnt2].dataLength - 6;
					p_sendBuff[7] = ps_bttSdInf[loopCnt2].funcCode;
					p_sendBuff[8] = ps_bttSdInf[loopCnt2].startAdU;
					p_sendBuff[9] = ps_bttSdInf[loopCnt2].startAdL;
					p_sendBuff[10] = ps_bttSdInf[loopCnt2].dataCntU;
					p_sendBuff[11] = ps_bttSdInf[loopCnt2].dataCntL;
					if (ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_10)
					{
						p_sendBuff[12] = p_sendBuff[11] * 2; // 書込みに絞らないと送信バッファ配列外に書き込む
					}
				}
				dataLength = ps_bttSdInf[loopCnt2].dataLength;
				tranID = (p_sendBuff[0] * 0x100) + p_sendBuff[1];	/* [20241111] Add */
	
				/* ファンクションコードが書込みの場合 */
				if(( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_10 ) ||
				   ( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_06 ))
				{
/* [20230814] Chg St */
					/* 要求毎の設定 */
					switch( ps_bttSdInf[loopCnt2].addrType )
					{

						case CMD_SET_POWER :		/* 充放電の設定 */
							/* 操作権限があり、且つ、IslandMode が 連係時 */
							if( ( svBttCtlNow == 1 ) && ( svGridStatNow == 1 ) )
							{
								/* スレッド停止フラグがONの場合は、出力停止する */
								if( threadStop_local != 0 )
								{
									directPower = 0;
									chgTime = 0;		/* [20250731] Add */
								}
								else
								{
									/* 出力制御値を取得 */
									commfunc_pvctl_bttPowGet( &directPower, &chgTime, ps_g_TMSTSFIL);
								}
/* [20250731] Add St */
								/* ランプレート無効(定格出力) */
								rampRate = ps_g_BATTEINF->maxDisChgPwr;

								/* 有効電力計測番号を保持 */
								actPowTmNumb = s_typeCalcInf[s_bttInfo[workIdxNumb].devType -1].actPowNumb;

								/* 出力抑制(100%以外)の場合 */
								if( chgTime > 0 )
								{
									/* 充電方向で有効電力が欠測の場合は、設定しない(前回値を使用) */
									if( ( directPower < 0 ) &&
										( s_bttData[workIdxNumb].tmFlgData[actPowTmNumb-1] == PV_TMFLG_DROPTM ))
									{
										continue;
									}

									/* 指令値が放電方向、または、(指令値が充電方向、且つ、有効電力が充電の場合) */
									if( ( directPower >= 0 ) ||
										(( directPower < 0 ) &&
										 ( s_bttData[workIdxNumb].tmValData[actPowTmNumb-1] > 0 )))
									{
										/* 出力変化時間に合わせたランプレートを設定する */
										rampRate = ((ps_g_BATTEINF+bttNumb-1)->maxDisChgPwr / (chgTime * 60 ));
									}
								}
/* [20250731] Add Ed */

/* [20241017] Chg St */
								/* 有効電力 */
								u_change.data32f = (float)directPower;
								p_sendBuff[13] = (( u_change.data32bit >> 24 ) & 0xFF );
								p_sendBuff[14] = (( u_change.data32bit >> 16 ) & 0xFF );
								p_sendBuff[15] = (( u_change.data32bit >> 8 ) & 0xFF );
								p_sendBuff[16] = ( u_change.data32bit & 0xFF );
								/* 有効電力変化率 */
								u_change.data32f = (float)rampRate;	/* [20250731] Chg */
								p_sendBuff[17] = (( u_change.data32bit >> 24 ) & 0xFF );
								p_sendBuff[18] = (( u_change.data32bit >> 16 ) & 0xFF );
								p_sendBuff[19] = (( u_change.data32bit >> 8 ) & 0xFF );
								p_sendBuff[20] = ( u_change.data32bit & 0xFF );
/* [20241017] Chg Ed */
							}
							else
							{
								continue;
							}
							break;
/* [20230721] Add St */
						case CMD_SET_POWER_FACT :		/* 力率設定 */
							/* 外部ファイルのチェック */
							/* 連係時 */
							/* 操作権限が遠方時 */
							if( ( s_bttExtraParam[workIdxNumb].statFlag == 1 ) &&
								( svGridStatNow == 1 ) &&
								( svBttCtlNow == 1 ) )
							{
								/* 設定ファイルのチェック*/
								retFileCode = func_get_extraParam( extdataPath, &fileTime, &s_bttExtraParam[workIdxNumb] );

								/* 通信異常復帰時、または、ファイル更新時 */
								if( ((reqKind & PV_BTTTESLA2_REQ_CB)  == PV_BTTTESLA2_REQ_CB ) ||
									( retFileCode == COM_PARAM_GET_OK ) )
								{
/* [20241017] Chg St */
									/* 力率有効/無効 */
									p_sendBuff[13] = (( s_bttExtraParam[workIdxNumb].statFlag  >> 8 ) & 0xFF );
									p_sendBuff[14] = ( s_bttExtraParam[workIdxNumb].statFlag  & 0xFF );
									/* 放電力率設定値 */
									u_change.data32f = (float)s_bttExtraParam[workIdxNumb].dischargeSetpoint;
									p_sendBuff[15] = (( u_change.data32bit >> 24 ) & 0xFF );
									p_sendBuff[16] = (( u_change.data32bit >> 16 ) & 0xFF );
									p_sendBuff[17] = (( u_change.data32bit >> 8 ) & 0xFF );
									p_sendBuff[18] = ( u_change.data32bit & 0xFF );
									/* 放電力率励起設定 */
									p_sendBuff[19] = ((s_bttExtraParam[workIdxNumb].dischargeLead >> 8 ) & 0xFF);
									p_sendBuff[20] = (s_bttExtraParam[workIdxNumb].dischargeLead & 0xFF);
									/* 充電力率設定値 */
									u_change.data32f = (float)s_bttExtraParam[workIdxNumb].chargeSetpoint;
									p_sendBuff[21] = (( u_change.data32bit >> 24 ) & 0xFF );
									p_sendBuff[22] = (( u_change.data32bit >> 16 ) & 0xFF );
									p_sendBuff[23] = (( u_change.data32bit >> 8 ) & 0xFF );
									p_sendBuff[24] = ( u_change.data32bit & 0xFF );
									/* 充電力率励起設定 */
									p_sendBuff[25] = ((s_bttExtraParam[workIdxNumb].chargeLead >> 8 ) & 0xFF);
									p_sendBuff[26] = (s_bttExtraParam[workIdxNumb].chargeLead & 0xFF);
/* [20241017] Chg Ed */
								}
								else
								{
									continue;
								}
							}
							else
							{
								continue;
							}
							break;
/* [20230721] Add Ed */
/* [20251008] Add St */
						case CMD_SET_HEART_BEAT :		/* ハートビート設定 */
							/* 操作権限が遠方時 */
							if(  svBttCtlNow == 1 )
							{
								if( heatbeatCnt >= 1000 )
								{
									heatbeatCnt = 0;
								}
								heatbeatCnt++;
								/* ハートビート設定 */
								p_sendBuff[13] = (( heatbeatCnt  >> 8 ) & 0xFF );
								p_sendBuff[14] = ( heatbeatCnt  & 0xFF );
							}
							else
							{
								continue;
							}
							break;
/* [20251008] Add Ed */
/* [20251006] Add St */
						case CMD_GET_FREQ_DROOP :			/* 周波数ドループ、有効電力制限の取得 */
							/* 一次調整力オフライン対応をソフトで実装するため、本読み込み処理をスキップする */
							continue;
							break;
/* [20251006] Add Ed */
/* [20241017] Add St */
						case CMD_SET_MAXMIN_POWER :			/* 有効充放電電力の設定 */
							/* 一次調整力オフライン対応をソフトで実装するため、本書き込み処理をスキップする */
							continue;
							break;
						case CMD_SET_LOWPASS_FILTER :		/* 周波数ドループ(ローパスフィルター応答時間)の設定 */
							/* 一次調整力オフライン対応をソフトで実装するため、本書き込み処理をスキップする */
							continue;
							break;
						case CMD_SET_FREQ_DROOP :			/* 周波数ドループの設定 */
							/* 一次調整力オフライン対応をソフトで実装するため、本書き込み処理をスキップする */
							continue;
							break;
						case CMD_SET_STATE_REQUEST :		/* 状態指令の設定 */
							/* 一次調整力オフライン対応をソフトで実装するため、本書き込み処理をスキップする */
							continue;
							break;
/* [20241017] Add Ed */
						default :
							break;
					}
				}
				else
				{
					/* 要求が種別が一致しない場合 */
					if( ( reqKind & ps_bttSdInf[loopCnt2].reqKind) == 0 )
					{
						/* 処理をスキップする */
						continue;
					}
				}
/* [20230814] Chg Ed */

				/* 処理結果の初期値を異常に設定 */
				retCode = -1;

				/* 要求実施回数の上限まで繰り返す */
				/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
				while( s_serial[workIdxNumb].requestCount < PV_SERIAL_REQUEST_MAX )
				{
					/* 要求のリトライは行わない */
					/* シリアル通信ログ領域を初期化 */
					s_serialLog.dataCnt = 0;

					/* バッテリーに要求を送信 */
					retCode = commfunc_serial_send( &s_serial[workIdxNumb], p_sendBuff, dataLength, &s_serialLog );
					if( retCode < 0 )
					{
						sprintf( errMsg, "TCP Port Write Error! RET:%d num/id:%02d/%02d (%s)",
							retCode, bttNumb, idNumb, s_serial[workIdxNumb].targetName );
						commfunc_syslog_msgset( pv_own_name, errMsg );

						/* データ送信異常（送信不可異常）発生 */
						/* 状変（通信ステータス状変）発生 */
						func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20241017] Chg */

						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* データ送信異常（送信不可異常）復帰 */
						/* 状変（通信ステータス状変）復帰 */
						func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 0 );	/* [20241017] Chg */
					}

					/* データ処理バッファをクリア */
					memset(readBuff, 0, sizeof(readBuff));
	
					/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
					retCode = func_bttData_recv( &s_serial[workIdxNumb], tranID, bttNumb, idNumb,	/* [20241111] Chg */
												 &ps_bttSdInf[loopCnt2],
												 p_sendBuff, readBuff, &s_serialLog, threadNumb );	/* [20241017] Chg */
					if( retCode == 0 )
					{
						/* 正常データ受信完了 */
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_AUTO, &s_serial[workIdxNumb], &s_serialLog );
						/* 通信ステータス状変：全ステータス（復帰） */
						func_btt_johen_resetstat( threadNumb, bttNumb );	/* [20241017] Chg */
						//commfunc_johen_resetstat( PV_MKIND_BATTE, bttNumb,
						//	(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );

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
				printf("%s : SERIAL POERT NOT-OPENED STATUS[%d] stFlg[%d]\n", PV_OWN_NAME, s_serial[workIdxNumb].status, s_threadArg[workIdxNumb].stFlg );
#endif
				/* シリアルポートが異常の場合 */
				retCode = -1;
			}

			/* 読み込み処理のみ計測情報を格納する */
			if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_03 )
			{
				/* データ処理バッファから装置状態と計測値を抽出 */
				func_bttData_get( s_bttInfo[workIdxNumb].devType, loopCnt2 + 1, bttNumb, retCode, &s_bttData[workIdxNumb], readBuff);
			}

			/* モード指令の取得 */
			if( ( ps_bttSdInf[loopCnt2].addrType == CMD_GET_BATT_SWITCH ) && ( retCode == 0 ) )
			{
				/* SV連系状態取得 */
				commfunc_sv_stat_get( PV_MKIND_BATTE, bttNumb, onGridSvNumb, &svGridStatNow, ps_g_SVSTSFIL);	/* [20230529] Chg */

				/* 現在状態がONで、前回状態がOFFの場合 */
				if(( svGridStatNow == 1 ) &&
				   ( svGridStatPre == 0 ))
				{
					/* 要求種別を「通信復帰時/連系切り替わり時」に変更 */
					reqKind = PV_BTTTESLA2_REQ_CB;
				}
				svGridStatPre = svGridStatNow;
			}
		}

		/* ループ処理の終端、且つ、正常終了の場合 */
		if( ps_bttSdInf[loopCnt2].funcCode == (unsigned char)0xFF )
		{

			if( retCode == 0 )
			{
				/* SoC情報を更新 */
				func_set_soc(workIdxNumb, s_bttData );

				/* 通信復帰時の場合 */
				if( reqKind == PV_BTTTESLA2_REQ_CB )
				{
					/* 通常時に遷移 */
					reqKind = PV_BTTTESLA2_REQ_NOR;
				}

				/* 現在状態がONで、前回状態がOFFの場合 */
				if(( svBttCtlNow == 1 ) &&
				   ( svBttCtlPre == 0 ))
				{
					/* 要求種別を「通信復帰時/連系切り替わり時」に変更 */
					reqKind = PV_BTTTESLA2_REQ_CB;
				}
				svBttCtlPre = svBttCtlNow;
			}
			else
			{
				/* SoC情報を更新 */
				func_set_soc(workIdxNumb, s_bttData );
			}
		}

		/* 計測種別毎の保持 */
		retCode = commfunc_devTmData_set( PV_MKIND_BATTE, bttNumb, PV_BTTTESLA2_MAXTM, &s_bttData[workIdxNumb], &s_bttAvlData[workIdxNumb], ps_g_TMDATINF );

		/* 計測情報を計測値格納ファイルに保存 */
		retCode = commfunc_tmstsfil_devset( PV_MKIND_BATTE, bttNumb, PV_BTTTESLA2_MAXTM, &s_bttData[workIdxNumb], &s_bttAvlData[workIdxNumb], ps_g_TMDATINF, ps_g_TMSTSFIL );

		if( threadStop_local != 0 )
		{
			break;
		}
	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &s_serial[workIdxNumb] );

	//★複数スレッドの対応が必要
	/* スレッド終了フラグＯＮ */
	ps_threadArg->edFlg = 1;

#ifdef PV_DEBUG
	printf("%s : IDX[%02d] PCS[%02d] OPEN-Thread END\n", pv_own_name, workIdxNumb, bttNumb );
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
int	func_bttData_recv(
	struct st_serialinf *ps_serial,
	int tranID,
	int bttNumb,
	int targetID,
	struct st_bttTeslaSdInf	*ps_bttSdInf,
	unsigned char *sendBuff,
	unsigned char *dataBuff,
	struct st_serialLog *ps_serialLog,
	int threadNumb)
{
	fd_set			rfds;					/* ファイルディスクリプタ集合 */
	unsigned char	readBuff[4096];			/* リードバッファ */
	char			flag_GOD = 0;			/* 受信完了フラグ */
	char			*workPoint;				/* データ格納バッファワークポインタ */
	int				readCnt = 0;			/* データ受信バイト数 */
	int				totalLength = 0;		/* 受信データ量 */
	int				dataLength = 0;			/* 受信データ長 */
	int				checkLength = 0;		/* 受信データ長（期待値） */
	int				dataByte = 0;			/* 受信データ内データバイト数（期待値） */
	struct timeval	timedata;				/* 現在時刻取得用 */
	struct timeval	timeout;				/* 応答タイムアウト時刻 */
	int				retCode;				/* 関数戻り値 */
	int				loopCnt;				/* ループカウンタ */
	int 			reqFunc;				/* Functionコード */
	int 			reqCnt;					/* データ回数 */


	reqFunc = ps_bttSdInf->funcCode;
	reqCnt = ps_bttSdInf->dataCntU * 256 + ps_bttSdInf->dataCntL;

	/* シリアルポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* ファンクションコードから受信データサイズを決定する（期待値） */
	switch( reqFunc )
	{
		case PV_BTTTESLA2_FUNC_10:
		case PV_BTTTESLA2_FUNC_06:
			checkLength = 12;
				break;
		case PV_BTTTESLA2_FUNC_03:
		default:
			dataByte = reqCnt * 2;
			checkLength = dataByte + 9;
			break;
	}

#ifdef PV_DEBUG
	printf("%s : Data-Recv TargetID: %d  (length:%d)  START\n",PV_OWN_NAME,targetID,checkLength);
#endif


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
					sprintf( errMsg, "Serial Port Not Open! Numb:%02d Id:%02d (%s)", bttNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20241017] Chg */
					return( PV_SERIAL_ERRNOTREADY );
					break;
				case PV_SERIAL_ERRTIMEOUT :
					sprintf( errMsg, "Read Timeout! DataLen:%d Numb:%02d Id:%02d (%s)", totalLength, bttNumb, targetID, ps_serial->targetName);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 要求実施回数を＋１ */
					ps_serial->requestCount += 1;
					/* 要求実施回数が上限に達している場合 */
					if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
					{
						/* 通信ステータス状変：通信不応答異常（発生） */
						func_btt_johen_stat( threadNumb, PV_ST01_NONRESPONSE, bttNumb, 1 );	/* [20241017] Chg */
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", bttNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20241017] Chg */
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
//printf("[%02X]", readBuff[loopCnt] );
#endif
			/* データバッファに受信データを格納 */
			*workPoint = readBuff[loopCnt];
			workPoint++;
			dataLength++;
			/* ９バイト分のデータを受信したら先頭データのチェックを行う */
			if( dataLength == 9 )
			{
				/* トランザクションIDの確認（データフレーム毎に固有の値を使用） */
				if( ( *dataBuff != (unsigned char)(tranID / 0x100) ) || ( *(dataBuff + 1) != (unsigned char)(tranID % 0x100) ) )		/* [20241111] Chg */
				{
#ifdef PV_DEBUG_SERIAL
printf("<TRAN-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv TransactionID Error! Numb:%02d Expect[%02X][%02X],Result[%02X][%02X]", bttNumb, (unsigned char)(tranID / 0x100), (unsigned char)(tranID % 0x100), *dataBuff, *(dataBuff+1) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* プロトコルIDの確認（本装置では固定的に0x0000を使用） */
				else if( ( *(dataBuff + 2) != 0 ) || ( *(dataBuff + 3) != 0 ) )
				{
#ifdef PV_DEBUG_SERIAL
printf("<PROT-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d Expect[00][00],Result[%02X][%02X]", bttNumb, *(dataBuff+2), *(dataBuff+3) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* 送信先（送信元）アドレスのチェック */
				else if( *(dataBuff + 6) != 0x01 )
				{
#ifdef PV_DEBUG_SERIAL
printf("<UNIT-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv Address Error! Numb:%02d Expect[%02X],Result[%02X]", bttNumb, 1, *(dataBuff+6) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* ファンクションコードが不一致の場合 */
				else if ( *(dataBuff+7) != (unsigned char)reqFunc ) 
				{
#ifdef PV_DEBUG_SERIAL
printf("<FUNC ERROR>");
#endif
					sprintf( errMsg, "Data-Recv function Error! Numb:%02d Expect[%02X],Result[%02X]", bttNumb, reqFunc, *(dataBuff+7) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 例外レスポンスの場合 */
					if( (*(dataBuff+7) & (unsigned char)0x80) == (unsigned char)0x80 )
					{
						/* 受信データ長（期待値）を５バイトに設定 */
						checkLength = 9;
					}
					else
					{
						/* 受信データの初期化 */
						dataLength = 0;
					}
				}
				/* ファンクションコードが０３でデータバイト数が異なる場合 */
				else if ( (reqFunc == PV_BTTTESLA2_FUNC_03) && ( ( *(dataBuff+5) != (dataByte + 3) ) || (  *(dataBuff+8) != dataByte ) ) )
				{
					sprintf( errMsg, "Data-Recv datasize Error! Numb:%02d Expect[%02X][%02X],Result[%02X][%02X]",
						bttNumb, dataByte + 3, dataByte, *(dataBuff+5), *(dataBuff+8) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
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
			printf("$s : Data-Poling Error\n",PV_OWN_NAME);
#endif
			/* 例外コードが不正ファンクションの場合 */
			if( *(dataBuff+8) == (unsigned char)0x01 )
			{
				/* 通信ステータス状変：下りコマンド異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST19_DWCOMMANDERR, bttNumb, 1 );	/* [20241017] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST22_DWPARAMERR, bttNumb, 1 );	/* [20241017] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST23_DWDATAERR, bttNumb, 1 );	/* [20241017] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが サーバ異常値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x04 )
			{
				/* 通信ステータス状変：デバイス異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST16_DEVICEERROR, bttNumb, 1 );	/* [20241017] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが ＢＵＳＹの場合 */
			/* 例外コードが 応答待ちの場合 */
			else if(( *(dataBuff+8) == (unsigned char)0x05 ) ||
			        ( *(dataBuff+8) == (unsigned char)0x06 ))
			{
				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", bttNumb, *(dataBuff+8));
				commfunc_syslog_msgset( pv_own_name, errMsg );
				/* 要求実施回数を＋１ */
				ps_serial->requestCount += 1;
				/* 要求実施回数が上限に達している場合 */
				if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
				{
					/* 通信ステータス状変：デバイスＢＵＳＹ（発生） */
					func_btt_johen_stat( threadNumb, PV_ST15_DEVICEBUSY, bttNumb, 1 );	/* [20241017] Chg */
				}
				/* タイムアウト */
				return( PV_SERIAL_ERRTIMEOUT );
			}
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST20_DWFORMATERR, bttNumb, 1 );	/* [20241017] Chg */
				return( PV_SERIAL_ERROTHER );
			}
		}
		else
		{
			/* ファンクションコードが書き込み(１０，６）の場合 */
			if(( reqFunc == PV_BTTTESLA2_FUNC_10 ) ||
			   ( reqFunc == PV_BTTTESLA2_FUNC_10 ))
			{
				/* 開始アドレスとレジスタ数が異なる場合 */
				/* （開始アドレスと設定値が異なる場合） */
				if( (*(dataBuff+8)  != sendBuff[8]  ) ||
				    (*(dataBuff+9)  != sendBuff[9]  ) ||
				    (*(dataBuff+10) != sendBuff[10] ) ||
				    (*(dataBuff+11) != sendBuff[11] ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					func_btt_johen_stat( threadNumb, PV_ST27_DWCONTROLERR, bttNumb, 1 );	/* [20241017] Chg */
					return( PV_SERIAL_ERROTHER );
				}
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
			func_btt_johen_stat( threadNumb, PV_ST01_NONRESPONSE, bttNumb, 1 );	/* [20241017] Chg */
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
 * 		引数 bttNumb で指定されたバッテリーについて以下の処理を行う。
 * 		引数 dataBuff で指定された受信バッファの内容から装置状態と計測値を取得する。
 * 		受信結果 resultCode が異常の場合は、計測値を０、計測ステータスを欠測に設定
 * 		する。
 * 		受信結果 resultCode が正常な場合は受信バッファのASCIIデータから装置状態と
 * 		計測値を取得し、計測ステータスを生成する。
 * 		装置状態が取得できた場合は、状態更新処理を行う。
 * 戻り値：
 * 		0：正常処理
 */
int	func_bttData_get( int bttType, int reqCnt, int bttNumb, int resultCode, struct st_bttData *ps_bttData, unsigned char *dataBuff)
{
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char		dataByte[8];			/* データ格納用領域 */
		short int			data16bit;				/* 16bitデータ取得用 */
		unsigned short int	data16bitu;				/* 16bitデータ取得用（符号無し） */
		int					data32bit;				/* 32bitデータ取得用 */
		float				data32f;				/* 32bitデータ取得用 */
		long				data32l;				/* 32bitデータ取得用 *//* [20241017] Add */
		unsigned long long 	data64bitu;				/* 64bitデータ取得用（符号無し） */
	} u_change;
	unsigned char	workStatus[4];					/* 装置状態一時保管用（2バイト分）*/
	int				codNumb;						/* 受信バッファ内の状態番号 */
	int				stsNumb;						/* 装置の状態番号 */
	int				tmNumb;							/* 計測情報番号 */
	int				workPoint;						/* 文字列開始ポインタ */
	int				loopCnt1;						/* ループカウンタ */
	int				loopCnt2;						/* ループカウンタ */
	int				loopCnt3;						/* ループカウンタ */
	int				bitMask;						/* ビットマスク */
	int				tmIfLinkBase;					/* 計測情報ファイルリンク */
	int				svStLinkBase;					/* 状態情報ファイルリンク */
	int				svIfLinkBase;					/* 装置状態情報ファイルリンク */
	struct st_bttTeslaSvInf	*ps_bttSvInf;			/* 装置状態情報取得情報 */
	struct st_bttTeslaTmInf	*ps_bttTmInf;			/* 計測値用情報取得情報 */
	struct st_devTmRange	*ps_bttTmRange;			/* 計測値範囲情報 */
	struct st_stsMaskTbl	*ps_bttMaskTbl;			/* 状態マスク情報 */
	struct st_stsCodeTbl	*ps_bttCodeTbl;			/* 数値変換情報 */
	long			tmVal;							/* 計測値 *//* [20241017] Chg */
	int				svStatFlg = 0;					/* SV情報更新有無 *//* [20241114] Add */

	/* 格納位置を取得 */
	tmIfLinkBase = ps_g_BATTEINF[bttNumb - 1].tmIfLink;
	svStLinkBase = ps_g_BATTEINF[bttNumb - 1].svStLink;
	svIfLinkBase = ps_g_BATTEINF[bttNumb - 1].svIfLink;

#ifdef PV_DEBUG_DT1
	printf("%s : PCS[ %02d ] TYPE[%d] REQ[%d] COD[%d] LINK[TMST:%6d, SVST:%6d, SVIF:%6d]\n",
		PV_OWN_NAME, bttNumb, bttType, reqCnt, resultCode, tmIfLinkBase, svStLinkBase, svIfLinkBase );
#endif

	/* 処理タイプ１の場合 */
	if( bttType == PV_BTTTESLA2_TYPE1 )
	{
		ps_bttSvInf = s_type1SvInf;
		ps_bttTmInf = s_type1TmInf;
		ps_bttTmRange  = s_type1TmRange;
	}
	else//if( bttType == PV_BTTTESLA2_TYPE2 )
	{
		ps_bttSvInf = s_type1SvInf;
		ps_bttTmInf = s_type1TmInf;
		ps_bttTmRange  = s_type1TmRange;
	}
	
	/* ------------------ */
	/* 装置状態情報の取得 */
	/* ------------------ */

	/* 受信処理結果が正常の場合のみ装置状態を取得 */
	if( resultCode == 0 )
	{
		/* ストッパーまで繰り返す */
		for( loopCnt1 = 0 ; ps_bttSvInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
		{
			/* 取得情報の要求回数が一致しない場合は次のデータへ */
			if( reqCnt != ps_bttSvInf[loopCnt1].reqCnt )
				continue;

			/* 処理タイプがビットタイプの場合 */
			if( ps_bttSvInf[loopCnt1].svType == PV_SVTYPE_BIT )
			{
				/* SV情報更新 */
				svStatFlg = 1;
#ifdef PV_DEBUG_SV
	printf("%s : BIT-TYPE\n",PV_OWN_NAME);
#endif
				/* 初期化が必要な場合 */
				if( ps_bttSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink ], 0, ps_bttSvInf[loopCnt1].iniSize );
				}

				/* 指定バイト数分コピーする */
				for( loopCnt2 = 1 ; loopCnt2 <= ps_bttSvInf[loopCnt1].dataSize ; loopCnt2++ )
				{
					/* 状態を取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
					workStatus[loopCnt2 - 1] = *(dataBuff + ps_bttSvInf[loopCnt1].buffLink + (ps_bttSvInf[loopCnt1].dataSize - loopCnt2));

					/* ビット数分繰り返す（MSB-LSB変換） */
					for( bitMask = 1, loopCnt3 = 0 ; loopCnt3 < 8 ; loopCnt3++, bitMask<<=1 )
					{
						if( workStatus[loopCnt2 - 1] & bitMask )
							ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2 - 1 ] |= (unsigned char)(0x80 >> loopCnt3);
#ifdef PV_DEBUG_SV
	/* 状態データバイト位置、装置状態データ */
	printf("PCSNISS2 : SV-STS[%2d] : %02X\n", ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2 - 1, (short int)ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2 - 1 ] );
#endif
					}
				}
			}
			/* 処理タイプがマスクタイプの場合（未使用） */
			else if( ps_bttSvInf[loopCnt1].svType == PV_SVTYPE_MSK )
			{
				/* SV情報更新 */
				svStatFlg = 1;
#ifdef PV_DEBUG_SV
	printf("%s : MSK-TYPE\n", PV_OWN_NAME);
#endif
				/* マスクテーブルポインタを取得 */
				ps_bttMaskTbl = (struct st_stsMaskTbl *)ps_bttSvInf[loopCnt1].ps_tblPointer;

				/* 初期化が必要な場合 */
				if( ps_bttSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink ], 0, ps_bttSvInf[loopCnt1].iniSize );
				}

				/* 状態テーブルの設定数分繰り返す */
				for( loopCnt2 = 0 ; ps_bttMaskTbl[loopCnt2].buffLink != -1 ; loopCnt2++ )
				{
					/* ビットが設定されている場合 */
					if( *(dataBuff + ps_bttMaskTbl[loopCnt2].buffLink) & ps_bttMaskTbl[loopCnt2].svMask )
					{
						/* ビット状態を設定 */
						ps_bttData->bttStatus[ ps_bttMaskTbl[loopCnt2].bttSvLink ] |= ps_bttMaskTbl[loopCnt2].setMask;
					}
				}

#ifdef PV_DEBUG_SV
	for(loopCnt2 = 0 ; loopCnt2 < ps_bttSvInf[loopCnt1].iniSize ; loopCnt2++ )
	printf("%s :    SV-STS[%2d] : %02X\n", PV_OWN_NAME, ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2, ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2 ] );
#endif
			}
			/* 処理タイプが数値タイプの場合（未使用） */
			else if( ps_bttSvInf[loopCnt1].svType == PV_SVTYPE_NUM )
			{
				/* SV情報更新 */
				svStatFlg = 1;
				/* コード変換テーブルポインタを取得 */
				ps_bttCodeTbl = (struct st_stsCodeTbl *)ps_bttSvInf[loopCnt1].ps_tblPointer;

				/* 初期化が必要な場合 */
				if( ps_bttSvInf[loopCnt1].iniSize > 0 )
				{
					/* 状態を初期化 */
					memset( &ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink ], 0, ps_bttSvInf[loopCnt1].iniSize );
				}

				/* 状態・コード番号の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
				for( loopCnt2 = 1 ; loopCnt2 <= ps_bttSvInf[loopCnt1].dataSize ; loopCnt2++ )
					u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_bttSvInf[loopCnt1].buffLink + ps_bttSvInf[loopCnt1].dataSize - loopCnt2];

				/* サイズが２バイトサイズの場合 */
				if( ps_bttSvInf[loopCnt1].dataSize == 2 )
				{
					/* 16bitサイズのデータとして数値を取得 */
					codNumb = u_change.data16bit;
				}
				/* サイズが４バイトサイズの場合 */
				else if( ps_bttSvInf[loopCnt1].dataSize == 4 )
				{
					/* 32bitサイズのデータとして数値を取得 */
					codNumb = u_change.data32bit;
				}
				else
				{
					/* 8bitサイズのデータとして数値を取得 */
					codNumb = u_change.dataByte[0];
				}

				/* コード番号から状態番号を取得 */
				for( stsNumb = -1, loopCnt2 = 0 ; ps_bttCodeTbl[loopCnt2].codNumb != -1 ; loopCnt2++ )
				{
					/* 変換テーブルのコード番号がストッパーの場合 */
					if( ps_bttCodeTbl[loopCnt2].codNumb == -1 )
					{
						/* 状態番号を設定 */
						stsNumb = ps_bttCodeTbl[loopCnt2].stsNumb;
						break;
					}
					/* 変換テーブルのコード番号が一致する場合 */
					if( ps_bttCodeTbl[loopCnt2].codNumb == codNumb )
					{
						/* 状態番号を設定 */
						stsNumb = ps_bttCodeTbl[loopCnt2].stsNumb;
						break;
					}
				}

#ifdef PV_DEBUG_SV
				printf("%s : NUM-TYPE CODE[ %d ] SV-NUM[ %d ]\n", PV_OWN_NAME, codNumb, stsNumb );
#endif

				/* 状態番号が１以上の場合 */
				if( stsNumb > 0 )
				{
					/* 状態を更新 */
					ps_bttData->bttStatus[ (stsNumb - 1) / 8 ] |= (unsigned char)0x80 >> ((stsNumb - 1) % 8);
				}
				/* コード番号に該当する状態番号が見つからなかった場合 */
				else if( stsNumb == -1 )
				{
					sprintf( errMsg, "SV Status Number Not Found !! PCS-No[%02d] Type[%d] Buff[%d] Size[%d] NUMBER[%d]",
						bttNumb, bttType, ps_bttSvInf[loopCnt1].buffLink, ps_bttSvInf[loopCnt1].dataSize, codNumb );
						commfunc_syslog_msgset( pv_own_name, errMsg );
				}
			}

#ifdef PV_DEBUG_SV
//			for( loopCnt2 = 0 ; loopCnt2 < sizeof(ps_bttData->bttStatus); loopCnt2++ )
//			{
//				printf("%s : SV-STS[%2d] : %02X\n",
//					PV_OWN_NAME, ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2, ps_bttData->bttStatus[ ps_bttSvInf[loopCnt1].bttSvLink + loopCnt2 ] );
//			}
#endif
		}

		/* SV情報更新があった場合 */
		if( svStatFlg == 1 )
		{
			/* 装置状態情報の更新（ビットステータス形式で更新） */
			commfunc_johen_sv( PV_MKIND_BATTE, bttNumb, PV_MAX_BATTE_SVPOS, ps_bttData->bttStatus,
				svIfLinkBase, svStLinkBase, 
				ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
	}

	/* ------------------------------------ */
	/* 通常形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */

	/* ストッパーまで繰り返す */
	for( loopCnt1 = 0 ; ps_bttTmInf[loopCnt1].reqCnt != -1 ; loopCnt1++ )
	{
		/* 取得情報の要求回数が一致しない場合は次のデータへ */
		if( reqCnt != ps_bttTmInf[loopCnt1].reqCnt )
			continue;

		/* 計測情報番号を取得 */
		tmNumb = ps_bttTmInf[loopCnt1].bttTmNumb - 1;

		/* 受信結果が異常 または 計測情報が未実装の場合 */
		if( ( resultCode != 0 ) || ( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 ) )
		{
			/* 未実装の場合 */
			if( ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].statFlag == 0 )
			{
				/* 計測ステータスをリセット */
				ps_bttData->tmFlgData[tmNumb] = 0;
			}
			else
			{
				/* 計測ステータスに欠測を設定 */
				ps_bttData->tmFlgData[tmNumb] = PV_TMFLG_DROPTM;
			}
			/* 計測値を０に設定 */
			ps_bttData->tmValData[tmNumb] = 0;

#ifdef PV_DEBUG_TM
		printf("%s : TM[%2d] ERR FLG:VAL[%Xh:%9d]  NAME:[%s]\n",
			PV_OWN_NAME, tmNumb+1, ps_bttData->tmFlgData[tmNumb], ps_bttData->tmValData[tmNumb], ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
			continue;
		}
		/* 受信結果正常 かつ 実装の場合 */
		else
		{
			/* 変換用領域の初期化 */
			memset( &u_change, 0, sizeof(u_change));
	
			/* 計測値の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
			for( loopCnt2 = 1 ; loopCnt2 <= ps_bttTmInf[loopCnt1].dataSize ; loopCnt2++ )
				u_change.dataByte[loopCnt2 - 1] = dataBuff[ps_bttTmInf[loopCnt1].buffLink + ps_bttTmInf[loopCnt1].dataSize - loopCnt2];

/* [20241017] Chg St */
			/* 計測値のサイズが２バイトサイズの場合 */
			if( ( ps_bttTmInf[loopCnt1].dataSize == 2 ) && ( ps_bttTmInf[loopCnt1].type == 2 ) )
			{
				/* 符号なしbitサイズのデータとして計測値を取得 */
				tmVal = u_change.data16bitu;
			}
			else if ( ( ps_bttTmInf[loopCnt1].dataSize == 2 ) && ( ps_bttTmInf[loopCnt1].type == 1 ) )
			{
				/* 符号付き16bitサイズのデータとして計測値を取得 */
				tmVal = u_change.data16bit;
			}
			else if ( ( ps_bttTmInf[loopCnt1].dataSize == 4 ) && ( ps_bttTmInf[loopCnt1].type == 2 ) )
			{
				/* 32bitサイズのfloat型データとして計測値を取得 */
				tmVal = (long)(u_change.data32f * commfunc_pow10abs(ps_g_TMDATINF[ tmIfLinkBase + tmNumb ].expData));
#ifdef PV_DEBUG_FREQ
				if( tmNumb == 21 )
				{
					printf("Freq tmNumb=[%d] tmVal=[%ld] u_change.data32f=[%.06f]\n",tmNumb, tmVal, u_change.data32f);
				}
#endif /* PV_DEBUG_FREQ */
			}
			else
			{
				tmVal = u_change.data32l;
				sprintf( errMsg, "TM dataSize and type not sport !! dataSize[%02d] type[%d] value(long)[%ld]",
						ps_bttTmInf[loopCnt1].dataSize, ps_bttTmInf[loopCnt1].type, tmVal );
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
/* [20241017] Chg Ed */

			/* 計測値が既定値の範囲内か？ */
			if( ( ps_bttTmRange[tmNumb].tmLower > tmVal ) ||
				( ps_bttTmRange[tmNumb].tmUpper < tmVal ) )
			{
				/* 計測異常２をセット */
				/* 計測フラグ->計測値の順で更新する */
				ps_bttData->tmFlgData[tmNumb] = PV_TMFLG_TMERR2;
				ps_bttData->tmValData[tmNumb] = tmVal;
			}
			else
			{
				/* 計測フラグを正常にセット */
				/* 計測値->計測フラグの順で更新する */
				ps_bttData->tmValData[tmNumb] = tmVal;
				ps_bttData->tmFlgData[tmNumb] = PV_TMFLG_NORMAL;
			}
		}

#ifdef PV_DEBUG_TM
		printf("%s : TM[%2d] NOR FLG:VAL[%Xh:%9d] UP[%9d] LW[%9d] NAME:[%s]\n",
			PV_OWN_NAME, tmNumb+1, ps_bttData->tmFlgData[tmNumb], ps_bttData->tmValData[tmNumb], ps_bttTmRange[tmNumb].tmUpper, ps_bttTmRange[tmNumb].tmLower, ps_g_TMDATINF[tmIfLinkBase + tmNumb].tmName );
#endif
	}

	/* ------------------------------------ */
	/* 特殊形式で取得可能な計測値情報の取得 */
	/* ------------------------------------ */
/* /* [20230312] Add St */
	/* バッテリーメータの計測値が全て0の場合、 */
	/* 放電電力量/充電電力量を欠測とする */
	tmVal = 0;
	/* ストッパーまで繰り返す */
	for( loopCnt1 = 0 ; s_stsMeterTbl_Type1_Battery[loopCnt1].tmNumb != -1 ; loopCnt1++ )
	{
		tmNumb = s_stsMeterTbl_Type1_Battery[loopCnt1].tmNumb -1;
		/* 計測値が0以外の場合は、保持 */
		if( ps_bttData->tmValData[tmNumb] != 0 )
		{
			tmVal = ps_bttData->tmValData[tmNumb];
		}
	}
	/* 全ての計測値が0の場合 */
	if( tmVal == 0 )
	{
		for( loopCnt1 = 0 ; s_stsMeterTbl_Type1_Battery[loopCnt1].tmNumb != -1 ; loopCnt1++ )
		{
			tmNumb = s_stsMeterTbl_Type1_Battery[loopCnt1].tmNumb -1;
			/* 蓄積値の場合は、欠測に修正 */
			if( s_stsMeterTbl_Type1_Battery[loopCnt1].tntegratedFlg != 0 )
			{
				ps_bttData->tmFlgData[tmNumb] = PV_TMFLG_DROPTM;
			}
		}
	}
/* [20230312] Add Ed */

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

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * SoC情報更新
 *
 * 機能：
 * 		SoC情報を更新する
 * 		
 * 戻り値：
 * 		0：正常処理
 */
void func_set_soc( int bttIndex, struct st_bttData *pbttData )
{
	int		tmFlg = PV_TMFLG_DROPTM;
	long	tmVal = 0;/* [20241017] Chg */
	int		soNumb;
	int		fullenNumb;
	int		remainNumb;
	int		tmNumb;
	int		expData;

	/* 装置番号を取得 */
	soNumb = s_bttInfo[bttIndex].devNumb;

	/* SoC算出に必要な計測番号を取得 */
	tmNumb = s_typeCalcInf[s_bttInfo[bttIndex].devType -1].socNumb;
	fullenNumb = s_typeCalcInf[s_bttInfo[bttIndex].devType -1].fullenNumb;
	remainNumb = s_typeCalcInf[s_bttInfo[bttIndex].devType -1].remainNumb;

	/* 乗率を取得 */
	expData = ps_g_TMDATINF[ ps_g_BATTEINF[soNumb -1 ].tmIfLink + tmNumb -1 ].expData;

	/* 型式毎にSoCを算出する */
//	if( s_bttInfo[bttIndex].devType == PV_BTTTESLA2_TYPE1 )	/* [20230529] Del */
	{
		/* フル充電エネルギー、又は、残電力量が正常な場合 */
		if( ( pbttData[bttIndex].tmFlgData[fullenNumb-1] == PV_TMFLG_NORMAL ) &&
		    ( pbttData[bttIndex].tmFlgData[remainNumb-1] == PV_TMFLG_NORMAL ) )
		{
			tmFlg = PV_TMFLG_NORMAL;

/* [20200831] Chg Ed */
			/* フル充電エネルギーが0以下の場合 */
			if( pbttData[bttIndex].tmValData[fullenNumb-1] <= 0 )
			{
				/* SoCを欠測にする */
				tmFlg = PV_TMFLG_DROPTM;
			}
			/* 残電力量が0以下の場合 */
			else if( pbttData[bttIndex].tmValData[remainNumb-1] <= 0 )
/* [20200831] Chg Ed */
			{
				/* SoCは0%に設定 */
				tmVal = 0;
			}
			/* フル充電エネルギーが残電力量以下の場合 */
			else if( pbttData[bttIndex].tmValData[fullenNumb-1] <= pbttData[bttIndex].tmValData[remainNumb-1] )
			{
				/* SoCは100%に設定 */
				tmVal = 1 * commfunc_pow10abs(expData-2);
			}
			else
			{
				/* SoCを算出 */
				tmVal = (int)(((long long)pbttData[bttIndex].tmValData[remainNumb-1] * commfunc_pow10abs(expData-2)) / pbttData[bttIndex].tmValData[fullenNumb-1]);
			}
		}
	}

	/* SoCの計測値を保存する */
	if( tmFlg == PV_TMFLG_NORMAL )
	{
		/* 計測値ステータスが正常な場合、計測値->計測フラグの順で更新する */
		pbttData[bttIndex].tmValData[tmNumb-1] = tmVal;
		pbttData[bttIndex].tmFlgData[tmNumb-1] = tmFlg;
	}
	else
	{
		/* 計測値ステータスが異常な場合、計測フラグ->計測値の順で更新する */
		pbttData[bttIndex].tmFlgData[tmNumb-1] = tmFlg;
		pbttData[bttIndex].tmValData[tmNumb-1] = tmVal;
	}

	return;
}

/* [20230721] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 外部設定値取得
 *
 * 機能：
 * 		外部ファイルからパラメータを取得する
 */
int func_get_extraParam( char *pc_filePath, time_t *pt_time, struct st_extraParam *ps_bttExtraParam  )
{
	int						retCode;					/* 戻り値 */
	char					getStr[128];				/* 取得文字列 */
	struct st_extraParam 	extraParam;					/* ローカル変数 */

	/* パラメータ初期化 */
	memset(&extraParam, 0, sizeof(extraParam));
	
	/* 設定ファイルのチェック */
	retCode = commfunc_file_info_get( pc_filePath, pt_time, PV_BTTTESLA2_PARAM1, getStr );
	if( retCode == COM_PARAM_GET_OK )
	{
		/* 実装済みに変更 */
		extraParam.statFlag = 1;

		/* 充電力率設定値を更新 */
		extraParam.chargeSetpoint = atof(getStr);

		retCode = commfunc_file_info_get( pc_filePath, NULL, PV_BTTTESLA2_PARAM2, getStr );
		if( retCode == COM_PARAM_GET_OK )
		{
			/* 放電力率設定値の更新 */
			extraParam.dischargeSetpoint = atof(getStr);

			retCode = commfunc_file_info_get( pc_filePath, NULL, PV_BTTTESLA2_PARAM3, getStr );
			if( retCode == COM_PARAM_GET_OK )
			{
				/* Droop機能応答時間の更新 */
				extraParam.droopTimeResponse = atof(getStr);

				sprintf( errMsg, "Updated reactive mode parameters.");
				commfunc_syslog_msgset( pv_own_name, errMsg );
				sprintf( errMsg, " %s=%f", PV_BTTTESLA2_PARAM1, extraParam.chargeSetpoint );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				sprintf( errMsg, " %s=%f", PV_BTTTESLA2_PARAM2, extraParam.dischargeSetpoint );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				sprintf( errMsg, " %s=%f", PV_BTTTESLA2_PARAM3, extraParam.droopTimeResponse );
				commfunc_syslog_msgset( pv_own_name, errMsg );

				/* 力率励起を設定 */
				extraParam.chargeLead = 1;
				if( extraParam.chargeSetpoint < 0 )
				{
					/* 絶対値に変更 */
					extraParam.chargeSetpoint *= -1;
					extraParam.chargeLead = -1;
				}

				/* 力率励起を設定 */
				extraParam.dischargeLead = 1;
				if( extraParam.dischargeSetpoint < 0 )
				{
					/* 絶対値に変更 */
					extraParam.dischargeSetpoint *= -1;
					extraParam.dischargeLead = -1;
				}

				/* 設定情報をコピー */
				memcpy( ps_bttExtraParam, &extraParam, sizeof(extraParam));
			}
		}
	}

	return retCode;
}
/* [20230721] Add Ed */

/* [20241017] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * バッテリーＴＣＰ／ＩＰ通信接続処理(周波数取得)
 *
 * 機能：
 *		引数 args で指定された情報内に格納されているバッテリーに対
 *		して、接続を行う（オープンからコネクトまで）
 *		実際の接続処理は共通関数 commfunc_serial_open() を使用する。
 *		※func_bttOpen_threadを周波数取得を目的に読込専用化したもの
 *		
 */
void *func_bttOpen_bttMtThread( void *args )
{
#ifdef PV_DEBUG_FREQ
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char		dataByte[8];			/* データ格納用領域 */
		short int			data16bit;				/* 16bitデータ取得用 */
		unsigned short int	data16bitu;				/* 16bitデータ取得用（符号無し） */
		int					data32bit;				/* 32bitデータ取得用 */
		float				data32f;				/* 32bitデータ取得用 */
		long				data32l;				/* 32bitデータ取得用 *//* [20241017] Add */
		unsigned long long 	data64bitu;				/* 64bitデータ取得用（符号無し） */
	} u_change;
#endif /* PV_DEBUG_FREQ */
	unsigned  char			*p_sendBuff;				/* 送信バッファポンタ */
	unsigned  char			sendBuff_r[12] = {
								0x00,0x00,				/* トランザクションID（本装置では固定的に0を使用） */
								0x00,0x00,				/* プロトコルID（MODBUS=0を使用） */
								0x00,0x06,				/* データ長 */
								0x01,					/* 送信先局番（固定値0x01を使用） */
								0x00,					/* ファンクションコード（保持レジスタ読出） */
								0x00,0x00,				/* 開始アドレス */
								0x00,0x00				/* レジスタの数 */
							};
	int						dataLength;					/* 送信データ長 */
	unsigned char			readBuff[4096];				/* 受信バッファ */
	int						openRetCode = -1;			/* シリアルポートオープン結果 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt2;					/* ループカウンタ */

	int						bttNumb;					/* 処理中のバッテリー番号 */
	int						idNumb;						/* 処理中のバッテリー装置ＩＤ */
	struct st_serialLog		s_serialLog;				/* シリアル通信ログ格納領域 */
	struct timespec			sleepTm;					/* 送信待ちタイマ */
	struct st_bttTeslaSdInf	*ps_bttSdInf;				/* 送信情報ポインタ */
	int						workIdxNumb;				/* 処理インデックス番号 */
	struct st_threadArg		*ps_threadArg;				/* スレッド引数処理用 */
	int						reqKind = 0;				/* 要求種別フラグ */
	int						threadStop_local;			/* 停止フラグ(thread用) */
	int						tranID;						/* トランザクションID *//* [20241111] Add */
	int						lineNo;						/* 通信回線番号 *//* [20241017] Add */
	int						threadNumb;					/* スレッド番号 *//* [20241017] Add */

	ps_threadArg = (struct st_threadArg *)args;

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));

	/* 処理インデックス番号の取得 */
	workIdxNumb = ps_threadArg->indexNumb;

	/* パワコン番号の取得 */
	bttNumb = s_bttInfo[workIdxNumb].devNumb;
	/* バッテリー装置IDの取得 */
	idNumb  = s_bttInfo[workIdxNumb].idNumb ;
	/* スレッド番号の取得 */
	threadNumb = ps_threadArg->threadNumb;	/* [20241017] Add */

#ifdef PV_DEBUG
	printf("%s :bttMtThread IDX[%02d] PCS[%02d] OPEN-Thread START\n", pv_own_name, workIdxNumb, bttNumb );
#endif

	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = s_bttMtSerial[workIdxNumb].sndTime / 1000;
	sleepTm.tv_nsec = (s_bttMtSerial[workIdxNumb].sndTime % 1000) * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( 1 )
	{
		/* 停止フラグを保持(ループの最初から停止フラグの情報を使用するようにここで保持する) */
		threadStop_local = threadStop;

		/* ポートがオープンしていない場合 */
		if( s_bttMtSerial[workIdxNumb].status != PV_STAT_COM_CONNECT )
		{
			/* 要求種別を「通信復帰時」に変更 */
			reqKind = PV_BTTTESLA2_REQ_CB;
		}
#ifdef PV_DEBUG_BMT
		sprintf( errMsg, "s_bttMtSerial status[%d] requestCount[%d] fd_sockfd[%d] ttyUSBFlag[%d] lineNo[%d] deviceType[%d] targetName[%s] log_open[%d] locIPAddr[%d] dstIPAddr[%d] portNumb[%d] portNumbLoc[%d] fd_sockfdUdp[%d] baudRate[%d] dataSize[%d] parity[%d] stopBit[%d] rcvTime[%d] sndTime[%d] cycleTime[%d]",
			s_bttMtSerial[workIdxNumb].status,
			s_bttMtSerial[workIdxNumb].requestCount,
			s_bttMtSerial[workIdxNumb].fd_sockfd,
			s_bttMtSerial[workIdxNumb].ttyUSBFlag,
			s_bttMtSerial[workIdxNumb].lineNo,
			s_bttMtSerial[workIdxNumb].deviceType,
			s_bttMtSerial[workIdxNumb].targetName,
			s_bttMtSerial[workIdxNumb].log_open,
			s_bttMtSerial[workIdxNumb].locIPAddr,
			s_bttMtSerial[workIdxNumb].dstIPAddr,
			s_bttMtSerial[workIdxNumb].portNumb,
			s_bttMtSerial[workIdxNumb].portNumbLoc,
			s_bttMtSerial[workIdxNumb].fd_sockfdUdp,
			s_bttMtSerial[workIdxNumb].baudRate,
			s_bttMtSerial[workIdxNumb].dataSize,
			s_bttMtSerial[workIdxNumb].parity,
			s_bttMtSerial[workIdxNumb].stopBit,
			s_bttMtSerial[workIdxNumb].rcvTime,
			s_bttMtSerial[workIdxNumb].sndTime,
			s_bttMtSerial[workIdxNumb].cycleTime,
			s_bttMtSerial[workIdxNumb].status);
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* PV_DEBUG_BMT */
		/* シリアルポートをオープン */
		openRetCode = commfunc_serial_open( &s_bttMtSerial[workIdxNumb] );
		if( openRetCode >= 0 )
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（復帰） */
			func_btt_johen_stat( threadNumb, PV_ST14_LANERR, bttNumb, 0 );/* [20241017] Chg */

			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 0;

		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			func_btt_johen_stat( threadNumb, PV_ST14_LANERR, bttNumb, 1 );/* [20241017] Chg */

#ifdef PV_DEBUG
		printf("%s :bttMtThread IDX[%02d] PCS[%02d] Thread OPEN ERROR!!\n", pv_own_name, workIdxNumb, bttNumb );
#endif
			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 1;
			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 5 );
		}

		/* 要求実施回数を初期化 */
		s_bttMtSerial[workIdxNumb].requestCount = 0;

		/* 処理タイプの設定 */
		/* 送信情報を処理タイプ１に設定 */
		ps_bttSdInf = s_type1SdInf;

		/* 戻り値初期化 */
		retCode = 0;
		/* 複数回に分けて情報を収集する */
		for( loopCnt2 = 0 ; ps_bttSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
		{
#ifdef PV_DEBUG
			printf("%s :freqThread LOOP:%02d BTTNO:%02d BTTID-No:%02d REQUEST:%d OPN-STS:%d, retCode=%d\n", PV_OWN_NAME, workIdxNumb+1, bttNumb, idNumb, loopCnt2+1, s_bttMtSerial[workIdxNumb].status, retCode );
#endif
/* [20241017] Add St */
			/* 要求毎の設定 */
			if ( ps_bttSdInf[loopCnt2].addrType != CMD_GET_BATTERY_METER )
			{
				/* バッテリメータの情報専用で処理するため他はスキップ */
				continue;
			}
/* [20241017] Add Ed */
			/* 送信バッファの設定 */
			p_sendBuff = &sendBuff_r[0];

			/* 送信待ち処理 */
			nanosleep( &sleepTm, NULL);

			/* 通信状態が正常の場合、且つ、前回処理が正常の場合 */
			if( ( s_bttMtSerial[workIdxNumb].status == PV_STAT_COM_CONNECT ) &&
				( retCode == 0 ))
			{
#ifdef PV_DEBUG_SERIAL
				printf("%s : SERIAL POERT OPENED\n", PV_OWN_NAME);
#endif
/* [20241111] Add St */
				p_sendBuff[0]  = idNumb;
				p_sendBuff[1]  = loopCnt2 + 1;
/* [20241111] Add Ed */
				p_sendBuff[5] = ps_bttSdInf[loopCnt2].dataLength - 6;
				p_sendBuff[7] = ps_bttSdInf[loopCnt2].funcCode;
				p_sendBuff[8] = ps_bttSdInf[loopCnt2].startAdU;
				p_sendBuff[9] = ps_bttSdInf[loopCnt2].startAdL;
				p_sendBuff[10] = ps_bttSdInf[loopCnt2].dataCntU;
				p_sendBuff[11] = ps_bttSdInf[loopCnt2].dataCntL;
				dataLength = ps_bttSdInf[loopCnt2].dataLength;
				/* 送信バッファにファンクションと取得開始アドレス、取得データ数を設定 */
				tranID = (p_sendBuff[0] * 0x100) + p_sendBuff[1];	/* [20241111] Add */

				/* 要求が種別が一致しない場合 */
				if( ( reqKind & ps_bttSdInf[loopCnt2].reqKind) == 0 )
				{
					/* 処理をスキップする */
					continue;
				}

				/* 処理結果の初期値を異常に設定 */
				retCode = -1;

				/* 要求実施回数の上限まで繰り返す */
				/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
				while( s_bttMtSerial[workIdxNumb].requestCount < PV_SERIAL_REQUEST_MAX )
				{
					/* 要求のリトライは行わない */
					/* シリアル通信ログ領域を初期化 */
					s_serialLog.dataCnt = 0;

					/* バッテリーに要求を送信 */
					retCode = commfunc_serial_send( &s_bttMtSerial[workIdxNumb], p_sendBuff, dataLength, &s_serialLog );
					if( retCode < 0 )
					{
						sprintf( errMsg, "freqThread TCP Port Write Error! RET:%d num/id:%02d/%02d (%s)",
							retCode, bttNumb, idNumb, s_bttMtSerial[workIdxNumb].targetName );
						commfunc_syslog_msgset( pv_own_name, errMsg );

						/* データ送信異常（送信不可異常）発生 */
						/* 状変（通信ステータス状変）発生 */
						func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20241017] Chg */

						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* データ送信異常（送信不可異常）復帰 */
						/* 状変（通信ステータス状変）復帰 */
						func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 0 );	/* [20241017] Chg */
					}

					/* データ処理バッファをクリア */
					memset(readBuff, 0, sizeof(readBuff));
	
					/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
					retCode = func_bttData_recv( &s_bttMtSerial[workIdxNumb], tranID, bttNumb, idNumb,	/* [20241111] Chg */
												 &ps_bttSdInf[loopCnt2],
												 p_sendBuff, readBuff, &s_serialLog, threadNumb );		/* [20241017] Chg */
					if( retCode == 0 )
					{
#ifdef PV_DEBUG_BMT
						sprintf( errMsg, "btttest read thread func_bttData_recv OK threadNo=%d %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
								workIdxNumb, readBuff[0],readBuff[1],readBuff[2],readBuff[3],readBuff[4],readBuff[5],readBuff[6],readBuff[7],readBuff[8],readBuff[9],
								readBuff[10],readBuff[11],readBuff[12],readBuff[13],readBuff[14],readBuff[15],readBuff[16],readBuff[17],readBuff[18],readBuff[19],
								readBuff[20],readBuff[21],readBuff[22],readBuff[23],readBuff[24],readBuff[25],readBuff[26],readBuff[27],readBuff[28],readBuff[29],
								readBuff[30],readBuff[31],readBuff[32],readBuff[33],readBuff[34],readBuff[35],readBuff[36],readBuff[37],readBuff[38],readBuff[39],
								readBuff[40],readBuff[41],readBuff[42],readBuff[43],readBuff[44],readBuff[45],readBuff[46],readBuff[47],readBuff[48],readBuff[49]
						);
						commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* PV_DEBUG_BMT */
#ifdef PV_DEBUG_FREQ
						u_change.dataByte[0] = readBuff[28];
						u_change.dataByte[1] = readBuff[27];
						u_change.dataByte[2] = readBuff[26];
						u_change.dataByte[3] = readBuff[25];
						printf( "btttest read thread func_bttData_recv OK threadNo=%d read[25][26][27][28]=[%02x][%02x][%02x][%02x] little=%02x%02x%02x%02x float3=%.03f float6=%.06f\n",
								workIdxNumb, readBuff[25], readBuff[26], readBuff[27], readBuff[28], readBuff[28], readBuff[27], readBuff[26], readBuff[25], u_change.data32f, u_change.data32f
						);
#endif /* PV_DEBUG_FREQ */
						/* 正常データ受信完了 */
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_AUTO, &s_bttMtSerial[workIdxNumb], &s_serialLog );
						/* 通信ステータス状変：全ステータス（復帰） */
						func_btt_johen_resetstat( threadNumb, bttNumb );	/* [20241017] Chg */

						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_ON, &s_bttMtSerial[workIdxNumb], &s_serialLog );
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
				printf("%s :freqThread SERIAL POERT NOT-OPENED STATUS[%d] stFlg[%d]\n", PV_OWN_NAME, s_bttMtSerial[workIdxNumb].status, s_threadArg[workIdxNumb].stFlg );
#endif
				/* シリアルポートが異常の場合 */
				retCode = -1;
			}

			/* 読み込み処理のみ計測情報を格納する */
			if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA2_FUNC_03 )
			{
				/* データ処理バッファから装置状態と計測値を抽出 */
				func_bttData_get( s_bttInfo[workIdxNumb].devType, loopCnt2 + 1, bttNumb, retCode, &s_bttData[workIdxNumb], readBuff );
			}

		}

		/* ループ処理の終端、且つ、正常終了の場合 */
		if( ps_bttSdInf[loopCnt2].funcCode == (unsigned char)0xFF )
		{

			if( retCode == 0 )
			{
				/* 通信復帰時の場合 */
				if( reqKind == PV_BTTTESLA2_REQ_CB )
				{
					/* 通常時に遷移 */
					reqKind = PV_BTTTESLA2_REQ_NOR;
				}
			}
		}

		/* 計測種別毎の保持 */
		retCode = commfunc_devTmData_set( PV_MKIND_BATTE, bttNumb, PV_BTTTESLA2_MAXTM, &s_bttData[workIdxNumb], &s_bttAvlData[workIdxNumb], ps_g_TMDATINF );

		/* 計測情報を計測値格納ファイルに保存 */
		retCode = commfunc_tmstsfil_devset( PV_MKIND_BATTE, bttNumb, PV_BTTTESLA2_MAXTM, &s_bttData[workIdxNumb], &s_bttAvlData[workIdxNumb], ps_g_TMDATINF, ps_g_TMSTSFIL );

		if( threadStop_local != 0 )
		{
			break;
		}
	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &s_bttMtSerial[workIdxNumb] );

	//★複数スレッドの対応が必要
	/* スレッド終了フラグＯＮ */
	ps_threadArg->edFlg = 1;

#ifdef PV_DEBUG
	printf("%s :freqThread  IDX[%02d] PCS[%02d] OPEN-Thread END\n", pv_own_name, workIdxNumb, bttNumb );
#endif

	return( NULL );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状変処理（単独処理）
 *
 * 機能：
 * 		引数 threadNo で指定された通信ステータス状変格納変数を更新する。
 * 		引数 stNumb で指定された通信ステータスを更新する。
 * 		引数 bttNumb で指定された蓄電池用PCSについて以下の処理を行う。
 * 		引数 status で指定された通信ステータス状変発生復帰の値を使用する。
 * 		読込、書込のどちらかでも1の場合発生、双方復帰の場合、復帰を設定する
 * 戻り値：
 * 		0：正常処理
 */
void func_btt_johen_stat( int threadNumb, int stNumb, int bttNumb, int status )
{
#ifdef PV_DEBUG_THREAD_ST
	char			errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ *//* [20231023] Add */
#endif /* PV_DEBUG_THREAD_ST */

	/* それぞれの領域を更新する */
	if( threadNumb == 0 )
	{
		rdwt_thred_stList[stNumb - 1] = status;
	}
	else
	{
		read_thred_stList[stNumb - 1] = status;
	}
	/* どちらかのスレッドで1がある場合、発生 */
	if( rdwt_thred_stList[stNumb - 1] == 1 || read_thred_stList[stNumb - 1] == 1 )
	{
		join_stList[stNumb - 1] = 1;
	}
	/* どちらのスレッドも1がない場合、復帰 */
	else
	{
		join_stList[stNumb - 1] = 0;
	}
#ifdef PV_DEBUG_THREAD_ST
	sprintf( errMsg, "func_btt_johen_stat threadNumb = %d                     01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 21 24 25 26 27 28 29 30 31 32", rdwtFlg);
	commfunc_syslog_msgset( pv_own_name, errMsg );
	sprintf( errMsg, "func_btt_johen_stat threadNumb = %d rdwt_thred_stList = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", threadNumb, rdwt_thred_stList[0],rdwt_thred_stList[1],rdwt_thred_stList[2],rdwt_thred_stList[3],rdwt_thred_stList[4],rdwt_thred_stList[5],rdwt_thred_stList[6],rdwt_thred_stList[7],rdwt_thred_stList[8],rdwt_thred_stList[9],rdwt_thred_stList[10],rdwt_thred_stList[11],rdwt_thred_stList[12],rdwt_thred_stList[13],rdwt_thred_stList[14],rdwt_thred_stList[15],rdwt_thred_stList[16],rdwt_thred_stList[17],rdwt_thred_stList[18],rdwt_thred_stList[19],rdwt_thred_stList[20],rdwt_thred_stList[21],rdwt_thred_stList[22],rdwt_thred_stList[23],rdwt_thred_stList[24],rdwt_thred_stList[25],rdwt_thred_stList[26],rdwt_thred_stList[27],rdwt_thred_stList[28],rdwt_thred_stList[29],rdwt_thred_stList[30],rdwt_thred_stList[31]);
	commfunc_syslog_msgset( pv_own_name, errMsg );
	sprintf( errMsg, "func_btt_johen_stat threadNumb = %d read_thred_stList = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", threadNumb, read_thred_stList[0],read_thred_stList[1],read_thred_stList[2],read_thred_stList[3],read_thred_stList[4],read_thred_stList[5],read_thred_stList[6],read_thred_stList[7],read_thred_stList[8],read_thred_stList[9],read_thred_stList[10],read_thred_stList[11],read_thred_stList[12],read_thred_stList[13],read_thred_stList[14],read_thred_stList[15],read_thred_stList[16],read_thred_stList[17],read_thred_stList[18],read_thred_stList[19],read_thred_stList[20],read_thred_stList[21],read_thred_stList[22],read_thred_stList[23],read_thred_stList[24],read_thred_stList[25],read_thred_stList[26],read_thred_stList[27],read_thred_stList[28],read_thred_stList[29],read_thred_stList[30],read_thred_stList[31]);
	commfunc_syslog_msgset( pv_own_name, errMsg );
	sprintf( errMsg, "func_btt_johen_stat threadNumb = %d join_stList       = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", threadNumb, join_stList[0],join_stList[1],join_stList[2],join_stList[3],join_stList[4],join_stList[5],join_stList[6],join_stList[7],join_stList[8],join_stList[9],join_stList[10],join_stList[11],join_stList[12],join_stList[13],join_stList[14],join_stList[15],join_stList[16],join_stList[17],join_stList[18],join_stList[19],join_stList[20],join_stList[21],join_stList[22],join_stList[23],join_stList[24],join_stList[25],join_stList[26],join_stList[27],join_stList[28],join_stList[29],join_stList[30],join_stList[31]);
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* PV_DEBUG_THREAD_ST */
	/* 通信ステータス状変を設定する */
	commfunc_johen_stat( PV_MKIND_BATTE, bttNumb, stNumb, join_stList[stNumb - 1],
		(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状変処理（全通信ステータス復帰）
 *
 * 機能：
 * 		引数 threadNo で指定された通信ステータス状変格納変数を更新する。
 * 		引数 bttNumb で指定された蓄電池用PCSについて以下の処理を行う。
 * 		読込、書込の双方復帰の場合、全ステータス（復帰）を設定する
 * 戻り値：
 * 		0：正常処理
 */
void func_btt_johen_resetstat( int threadNumb, int bttNumb )
{
	int loop;
	int errCnt = 0;

	/* 書込、読込でそれぞれの領域を初期化する */
	if( threadNumb == 0 )
	{
		memset(rdwt_thred_stList, 0, sizeof(rdwt_thred_stList));
	}
	else
	{
		memset(read_thred_stList, 0, sizeof(read_thred_stList));
	}

	/* 統合リストを更新 */
	for( loop = 0; loop < sizeof(join_stList); loop++ )
	{
		/* どちらかのスレッドで1がある場合、発生 */
		if( rdwt_thred_stList[loop] == 1 || read_thred_stList[loop] == 1 )
		{
			join_stList[loop] = 1;
			errCnt++;
		}
		/* どちらのスレッドも1がない場合、復帰 */
		else
		{
			join_stList[loop] = 0;
		}
	}

	/* 書込、読込で状変が発生していない場合 */
	if ( errCnt == 0 )
	{
		/* 通信ステータス状変：全ステータス（復帰） */
		commfunc_johen_resetstat( PV_MKIND_BATTE, bttNumb,
			(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
	}

}
/* [20241017] Add Ed */
