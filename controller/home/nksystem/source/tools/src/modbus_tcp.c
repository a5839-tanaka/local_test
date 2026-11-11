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
 * 研究開発案件
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <time.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvcomm.h"

/* デバッグフラグ */
#if 1
#define	PV_DEBUG_SERIAL
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_DT3								/* デバッグフラグ：移動平均ストックデータ表示 */
#define PV_CHECK_DROP								/* デバッグフラグ：収取周期と計測値取得時間から欠測かを表示 */
#endif

int	func_process_init( char *, char * );
int commfunc_syslog_msgset_local( char *, char * );

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
 * 研究開発案件
 * -----------------------------------------------------------------------------
 * 	Ｔｅｓｌａ製リチウムイオン電池通信プロセス
 * （タイプ１：Ｅｔｈｅｒｎｅｔ：ＭｏｄｂｕｓＴＣＰ）
 * 対応機種：
 *
 */

/* 通信タイプ（プロトコル判定用） */
#define	PV_TYPE1		1					/* 通信タイプ１ */
#define	PV_TYPE2		2					/* 通信タイプ２（未使用） */
#define	PV_TYPE3		3					/* 通信タイプ３（未使用） */

/* 要求タイプ */
#define	PV_FUNC_03 	0x03					/* ファンクション3（複数レジスタの読み込み） */
#define	PV_FUNC_04 	0x04					/* ファンクション４（保持レジスタの読出し） */
#define	PV_FUNC_06 	0x06					/* ファンクション６（単レジスタの書込み） */
#define	PV_FUNC_10 	0x10					/* ファンクション１０（複数レジスタの書込み） */
#define PV_ST14_LANERR	14
#define PV_MAX_DEV	128

struct st_devSdInf {
	unsigned char		funcCode;					/* ファンクションコード */
	unsigned char		startAdU;					/* 開始アドレス（上位バイト） */
	unsigned char		startAdL;					/* 開始アドレス（下位バイト） */
	unsigned char		dataCntU;					/* 要求データ数（上位バイト） */
	unsigned char		dataCntL;					/* 要求データ数（下位バイト） */
	unsigned char		dataLength;					/* 送信データ長 */
	unsigned char		reqKind;					/* 要求種別(0:電源ON時、1:TCP/IP接続時、2:通常時) */
};

struct st_threadArg {
	int					indexNumb;					/* 処理中インデックス番号 */
	int					stFlg;						/* スレッド開始フラグ */
	int					edFlg;						/* スレッド終了フラグ */
};

/* ＭＭＡＰアクセス用ポインタ変数 */
struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_COMLIINF		*ps_g_COMLIINF;				/* 通信回線情報ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;				/* 計測情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;				/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;				/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */

/* ファイルインデックス（本プロセスで使用するファイルの登録） */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_COMLIINF, (void **)&ps_g_COMLIINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0,}
};

static char					pv_own_name[128];			/* 自プロセス情報 */
static char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int					threadStop = 0;				/* スレッドストップフラグ */
static FILE					*fpOut=NULL;				/* ファイルポインタ */
static int					dataCount = 0;					/* データ数 */

static struct st_serialinf	g_serial;

struct st_reggInfo {
	int				unitId;
	int				funcId;
	int				regAddr;
	int				dataCnt;
	unsigned short	dataVal[128];
	int				dataSize;
	char			regType[32];
	char			regName[512];
};
struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][256];				/* 100データ（1データあたり128バイト） */
};


struct st_reggInfo		s_reggInfo[512];

/* 内部関数定義 */
void	*func_devOpen_thread( void *args );
int		func_devData_recv( struct st_serialinf *, int, int, struct st_devSdInf *, unsigned char *, unsigned char *, struct st_serialLog * );
int		func_abnormal_exit( int );

int	commfunc_serial_cycletmchk_local( struct st_serialinf *, int, int *, struct st_COMLIINF * );
int	commfunc_serial_logout_local( int, struct st_serialinf *, struct st_serialLog * );
int	commfunc_johen_stat_local(int, int, int, int, int, struct st_STSTSFIL * );
int		func_regData_get( int retCode, int reqCnt, unsigned char *dataBuff);
int func_cmn_sep_data_local( char *, struct st_sepStr * );


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信プロセス主処理
 *
 * 機能：
 * 		引数で指定された通信回線番号を元に通信回線情報ファイル
 * 		（COMLIINF.dat）から通信回線の設定情報を取得する。
 * 		引数で指定された通信回線番号を元にバッテリー情報ファイル
 * 		（POWCNINF.dat）から通信回線番号が一致する装置の情報を取得する。
 * 		ファイルに設定された周期でバッテリーと通信を行い、情報を取得し
 * 		て編集結果を指定されたファイルに保存する。
 * 		管理プロセスから停止指示が出た場合はプロセスを終了する。
 *
 */
int main(int argc, char *argv[])
{
	int						retCode;					/* 関数戻り値格納用 */
	pthread_t				threadID;					/* スレッドＩＤ */
	int						mutexCreate = 0;
	
	memset( &g_serial, 0, sizeof(g_serial));

	/* 自プロセス情報を設定 */
	memset(pv_own_name, 0, sizeof(pv_own_name));

	/* プロセス起動時の引数から通信回線番号を取得 */
	if( argc < 2 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("./ModbusTCP inputReggFile\n");
		return( -1 );
	}
	else
	{
		if( func_process_init(argv[1], argv[2]) < 0 )
		{
			return -1;
		}
	}

	/* ミューテックス領域をアタッチ */
	if( commfunc_mutex_attach() != 0 )
	{
		/* ミューテックス領域を作成 */
		if( commfunc_mutex_create() != 0 )
		{
			printf(errMsg, "File Resource Create Error!" );
			commfunc_syslog_msgset_local( pv_own_name, errMsg );
			return( -1 );
		}
		/* ミューテックス作成フラグ設定 */
		mutexCreate = 1;
	}

	/* ファイルをオープンしてＭＭＡＰ化する */
	retCode = commfunc_file_open( s_fileIndex );
	if( retCode < 0 )
	{
		printf(errMsg, "File Open Error");
		commfunc_syslog_msgset_local( pv_own_name, errMsg );
		return( -1 );
	}


	/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
	if( pthread_create( &threadID, NULL, func_devOpen_thread, NULL ) != 0 )
	{

		sprintf( errMsg, "Thread Execute Error" );
		commfunc_syslog_msgset_local( pv_own_name, errMsg );

		/* プロセス異常終了処理 */
		func_abnormal_exit( g_serial.lineNo );
		return( -1 );
	}

	sleep(5);

	// ★複数スレッドの終了について検討する必要がある。
	/* スレッドの終了を待つ */
	threadStop = 1;
	pthread_join( threadID, NULL );

	/* プロセス動作中フラグを解除 */
	//commfunc_manage_runFlagSet( s_lineNo, PV_FLG_STS_RESET );
	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	/* ミューテックス領域を生成していた場合 */
	if( mutexCreate == 1)
	{
		/* ミューテックス領域を削除 */
		commfunc_mutex_destroy();
	}

	if( fpOut != NULL )
	{
		/* ファイルを閉じる */
		fclose( fpOut );
		fpOut = NULL;
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * Ｍｏｄｂｕｓ ＴＣＰ／ＩＰ通信接続処理
 *
 * 機能：
 *		引数 args で指定された情報内に格納されているバッテリーに対
 *		して、接続を行う（オープンからコネクトまで）
 *		実際の接続処理は共通関数 commfunc_serial_open() を使用する。
 *		
 */
void *func_devOpen_thread( void *args )
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
	unsigned  char			sendBuff_w[134] = {
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
	int						directPower;				/* 充放電電力 */
	int						dataLength;					/* 送信データ長 */
	unsigned char			readBuff[4096];				/* 受信バッファ */
	int						openRetCode = -1;			/* シリアルポートオープン結果 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						loopCnt3;					/* ループカウンタ */

	int						pcsNumb;					/* 処理中のバッテリー番号 */
	int						idNumb;						/* 処理中のバッテリー装置ＩＤ */
	struct st_serialLog		s_serialLog;				/* シリアル通信ログ格納領域 */
	struct timespec			sleepTm;					/* 送信待ちタイマ */
	struct st_devSdInf		*ps_devSdInf;				/* 送信情報ポインタ */
	int						workIdxNumb;			/* 処理インデックス番号 */
	struct timeval			nowTime;
	int						reqKind = 0;			/* 要求種別フラグ */
	unsigned char			standby;				/* スタンバイ設定 */
	struct timespec			sleepStanby;			/* スタンバイ時のスリープ時間 */
	struct st_devSdInf	s_bttSdInf[512];			/* 送信情報ポインタ */

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));

	/* パワコン番号の取得（未使用） */
	pcsNumb = 0;

#ifdef PV_DEBUG
	sprintf(errMsg, "func_devOpen_thread : OPEN-Thread START");
	commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif

	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = g_serial.sndTime / 1000;
	sleepTm.tv_nsec = (g_serial.sndTime % 1000) * 1000000;

	/* スタンバイ時のスリープ時間 */
	sleepStanby.tv_sec  = 0;
	sleepStanby.tv_nsec = 100 * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( threadStop == 0 )
	{

		/* シリアルポートをオープン */
		openRetCode = commfunc_serial_open( &g_serial );
		if( openRetCode >= 0 )
		{
			;
		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST14_LANERR, 1,
			    pcsNumb, ps_g_STSTSFIL );

#ifdef PV_DEBUG
			sprintf(errMsg, "OPEN-Thread OPEN ERROR!!");
			commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 5 );
		}

		/* シリアルポートオープンが正常の場合 */
		if( openRetCode >= 0 )
		{
			ps_devSdInf = s_bttSdInf;
			for( loopCnt2 = 0 ; s_reggInfo[loopCnt2].funcId != 0; loopCnt2++ )
			{
				/* 要求実施回数を初期化 */
				g_serial.requestCount = 0;
#ifdef PV_DEBUG
				sprintf(errMsg, "loopCnt[%d / %d] status=%d, funcId=%d", loopCnt2, dataCount, g_serial.status, s_reggInfo[loopCnt2].funcId );
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
				if( ( s_reggInfo[loopCnt2].funcId != PV_FUNC_03 ) &&
				    ( s_reggInfo[loopCnt2].funcId != PV_FUNC_04 ) &&
				    ( s_reggInfo[loopCnt2].funcId != PV_FUNC_06 ) &&
					( s_reggInfo[loopCnt2].funcId != PV_FUNC_10 ) )
				{
					continue;
				}

				/* 送信バッファにファンクションと取得開始アドレス、取得データ数を設定 */
				if( s_reggInfo[loopCnt2].funcId == PV_FUNC_06 )
				{
					p_sendBuff = &sendBuff_w[0];
					p_sendBuff[6] = s_reggInfo[loopCnt2].unitId;
					dataLength = 0x0C;
					p_sendBuff[5]  = dataLength - 6;
					p_sendBuff[7]  = s_reggInfo[loopCnt2].funcId;
					p_sendBuff[8]  = (s_reggInfo[loopCnt2].regAddr / 0x100);
					p_sendBuff[9]  = (s_reggInfo[loopCnt2].regAddr & 0xFF);
					p_sendBuff[10] = (s_reggInfo[loopCnt2].dataVal[0] / 0x100);
					p_sendBuff[11] = (s_reggInfo[loopCnt2].dataVal[0] & 0xFF);

				}
				else if( s_reggInfo[loopCnt2].funcId == PV_FUNC_10 )
				{
					p_sendBuff = &sendBuff_w[0];
					p_sendBuff[6] = s_reggInfo[loopCnt2].unitId;
					dataLength = 13 + ( s_reggInfo[loopCnt2].dataCnt * 2 );
					p_sendBuff[5]  = dataLength - 6;
					p_sendBuff[7]  = s_reggInfo[loopCnt2].funcId;
					p_sendBuff[8]  = (s_reggInfo[loopCnt2].regAddr / 0x100);
					p_sendBuff[9]  = (s_reggInfo[loopCnt2].regAddr & 0xFF);
					p_sendBuff[10] = (s_reggInfo[loopCnt2].dataCnt / 0x100);
					p_sendBuff[11] = (s_reggInfo[loopCnt2].dataCnt & 0xFF);
					p_sendBuff[12] = s_reggInfo[loopCnt2].dataCnt * 2;
					for( loopCnt3 = 0; loopCnt3 < s_reggInfo[loopCnt2].dataCnt; loopCnt3++ )
					{
						p_sendBuff[13 + (loopCnt3*2) ] = (s_reggInfo[loopCnt2].dataVal[loopCnt3] / 0x100);
						p_sendBuff[14 + (loopCnt3*2) ] = (s_reggInfo[loopCnt2].dataVal[loopCnt3] & 0xFF);
					}
				}
				else if( ( s_reggInfo[loopCnt2].funcId == PV_FUNC_03 ) ||
						 ( s_reggInfo[loopCnt2].funcId == PV_FUNC_04 ) )
				{
					p_sendBuff = &sendBuff_r[0];
					p_sendBuff[6] = s_reggInfo[loopCnt2].unitId;
					dataLength = 0x0C;
					p_sendBuff[5] = dataLength - 6;
					p_sendBuff[7] = s_reggInfo[loopCnt2].funcId;
					p_sendBuff[8] = (s_reggInfo[loopCnt2].regAddr / 0x100);
					p_sendBuff[9] = (s_reggInfo[loopCnt2].regAddr & 0xFF);
					p_sendBuff[10] = (s_reggInfo[loopCnt2].dataSize / 0x100);
					p_sendBuff[11] = (s_reggInfo[loopCnt2].dataSize & 0xFF);
				}

				/* 装置IDの取得 */
				idNumb  = s_reggInfo[loopCnt2].unitId ;
				/* 装置IDの取得 */
				ps_devSdInf[loopCnt2].funcCode = s_reggInfo[loopCnt2].funcId;
				ps_devSdInf[loopCnt2].dataCntU = (s_reggInfo[loopCnt2].dataCnt / 0x100);
				ps_devSdInf[loopCnt2].dataCntL = (s_reggInfo[loopCnt2].dataCnt & 0xFF);

				/* 送信待ち処理 */
				nanosleep( &sleepTm, NULL);

				/* シリアルポートオープン正常の場合 */
				if( g_serial.status == PV_STAT_COM_CONNECT )
				{
#ifdef PV_DEBUG_SERIAL
					sprintf(errMsg, "SERIAL POERT OPENED");
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
					/* 処理結果の初期値を異常に設定 */
					retCode = -1;

					/* 要求実施回数の上限まで繰り返す */
					/* 要求実施回数の加算は受信処理のタイムアウト時に実施 */
					while( g_serial.requestCount < PV_SERIAL_REQUEST_MAX )
					{
						/* 要求のリトライは行わない */
						/* シリアル通信ログ領域を初期化 */
						s_serialLog.dataCnt = 0;

						/* バッテリーに要求を送信 */
						retCode = commfunc_serial_send( &g_serial, p_sendBuff, dataLength, &s_serialLog );
						if( retCode < 0 )
						{
							sprintf( errMsg, "TCP Port Write Error! RET:%d num/id:%02d/%02d (%s)",
								retCode, pcsNumb, idNumb, g_serial.targetName );
							commfunc_syslog_msgset_local( pv_own_name, errMsg );

							/* データ送信異常（送信不可異常）発生 */
							/* 状変（通信ステータス状変）発生 */
							commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 1,
								pcsNumb, ps_g_STSTSFIL );

							/* 再要求不要のため処理を抜ける */
							break;
						}

						/* データ処理バッファをクリア */
						memset(readBuff, 0, sizeof(readBuff));

						/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
						retCode = func_devData_recv(	&g_serial, pcsNumb, idNumb,
														&ps_devSdInf[loopCnt2],
														p_sendBuff, readBuff, &s_serialLog );

						/* シリアル通信ログを出力 */
						commfunc_serial_logout_local( PV_SERIAL_LOG_AUTO, &g_serial, &s_serialLog );

						/* データ処理バッファから装置状態と計測値を抽出 */
						func_regData_get( retCode, loopCnt2, readBuff);

						break;

					}
				}
				else
				{
#ifdef PV_DEBUG_SERIAL
					sprintf(errMsg, "SERIAL POERT NOT-OPENED STATUS[%d]", g_serial.status );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
					/* シリアルポートが異常の場合 */
					retCode = -1;
				}
			}
		}
		break;
	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &g_serial );

#ifdef PV_DEBUG
	sprintf(errMsg, "OPEN-Thread END");
	commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
int	func_devData_recv(
	struct st_serialinf *ps_serial,
	int pcsNumb,
	int targetID,
	struct st_devSdInf	*ps_devSdInf,
	unsigned char *sendBuff,
	unsigned char *dataBuff,
	struct st_serialLog *ps_serialLog )
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


	reqFunc = ps_devSdInf->funcCode;
	reqCnt = ps_devSdInf->dataCntU * 256 + ps_devSdInf->dataCntL;

	/* シリアルポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* ファンクションコードから受信データサイズを決定する（期待値） */
	switch( reqFunc )
	{
		case PV_FUNC_06:
			checkLength = 12;
				break;
		case PV_FUNC_10:
		case PV_FUNC_03:
		case PV_FUNC_04:
		default:
			dataByte = reqCnt * 2;
			checkLength = dataByte + 9;
			break;
	}

#ifdef PV_DEBUG
	sprintf(errMsg, "Data-Recv TargetID: %d  (length:%d)  START", targetID, checkLength);
	commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
					sprintf( errMsg, "Serial Port Not Open! Numb:%02d Id:%02d (%s)", pcsNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 1,
						pcsNumb, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRNOTREADY );
					break;
				case PV_SERIAL_ERRTIMEOUT :
					sprintf( errMsg, "Read Timeout! DataLen:%d Numb:%02d Id:%02d (%s)", totalLength, pcsNumb, targetID, ps_serial->targetName);
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 要求実施回数を＋１ */
					ps_serial->requestCount += 1;
					/* 要求実施回数が上限に達している場合 */
					if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
					{
						/* 通信ステータス状変：通信不応答異常（発生） */
						commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST01_NONRESPONSE, 1,
							pcsNumb, ps_g_STSTSFIL );
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", pcsNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST17_NOTSENDERR, 1,
						pcsNumb, ps_g_STSTSFIL );
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
		sprintf(errMsg, "FD:%d : RECV:%3dByte : ", ps_serial->fd_sockfd, readCnt );
#endif
		/* 受信データ数分繰り返す */
		for(loopCnt = 0 ; loopCnt < readCnt ; loopCnt++)
		{
#ifdef PV_DEBUG_SERIAL
		sprintf(&errMsg[strlen(errMsg)], "[%02X]", readBuff[loopCnt] );
#endif
			/* データバッファに受信データを格納 */
			*workPoint = readBuff[loopCnt];
			workPoint++;
			dataLength++;
			/* ９バイト分のデータを受信したら先頭データのチェックを行う */
			if( dataLength == 9 )
			{
				/* トランザクションIDの確認（データフレーム毎に固有の値を使用） */
				/* ＃ツールでは確認しない */
				//if( ( *dataBuff != 0 ) || ( *(dataBuff + 1) != 0 ) )
//				{
//#ifdef PV_DEBUG_SERIAL
//printf("<TRAN-ID ERROR>");
//#endif
//					sprintf( errMsg, "Data-Recv TransactionID Error! Numb:%02d Expect[00][00],Result[%02X][%02X]", pcsNumb, *dataBuff, *(dataBuff+1) );
//					commfunc_syslog_msgset_local( pv_own_name, errMsg );
//					/* 受信データの初期化 */
//					dataLength = 0;
//				}
				/* プロトコルIDの確認（本装置では固定的に0x0000を使用） */
				if( ( *(dataBuff + 2) != 0 ) || ( *(dataBuff + 3) != 0 ) )
				{
					sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d Expect[00][00],Result[%02X][%02X]", pcsNumb, *(dataBuff+2), *(dataBuff+3) );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* 送信先（送信元）アドレスのチェック */
				else if( *(dataBuff + 6) != targetID )
				{
					sprintf( errMsg, "Data-Recv Address Error! Numb:%02d Expect[%02X],Result[%02X]", pcsNumb, 1, *(dataBuff+6) );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
				/* ファンクションコードが不一致の場合 */
				else if ( *(dataBuff+7) != (unsigned char)reqFunc ) 
				{
					sprintf( errMsg, "Data-Recv function Error! Numb:%02d Expect[%02X],Result[%02X]", pcsNumb, reqFunc, *(dataBuff+7) );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 例外レスポンスの場合 */
					if( (*(dataBuff+7) & (unsigned char)0x80) == (unsigned char)0x80 )
					{
						/* 受信データ長（期待値）を9バイトに設定 */
						checkLength = 9;
					}
					else
					{
						/* 受信データの初期化 */
						dataLength = 0;
					}
				}
				/* ファンクションコードが０３でデータバイト数が異なる場合 */
				else if ( (reqFunc == PV_FUNC_03) && ( ( *(dataBuff+5) != (dataByte + 3) ) || (  *(dataBuff+8) != dataByte ) ) )
				{
					sprintf( errMsg, "Data-Recv datasize Error! Numb:%02d Expect[%02X][%02X],Result[%02X][%02X]",
						pcsNumb, dataByte + 3, dataByte, *(dataBuff+5), *(dataBuff+8) );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 受信データの初期化 */
					dataLength = 0;
				}
			}
			/* 受信データが規程の長さに達した場合 */
			if( dataLength >= checkLength)
			{
				/* データ受信完了フラグ設定 */
				flag_GOD = 1;
				/* read処理から抜ける */
				break;
			}
		}
#ifdef PV_DEBUG_SERIAL
		commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
	}
	/* データ受信完了フラグがセットされている？ */
	if( flag_GOD == 1 )
	{
		/* 受信データが例外レスポンスの場合（ */
		if( checkLength == 9 )
		{
#ifdef PV_DEBUG
			sprintf(errMsg, "Data-Poling Error");
			commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
			/* 例外コードが不正ファンクションの場合 */
			if( *(dataBuff+8) == (unsigned char)0x01 )
			{
				/* 通信ステータス状変：下りコマンド異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST19_DWCOMMANDERR, 1,
					pcsNumb, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST22_DWPARAMERR, 1,
					pcsNumb, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST23_DWDATAERR, 1,
					pcsNumb, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが サーバ異常値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x04 )
			{
				/* 通信ステータス状変：デバイス異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST16_DEVICEERROR, 1,
					pcsNumb, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが ＢＵＳＹの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x05 )
			{
				/* 通常時、且つ、制御書込みの場合、ハートビートパターンを切り替える */
				if( (ps_devSdInf->reqKind == 2 ) &&
				    (reqFunc == PV_FUNC_10 ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST27_DWCONTROLERR, 1,
						pcsNumb, ps_g_STSTSFIL );
				}

				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", pcsNumb, *(dataBuff+8));
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 応答待ちの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x06 )
			{
				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", pcsNumb, *(dataBuff+8));
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				/* 要求実施回数を＋１ */
				ps_serial->requestCount += 1;
				/* 要求実施回数が上限に達している場合 */
				if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
				{
					/* 通信ステータス状変：通信不応答異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST01_NONRESPONSE, 1,
						pcsNumb, ps_g_STSTSFIL );
				}
				/* タイムアウト */
				return( PV_SERIAL_ERRTIMEOUT );
			}
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST20_DWFORMATERR, 1,
					pcsNumb, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
		}
		else
		{
			/* ファンクションコードが書き込み(１０，６）の場合 */
			if(( reqFunc == PV_FUNC_10 ) ||
			   ( reqFunc == PV_FUNC_10 ))
			{
				/* 開始アドレスとレジスタ数が異なる場合 */
				/* （開始アドレスと設定値が異なる場合） */
				if( (*(dataBuff+8)  != sendBuff[8]  ) ||
				    (*(dataBuff+9)  != sendBuff[9]  ) ||
				    (*(dataBuff+10) != sendBuff[10] ) ||
				    (*(dataBuff+11) != sendBuff[11] ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST27_DWCONTROLERR, 1,
						pcsNumb, ps_g_STSTSFIL );
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
			commfunc_johen_stat_local( PV_MKIND_POWCN, pcsNumb, PV_ST01_NONRESPONSE, 1,
				pcsNumb, ps_g_STSTSFIL );
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
	//commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_SET );
	/* プロセス動作中フラグを解除 */
	//commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_RESET );
	/* ファイルをオープンしてＭＭＡＰ化する */
	commfunc_file_close( s_fileIndex );

	if( fpOut != NULL )
	{
		/* ファイルを閉じる */
		fclose( fpOut );
		fpOut = NULL;
	}

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ログ出力処理
 *
 * 機能：
 *		引数 logMode の設定により、ps_serialLog に格納されている通信ログを出力す
 * 		る。　logMode に PV_SERIAL_LOG_ON が設定されている場合は、強制的にログを
 *		出力する。PV_SERIAL_LOG_AUTO が設定されている場合は、管理情報ファイルの
 *		通信回線毎のログ出力設定が ON の場合に出力を行う。
 *		出力されるログファイルは通信回線毎に作成され、履歴は過去３回分まで保持さ
 *		れる。
 *
 * 入力：
 *		logMode       : ログ出力モード
 *		               1 (PV_SERIAL_LOG_ON)   : 強制ログ出力
 *		               0 (PV_SERIAL_LOG_AUTO) : 出力設定による自動判断
 *		*ps_serialinf : シリアルポート設定情報ポインタ
 *		*ps_serialLog : ログ格納領域ポインタ
 *
 * 出力：
 *		0             : 正常処理
 *		-1            : 異常終了
 *
 */
int commfunc_serial_logout_local( int logMode, struct st_serialinf *ps_serialinf, struct st_serialLog *ps_serialLog )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
	char					*pt1;					/* ポインタ */
	char					*pt2;					/* ポインタ */
	int						fd;						/* ファイルディスクリプタ */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */
	struct tm				localTime;				/* 年月日時分秒 */
/* [20131203] Chg St */
/*	char					kindName[3][3] = { "SD", "RD", "ST" };	*/
	char					kindName[6][5] = { "SEND", "RECV", "SETD", "ERRO", "TOUT", "OVER" };
/* [20131203] Chg St */

	/* シリアルログメッセージ出力処理 */
	for( loopCnt1 = 0 ; loopCnt1 < ps_serialLog->dataCnt ; loopCnt1++ )
	{
		/* 年月日時分秒を取得 */
		gmtime_r( &ps_serialLog->s_logData[loopCnt1].sec, &localTime );
		printf(  "%04d/%02d/%02d %02d:%02d:%02d.%06d %s[%3d] ",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			ps_serialLog->s_logData[loopCnt1].uSec,
			kindName[ps_serialLog->s_logData[loopCnt1].kind],
			ps_serialLog->s_logData[loopCnt1].dataLen );

		if( ( ( ps_serialinf->deviceType == PV_STYPE_DIRTCP ) ||
		      ( ps_serialinf->deviceType == PV_STYPE_UDPDIR ) ||
		      ( ps_serialinf->deviceType == PV_STYPE_UDPOTH ) ) &&
		    ( ( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_SEND ) ||
		      ( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_READ ) ) )
		{
			pt1 = (char *)&ps_serialLog->s_logData[loopCnt1].locIPAddr;
			pt2 = (char *)&ps_serialLog->s_logData[loopCnt1].dstIPAddr;
			printf("(%d.%d.%d.%d:%d)-(%d.%d.%d.%d:%d)",
				*(pt1), *(pt1 + 1), *(pt1 + 2), *(pt1 + 3), ps_serialLog->s_logData[loopCnt1].locPort,
				*(pt2), *(pt2 + 1), *(pt2 + 2), *(pt2 + 3), ps_serialLog->s_logData[loopCnt1].dstPort );

			for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
			{
				if( ( loopCnt2 % 16 ) == 0 )
					printf("\n  %03Xh(%04d): ", loopCnt2, loopCnt2);

				printf("%02X ", (unsigned char)ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
			}

			printf("\n");
		}
		else
		{
			for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
			{
				printf("%02X ", (unsigned char)ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
			}
			printf("\n");
		}
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログファイル格納処理
 *
 * 書式：
 * 		int commfunc_syslog_msgset_local(
 * 			char *pName,
 * 			char *pMesg )
 * 機能：
 * 		システムログファイル（PVSYSLOG.dat）に引数 pName のプロセス名と pMesg の
 * 		ログメッセージを保存する。
 * 		保存時には保存処理を行った時刻として積算秒の情報を付加して保存する。
 * 戻り値：
 * 		0：正常処理 ／ -1：異常終了
 * 入力：
 * 		*pName
 * 			プロセス名称（最大１２文字）
 * 		*pMesg
 * 			ログメッセージ
 */
int commfunc_syslog_msgset_local(
	char *pName,
	char *pMesg )
{
	char				recordMesg[PV_MSG_SIZE_LOG];
	struct timeval		nowtime;
	struct tm			localTime;

	/* 記録情報を初期化 */
	memset( recordMesg, 0, PV_MSG_SIZE_LOG );
	/* プロセス名をコピー */
	strncpy( recordMesg, pName, PV_MSG_SIZE_LOG - 1 );
	strcat( recordMesg, ": " );
	/* メッセージをコピー */
	strncat( recordMesg, pMesg, PV_MSG_SIZE_LOG - strlen(recordMesg) );

	/* 現在時刻を取得 */
	gettimeofday( &nowtime, NULL );

	/* 年月日時分秒を取得 */
	gmtime_r( &nowtime.tv_sec, &localTime );

	/* メッセージを生成 */
	printf( "%04d/%02d/%02d %02d:%02d:%02d.%06d %s\n",
		localTime.tm_year + 1900,
		localTime.tm_mon + 1,
		localTime.tm_mday,
		localTime.tm_hour,
		localTime.tm_min,
		localTime.tm_sec,
		nowtime.tv_usec,
		recordMesg );


	
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測周期確認処理
 *
 * 機能：
 * 		引数 ps_serial に格納されている通信回線の設定値と、引数 targetCnt で指定
 * 		された通信対象機器数、引数 requestCnt で指定された１台あたりのリクエスト
 * 		回数を元に、収集周期が適正値であるか確認する。
 * 		収集周期が適正値ではない場合は、収集周期を変更する。
 * 		収集周期から１分間の移動平均算出に必要なデータ数を引数 stockCnt に設定す
 * 		る。
 */
int	commfunc_serial_cycletmchk_local(
	struct st_serialinf *ps_serial,			/* シリアル設定情報 */
	int requestCnt,							/* １収集周期で要求する回数 */
	int *stockCnt,							/* PV_PROC_TMSTOCK_TIME秒で蓄積するデータ数 */
	struct st_COMLIINF *ps_COMLIINF			/* 通信回線情報ファイルポインタ */
)
{
	int			oldTime = 0;				/* 変更前タイマ */
	int			workTime = 0;				/* ワーク */
	char		errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	/* 変更前タイマを取得 */
	oldTime = ps_serial->cycleTime;

	/* 収集周期を本装置で処理可能な周期に調整 */
	/* 設定可能な値は 300,60,30,20,15,12,10,6,5,4,3,2,1 秒 */
	if( oldTime > 60)
		ps_serial->cycleTime = 300;
	else if( oldTime > 30 )
		ps_serial->cycleTime = 60;
	else if( oldTime > 20 )
		ps_serial->cycleTime = 30;
	else if( oldTime > 15 )
		ps_serial->cycleTime = 20;
	else if( oldTime > 12 )
		ps_serial->cycleTime = 15;
	else if( oldTime > 10 )
		ps_serial->cycleTime = 12;
	else if( oldTime > 6 )
		ps_serial->cycleTime = 10;
	else if( oldTime > 5 )
		ps_serial->cycleTime = 6;
	else if( oldTime > 4 )
		ps_serial->cycleTime = 5;
	else if( oldTime > 3 )
		ps_serial->cycleTime = 4;
	else if( oldTime > 2 )
		ps_serial->cycleTime = 3;
	else if( oldTime > 1 )
		ps_serial->cycleTime = 2;
	else
		ps_serial->cycleTime = 1;

	/* 受信応答タイマと送信待ちタイマの合計値に要求回数分を考慮 */
	/* その他の処理時間（移動平均算出や計測値保存ファイルへの出力処理）を100ms考慮 */
	workTime = ( ps_serial->rcvTime + ps_serial->sndTime ) * requestCnt + 100;

	/* 合計値が収集周期を超過している場合 */
	if( workTime > (ps_serial->cycleTime * 1000 ) )
	{
		/* 収集周期を演算結果から調整 */
		if( workTime > 60000 )
			ps_serial->cycleTime = 300;
		else if( workTime > 30000 )
			ps_serial->cycleTime = 60;
		else if( workTime > 20000 )
			ps_serial->cycleTime = 30;
		else if( workTime > 15000 )
			ps_serial->cycleTime = 20;
		else if( workTime > 12000 )
			ps_serial->cycleTime = 15;
		else if( workTime > 10000 )
			ps_serial->cycleTime = 12;
		else if( workTime > 6000 )
			ps_serial->cycleTime = 10;
		else if( workTime > 5000 )
			ps_serial->cycleTime = 6;
		else if( workTime > 4000 )
			ps_serial->cycleTime = 5;
		else if( workTime > 3000 )
			ps_serial->cycleTime = 4;
		else if( workTime > 2000 )
			ps_serial->cycleTime = 3;
		else if( workTime > 1000 )
			ps_serial->cycleTime = 2;
		else
			ps_serial->cycleTime = 1;
	}

    /* 収集周期から移動平均ストック数を設定 */
	if( ps_serial->cycleTime == 1)
		*stockCnt = 60;
	else if( ps_serial->cycleTime == 2 )
		*stockCnt = 30;
	else if( ps_serial->cycleTime == 3 )
		*stockCnt = 20;
	else if( ps_serial->cycleTime == 4 )
		*stockCnt = 15;
	else if( ps_serial->cycleTime == 5 )
		*stockCnt = 12;
	else if( ps_serial->cycleTime == 6 )
		*stockCnt = 10;
	else if( ps_serial->cycleTime == 10 )
		*stockCnt = 6;
	else if( ps_serial->cycleTime == 12 )
		*stockCnt = 5;
	else if( ps_serial->cycleTime == 15 )
		*stockCnt = 4;
	else if( ps_serial->cycleTime == 20 )
		*stockCnt = 3;
	else if( ps_serial->cycleTime == 30 )
		*stockCnt = 2;
	else
		*stockCnt = 1;

	sprintf( errMsg, "CycleTim Adjust [ Line:%02d CycleTim:%d->%ds AvlStockCnt:%d , Ref(TotalReqCnt:%d TotalTime:%dms) ]",
		ps_serial->lineNo, oldTime, ps_serial->cycleTime, *stockCnt, requestCnt, workTime );
	commfunc_syslog_msgset( "commfunc_serial_cycletmchk()", errMsg );


	/* 通信回線情報に収集周期を格納 */
//	(ps_COMLIINF + ps_serial->lineNo - 1)->cycleTime = ps_serial->cycleTime;

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状変処理（単独処理）
 *
 * 書式：
 * 		int	commfunc_johen_stat_local(
 * 			int soKind,
 * 			int unitId,
 * 			int stNumb,
 * 			int svStat,
 * 			int ststLink,
 * 			struct st_STSTSFIL *ps_STSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、unitId（装置番号）、stNumb（状態番号）、stStat
 * 		（状態情報）の情報を元に、通信ステータス状態情報保存ファイル
 * 		（STSTSFIL.dat）に格納されている状態情報を比較し、状態が不一致の場合は状
 * 		変処理を行う。
 * 		状態情報の不一致確認後は、stStat に格納されている状態情報でSTSTSFIL.dat
 * 		を更新する。更新位置はststLink（比較開始位置）を元に行う。
 *		その他の引数は、ps_STSTSFIL（通信ステータス状態格納ファイルのMMAPポイン
 *		タ情報）を引数として処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		unitId
 * 			装置番号
 * 		stNumb
 * 			状態番号
 * 		stStat
 * 			状態情報
 * 		ststLink
 * 			通信ステータス状態保存ファイルリンク情報
 * 		*ps_STSTSFIL
 * 			通信ステータス状態保存ファイルMMAPポインタ
 */
int	commfunc_johen_stat_local(
	int soKind,									/* 装置種別 */
	int unitId,									/* 装置番号 */
	int stNumb,									/* 状態番号 */
	int	stStat,									/* 通信ステータス状態 */
	int ststLink,								/* 通信ステータス状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_STSTSFIL *ps_STSTSFIL )
{
	int					stPoint;				/* 状変ポイント位置 */
	unsigned char		stBit;					/* 状変ビット位置 */
	unsigned char		stOld;					/* 通信ステータス状態（旧状態） */
	unsigned char		stNew;					/* 通信ステータス状態（新状態） */
	unsigned char		stChek;					/* 状変検出 */

	switch( stNumb )
	{
			/* 通信ステータス状変：通信不応答異常（発生） */
		case PV_ST01_NONRESPONSE :
			sprintf(errMsg, "通信不応答異常");
			break;
			/* 通信ステータス状変：下り制御異常（発生） */
		case PV_ST27_DWCONTROLERR :
			sprintf(errMsg, "下り制御異常");
			break;
			/* 通信ステータス状変：下りフォーマット異常（発生） */
		case PV_ST20_DWFORMATERR :
			sprintf(errMsg, "下りフォーマット異常");
			break;
			/* 通信ステータス状変：デバイス異常（発生） */
		case PV_ST16_DEVICEERROR :
			sprintf(errMsg, "デバイス異常");
			break;
			/* 通信ステータス状変：下りデータ異常（発生） */
		case PV_ST23_DWDATAERR :
			sprintf(errMsg, "下りデータ異常");
			break;
			/* 通信ステータス状変：下りパラメータ異常（発生） */
		case PV_ST22_DWPARAMERR :
			sprintf(errMsg, "下りパラメータ異常");
			break;
			/* 通信ステータス状変：下りコマンド異常（発生） */
		case PV_ST19_DWCOMMANDERR :
			sprintf(errMsg, "下りコマンド異常");
			break;
			/* 通信ステータス状変：下り送信不可異常（発生） */
		case PV_ST17_NOTSENDERR :
			sprintf(errMsg, "下り送信不可異常");
			break;
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
		case PV_ST14_LANERR :
			sprintf(errMsg, "ＬＡＮ接続異常");
			break;
	}

	if( stStat == 1 )
		sprintf(&errMsg[strlen(errMsg)], ":（発生）");
	else
		sprintf(&errMsg[strlen(errMsg)], ":（復帰）");
	
	commfunc_syslog_msgset_local( pv_own_name, errMsg );

	return( 0 );
}



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス初期化処理
 *
 * 機能：
 * 		プロセス初期化処理
 * 戻り値：
 * 	    0：正常
 * 	   -1：異常
 */
int	func_process_init( char *inputPath, char *outputPath )
{
	FILE					*fp;							/* ファイルポインタ */
	char					readBuff[256];					/* ファイル読み込みバッファ */
	char					filePath[256];					/* ファイルパス */
	int						sepCount;						/* データ数（デリミタ分割数） */
	int						retCode = 0;					/* 戻り値 */
	struct st_sepStr		s_sepStr;						/* データ分割格納 */
	int						syoriCount = 0;					/* 処理カウンタ */
	int						loopCnt1 = 0;

	/* ファイルパスの展開 */
//	strcpy(filePath, "./" );
//	strcat(filePath, inputPath );
	strcpy(filePath, inputPath );

	/* 情報を初期化 */
	memset(s_reggInfo, 0, sizeof(s_reggInfo));

	/* CSVファイルオープン */
	if( (fp = fopen(filePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 256, fp) != NULL )
		{
			/* CSV処理カウンタを＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' )
			{
				continue;
			}
			/* 未入力行はスキップ */
			if( strlen( readBuff ) == 0 ) continue;
			
			/* データの分割 */
			sepCount = func_cmn_sep_data_local( readBuff, &s_sepStr );
//[%d],Count[%d]:%s", syoriCount, sepCount, readBuff);

			/* データ数が0の場合はスキップ */
			if( sepCount == 0 ) continue;

			if( sepCount == 2 )
			{

				sprintf(g_serial.targetName, "%s", s_sepStr.data[0] );
				g_serial.portNumb = atoi( s_sepStr.data[1] );
				g_serial.deviceType = PV_STYPE_TCP;
				g_serial.lineNo = 1;
				g_serial.rcvTime = 1000;
				g_serial.sndTime = 500;

				sprintf(errMsg, "******* ModbusTCP Info ******");
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				sprintf(errMsg, "TargetName(Port) : %s:%d", g_serial.targetName, g_serial.portNumb);
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				sprintf(errMsg, "recieveTime(ms)  : %d", g_serial.rcvTime);
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				sprintf(errMsg, "sendWaitTime(ms) : %d", g_serial.sndTime);
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				sprintf(errMsg, "*****************************");
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
			}
			else
			{
				if( atoi( s_sepStr.data[1] ) == PV_FUNC_06 )
				{
					if( sepCount != 4 )
					{
						sprintf( errMsg, "Function 0x06 : File Format Error (Data is not 3). line[%d] DATACNT[%d]", syoriCount, sepCount );
						retCode = -1;
						break;
					}

					s_reggInfo[dataCount].unitId = atoi( s_sepStr.data[0] );
					s_reggInfo[dataCount].funcId = atoi( s_sepStr.data[1] );
					s_reggInfo[dataCount].regAddr = atoi( s_sepStr.data[2] );
					s_reggInfo[dataCount].dataVal[0] = atoi( s_sepStr.data[3] );
					s_reggInfo[dataCount].dataCnt = 1;
					
					sprintf(errMsg, "[%d] : unitId[%d], funcId[%d], regAddr[%d], Value[%d]", dataCount, s_reggInfo[dataCount].unitId, s_reggInfo[dataCount].funcId, s_reggInfo[dataCount].regAddr, s_reggInfo[dataCount].dataVal[0] );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					
				}
				else if( atoi( s_sepStr.data[1] ) == PV_FUNC_10 )
				{
					if( sepCount < 4 )
					{
						sprintf( errMsg, "Function 0x06 : File Format Error (Data is less than 3). line[%d] DATACNT[%d]", syoriCount, sepCount );
						retCode = -1;
						break;
					}

					s_reggInfo[dataCount].unitId = atoi( s_sepStr.data[0] );
					s_reggInfo[dataCount].funcId = atoi( s_sepStr.data[1] );
					s_reggInfo[dataCount].regAddr = atoi( s_sepStr.data[2] );
					s_reggInfo[dataCount].dataCnt = sepCount - 3;
					sprintf(errMsg, "[%d] : unitId[%d], funcId[%d], regAddr[%d], Value", dataCount, s_reggInfo[dataCount].unitId, s_reggInfo[dataCount].funcId, s_reggInfo[dataCount].regAddr );
					for( loopCnt1 = 0; loopCnt1 < s_reggInfo[dataCount].dataCnt; loopCnt1++ )
					{
						s_reggInfo[dataCount].dataVal[loopCnt1] = atoi( s_sepStr.data[3 + loopCnt1] );
						sprintf(&errMsg[strlen(errMsg)], "[0x%04x(%d)]", s_reggInfo[dataCount].dataVal[loopCnt1], s_reggInfo[dataCount].dataVal[loopCnt1] );
					}
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
				}
				else if( ( atoi( s_sepStr.data[1] ) == PV_FUNC_03 ) ||
						 ( atoi( s_sepStr.data[1] ) == PV_FUNC_04 ) )
				{
					if( sepCount != 6 )
					{
						sprintf( errMsg, "Function 0x03/0x04 : File Format Error (Data is not 5). line[%d] DATACNT[%d]", syoriCount, sepCount );
						retCode = -1;
						break;
					}
					s_reggInfo[dataCount].unitId = atoi( s_sepStr.data[0] );
					s_reggInfo[dataCount].funcId = atoi( s_sepStr.data[1] );
					s_reggInfo[dataCount].regAddr = atoi( s_sepStr.data[2] );
					s_reggInfo[dataCount].dataSize = atoi( s_sepStr.data[3] );
					memcpy( s_reggInfo[dataCount].regType, s_sepStr.data[4], 32 );
					memcpy( s_reggInfo[dataCount].regName, s_sepStr.data[5], 512 );
					s_reggInfo[dataCount].dataCnt = s_reggInfo[dataCount].dataSize;
					sprintf(errMsg, "[%d] : unitId[%d], funcId[%d], regAddr[%d], dataSize[%d]", dataCount, s_reggInfo[dataCount].unitId, s_reggInfo[dataCount].funcId, s_reggInfo[dataCount].regAddr, s_reggInfo[dataCount].dataSize );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					
				}
				/* データ数をカウント */
				dataCount++;
			}
		}
		/* CSVファイルをクローズ */
		fclose( fp );

	}
	else
	{
		sprintf( errMsg, "File[%s] Open error", filePath );
		retCode = -1;
	}

	if( retCode < 0 )
	{
		commfunc_syslog_msgset_local( pv_own_name, errMsg );
	}
	if( outputPath != NULL )
	{
		/* ファイルパスの展開 */
//		strcpy(filePath, "./" );
//		strcat(filePath, outputPath );
		strcpy(filePath, outputPath );

		/* CSVファイルオープン */
		if( (fpOut = fopen(filePath, "w")) == NULL)
		{
			printf("出力ファイルのオープンに失敗しました\n");
			retCode = -1;
		}
	}

	/* 終了 */
	return( retCode );	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ解析処理
 *
 * 		
 * 戻り値：
 * 		0：正常処理
 */
int	func_regData_get( int retCode, int reqCnt, unsigned char *dataBuff)
{
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char			dataByte[16];			/* データ格納用領域 */
		short int				data16bit;				/* 16bitデータ取得用 */
		unsigned short int		data16bitu;				/* 16bitデータ取得用（符号無し） */				
		unsigned long long int	data64bitu;			/* 64bitデータ取得用（符号無し） */				
		int						data32bit;				/* 32bitデータ取得用 */
		unsigned int			data32bitu;				/* 32bitデータ取得用 */
		float					data32f;				/* 32bitデータ取得用 */
	} u_change;
	int				loopCnt2;						/* ループカウンタ */
	int		tmVal = 0;
	char						outputStr[128];

	/* 変換用領域の初期化 */
	memset( &u_change, 0, sizeof(u_change));

	if( retCode == 0 )
	{
		sprintf(outputStr, "Success: ");
	}
	else
	{
		sprintf(outputStr, "Failure: ");
	}

	if( dataBuff[7] == PV_FUNC_06 )
	{
		sprintf( &outputStr[strlen(outputStr)], "FuncId=0x%02x, StartAddr=0x%04X(%04d), Value=0x%04X(%d)", dataBuff[7], ((dataBuff[8]*256)+dataBuff[9]), ((dataBuff[8]*256)+dataBuff[9]), ((dataBuff[10]*256)+dataBuff[11]), ((dataBuff[10]*256)+dataBuff[11]));
	}
	else if( dataBuff[7] == PV_FUNC_10 )

	{
		sprintf( &outputStr[strlen(outputStr)], "FuncId=0x%02x, StartAddr=0x%04X(%04d), RegCnt=%d", dataBuff[7], ((dataBuff[8]*256)+dataBuff[9]), ((dataBuff[10]*256)+dataBuff[11]));
	}
	else if( ( dataBuff[7] == PV_FUNC_03 ) ||
			 ( dataBuff[7] == PV_FUNC_04 ) )
	{
		/* レジスタタイプ毎の処理 */
		if( strncmp( s_reggInfo[reqCnt].regType, "string", 32) == 0 )
		{
			sprintf(outputStr, "%s", &dataBuff[9]);
#ifdef PV_DEBUG_DT1
//		sprintf(errMsg, "Addr[0x%04X]:Val[%s]:NAME[%s]",
//			s_reggInfo[reqCnt].regAddr, tmVal, s_reggInfo[reqCnt].regName );
//		commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
		}
		else
		{
			sprintf(&outputStr[strlen(outputStr)], "FuncId=0x%02x, StartAddr=0x%04X(%04d)", dataBuff[7], s_reggInfo[reqCnt].regAddr, s_reggInfo[reqCnt].regAddr );
			sprintf(&outputStr[strlen(outputStr)], ", Type=%-8s", s_reggInfo[reqCnt].regType);
			sprintf(&outputStr[strlen(outputStr)], ", Value=");
			/* 計測値の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
			for( loopCnt2 = 1 ; loopCnt2 <= (s_reggInfo[reqCnt].dataSize*2) ; loopCnt2++ )
				u_change.dataByte[loopCnt2 - 1] = dataBuff[9 + (s_reggInfo[reqCnt].dataSize*2) - loopCnt2];

			/* 計測値のサイズが２バイトサイズの場合 */
			if( strncmp( s_reggInfo[reqCnt].regType, "int16", 32) == 0 )
			{
				/* 符号付き16bitサイズのデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%04X(%d)", u_change.data16bit, u_change.data16bit );
			}
			/* 計測値のサイズが４バイトサイズの場合 */
			else if( strncmp( s_reggInfo[reqCnt].regType, "int32", 32) == 0 )
			{
				/* 符号付き32bitサイズのデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%08X(%d)", u_change.data32bit, u_change.data32bit );
			}
			/* 計測値のサイズが４バイトサイズの場合 */
			else if( strncmp( s_reggInfo[reqCnt].regType, "uint32", 32) == 0 )
			{
				/* 32bitサイズのデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%08X(%d)", u_change.data32bitu, u_change.data32bitu );
			}
			else if( strncmp( s_reggInfo[reqCnt].regType, "uint16", 32) == 0 )
			{
				/* 符号なし16bitサイズのデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%04X(%d)", u_change.data16bitu, u_change.data16bitu );
			}
			else if( strncmp( s_reggInfo[reqCnt].regType, "uint64", 32) == 0 )
			{
				/* 符号なし64bitサイズのデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%llx(%llu)", u_change.data64bitu, u_change.data64bitu );
			}
			else if( strncmp( s_reggInfo[reqCnt].regType, "float32", 32) == 0 )
			{
				/* 32bitサイズのFloatデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%08X(%.4f)", u_change.data32bitu, u_change.data32f );
			}
			else
			{
				/* 暫定的に16bitサイズのデータとして計測値を取得 */
				sprintf(&outputStr[strlen(outputStr)], "0x%04X(%d)", u_change.data16bit, u_change.data16bit );
			}
			sprintf(&outputStr[strlen(outputStr)], ", Name=%-30s", s_reggInfo[reqCnt].regName);

#ifdef PV_DEBUG_DT1
//		sprintf(errMsg, "Addr[0x%04X]:Val[%s]:NAME[%s]",
//				s_reggInfo[reqCnt].regAddr, outputStr, s_reggInfo[reqCnt].regName );
//		commfunc_syslog_msgset_local( pv_own_name, errMsg );
#endif
		}
	}


	if( fpOut != NULL )
	{
		fprintf( fpOut, "%s\n", outputStr);
	}
	else
	{
		commfunc_syslog_msgset_local( pv_own_name, outputStr );
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶ分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列をカンマで区切り、ps_sepStr に格納する。
 * 戻り値：
 * 		カンマで区切られたデー多数を返す。
 */
int func_cmn_sep_data_local( char *pc_data, struct st_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_sepStr, 0, sizeof(struct st_sepStr) );

	pt = pc_data;
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		if( *pt == ',' )
		{
			ps_sepStr->data[idx1][idx2] = 0;
			idx2 = 0;
			idx1++;
			continue;
		}
		ps_sepStr->data[idx1][idx2] = *pt;
		idx2++;
	}
	ps_sepStr->data[idx1][idx2] = 0;
	if(idx2) idx1++;

	return( idx1 );
}


