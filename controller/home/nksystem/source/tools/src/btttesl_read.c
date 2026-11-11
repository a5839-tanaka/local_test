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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvcomm.h"

//#include "btttesl1.h"

/* デバッグフラグ */
#if 0
#define	PV_DEBUG_SERIAL
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：受信データ解析結果表示 */
#define	PV_DEBUG_DT3								/* デバッグフラグ：移動平均ストックデータ表示 */
#define PV_CHECK_DROP								/* デバッグフラグ：収取周期と計測値取得時間から欠測かを表示 */
#endif

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
#define	PV_BTTTESLA1_TYPE1		1					/* 通信タイプ１ */
#define	PV_BTTTESLA1_TYPE2		2					/* 通信タイプ２（未使用） */
#define	PV_BTTTESLA1_TYPE3		3					/* 通信タイプ３（未使用） */

/* 要求タイプ */
#define	PV_BTTTESLA1_FUNC_01 0x01					/* ファンクション１（コイルの読出し） */
#define	PV_BTTTESLA1_FUNC_02 0x02					/* ファンクション２（入力ステータスの読出し） */
#define	PV_BTTTESLA1_FUNC_03 0x03					/* ファンクション３（保持レジスタの読出し） */
#define	PV_BTTTESLA1_FUNC_04 0x04					/* ファンクション４（入力レジスタの読出し） */
#define	PV_BTTTESLA1_FUNC_10 0x10					/* ファンクション１０（複数レジスタの書込み） */

struct st_bttTeslaSdInf {
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

/* 対応機種情報 */
/* 対応機種情報：型式名称 */
struct st_devTypeName	s_bttTypeName[] =
{
	/*	      +----------------------------------- 型式名称     */
	/*	      |                   +--------------- 通信処理方式 */
	/*	      |                   |           +--- 基本要求回数 */
	/*-1234567890123456           |           |    （型式名称は16文字以内で定義） */
	{ "TESLA-POWERPK20",     PV_BTTTESLA1_TYPE1,  3 },
	{ "-STOPPER-",                       -1, -1 }
};


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
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */

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
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0,}
};

static char					pv_own_name[128];			/* 自プロセス情報 */
static char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int					bttId2Numb[PV_MAX_DEVICE];	/* 装置ID-パワコン番号変換用 */
static int					bttId2Index[PV_MAX_DEVICE];	/* 装置ID-インデックス番号変換 */
static int					bttMaxCnt = 0;				/* 処理対象バッテリー数 */
static struct st_serialinf	s_serial[PV_MAX_BATTE];		/* シリアル回線情報 */
static struct st_stdDevInfo	s_bttInfo[PV_MAX_BATTE];	/* バッテリー情報格納領域 */
static struct st_bttData	s_bttData[PV_MAX_BATTE];	/* 計測情報（瞬時値格納用） */
static struct st_threadArg	s_threadArg[PV_MAX_BATTE];	/* スレッド管理情報 */
static int					threadStop = 0;				/* スレッドストップフラグ */
static unsigned char		heartBeatIndex=0;			/* ハートビートパターンインデックス */
static FILE					*fpOut;					/* ファイルポインタ */


struct st_reggInfo {
	int			regAddr;
	int			dataSize;
	char		regType[32];
	char		regName[512];
};
struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};


static struct st_reggInfo		s_reggInfo[512];

/* 内部関数定義 */
void	*func_bttOpen_thread( void *args );
int		func_bttData_recv( struct st_serialinf *, int, int, struct st_bttTeslaSdInf *, unsigned char *, unsigned char *, struct st_serialLog * );
int		func_abnormal_exit( int );

int	commfunc_serial_infoget_local( char, struct st_serialinf *, struct st_COMLIINF *, char * );
int	commfunc_serial_open_local( struct st_serialinf * );
int	commfunc_serial_cycletmchk_local( struct st_serialinf *, int, struct st_COMLIINF * );
int	commfunc_serial_logout_local( int, struct st_serialinf *, struct st_serialLog * );
int	commfunc_johen_stat_local(int, int, int, int, int, struct st_STSTSFIL * );
int		func_regData_get( int reqCnt, unsigned char *dataBuff);
int func_sep_data( char *, struct st_sepStr * );
int	func_process_init( char *, char * );
int commfunc_syslog_msgset_local( char *, char * );


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
	int						requestCnt = 0;				/* 合計要求回数 */
	int						bttNumb;					/* 処理中のバッテリー番号 */
	int						timeOut = 0;				/* タイムアウト値 */
	struct timeval			nowTime;					/* 現在時刻 */
	struct timeval 			wrkTime;					/* 比較時刻 */
	int						dropFlg;					/* 欠測フラグ */
	struct st_bttData		bttData[PV_MAX_BATTE];		/* 計測情報 */
	int						mutexCreate = 0;
	

	
	fpOut = NULL;

	/* プロセス起動時の引数から通信回線番号を取得 */
	if( argc < 2 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("btttesl_read inputReggFile outputFile\n");
		return( -1 );
	}
	else
	{
		if( func_process_init(argv[1], argv[2]) < 0 )
		{
			return -1;
		}
	}


	/* 自プロセス情報を設定 */
	sprintf( pv_own_name, "%s(%02d)", PV_OWN_NAME, lineNo );

	/* ミューテックス領域をアタッチ */
	if( commfunc_mutex_attach() != 0 )
	{
		/* ミューテックス領域を作成 */
		if( commfunc_mutex_create() != 0 )
		{
			printf("%s : File Resource Create Error!\n", PV_OWN_NAME );
			if( fpOut != NULL )
				/* ファイルを閉じる */
				fclose( fpOut );
			return( -1 );
		}
		/* ミューテックス作成フラグ設定 */
		mutexCreate = 1;
	}

	/* ファイルをオープンしてＭＭＡＰ化する */
	retCode = commfunc_file_open( s_fileIndex );
	if( retCode < 0 )
	{
		printf("%s : File Open Error\n", PV_OWN_NAME);
		if( fpOut != NULL )
			/* ファイルを閉じる */
			fclose( fpOut );
		return( -1 );
	}

	/* 通信回線番号を検索 */
	lineNo = -1;
	for( loopCnt1 = 0; loopCnt1 < PV_MAX_COMLINE; loopCnt1++ )
	{
		if( strncmp( ps_g_COMLIINF[loopCnt1].protoName, PV_COMPROTOCOL, 12 ) == 0 )
		{
			lineNo = loopCnt1 + 1;
			break;
		}

	}

	if( lineNo == -1 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("COMLINE not Found\n");
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return -1;
	}

	/* プロセス動作中フラグを設定 */
	//commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_SET );

	/* シリアル回線情報を初期化 */
	memset( s_serial, 0, sizeof(s_serial) );
	/* データ格納領域を初期化 */
	memset( &s_bttData, 0, sizeof(s_bttData));
	/* スレッド管理情報を初期化 */
	memset( s_threadArg, 0, sizeof(s_threadArg));

	/* 通信回線情報を取得 */
	if( (retCode = commfunc_serial_infoget_local( lineNo, &s_serial[0], ps_g_COMLIINF, PV_COMPROTOCOL )) != 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [COMLINLIF.csv] Information Get Error Line:%d", lineNo );
		commfunc_syslog_msgset_local( pv_own_name, errMsg );
		/* プロセス異常終了処理 */
		func_abnormal_exit( lineNo );	
		return( -1 );
	}

	/* バッテリー情報を取得 */
	if( (bttMaxCnt = commfunc_devInfo_get( PV_MKIND_BATTE, lineNo, s_bttTypeName, s_bttInfo, &requestCnt, bttId2Numb, bttId2Index, ps_g_BATTEINF )) <= 0 )
	{
		sprintf( errMsg, "DATA ERROR!! File [BATTEINF.csv] Not Exist Target Machine of Line:%d", lineNo );
		commfunc_syslog_msgset_local( pv_own_name, errMsg );
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

	}

	/* データ格納領域に欠測を設定 */
	for( loopCnt1 = 0; loopCnt1 < bttMaxCnt; loopCnt1++ )
	{
		for( loopCnt2 = 0; loopCnt2 < PV_MAX_BATTE_TMPOS; loopCnt2++ )
		{
			s_bttData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
		}
	}

	/* 収集周期の調整と移動平均算出用データ数の取得 */
	commfunc_serial_cycletmchk_local( &s_serial[0], requestCnt, ps_g_COMLIINF );

#if 0


	/* バッテリーの装置状態を復元 */
	for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{
		/* バッテリー番号の取得 */
		bttNumb = s_bttInfo[loopCnt1].devNumb;
		/* バッテリーの装置状態を復元する */
		commfunc_sv_get( PV_MKIND_BATTE, bttNumb, PV_MAX_BATTE_SVPOS, s_bttData[loopCnt1].bttStatus, ps_g_SVSTSFIL );

	}

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( lineNo, PV_FLG_STS_RESET );
#endif
	/* 当該通信回線に所属するバッテリーの台数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < bttMaxCnt ; loopCnt1++ )
	{

#ifdef PV_DEBUG
		printf("%s : PCS[%02d] OPEN-Thread EXECUTE\n", pv_own_name, s_bttInfo[loopCnt1].devNumb );
#endif
		/* 通信回線オープン（接続ポートオープン＆コネクト）用スレッドを起動 */
		s_threadArg[loopCnt1].stFlg = 1;
		if( pthread_create( &threadID[loopCnt1], NULL, func_bttOpen_thread, &s_threadArg[loopCnt1] ) != 0 )
		{

#ifdef PV_DEBUG
printf("%s : PCS[%02d] OPEN-Thread EXECUTE Error\n", pv_own_name, s_bttInfo[loopCnt1].devNumb );
#endif
			sprintf( errMsg, "Thread Execute Error" );
			commfunc_syslog_msgset_local( pv_own_name, errMsg );

			/* プロセス異常終了処理 */
			func_abnormal_exit( lineNo );
			return( -1 );
		}
	}

	sleep(1);

	// ★複数スレッドの終了について検討する必要がある。
	/* スレッドの終了を待つ */
	threadStop = 1;
	pthread_join( threadID[0], NULL );

	/* プロセス動作中フラグを解除 */
	//commfunc_manage_runFlagSet( lineNo, PV_FLG_STS_RESET );
	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	/* ミューテックス領域を生成していた場合 */
	if( mutexCreate )
	{
		/* ミューテックス領域を削除 */
		commfunc_mutex_destroy();
	}

	if( fpOut != NULL )
	{
		/* ファイルを閉じる */
		fclose( fpOut );
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 回線情報取得
 *
 * 機能：
 * 		引数 lineNo で指定された通信回線番号のシリアル設定情報を通信回線
 * 		情報ファイル（COMLIINF.dat）から取得し、引数 ps_serial で指定されたアド
 * 		レスへセットする。
 * 		引数 pname で指定された通信プロトコル名称と通信回線情報ファイルに
 * 		設定されている通信プロトコル名称が一致しない場合は異常とする。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_serial_infoget_local( char lineNo, struct st_serialinf *ps_serial, struct st_COMLIINF *ps_comlininf, char *pname)
{
											/* デバイスタイプ */
	char		devType[5][32] = { "Serial", "TCP485", "TCP/IP", "UDP/IP", "UDP/IP" };
	char		errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	/* 通信回線番号が不正の場合 */
	if( (lineNo < 1) || (lineNo > PV_MAX_COMLINE) )
	{
#if 0
		printf("commfunc_serial_infoget_local(): ERROR line number range (line:%d)\n", lineNo );
#endif
		return( -1 );
	}

	/* 通信回線が未実装の場合 */
	if( (ps_comlininf + lineNo - 1)->statFlag == 0 )
	{
#if 0
		printf("commfunc_serial_infoget_local(): ERROR non-exist line (line:%d)\n", lineNo );
#endif
		return( -1 );
	}

	/* 通信プロトコル名称が一致していない場合 */
	if( strncmp( (ps_comlininf + lineNo - 1)->protoName, pname, 12) != 0 )
	{
#if 0
		printf("commfunc_serial_infoget_local(): ERROR protocol different (line:%d)\n", lineNo);
#endif
		return( -1 );
	}

	/* シリアル設定情報を格納 */
	ps_serial->lineNo     = (int)lineNo;
	ps_serial->deviceType = (int)(ps_comlininf + lineNo - 1)->deviceType;
	strncpy(ps_serial->targetName, (ps_comlininf + lineNo - 1)->targetName, 20);
	ps_serial->portNumb   = (int)(ps_comlininf + lineNo - 1)->portNumb;
	ps_serial->baudRate   = (int)(ps_comlininf + lineNo - 1)->baudRate;
	ps_serial->dataSize   = (int)(ps_comlininf + lineNo - 1)->dataSize;
	ps_serial->parity     = (int)(ps_comlininf + lineNo - 1)->parity;
	ps_serial->stopBit    = (int)(ps_comlininf + lineNo - 1)->stopBit;
	ps_serial->rcvTime    = (int)(ps_comlininf + lineNo - 1)->rcvTime;
	ps_serial->sndTime    = (int)(ps_comlininf + lineNo - 1)->sndTime;
	ps_serial->cycleTime    = (int)(ps_comlininf + lineNo - 1)->cycleTime;

#ifdef PV_DEBUG
    printf("commfunc_serial_infoget_local() : SERIAL DEVICE INFO [SERIAL LINE:%d]\n", lineNo);
    printf("commfunc_serial_infoget_local() :   DeviceType : %d\n", ps_serial->deviceType);
    printf("commfunc_serial_infoget_local() :   DeviceName : %s\n", ps_serial->targetName );
    printf("commfunc_serial_infoget_local() :   Port       : %d\n", ps_serial->portNumb );
    printf("commfunc_serial_infoget_local() :   Baudrate   : %d\n", ps_serial->baudRate );
    printf("commfunc_serial_infoget_local() :   Datasize   : %d\n", ps_serial->dataSize );
    printf("commfunc_serial_infoget_local() :   Parity     : %d\n", ps_serial->parity );
    printf("commfunc_serial_infoget_local() :   Stopbit    : %d\n", ps_serial->stopBit );
    printf("commfunc_serial_infoget_local() :   RcvTime    : %d\n", ps_serial->rcvTime );
    printf("commfunc_serial_infoget_local() :   SndTime    : %d\n", ps_serial->sndTime );
    printf("commfunc_serial_infoget_local() :   CycleTime  : %d\n", ps_serial->cycleTime );
#endif

	sprintf( errMsg, "Line:%02d [ Type:%d(%s) Dev/Net:%s Port:%d Bps:%d Size:%d Pari:%d StopB:%d RcvTim:%dms SndTim:%dms CycleTim:%ds ]",
		ps_serial->lineNo,
		ps_serial->deviceType, devType[ps_serial->deviceType - 1],
		ps_serial->targetName, ps_serial->portNumb,
		ps_serial->baudRate, ps_serial->dataSize, ps_serial->parity, ps_serial->stopBit,
		ps_serial->rcvTime, ps_serial->sndTime, ps_serial->cycleTime ); 
	commfunc_syslog_msgset( "commfunc_serial_infoget_local()", errMsg );

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
 *		実際の接続処理は共通関数 commfunc_serial_open_local() を使用する。
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
	int						directPower;				/* 充放電電力 */
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
	int						workIdxNumb;			/* 処理インデックス番号 */
	struct st_threadArg		*ps_threadArg;			/* スレッド引数処理用 */
	struct timeval			nowTime;
	int						reqKind = 0;			/* 要求種別フラグ */
	unsigned char			standby;				/* スタンバイ設定 */
	struct timespec			sleepStanby;			/* スタンバイ時のスリープ時間 */
	struct st_bttTeslaSdInf	s_bttSdInf[512];			/* 送信情報ポインタ */

	ps_threadArg = (struct st_threadArg *)args;

	/* シリアル通信ログ格納領域を初期化 */
	memset( &s_serialLog, 0, sizeof(s_serialLog));

	/* 処理インデックス番号の取得 */
	workIdxNumb = ps_threadArg->indexNumb;

	/* パワコン番号の取得 */
	bttNumb = s_bttInfo[workIdxNumb].devNumb;
	/* バッテリー装置IDの取得 */
	idNumb  = s_bttInfo[workIdxNumb].idNumb ;

#ifdef PV_DEBUG
	printf("%s : IDX[%02d] PCS[%02d] OPEN-Thread START\n", pv_own_name, workIdxNumb, bttNumb );
#endif

	/* 送信待ちタイマを設定 */
	sleepTm.tv_sec  = s_serial[workIdxNumb].sndTime / 1000;
	sleepTm.tv_nsec = (s_serial[workIdxNumb].sndTime % 1000) * 1000000;

	/* スタンバイ時のスリープ時間 */
	sleepStanby.tv_sec  = 0;
	sleepStanby.tv_nsec = 100 * 1000000;

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( threadStop == 0 )
	{

		/* シリアルポートをオープン */
		openRetCode = commfunc_serial_open_local( &s_serial[workIdxNumb] );
		if( openRetCode >= 0 )
		{

			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 0;

		}
		else
		{
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
			commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST14_LANERR, 1,
			    (ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );

#ifdef PV_DEBUG
		printf("%s : IDX[%02d] PCS[%02d] OPEN-Thread OPEN ERROR!!\n", pv_own_name, workIdxNumb, bttNumb );
#endif
			/* シリアルポートステータスを更新 */
			s_bttData[workIdxNumb].serialStatus = 1;
			/* シリアルポート異常継続時に高速ループに陥るの回避 */
			sleep( 5 );
		}

		/* シリアルポートオープンが正常の場合 */
		if( openRetCode >= 0 )
		{
			/* 要求実施回数を初期化 */
			s_serial[workIdxNumb].requestCount = 0;

			ps_bttSdInf = s_bttSdInf;
			for( loopCnt2 = 0 ; s_reggInfo[loopCnt2].regAddr != 0 ; loopCnt2++ )
			{

				p_sendBuff = &sendBuff_r[0];
				ps_bttSdInf[loopCnt2].dataLength = 0x0C;
				ps_bttSdInf[loopCnt2].funcCode = PV_BTTTESLA1_FUNC_03;
				ps_bttSdInf[loopCnt2].startAdU = (s_reggInfo[loopCnt2].regAddr / 0x100);
				ps_bttSdInf[loopCnt2].startAdL = (s_reggInfo[loopCnt2].regAddr & 0xFF);
				ps_bttSdInf[loopCnt2].dataCntU = ((s_reggInfo[loopCnt2].dataSize/2) / 0x100);
				ps_bttSdInf[loopCnt2].dataCntL = ((s_reggInfo[loopCnt2].dataSize/2) & 0xFF);

				/* 送信待ち処理 */
				nanosleep( &sleepTm, NULL);

				/* シリアルポートオープン正常の場合 */
				if( s_serial[workIdxNumb].status == PV_STAT_COM_CONNECT )
				{
#ifdef PV_DEBUG_SERIAL
					printf("%s : SERIAL POERT OPENED\n", PV_OWN_NAME);
#endif
					/* 送信バッファにファンクションと取得開始アドレス、取得データ数を設定 */
					p_sendBuff[5] = ps_bttSdInf[loopCnt2].dataLength - 6;
					p_sendBuff[7] = ps_bttSdInf[loopCnt2].funcCode;
					p_sendBuff[8] = ps_bttSdInf[loopCnt2].startAdU;
					p_sendBuff[9] = ps_bttSdInf[loopCnt2].startAdL;
					p_sendBuff[10] = ps_bttSdInf[loopCnt2].dataCntU;
					p_sendBuff[11] = ps_bttSdInf[loopCnt2].dataCntL;
					p_sendBuff[12] = p_sendBuff[11] * 2;
					dataLength = ps_bttSdInf[loopCnt2].dataLength;

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
							commfunc_syslog_msgset_local( pv_own_name, errMsg );

							/* データ送信異常（送信不可異常）発生 */
							/* 状変（通信ステータス状変）発生 */
							commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST17_NOTSENDERR, 1,
								(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );

							/* 再要求不要のため処理を抜ける */
							break;
						}

						/* データ処理バッファをクリア */
						memset(readBuff, 0, sizeof(readBuff));
		
						/* ＴＣＰポートからデータを受信し、データ処理バッファに保存 */
						retCode = func_bttData_recv(	&s_serial[workIdxNumb], bttNumb, idNumb,
														&ps_bttSdInf[loopCnt2],
														p_sendBuff, readBuff, &s_serialLog );
						if( retCode == 0 )
						{
							/* 正常データ受信完了 */
							/* シリアル通信ログを出力 */
							commfunc_serial_logout_local( PV_SERIAL_LOG_AUTO, &s_serial[workIdxNumb], &s_serialLog );

							/* 再要求不要のため処理を抜ける */

							/* データ処理バッファから装置状態と計測値を抽出 */
							func_regData_get( loopCnt2, readBuff);
							break;
						}
						else
						{
							/* シリアル通信ログを出力 */
							commfunc_serial_logout_local( PV_SERIAL_LOG_ON, &s_serial[workIdxNumb], &s_serialLog );
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

			}
			break;
		}
	}

	/* シリアルポートをクローズする */
	commfunc_serial_close( &s_serial[workIdxNumb] );

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
	int bttNumb,
	int targetID,
	struct st_bttTeslaSdInf	*ps_bttSdInf,
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
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
					return( PV_SERIAL_ERRNOTREADY );
					break;
				case PV_SERIAL_ERRTIMEOUT :
					sprintf( errMsg, "Read Timeout! DataLen:%d Numb:%02d Id:%02d (%s)", totalLength, bttNumb, targetID, ps_serial->targetName);
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 要求実施回数を＋１ */
					ps_serial->requestCount += 1;
					/* 要求実施回数が上限に達している場合 */
					if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
					{
						/* 通信ステータス状変：通信不応答異常（発生） */
						commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST01_NONRESPONSE, 1,
							(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
					}
					return( PV_SERIAL_ERRTIMEOUT );
					break;
				case PV_SERIAL_ERRACCESS :
					sprintf( errMsg, "Read Error! Numb:%02d Id:%02d (%s)", bttNumb, targetID, ps_serial->targetName );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
					/* 通信ステータス状変：下り送信不可異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST17_NOTSENDERR, 1,
						(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
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
			/* ９バイト分のデータを受信したら先頭データのチェックを行う */
			if( dataLength == 9 )
			{
				/* トランザクションIDの確認（本装置では固定的に0x0000を使用） */
				if( ( *dataBuff != 0 ) || ( *(dataBuff + 1) != 0 ) )
				{
#ifdef PV_DEBUG_SERIAL
printf("<TRAN-ID ERROR>");
#endif
					sprintf( errMsg, "Data-Recv TransactionID Error! Numb:%02d Expect[00][00],Result[%02X][%02X]", bttNumb, *dataBuff, *(dataBuff+1) );
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
					commfunc_syslog_msgset_local( pv_own_name, errMsg );
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
				commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST19_DWCOMMANDERR, 1,
					(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが不正データアドレスの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x02 )
			{
				/* 通信ステータス状変：下りパラメータ異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST22_DWPARAMERR, 1,
					(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 不正データ値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x03 )
			{
				/* 通信ステータス状変：下りデータ異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST23_DWDATAERR, 1,
					(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが サーバ異常値の場合 */
			else if( *(dataBuff+8) == (unsigned char)0x04 )
			{
				/* 通信ステータス状変：デバイス異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST16_DEVICEERROR, 1,
					(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが ＢＵＳＹの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x05 )
			{
				/* 通常時、且つ、制御書込みの場合、ハートビートパターンを切り替える */
				if( (ps_bttSdInf->reqKind == 2 ) &&
				    (reqFunc == PV_BTTTESLA1_FUNC_10 ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST27_DWCONTROLERR, 1,
						(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
					/* ハートビートパターンインデックスを更新 */
					heartBeatIndex = ( heartBeatIndex + 1) % 2;
				}

				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", bttNumb, *(dataBuff+8));
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				return( PV_SERIAL_ERROTHER );
			}
			/* 例外コードが 応答待ちの場合 */
			else if( *(dataBuff+8) == (unsigned char)0x06 )
			{
				sprintf( errMsg, "Data-Recv ProtocolID Error! Numb:%02d exception code:0x%02x", bttNumb, *(dataBuff+8));
				commfunc_syslog_msgset_local( pv_own_name, errMsg );
				/* 要求実施回数を＋１ */
				ps_serial->requestCount += 1;
				/* 要求実施回数が上限に達している場合 */
				if( ps_serial->requestCount >= PV_SERIAL_REQUEST_MAX )
				{
					/* 通信ステータス状変：通信不応答異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST01_NONRESPONSE, 1,
						(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
				}
				/* タイムアウト */
				return( PV_SERIAL_ERRTIMEOUT );
			}
			else
			{
				/* 通信ステータス状変：下りフォーマット異常（発生） */
				commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST20_DWFORMATERR, 1,
					(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
				return( PV_SERIAL_ERROTHER );
			}
		}
		else
		{
			/* ファンクションコードが１０の場合 */
			if( reqFunc == PV_BTTTESLA1_FUNC_10 )
			{
				/* 開始アドレスとレジスタ数が異なる場合 */
				if( (*(dataBuff+8)  != sendBuff[8]  ) ||
				    (*(dataBuff+9)  != sendBuff[9]  ) ||
				    (*(dataBuff+10) != sendBuff[10] ) ||
				    (*(dataBuff+11) != sendBuff[11] ) )
				{
					/* 通信ステータス状変：下り制御異常（発生） */
					commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST27_DWCONTROLERR, 1,
						(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
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
			commfunc_johen_stat_local( PV_MKIND_BATTE, bttNumb, PV_ST01_NONRESPONSE, 1,
				(ps_g_BATTEINF+bttNumb-1)->stStLink, ps_g_STSTSFIL );
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
		}

		return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ポートオープン
 *
 * 機能：
 * 		ps_serialで指定された情報に基づき、ポートをオープンする。
 * 		ps_serialのdeviceTypeにPV_STYPE_TTYが設定されている場合は、ハードウェア
 * 		内のシリアルポート（ＵＳＢの場合もある）をオープンする。
 * 		PV_STYPE_TCPまたはPV_STYPE_DIRTCPが設定されている場合は、ネットワークポ
 *		ートのオープンとTCPによる接続を行う。
 *		PV_STYPE_UDPDIRまたはPV_STYPE_UDPOTHが設定されている場合は、ネットワーク
 *		のポートオープンを行う。
 *		ps_serialのボーレートなどの設定について、
 * 		PV_STYPE_TTYの場合は、シリアルポートオープン成功した場合は、ps_serialの
 * 		設定情報に基づき通信速度や通信条件をポートに対して設定する。
 * 		PV_STYPE_TCPの場合は、接続先の変換器で通信速度等が設定されているため、参
 *		考程度の情報として扱う。
 *		PV_STYPE_DIRTCP、PV_STYPE_UDPDIR、PV_STYPE_UDPOTHの場合は、相手先装置と
 *		の直接通信となるため使用しない。
 * 戻り値：
 *		 1：正常終了（オープン実施正常）
 * 		 0：正常終了（オープン済み）
 * 		-1：異常終了
 */
int	commfunc_serial_open_local( struct st_serialinf *ps_serial )
{
	int						serialSet;					/* シリアル設定 */
	int						sockOptVal = 1;				/* ソケットオプション設定値 */
	int						retCode;					/* 戻り値 */
	struct termios			termio;						/* シリアルポート設定用 */
	struct sockaddr_in		dstAddr;					/* TCP/UDP接続用 */
	struct sockaddr_in		chkAddr;					/* ローカルポートIPアドレス取得用 */
	int						chkLength;					/* データサイズ */
	char					errMsg[PV_MSG_SIZE_LOG];

	/* シリアルポートはオープン済み？ */
	if( ps_serial->status != PV_STAT_COM_CLOSE )
	{
		return( 0 );
	}

	/* USB-485変換器フラグの初期化 */
	ps_serial->ttyUSBFlag = 0;

	/* デバイスタイプはシリアルポート（EIA232経由）？ */
	if( ps_serial->deviceType == PV_STYPE_TTY )
	{
		/* シリアルポートが実在するか？ */
		if( (retCode = access( ps_serial->targetName, F_OK )) != 0 )
		{
			sprintf( errMsg, "ERROR Serial Device Not Found (%s) Line(%02d) Errno(%d)", ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* シリアルポートオープン */
		ps_serial->fd_sockfd = open(ps_serial->targetName, O_RDWR | O_NOCTTY);
		if( ps_serial->fd_sockfd == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR Serial Device Open Error (%s) Line(%02d) Errno(%d)", ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		sprintf( errMsg, "OPEN SerialType(Direct:EIA232->EIA485:%s) Line(%02d)", ps_serial->targetName, ps_serial->lineNo );
		commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );

		/* ステータスにオープンを設定	*/
		ps_serial->status = PV_STAT_COM_OPEN;

		/* シリアルポートの設定 */
		termio.c_iflag = 0; /* 入力モードの設定 */
		termio.c_oflag = 0; /* 出力モードの設定 */
		termio.c_cflag = 0; /* 制御モードの設定 */
		termio.c_lflag = 0; /* ローカルモードの設定初期化 */ 
	
		/* 入力モードの設定 */
		/* （IGNPAR : パリティチェックを無効） */
		termio.c_iflag = IGNPAR ; 
		/* 出力モードの設定 */
		/* （RAWモードで出力を行う） */
		termio.c_oflag = 0;
		/* 制御モードの設定 */
		termio.c_cflag = 0 ;
		/* （CLOCAL : モデムの制御線を無視） */
		termio.c_cflag |= CLOCAL ;
		/* （CREAD  : 受信を有効に設定） */
		termio.c_cflag |= CREAD ;
		/* 通信速度の設定 */
		switch(ps_serial->baudRate)
		{
			case 2400:
					termio.c_cflag |= B2400;
					break;
			case 4800:
					termio.c_cflag |= B4800;
					break;
			case 9600:
					termio.c_cflag |= B9600;
					break;
			case 19200:
					termio.c_cflag |= B19200;
					break;
			case 38400:
					termio.c_cflag |= B38400;
					break;
			case 57600:
					termio.c_cflag |= B57600;
					break;
			case 115200:
					termio.c_cflag |= B115200;
					break;
			default:
					termio.c_cflag |= B9600;
					break;
		}
		/* データサイズの設定 */
		switch(ps_serial->dataSize)
		{
			case 7:
					termio.c_cflag |= CS7;
					break;
			case 8:
					termio.c_cflag |= CS8;
					break;
			default:
					termio.c_cflag |= CS8;
					break;
		}
		/* ストップビットの設定 */
		switch(ps_serial->stopBit)
		{
			case 2:
					/* CSTOPB : ストップビットを1->2に変更 */
					termio.c_cflag |= CSTOPB;
					break;
			default:
					break;
		}
		/* パリティの設定 */
		switch(ps_serial->parity)
		{
			case 0:
					/* パリティ設定なし */
					break;
			case 1:
					/* 奇数パリティを設定 */
					termio.c_cflag |= (PARENB | PARODD);
					break;
			case 2:
					/* 偶数パリティを設定 */
					termio.c_cflag |= PARENB;
					break;
			default:
					break;
		}
		/* ローカルモードの設定 */
		termio.c_lflag = 0;
		/* キャラクタ間タイマを使わない */
		termio.c_cc[VTIME] = 0;
		/* 読み込みブロックを行わない */
		termio.c_cc[VMIN] = 0;
	
		/* シリアルポートの設定を反映する */
		tcsetattr( ps_serial->fd_sockfd, TCSANOW,  &termio );

		/* ステータスに通信可を設定	*/
		ps_serial->status = PV_STAT_COM_CONNECT;

		/* ttyタイプが /dev/ttyUSB* の場合 */
		if( strstr( ps_serial->targetName, "ttyUSB" ) != NULL )
		{
			/* ttyUSBフラグを設定 */
			ps_serial->ttyUSBFlag = 1;

			/* DTR信号を ON  に設定し、受信用トランシーバを有効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_DTR;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIS, &serialSet );
			if( retCode != 0 ) printf("ioctl Error Ret(%d) Errno(%d)\n", retCode, errno);

			/* RTS信号を OFF に設定し、送信用トランシーバを無効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_RTS;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIC, &serialSet );
			if( retCode != 0 ) printf("ioctl Error Ret(%d) Errno(%d)\n", retCode, errno);

		}
	}
	/* デバイスタイプはＬＡＮ－４８５変換器タイプまたは直接ＴＣＰ接続タイプ？ */
	else if( ( ps_serial->deviceType == PV_STYPE_TCP ) || ( ps_serial->deviceType == PV_STYPE_DIRTCP ) )
	{
		/* ソケットオープン */
		/* （IPv4、ストリーム） */
		ps_serial->fd_sockfd = socket( AF_INET, SOCK_STREAM, 0 );
		if( ps_serial->fd_sockfd == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR TCP Socket Error (%s) Line(%02d) Errno(%d)",
				ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* ソケットオプションの設定（KEEPALIVEによる接続監視） */
		if( setsockopt( ps_serial->fd_sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&sockOptVal, sizeof(sockOptVal) ) != 0 )
		{
			sprintf( errMsg, "ERROR TCP Socket Option Setting Error (%s) Line(%02d) Errno(%d)",
				ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
		}

		/* ステータスにオープンを設定	*/
		ps_serial->status = PV_STAT_COM_OPEN;

		/* アドレス情報を初期化 */
		memset( &dstAddr, 0, sizeof( dstAddr ) );
		/* ネットワークファミリの設定 */
		dstAddr.sin_family = AF_INET;
		/* アクセスポートの設定 */
		dstAddr.sin_port   = htons( ps_serial->portNumb );
		/* IPアドレスの設定 */
		if( inet_aton( ps_serial->targetName, &(dstAddr.sin_addr)) == 0 )
		{
			/* IPアドレスが無効な場合 */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;

			sprintf( errMsg, "ERROR TCP Device IP-Addr Error (%s:%d) Line(%02d)",
				ps_serial->targetName, ps_serial->portNumb, ps_serial->lineNo );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* シリアル変換機器（変換サーバ）または通信対象装置に接続 */
		if( connect( ps_serial->fd_sockfd, (struct sockaddr *)&dstAddr, sizeof( dstAddr ) ) == -1 )
		{
			/* 接続エラー */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR TCP Device CONNECT Error (%s:%d) Line(%02d) Errno(%d)",
				ps_serial->targetName, ps_serial->portNumb, ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* ローカルポートに関する情報の取得 */
		chkLength = sizeof( chkAddr );
		memset( &chkAddr, 0, chkLength );
		if( getsockname( ps_serial->fd_sockfd, (struct sockaddr *)&chkAddr, &chkLength ) == -1 )
		{
			/* パラメータ取得エラー */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR TCP GETSOCKNAME Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* 相手先装置のIPアドレス（ネットワークバイトオーダー）を格納 */
		ps_serial->dstIPAddr = dstAddr.sin_addr.s_addr;
		/* ローカルポートのIPアドレス（ネットワークバイトオーダー）を格納 */
		ps_serial->locIPAddr = chkAddr.sin_addr.s_addr;
		/* ローカルポート番号（ホストバイトオーダー）を格納 */
		ps_serial->portNumbLoc = ntohs(chkAddr.sin_port);

		if( ps_serial->log_open == 0 )
		{
			sprintf( errMsg, "OPEN SerialType(TCP:Etherner:%d->%s:%d) Line(%02d)",
				ps_serial->portNumbLoc, ps_serial->targetName, ps_serial->portNumb, ps_serial->lineNo );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
		}

		/* ステータスに通信可を設定	*/
		ps_serial->status = PV_STAT_COM_CONNECT;
	}
	/* デバイスタイプはＵＤＰ接続タイプ？ */
	else if( ( ps_serial->deviceType == PV_STYPE_UDPDIR ) || ( ps_serial->deviceType == PV_STYPE_UDPOTH ) )
	{
		/* ソケットオープン（送信先ポート）（IPv4、データグラム） */
		ps_serial->fd_sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
		if( ps_serial->fd_sockfd == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR UDP(SEND) Socket Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* 送信用ポートの設定 */
		memset( &dstAddr, 0, sizeof( dstAddr ) );
		/* ネットワークファミリの設定 */
		dstAddr.sin_family      = AF_INET;
		/* ポート番号の指定（自動割当を使用するため0を指定） */
		dstAddr.sin_port        = 0;
		/* インターフェイスの指定（自動選択のためINADDR_ANYを指定） */
		dstAddr.sin_addr.s_addr = INADDR_ANY;

		/* 送信用ポートをバインド */
		if( bind( ps_serial->fd_sockfd, (struct sockaddr *)&dstAddr, sizeof(dstAddr) ) == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR UDP(SEND) BIND Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}

		/* ローカルポートに関する情報の取得 */
		chkLength = sizeof( chkAddr );
		memset( &chkAddr, 0, chkLength );
		if( getsockname( ps_serial->fd_sockfd, (struct sockaddr *)&chkAddr, &chkLength ) == -1 )
		{
			/* パラメータ取得エラー */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR UDP(SEND) GETSOCKNAME Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
			return( -1 );
		}
		/* ローカルポートのIPアドレス（ネットワークバイトオーダー）を格納 */
		ps_serial->locIPAddr = chkAddr.sin_addr.s_addr;
		/* ローカルポート番号（ホストバイトオーダー）を格納 */
		ps_serial->portNumbLoc = ntohs(chkAddr.sin_port);

		/* ＵＤＰアクセスが送信元とは異なるポートに返送するタイプの場合 */
		if( ps_serial->deviceType == PV_STYPE_UDPOTH )
		{
			/* 受信専用ソケットオープン（送信先ポート）（IPv4、データグラム） */
			ps_serial->fd_sockfdUdp = socket( AF_INET, SOCK_DGRAM, 0 );
			if( ps_serial->fd_sockfdUdp == -1 )
			{
				/* 送信用ソケットをクローズ */
				close( ps_serial->fd_sockfd );
				/* ステータスに未オープンを設定	*/
				ps_serial->status = PV_STAT_COM_CLOSE;
				sprintf( errMsg, "ERROR UDP(RCV) Socket Error (%s) Line(%02d) Errno(%d)",
					ps_serial->targetName, ps_serial->lineNo, errno );
				commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
				return( -1 );
			}

			/* 受信専用ポートの設定 */
			memset( &dstAddr, 0, sizeof( dstAddr ) );
			/* ネットワークファミリの設定 */
			dstAddr.sin_family      = AF_INET;
			/* ポート番号の指定（受信ポート番号を設定） */
			dstAddr.sin_port        = htons( ps_serial->baudRate );
			/* インターフェイスの指定（自動選択のためINADDR_ANYを指定） */
			dstAddr.sin_addr.s_addr = INADDR_ANY;

			/* 受信専用ポートをバインド */
			if( bind( ps_serial->fd_sockfdUdp, (struct sockaddr *)&dstAddr, sizeof(dstAddr) ) == -1 )
			{
				/* 送信用ソケットをクローズ */
				close( ps_serial->fd_sockfd );
				/* 受信用ソケットをクローズ */
				close( ps_serial->fd_sockfdUdp );
				/* ステータスに未オープンを設定	*/
				ps_serial->status = PV_STAT_COM_CLOSE;
				sprintf( errMsg, "ERROR UDP(RECV) BIND Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
				commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
				return( -1 );
			}
		}

		/* ステータスに通信可を設定	*/
		ps_serial->status = PV_STAT_COM_CONNECT;

		/* ＵＤＰアクセスが送信元ポートに返送するタイプの場合 */
		if( ps_serial->deviceType == PV_STYPE_UDPDIR )
		{
			sprintf( errMsg, "OPEN SerialType(UDP:Etherner:%d->DST:%d) Line(%02d)",
				ps_serial->portNumbLoc, ps_serial->portNumb, ps_serial->lineNo );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
		}
		else
		{
			sprintf( errMsg, "OPEN SerialType(UDP:Etherner:%d->DST:%d) (RECV:Ethernet:%d) Line(%02d)",
				ps_serial->portNumbLoc, ps_serial->portNumb, ps_serial->baudRate, ps_serial->lineNo );
			commfunc_syslog_msgset_local( "commfunc_serial_open_local()", errMsg );
		}
	}
	else
	{
		/* ステータスに未オープンを設定	*/
		ps_serial->status = PV_STAT_COM_CLOSE;
		return( -1 );
	}

	return( 1 );
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
	char					kindName[6][5] = { "SEND", "RECV", "SETD", "ERRO", "TOUT", "OVER" };

#if 0
	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_serial_logout_local(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ログ出力を行う必要があるか？ */
	if( logMode == PV_SERIAL_LOG_AUTO )
	{
		/* ログ出力フラグが設定されている場合 */
		if( ps_gpt_PVSYSINF->lineLogFlag[ ps_serialinf->lineNo - 1 ] != 0 )
		{
			/* ログ出力対象に設定 */
			logMode = PV_SERIAL_LOG_ON;
		}
	}

	/* ログ出力が必要な場合 */
	if( logMode == PV_SERIAL_LOG_ON )
	{

		/* 出力用ログファイル名の設定 */
		sprintf( fileName1, "%s%s%02d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );

		/* ファイルが存在しない場合 */
		if( access( fileName1, F_OK ) != 0 )
		{
			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
			{
				return( -1 );
			} 
			close( fd );
		}

		/* システムログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			return( -1 );
		}

		/* ファイルサイズの取得 */
		fileSize = ftell( fp );

		/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
		if(fileSize > 1000000 )
		{ 
			/* ファイルを閉じる */
			fclose( fp );

			/* ログファイルの保存数分繰り返す */
			for(loopCnt1 = 3 ; loopCnt1 > 1 ; loopCnt1-- )
			{
				/* ファイル名の設定 */
				sprintf( fileName1, "%s%s%02d_%d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo, loopCnt1 - 1 );
				sprintf( fileName2, "%s%s%02d_%d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo, loopCnt1 );
				/* 古いファイルの削除 */
				if( access( fileName2, F_OK ) == 0 )
					unlink( fileName2 );
				/* ローテーションの実施 */
				if( access( fileName1, F_OK ) == 0 )
					rename( fileName1, fileName2 );
			}
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s%02d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );
			sprintf( fileName2, "%s%s%02d_1.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			rename( fileName1, fileName2 );

			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH ) ) == -1 )
			{
				return( -1 );
			} 
			close( fd );

			/* システムログファイルをオープン */
			if( (fp = fopen( fileName1, "a" )) == NULL )
			{
				return( -1 );
			}
		}

		/* シリアルログメッセージ出力処理 */
		for( loopCnt1 = 0 ; loopCnt1 < ps_serialLog->dataCnt ; loopCnt1++ )
		{
			/* 年月日時分秒を取得 */
			localtime_r( &ps_serialLog->s_logData[loopCnt1].sec, &localTime );
			fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s[%3d] ",
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
				fprintf( fp, "(%d.%d.%d.%d:%d)-(%d.%d.%d.%d:%d)",
					*(pt1), *(pt1 + 1), *(pt1 + 2), *(pt1 + 3), ps_serialLog->s_logData[loopCnt1].locPort,
					*(pt2), *(pt2 + 1), *(pt2 + 2), *(pt2 + 3), ps_serialLog->s_logData[loopCnt1].dstPort );

				for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
				{
					if( ( loopCnt2 % 16 ) == 0 )
						fprintf( fp, "\n  %03Xh(%04d): ", loopCnt2, loopCnt2);

					fprintf( fp, "%02X ", ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}

				fprintf( fp, "\n");
			}
			else
			{
				for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
				{
					fprintf( fp, "%02X ", ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}
				fprintf( fp, "\n");
			}
		}

		/* ファイルを閉じる */
		fclose( fp );

		/* カウンタをリセット */
		ps_serialLog->dataCnt = 0;
	}
#else
		/* シリアルログメッセージ出力処理 */
		for( loopCnt1 = 0 ; loopCnt1 < ps_serialLog->dataCnt ; loopCnt1++ )
		{
			/* 年月日時分秒を取得 */
			localtime_r( &ps_serialLog->s_logData[loopCnt1].sec, &localTime );
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

					printf("%02X ", ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}

				printf("\n");
			}
			else
			{
				for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
				{
					printf("%02X ", ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}
				printf("\n");
			}
		}
#endif
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
	localtime_r( &nowtime.tv_sec, &localTime );

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
	struct st_COMLIINF *ps_COMLIINF			/* 通信回線情報ファイルポインタ */
)
{
	int			oldTime = 0;				/* 変更前タイマ */
	int			workTime = 0;				/* ワーク */
	char		errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	/* 変更前タイマを取得 */
	oldTime = ps_serial->cycleTime;

	/* 収集周期を本装置で処理可能な周期に調整 */
	/* 設定可能な値は 10000,6000,5000,4000,3000,2000,1000, 秒 */
	if( oldTime > 6000 )
		ps_serial->cycleTime = 10000;
	else if( oldTime > 5000 )
		ps_serial->cycleTime = 6000;
	else if( oldTime > 4000 )
		ps_serial->cycleTime = 5000;
	else if( oldTime > 3000 )
		ps_serial->cycleTime = 4000;
	else if( oldTime > 2000 )
		ps_serial->cycleTime = 3000;
	else if( oldTime > 1000 )
		ps_serial->cycleTime = 2000;
	else if( oldTime > 500 )
		ps_serial->cycleTime = 1000;
	else if( oldTime > 250 )
		ps_serial->cycleTime = 500;
	else if( oldTime > 200 )
		ps_serial->cycleTime = 250;
	else
		ps_serial->cycleTime = 100;

	sprintf( errMsg, "CycleTim Adjust [ Line:%02d CycleTim:%d->%ds]",
		ps_serial->lineNo, oldTime, ps_serial->cycleTime);
	commfunc_syslog_msgset_local( "commfunc_syslog_msgset_local()", errMsg );

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
 * 			int soNumb,
 * 			int stNumb,
 * 			int svStat,
 * 			int ststLink,
 * 			struct st_STSTSFIL *ps_STSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、stNumb（状態番号）、stStat
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
 * 		soNumb
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
	int soNumb,									/* 装置番号 */
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
			printf("通信不応答異常");
			break;
			/* 通信ステータス状変：下り制御異常（発生） */
		case PV_ST27_DWCONTROLERR :
			printf("下り制御異常");
			break;
			/* 通信ステータス状変：下りフォーマット異常（発生） */
		case PV_ST20_DWFORMATERR :
			printf("下りフォーマット異常");
			break;
			/* 通信ステータス状変：デバイス異常（発生） */
		case PV_ST16_DEVICEERROR :
			printf("デバイス異常");
			break;
			/* 通信ステータス状変：下りデータ異常（発生） */
		case PV_ST23_DWDATAERR :
			printf("下りデータ異常");
			break;
			/* 通信ステータス状変：下りパラメータ異常（発生） */
		case PV_ST22_DWPARAMERR :
			printf("下りパラメータ異常");
			break;
			/* 通信ステータス状変：下りコマンド異常（発生） */
		case PV_ST19_DWCOMMANDERR :
			printf("下りコマンド異常");
			break;
			/* 通信ステータス状変：下り送信不可異常（発生） */
		case PV_ST17_NOTSENDERR :
			printf("下り送信不可異常");
			break;
			/* 通信ステータス状変：ＬＡＮ接続異常（発生） */
		case PV_ST14_LANERR :
			printf("ＬＡＮ接続異常");
			break;
	}

	if( stStat == 1 )
		printf("（発生）\n");
	else
		printf("（復帰）\n");
	

	return( 0 );
}



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 夏時間情報読み込み
 *
 * 機能：
 * 		夏時間情報を読み込み、更新する
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
	int						dataCount = 0;					/* データ数 */

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
			sepCount = func_sep_data( readBuff, &s_sepStr );

			/* データ数が0の場合はスキップ */
			if( sepCount == 0 ) continue;

			/* データ数が２より少ない場合 */
			if( sepCount < 3 )
			{
				sprintf( errMsg, "File Format Error (Data is less than 3). line[%d] DATACNT[%d]", syoriCount, sepCount );
				commfunc_syslog_msgset_local( PV_OWN_NAME, errMsg );
				retCode = -1;
				break;
			}

			s_reggInfo[dataCount].regAddr = atoi( s_sepStr.data[0] );

			s_reggInfo[dataCount].dataSize = atoi( s_sepStr.data[1] );

			memcpy( s_reggInfo[dataCount].regType, s_sepStr.data[2], 32 );

			if( sepCount == 4 )
			{
				memcpy( s_reggInfo[dataCount].regName, s_sepStr.data[3], 512 );
			}

			/* データ数をカウント */
			dataCount++;
		}
		/* CSVファイルをクローズ */
		fclose( fp );

	}
	else
	{
		sprintf( errMsg, "File[%s] Open error", filePath );
		commfunc_syslog_msgset_local( PV_OWN_NAME, errMsg );
		retCode = -1;
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
int	func_regData_get( int reqCnt, unsigned char *dataBuff)
{
	union {											/* ビッグエンディアン－リトルエンディアン変換 */
		unsigned char			dataByte[16];			/* データ格納用領域 */
		short int				data16bit;				/* 16bitデータ取得用 */
		unsigned short int		data16bitu;				/* 16bitデータ取得用（符号無し） */				
		unsigned long long int	data64bitu;			/* 64bitデータ取得用（符号無し） */				
		int						data32bit;				/* 32bitデータ取得用 */
		float					data32f;				/* 32bitデータ取得用 */
	} u_change;
	int				loopCnt2;						/* ループカウンタ */
	int		tmVal = 0;
	char						outputStr[128];

	/* 変換用領域の初期化 */
	memset( &u_change, 0, sizeof(u_change));

	/* レジスタタイプ毎の処理 */
	if( strncmp( s_reggInfo[reqCnt].regType, "string", 32) == 0 )
	{
		sprintf(outputStr, "%s", &dataBuff[9]);
#ifdef PV_DEBUG_DT1
	printf("%s : Addr[0x%04x]:Val[%s]:NAME[%s]\n",
		PV_OWN_NAME, s_reggInfo[reqCnt].regAddr, tmVal, s_reggInfo[reqCnt].regName );
#endif
		if( fpOut != NULL )
		{
			fprintf( fpOut, "0x%04x(%04d):%-48s(%8s):%s\n", s_reggInfo[reqCnt].regAddr, s_reggInfo[reqCnt].regAddr, s_reggInfo[reqCnt].regName, s_reggInfo[reqCnt].regType, outputStr);
		}
	}
	else
	{
		/* 計測値の取得（ビッグエンディアンからリトルエンディアンへの配置変更） */
		for( loopCnt2 = 1 ; loopCnt2 <= s_reggInfo[reqCnt].dataSize ; loopCnt2++ )
			u_change.dataByte[loopCnt2 - 1] = dataBuff[9 + s_reggInfo[reqCnt].dataSize - loopCnt2];

		/* 計測値のサイズが２バイトサイズの場合 */
		if( strncmp( s_reggInfo[reqCnt].regType, "int16", 32) == 0 )
		{
			/* 符号付き16bitサイズのデータとして計測値を取得 */
			sprintf(outputStr, "0x%04x(%d)", u_change.data16bit, u_change.data16bit );
		}
		/* 計測値のサイズが４バイトサイズの場合 */
		else if( strncmp( s_reggInfo[reqCnt].regType, "int32", 32) == 0 )
		{
			/* 32bitサイズのデータとして計測値を取得 */
			sprintf(outputStr, "0x%08x(%d)", u_change.data32bit, u_change.data32bit );
		}
		else if( strncmp( s_reggInfo[reqCnt].regType, "uint16", 32) == 0 )
		{
			/* 符号なし16bitサイズのデータとして計測値を取得 */
			sprintf(outputStr, "0x%04x(%d)", u_change.data16bitu, u_change.data16bitu );
		}
		else if( strncmp( s_reggInfo[reqCnt].regType, "uint64", 32) == 0 )
		{
			/* 符号なし64bitサイズのデータとして計測値を取得 */
			sprintf(outputStr, "0x%llx(%llu)", u_change.data64bitu, u_change.data64bitu );
		}
		else
		{
			/* 暫定的に16bitサイズのデータとして計測値を取得 */
			sprintf(outputStr, "0x%04x(%d)", u_change.data16bit, u_change.data16bit );
		}

#ifdef PV_DEBUG_DT1
	printf("%s : Addr[0x%04x]:Val[%s]:NAME[%s]\n",
		PV_OWN_NAME, s_reggInfo[reqCnt].regAddr, outputStr, s_reggInfo[reqCnt].regName );
#endif

		if( fpOut != NULL )
		{
			fprintf( fpOut, "0x%04x(%04d):%-48s(%8s):%s\n", s_reggInfo[reqCnt].regAddr, s_reggInfo[reqCnt].regAddr, s_reggInfo[reqCnt].regName, s_reggInfo[reqCnt].regType, outputStr);
		}
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
int func_sep_data( char *pc_data, struct st_sepStr *ps_sepStr )
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


