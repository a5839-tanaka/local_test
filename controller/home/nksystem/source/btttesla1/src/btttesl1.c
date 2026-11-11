/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260423] 2026.04.23
 * ・バッテリメータの計測情報を別スレッドで取得するように修正（一次調整力対応）
 * ・通信異常の全復帰を全ての要求が正常終了した際に実施するように変更(チャタリング防止)
 * [20231023] 2023.10.23
 * ・出力制御代表機試験中の変更(ランプレート設定のレジスタ誤り)
 * [20230922] 2023.09.22
 * ・対向試験でのIslandModeの認識違いの対応。(自立なし場合は、IslandMode=5にしかならない)
 * [20230828] 2023.08.28
 * ・出力制御対応(ランプ制御設定) 結合試験不具合
 * [20230823] 2023.08.23
 * ・出力制御対応(ランプ制御設定)
 * [20230814] 2023.08.14
 * ・出力制御対応に伴いレジスタ情報を追加
 * [20230721] 2023.07.21
 * ・無効電力モードの力率設定対応
 * [20230309] 2023.03.09
 * ・算出した力率が100%を超えた場合は、100%に補正する
 * [20200831] 2020.08.31
 * ・SoCの算出時に、フル充電エネルギーが0以下の場合、欠測にするように修正
 * [20200828] 2020.08.28
 * ・力率の算出式にて、有効電力が負の場合、力率が負になる不具合の対応
 * [20200824] 2020.08.24
 * ・有効電力,無効電力から算出した力率(平均)を最大充電電力,最大放電電力に適用する
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

#include "btttesl1.h"

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
#endif

#define	PV_OWN_NAME			"BTTTESLA1"				/* 自プログラム名 */

/* 通信回線で使用するプロトコル名 */
#define	PV_COMPROTOCOL		PV_COMPRO_TESLA1		/* Ｔｅｓｌａ通信プロトコル（MODBUS-TCP） */

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
static struct st_stdDevInfo		s_bttInfo[PV_MAX_BATTE];	/* バッテリー情報格納領域 */
static struct st_bttData		s_bttData[PV_MAX_BATTE];	/* 計測情報（瞬時値格納用） */
static struct st_threadArg		s_threadArg[PV_MAX_BATTE];	/* スレッド管理情報 */
static struct st_bttAvlData		s_bttAvlData[PV_MAX_BATTE];	/* 計測情報（移動平均演算用） */
static int						threadStop = 0;				/* スレッドストップフラグ */
static unsigned char			heartBeatIndex=0;			/* ハートビートパターンインデックス */
static struct st_bttFactTm		s_bttFactTm[PV_MAX_BATTE];	/* 力率計測情報（移動平均演算用） *//* [20200824] Add */
static struct st_paraReactivePowerFact s_bttPowerFact[PV_MAX_BATTE];	/* 力率設定値 *//* [20230721] Add */
static struct st_threadArg		s_bttMtThreadArg[PV_MAX_BATTE];	/* スレッド管理情報(周波数取得) *//* [20260423] Add */
static char						thread_stList[2][PV_MAX_STPOS];	/* 通信ステータス *//* [20260423] Add */
static char						join_stList[PV_MAX_STPOS];			/* 通信ステータス統合情報ST *//* [20260423] Add */

/* 内部関数定義 */
void	*func_bttOpen_thread( void *args );
int		func_bttData_recv( struct st_serialinf *, int, int, int, struct st_bttTeslaSdInf *, unsigned char *, unsigned char *, struct st_serialLog *, int);	/* [20260423] Chg */
int		func_bttData_get( int, int, int, int, struct st_bttData *, unsigned char *, int );	/* [20260423] Chg */
int		func_abnormal_exit( int );
void	func_set_soc( int, struct st_bttData *);
void 	func_set_fact( int , struct st_bttData *, struct st_bttFactTm *  );
int 	func_get_reactive_power_fact( char *, time_t *, struct st_paraReactivePowerFact *  );	/* [20230721] Add */
void	func_btt_johen_stat( int, int, int, int );				/* 通信ステータス状変処理（単独処理） *//* [20260423] Add */
void	func_btt_johen_resetstat( int, int );					/* 通信ステータス状変処理（全通信ステータス復帰） *//* [20260423] Add */

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
	pthread_t				bttMtThreadID[PV_MAX_BATTE];		/* スレッドＩＤ *//* [20260423] Add */
	int						requestCnt = 0;				/* 合計要求回数 */
	int						bttNumb;					/* 処理中のバッテリー番号 */
	int						minDataMax;					/* 移動平均データ最大数 */
	int						stocMax;					/* 計測蓄積ストック最大数 */
	int						timeOut = 0;				/* タイムアウト値 */
	struct timeval			nowTime;					/* 現在時刻 */
	struct timeval 			wrkTime;					/* 比較時刻 */
	int						dropFlg;					/* 欠測フラグ */
	struct st_bttData		bttData[PV_MAX_BATTE];		/* 計測情報 */
	long long				stepPow;
	struct st_bttTeslaSdInf	*ps_bttSdInf;				/* 送信情報ポインタ *//* スレッド起動確認用 */
	

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
	/* データ格納領域を初期化 */
	memset( &s_bttData, 0, sizeof(s_bttData));
	/* スレッド管理情報を初期化 */
	memset( s_threadArg, 0, sizeof(s_threadArg));
	memset( s_bttMtThreadArg, 0, sizeof(s_bttMtThreadArg));/* [20260423] Add */
	/* 通信ステータス統合情報を初期化 */
	memset( join_stList, 0, sizeof(join_stList));/* [20260423] Add */
	
	/* 力率設定値情報を初期化 */
	memset( s_bttPowerFact, 0, sizeof(s_bttPowerFact));
	

	/* 通信回線情報を取得 */
	if( (retCode = commfunc_serial_infoget( lineNo, &s_serial[0], ps_g_COMLIINF, PV_COMPROTOCOL )) != 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [COMLINLIF.csv] Information Get Error Line:%d", lineNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

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
	}

	/* 複数パワコン対応処理 */
	for( loopCnt1 = 1 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{
		/* シリアル回線情報をコピーする */
		memcpy( &s_serial[loopCnt1], &s_serial[0], sizeof(struct st_serialinf) );
	}

	/* 複数パワコン対応処理 */
	for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{
		/* IPアドレスの第4オクテットを装置IDを使用して設定 */
		sprintf( ip4Data, "%d", s_bttInfo[ loopCnt1 ].idNumb );
		strcat( s_serial[loopCnt1].targetName, ip4Data );

#ifdef PV_DEBUG
	printf("%s : PCS:%02d IP-ADDRESS:%s\n", pv_own_name, s_bttInfo[loopCnt1].devNumb, s_serial[loopCnt1].targetName );
#endif
		/* スレッド管理情報を設定 */
		s_threadArg[loopCnt1].indexNumb = loopCnt1;
		/* 周波数スレッド管理情報を設定 */
		s_bttMtThreadArg[loopCnt1].indexNumb = loopCnt1;/* [20260423] Add */

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
	retCode = commfunc_tmstsfil_get( PV_MKIND_BATTE, bttMaxCnt, PV_BTTTESLA1_MAXTM, s_bttInfo, s_bttData, s_bttAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );


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
		s_threadArg[loopCnt1].threadNumb = PV_BTTTESLA1_NORMAL_THREAD;/* [20260423] Add Ed */
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
/* [20260423] Add St */
		sprintf( errMsg, "normal threadID:%d", threadID[loopCnt1]);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* 処理タイプの設定 */
		if( s_bttInfo[loopCnt1].devType == PV_BTTTESLA1_TYPE1 )
		{
			/* 送信情報を処理タイプ１に設定 */
			ps_bttSdInf = s_type1SdInf;
		}
		else//if( s_bttInfo[workIdxNumb].devType == PV_BTTTESLA1_TYPE2 )
		{
			/* 送信情報を処理タイプ２に設定 */
			ps_bttSdInf = s_type2SdInf;				/* [20230814] Chg */
		}
		/* 要求回数分ループ */
		for( loopCnt2 = 0 ; ps_bttSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
		{
			/* メータスレッドの要求がある場合 */
			if( PV_BTTTESLA1_METER_THREAD == ps_bttSdInf[loopCnt2].threadNo )
			{
				/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
				s_bttMtThreadArg[loopCnt1].stFlg = 1;
				s_bttMtThreadArg[loopCnt1].threadNumb = PV_BTTTESLA1_METER_THREAD;
				if( pthread_create( &bttMtThreadID[loopCnt1], NULL, func_bttOpen_thread, &s_bttMtThreadArg[loopCnt1] ) != 0 )
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
				sprintf( errMsg, "meter threadID:%d", bttMtThreadID[loopCnt1]);
				commfunc_syslog_msgset( pv_own_name, errMsg );
				break;
			}
		}
		
/* [20260423] Add Ed */
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
/* [20260423] Add St */
	if( s_bttMtThreadArg[0].stFlg == 1 )
	{
		pthread_join( bttMtThreadID[0], NULL );
	}
/* [20260423] Add Ed */

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
	unsigned  char			sendBuff_w[21] = {
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
								0x00,0x3C				/* ハートビートタイムアウト(60sec固定) */
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
	struct st_ctlSvInf		ctlSvInf;					/* 制御管理情報 */
	int						retCheck;					/* 制御チェック結果 */
	int						islandCtl;					/* Island Control Mode */
	int						setLoopCnt;					/* 制御カウント */
	int						threadStop_local;			/* 停止フラグ(thread用) */
	int						onGridSvNumb;				/* 連系状態SV番号 *//* [20230529] Add */
	ps_threadArg = (struct st_threadArg *)args;
/* [20230721] Add St */
	time_t					fileTime=0;					/* ファイル更新時刻 */
	char					extdataPath[128];			/* 外部データパス */
	int						retFileCode;				/* 関数戻り値格納用 */
	int						reactiveMode;				/* 無効電力モード */
/* [20230721] Add Ed */
	int						rampRate;					/* ランプレート値 *//* [20230823] Add */
	int						chgTime;					/* 出力変化時間 *//* [20230823] Add */
	int						actPowTmNumb;				/* 有効電力計測番号 *//* [20230823] Add */
	int						threadNumb;					/* スレッド番号 *//* [20260423] Add */
	struct st_serialinf		serial[PV_MAX_BATTE];		/* シリアル回線情報 *//* [20260423] Add */
	int						tranID;						/* トランザクションID *//* [20241111] Add */

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));
	/* 制御管理情報を初期化 */
	memset(&ctlSvInf, 0, sizeof(struct st_ctlSvInf));


	/* 処理インデックス番号の取得 */
	workIdxNumb = ps_threadArg->indexNumb;

	/* パワコン番号の取得 */
	bttNumb = s_bttInfo[workIdxNumb].devNumb;
	/* バッテリー装置IDの取得 */
	idNumb  = s_bttInfo[workIdxNumb].idNumb ;

	/* スレッド番号の取得 */
	threadNumb = ps_threadArg->threadNumb;	/* [20260423] Add */

	/* シリアル回線情報のコピー */
	memcpy( serial, s_serial, sizeof(s_serial) );	/* [20260423] Add */

	/* 外部データファイルを設定 */
	sprintf(extdataPath, "%s%s_%d.txt", PV_PATH_EXT, PV_OWN_NAME, bttNumb );

	/* 設定ファイルのチェック*/
	retFileCode = func_get_reactive_power_fact( extdataPath, &fileTime, &s_bttPowerFact[workIdxNumb] );
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
	sleepTm.tv_sec  = serial[workIdxNumb].sndTime / 1000;
	sleepTm.tv_nsec = (serial[workIdxNumb].sndTime % 1000) * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( 1 )
	{
		/* 停止フラグを保持(ループの最初から停止フラグの情報を使用するようにここで保持する) */
		threadStop_local = threadStop;

		/* ポートがオープンしていない場合 */
		if( serial[workIdxNumb].status != PV_STAT_COM_CONNECT )
		{
			/* 要求種別を「通信復帰時」に変更 */
			reqKind = PV_BTTTESLA1_REQ_CB;
		}
		/* シリアルポートをオープン */
		openRetCode = commfunc_serial_open( &serial[workIdxNumb] );
		if( openRetCode >= 0 )
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（復帰） */
			func_btt_johen_stat( threadNumb, PV_ST14_LANERR, bttNumb, 0 );	/* [20260423] Chg */

			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 0;

		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			func_btt_johen_stat( threadNumb, PV_ST14_LANERR, bttNumb, 1 );	/* [20260423] Chg */

#ifdef PV_DEBUG
		printf("%s : IDX[%02d] PCS[%02d] Thread OPEN ERROR!!\n", pv_own_name, workIdxNumb, bttNumb );
#endif
			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 1;
			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 5 );
		}

		/* 要求実施回数を初期化 */
		serial[workIdxNumb].requestCount = 0;

		/* 処理タイプの設定 */
		if( s_bttInfo[workIdxNumb].devType == PV_BTTTESLA1_TYPE1 )
		{
			/* 送信情報を処理タイプ１に設定 */
			ps_bttSdInf = s_type1SdInf;
/* [20230529] Chg St */
			onGridSvNumb = PV_BTTTESLA1_SV_ISLANDOFF;		/* [20230922] Chg */
			/* 無効電力力率パラメータファイルがある場合 */
			if( s_bttPowerFact[workIdxNumb].statFlag == 1 )
			{
				/* 力率モードに設定 */
				reactiveMode = 3;
			}
			else
			{
				/* 自動に設定 */
				reactiveMode = 0;
			}
		}
		else//if( s_bttInfo[workIdxNumb].devType == PV_BTTTESLA1_TYPE2 )
		{
			/* 送信情報を処理タイプ２に設定 */
			ps_bttSdInf = s_type2SdInf;				/* [20230814] Chg */
			onGridSvNumb = PV_BTTTESLA1_SV_INTENTIONAL_READY;
			reactiveMode = 0;
/* [20230529] Chg Ed */
		}

		/* 状態取得：バッテリ操作権限 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_OPERATION, &svBttCtlNow, ps_g_SVSTSFIL);

		/* 初期化 */
		setLoopCnt = 0;
		/* 制御チェック */
		retCheck = commfunc_pvctl_ctlCheck( PV_MKIND_BATTE, bttNumb, &ctlSvInf );

		if( retCheck == 1 )
		{
			/* 制御対象SVチェック */
			if( ( ctlSvInf.svNo == PV_BTTTESLA1_SV_INTENTIONAL_READY ) ||
				( ctlSvInf.svNo == PV_BTTTESLA1_SV_INTENTIONAL_ISLAND ) ||
				( ctlSvInf.svNo == PV_BTTTESLA1_SV_PQ_ISLAND ) ||
				( ctlSvInf.svNo == PV_BTTTESLA1_SV_ISLANDOFF ) )
			{
				/* Island Control Mode */
				setLoopCnt = CMD_SET_ISLAND_MODE;		/* [20230814] Chg */
			}
			else if(( ctlSvInf.svNo == PV_BTTTESLA1_SVNUMB_REALPOWER_STOP ) ||
					( ctlSvInf.svNo == PV_BTTTESLA1_SVNUMB_REALPOWER_DIR ) )
			{
				/* Real Power Command Mode */
				setLoopCnt = CMD_SET_REAL_MODE;			/* [20230814] Chg */
			}
			else
			{
				/* 制御対象外の場合 */
				commfunc_pvctl_resSet( -1, &ctlSvInf );
				retCheck = 0;
			}
#ifdef PV_DEBUG_CTL
			printf(" Tesla : svBttCtlNow=%d, setLoopCnt=%d, ctlSvInf.svNo=%d, ctlSvInf.svValue=%d\n", svBttCtlNow, setLoopCnt, ctlSvInf.svNo, ctlSvInf.svValue );
#endif
		}

		/* 戻り値初期化 */
		retCode = 0;
		/* 複数回に分けて情報を収集する */
		for( loopCnt2 = 0 ; ps_bttSdInf[loopCnt2].funcCode != (unsigned char)0xFF ; loopCnt2++ )
		{
#ifdef PV_DEBUG
			printf("%s : LOOP:%02d BTTNO:%02d BTTID-No:%02d REQUEST:%d OPN-STS:%d, retCode=%d\n", PV_OWN_NAME, workIdxNumb+1, bttNumb, idNumb, loopCnt2+1, serial[workIdxNumb].status, retCode );
#endif
/* [20260423] Add St */
			/* スレッドNo.が一致しない場合 */
			if( threadNumb != ps_bttSdInf[loopCnt2].threadNo )
			{
				/* 処理をスキップする */
				continue;
			}
/* [20260423] Add Ed */

			/* 送信バッファの設定 */
			if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA1_FUNC_03 )
			{
				p_sendBuff = &sendBuff_r[0];
				
			}
			else
			{
				p_sendBuff = &sendBuff_w[0];
			}

			/* 通信状態が正常の場合、且つ、前回処理が正常の場合 */
			if( ( serial[workIdxNumb].status == PV_STAT_COM_CONNECT ) &&
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
				if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA1_FUNC_06 )
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
					p_sendBuff[12] = p_sendBuff[11] * 2;
				}
				dataLength = ps_bttSdInf[loopCnt2].dataLength;
				tranID = (p_sendBuff[0] * 0x100) + p_sendBuff[1];	/* [20241111] Add */
	
				/* ファンクションコードが書込みの場合 */
				if(( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA1_FUNC_10 ) ||
				   ( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA1_FUNC_06 ))
				{
/* [20230814] Chg St */
					/* 要求毎の設定 */
					switch( ps_bttSdInf[loopCnt2].addrType )
					{
/* [20230823] Add St */
						case CMD_SET_RAMP_RATE :		/* ランプレートの設定 */
							/* スケジュール情報が未実装の場合 */
							/* または、操作権限がない場合 */
							if( ( ps_g_SCHCTLFIL->statFlag == 0 ) ||
								( svBttCtlNow == 0 ) )
							{
								/* スキップ */
								continue;
							}
							else
							{
								/* ランプレート無効 */
								rampRate = 0;
								/* 出力制御値を取得 */
								commfunc_pvctl_bttPowGet( &directPower, &chgTime, ps_g_TMSTSFIL);

								/* IslandMode が 連係時 */
								if( svGridStatNow == 1 )
								{
									/* 有効電力計測番号を保持 */
									actPowTmNumb = s_typeCalcInf[s_bttInfo[workIdxNumb].devType -1].actPowNumb;

									/* 出力抑制(100%以外)の場合 */
									if( chgTime > 0 )
									{
										/* 充電方向で有効電力が欠測の場合は、設定しない(前回値を使用) */
										if( ( directPower < 0 ) &&
											( s_bttData[workIdxNumb].tmFlgData[actPowTmNumb-1] == PV_TMFLG_DROPTM ))	/* [20230828] Chg */
										{
											continue;
										}

										/* 指令値が放電方向、または、(指令値が充電方向、且つ、有効電力が充電の場合) */
										if( ( directPower >= 0 ) ||
											(( directPower < 0 ) &&
											 ( s_bttData[workIdxNumb].tmValData[actPowTmNumb-1] > 0 )))					/* [20230828] Chg */
										{
											/* 出力変化時間に合わせたランプレートを設定する */
											rampRate = ((ps_g_BATTEINF+bttNumb-1)->maxDisChgPwr / (chgTime * 60 ));
										}
									}
								}
								p_sendBuff[13] = ((rampRate >> 24 ) & 0xFF);
								p_sendBuff[14] = ((rampRate >> 16 ) & 0xFF);
								p_sendBuff[15] = ((rampRate >> 8 ) & 0xFF);
								p_sendBuff[16] = (rampRate & 0xFF);
/* [20231023] Add St */
								p_sendBuff[17] = ((rampRate >> 24 ) & 0xFF);
								p_sendBuff[18] = ((rampRate >> 16 ) & 0xFF);
								p_sendBuff[19] = ((rampRate >> 8 ) & 0xFF);
								p_sendBuff[20] = (rampRate & 0xFF);
/* [20231023] Add Ed */
							}
							break;
/* [20230823] Add Ed */

						case CMD_SET_POWER :		/* 充放電の設定 */
							/* 操作権限があり、且つ、IslandMode が 連係時 */
							if( ( svBttCtlNow == 1 ) && ( svGridStatNow == 1 ) )
							{
								/* スレッド停止フラグがONの場合は、出力停止する */
								if( threadStop_local != 0 )
								{
									directPower = 0;
								}
								else
								{
									/* 出力制御値を取得 */
									commfunc_pvctl_bttPowGet( &directPower, &chgTime, ps_g_TMSTSFIL);
								}

								/* ハートビートタイムアウトを設定する */
								p_sendBuff[19] = ((PV_BTTTESLA1_HEARTBEAT_TO >> 8 ) & 0xFF);
								p_sendBuff[20] = (PV_BTTTESLA1_HEARTBEAT_TO & 0xFF);

								p_sendBuff[13] = ((directPower >> 24 ) & 0xFF);
								p_sendBuff[14] = ((directPower >> 16 ) & 0xFF);
								p_sendBuff[15] = ((directPower >> 8 ) & 0xFF);
								p_sendBuff[16] = (directPower & 0xFF);

								/* ハートビートパターンを設定 */
								p_sendBuff[17] = heartBeatPtn[heartBeatIndex];
								p_sendBuff[18] = heartBeatPtn[(heartBeatIndex+1)%2];
							}
							else
							{
								continue;
							}
							break;
/* [20230721] Add St */
						case CMD_SET_POWER_FACT :		/* 力率設定 */
							/* 外部ファイルのチェック */
							/* 操作権限が遠方時 */
							if( ( s_bttPowerFact[workIdxNumb].statFlag == 1 ) &&
								( svGridStatNow == 1 ) &&
								( svBttCtlNow == 1 ) &&
								( reactiveMode == 3 ) )
							{
								/* 設定ファイルのチェック*/
								retFileCode = func_get_reactive_power_fact( extdataPath, &fileTime, &s_bttPowerFact[workIdxNumb] );

								/* 通信異常復帰時、または、ファイル更新時 */
								if( ((reqKind & PV_BTTTESLA1_REQ_CB)  == PV_BTTTESLA1_REQ_CB ) ||
									( retFileCode == COM_PARAM_GET_OK ) )
								{
									/* 無効電力モード 力率設定 */
									p_sendBuff[13] = ((s_bttPowerFact[workIdxNumb].chargeSetpoint >> 24 ) & 0xFF);
									p_sendBuff[14] = ((s_bttPowerFact[workIdxNumb].chargeSetpoint >> 16 ) & 0xFF);
									p_sendBuff[15] = ((s_bttPowerFact[workIdxNumb].chargeSetpoint >> 8 ) & 0xFF);
									p_sendBuff[16] = (s_bttPowerFact[workIdxNumb].chargeSetpoint & 0xFF);

									p_sendBuff[17] = ((s_bttPowerFact[workIdxNumb].chargeLead >> 8 ) & 0xFF);
									p_sendBuff[18] = (s_bttPowerFact[workIdxNumb].chargeLead & 0xFF);

									p_sendBuff[19] = ((s_bttPowerFact[workIdxNumb].dischargeSetpoint >> 24 ) & 0xFF);
									p_sendBuff[20] = ((s_bttPowerFact[workIdxNumb].dischargeSetpoint >> 16 ) & 0xFF);
									p_sendBuff[21] = ((s_bttPowerFact[workIdxNumb].dischargeSetpoint >> 8 ) & 0xFF);
									p_sendBuff[22] = (s_bttPowerFact[workIdxNumb].dischargeSetpoint & 0xFF);

									p_sendBuff[23] = ((s_bttPowerFact[workIdxNumb].dischargeLead >> 8 ) & 0xFF);
									p_sendBuff[24] = (s_bttPowerFact[workIdxNumb].dischargeLead & 0xFF);
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
						case CMD_SET_REACTIVE_MODE :		/* Reactive Power Command Mode */
							/* 操作権限があり、且つ、IslandMode が 連係切り替わり時、且つ、通信異常復帰時 */
							if( ( svBttCtlNow == 1 ) &&
								( svGridStatNow == 1 ) &&
								( ( reqKind & PV_BTTTESLA1_REQ_CB)  == PV_BTTTESLA1_REQ_CB )  )
							{
								p_sendBuff[13] = 0x00; 					/* ┐                */
								p_sendBuff[14] = reactiveMode & 0xFF; 	/* ┘ 無効電力モード */
							}
							else
							{
								continue;
								
							}
							break;
/* [20230721] Add Ed */
						case CMD_SET_REAL_MODE :			/* Real Power Command Mode, Always Active , PeakPower Mode */
							/* 操作権限がある場合 */
							if( svBttCtlNow == 1 )
							{
								/* 制御対象の場合 */
								if( CMD_SET_REAL_MODE == setLoopCnt )
								{
									if( ctlSvInf.svNo == PV_BTTTESLA1_SVNUMB_REALPOWER_DIR )
									{
										p_sendBuff[13] = 0x00; /* ┐        */
										p_sendBuff[14] = 0x01; /* ┘ 0x0001 */
										
									}
									else if( ctlSvInf.svNo == PV_BTTTESLA1_SVNUMB_REALPOWER_STOP )
									{
										p_sendBuff[13] = 0x00; /* ┐        */
										p_sendBuff[14] = 0x00; /* ┘ 0x0000 */
									}
									p_sendBuff[15] = 0x00; /* ┐        */
									p_sendBuff[16] = 0x01; /* ┘ 0x0001 */
									p_sendBuff[17] = 0x00; /* ┐        */
									p_sendBuff[18] = 0x02; /* ┘ 0x0002 */
								}
								/* IslandMode が 連係時、且つ、（通信復帰時、または、IslandModeが連系以外からの切り替わり時 */
								else if( ( svGridStatNow == 1 ) &&
									( ( reqKind & PV_BTTTESLA1_REQ_CB)  == PV_BTTTESLA1_REQ_CB )  )
								{
									p_sendBuff[13] = 0x00; /* ┐        */
									p_sendBuff[14] = 0x01; /* ┘ 0x0001 */
									p_sendBuff[15] = 0x00; /* ┐        */
									p_sendBuff[16] = 0x01; /* ┘ 0x0001 */
									p_sendBuff[17] = 0x00; /* ┐        */
									p_sendBuff[18] = 0x02; /* ┘ 0x0002 */
								}
								else
								{
									continue;
								}
							}
							else
							{
								/* 制御対象の場合 */
								if( CMD_SET_REAL_MODE == setLoopCnt )
								{
									/* 制御失敗を設定 */
									commfunc_pvctl_resSet( -1, &ctlSvInf );
								}
								continue;
							}
							break;
						case CMD_SET_ISLAND_MODE :		/* Island Control Mode */
							/* 操作権限があり、且つ、制御指示の場合 */
							if( svBttCtlNow == 1 )
							{
								if( CMD_SET_ISLAND_MODE == setLoopCnt )
								{
									switch( ctlSvInf.svNo )
									{
										case PV_BTTTESLA1_SV_INTENTIONAL_READY :
											islandCtl = 2;
											break;
										case PV_BTTTESLA1_SV_INTENTIONAL_ISLAND :
											islandCtl = 3;
											break;
										case PV_BTTTESLA1_SV_PQ_ISLAND :
											islandCtl = 4;
											break;
										case PV_BTTTESLA1_SV_ISLANDOFF :
											islandCtl = 5;
											break;
										default :
											/* 上記以外は、Real Power Control Commandの場合 */
											continue;
											break;
									}
#ifdef PV_DEBUG_CTL
									printf(" Tesla : set island=%d\n", islandCtl );
#endif
									p_sendBuff[13] = 0x00;
									p_sendBuff[14] = islandCtl;
								}
								else
								{
									continue;
								}
							}
							else
							{
								/* 制御対象の場合 */
								if( CMD_SET_ISLAND_MODE == setLoopCnt )
								{
									/* 制御失敗を設定 */
									commfunc_pvctl_resSet( -1, &ctlSvInf );
								}
								continue;
							}
							break;
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

				/* 送信待ち処理 */
				nanosleep( &sleepTm, NULL);		/* [20260423] Chg */

				/* 処理結果の初期値を異常に設定 */
				retCode = -1;

				/* 要求実施回数の上限まで繰り返す */
				/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
				while( serial[workIdxNumb].requestCount < PV_SERIAL_REQUEST_MAX )
				{
					/* 要求のリトライは行わない */
					/* シリアル通信ログ領域を初期化 */
					s_serialLog.dataCnt = 0;

					/* バッテリーに要求を送信 */
					retCode = commfunc_serial_send( &serial[workIdxNumb], p_sendBuff, dataLength, &s_serialLog );
					if( retCode < 0 )
					{
						sprintf( errMsg, "TCP Port Write Error! RET:%d num/id:%02d/%02d (%s)",
							retCode, bttNumb, idNumb, serial[workIdxNumb].targetName );
						commfunc_syslog_msgset( pv_own_name, errMsg );

						/* データ送信異常（送信不可異常）発生 */
						func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20260423] Chg */

						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* データ送信異常（送信不可異常）復帰 */
						/* 状変（通信ステータス状変）復帰 */
						func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 0 );	/* [20260423] Chg */
					}

					/* データ処理バッファをクリア */
					memset(readBuff, 0, sizeof(readBuff));
	
					/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
					retCode = func_bttData_recv( &serial[workIdxNumb], tranID, bttNumb, idNumb,
												 &ps_bttSdInf[loopCnt2],
												 p_sendBuff, readBuff, &s_serialLog, threadNumb );	/* [20260423] Chg */
					if( retCode == 0 )
					{
						/* 正常データ受信完了 */
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_AUTO, &serial[workIdxNumb], &s_serialLog );
/* [20260423] Chg */
						/* 再要求不要のため処理を抜ける */
						break;
					}
					else
					{
						/* シリアル通信ログを出力 */
						commfunc_serial_logout( PV_SERIAL_LOG_ON, &serial[workIdxNumb], &s_serialLog );
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
				printf("%s : SERIAL POERT NOT-OPENED STATUS[%d] stFlg[%d]\n", PV_OWN_NAME, serial[workIdxNumb].status, s_threadArg[workIdxNumb].stFlg );
#endif
				/* シリアルポートが異常の場合 */
				retCode = -1;
			}

			/* 読み込み処理のみ計測情報を格納する */
			if( ps_bttSdInf[loopCnt2].funcCode == PV_BTTTESLA1_FUNC_03 )
			{
				/* データ処理バッファから装置状態と計測値を抽出 */
				func_bttData_get( s_bttInfo[workIdxNumb].devType, loopCnt2 + 1, bttNumb, retCode, &s_bttData[workIdxNumb], readBuff, threadNumb);		/* [20260423] Chg */
			}

/* [20230814] Chg St */
			/* Island Control Modeの取得 */
			if( ( ps_bttSdInf[loopCnt2].addrType == CMD_GET_ISLAND ) && ( retCode == 0 ) )
			{
				/* SV連系状態取得 */
				commfunc_sv_stat_get( PV_MKIND_BATTE, bttNumb, onGridSvNumb, &svGridStatNow, ps_g_SVSTSFIL);	/* [20230529] Chg */

				/* 現在状態がONで、前回状態がOFFの場合 */
				if(( svGridStatNow == 1 ) &&
				   ( svGridStatPre == 0 ))
				{
					/* 要求種別を「通信復帰時/連系切り替わり時」に変更 */
					reqKind = PV_BTTTESLA1_REQ_CB;
				}
				svGridStatPre = svGridStatNow;
			}
			/* ハートビートパターンの取得 */
			else if( ( ps_bttSdInf[loopCnt2].addrType == CMD_GET_HEART_BEAT ) && ( retCode == 0 ) )
			{
				/* ハートビートパターンが同じ場合 */
				if( ( readBuff[9] == heartBeatPtn[heartBeatIndex] ) &&
					( readBuff[10] == heartBeatPtn[(heartBeatIndex+1)%2] ))
				{
					/* ハートビートパターンインデックスを更新 */
					heartBeatIndex = ( heartBeatIndex + 1) % 2;
				}
			}
			/* 充放電の設定が正常の場合 */
			else if( ( ps_bttSdInf[loopCnt2].addrType == CMD_SET_POWER ) && ( retCode == 0 ))
			{
				/* 制御情報を更新する */
				/* ハートビートパターンインデックスを更新 */
				heartBeatIndex = ( heartBeatIndex + 1) % 2;
			}
			/* 制御カウントと同じ場合 */
			else if( ps_bttSdInf[loopCnt2].addrType == setLoopCnt )
			{
#ifdef PV_DEBUG_CTL
				printf(" Tesla : response retCode=%d\n", retCode );
#endif
				/* 制御結果を設定 */
				commfunc_pvctl_resSet( retCode, &ctlSvInf );
			}
/* [20230814] Chg Ed */
		}

		/* ループ処理の終端、且つ、正常終了の場合 */
		if( ps_bttSdInf[loopCnt2].funcCode == (unsigned char)0xFF )
		{
/* [20200824] Add St */
			/* 力率情報を更新 */
			func_set_fact( workIdxNumb, &s_bttData[workIdxNumb], &s_bttFactTm[workIdxNumb] );
/* [20200824] Add Ed */

			if( retCode == 0 )
			{
				/* 通信ステータス状変：全ステータス（復帰） */
				func_btt_johen_resetstat( threadNumb, bttNumb );	/* [20260423] Chg */

				/* SoC情報を更新 */
				func_set_soc(workIdxNumb, s_bttData );

				/* 通信復帰時の場合 */
				if( reqKind == PV_BTTTESLA1_REQ_CB )
				{
					/* 通常時に遷移 */
					reqKind = PV_BTTTESLA1_REQ_NOR;
				}

				/* 現在状態がONで、前回状態がOFFの場合 */
				if(( svBttCtlNow == 1 ) &&
				   ( svBttCtlPre == 0 ))
				{
					/* 要求種別を「通信復帰時/連系切り替わり時」に変更 */
					reqKind = PV_BTTTESLA1_REQ_CB;
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
		retCode = commfunc_devTmData_set( PV_MKIND_BATTE, bttNumb, PV_BTTTESLA1_MAXTM, &s_bttData[workIdxNumb], &s_bttAvlData[workIdxNumb], ps_g_TMDATINF );

		/* 計測情報を計測値格納ファイルに保存 */
		retCode = commfunc_tmstsfil_devset( PV_MKIND_BATTE, bttNumb, PV_BTTTESLA1_MAXTM, &s_bttData[workIdxNumb], &s_bttAvlData[workIdxNumb], ps_g_TMDATINF, ps_g_TMSTSFIL );

		if( threadStop_local != 0 )
		{
			break;
		}
	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &serial[workIdxNumb] );

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
	int threadNumb )		/* [20260423] Add */
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
		case PV_BTTTESLA1_FUNC_10:
		case PV_BTTTESLA1_FUNC_06:
			checkLength = 12;
				break;
		case PV_BTTTESLA1_FUNC_03:
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
					func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20260423] Chg */
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
						func_btt_johen_stat( threadNumb, PV_ST01_NONRESPONSE, bttNumb, 1 );	/* [20260423] Chg */
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", bttNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					func_btt_johen_stat( threadNumb, PV_ST17_NOTSENDERR, bttNumb, 1 );	/* [20260423] Chg */
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
				else if ( (reqFunc == PV_BTTTESLA1_FUNC_03) && ( ( *(dataBuff+5) != (dataByte + 3) ) || (  *(dataBuff+8) != dataByte ) ) )
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
				func_btt_johen_stat( threadNumb, PV_ST19_DWCOMMANDERR, bttNumb, 1 );	/* [20260423] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST22_DWPARAMERR, bttNumb, 1 );	/* [20260423] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST23_DWDATAERR, bttNumb, 1 );	/* [20260423] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが サーバ異常値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x04 )
			{
				/* 通信ステータス状変：デバイス異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST16_DEVICEERROR, bttNumb, 1 );	/* [20260423] Chg */
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが ＢＵＳＹの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x05 )
			{
				/* 通常時、且つ、制御書込みの場合、ハートビートパターンを切り替える */
				if( (ps_bttSdInf->reqKind == PV_BTTTESLA1_REQ_AW ) &&
				    (reqFunc == PV_BTTTESLA1_FUNC_10 ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					func_btt_johen_stat( threadNumb, PV_ST27_DWCONTROLERR, bttNumb, 1 );	/* [20260423] Chg */
					/* ハートビートパターンインデックスを更新 */
					heartBeatIndex = ( heartBeatIndex + 1) % 2;
				}
/* [20260423] Add St */
				else
				{
					/* 通信ステータス状変：デバイスＢＵＳＹ（発生） */
					func_btt_johen_stat( threadNumb, PV_ST15_DEVICEBUSY, bttNumb, 1 );
				}
/* [20260423] Add Ed */

				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", bttNumb, *(dataBuff+8));
				commfunc_syslog_msgset( pv_own_name, errMsg );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 応答待ちの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x06 )
			{
				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", bttNumb, *(dataBuff+8));
				commfunc_syslog_msgset( pv_own_name, errMsg );
				/* 要求実施回数を＋１ */
				ps_serial->requestCount += 1;
				/* 要求実施回数が上限に達している場合 */
				if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
				{
					/* 通信ステータス状変：デバイスＢＵＳＹ（発生） */
					func_btt_johen_stat( threadNumb, PV_ST15_DEVICEBUSY, bttNumb, 1 );	/* [20260423] Chg */
				}
				/* タイムアウト */
				return( PV_SERIAL_ERRTIMEOUT );
			}
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				func_btt_johen_stat( threadNumb, PV_ST20_DWFORMATERR, bttNumb, 1 );	/* [20260423] Chg */
				return( PV_SERIAL_ERROTHER );
			}
		}
		else
		{
			/* ファンクションコードが書き込み(１０，６）の場合 */
			if(( reqFunc == PV_BTTTESLA1_FUNC_10 ) ||
			   ( reqFunc == PV_BTTTESLA1_FUNC_10 ))
			{
				/* 開始アドレスとレジスタ数が異なる場合 */
				/* （開始アドレスと設定値が異なる場合） */
				if( (*(dataBuff+8)  != sendBuff[8]  ) ||
				    (*(dataBuff+9)  != sendBuff[9]  ) ||
				    (*(dataBuff+10) != sendBuff[10] ) ||
				    (*(dataBuff+11) != sendBuff[11] ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					func_btt_johen_stat( threadNumb, PV_ST27_DWCONTROLERR, bttNumb, 1 );	/* [20260423] Chg */
					sprintf( errMsg, "Data-Recv data Error! Numb:%02d Expect[%02X][%02X][%02X][%02X],Result[%02X][%02X][%02X][%02X]",
						bttNumb, sendBuff[8], sendBuff[9], sendBuff[10], sendBuff[11], *(dataBuff+8), *(dataBuff+9), *(dataBuff+10), *(dataBuff+11) );
					commfunc_syslog_msgset( pv_own_name, errMsg );
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
			func_btt_johen_stat( threadNumb, PV_ST01_NONRESPONSE, bttNumb, 1 );	/* [20260423] Chg */
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
int	func_bttData_get( int bttType, int reqCnt, int bttNumb, int resultCode, struct st_bttData *ps_bttData, unsigned char *dataBuff, int threadNumb)
{
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char		dataByte[8];			/* データ格納用領域 */
		short int			data16bit;				/* 16bitデータ取得用 */
		unsigned short int	data16bitu;				/* 16bitデータ取得用（符号無し） */
		int					data32bit;				/* 32bitデータ取得用 */
		float				data32f;				/* 32bitデータ取得用 */
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
	int				tmVal;							/* 計測値 */

	/* 格納位置を取得 */
	tmIfLinkBase = ps_g_BATTEINF[bttNumb - 1].tmIfLink;
	svStLinkBase = ps_g_BATTEINF[bttNumb - 1].svStLink;
	svIfLinkBase = ps_g_BATTEINF[bttNumb - 1].svIfLink;

#ifdef PV_DEBUG_DT1
	printf("%s : PCS[ %02d ] TYPE[%d] REQ[%d] COD[%d] LINK[TMST:%6d, SVST:%6d, SVIF:%6d]\n",
		PV_OWN_NAME, bttNumb, bttType, reqCnt, resultCode, tmIfLinkBase, svStLinkBase, svIfLinkBase );
#endif

	/* 処理タイプ１の場合 */
	if( bttType == PV_BTTTESLA1_TYPE1 )
	{
		ps_bttSvInf = s_type1SvInf;
		ps_bttTmInf = s_type1TmInf;
		ps_bttTmRange  = s_type1TmRange;
	}
	else//if( bttType == PV_BTTTESLA1_TYPE2 )
	{
		ps_bttSvInf = s_type2SvInf;				/* [20230814] Chg */
		ps_bttTmInf = s_type2TmInf;				/* [20230814] Chg */
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
/* [20260423] Chg St */
		/* 通常スレッドでのみSV情報を更新 */
		/* ※複数のスレッドでSV情報を更新すると、状態情報の領域を初期化し、SVをセットする前にディスパッチが走ると意図しないSVで更新してしまう為、 */
		/*   SV情報は1つのスレッドでのみ更新するように設計する */
		if( threadNumb == PV_BTTTESLA1_NORMAL_THREAD )
		{
			/* 装置状態情報の更新（ビットステータス形式で更新） */
			commfunc_johen_sv( PV_MKIND_BATTE, bttNumb, PV_MAX_BATTE_SVPOS, ps_bttData->bttStatus,
				svIfLinkBase, svStLinkBase, 
				ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
/* [20260423] Chg Ed */
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

			/* 計測値のサイズが２バイトサイズの場合 */
			if( ps_bttTmInf[loopCnt1].type == 2 )
			{
				/* 16bitサイズのデータとして計測値を取得 */
				tmVal = (int)(u_change.data32f);
			}
			/* 計測値のサイズが２バイトサイズの場合 */
			else if( ps_bttTmInf[loopCnt1].dataSize == 2 )
			{
				/* 符号付き判定 */
				if( ps_bttTmInf[loopCnt1].type == 0 )
				{
					/* 符号なしbitサイズのデータとして計測値を取得 */
					tmVal = u_change.data16bitu;
				}
				else
				{
					/* 符号付き16bitサイズのデータとして計測値を取得 */
					tmVal = u_change.data16bit;
				}
			}
			/* 計測値のサイズが４バイトサイズの場合 */
			else if( ps_bttTmInf[loopCnt1].dataSize == 4 )
			{
				/* 32bitサイズのデータとして計測値を取得 */
				tmVal = u_change.data32bit;
			}
			/* 計測値のサイズが８バイトサイズの場合 */
			else if( ps_bttTmInf[loopCnt1].dataSize == 8 )
			{
				/* 符号付32bitデータの正の上限に変換 */
				tmVal = (int)(u_change.data64bitu & 0x7FFFFFFF);
			}
			else
			{
				/* 暫定的に16bitサイズのデータとして計測値を取得 */
				tmVal = u_change.data16bit;
			}
			
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

	/* サイトメータの計測値が全て0の場合、 */
	/* 放電電力量/充電電力量を欠測とする */
	tmVal = 0;
	/* ストッパーまで繰り返す */
	for( loopCnt1 = 0 ; s_stsMeterTbl_Type1_Site[loopCnt1].tmNumb != -1 ; loopCnt1++ )
	{
		tmNumb = s_stsMeterTbl_Type1_Site[loopCnt1].tmNumb -1;
		/* 計測値が0以外の場合は、保持 */
		if( ps_bttData->tmValData[tmNumb] != 0 )
		{
			tmVal = ps_bttData->tmValData[tmNumb];
		}
	}
	/* 全ての計測値が0の場合 */
	if( tmVal == 0 )
	{
		for( loopCnt1 = 0 ; s_stsMeterTbl_Type1_Site[loopCnt1].tmNumb != -1 ; loopCnt1++ )
		{
			tmNumb = s_stsMeterTbl_Type1_Site[loopCnt1].tmNumb -1;
			/* 蓄積値の場合は、欠測に修正 */
			if( s_stsMeterTbl_Type1_Site[loopCnt1].tntegratedFlg != 0 )
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
	int		tmVal = 0;
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
//	if( s_bttInfo[bttIndex].devType == PV_BTTTESLA1_TYPE1 )	/* [20230529] Del */
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

/* [20200824] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 力率を算出
 *
 * 機能：
 * 		有効電力、無効電力から力率を算出する
 */
void func_set_fact( int bttIndex, struct st_bttData *ps_bttData, struct st_bttFactTm *ps_bttFactTm  )
{
	int				tmFlg = PV_TMFLG_DROPTM;
	int				tmVal = 0;
	int				soNumb;
	int				powerNumb;
	int				varNumb;
	int				tmNumb;
	int				avaChgFactNumb;
	int				avaDisFactNumb;
	int				expData;
	int				index;
	long long 		powerVal;
	long long 		varVal;
	float 			tempf;
	int				factNum;
	long long 		factSum;
	int				type;
	int				loopCnt1;
	int				factLimit;		/* [20230309] Add */

	/* Indexを保持 */
	index = ps_bttFactTm->index;

	/* 装置番号を取得 */
	soNumb = s_bttInfo[bttIndex].devNumb;

	/* 力率算出に必要な計測番号を取得 */
	type = s_bttInfo[bttIndex].devType -1;
	tmNumb = s_typeFactInf[type].factNumb;
	powerNumb = s_typeFactInf[type].powerNumb;
	varNumb = s_typeFactInf[type].varNumb;
	avaChgFactNumb = s_typeFactInf[type].avaChgFactNumb;
	avaDisFactNumb = s_typeFactInf[type].avaDisFactNumb;

	/* 乗率を取得 */
	expData = ps_g_TMDATINF[ ps_g_BATTEINF[soNumb -1 ].tmIfLink + tmNumb -1 ].expData;

	/* 型式毎に力率を算出する */
//	if( s_bttInfo[bttIndex].devType == PV_BTTTESLA1_TYPE1 )		/* [20230529] Del */
	{
		/* 有効電力、無効電力が正常な場合 */
		if( ( ps_bttData->tmFlgData[powerNumb-1] == PV_TMFLG_NORMAL ) &&
		    ( ps_bttData->tmFlgData[varNumb-1] == PV_TMFLG_NORMAL ) )
		{

			/* 有効電力のべき乗(0.1kW単位で計算する) */
			powerVal = ( ps_bttData->tmValData[powerNumb-1] / 100 ) * ( ps_bttData->tmValData[powerNumb-1] / 100 );

			/* 無効電力のべき乗(0.1kW単位で計算する) */
			varVal = ( ps_bttData->tmValData[varNumb-1] / 100 ) * ( ps_bttData->tmValData[varNumb-1] / 100 );

#ifdef PV_DEBUG_FACT
			sprintf( errMsg, "powerVal=%ld, varVal=%ld", powerVal, varVal);
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

			/* 有効電力と無効電力の和が０の場合 */
			if(( powerVal + varVal ) > 0 )
			{
				tmFlg = PV_TMFLG_NORMAL;

				/* 有効電力と無効電力を加算する */
				tempf = powerVal + varVal;
#ifdef PV_DEBUG_FACT
				sprintf( errMsg, "tempf=%f", tempf);
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

/* [20200828] Chg Ed */
				/* 有効電力の絶対値を取得 */
				if( ps_bttData->tmValData[powerNumb-1] < 0 )
				{
					tmVal = -1 * ps_bttData->tmValData[powerNumb-1];
				}
				else
				{
					tmVal = ps_bttData->tmValData[powerNumb-1];
				}
				/* 力率cosθを取得 */
				tmVal = (int)((float)tmVal * commfunc_pow10abs(expData) / sqrtf(tempf));
/* [20200828] Chg Ed */
#ifdef PV_DEBUG_FACT
				sprintf( errMsg, "fact=%d", tmVal);
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
			}

			//sprintf( errMsg, "[%d].tmLower=%d, tmUpper=%d", tmNumb, ( s_type1TmRange[tmNumb-1].tmLower * commfunc_pow10abs(expData) ), ( s_type1TmRange[tmNumb-1].tmUpper * commfunc_pow10abs(expData) ));
			//commfunc_syslog_msgset( pv_own_name, errMsg );
			/* 計測値が既定値の範囲内か？ */
			if( ( ( s_type1TmRange[tmNumb-1].tmLower * commfunc_pow10abs(expData) ) > tmVal ) ||
				( ( s_type1TmRange[tmNumb-1].tmUpper * commfunc_pow10abs(expData) ) < tmVal ) )
			{
				/* 計測異常２をセット */
				tmFlg = PV_TMFLG_TMERR2;
			}
		}

		/* Indexの周期情報に計測ステータス、計測値を更新する */
		ps_bttFactTm->s_tmData[index].tmFlg = tmFlg;
		ps_bttFactTm->s_tmData[index].tmVal = tmVal;
#ifdef PV_DEBUG_FACT
		sprintf( errMsg, "s_tmData[%d] tmFlg=%d, tmVal=%d", index, tmFlg, tmVal);
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif


		/* 1分間の力率情報の平均を算出 */
		factNum = 0;
		factSum = 0;
		for( loopCnt1 = 0; loopCnt1 < ps_bttFactTm->minDataNum; loopCnt1++ )
		{
			/* 正常な値をカウントし加算 */
			if( ps_bttFactTm->s_tmData[loopCnt1].tmFlg == PV_TMFLG_NORMAL)
			{
				factSum += ps_bttFactTm->s_tmData[loopCnt1].tmVal;
				factNum++;
			}
		}

		/* 1つでも正常なデータがある場合 */
		if( ( factNum > 0 ) && ( factSum > 0 ) )
		{
			/* 平均値を算出 */
			tmFlg = PV_TMFLG_NORMAL;
			tmVal = (int)(factSum / factNum);

/* [20230309] Add St */
			factLimit = 100 * commfunc_pow10abs(expData);

			/* 範囲チェック */
			/* 力率が100%を超える場合は、100%に補正する */
			if( tmVal > factLimit )
			{
				tmVal = factLimit;
			}
			/* 0％を下回ることはない */
/* [20230309] Add Ed */
		}
		else
		{
			tmFlg = PV_TMFLG_DROPTM;
			tmVal = 0;
		}
#ifdef PV_DEBUG_FACT
		sprintf( errMsg, "Ave Num(%d), factSum=%d, tmVal=%d", factNum, factSum, tmVal);
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
		/* 力率の計測値を設定 */
		ps_bttData->tmFlgData[tmNumb-1] = tmFlg;
		ps_bttData->tmValData[tmNumb-1] = tmVal;

		/* 充電可能電力、放電可能電力をコピー */
		ps_bttData->tmFlgData[avaChgFactNumb-1] = ps_bttData->tmFlgData[s_typeFactInf[type].avaChgNumb-1];
		ps_bttData->tmValData[avaChgFactNumb-1] = ps_bttData->tmValData[s_typeFactInf[type].avaChgNumb-1];
		ps_bttData->tmFlgData[avaDisFactNumb-1] = ps_bttData->tmFlgData[s_typeFactInf[type].avaDisNumb-1];
		ps_bttData->tmValData[avaDisFactNumb-1] = ps_bttData->tmValData[s_typeFactInf[type].avaDisNumb-1];

		/* 力率が正常の場合 */
		if( tmFlg == PV_TMFLG_NORMAL )
		{
			ps_bttData->tmValData[avaChgFactNumb-1] = (int)((long long)ps_bttData->tmValData[avaChgFactNumb-1] * tmVal / commfunc_pow10abs(expData-2));
			ps_bttData->tmValData[avaDisFactNumb-1] = (int)((long long)ps_bttData->tmValData[avaDisFactNumb-1] * tmVal / commfunc_pow10abs(expData-2));
		}
	}
	return;
}
/* [20200824] Add Ed */


/* [20230721] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 無効電力モード 力率設定値取得
 *
 * 機能：
 * 		無効電力モードの力率パラメータを外部ファイルから取得する
 */
int func_get_reactive_power_fact( char *pc_filePath, time_t *pt_time, struct st_paraReactivePowerFact *ps_bttPowerFact  )
{
	int				retCode;					/* 戻り値 */
	char			getStr[128];				/* 取得文字列 */

	/* 設定ファイルのチェック */
	retCode = commfunc_file_info_get( pc_filePath, pt_time, PV_BTTTESLA1_PARAM1, getStr );
	if( retCode == COM_PARAM_GET_OK )
	{
		/* 実装済みに変更 */
		ps_bttPowerFact->statFlag = 1;

		/* 充電力率設定値を更新 */
		ps_bttPowerFact->chargeSetpoint = atoi(getStr);

		retCode = commfunc_file_info_get( pc_filePath, NULL, PV_BTTTESLA1_PARAM2, getStr );
		if( retCode == COM_PARAM_GET_OK )
		{
			/* 充電力率進み/遅れの更新 */
			ps_bttPowerFact->chargeLead = atoi(getStr);

			retCode = commfunc_file_info_get( pc_filePath, NULL, PV_BTTTESLA1_PARAM3, getStr );
			if( retCode == COM_PARAM_GET_OK )
			{
				/* 放電力率設定値の更新 */
				ps_bttPowerFact->dischargeSetpoint = atoi(getStr);

				retCode = commfunc_file_info_get( pc_filePath, NULL, PV_BTTTESLA1_PARAM4, getStr );
				if( retCode == COM_PARAM_GET_OK )
				{
					/* 放電力率進み/遅れの更新 */
					ps_bttPowerFact->dischargeLead = atoi(getStr);

					sprintf( errMsg, "Updated reactive mode parameters.");
					commfunc_syslog_msgset( pv_own_name, errMsg );
					sprintf( errMsg, " %s=%d", PV_BTTTESLA1_PARAM1, ps_bttPowerFact->chargeSetpoint );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					sprintf( errMsg, " %s=%d", PV_BTTTESLA1_PARAM2, ps_bttPowerFact->chargeLead );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					sprintf( errMsg, " %s=%d", PV_BTTTESLA1_PARAM3, ps_bttPowerFact->dischargeSetpoint );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					sprintf( errMsg, " %s=%d", PV_BTTTESLA1_PARAM4, ps_bttPowerFact->dischargeLead );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
				
			}
		}
	}

	return retCode;
}
/* [20230721] Add Ed */
/* [20260423] Add St */
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
	thread_stList[threadNumb][stNumb - 1] = status;

	/* どちらかのスレッドで1がある場合、発生 */
	if( (thread_stList[0][stNumb - 1] == 1) || (thread_stList[1][stNumb - 1] == 1) )
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
	int							loop;
	int 						errCnt = 0;

	/* 通信ステータス領域を初期化する */
	memset(thread_stList[threadNumb], 0, PV_MAX_STPOS);

	/* 統合リストを更新 */
	for( loop = 0; loop < sizeof(join_stList); loop++ )
	{
		/* どちらかのスレッドで1がある場合、発生 */
		if( (thread_stList[0][loop] == 1) || (thread_stList[1][loop] == 1) )
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
/* [20260423] Add Ed */
