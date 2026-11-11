/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＥＭＳ装置監視ライブラリ
 * -----------------------------------------------------------------------------
 * 共通関数：ファイルアクセス関数
 *
 * 機能：
 * 		以下の機能を提供する。
 *		・ＥＭＳの装置監視で使用するファイルをＭＭＡＰ化する。
 *		・ミューテックスを管理する領域を共通メモリ上に生成し、ミューテックスを
 *		　初期化する。
 */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "embase.h"

/*
 * ＭＭＡＰ定義テーブル
 * ＥＭＳの装置監視機能でで使用するオンラインファイルを全て定義する
 * （構造体の配置は必ずファイル番号に対応した位置に定義すること）
 */
struct st_emsMmapInfo {
	char			fileName[1024];						/* ファイル名 */
	int				fileSize;							/* ファイルサイズ（ＭＭＡＰのメモリ確保に使用） */
	int				fd;									/* ファイルディスクリプタ */
	int				openStat;							/* オープンステータス */
	void			*point;								/* ＭＭＡＰアドレス */
} s_emsMmapInfo = {
	EMBASE_FNAME_EMSYSTEM, sizeof(struct st_EMSYSTEM), 0, 0, NULL
};

struct st_EMSYSTEM		*ps_libEMSYSTEM;				/* ファイル構造体 */

int	embase_func_startup_adm(void **);
int	embase_func_startup();
int	embase_func_finish();

int embase_func_get_kei_info();
int embase_func_get_lifchk_service();
int embase_func_get_lifchk_internal();
int embase_func_get_1kei_mode();
int embase_func_get_2kei_mode();
int embase_func_get_my_mode();
int embase_func_get_1kei_net_service();
int embase_func_get_2kei_net_service();
int embase_func_get_my_net_service();
int embase_func_get_1kei_net_internal();
int embase_func_get_2kei_net_internal();
int embase_func_get_my_net_internal();

int embase_func_set_kei_info( int );
int embase_func_set_lifchk_service( int );
int embase_func_set_lifchk_internal( int );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態ファイルアクセス開始（管理プロセス向け）
 *
 * 機能：
 * 		装置管理ファイル（/home/nkuser/sONLDATA/EMSYSTEM.dat）ファイルをＭＭＡＰ
 * 		として読書可状態でオープンする。
 * 戻り値：
 * 		 0：正常終了
 * 		 -1,-2,-3,-4：異常終了
 */
int	embase_func_startup_adm(void **filePoint)
{
	int					retCode = 0;					/* 戻り値 */
	int					clrFlg = 0;						/* ファイルクリアフラグ */
	char				filePath[1024];					/* ファイルパス */
	void				*point;

	/* ステータスがＭＭＡＰに展開済みの場合 */
	if( s_emsMmapInfo.openStat == 1 )
	{
		/* 展開済みエラー */
		retCode = -1;
		return( retCode );
	}

	/* ファイルパスを設定 */
	sprintf( filePath, "%s%s", EMBASE_FILE_PATH, s_emsMmapInfo.fileName );

	/* ファイルを読書可能でオープン */
	if( (s_emsMmapInfo.fd = open( filePath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH )) == -1 )
	{
		/* ファイルオープンエラー */
		retCode = -2;
		return( retCode );
	}
	/* ファイルの長さを調整 */
	if( ftruncate( s_emsMmapInfo.fd, s_emsMmapInfo.fileSize ) == -1 )
	{
		retCode = -3;
		return( retCode );
	}

	/* ファイルを読書可能なＭＭＡＰとして共有メモリに展開 */
	point =  (void *)mmap( NULL, s_emsMmapInfo.fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, s_emsMmapInfo.fd, 0 );

	if( MAP_FAILED == point )
	{
		/* エラー */
		/* ファイルクローズ */
		close( s_emsMmapInfo.fd );

		retCode = -4;
		return( retCode );
	}

	/* 共有メモリアドレスをセット */
	s_emsMmapInfo.point = point;

	/* ステータスをＭＭＡＰ展開済みに設定 */
	s_emsMmapInfo.openStat = 1;

	/* ポインタを設定 */
	ps_libEMSYSTEM = point;

	/* ポインタを設定 */
	*(filePoint) = point;

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態ファイルアクセス開始（一般プロセス向け）
 *
 * 機能：
 * 		装置管理ファイル（/home/nkuser/sONLDATA/EMSYSTEM.dat）ファイルをＭＭＡＰ
 * 		として読込専用でオープンする。
 * 戻り値：
 * 		 0：正常終了
 * 		 -1,-2,-3,-4：異常終了
 */
int	embase_func_startup()
{
	int					retCode = 0;					/* 戻り値 */
	int					clrFlg = 0;						/* ファイルクリアフラグ */
	char				filePath[1024];					/* ファイルパス */
	void				*point;

	/* ステータスがＭＭＡＰに展開済みの場合 */
	if( s_emsMmapInfo.openStat == 1 )
	{
		/* 展開済みエラー */
		retCode = -1;
		return( retCode );
	}

	/* ファイルパスを設定 */
	sprintf( filePath, "%s%s", EMBASE_FILE_PATH, s_emsMmapInfo.fileName );

	/* ファイルを読書可能でオープン */
	if( (s_emsMmapInfo.fd = open( filePath, O_RDONLY)) == -1 )
	{
		/* ファイルオープンエラー */
		retCode = -2;
		return( retCode );
	}

	/* ファイルを読書可能なＭＭＡＰとして共有メモリに展開 */
	point =  (void *)mmap( NULL, s_emsMmapInfo.fileSize, PROT_READ, MAP_SHARED, s_emsMmapInfo.fd, 0 );

	if( MAP_FAILED == point )
	{
		/* エラー */
		/* ファイルクローズ */
		close( s_emsMmapInfo.fd );

		retCode = -4;
		return( retCode );
	}

	/* 共有メモリアドレスをセット */
	s_emsMmapInfo.point = point;

	/* ポインタを設定 */
	ps_libEMSYSTEM = point;

	/* ステータスをＭＭＡＰ展開済みに設定 */
	s_emsMmapInfo.openStat = 1;

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態ファイルアクセス終了
 *
 * 機能：
 * 		ＭＭＡＰとしてオープンしていた装置管理ファイル
 *		（/home/nkuser/sONLDATA/EMSYSTEM.dat）ファイルをＭＭＡＰをクローズする。
 * 戻り値：
 * 		 0：正常終了
 */
int embase_func_finish()
{
	int					retCode = 0;					/* 戻り値 */

	/* ファイルがＭＭＡＰに未展開の場合 */
	if( s_emsMmapInfo.openStat == 0 )
	{
		/* 処理対象外 */
		return( retCode );
	}

	/* ＭＭＡＰをアンマップする */
	munmap( s_emsMmapInfo.point, s_emsMmapInfo.fileSize );

	/* ファイルをクローズ */
	close( s_emsMmapInfo.fd );

	/* 共有メモリアドレスをリセット */
	s_emsMmapInfo.point = NULL;

	/* ＭＭＡＰアドレスをリセット */
	ps_libEMSYSTEM = NULL;

	/* ステータスをＭＭＡＰアンマップ済みに設定 */
	s_emsMmapInfo.openStat = 0;

	return( retCode );
}

/* 自装置の系番号を返す */
int embase_func_get_kei_info()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 自系の系番号を返す */
	return( ps_libEMSYSTEM->sysNumber );
}
/* 自装置の死活監視状態（サービス側）の状態を返す */
int embase_func_get_lifchk_service()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 自系のネットワークＩ／Ｆ（サービス側）の状態を返す */
	return( ps_libEMSYSTEM->lifChkSts[EMBASE_NETDEV_SERVICE] );
}
/* 自装置の死活監視状態（系列間側）の状態を返す */
int embase_func_get_lifchk_internal()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 自系のネットワークＩ／Ｆ（サービス側）の状態を返す */
	return( ps_libEMSYSTEM->lifChkSts[EMBASE_NETDEV_INTERNAL] );
}

/* １系装置の装置モードを返す */
int embase_func_get_1kei_mode()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* １系の装置モードを返す */
	return( ps_libEMSYSTEM->s_kei[EMBASE_SYSNUMBER_1-1].sysMode );
}
/* １系装置のネットワークＩ／Ｆ状態（サービス用）を返す */
int embase_func_get_1kei_net_service()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* １系のネットワークＩ／Ｆ状態（サービス用）を返す */
	return( ps_libEMSYSTEM->s_kei[EMBASE_SYSNUMBER_1-1].netDevSts[EMBASE_NETDEV_SERVICE] );
}
/* １系装置のネットワークＩ／Ｆ状態（系列間用）を返す */
int embase_func_get_1kei_net_internal()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* １系のネットワークＩ／Ｆ状態（系列間用）を返す */
	return( ps_libEMSYSTEM->s_kei[EMBASE_SYSNUMBER_1-1].netDevSts[EMBASE_NETDEV_INTERNAL] );
}
/* ２系装置の装置モードを返す */
int embase_func_get_2kei_mode()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ２系の装置モードを返す */
	return( ps_libEMSYSTEM->s_kei[EMBASE_SYSNUMBER_2-1].sysMode );
}
/* ２系装置のネットワークＩ／Ｆ状態（サービス用）を返す */
int embase_func_get_2kei_net_service()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ２系のネットワークＩ／Ｆ状態（サービス用）を返す */
	return( ps_libEMSYSTEM->s_kei[EMBASE_SYSNUMBER_2-1].netDevSts[EMBASE_NETDEV_SERVICE] );
}
/* ２系装置のネットワークＩ／Ｆ状態（系列間用）を返す */
int embase_func_get_2kei_net_internal()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ２系のネットワークＩ／Ｆ状態（系列間用）を返す */
	return( ps_libEMSYSTEM->s_kei[EMBASE_SYSNUMBER_2-1].netDevSts[EMBASE_NETDEV_INTERNAL] );
}
/* 自系装置の装置モードを返す */
int embase_func_get_my_mode()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 自系の装置モードを返す */
	return( ps_libEMSYSTEM->s_kei[ps_libEMSYSTEM->sysNumber-1].sysMode );
}
/* 自系装置のネットワークＩ／Ｆ状態（サービス用）を返す */
int embase_func_get_my_net_service()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 自系のネットワークＩ／Ｆ状態（サービス用）を返す */
	return( ps_libEMSYSTEM->s_kei[ps_libEMSYSTEM->sysNumber-1].netDevSts[EMBASE_NETDEV_SERVICE] );
}
/* 自系装置のネットワークＩ／Ｆ状態（系列間用）を返す */
int embase_func_get_my_net_internal()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 自系のネットワークＩ／Ｆ状態（系列間用）を返す */
	return( ps_libEMSYSTEM->s_kei[ps_libEMSYSTEM->sysNumber-1].netDevSts[EMBASE_NETDEV_INTERNAL] );
}


/* 自装置の系番号を設定する */
int embase_func_set_kei_info(int a_kei)
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 装置番号が規程外の場合 */
	if( (EMBASE_SYSNUMBER_1 != a_kei) && (EMBASE_SYSNUMBER_2 != a_kei) ) return( -1 );
	/* 自系の系番号を設定 */
	ps_libEMSYSTEM->sysNumber = a_kei;
	return( 0 );
}
/* 自装置の死活監視状態（サービス側）の状態を設定する */
int embase_func_set_lifchk_service(int a_stat)
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ステータスが規定外の場合 */
	if( (EMBASE_LIFCHK_OK != a_stat) && (EMBASE_LIFCHK_NG != a_stat) ) return( -1 );
	/* 自系のネットワークＩ／Ｆ（サービス側）の状態を設定 */
	ps_libEMSYSTEM->lifChkSts[EMBASE_NETDEV_SERVICE] = a_stat;
	return( 0 );
}
/* 自装置の死活監視状態（系列間側）の状態を設定する */
int embase_func_set_lifchk_internal(int a_stat)
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ステータスが規定外の場合 */
	if( (EMBASE_LIFCHK_OK != a_stat) && (EMBASE_LIFCHK_NG != a_stat) ) return( -1 );
	/* 自系のネットワークＩ／Ｆ（サービス側）の状態を返す */
	ps_libEMSYSTEM->lifChkSts[EMBASE_NETDEV_INTERNAL] = a_stat;
	return( 0 );
}
