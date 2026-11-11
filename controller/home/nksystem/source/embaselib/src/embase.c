/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＥＭＳ装置監視ライブラリ
 * -----------------------------------------------------------------------------
 * 共通関数：ＭＭＡＰファイルアクセス、ＰＯＳＩＸメッセージ
 *
 * 機能：
 * 		以下の機能を提供する。
 *		・ＥＭＳの装置監視で使用するファイルをＭＭＡＰ化する。
 */
#include <unistd.h>
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
#include <time.h>
#include <mqueue.h>

#include "embase.h"

#define DEBUG_LIB

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
mqd_t					msg_des = -1;					/* メッセージキューディスクリプタ */
int						msgq_cflag = 0;					/* メッセージキュー生成フラグ */

int	embase_func_startup_adm(void **);
int	embase_func_startup( int );
int	embase_func_finish();
int embase_func_msgq_send( int );
int embase_func_msgq_receive();

int embase_func_get_kei_info();
int embase_func_get_lifchk_service();
int embase_func_get_lifchk_internal();
int embase_func_get_1kei_mode();
int embase_func_get_2kei_mode();
int embase_func_get_my_mode();
int embase_func_get_par_mode();
int embase_func_get_1kei_net_service();
int embase_func_get_2kei_net_service();
int embase_func_get_my_net_service();
int embase_func_get_par_net_service();
int embase_func_get_1kei_net_internal();
int embase_func_get_2kei_net_internal();
int embase_func_get_my_net_internal();
int embase_func_get_par_net_internal();
time_t embase_func_get_lastrecv_time();
int embase_func_get_drbd_my_role();
int embase_func_get_drbd_par_role();
int embase_func_get_drbd_my_dstate();
int embase_func_get_drbd_par_dstate();
int embase_func_get_drbd_my_cstate();
time_t embase_func_get_mode_tran_time();
time_t embase_func_get_my_operate_total_time();
time_t embase_func_get_par_operate_total_time();
time_t embase_func_get_my_operate_last_time();
time_t embase_func_get_par_operate_last_time();
int embase_func_get_newSysMode();

int embase_func_set_kei_info( int );
int embase_func_set_lifchk_service( int );
int embase_func_set_lifchk_internal( int );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 共通環境利用開始（管理プロセス向け）
 *
 * 機能：
 * 		装置管理ファイル（/home/nksystem/sONLDATA/EMSYSTEM.dat）ファイルをＭＭＡＰ
 * 		として読書可状態でオープンする。
 * 戻り値：
 * 		  0：正常終了
 * 		 -1：異常終了（多重ファイルオープン異常）
 *		 -2：異常終了（ファイルオープン異常）
 *		 -3：異常終了（ファイルサイズ調整異常）
 *		 -4：異常終了（共有メモリ展開異常）
 *		 -5：異常終了（メッセージキュー生成異常）
 */
int	embase_func_startup_adm(void **filePoint)
{
	int					retCode = 0;					/* 戻り値 */
	int					clrFlg = 0;						/* ファイルクリアフラグ */
	char				filePath[1024];					/* ファイルパス */
	void				*point;
	struct mq_attr		s_mqattr;						/* メッセージキュー定義 */
	mode_t				umask_data;						/* UMASK保存値 */


	/* ステータスがＭＭＡＰに展開済みの場合 */
	if( s_emsMmapInfo.openStat == 1 )
	{
		/* 展開済みエラー */
		retCode = -1;
		return( retCode );
	}

	/* ファイルパスを設定 */
	sprintf( filePath, "%s%s", EMBASE_FPATH_SONLINE, s_emsMmapInfo.fileName );

	/* ファイルを読書可能でオープン */
	if( (s_emsMmapInfo.fd = open( filePath, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO )) == -1 )
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

	/* メッセージ受信用キューの設定 */
	s_mqattr.mq_flags   = O_NONBLOCK;						/* ノンンブロッキングモード */
	s_mqattr.mq_maxmsg  = 10;								/* メッセージキュー最大メッセージ数 */
	s_mqattr.mq_msgsize = sizeof( struct st_msgFrame );		/* メッセージ最大サイズ */

	/* UMASK値を変更する */
	umask_data = umask( 0 );

	/* メッセージキューを受信専用でオープンする */
	if( (msg_des = mq_open( EMBASE_MSGQUE_EMSYSTEM, O_RDONLY | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, &s_mqattr)) == -1 )
	{
		/* UMASK値を戻す */
		umask( umask_data );
		/* エラー */
		return( -5 );
	}

	/* UMASK値を戻す */
	umask( umask_data );

	/* メッセージキュー生成フラグ設定 */
	msgq_cflag = 1;

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 共通環境利用開始（一般プロセス向け）
 *
 * 機能：
 * 		装置管理ファイル（/home/nksystem/sONLDATA/EMSYSTEM.dat）ファイルをＭＭＡＰ
 * 		として読込専用でオープンする。
 *		管理プロセスへのメッセージ送信用キューをオープンする（注1）
 *		（注1）
 *		引数 startMode が 0 の場合はＭＭＡＰファイルのみ処理を行い、1 の場合はメ
 *		ッセージキューの処理も行う
 * 戻り値：
 * 		  0：正常終了
 * 		 -1：異常終了（多重ファイルオープン異常）
 *		 -2：異常終了（ファイルオープン異常）
 *		 -3：異常終了（共有メモリ展開異常）
 *		 -4：異常終了（メッセージキューオープン異常）
 */
int	embase_func_startup( int startMode )
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
	sprintf( filePath, "%s%s", EMBASE_FPATH_SONLINE, s_emsMmapInfo.fileName );

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

		retCode = -3;
		return( retCode );
	}

	/* 共有メモリアドレスをセット */
	s_emsMmapInfo.point = point;

	/* ポインタを設定 */
	ps_libEMSYSTEM = point;

	/* ステータスをＭＭＡＰ展開済みに設定 */
	s_emsMmapInfo.openStat = 1;

	/* モードが 0 以外の場合 */
	if( startMode )
	{
		/* メッセージキューを送信専用でオープンする（ノンブロッキングモード） */
		if( (msg_des = mq_open( EMBASE_MSGQUE_EMSYSTEM, O_WRONLY | O_NONBLOCK)) == -1 )
		{
#ifdef DEBUG_LIB
printf("EMBASELIB: mq_open Error errno ( %d )\n", errno );
#endif
			/* エラー */
			return( -4 );
		}
	}

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 共通環境利用終了
 *
 * 機能：
 * 		ＭＭＡＰとしてオープンしていた装置管理ファイル
 *		（/home/nksystem/sONLDATA/EMSYSTEM.dat）ファイルをＭＭＡＰをクローズする。
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

	/* メッセージキューをクローズする */
	if( msg_des != -1 )
	{
		/* メッセージキューをクローズ */
		mq_close( msg_des );
		/* ディスクリプタを初期化 */
		msg_des = -1;
		/* メッセージキュー生成フラグONの場合 */
		if( msgq_cflag )
		{
			/* メッセージキューを削除 */
			mq_unlink( EMBASE_MSGQUE_EMSYSTEM );
			/* フラグを初期化 */
			msgq_cflag = 0;
		}
	}

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * メッセージ送信処理（一般プロセス用）
 *
 * 機能：
 *		引数 msgKind で指定されたメッセージを送信する
 * 戻り値：
 * 		  0：正常終了
 *		 -1：異常終了（メッセージキュー未オープン）
 *		 -2：異常終了（メッセージ送信異常）
 */
int embase_func_msgq_send( int msgKind )
{
	int					retCode = 0;					/* 戻り値 */
	struct st_msgFrame	s_msgFrame;						/* メッセージ送信フレーム */

	/* メッセージキューが未オープンの場合 */
	if( -1 == msg_des )
	{
#ifdef DEBUG_LIB
printf("embaselib: embase_func_msgq_send(): ERROR! message queue not open!\n" );
#endif
		return( -1 );
	}

	/* 送信フレーム初期化 */
	memset( &s_msgFrame, 0, sizeof( s_msgFrame ) );

	/* メッセージ送信時刻の設定 */
	s_msgFrame.sendTime = time( NULL );
	/* メッセージ種別の設定 */
	s_msgFrame.msgKind = msgKind;

	/* メッセージの送信 */
	if( (retCode = mq_send( msg_des, (const char *)&s_msgFrame, sizeof( s_msgFrame ), 1)) != 0 )
	{
#ifdef DEBUG_LIB
printf("embaselib: embase_func_msgq_send(): mq_send ERROR! (errno:%d)\n", errno );
#endif
		/* エラー */
		return( -2 );
	}

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * メッセージ受信処理（管理プロセス用）
 *
 * 機能：
 *		メッセージキューからメッセージを受信する。メッセージの送信時刻が許容時間
 *		よりも過去の場合は受信破棄し、新しいメッセージを受信する。
 *		メッセージキューにメッセージがない場合は関数から抜ける。
 * 戻り値：
 * 		  0：正常終了（メッセージなし）
 * 		 0<：正常終了（メッセージ種別）
 *		 -1：異常終了（メッセージキュー未オープン）
 *		 -2：異常終了（メッセージ受信異常）
 */
int embase_func_msgq_receive()
{
	int					retCode = 0;					/* 戻り値 */
	struct st_msgFrame	s_msgFrame;						/* メッセージ受信フレーム */
	time_t				nowTime;						/* 現在時刻 */
	unsigned int		msg_pri;						/* メッセージプライオリティ */

	/* メッセージキューが未オープンの場合 */
	if( -1 == msg_des )
	{
#ifdef DEBUG_LIB
printf("embaselib: embase_func_msgq_receive(): ERROR! message queue not open!\n" );
#endif
		return( -2 );
	}

	/* 現在時刻の取得 */
	nowTime = time( NULL );

	/* メッセージの受信 */
	while( 1 )
	{
		/* メッセージの受信 */
		if( -1 == (retCode = mq_receive( msg_des, (char *)&s_msgFrame, sizeof( s_msgFrame ), &msg_pri)))
		{
			/* メッセージキューに何もメッセージがない場合 */
			if( errno == EAGAIN )
			{
				/* 正常：メッセージなし */
				return( 0 );
			}

#ifdef DEBUG_LIB
printf("embaselib: embase_func_msgq_receive(): mq_receive ERROR! (errno:%d)\n", errno );
#endif
			/* エラー */
			return( -3 );
		}

		/* メッセージの送信時刻が許容範囲であれば処理を受信処理を抜ける */
		if( nowTime < (s_msgFrame.sendTime + EMBASE_MSG_TIMEOUT) )
		{
			break;
		}
	}

	/* メッセージ種別を返す */
	return( s_msgFrame.msgKind );
}

/*------------------------------------------------------------------------*/

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
/* 他系装置の装置モードを返す */
int embase_func_get_par_mode()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 他系の装置モードを返す */
	if(ps_libEMSYSTEM->sysNumber == 1)
		return( ps_libEMSYSTEM->s_kei[1].sysMode );
	else
		return( ps_libEMSYSTEM->s_kei[0].sysMode );
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
/* 他系装置のネットワークＩ／Ｆ状態（サービス用）を返す */
int embase_func_get_par_net_service()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 他系のネットワークＩ／Ｆ状態（サービス用）を返す */
	if(ps_libEMSYSTEM->sysNumber == 1)
		return( ps_libEMSYSTEM->s_kei[1].netDevSts[EMBASE_NETDEV_SERVICE] );
	else
		return( ps_libEMSYSTEM->s_kei[0].netDevSts[EMBASE_NETDEV_SERVICE] );
}
/* 他系装置のネットワークＩ／Ｆ状態（系列間用）を返す */
int embase_func_get_par_net_internal()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 他系のネットワークＩ／Ｆ状態（系列間用）を返す */
	if(ps_libEMSYSTEM->sysNumber == 1)
		return( ps_libEMSYSTEM->s_kei[1].netDevSts[EMBASE_NETDEV_INTERNAL] );
	else
		return( ps_libEMSYSTEM->s_kei[0].netDevSts[EMBASE_NETDEV_INTERNAL] );
}
/* 死活監視情報の最終受信時刻を返す */
time_t embase_func_get_lastrecv_time()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 死活監視情報最終受信時刻を返す */
	return( ps_libEMSYSTEM->recvTime );
}
/* ＤＲＢＤの自系のロール状態を返す */
int embase_func_get_drbd_my_role()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ＤＲＢＤの自系のロール状態を返す */
	return( ps_libEMSYSTEM->drbdRole[ 0 ] );
}
/* ＤＲＢＤの他系のロール状態を返す */
int embase_func_get_drbd_par_role()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ＤＲＢＤの他系のロール状態を返す */
	return( ps_libEMSYSTEM->drbdRole[ 1 ] );
}
/* ＤＲＢＤの自系のディスク状態を返す */
int embase_func_get_drbd_my_dstate()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ＤＲＢＤの自系のディスク状態を返す */
	return( ps_libEMSYSTEM->drbdDstate[ 0 ] );
}
/* ＤＲＢＤの他系のディスク状態を返す */
int embase_func_get_drbd_par_dstate()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ＤＲＢＤの他系のディスク状態を返す */
	return( ps_libEMSYSTEM->drbdDstate[ 1 ] );
}
/* ＤＲＢＤの自系の接続状態を返す */
int embase_func_get_drbd_my_cstate()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* ＤＲＢＤの自系の接続状態を返す */
	return( ps_libEMSYSTEM->drbdCstate );
}
/* 運用モード変更確認時間を返す */
time_t embase_func_get_mode_tran_time()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 運用モード変更確認時間を返す */
	return( ps_libEMSYSTEM->modeTranTime );
}
/* 自系の運用モード正常稼働時間を返す */
time_t embase_func_get_my_operate_total_time()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 運用モード正常稼働時間を返す */
	return( ps_libEMSYSTEM->operateTotalTime[0] );
}
/* 相手の運用モード正常稼働時間を返す */
time_t embase_func_get_par_operate_total_time()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 運用モード正常稼働時間を返す */
	return( ps_libEMSYSTEM->operateTotalTime[1] );
}
/* 自系の運用モード最終常稼働時刻を返す */
time_t embase_func_get_my_operate_last_time()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 運用モード最終稼働時刻を返す */
	return( ps_libEMSYSTEM->operateLastTime[0] );
}
/* 相手の運用モード最終稼働時刻を返す */
time_t embase_func_get_par_operate_last_time()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 運用モード最終稼働時刻を返す */
	return( ps_libEMSYSTEM->operateLastTime[1] );
}
/* 遷移先モードを返す */
int embase_func_get_newSysMode()
{
	/* ファイルがＭＭＡＰに未展開の場合 */
    if( ps_libEMSYSTEM == NULL ) return( -1 );
	/* 遷移先モードを返す */
	return( ps_libEMSYSTEM->newSysMode );
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
