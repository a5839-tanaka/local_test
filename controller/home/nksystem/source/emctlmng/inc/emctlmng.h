/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260514] 2026.05.14
 * ・一次調整力の系統周波数を平均値を算出し使用するように修正
 * [20240910] 2024.09.10 Ph4.2対応
 * ・容量市場の実需給、実効性テストの優先度を外部ファイルから取得するように修正
 * [20210113] 2020.01.13
 * ・PV発電禁止(連系中)のPV抑制を変化量に従って抑制するように修正
 * [20210112] 2020.01.12
 * ・受電電力目標値の切り替え時にを変化量に従って移行するように修正
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * エネルギーマネジメントシステム
 * -----------------------------------------------------------------------------
 * 制御管理プロセス
 *
 */
/**************************************************
* 内部ヘッダ
**************************************************/
#ifndef _PVCTL_EMS_COMMON_H
#define _PVCTL_EMS_COMMON_H

/* [20210112] Add St */
#define EMS_CTLCONF_NAME		"ctrl_param.conf"		/* 設定ファイル */
#define EMS_CTLCONF_PARAM1		"RCV_TARGET_CHG_VAL"	/* 受電電力目標値変化量 *//* 未使用 */
/* [20210112] Add Ed */
#define EMS_CTLCONF_PARAM2		"ONGRID_PV_CHGBAN_VAL"	/* PV発電禁止(連系中)抑制変化量  *//* [20210113] Add *//* 未使用 */
#define EMS_CTLCONF_PARAM3		"CAPACITY_MARKET_PRI"	/* 容量市場優先度（0:実需給,1:実効性テスト） *//* [20240910] Add */
#define EMS_CTLCONF_PARAM4		"FREQ_AVE_COUNT"		/* 周波数平均100msカウント *//* [20260514] Add */


/********************************/
/* 共通                         */
/********************************/


/********************************/
/* その他                       */
/********************************/
/* ログレベル */
#define	PVCTL_MSGLEVEL_LOW		1
#define	PVCTL_MSGLEVEL_MID		2
#define	PVCTL_MSGLEVEL_HIG		3
#define	PVCTL_MSGLEVEL_ERR		4
#define	PVCTL_MSGLEVEL_NONE		5

/* ログ出力 */
#if (PVCTL_MSGLEVEL <= PVCTL_MSGLEVEL_LOW)
	#define PVCTL_MSG_LOW (printf("[LOW]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVCTL_MSG_LOW( msg, ... )
#endif

#if (PVCTL_MSGLEVEL <= PVCTL_MSGLEVEL_MID)
	#define PVCTL_MSG_MID (printf("[MID]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVCTL_MSG_MID( msg, ... )
#endif

#if (PVCTL_MSGLEVEL <= PVCTL_MSGLEVEL_HIG)
	#define PVCTL_MSG_HIG (printf("[HIG]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVCTL_MSG_HIG( msg, ... )
#endif

#if (PVCTL_MSGLEVEL <= PVCTL_MSGLEVEL_ERR)
	#define PVCTL_MSG_ERR (printf("[ERR]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define PVCTL_MSG_ERR( msg, ... )
#endif


/********************************/
/* 構造体定義                   */
/********************************/
struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[256][128];				/* 100データ（1データあたり128バイト） */
};


struct st_avlData {									/* 移動平均値 */
	int			stocMax;							/* 最大ストック数 */
	int			stocPtr;							/* ストック位置 */
	int			tmAvlFlg;							/* 移動平均：計測フラグ */
	int			tmAvlVal;							/* 移動平均：計測値 */
	int			tmStocFlg[AVE_CNT_MAX];				/* 移動平均演算用ストック：計測フラグ *//* [20260514] Chg */
	int			tmStocVal[AVE_CNT_MAX];				/* 移動平均演算用ストック：計測値 *//* [20260514] Chg */
};

/********************************/
/* デマンド制御関連             */
/********************************/
#define	PVCTL_DMD_TIMED			30		/* デマンド時限(分) */
#define	PVCTL_DMD_TIMED_SEC		1800	/* デマンド時限(秒) */

/* フラグ制御 */
#define	PVCTL_FLG_OFF			0
#define	PVCTL_FLG_ON			1

#endif // _PVCTL_EMS_COMMON_H

