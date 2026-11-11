/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20250717] 2025.07.17
 * ・時刻同期サービスを停止する処理を追加
 * [20250710] 2025.07.10
 * ・時刻同期サービスのShellを変更
 * [20200814] 2020.08.14
 * ・CDTスケジュール制御対応
 * [20170327] 2017.03.27
 * ・更新スケジュール未取得時に固定スケジュール(月間)を取得するように処理追加
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * スケジュール管理プロセス
 *
 */
/**************************************************
* 内部ヘッダ
**************************************************/
#ifndef _PVSCH_MNG_COMMON_H
#define _PVSCH_MNG_COMMON_H

#define	PV_SSL_READ_SIZE			1024
#define PV_SSL_CERT_KEY				"root_cert.SCHEDULESERVER"
#define PV_SCH_NTP_SHELL			"ntp_sync.sh"	/* [20250710] Chg */
#define PV_SCH_SYNC_ERR_MIN			60		/* 時刻同期によるスケジュールリセット時間(分) */
#define PV_FIX_SCH_RTY_MIN			5		/* 固定スケジュールリトライ時間(分) */
#define	PV_FIX_SCH_RTY_NUM			5		/* 固定スケジュールリトライ回数 */
#define PV_FIX_SCH_GET_INTER_DAY	3		/* 固定スケジュール(月間)翌月ファイル取得期間(日) [20170327] Add */
#define PV_UP_SCH_RTY_MIN_1ST		5		/* 更新スケジュールリトライ時間(初期起動時) */
#define PV_UP_SCH_RTY_MIN			30		/* 更新スケジュールリトライ時間(通常時) */
#define PV_ID_CHECK_RTY_MIN			1		/* ID登録確認リトライ時間(1分) ※未使用 */
#define PV_REQ_SCH_TYPE_NONE		0		/* 要求スケジュール種別（なし） */
#define PV_REQ_SCH_TYPE_YEAR		1		/* 要求スケジュール種別（年間） */
#define PV_REQ_SCH_TYPE_MONTH		2		/* 要求スケジュール種別（月間） */
#define PV_REQ_SCH_TYPE_UPDATE		3		/* 要求スケジュール種別（更新） */
#define PV_REQ_SCH_TYPE_IDREG		4		/* 要求スケジュール種別（ID登録確認） */
#define PV_SCH_NTP_LIMIT_SEC		(10*60)	/* 時刻同期実施可能期間(秒) */
#define PV_SCH_FIXED_LIMIT_SEC		(20*60)	/* 固定スケジュール要求可能期間(秒) */
#define PV_SCH_FMT_NO_ID			301		/* フォーマットNo（ID登録結果） */
#define PV_SCH_FMT_NO_YEAR			201		/* フォーマットNo（年間） */
#define PV_SCH_FMT_NO_MONTH			202		/* フォーマットNo（月間） */
#define PV_SCH_FMT_NO_UPDATE		203		/* フォーマットNo（更新） */
/* 年間日数取得 */
#define PV_GET_YEAR_DAYS(year)		( 365 + ((year % 400)==0 ? 1 : ((year % 100)==0 ? 0 : ((year % 4)==0 ? 1 : 0))))
/* 当日の積算秒取得 */
#define PV_GET_TODAY_SEC(hour, min)	( (hour*60*60)+(min*60) )

/* [20200814] Add St */
#define	PV_SCH_CDT_CRE_MIN			20									/* 定周期 CDTスケジュールファイル作成タイミング */
#define	PV_SCH_CDT_TIMEOUT			10									/* 通信異常のタイムアウト時間 */

#define	PV_SCH_TCP_RES_SND_TIME		50									/* TCP通信のサーバ側 送信(応答)待ち時間(ms) */
#define	PV_SCH_TCP_CONNECT_TIME		10000								/* TCP通信のサーバ側 からの接続待ち時間(ms) */
#define	PV_SCH_TCP_RCV_TIME			5000								/* TCP通信のサーバー／クライアント側 受信タイムアウト時間(ms) */
#define	PV_SCH_TCP_REQ_SND_TIME		4500								/* TCP通信のクライアント側 送信待ち時間(ms) */
/* [20200814] Add Ed */

/* [20250717] Add St */
#define PV_SCH_SEP_FLG_NORMAL	0x00					/* 通常処理 */
#define PV_SCH_SEP_FLG_SPCSKP	0x01					/* 空白、タブ文字はデータとして処理しない */
#define PV_SCH_SEP_FLG_CONTIN	0x80					/* 区切り文字が連続した場合はデータカウントしない */
/* [20250717] Add Ed */


/********************************/
/* その他                       */
/********************************/
/* ログレベル */
#define	PVSCH_MSGLEVEL_LOW		1
#define	PVSCH_MSGLEVEL_MID		2
#define	PVSCH_MSGLEVEL_HIG		3
#define	PVSCH_MSGLEVEL_ERR		4
#define	PVSCH_MSGLEVEL_NONE		5

/* ログ出力 */
#if (PVSCH_MSGLEVEL <= PVSCH_MSGLEVEL_LOW)
	#define PVSCH_MSG_LOW (printf("[LOW]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVSCH_MSG_LOW( msg, ... )
#endif

#if (PVSCH_MSGLEVEL <= PVSCH_MSGLEVEL_MID)
	#define PVSCH_MSG_MID (printf("[MID]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVSCH_MSG_MID( msg, ... )
#endif

#if (PVSCH_MSGLEVEL <= PVSCH_MSGLEVEL_HIG)
	#define PVSCH_MSG_HIG (printf("[HIG]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVSCH_MSG_HIG( msg, ... )
#endif

#if (PVSCH_MSGLEVEL <= PVSCH_MSGLEVEL_ERR)
	#define PVSCH_MSG_ERR (printf("[ERR]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVSCH_MSG_ERR( msg, ... )
#endif


//=============
// typedef
//=============

//=============
// extern
//=============

//=============
// struct
//=============
struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};

/* [20200814] Add St */
struct st_threadArg {
	pthread_t			threadID;							/* スレッドID */
	int					indexNumb;							/* 処理中インデックス番号 */
	int					stFlg;								/* スレッド開始フラグ */
	int					edFlg;								/* スレッド終了フラグ */
	int					fileSendReq;						/* ファイル送信要求フラグ */
	int					fileSendComp;						/* ファイル送信完了フラグ */
};

struct st_rcvKeepBuff {
	int					length;									/* 保存データ長 */
	char				readBuff[4096];							/* 保存バッファ */
};
/* [20200814] Add Ed */


#endif // _PVSCH_MNG_COMMON_H

