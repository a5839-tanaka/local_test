/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * [20260514] 2026.05.14
 * ・一次調整力の系統周波数を平均値を算出し使用するように修正
 * [20251215] 2025.12.15 機能追加
 * ・CSVデータ数の文字列を修正
 * [20251212] 2025.12.12 機能追加
 * ・一次調整力パラメータ設定に充電応動有無を追加
 * [20250930] 2025.09.30
 * ・一次調整力レポートデータのデータ番号範囲チェックの誤りを修正
 * [20250909] 2025.09.09
 * ・充放電計画に一次使用有無を追加
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 *   関連ファイルを追加
 *   需給計画値情報ファイル修正
 * [20240220] 2024.02.20 Ph4対応
 * ・RCVPWINFにSoC不感帯を追加
 * ・CEMSPINFに需給調整市場、容量市場の優先度を追加
 * [20230905] 2023.09.05
 * ・設定変更ファイル、遠隔制御ファイルの内容をログファイルに出力するように修正
 * [20230830] 2023.08.30
 * ・充電可能電力、放電可能電力を動的に反映できるように修正
 * [20230301] 2023.03.01 仕様変更
 * ・BT出力変化量制御の有効/無効を廃止(常に有効)し、変化量を充電/放電方向に分ける
 * [20230220] 2023.02.20 仕様変更
 * ・制御不感帯を%->Wに変更
 * [20230125] 2023.01.25
 * ・需給計画設定のSVを追加
 * [20220909] 2022.09.09 不具合
 * ・CEMSPINFのバイオガス制御の有効/無効、蓄電池制御の有効/無効の順序が逆になっている
 * [20210128] 2021.01.28
 * ・受電電力計画値機能追加
 * [20210115] 2021.01.15
 * ・放電再開SoC、充電再開SoCを絶対値ではなく、相対値に変更
 * [20210112] 2020.01.12
 * ・外部ファイルの読み込み共通関数を追加
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 共通関数：ファイルアクセス関数
 *
 * 機能：
 * 		以下の機能を提供する。
 *		・収集装置で使用するファイルをＭＭＡＰ化する。
 *		・ミューテックスを管理する領域を共通メモリ上に生成し、ミューテックスを
 *		　初期化する。
 */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PV_LIB_COMM_FILE

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#if 0
#define	PV_DEBUG 1
#endif

struct st_cmn_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};


/*
 * ＭＭＡＰ定義テーブル
 * 収集装置で使用するオンラインファイルを全て定義する
 * （構造体の配置は必ずファイル番号に対応した位置に定義すること）
 */
struct st_mmapInfo {
	char			fileName[1024];		/* ファイル名 */
	int				fileSize;			/* ファイルサイズ（ＭＭＡＰのメモリ確保に使用） */
	int				writeEnable;		/* 書き込む場合に１、読込み専用の場合は０ */
} s_mmapInfo[] = {
	/****** [ 01 - 20 ] 基本情報ファイル ******/
	{	PV_FNAME_PVSYSINF,	sizeof(struct st_PVSYSINF)								, 1 },	/* 01:システム管理ファイル */
	{	PV_FNAME_COMLIINF,	sizeof(struct st_COMLIINF) * PV_MAX_COMLINE				, 1 },	/* 02:通信回線情報ファイル */
	{	PV_FNAME_TMDATINF,	sizeof(struct st_TMDATINF) * PV_MAX_TMDATA				, 0 },	/* 03:計測情報ファイル */
	{	PV_FNAME_SVDATINF,	sizeof(struct st_SVDATINF) * PV_MAX_SVDATA				, 0 },	/* 04:装置状態情報ファイル */
	{	PV_FNAME_STDATINF,	sizeof(struct st_STDATINF) * PV_MAX_STDATA				, 0 },	/* 05:通信ステータス情報ファイル */
	{	PV_FNAME_SVMSKINF,	sizeof(struct st_SVMSKINF) * ((PV_MAX_SVDATA-1)/8+1)	, 0 },	/* 06:装置状態状変マスク情報ファイル */
	{	PV_FNAME_PVSVRINF,	sizeof(struct st_PVSVRINF)								, 0 },	/* 07:モニタリングサーバ情報ファイル */
	{	PV_FNAME_TMRECINF,	sizeof(struct st_TMRECINF) * PV_MAX_TM_HISTORY			, 0 },	/* 08:計測履歴情報ファイル */
	{	PV_FNAME_SVRECINF,	sizeof(struct st_SVRECINF) * PV_MAX_SV_HISTORY			, 0 },	/* 09:状態履歴情報ファイル */
	{	PV_FNAME_TMAGGINF,	sizeof(struct st_TMAGGINF) * PV_MAX_PVSYS_AGGTMPOS		, 0 },	/* 10:集約計測情報ファイル */
	{	PV_FNAME_SVAGGINF,	sizeof(struct st_SVAGGINF) * PV_MAX_PVSYS_AGGSVPOS		, 0 },	/* 11:集約状態情報ファイル */
	{	PV_FNAME_TMMONINF,	sizeof(struct st_TMMONINF) * PV_MAX_PVSYS_MONSVPOS		, 0 },	/* 12:計測監視情報 */
	{	PV_FNAME_CEMSPINF,	sizeof(struct st_CEMSPINF)								, 1 },	/* 13:ＣＥＭＳ制御 */
	{	PV_FNAME_DEMNDINF,	sizeof(struct st_DEMNDINF)								, 1 },	/* 14:デマンド監視情報 */
	{	PV_FNAME_RCVPWINF,	sizeof(struct st_RCVPWINF)								, 1 },	/* 15:受電電力一定制御情報 */
	{	PV_FNAME_OVRCHINF,	sizeof(struct st_OVRCHINF)								, 1 },	/* 16:過充電回避制御 */
	{	PV_FNAME_MDCTLINF,	sizeof(struct st_MDCTLINF) * PV_CTL_MAX					, 0 },	/* 17:モード切替時制御 */
	{	PV_FNAME_PWPLNINF,	sizeof(struct st_PWPLNINF) * PV_POWPLAN_IDX_MAX			, 1	},	/* 18 *//* [20210128] Add */
	{	PV_FNAME_IOTCNINF,	sizeof(struct st_IOTCNINF) 								, 0 },	/* 19:IOT設定情報 [20230529] Add */
	{	PV_FNAME_PGCONINF,	sizeof(struct st_PGCONINF) * PV_SCH_TYPE_MAX			, 0 },	/* 20:電力会社向け情報 [20230529] Add */
	/****** [ 21 - 30 ] 対向装置関連情報ファイル ******/
	{	PV_FNAME_BATTEINF,	sizeof(struct st_BATTEINF) * PV_MAX_BATTE				, 1	},	/* 21 *//* [20230830] Chg */
	{	PV_FNAME_POWCNINF,	sizeof(struct st_POWCNINF) * PV_MAX_POWCN				, 0	},	/* 22 */
	{	PV_FNAME_MLTMTINF,	sizeof(struct st_MLTMTINF) * PV_MAX_MLTMT				, 0	},	/* 23 */
	{	PV_FNAME_REMIOINF,	sizeof(struct st_REMIOINF) * PV_MAX_REMIO				, 0	},	/* 24 */
	{	PV_FNAME_PLCFAINF,	sizeof(struct st_PLCFAINF) * PV_MAX_PLCFA				, 0	},	/* 25 */
	{	"",					0														, 0	},	/* 26 */
	{	"",					0														, 0	},	/* 27 */
	{	"",					0														, 0	},	/* 28 */
	{	"",					0														, 0	},	/* 29 */
	{	"",					0														, 0	},	/* 30 */
	{	PV_FNAME_RTCTLINF,	sizeof(struct st_RTCTLINF)								, 1 },	/* 31 */
	{	PV_FNAME_DEMNDFIL,	sizeof(struct st_DEMNDFIL)								, 1 },	/* 32 */
/* [20250807] Add St */
	{	PV_FNAME_FCRSTINF,	sizeof(struct st_FCRSTINF)								, 1	},	/* 33 *//* [20250807] Add */
	{	PV_FNAME_FCRRPINF,	sizeof(struct st_FCRRPINF)								, 1	},	/* 34 *//* [20250807] Add */
/* [20250807] Add Ed */
	{	PV_FNAME_RPRECFIL,	sizeof(struct st_RPRECFIL)								, 1	},	/* 35 *//* [20260515] Add */
	{	"",					0														, 0	},	/* 36 */
	{	"",					0														, 0	},	/* 37 */
	{	"",					0														, 0	},	/* 38 */
	{	"",					0														, 0	},	/* 39 */
	{	"",					0														, 0	},	/* 40 */
	/****** [ 41 - 60 ] 状態ファイル ******/
	{	PV_FNAME_TMSTSFIL,	sizeof(struct st_TMSTSFIL) * PV_MAX_TMDATA				, 1	},	/* 41 */
	{	PV_FNAME_SVSTSFIL,	sizeof(struct st_SVSTSFIL) * ((PV_MAX_SVDATA-1)/8+1)	, 1	},	/* 42 */
	{	PV_FNAME_STSTSFIL,	sizeof(struct st_STSTSFIL) * ((PV_MAX_STDATA-1)/8+1)	, 1	},	/* 43 */
	{	PV_FNAME_JOHENFIL,	sizeof(struct st_JOHENFIL)								, 1	},	/* 44 */
	{	PV_FNAME_TMSTOFIL,	sizeof(struct st_TMSTOFIL)								, 1	},	/* 45 */
	{	PV_FNAME_TMRECFIL,	sizeof(struct st_TMRECFIL)								, 1	},	/* 46 */
	{	PV_FNAME_SVRECFIL,	sizeof(struct st_SVRECFIL)								, 1	},	/* 47 */
	{	PV_FNAME_SVAGGFIL,	sizeof(struct st_SVAGGFIL) * PV_MAX_PVSYS_AGGSVPOS		, 1 },	/* 48 */
	{	PV_FNAME_EMCTLFIL,	sizeof(struct st_EMCTLFIL)								, 1	},	/* 49 */
	{	PV_FNAME_CTLJHFIL,	sizeof(struct st_CTLJHFIL)								, 1	},	/* 50 */
	{	PV_FNAME_PVCTLFIL,	sizeof(struct st_PVCTLFIL) * PV_CTL_TYPE_MAX			, 1	},	/* 51 */
	{	PV_FNAME_PWPLNFIL,	sizeof(struct st_PWPLNFIL)								, 1	},	/* 52 *//* [20210128] Add */
	{	PV_FNAME_SCHCTLFIL,	sizeof(struct st_SCHCTLFIL) * PV_SCH_TYPE_MAX			, 1	},	/* 53 *//* [20230529] Add */
	{	"",					0														, 0	},	/* 54 */
	{	"",					0														, 0	},	/* 55 */
	{	"",					0														, 0	},	/* 56 */
	{	"",					0														, 0	},	/* 57 */
	{	"",					0														, 0	},	/* 58 */
	{	"",					0														, 0	},	/* 59 */
	{	PV_FNAME_PVSYSLOG,	sizeof(struct st_PVSYSLOG)								, 1	},	/* 60 */
};


/* 内部関数定義 */
int func_cmn_sep_data( char *, struct st_cmn_sepStr * );
int	commfunc_chkUKind( int );
int commfunc_chkUComp( int , int );
int func_cmn_sepequ_data( char *, struct st_cmn_sepStr * );	/* [20210112] Add */


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミューテックス生成処理
 *
 * 機能：
 * 		収集装置で使用するオンラインファイル（最大３０ファイル）の排他管理を行ミ
 * 		ューテックス情報を保存する領域を共有メモリ上に確保し、ミューテックス情報
 * 		を初期化する。
 * 注意：
 * 		この関数は管理プロセス（nkmanageプロセス）のみ呼び出し可とする
 * 		一般プロセスは commfunc_mutex_attach() を呼び出すこと
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_mutex_create( void )
{
	int							shmid;
	int							errPid;
	int							errTid;
	int							loopCnt;
	pthread_mutexattr_t			mattr;

	/* ミューテックスの属性をプロセス間共有可能に設定する */
	pthread_mutexattr_init( &mattr );
	pthread_mutexattr_setpshared( &mattr, PTHREAD_PROCESS_SHARED );

	/* 共有メモリ上にミューテックス領域を確保する */
	shmid = shmget( PV_SHMKEY_MUTEX, PV_SHMSIZ_MUTEX, IPC_CREAT | 0666 );
	if( shmid == -1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_create(): shmget error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
		return( -1 );
	}
	/* 共有メモリのアドレスを取得する */
	ps_g_MUTEXMASTER = (pthread_mutex_t *)shmat( shmid, NULL, 0);	
	if( ps_g_MUTEXMASTER == (pthread_mutex_t *)-1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_create(): shmat error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
		return( -1 );
	}
	/* 共有メモリ上に確保したミューテックス格納領域にファイル数分のミューテックスを配置する */
	for( loopCnt = 0 ; loopCnt < PV_MAX_FILEINDEX ; loopCnt++ )
	{
		/* ミューテックスを初期化する */
		pthread_mutex_init( ps_g_MUTEXMASTER + loopCnt, &mattr );
	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミューテックスアタッチ処理
 *
 * 機能：
 * 		共有メモリ上のミューテックス情報にアタッチする
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_mutex_attach( void )
{
	int							shmid;
	int							errPid;
	int							errTid;
	int							loopCnt;
	pthread_mutexattr_t			mattr;

	/* 共有メモリ情報の取得が未実施の場合 */
	if( ps_g_MUTEXMASTER == NULL )
	{
		/* 共有メモリ上のＩＤを取得する */
		shmid = shmget( PV_SHMKEY_MUTEX, PV_SHMSIZ_MUTEX, 0 );
		if( shmid == -1 )
		{
			commfunc_pidtid_get( &errPid, &errTid );
			printf("commfunc_mutex_attach(): shmget error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
			return( -1 );
		}
		/* 共有メモリのアドレスを取得する */
		ps_g_MUTEXMASTER = (pthread_mutex_t *)shmat( shmid, NULL, 0);	
		if( ps_g_MUTEXMASTER == (pthread_mutex_t *)-1 )
		{
			commfunc_pidtid_get( &errPid, &errTid );
			printf("commfunc_mutex_attach(): shmat error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
			return( -1 );
		}
	}
#ifdef PV_DEBUG
	else
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_attach(): Allready Attached Shared Memory (Mutex Area) (pid:%d,tid:%d)\n", errPid, errTid );
	}
#endif
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミューテックスデタッチ処理
 *
 * 機能：
 * 		共有メモリ上のミューテックス情報をデタッチする
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_mutex_detach( void )
{
	int							shmid;
	int							errPid;
	int							errTid;
	int							loopCnt;

	/* ミューテックス用のアドレスが未設定？ */
	if( ps_g_MUTEXMASTER == NULL )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_detach(): Mutex Addr null error (pid:%d,tid:%d)\n", errPid, errTid );
		return( -1 );
	}
	/* 共有メモリ上のＩＤを取得する */
	shmid = shmget( PV_SHMKEY_MUTEX, PV_SHMSIZ_MUTEX, 0 );
	if( shmid == -1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_detach(): shmget error (errno:%d) (pid:%d,tid:%d)\n",errno , errPid, errTid );
		return( -1 );
	}
	/* 共有メモリをデタッチする */
	if( shmdt( ps_g_MUTEXMASTER ) == -1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_detach(): shmdt error (errno:%d) (pid:%d,tid:%d)\n",errno , errPid, errTid );
	}
	ps_g_MUTEXMASTER = NULL;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミューテックス破棄処理
 *
 * 機能：
 * 		共有メモリ上のミューテックス情報を破棄する
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_mutex_destroy( void )
{
	int							shmid;
	int							errPid;
	int							errTid;
	int							loopCnt;

	/* ミューテックス用のアドレスが未設定？ */
	if( ps_g_MUTEXMASTER == NULL )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_destroy(): Mutex Addr null error (pid:%d,tid:%d)\n", errPid, errTid );
		return( -1 );
	}
	/* 共有メモリ上のＩＤを取得する */
	shmid = shmget( PV_SHMKEY_MUTEX, sizeof(pthread_mutex_t) * PV_MAX_FILEINDEX, 0 );
	if( shmid == -1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_destroy(): shmget error (errno:%d) (pid:%d,tid:%d)\n",errno , errPid, errTid );
		return( -1 );
	}
	/* 共有メモリ上に確保したミューテックス格納領域にファイル数分のミューテックスを破棄する */
	for( loopCnt = 0 ; loopCnt < PV_MAX_FILEINDEX ; loopCnt++ )
	{
		/* ミューテックスを初期化する */
		pthread_mutex_destroy( ps_g_MUTEXMASTER + loopCnt );
	}
	/* 共有メモリをデタッチする */
	if( shmdt( ps_g_MUTEXMASTER ) == -1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_destroy(): shmdt error (errno:%d) (pid:%d,tid:%d)\n",errno , errPid, errTid );
	}
	ps_g_MUTEXMASTER = NULL;
	/* 共有メモリを破棄する */
	if( shmctl( shmid, IPC_RMID, NULL ) == -1 ) 
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_mutex_destroy(): shmctl IPC_RMID error (errno:%d) (pid:%d,tid:%d)\n",errno , errPid, errTid );
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルオープン処理
 *
 * 機能：
 * 		引数 ps_fileIndex に登録されているファイル（複数の場合あり）をオープンし
 * 		た後にＭＭＡＰ化する。ＭＭＡＰ化により取得したアドレスを ps_fileIndex に
 * 		登録する。
 * 		また、ＭＭＡＰ化したファイルのミューテックス情報も同時に ps_fileIndex に
 * 		登録する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int commfunc_file_open( struct st_fileIndex *ps_fileIndex )
{
	char				filePath[1024];
	int					fd;
	int					errPid;
	int					errTid;
	int					loopCnt;
	void				*point;

	/* ミューテックス領域をアタッチする */
	if( commfunc_mutex_attach() == -1 )
	{
		commfunc_pidtid_get( &errPid, &errTid );
		printf("commfunc_file_open(): Mutex attach error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
		return( -1 );
	}

	/* ファイルインデックス分処理を繰り返す */
	for(loopCnt = 0 ; loopCnt < PV_MAX_FILEINDEX ; loopCnt++)
	{
		/* ファイルインデックスが終端の場合 */
		if( (ps_fileIndex + loopCnt)->fileNumb == 0 )
			break;
		/* ファイルがＭＭＡＰに展開済みの場合 */
		if( (ps_fileIndex + loopCnt)->openStat == 2 )
			continue;
		/* ファイル番号が不正の場合 */
		if( (ps_fileIndex + loopCnt)->fileNumb > PV_MAX_FILEINDEX )
		{
			commfunc_pidtid_get( &errPid, &errTid );
			printf("commfunc_file_open(): File number error (FileNo:%d:%s) (pid:%d,tid:%d)\n", (ps_fileIndex + loopCnt)->fileNumb, s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileName, errPid, errTid );
			/* 全ファイルクローズ処理 */
			commfunc_file_close( ps_fileIndex );
			break;
		}
		/* ファイル名称が未定義の場合 */
		if( strlen( s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileName ) == 0 )
		{
			commfunc_pidtid_get( &errPid, &errTid );
			printf("commfunc_file_open(): File name error (FileNo:%d) (pid:%d,tid:%d)\n", (ps_fileIndex + loopCnt)->fileNumb, errPid, errTid );
			/* 全ファイルクローズ処理 */
			commfunc_file_close( ps_fileIndex );
			break;
		}
		/* ファイルパスの初期化 */
		strcpy( filePath, PV_PATH_DAT );
		/* .dat ファイルをオープン */
		strcat( filePath, s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileName );
		strcat( filePath, ".dat" );
		if( s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].writeEnable == 1 )
		{
			fd = open( filePath, O_RDWR );
		}
		else
		{
			fd = open( filePath, O_RDONLY );
		}
		if( fd == -1 )
		{
			commfunc_pidtid_get( &errPid, &errTid );
			printf("commfunc_file_open(): File[%d:%s] open error open-mode[%d] (errno:%d) (pid:%d,tid:%d)\n",
				loopCnt, s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileName, s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].writeEnable, errno, errPid, errTid );
			commfunc_file_close( ps_fileIndex );
			return( -1 );
		}
		/* ファイルディスクリプタをセット */
		(ps_fileIndex + loopCnt)->fd = fd;
		/* ファイルオープンステータスをオープン済みに設定 */
		(ps_fileIndex + loopCnt)->openStat = 1;
		/* ファイルをＭＭＡＰとして共有メモリに展開 */
		if( s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].writeEnable == 1 )
		{
			point = (void *)mmap( NULL, s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
		}
		else
		{
			point = (void *)mmap( NULL, s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileSize, PROT_READ, MAP_SHARED, fd, 0 );
		}
		if( point == MAP_FAILED )
		{
			commfunc_pidtid_get( &errPid, &errTid );
			printf("commfunc_file_open(): Mmap error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
			/* 全ファイルクローズ処理 */
			commfunc_file_close( ps_fileIndex );
			return( -1 );
		}
		/* 共有メモリアドレスをセット */
		*((ps_fileIndex + loopCnt)->point) = point;
	
		/* 共有メモリサイズをセット */
		(ps_fileIndex + loopCnt)->fileSize = s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].fileSize;
		/* ファイルオープンステータスをＭＭＡＰ展開済みに設定 */
		(ps_fileIndex + loopCnt)->openStat = 2;

		switch( (ps_fileIndex + loopCnt)->fileNumb )
		{
			case PV_FILENO_PVSYSINF:
				ps_gpt_PVSYSINF = (struct st_PVSYSINF *)point;
				break;
			case PV_FILENO_JOHENFIL:
				ps_gpt_JOHENFIL = (struct st_JOHENFIL *)point;
				break;
			case PV_FILENO_PVSYSLOG:
				ps_gpt_PVSYSLOG = (struct st_PVSYSLOG *)point;
				break;
			case PV_FILENO_PVCTLFIL:
				ps_gpt_PVCTLFIL = (struct st_PVCTLFIL *)point;
				break;
			case PV_FILENO_CTLJHFIL:
				ps_gpt_CTLJHFIL = (struct st_CTLJHFIL *)point;
				break;
			case PV_FILENO_TMRECFIL:
				ps_gpt_TMRECFIL = (struct st_TMRECFIL *)point;
				break;
			case PV_FILENO_SVRECFIL:
				ps_gpt_SVRECFIL = (struct st_SVRECFIL *)point;
				break;
			case PV_FILENO_SCHCTLFIL:
				ps_gpt_SCHCTLFIL = (struct st_SCHCTLFIL *)point;
				break;
/* [20260515] Add St */
			case PV_FILENO_RPRECFIL:
				ps_gpt_RPRECFIL = (struct st_RPRECFIL *)point;
				break;
/* [20260515] Add Ed */
			default:
				break;
		}

	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルクローズ処理
 *
 * 機能：
 * 		引数 ps_fileIndex に登録されているファイル（複数の場合あり）のＭＭＡＰを
 * 		アンマップした後にクローズする。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_file_close( struct st_fileIndex *ps_fileIndex )
{
	int					loopCnt;

	/* ファイルインデックス分処理を繰り返す */
	for(loopCnt = 0 ; loopCnt < PV_MAX_FILEINDEX ; loopCnt++)
	{
		/* ファイルインデックスが終端の場合 */
		if( (ps_fileIndex + loopCnt)->fileNumb == 0 )
			break;

		/* ファイルがクローズ済みの場合（未オープンの場合） */
		if( (ps_fileIndex + loopCnt)->openStat == 0 )
			continue;

		/* ファイルがＭＭＡＰに展開されている場合 */
		if( (ps_fileIndex + loopCnt)->openStat == 2 )
		{
			/* ＭＭＡＰをアンマップする */
			munmap( *((ps_fileIndex + loopCnt)->point), (ps_fileIndex + loopCnt)->fileSize );
			/* ファイルオープンステータスをオープン済みに設定 */
			(ps_fileIndex + loopCnt)->openStat = 1;
		}

		/* ファイルがオープン済みの場合 */
		if( (ps_fileIndex + loopCnt)->openStat == 1 )
		{
			/* ファイルをクローズ */
			close((ps_fileIndex + loopCnt)->fd );
		}
		/* ファイルのオープンステータスをクローズに設定 */
		(ps_fileIndex + loopCnt)->openStat = 0;

		switch( (ps_fileIndex + loopCnt)->fileNumb )
		{
			case PV_FILENO_PVSYSINF:
				ps_gpt_PVSYSINF = NULL;
				break;
			case PV_FILENO_JOHENFIL:
				ps_gpt_JOHENFIL = NULL;
				break;
			case PV_FILENO_PVSYSLOG:
				ps_gpt_PVSYSLOG = NULL;
				break;
			case PV_FILENO_PVCTLFIL:
				ps_gpt_PVCTLFIL = NULL;
				break;
			case PV_FILENO_CTLJHFIL:
				ps_gpt_CTLJHFIL = NULL;
				break;
			case PV_FILENO_TMRECFIL:
				ps_gpt_TMRECFIL = NULL;
				break;
			case PV_FILENO_SVRECFIL:
				ps_gpt_SVRECFIL = NULL;
				break;
			case PV_FILENO_SCHCTLFIL:
				ps_gpt_SCHCTLFIL = NULL;
				break;
/* [20260515] Add St */
			case PV_FILENO_RPRECFIL:
				ps_gpt_RPRECFIL = NULL;
				break;
/* [20260515] Add Ed */
			default:
				break;
		}
	}
	return( 0 );
}

/* [20130823] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル同期処理
 *
 * 機能：
 * 		引数 ps_fileIndex に登録されているファイル（複数の場合あり）のＭＭＡＰを
 * 		ファイルシステムに同期（ＳＹＮＣ）する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_file_sync( struct st_fileIndex *ps_fileIndex )
{
	int					loopCnt;

	/* ファイルインデックス分処理を繰り返す */
	for(loopCnt = 0 ; loopCnt < PV_MAX_FILEINDEX ; loopCnt++)
	{
		/* ファイルインデックスが終端の場合 */
		if( (ps_fileIndex + loopCnt)->fileNumb == 0 )
			break;

		/* ファイルがクローズ済みの場合（未オープンの場合） */
		if( (ps_fileIndex + loopCnt)->openStat == 0 )
			continue;

		/* ファイルがＭＭＡＰに展開されている場合 */
		if( (ps_fileIndex + loopCnt)->openStat == 2 )
		{
			/* ＭＭＡＰが書込可能なファイルの場合 */
			if( s_mmapInfo[(ps_fileIndex + loopCnt)->fileNumb - 1].writeEnable != 0 )
			{
				/* ファイルを同期（ＳＹＮＣ） */
				msync( *((ps_fileIndex + loopCnt)->point), (ps_fileIndex + loopCnt)->fileSize, MS_SYNC );
			}
		}
	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルリマップ処理
 *
 * 機能：
 * 		引数 ps_fileIndex に登録されているファイル（複数の場合あり）から、書込可
 *      能なファイルについてＭＵＮＭＡＰした後に再度ＭＭＡＰする。
 * 重要：
 *		この関数を呼び出すプロセスは、内部でファイルのロッキング（ミューテックス
 *		ロック）を処理するため、関数を呼び出す前に全てのファイルのロックを解除し
 *		ておくこと
 * 備考：
 *		OpenBlocksA6(Debian6)の場合mmap()関数によりファイルをメモリマッピングし
 *      た場合下記の挙動を示すことが判明した。
 *		・msync(MS_ASYNC)関数およびmsync(MS_SYNC)関数による物理媒体への書き出し
 *		  指示は機能しない。
 *		・fsync()関数およびfdatasync()関数による物理媒体への書き出し指示はは機能
 *		  しない。
 *		・OSがメモリマッピングした領域をダーティー化認識するのは、マッピング後に
 *		  初めてデータ更新された場合のみ（最初のみで同じページ内のデータを何回更
 *		  新しても２回目以降はダーティー化認識しない）
 *		※OpenBlocksA6(Debian7)では一般的なLinux系OSと同様の動作をするため、上記
 *		  挙動の対象となるのはOpenBlocksA6(Debian6)のみ。
 *
 *		以上のことから、OpenBlocksA6(Debian6)では、mmap()関数でファイルをメモリ
 *		マッピングした場合、最初の数分間の更新はダーティー化認識して物理媒体に書
 *		き出されるが、プログラムがメモリの内容を何回更新してもダーティー化認識し
 *		ないため物理媒体への書き出しが発生しない。それ以降はmunmap()関数によりア
 *		ンマップしない限り物理媒体に出力されないことが判明した。
 *		収集装置のOS再起動やプログラム・データ入替時はソフトウェア停止時に
 *		munmap()関数によりアンマップしているのでファイルの内容は停止時の値が保存
 *		される。しかし、突発的な電源喪失時など停止手順を踏まなかった場合は、電源
 *		喪失直前までの状態は保存されない。（ソフトウェア起動後数分後の状態しか保
 *		存されていない）
 *		
 *		収集装置の運転を継続中でもファイルの書き出しを行うため、munmap()を実施し
 *		た後直ちにmmap()を行う、その後メモリの各ページを部分的に更新してダーティ
 *		ー化する処理を追加する
 *
 *		この処理は、OpenBlocksA6(Debain6)専用とする
 *
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_file_remap( struct st_fileIndex *ps_fileIndex )
{
	char				pageDt;
	char				*pagePt;
	int					errPid;
	int					errTid;
	int					loopCnt1;
	int					loopCnt2;
	int					pageSize;
	void				*point;

	/* 本処理はOpenBlocksA6(Debian6)のみ必要な処理 */
	/* 上記以外のシステムでは使用しないようにする  */

	/* メモリページサイズを取得する */
	pageSize = getpagesize();

	/* ファイルインデックス分処理を繰り返す */
	for(loopCnt1 = 0 ; loopCnt1 < PV_MAX_FILEINDEX ; loopCnt1++)
	{
		/* ファイルインデックスが終端の場合 */
		if( (ps_fileIndex + loopCnt1)->fileNumb == 0 )
			break;

		/* ファイルがクローズ済みの場合（未オープンの場合） */
		if( (ps_fileIndex + loopCnt1)->openStat == 0 )
			continue;

		/* ファイルがＭＭＡＰに展開されている場合 */
		if( (ps_fileIndex + loopCnt1)->openStat == 2 )
		{
			/* マッピングしているファイルが書込可能な場合 */
			if( s_mmapInfo[(ps_fileIndex + loopCnt1)->fileNumb - 1].writeEnable == 1 )
			{
				/* ＭＭＡＰをアンマップする */
				munmap( *((ps_fileIndex + loopCnt1)->point), (ps_fileIndex + loopCnt1)->fileSize );

				/* ファイルオープンステータスをオープン済みに設定 */
				(ps_fileIndex + loopCnt1)->openStat = 1;

				/* ファイルをＭＭＡＰとして共有メモリに展開 */
				point = (void *)mmap( NULL,
				                      s_mmapInfo[(ps_fileIndex + loopCnt1)->fileNumb - 1].fileSize, 
				                      PROT_READ | PROT_WRITE, 
				                      MAP_SHARED, 
				                      (ps_fileIndex + loopCnt1)->fd, 
				                      0 );
				if( point == MAP_FAILED )
				{
					commfunc_pidtid_get( &errPid, &errTid );
					printf("commfunc_file_open(): Mmap error (errno:%d) (pid:%d,tid:%d)\n", errno, errPid, errTid );
					/* 全ファイルクローズ処理 */
					commfunc_file_close( ps_fileIndex );
					return( -1 );
				}
				/* 共有メモリアドレスをセット */
				*((ps_fileIndex + loopCnt1)->point) = point;

				/* ファイルが PVSYSINF,JOHENFL,PVSYSLOG の場合 */
				switch( (ps_fileIndex + loopCnt1)->fileNumb )
				{
					case PV_FILENO_PVSYSINF:
						ps_gpt_PVSYSINF = (struct st_PVSYSINF *)point;
						break;
					case PV_FILENO_JOHENFIL:
						ps_gpt_JOHENFIL = (struct st_JOHENFIL *)point;
						break;
					case PV_FILENO_PVSYSLOG:
						ps_gpt_PVSYSLOG = (struct st_PVSYSLOG *)point;
						break;
					case PV_FILENO_PVCTLFIL:
						ps_gpt_PVCTLFIL = (struct st_PVCTLFIL *)point;
						break;
					case PV_FILENO_CTLJHFIL:
						ps_gpt_CTLJHFIL = (struct st_CTLJHFIL *)point;
						break;
					case PV_FILENO_TMRECFIL:
						ps_gpt_TMRECFIL = (struct st_TMRECFIL *)point;
						break;
					case PV_FILENO_SVRECFIL:
						ps_gpt_SVRECFIL = (struct st_SVRECFIL *)point;
						break;
					case PV_FILENO_SCHCTLFIL:
						ps_gpt_SCHCTLFIL = (struct st_SCHCTLFIL *)point;
						break;
/* [20260515] Add St */
					case PV_FILENO_RPRECFIL:
						ps_gpt_RPRECFIL = (struct st_RPRECFIL *)point;
						break;
/* [20260515] Add Ed */
					default:
						break;
				}

				/* ファイルオープンステータスをＭＭＡＰ展開済みに設定 */
				(ps_fileIndex + loopCnt1)->openStat = 2;

				/*--------------------------------------------------*/
				/* ＯＳによるファイル更新を促すため、               */
				/* メモリにマッピングした全ページをダーティー化する */
				/*--------------------------------------------------*/

				/* ファイルをロックする */
				commfunc_mutex_lock( (ps_fileIndex + loopCnt1)->fileNumb );

				/* メモリアドレスを設定 */
				pagePt = (char *)point;

				/* 各ページの先頭１バイトを更新することにより全ページをダーティー化する */
				for( loopCnt2 = 0 ; loopCnt2 < (ps_fileIndex + loopCnt1)->fileSize ; loopCnt2 += pageSize )
				{
					/* １バイト分のデータを取り出す */
					pageDt = *pagePt;
					/* 取り出したデータを下記戻す（書込を行うことでダーティー化する） */
					*pagePt = pageDt;
					/* 次ページのポインタを設定 */
					pagePt += pageSize;
				}

				/* ファイルをアンロックする */
				commfunc_mutex_unlock( (ps_fileIndex + loopCnt1)->fileNumb );
			}
		}

	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミューテックスロック処理
 *
 * 機能：
 * 		引数 fileNo で指定されたファイルのミューテックスをロックする
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_mutex_lock( int fileNo )
{
	int					pidNumb = 0;
	int					tidNumb = 0;
	int					retCode;

	/* ミューテック領域が確保されていない場合 */
	if( ps_g_MUTEXMASTER == NULL ) return( -1 );

	/* プロセスＩＤとスレッドＩＤを取得 */
	commfunc_pidtid_get( &pidNumb, &tidNumb );
	
	/* ファイル番号が規定値以内か確認 */
	if( ( fileNo < 0 ) || ( fileNo > PV_MAX_FILEINDEX ) )
	{
		printf("commfunc_mutex_lock(): File-No:%02d Number Error (pid:%d,tid:%d)\n", fileNo, pidNumb, tidNumb );
		return ( -1 );
	}

	/* 多重ロックの検出（自身による二重ロック防止） */
	if( ps_g_MUTEXMASTER[ fileNo - 1 ].__data.__owner == tidNumb )
	{
		printf("commfunc_mutex_lock(): File-No:%02d Recursive Lock Error (pid:%d,tid:%d)\n", fileNo, pidNumb, tidNumb );
		return( -1 );
	}

#if PV_DEBUG
	if( getppid() == 1 )
		printf("commfunc_mutex_lock():   File-No:%02d LOCK REQUEST (pid:%d,tid:%d) NKMANAGE\n", fileNo, pidNumb, tidNumb );
	else
		printf("commfunc_mutex_lock():   File-No:%02d LOCK REQUEST (pid:%d,tid:%d)\n", fileNo, pidNumb, tidNumb );
#endif

	/* ミューテックスロック */
	retCode = pthread_mutex_lock( &ps_g_MUTEXMASTER[ fileNo - 1 ] );
	if( retCode != 0 )
	{
		printf("commfunc_mutex_lock(): File-No:%02d pthread_mutex_lock Error (errno:%d) (pid:%d,tid:%d)\n", fileNo, errno, pidNumb, tidNumb );
		return( -1 );
	}

#if PV_DEBUG
	if( getppid() == 1 )
		printf("commfunc_mutex_lock():   File-No:%02d LOCK = I N = (pid:%d,tid:%d) NKMANAGE\n", fileNo, pidNumb, tidNumb );
	else
		printf("commfunc_mutex_lock():   File-No:%02d LOCK = I N = (pid:%d,tid:%d)\n", fileNo, pidNumb, tidNumb );
#endif

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミューテックスアンロック処理
 *
 * 機能：
 * 		引数 fileNo で指定されたファイルのミューテックスをアンロックする
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	commfunc_mutex_unlock( int fileNo )
{
	int					pidNumb = 0;
	int					tidNumb = 0;
	int					retCode;

	/* ミューテック領域が確保されていない場合 */
	if( ps_g_MUTEXMASTER == NULL ) return( -1 );

	/* プロセスＩＤとスレッドＩＤを取得 */
	commfunc_pidtid_get( &pidNumb, &tidNumb );
	
	/* ファイル番号が規定値以内か確認 */
	if( ( fileNo < 0 ) || ( fileNo > PV_MAX_FILEINDEX ) )
	{
		printf("commfunc_mutex_unlock(): File-No:%02d Number Error (pid:%d,tid:%d)\n", fileNo, pidNumb, tidNumb );
		return ( -1 );
	}

	/* ロック保持者のチェック（他者ロックの誤解助防止） */
	if( ps_g_MUTEXMASTER[ fileNo - 1 ].__data.__owner != tidNumb )
	{
		printf("commfunc_mutex_unlock(): File-No:%02d Different Owner Error Lock owner tid:%d (pid:%d,tid:%d)\n",
			fileNo, ps_g_MUTEXMASTER[ fileNo - 1 ].__data.__owner, pidNumb, tidNumb );
		return( -1 );
	}

#if PV_DEBUG
	if( getppid() == 1 )
		printf("commfunc_mutex_unlock(): File-No:%02d LOCK RELEASE (pid:%d,tid:%d) NKMANAGE\n", fileNo, pidNumb, tidNumb );
	else
		printf("commfunc_mutex_unlock(): File-No:%02d LOCK RELEASE (pid:%d,tid:%d)\n", fileNo, pidNumb, tidNumb );
#endif

	/* ミューテックスアンロック */
	retCode = pthread_mutex_unlock( &ps_g_MUTEXMASTER[ fileNo - 1 ] );
	if( retCode != 0 )
	{
		printf("commfunc_mutex_unlock(): File-No:%02d pthread_mutex_unlock Error (errno:%d) (pid:%d,tid:%d)\n", fileNo, errno, pidNumb, tidNumb );
		return( -1 );
	}

	return( 0 );
}

/* [20230830] Add St */
int	func_BATTEINF_Check( FILE *fp, int readType, struct st_BATTEINF *ps_BATTEINF, char * csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	memset( ps_BATTEINF, 0, sizeof(struct st_BATTEINF) );

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;
		/* 未入力行（9文字以下）はスキップ */
		if( strlen( readBuff ) < 10) continue;
		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* デマンド制御情報に関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_BATTEINF, sizeof(PV_FNAME_BATTEINF) ) != 0 )
				continue;
			csvOffset++;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 13+csvOffset)
		{
			sprintf( errMsg, "CSV-Format Error (Data is not 13). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* バッテリー番号の取得 */
		// Skip : bttNumb = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;

		/* 実装/未実装の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 未実装の場合 */
		if( workData == 0 ) continue;
		/* 実装以外の場合 */
		if( workData != 1 )
		{
			sprintf( errMsg, "CSV-Format Error (Flag Error). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->statFlag = workData;

		/* 所属回線番号の取得 */
		// Skip : workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;

		/* 装置ＩＤ番号の取得 */
		// Skip : workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;

		/* 型式名称の取得 */
		// Skip : if( strlen(s_sepStr.data[csvOffset]) == 0 )
		csvOffset++;

		/* 名称の取得 */
		// Skip : if( strlen(s_sepStr.data[csvOffset]) == 0 )
		csvOffset++;

		/* バッテリ用途の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( ( workData < BT_TYPE_STORAGE_NO_OFFGRID) ||
			( workData > BT_TYPE_MAX) )
		{
			sprintf( errMsg, "CSV-Format Error (batteType <= 0). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->batteType = workData;

		/* バッテリ容量を取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData <= 0 )
		{
			sprintf( errMsg, "CSV-Format Error (batteCap <= 0). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->batteCap = workData;
		
		/* 最大定格電力を取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData <= 0 )
		{
			sprintf( errMsg, "CSV-Format Error (Max Rated Power <= 0). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->maxRatedPwr = workData;
		
		/* 最大放電電力を取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format Error (discharge Power < 0). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->maxDisChgPwr = workData;

		/* 最大充電電力を取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format Error (charge Power < 0 ). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->maxChargePwr = workData;


		/* SoC閾値上限を取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( (workData < 0 ) ||
		    ( workData > 100 ))
		{
			sprintf( errMsg, "CSV-Format Error (SoC Upper Threshold Error). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->socUpper = workData;

		/* SoC閾値下限を取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( (workData < 0 ) ||
		    ( ps_BATTEINF->socUpper <= workData ))
		{
			sprintf( errMsg, "CSV-Format Error (SoC Lower Threshold Error). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		ps_BATTEINF->socLower = workData;
	}

	return retCode;
}
/* [20230830] Add Ed */

int	func_DEMNDINF_Check( FILE *fp, int readType, struct st_DEMNDINF *ps_DEMNDINF, char * csvFilePath, struct st_PVSYSINF *ps_PVSYSINF, struct st_TMDATINF *ps_TMDATINF )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int						csvOffset=0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						retCode = 0;				/* 戻り値 */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int						soKind;						/* 装置種別 */
	int						soNumb;						/* 装置番号 */
	int						tmNumb;						/* 計測番号 */
	int						workData;					/* 数値変換用 */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	short					expData;					/* 乗率 */
	short					unitKind;					/* 単位種別 */
	int						maxTmNumb;					/* 最大計測数 */
	int						maxMNumb;					/* 最大装置数 */
	int						linkBase;					/* 計測情報基本リンク */
	char					moniFlag = EMCTL_FLG_OFF;	/* デマンド監視対象設定フラグ */
	int						expMin;						/* 乗率最小値 */
	int 					unitMin;					/* 単位最小 */

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		csvOffset = 0;
		/* CSV処理カウンタを＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;

		/* 未入力行（1文字以下）はスキップ */
		if( strlen( readBuff ) < 1) continue;

		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* デマンド制御情報に関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_DEMNDINF, sizeof(PV_FNAME_DEMNDINF) ) != 0 )
				continue;
			csvOffset++;
		}

		if( csvCount == 3+csvOffset )
		{
			/* 装置種別の取得 */
			soKind = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (soKind < 0) || (soKind > PV_MKIND_LAST) )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (soKind:%d). File[%s] Line[%d] DAT[%d]", soKind, csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}
			ps_DEMNDINF->soKind = soKind;

			/* 装置が実装されていない場合 */
			if( ps_PVSYSINF->s_machineInfo[soKind].machineCnt == 0 )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (soKind machineCnt is 0(%d)). File[%s] Line[%d] DAT[%d]", ps_PVSYSINF->s_machineInfo[soKind].machineCnt, csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}

			/* 装置の最大情報数の取得 */
			maxMNumb  = ps_PVSYSINF->s_machineInfo[soKind].maxMachine;
			maxTmNumb = ps_PVSYSINF->s_machineInfo[soKind].maxTm;

			/* 装置番号の取得 */
			soNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (soNumb < 1) || (soNumb > maxMNumb) )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (soNumb). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}
			ps_DEMNDINF->soNumb = soNumb;

			/* 計測番号の取得 */
			tmNumb = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (tmNumb < 1) || (tmNumb > maxTmNumb) )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (tmNumb). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}
			ps_DEMNDINF->tmNumb = tmNumb;

			/* 装置の最大情報数の取得 */
			linkBase  = ps_PVSYSINF->s_machineInfo[soKind].tmIfLink + maxTmNumb * (soNumb - 1) + (tmNumb - 1);

			/* 計測値が実装されているか */
			if( ps_TMDATINF[linkBase].statFlag == 0 )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (TM Stat). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}
			/* 計測単位 */
			unitKind = PV_TM_UKIND_WH;

			/* 乗率（冪乗） */
			expData = 0;

			/* 乗率最小および単位最小を設定 */
			expMin  =  999999999;
			unitMin =  999999999;

/* [20250807] Chg St */
			/* 演算結果の単位確認 */
			if( (retCode = commfunc_sumInfChk( unitKind, unitKind, expData, &expMin, &unitMin, 1 )) != 0 )
/* [20250807] Chg Ed */
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (Unit Kind Data). File[%s] Line[%d] DAT[%d,4] retCode=%d", csvFilePath, syoriCount, retCode );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}

/* [20250807] Chg St */
			/* 単位種別の確認 */
			if( (retCode = commfunc_sumInfChk( unitKind, ps_TMDATINF[linkBase].unitKind, ps_TMDATINF[linkBase].expData, &expMin, &unitMin, 1 )) != 0 )
/* [20250807] Chg Ed */
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (Unit Kind). File[%s] Line[%d] DAT[%d,4] retCode=%d", csvFilePath, syoriCount, retCode);
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				printf("%s\n", errMsg);
				break;
			}

			/* デマンド監視情報を格納 */
			ps_DEMNDINF->soKind		= soKind;						/* 所属装置種別 */
			ps_DEMNDINF->soNumb		= soNumb;						/* 所属装置番号 */
			ps_DEMNDINF->tmNumb		= tmNumb;						/* 装置内計測番号 */
			ps_DEMNDINF->expData	= expData;						/* 乗率 */
			ps_DEMNDINF->unitKind	= unitKind;						/* 単位種別 */

			/* デマンド演算情報の設定 */
			ps_DEMNDINF->tmIfLink	= linkBase;									/* 計測情報リンク情報（監視対象計測値） */
			ps_DEMNDINF->dmdpowExp   = commfunc_pow10abs( expData - expMin ) ;	/* 調整用乗率値（デマンド電力）の設定 */
			expData = commfunc_chkUChosei( unitKind ) - unitMin;				/* 単位種別による乗率を求める */
			ps_DEMNDINF->dmdpowdUnit = commfunc_pow10abs( expData );			/* 調整用単位値（デマンド電力）の設定 */
			expData  = ps_TMDATINF[linkBase].expData - expMin;					/* デマンド演算対象の乗率と全計測値の乗率の差分を調整 */
			ps_DEMNDINF->choseiExp	= commfunc_pow10abs( expData );				/* 調整用乗率（監視対象計測値）の設定 */
			expData = commfunc_chkUChosei( ps_TMDATINF[linkBase].unitKind ) - unitMin;	/* 単位種別による乗率を求める */
			ps_DEMNDINF->choseiUnit	= commfunc_pow10abs( expData );				/* 調整用単位値（監視対象計測値）の設定 */
			ps_DEMNDINF->choseiConv	= ps_TMDATINF[linkBase].convData;			/* 調整用変換値（監視対象計測値）の設定 */
			ps_DEMNDINF->choseiBase	= ps_TMDATINF[linkBase].baseData;			/* 調整用ベース値（監視対象計測値）の設定 */

			/* デマンド監視対象設定フラグをONに設定 */
			moniFlag = EMCTL_FLG_ON;
			ps_DEMNDINF->statFlag = 1;

		}
		else if( csvCount == 6+csvOffset )
		{
			/* 契約電力の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData <= 0)
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (dmdContractPow). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_DEMNDINF->dmdContractPow = workData;

			/* デマンド監視Ｌｖ１閾値の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (thresholdVal Lv1). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			if( workData > 0 )
			{
				ps_DEMNDINF->s_thresholdInf[0].statFlag = 1;
				ps_DEMNDINF->s_thresholdInf[0].thresholdVal = workData;
			}

			/* デマンド監視Ｌｖ２閾値の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (thresholdVal Lv1). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			if( workData > 0 )
			{
				ps_DEMNDINF->s_thresholdInf[1].statFlag = 1;
				ps_DEMNDINF->s_thresholdInf[1].thresholdVal = workData;
			}

			/* デマンド監視Ｌｖ３閾値の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (thresholdVal Lv1). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			if( workData > 0 )
			{
				ps_DEMNDINF->s_thresholdInf[2].statFlag = 1;
				ps_DEMNDINF->s_thresholdInf[2].thresholdVal = workData;
			}

			/* デマンド監視Ｌｖ４閾値の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (thresholdVal Lv1). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			if( workData > 0 )
			{
				ps_DEMNDINF->s_thresholdInf[3].statFlag = 1;
				ps_DEMNDINF->s_thresholdInf[3].thresholdVal = workData;
			}

			/* 監視開始時刻 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( ( workData < 0 ) || ( workData >= 30 ) )
			{
				sprintf( errMsg, "CSV-Format(DEMNDINF) Error (startUpTime). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			ps_DEMNDINF->startUpTime = workData;
			
		}
		else
		{
			sprintf( errMsg, "CSV-Format(DEMNDINF) Error (Data is not 6 or 3). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
#if 0
		printf("DEMNDINF[%d] : FLG[%d], SK:%2d, SN:%2d, TN:%3d, LINK:%4d, RTD:%8d, UP:%8d, LW:%8d, RCHK:%d, RPWR:%8d, SPWR:%8d, RNK:%2d, RSN:%2d, RSVN:%3d, SVLINK:%4d\n",
			dgPos+1,
			ps_DEMNDINF[dgPos].statFlag,
			ps_DEMNDINF[dgPos].soKind,
			ps_DEMNDINF[dgPos].soNumb,
			ps_DEMNDINF[dgPos].tmNumb,
			ps_DEMNDINF[dgPos].tmIfLink,
			ps_DEMNDINF[dgPos].ratedPower,
			ps_DEMNDINF[dgPos].upplmt,
			ps_DEMNDINF[dgPos].lowlmt,
			ps_DEMNDINF[dgPos].runCheck,
			ps_DEMNDINF[dgPos].stopPower,
			ps_DEMNDINF[dgPos].runSoKind,
			ps_DEMNDINF[dgPos].runSoNumb,
			ps_DEMNDINF[dgPos].runSvIfLink);
#endif

	}

	/* デマンド監視対象が設定されていない場合 */
	if( moniFlag == EMCTL_FLG_OFF )
	{
		sprintf( errMsg, "CSV-Format(DEMNDINF) Error (No Data MoniData). File[%s]", csvFilePath);
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
		retCode = -1;
	}

	/* 終了 */
	return( retCode );	
}

int	func_CEMSPINF_Check( FILE *fp, int readType, struct st_CEMSPINF *ps_CEMSPINF, char * csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	char					setFlag[PV_BIO_SUPPRESS_LV_MAX];
	int						setIdx;

	/* ファイルの内容を0クリア */
	memset( ps_CEMSPINF, 0, sizeof(struct st_CEMSPINF) );
	memset( setFlag, 0, sizeof(setFlag));

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		csvOffset=0;
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;

		/* 未入力行（3文字以下）はスキップ */
		if( strlen( readBuff ) < 3) continue;

		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* CEMSに関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_CEMSPINF, sizeof(PV_FNAME_CEMSPINF) ) != 0 )
				continue;
			csvOffset++;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 13+csvOffset )		/* [20240220] Chg */
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (Data is not %d ). File[%s] Line[%d] DATACNT[%d]", 13+csvOffset, csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}

/* [20230217] Add St */

		/* 価格連動制御機能優先度の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (arbitragePri[%d]). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->arbitragePri = workData;


		/* ピークカット機能優先度の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (peakCutPri[%d]). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->peakCutPri = workData;

		/* BCP機能優先度の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (bcpPri[%d]). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->bcpPri = workData;
/* [20230217] Add Ed */

/* [20240220] Add St */
		/* 需給調整市場優先度の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (balancePri[%d]). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->balancePri = workData;

		/* 容量市場優先度の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (capacityPri[%d]). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->capacityPri = workData;
/* [20240220] Add Ed */

		/* BT充放電制御設定の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( (workData != 0) && (workData != 1) )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (btControl). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->btControl = workData;

		/* PV抑制制御の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( (workData != 0) && (workData != 1) )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (pvControl). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->pvControl = workData;

		/* shutdownWaitの取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (shutdownWait). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->shutdownWait = workData;

/* [20230301] Chg St */
		/* BT出力変化量(充電)の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 1 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (btChangeAmtChg). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->btChangeAmtChg = workData;

		/* BT出力変化量(放電)の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 1 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (btChangeAmtDischg). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->btChangeAmtDischg = workData;

		/* PV出力変化量(抑制)の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 1 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (pvChangeAmtDown). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->pvChangeAmtDown = workData;

		/* PV出力変化量(発電)の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 1 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (pvChangeAmtUp). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->pvChangeAmtUp = workData;
/* [20230301] Chg Ed */

		/* 制御開始時間の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(CEMSPINF) Error (ctlCntDwn). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_CEMSPINF->ctlCountDown = workData;

	}

#if 0
		printf("CEMSPINF : balancePri:%d, arbitragePri:%d, btControl:%d, pvControl:%d, shutdownWait:%8d, btChangeAmtChg:%8d, btChangeAmtDischg:%2d, pvChangeAmtDown:%2d, pvChangeAmtUp:%d, ctlCountDown:%d\n",
			ps_CEMSPINF->balancePri,
			ps_CEMSPINF->arbitragePri,
			ps_CEMSPINF->btControl,
			ps_CEMSPINF->pvControl,
			ps_CEMSPINF->shutdownWait,
			ps_CEMSPINF->btChangeAmtChg,
			ps_CEMSPINF->btChangeAmtDischg,
			ps_CEMSPINF->pvChangeAmtDown,
			ps_CEMSPINF->pvChangeAmtUp,
			ps_CEMSPINF->ctlCountDown);
#endif
	if( retCode < 0 )
	{
		sprintf( errMsg, "CSV-Format(CEMSPINF) data is [%s]", readBuff );
		commfunc_syslog_msgset( "FILEMAKE", errMsg );
	}

	return retCode;
}


int	func_RCVPWINF_Check( FILE *fp, int readType, struct st_RCVPWINF *ps_RCVPWINF, char * csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int						checkVal;					/* 閾値(チェック用) */
	char					setFlag[PV_RCVPW_THR_NUM];
	int						setIdx;
	int						cntAddPowInf=0;

	memset( ps_RCVPWINF, 0, sizeof(struct st_RCVPWINF) );
	memset( setFlag, 0, sizeof(setFlag));

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		csvOffset=0;
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;

		/* 未入力行（3文字以下）はスキップ */
		if( strlen( readBuff ) < 3) continue;

		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* 需給バランス調整に関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_RCVPWINF, sizeof(PV_FNAME_RCVPWINF) ) != 0 )
				continue;
			csvOffset++;
		}

		if( csvCount == 11+csvOffset )	/* [20210128] Chg *//* [20240220] Chg */
		{
			/* BCP用SOCの取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (workData < 0) || (workData > 100) )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (BCPSoc). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->BCPSoc = workData;

/* [20210128] Del */

			/* 受電電力脱落負荷量 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData <= 0 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (dropOffLoadPower). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->dropOffLoadPower = workData;

			/* 制御不感帯 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )	/* [20230220] Chg */
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (deadZone). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->deadZone = workData;

/* [20210115] Chg St */
			/* 放電再開SoC相対値 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (workData <= 0) || (workData >= 100) )
/* [20210115] Chg Ed */
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (dischgRestartSoC). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->dischgRestartSoC = workData;

/* [20210115] Chg St */
			/* 充電再開SoC相対値 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (workData <= 0) || (workData >= 100) )
/* [20210115] Chg Ed */
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (chgRestartSoC). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->chgRestartSoC = workData;

			/* 受電電力平均時間 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( ((workData/100) <= 0 ) || ((workData/100) > AVE_CNT_MAX ))	/* [20260514] Chg */
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (rcvPowAveTime). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->rcvPowAveTime = workData;

			/* 負荷電力平均時間 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( ((workData/100) <= 0 ) || ((workData/100) > AVE_CNT_MAX ))	/* [20260514] Chg */
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (loadPowAveTime). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->loadPowAveTime = workData;

			/* 受電電力目標値変化量の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (rcvPowChangeAmt). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->rcvPowChangeAmt = workData;

			/* ピークカットの取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (rcvPowMaxDemand). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->rcvPowMaxDemand = workData;

			/* 最低受電電力の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
/* [20230529] Del */
			/* 逆潮流ありを考慮し、最低受電電力のチェックを撤廃 */
			ps_RCVPWINF->rcvPowMinDemand = workData;

/* [20240220] Add St */
			/* SoC制御不感帯の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (deadZoneSoC[%d]). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->deadZoneSoC = workData;
/* [20240220] Add Ed */

		}
		else if( csvCount == 3+csvOffset)
		{
			/* PV発電加算テーブル番号の取得 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( (workData <= 0) || (workData > PV_RCVPW_THR_NUM) )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (Index). File[%s] Line[%d] DAT[%d]=%d", csvFilePath, syoriCount, csvOffset, workData );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			setIdx = workData;

			/* 重複定義の確認 */
			if( setFlag[setIdx - 1] == 1 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (Index Dupulication Error). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				retCode = -1;
				break;
			}
			setFlag[setIdx - 1] = 1;


			/* 負荷電力閾値 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (loadPowLimit). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->s_pvAddPowInf[setIdx - 1].loadPowLimit = workData;


			/* PV発電加算値 */
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			if( workData < 0 )
			{
				sprintf( errMsg, "CSV-Format(RCVPWINF) Error (addPvPower). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}
			ps_RCVPWINF->s_pvAddPowInf[setIdx - 1].addPvPower = workData;

			/* カウント */
			cntAddPowInf++;
		}
		/* CSVデータ数が不正の場合 */
		else
		{
			sprintf( errMsg, "CSV-Format(RCVPWINF) Error (The number of data is incorrect). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
	}

	/* 閾値数を設定 */
	ps_RCVPWINF->pvAddPowInfNum = cntAddPowInf;

	return retCode;
}

int	func_OVRCHINF_Check( FILE *fp, int readType, struct st_OVRCHINF *ps_OVRCHINF, char * csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	memset( ps_OVRCHINF, 0, sizeof(struct st_OVRCHINF) );

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		csvOffset=0;
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;

		/* 未入力行（3文字以下）はスキップ */
		if( strlen( readBuff ) < 1) continue;

		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* PV出力抑制に関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_OVRCHINF, sizeof(PV_FNAME_OVRCHINF) ) != 0 )
				continue;
			csvOffset++;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 2+csvOffset )
		{
			sprintf( errMsg, "CSV-Format(OVRCHINF) Error (The number of data is incorrect). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}

		/* 脱落負荷量の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 1 )
		{
			sprintf( errMsg, "CSV-Format(OVRCHINF) Error (dropOffLoadPower). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_OVRCHINF->dropOffLoadPower = workData;

		/* PV発電再開SoCの取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData < 0 )
		{
			sprintf( errMsg, "CSV-Format(OVRCHINF) Error (dropOffLoadPower). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		ps_OVRCHINF->pvRestartSoc = workData;

	}

	return retCode;
}

int	func_CONFUPDATE_Check( FILE *fp, int *upserNo, int *updateType, char * csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int						csvOffset=0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						retCode = -1;				/* 戻り値 */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int						workData;					/* 数値変換用 */
	int						syoriCount = 0;				/* CSV処理カウンタ */

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		csvOffset = 0;
		/* CSV処理カウンタを＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;

		/* 未入力行（1文字以下）はスキップ */
		if( strlen( readBuff ) < 1) continue;

		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* デマンド制御情報に関する設定情報の場合、または */
		/* CEMS制御設定に関する設定情報の場合、または */
		/* 受電電力一定制御に関する設定情報の場合、または */
		/* 過充電回避制御に関する設定情報の場合 */
		if( ( strncmp( s_sepStr.data[0], PV_FNAME_DEMNDINF, sizeof(PV_FNAME_DEMNDINF) ) == 0 ) ||
			( strncmp( s_sepStr.data[0], PV_FNAME_CEMSPINF, sizeof(PV_FNAME_CEMSPINF) ) == 0 ) ||
			( strncmp( s_sepStr.data[0], PV_FNAME_RCVPWINF, sizeof(PV_FNAME_RCVPWINF) ) == 0 ) ||
			( strncmp( s_sepStr.data[0], PV_FNAME_OVRCHINF, sizeof(PV_FNAME_OVRCHINF) ) == 0 ) ||
			( strncmp( s_sepStr.data[0], PV_FNAME_BATTEINF, sizeof(PV_FNAME_BATTEINF) ) == 0 ) ||	/* [20230830] Add */
/* [20250807] Chg St */
			( strncmp( s_sepStr.data[0], PV_FNAME_PWPLNINF, sizeof(PV_FNAME_PWPLNINF) ) == 0 ) ||	/* [20210128] Add */
			( strncmp( s_sepStr.data[0], PV_FNAME_FCRSTINF, sizeof(PV_FNAME_FCRSTINF) ) == 0 ) ||	/* [20250807] Add */
			( strncmp( s_sepStr.data[0], PV_FNAME_FCRRPINF, sizeof(PV_FNAME_FCRRPINF) ) == 0 ) )	/* [20250807] Add */
/* [20250807] Chg Ed */
		{
			continue;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 2 )
		{
			sprintf( errMsg, "CSV-Format Error (The number of data is incorrect). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}

		/* ユーザ番号の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* ミツウロコではユーザ番号は使わないので、チェック処理を削除 */
		//if( workData < 1 )
		//{
		//	sprintf( errMsg, "CSV-Format Error (userNo). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
		//	commfunc_syslog_msgset( "FILEMAKE", errMsg );
		//	retCode = -1;
		//	break;
		//}
		//*upserNo = workData;

		/* 設定変更種別の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
/* [20230125] Chg St */
		if( ( workData != PV_UPDATECONF_TYPE_PARA ) &&
		    ( workData != PV_UPDATECONF_TYPE_PLAN ) )
/* [20230125] Chg Ed */
		{
			sprintf( errMsg, "CSV-Format Error (updateType). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		*updateType = workData;

		retCode = 0;

	}

	return retCode;
}

/* [20210128] Add St */
int	func_PWPLNINF_Check( FILE *fp, int readType, struct st_PWPLNINF *ps_PWPLNINF, char * csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int						dateCnt[2];					/* 登録日カウント */
	int						loopCnt1;
	int						planIdx;					/* 需給計画（連系・自立） */
	int						planType;					/* 計画種別 */
	int						*pData;						/* 計画データポインタ */
	int						planDay;					/* 計画日 */
	int						statPlanDay[PV_POWPLAN_IDX_MAX];	/* 計画有無*/
	int						rcvPlanDay[PV_POWPLAN_IDX_MAX];	/* 受電計画日*/
	int						bttPlanDay[PV_POWPLAN_IDX_MAX];	/* 蓄電池計画日*/
	int						socPlanDay[PV_POWPLAN_IDX_MAX];	/* SoC計画日*/
	int						bcpPlanDay[PV_POWPLAN_IDX_MAX];	/* BCP使用有無計画日*/
/* [20230529] Add St */
	int						acUsePlanDay[PV_POWPLAN_IDX_MAX];	/* 需給調整市場使用 */
	int						recSocPlanDay[PV_POWPLAN_IDX_MAX];	/* 需給調整市場必要電力量 */
	int						chgPlanDay[PV_POWPLAN_IDX_MAX];		/* 充電電力上限 */
	int						disPlanDay[PV_POWPLAN_IDX_MAX];		/* 放電電力上限 */
/* [20230529] Add St */
/* [20240220] Add St */
	int						peakcutPlanDay[PV_POWPLAN_IDX_MAX];	/* ピークカット超過有無 */
	int						cmUsePlanDay[PV_POWPLAN_IDX_MAX];	/* 容量市場使用 */
/* [20240220] Add St */
/* [20250807] Add St */
	int						fcrUsePlanDay[PV_POWPLAN_IDX_MAX];		/* 一次使用有無 *//* [20250909] Add */
	int						fcrDltKwPlanDay[PV_POWPLAN_IDX_MAX];	/* 一次調整力約定△kW */
/* [20250807] Add Ed */

	memset( ps_PWPLNINF, 0, sizeof(struct st_PWPLNINF)*2 );
	memset( dateCnt, 0, sizeof(dateCnt));

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		csvOffset=0;
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;

		/* 未入力行（3文字以下）はスキップ */
		if( strlen( readBuff ) < 3) continue;

		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* 需給バランス調整に関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_PWPLNINF, sizeof(PV_FNAME_PWPLNINF) ) != 0 )
				continue;
			csvOffset++;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 50+csvOffset)
		{
			sprintf( errMsg, "func_PWPLNINF_Check Error (The number of data is incorrect). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}

		/* 連系・自立 */
		//workData = atoi( s_sepStr.data[csvOffset] );
		//csvOffset++;
		//if( ( workData != PV_POWPLAN_IDX_ONGRID ) && ( workData != PV_POWPLAN_IDX_OFFGRID ) )
		//{
		//	sprintf( errMsg, "func_PWPLNINF_Check Error (plan idx:%d). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
		//	commfunc_syslog_msgset( "FILEMAKE", errMsg );
		//	printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
		//	retCode = -1;
		//	break;
		//}
		//planIdx = workData;
		/* 連系固定 */
		planIdx = PV_POWPLAN_IDX_ONGRID;

		/* スケジュール年月日の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 2020.1.1より過去は異常とする */
		if( workData < 20200101 )
		{
			sprintf( errMsg, "func_PWPLNINF_Check Error (plan date:%d). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		/* 受電電力計画値最大設定日数を超える場合 */
		if( (dateCnt[planIdx]+1) > PV_POWPLN_MAX_DAY )
		{
			/* 読み飛ばす */
			sprintf( errMsg, "func_PWPLNINF_Check Error (plan date MAXd). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			break;
		}
		planDay = workData;

		/* 計画種別 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		if( workData == PV_POWPLAN_TYPE_STAT )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].plnStat;
			statPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_RCVPOW )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].rcvPow;
			rcvPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_BTTPOW )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].bttPow;
			bttPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_SOC )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].bttSoC;
			socPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_USE_BCP )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].useBcp;
			bcpPlanDay[planIdx] = planDay;
		}
/* [20230529] Add St */
		else if( workData == PV_POWPLAN_TYPE_USE_AC )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].acUse;
			acUsePlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_REC_SOC )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].acSoC;
			recSocPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_CHG_LIMIT )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].avlChgPow;
			chgPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_DIS_LIMIT )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].avlDischgPow;
			disPlanDay[planIdx] = planDay;
		}
/* [20230529] Add Ed */
/* [20240220] Add St */
		else if( workData == PV_POWPLAN_TYPE_OVER_PEAKCUT )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].peakCutStat;
			peakcutPlanDay[planIdx] = planDay;
		}
		else if( workData == PV_POWPLAN_TYPE_USE_CM )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].capMktUse;
			cmUsePlanDay[planIdx] = planDay;
		}
/* [20240220] Add Ed */
/* [20250807] Add St */
/* [20250909] Add St */
		else if( workData == PV_POWPLAN_TYPE_FCR_USE )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].fcrUse;
			fcrUsePlanDay[planIdx] = planDay;
		}
/* [20250909] Add Ed */
		else if( workData == PV_POWPLAN_TYPE_FCR_DLTKW )
		{
			pData = ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].fcrDltKw;
			fcrDltKwPlanDay[planIdx] = planDay;
		}
/* [20250807] Add Ed */
		else
		{
			sprintf( errMsg, "func_PWPLNINF_Check Error (plan idx:%d). File[%s] Line[%d] DAT[%d]", workData, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
			retCode = -1;
			break;
		}
		planType = workData;

		/* 受電電力計画値の取得 */
		for( loopCnt1 = 0; loopCnt1 < PV_POWPLN_DAY_PERIOD; loopCnt1++ )
		{
			workData = atoi( s_sepStr.data[csvOffset] );
			csvOffset++;
			/* ミツウロコでは負の数も許容 */
			//if( workData < 0)
			//{
			//	workData = -1;
			//}
			
			*pData = workData;
			pData++;
		}

		/* フォーマット上、一番最初の計画種別を使用 */
		if( planType == PV_POWPLAN_TYPE_STAT )
		{
			/* 計画日を設定 */
			ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].plnDay = planDay;
		}
		else
		{
			/* 計画日が異なる場合は、フォーマットが合っていない */
			if( ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].plnDay != planDay )
			{
				sprintf( errMsg, "func_PWPLNINF_Check Error (planIdx=%d, planType=%d, planDay=%d != %d). Line[%d]", planIdx, planType, planDay, ps_PWPLNINF[planIdx].s_powDayPln[dateCnt[planIdx]].plnDay, syoriCount );
				commfunc_syslog_msgset( "FILEMAKE", errMsg );
				printf("FILEMAKE : InputFile[%s] CSV-Data error", csvFilePath );
				retCode = -1;
				break;
			}

/* [20230529] Chg St */
			if( ( statPlanDay[planIdx] == rcvPlanDay[planIdx] ) &&
				( statPlanDay[planIdx] == bttPlanDay[planIdx] )  &&
				( statPlanDay[planIdx] == socPlanDay[planIdx] )  &&
				( statPlanDay[planIdx] == bcpPlanDay[planIdx] )  &&
				( statPlanDay[planIdx] == acUsePlanDay[planIdx] )  &&
				( statPlanDay[planIdx] == recSocPlanDay[planIdx] )  &&
				( statPlanDay[planIdx] == chgPlanDay[planIdx] )   &&
				( statPlanDay[planIdx] == peakcutPlanDay[planIdx] )   &&	/* [20240220] Add */
				( statPlanDay[planIdx] == cmUsePlanDay[planIdx] )   &&		/* [20240220] Add */
/* [20250807] Add St */
				( statPlanDay[planIdx] == fcrDltKwPlanDay[planIdx] )   &&	/* [20250807] Add */
				( statPlanDay[planIdx] == fcrUsePlanDay[planIdx] )   &&		/* [20250909] Add */
/* [20250807] Add Ed */
				( statPlanDay[planIdx] == disPlanDay[planIdx] ) )
/* [20230529] Chg Ed */
			{
				/* 登録日をカウントアップ */
				dateCnt[planIdx]++;
				/* 登録日数を格納 */
				ps_PWPLNINF[planIdx].plnDayNum = dateCnt[planIdx];
			}
		}
	}

	return retCode;
}
/* [20210128] Add Ed */
/* [20250807] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力パラメータ設定情報チェック
 *
 * 機能：
 * 		引数 *fp で指定されたファイルを解析し、一次調整力パラメータ設定情報の
 *      内容チェックを行う。
 * 戻り値：
 * 		0：正常、-1：データ異常
 */
int	func_FCRSTINF_Check( FILE *fp, int readType, struct st_FCRSTINF *ps_FCRSTINF, char *csvFilePath )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	memset( ps_FCRSTINF, 0, sizeof(struct st_FCRSTINF) );

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		/* 読込データオフセットを初期位置設定 */
		csvOffset = 0;
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;
		/* 未入力行（9文字以下）はスキップ */
		if( strlen( readBuff ) < 10) continue;
		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* 一次調整力パラメータ設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_FCRSTINF, sizeof(PV_FNAME_FCRSTINF) ) != 0 )
				continue;
			csvOffset++;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 10+csvOffset )/* [20251212] 充電応動有無追加 Chg */
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (Data is not 10). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );/* [20251215] メッセージ修正 Chg */
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 基準周波数の取得 */
		workData = (int)atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 基準周波数が50以外かつ60以外 */
		if( ( workData != PV_FCRST_FREQ_50 ) && ( workData != PV_FCRST_FREQ_60 ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (Freq is not 50 and 60). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 基準周波数の設定 */
		ps_FCRSTINF->baseFreq = workData;

		/* 調定率の取得 */
		workData = (int)atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 調定率が0.1より小さい、または5.0より大きい */
		if( ( workData < PV_FCRST_VRRATE_LOWER ) || ( workData > PV_FCRST_VRRATE_UPPER ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (variationRate is not in range). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 調定率の設定 */
		ps_FCRSTINF->variationRate = workData;

		/* 不感帯の取得 */
		workData = (int)atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 不感帯が0より小さい
			または((基準周波数50かつ0.01より大きい) または(基準周波数60かつ0.012より大きい))*/
		if( ( workData < PV_FCRST_DEADBAND_LOWER ) || 
			( ( ( ps_FCRSTINF->baseFreq == PV_FCRST_FREQ_50 ) && ( workData > PV_FCRST_DEADBAND_50_UPPER ) ) || 
			  ( ( ps_FCRSTINF->baseFreq == PV_FCRST_FREQ_60 ) && ( workData > PV_FCRST_DEADBAND_60_UPPER ) ) ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (deadBand is not in range). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 不感帯の設定 */
		ps_FCRSTINF->deadBand = workData;
		/* 不感帯(下限)の設定 */
		ps_FCRSTINF->deadBandLower = ( ps_FCRSTINF->baseFreq ) - workData;
		/* 不感帯(上限)の設定 */
		ps_FCRSTINF->deadBandUpper = ( ps_FCRSTINF->baseFreq ) + workData;

		/* 異常判定周波数の取得 */
		workData = (int)atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 異常判定周波数が通常以外かつ北海道以外 */
		if( ( workData != PV_FCRST_FREQDEVTSHRSLD_NOMAL ) && ( workData != PV_FCRST_FREQDEVTSHRSLD_HOKKAIDO ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (freqDevtThrsld is not nomal and hokkaido). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 異常判定周波数の設定 */
		ps_FCRSTINF->freqDevtThrsld = workData;
		/* 異常判定周波数(下限)の設定 */
		ps_FCRSTINF->freqDevtThrsldLower = ( ps_FCRSTINF->baseFreq ) - workData;
		/* 異常判定周波数(上限)の設定 */
		ps_FCRSTINF->freqDevtThrsldUpper = ( ps_FCRSTINF->baseFreq ) + workData;

		/* 定格出力の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 定格出力が下限より小さい、または定格出力が上限より大きい */
		if( ( workData < PV_FCRST_RATEDPOWER_LOWER ) || ( workData > PV_FCRST_RATEDPOWER_UPPER ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (ratedPower is not in range). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 定格出力の設定 */
		ps_FCRSTINF->ratedPower = workData;

		/* 異常時出力継続時間の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 異常判出力継続時間が下限より小さい、または異常時出力継続時間が上限より大きい */
		if( ( workData < PV_FCRST_FAULTDURATION_LOWER ) || ( workData > PV_FCRST_FAULTDURATION_UPPER ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (faultDuration is not in range). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 異常時出力継続時間の設定 */
		ps_FCRSTINF->faultDuration = workData;

		/* 放電時出力変化量の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 放電時出力変化量が下限より小さい、または放電時出力変化量が上限より大きい */
		if( ( workData < PV_FCRST_DISCHGPOWERCHGAMT_LOWER ) || ( workData > PV_FCRST_DISCHGPOWERCHGAMT_UPPER ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (dischgPowerChgAmount is not in range). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 放電時出力変化量の設定 */
		ps_FCRSTINF->dischgPowerChgAmount = workData;

		/* 充電時出力変化量の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 充電時出力変化量が下限より小さい、または充電時出力変化量が上限より大きい */
		if( ( workData < PV_FCRST_CHGPOWERCHGAMT_LOWER ) || ( workData > PV_FCRST_CHGPOWERCHGAMT_UPPER ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (chgPowerChgAmount is not in range). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 充電時出力変化量の設定 */
		ps_FCRSTINF->chgPowerChgAmount = workData;

		/* 不感帯応動種別の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 不感帯応動種別が下限より小さい、または充電時出力変化量が上限より大きい */
		if( ( workData != PV_FCRST_DEADBAND_TYPE_NONE ) &&
			( workData != PV_FCRST_DEADBAND_TYPE_ONE ) &&
			( workData != PV_FCRST_DEADBAND_TYPE_TWO ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (deadBandResType does not match any of them). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 不感帯応動種別の設定 */
		ps_FCRSTINF->deadBandResType = workData;

/* [20251212] 充電応動有無の対応 Add St */
		/* 充電応動有無の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 充電応動有無が無効、有効以外 */
		if( ( workData != PV_FCRST_CHG_RESPONSIVE_DISABLED ) &&
			( workData != PV_FCRST_CHG_RESPONSIVE_ENABLED ) )
		{
			sprintf( errMsg, "CSV-Format(FCRSTINF) Error (chgResponsiveEnabled does not match any of them). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* 充電応動有無の設定 */
		ps_FCRSTINF->chgResponsiveEnabled = workData;
/* [20251212] 充電応動有無の対応 Add Ed */
	}

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力レポート蓄積項目情報チェック
 *
 * 機能：
 * 		引数 *fp で指定されたファイルを解析し、一次調整力レポート蓄積項目情報の
 *      行内容チェックを行う。
 * 戻り値：
 * 		0：正常、-1：データ異常
 */
int	func_FCRRPINF_Check( FILE *fp, int readType, struct st_FCRRPINF *ps_FCRRPINF, char *csvFilePath, struct st_PVSYSINF *ps_PVSYSINF, struct st_TMDATINF *ps_TMDATINF )
{
	char					readBuff[4096];				/* CSVファイル読み込みバッファ */
	int						csvOffset=0;
	int						workData;					/* 数値変換用 */
	int						retCode = 0;
	int						csvCount;					/* CSVデータ数（デリミタ分割数） */
	int						syoriCount = 0;				/* CSV処理カウンタ */
	struct st_cmn_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int						mKind;						/* 装置種別 */
	int						mNumb;						/* 装置番号 */
	int						tmNumb;						/* 計測番号 */
	int						maxTmNumb;					/* 最大計測数 */
	int						maxMNumb;					/* 最大装置数 */
	int						linkBase;					/* 計測情報基本リンク */
	char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int						dataCount = 0;				/* 有効なレポート蓄積項目数 */

	memset( ps_FCRRPINF, 0, sizeof(struct st_FCRRPINF) );

	/* CSVファイルの終端まで繰り返す */
	while( fgets( readBuff, 4096, fp) != NULL )
	{
		/* 読込データオフセットを初期位置設定 */
		csvOffset = 0;
		/* CSV処理カウンタ＋１ */
		syoriCount++;
		/* コメント行はスキップ */
		if( readBuff[0] == '#' ) continue;
		/* 未入力行（9文字以下）はスキップ */
		if( strlen( readBuff ) < 10) continue;
		/* CSVデータの分割 */
		csvCount = func_cmn_sep_data( readBuff, &s_sepStr );

		/* 更新の場合 */
		if( readType == PV_CSVTYPE_UPD )
		{
			/* 一次調整力レポート蓄積項目情報に関する設定情報ではない場合 */
			if( strncmp( s_sepStr.data[0], PV_FNAME_FCRRPINF, sizeof(PV_FNAME_FCRRPINF) ) != 0 )
				continue;
			csvOffset++;
		}

		/* CSVデータ数が不正の場合 */
		if( csvCount != 4+csvOffset)
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (Data is not 4). File[%s] LINE[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* データ番号の取得 */
		workData = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* データ番号が範囲外 */
		if( ( workData < PV_FCRRP_DATANUMB_LOWRE ) || ( workData > PV_FCRRP_DATANUMB_UPPER ) )/* [20250930] Chg */
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (DataNumb is not in area). File[%s] LINE[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			break;
		}
		/* データ番号の設定 */
		ps_FCRRPINF->s_fcrrpData[dataCount].dataNumb = workData;

		/* 装置種別の取得 */
		mKind = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 装置種別が範囲外 */
		if( (mKind < 0) || (mKind > PV_MKIND_LAST) )
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (mKind:%d). File[%s] Line[%d] DAT[%d]", mKind, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			printf("%s\n", errMsg);
			break;
		}
		/* 装置種別の設定 */
		ps_FCRRPINF->s_fcrrpData[dataCount].mKind = mKind;

		/* 装置が実装されていない場合 */
		if( ps_PVSYSINF->s_machineInfo[mKind].machineCnt == 0 )
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (mKind machineCnt is 0). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			printf("%s\n", errMsg);
			break;
		}

		/* 装置の最大情報数の取得 */
		maxMNumb  = ps_PVSYSINF->s_machineInfo[mKind].maxMachine;
		maxTmNumb = ps_PVSYSINF->s_machineInfo[mKind].maxTm;

		/* 装置番号の取得 */
		mNumb = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 装置番号が範囲外 */
		if( (mNumb < 1) || (mNumb > maxMNumb) )
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (mNumb:%d). File[%s] Line[%d] DAT[%d]", mNumb, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			printf("%s\n", errMsg);
			break;
		}
		/* 装置番号の設定 */
		ps_FCRRPINF->s_fcrrpData[dataCount].mNumb = mNumb;

		/* 計測番号の取得 */
		tmNumb = atoi( s_sepStr.data[csvOffset] );
		csvOffset++;
		/* 計測番号が範囲外 */
		if( (tmNumb < 1) || (tmNumb > maxTmNumb) )
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (tmNumb:%d). File[%s] Line[%d] DAT[%d]", tmNumb, csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			printf("%s\n", errMsg);
			break;
		}

		/* 装置の最大情報数の取得 */
		linkBase  = ps_PVSYSINF->s_machineInfo[mKind].tmIfLink + maxTmNumb * (mNumb - 1) + (tmNumb - 1);
		
		/* 計測値が実装されているか */
		if( ps_TMDATINF[linkBase].statFlag == 0 )
		{
			sprintf( errMsg, "CSV-Format(FCRRPINF) Error (TM Stat is 0). File[%s] Line[%d] DAT[%d]", csvFilePath, syoriCount, csvOffset );
			commfunc_syslog_msgset( "FILEMAKE", errMsg );
			retCode = -1;
			printf("%s\n", errMsg);
			break;
		}

		/* 計測番号の設定 */
		ps_FCRRPINF->s_fcrrpData[dataCount].tmNumb = tmNumb;

		/* 計測情報リンク情報（格納位置情報）を設定 */
		ps_FCRRPINF->s_fcrrpData[dataCount].tmIfLinkPos = linkBase;	/* [20260515] Add */


		/* 有効データ数のカウント */
		dataCount++;
		ps_FCRRPINF->dataMax = dataCount;
	}

	return retCode;
}
/* [20250807] Add Ed */

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
int func_cmn_sep_data( char *pc_data, struct st_cmn_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_sepStr, 0, sizeof(struct st_cmn_sepStr) );

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


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 単位種別・乗率確認処理
 *
 * 機能：
 * 		引数 uKind で指定された単位種別と、引数 checkKind の単位種別が同じ種別
 *		の単位であることを確認する。
 *		また、引数 expData と
 * 戻り値：
 * 		 0：正常
 * 		-1：異常（uKind の単位種別が合計演算に対応していない）
 *		-2：異常（cKind の単位種別が合計演算に対応していない）
 *		-3：異常（uKind と cKind の単位種別が異なる種別の単位の場合）
 */
/* [20250807] Chg St */
int commfunc_sumInfChk( int uKind, int cKind, int eData, int *eMin, int *uMin, int sumMaxCnt )
/* [20250807] Chg Ed */
{
	int						uData;					/* チェック処理用データ */

/* [20250807] Chg St */
	/* 集約対象が複数のものだけチェック */
	if( sumMaxCnt > 1 )
	{
		/* 処理不能な単位種別の確認 */
		if( commfunc_chkUKind( uKind ) != 0 )
			return( -1 );
		
		/* 処理不能な単位種別の確認 */
		if( commfunc_chkUKind( cKind ) != 0 )
			return( -2 );
	}
/* [20250807] Chg Ed */

	/* 乗率 eData が eMin より小さい場合 */
	if( eData < *eMin )
		*eMin = eData;

	/* 単位種別の調整値が uMin の調整値よりも小さい場合 */
	uData = commfunc_chkUChosei( cKind );
	if( uData < *uMin )
		*uMin = uData;

	/* 単位種別の確認 */
	if( commfunc_chkUComp( uKind, cKind ) != 0 )
		return( -3 );

	/* 戻り値（正常） */
	return( 0 );
	
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 単位種別確認（合計演算対応確認）
 *
 * 機能：
 * 		引数 uKind で指定された単位種別が合計演算可能な単位か確認する
 * 戻り値：
 * 		 0：正常
 * 		-1：異常（uKind の単位種別が合計演算に対応していない）
 */
int	commfunc_chkUKind( int uKind )
{
	switch( uKind )
	{
//		case PV_TM_UKIND_HZ:		/* Hz *//* [20250806] Del */
		case PV_TM_UKIND_C:			/* ℃ */
		case PV_TM_UKIND_KW_M2:		/* kW/m2 */
//		case PV_TM_UKIND_PERCENT:	/* ％ */
		case PV_TM_UKIND_WM2:		/* W/m2 */
//		case PV_TM_UKIND_KHZ:		/* kHz *//* [20250806] Del */
		case PV_TM_UKIND_MIN_1:		/* min-1 */
		case PV_TM_UKIND_CM:		/* cm */
		case PV_TM_UKIND_M:			/* m */
		case PV_TM_UKIND_MPA:		/* MPa */
		case PV_TM_UKIND_OHM:		/* Ω */
			return( -1 );
			break;
		default:
			break;
	}
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 単位種別確認（類似種別確認）
 *
 * 機能：
 * 		引数 uKind で指定された単位種別と引数 cKind で指定された単位種別が同類で
 *		あるか確認する。（接頭語が異なるだけかを確認する）
 * 戻り値：
 * 		 0：同類
 * 		-1：異なる単位の場合
 */
int commfunc_chkUComp( int uKind, int cKind )
{
	switch( uKind )
	{
		case PV_TM_UKIND_WH:		/*  1 (Wh) */
		case PV_TM_UKIND_KWH:		/*  2 (kWh) */
		case PV_TM_UKIND_MWH:		/* 26 (MWh) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_WH ) || ( cKind == PV_TM_UKIND_KWH ) || ( cKind == PV_TM_UKIND_MWH ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_VARH:		/*  3 (varh) */
		case PV_TM_UKIND_KVARH:		/*  4 (kvarh) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_VARH ) || ( cKind == PV_TM_UKIND_KVARH ) )
				return( 0 );
			else
				return( -1 );
			break;
			break;
		case PV_TM_UKIND_W:			/*  5 (W) */
		case PV_TM_UKIND_KW:		/*  6 (kW) */
		case PV_TM_UKIND_MW:		/* 27 (MW) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_W ) || ( cKind == PV_TM_UKIND_KW ) || ( cKind == PV_TM_UKIND_MW ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_A:			/*  7 (A) */
		case PV_TM_UKIND_KA:		/*  8 (kA) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_A ) || ( cKind == PV_TM_UKIND_KA ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_V:			/*  9 (V) */
		case PV_TM_UKIND_KV:		/* 10 (kV) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_V ) || ( cKind == PV_TM_UKIND_KV ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_KW_M2:		/* 13 (kW/m2) */
		case PV_TM_UKIND_WM2:		/* 25 (W/m2) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_KW_M2 ) || ( cKind == PV_TM_UKIND_WM2 ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_MAH:		/* 28 (mAh) */
		case PV_TM_UKIND_AH:		/* 29 (Ah) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_MAH ) || ( cKind == PV_TM_UKIND_AH ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_VAR:		/* 38 (var) */
		case PV_TM_UKIND_KVAR:		/* 21 (kvar) */
		case PV_TM_UKIND_MVAR:		/* 36 (Mvar) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_VAR ) || ( cKind == PV_TM_UKIND_KVAR ) || ( cKind == PV_TM_UKIND_MVAR ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_CM:		/* 33 (cm) */
		case PV_TM_UKIND_M:			/* 34 (m) */
		case PV_TM_UKIND_MM:		/* 37 (mm) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_MM ) || ( cKind == PV_TM_UKIND_CM ) || ( cKind == PV_TM_UKIND_M ) )
				return( 0 );
			else
				return( -1 );

		case PV_TM_UKIND_VA:		/* 39 (VA) */
		case PV_TM_UKIND_KVA:		/* 22 (kVA) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_VA ) || ( cKind == PV_TM_UKIND_KVA ) )
				return( 0 );
			else
				return( -1 );
			break;

/* [20250806] Chg St */
		case PV_TM_UKIND_HZ:		/* 11 (Hz) */
		case PV_TM_UKIND_KHZ:		/* 30 (kHz) */
			/* 接頭語が異なるだけの場合 */
			if( ( cKind == PV_TM_UKIND_HZ ) || ( cKind == PV_TM_UKIND_KHZ ) )
				return( 0 );
			else
				return( -1 );
			break;

		case PV_TM_UKIND_C:			/* 12 (℃) */
		case PV_TM_UKIND_S:			/* 14 (秒) */
		case PV_TM_UKIND_KAI:		/* 15 (回) */
		case PV_TM_UKIND_KWH_KAI:	/* 16 (kWh/回) */
		case PV_TM_UKIND_PERCENT:	/* 17 (%) */
		case PV_TM_UKIND_MA:		/* 18 (mA) */
		case PV_TM_UKIND_VRMS:		/* 19 (Vrms) */
		case PV_TM_UKIND_ARMS:		/* 20 (Arms) */
		case PV_TM_UKIND_HOUR:		/* 23 (時) */
		case PV_TM_UKIND_MIN:		/* 24 (分) */
		case PV_TM_UKIND_MIN_1:		/* 31 (min-1) */
		case PV_TM_UKIND_M3_S:		/* 32 (m3/s) */
		case PV_TM_UKIND_MPA:		/* 35 (MPa) */
		case PV_TM_UKIND_NUM:		/* 41 (台) */
		case PV_TM_UKIND_WS:		/* 42 (W/s) */
		case PV_TM_UKIND_MV:		/* 43 (mV) *//* [20251103] Add */
/* [20250806] Chg Ed */

			/* 単位種別が異なる場合 */
			if( uKind == cKind )
				return( 0 );
			else
				return( -1 );
			break;

		default:
			return( -1 );
			break;
	}
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 単位種別の相対乗率確認
 *
 * 機能：
 * 		引数 uKind で指定された単位種別の接頭語による装置内の相対乗率を求める
 *		例）
 *		＜電圧の場合＞
 *		　システムで使用する単位に Wh,kWh,MWh が存在する。Wh がシステム内で最小
 *		　単位となるので、これを基準 0 とし、 戻り値はそれぞれ 0,3,6 となる
 *		＜電流の場合＞
 *		　システムで使用する単位に mA,A,kA が存在する。mA がシステム内で最小単位
 *		　となるので、これを基準 0 とし、 戻り値はそれぞれ 0,3,6 となる
 * 戻り値：
 * 		 n：乗率ｎ（１０のｎ乗）
 */
int commfunc_chkUChosei( int uKind )
{
	switch( uKind )
	{
		case PV_TM_UKIND_WH:		/*  1 (Wh) */
			return( 0 );
			break;
		case PV_TM_UKIND_KWH:		/*  2 (kWh) */
			return( 3 );
			break;
		case PV_TM_UKIND_MWH:		/* 26 (MWh) */
			return( 6 );
			break;

		case PV_TM_UKIND_VARH:		/*  3 (varh) */
			return( 0 );
			break;
		case PV_TM_UKIND_KVARH:		/*  4 (kvarh) */
			return( 3 );
			break;

		case PV_TM_UKIND_W:			/*  5 (W) */
			return( 0 );
			break;
		case PV_TM_UKIND_KW:		/*  6 (kW) */
			return( 3 );
			break;
		case PV_TM_UKIND_MW:		/* 27 (MW) */
			return( 6 );
			break;

		case PV_TM_UKIND_MA:		/* 18 (mA) */
			return( 0 );
			break;
		case PV_TM_UKIND_A:			/*  7 (A) */
			return( 3 );
			break;
		case PV_TM_UKIND_KA:		/*  8 (kA) */
			return( 6 );
			break;

		case PV_TM_UKIND_V:			/*  9 (V) */
			return( 0 );
			break;
		case PV_TM_UKIND_KV:		/* 10 (kV) */
			return( 3 );
			break;

		case PV_TM_UKIND_HZ:		/* 11 (Hz) */
			return( 0 );
			break;

		case PV_TM_UKIND_KHZ:		/* 30 (kHz) */
			return( 3 );
			break;

		case PV_TM_UKIND_C:			/* 12 (℃) */
			return( 0 );
			break;

		case PV_TM_UKIND_WM2:		/* 25 (W/m2) */
			return( 0 );
			break;
		case PV_TM_UKIND_KW_M2:		/* 13 (kW/m2) */
			return( 3 );
			break;

		case PV_TM_UKIND_S:			/* 14 (秒) */
			return( 0 );
			break;

		case PV_TM_UKIND_KAI:		/* 15 (回) */
			return( 0 );
			break;

		case PV_TM_UKIND_KWH_KAI:	/* 16 (kWh/回) */
			return( 3 );
			break;

		case PV_TM_UKIND_PERCENT:	/* 17 (%) */
			return( 0 );
			break;

		case PV_TM_UKIND_VRMS:		/* 19 (Vrms) */
			return( 0 );
			break;

		case PV_TM_UKIND_ARMS:		/* 20 (Arms) */
			return( 0 );
			break;

		case PV_TM_UKIND_KVAR:		/* 21 (kvar) */
			return( 3 );
			break;

		case PV_TM_UKIND_KVA:		/* 22 (kVA) */
			return( 3 );
			break;

		case PV_TM_UKIND_HOUR:		/* 23 (時) */
			return( 0 );
			break;

		case PV_TM_UKIND_MIN:		/* 24 (分) */
			return( 0 );
			break;

		case PV_TM_UKIND_MAH:		/* 28 (mAh) */
			return( 0 );
			break;
		case PV_TM_UKIND_AH:		/* 29 (Ah) */
			return( 3 );
			break;

		case PV_TM_UKIND_MIN_1:		/* 31 (min-1) */
			return( 0 );
			break;

		case PV_TM_UKIND_M3_S:		/* 32 (m3/s) */
			return( 0 );
			break;

		case PV_TM_UKIND_MM:		/* 37 (mm) */
			return( 0 );
			break;

		case PV_TM_UKIND_CM:		/* 33 (cm) */
			return( 1 );
			break;

		case PV_TM_UKIND_M:			/* 34 (m) */
			return( 3 );
			break;

		case PV_TM_UKIND_MPA:		/* 35 (MPA) */
			return( 0 );
			break;

		case PV_TM_UKIND_MVAR:		/* 36 (MVAR) */
			return( 6 );
			break;

		case PV_TM_UKIND_VAR:		/* 38 (var) */
			return( 0 );
			break;

		case PV_TM_UKIND_VA:		/* 39 (VA) */
			return( 0 );
			break;

		case PV_TM_UKIND_NUM:		/* 41 (台) */
			return( 0 );
			break;

		case PV_TM_UKIND_WS:		/* 42 (W/s) */
			return( 0 );
			break;

		case PV_TM_UKIND_MV:		/* 43 (mV) *//* [20251103] Add */
			return( 0 );
			break;

		default:
			return( 0 );
			break;
	}
	return( 0 );
}


/* [20210112] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 制御設定情報取得処理
 *
 * 機能：
 *		設定情報から下記情報を取得する
 *		・蓄電システムの装置番号
 *		・マルチメータの装置情報（装置種別,装置番号,
 * 		プロセスの異常終了をシステム監視ファイルに登録し、ファイルリソースを開放
 * 		する。
 * 戻り値：
 * 		 1：パラメータ取得
 * 		 0：ファイル更新なし
 *		-1：設定ファイル読み込みエラー
 *		-2：パラメータ指定異常
 *		-3：パラメータで指定した装置・計測情報が未実装
 */
int	commfunc_file_info_get( char *filePath, time_t *checkTime, char *srchStr, char *getStr )
{
	FILE					*fp;							/* ファイルディスクリプタ */
	char					readBuff[2048];					/* リードバッファ */
	int						equCount;						/* パラメータ分割数 */
	struct st_cmn_sepStr	s_equStr;						/* データ分割用 */
	int						retCode=COM_PARAM_GET_NOTFOUND;	/* 戻り値 *//* 初期値は該当なし */
	struct stat				statBuff;						/* ファイル状態 */

	/* 設定ファイルが存在しない場合 */
	if( access( filePath, R_OK ) != 0 )
	{
		sprintf( getStr, "File Not Found. Control Config File [ %s ]", filePath );
		retCode = COM_PARAM_GET_NOFILE;
		return retCode;
	}
	/* ファイル状態の取得に失敗 */
	if( stat( filePath, &statBuff ) != 0 )
	{
		sprintf( getStr, "Configuration File Access Error!!" );
		retCode = COM_PARAM_GET_IOERR;
		return retCode;
	}

	/* チェック時刻が設定されている場合 */
	if( checkTime != NULL )
	{
		/* ファイルが変更されていない場合 */
		if( *checkTime == statBuff.st_mtime )
		{
			retCode = COM_PARAM_GET_NOUPDATE;
			return retCode;
		}
		else
		{
			/* ファイル時刻を更新 */
			*checkTime = statBuff.st_mtime;
		}
	}

	/* CSVファイルオープン */
	if( (fp = fopen(filePath, "r")) != NULL)
	{
		/* 設定ファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;

			/* パラメータ確認 */
			equCount = func_cmn_sepequ_data( readBuff, &s_equStr );

			/* パラメータ数が２以外の場合 */
			if( equCount != 2 )
				continue;

			/* 検索文字と一致する場合 */
			if( strncmp( s_equStr.data[0], srchStr, strlen(srchStr)+1 ) == 0 )
			{
				sprintf(getStr, "%s", s_equStr.data[1]);
				retCode = COM_PARAM_GET_OK;
				break;
			}
		}
		/* ファイルをクローズ */
		fclose(fp);
	}
	else
	{
		sprintf(getStr, "File Open Error. File:%s\n", filePath );
		retCode = COM_PARAM_GET_IOERR;
	}

	if( retCode ==  COM_PARAM_GET_NOTFOUND )
	{
		sprintf(getStr, "\"%s\" is not found.", srchStr);
	}
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * パラメータ分割処理
 *
 * 機能：
 *		引数 pc_data で指定された文字列を"＝"で区切り、ps_sepStr に格納する。
 * 戻り値：
 *		"＝"で区切られたデー多数を返す。
 */
int func_cmn_sepequ_data( char *pc_data, struct st_cmn_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_sepStr, 0, sizeof(struct st_cmn_sepStr) );

	pt = pc_data;
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		if( *pt == '=' )
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
/* [20210112] Add Ed */

/* [20230905] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * アップデートファイル ログ出力処理
 *
 * 機能：
 *		
 *
 * 入力：
 *
 * 出力：
 *
 */
int commfunc_updateconf_logout( char *pc_payload )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
	unsigned char			*pt1;					/* ポインタ */
	unsigned char			*pt2;					/* ポインタ */
	int						fd;						/* ファイルディスクリプタ */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */
	struct tm				localTime;				/* 年月日時分秒 */
	struct timeval			nowtime;
	int						logMode = PV_LOG_OFF;	/* ログ出力 */


	gettimeofday( &nowtime, NULL );
	localtime_r( &nowtime.tv_sec, &localTime );

	/* 出力用ログファイル名の設定 */
	sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_UPDATECNF );

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
		for(loopCnt1 = PV_LOG_STOCK_NUM ; loopCnt1 > 1 ; loopCnt1-- )	/* [20230902] Chg */
		{
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_UPDATECNF, loopCnt1 - 1 );
			sprintf( fileName2, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_UPDATECNF, loopCnt1 );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			if( access( fileName1, F_OK ) == 0 )
				rename( fileName1, fileName2 );
		}
		/* ファイル名の設定 */
		sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_UPDATECNF );
		sprintf( fileName2, "%s%s_1.log", PV_PATH_LOG, PV_FNAME_UPDATECNF );
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

	/* ログメッセージ出力処理 */
	fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s\n",
		localTime.tm_year + 1900,
		localTime.tm_mon + 1,
		localTime.tm_mday,
		localTime.tm_hour,
		localTime.tm_min,
		localTime.tm_sec,
		nowtime.tv_usec,
		pc_payload);

	/* ファイルを閉じる */
	fclose( fp );

	return( 0 );
}
int commfunc_remotecontrol_logout( char *pc_payload )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
	unsigned char			*pt1;					/* ポインタ */
	unsigned char			*pt2;					/* ポインタ */
	int						fd;						/* ファイルディスクリプタ */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */
	struct tm				localTime;				/* 年月日時分秒 */
	struct timeval			nowtime;
	int						logMode = PV_LOG_OFF;	/* ログ出力 */


	gettimeofday( &nowtime, NULL );
	localtime_r( &nowtime.tv_sec, &localTime );

	/* 出力用ログファイル名の設定 */
	sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_REMOTECTL );

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
		for(loopCnt1 = PV_LOG_STOCK_NUM ; loopCnt1 > 1 ; loopCnt1-- )	/* [20230902] Chg */
		{
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_REMOTECTL, loopCnt1 - 1 );
			sprintf( fileName2, "%s%s_%d.log", PV_PATH_LOG, PV_FNAME_REMOTECTL, loopCnt1 );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			if( access( fileName1, F_OK ) == 0 )
				rename( fileName1, fileName2 );
		}
		/* ファイル名の設定 */
		sprintf( fileName1, "%s%s.log", PV_PATH_LOG, PV_FNAME_REMOTECTL );
		sprintf( fileName2, "%s%s_1.log", PV_PATH_LOG, PV_FNAME_REMOTECTL );
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

	/* ログメッセージ出力処理 */
	fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s\n",
		localTime.tm_year + 1900,
		localTime.tm_mon + 1,
		localTime.tm_mday,
		localTime.tm_hour,
		localTime.tm_min,
		localTime.tm_sec,
		nowtime.tv_usec,
		pc_payload);

	/* ファイルを閉じる */
	fclose( fp );

	return( 0 );
}

/* [20230905] Add Ed */

