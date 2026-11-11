/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * デバイス処理用データ関連定義
 *
 */

#define	PV_DEVDATA_TNAME		32				/* 型式名称サイズ */

/*
 * デバイス名－通信方式 設定用構造体
 */
struct st_devTypeName {
	char			typeName[PV_DEVDATA_TNAME];	/* 型式名称 */
	int				devType;					/* 通信処理方式 */
	int				reqCnt;						/* 基本要求回数 */
};

/*
 * デバイス情報（通信処理方式、装置番号、装置ＩＤ）定義用構造体
 */
/* 標準構成の場合（エネルギーモニタ以外の場合） */
struct st_stdDevInfo {
	int				devType;					/* 装置タイプ（通信処理方式） */
	int				devNumb;					/* 装置番号 */
	int				idNumb;						/* 装置ID */
	int				reqCnt;						/* 基本要求回数 */
};

/*
 * 状態・計測データ保持用構造体
 */
/* 装置種別: 収集装置 */
struct st_sysData {
	unsigned char	sysStatus[(PV_MAX_PVSYS_SVPOS-1)/8+1];	/* 装置状態 *//* 未使用 */
	int				tmFlgData[PV_MAX_PVSYS_TMPOS];			/* 計測フラグ */
	int				tmValData[PV_MAX_PVSYS_TMPOS];			/* 計測値 */
	int				serialStatus;							/* シリアル回線状態 *//* 未使用 */
	int				skipTime;								/* 処理保留時間 *//* 未使用 */
	int				oldStsNumb;								/* 旧ステータス値保存 *//* 未使用 */
	int				oldStsNumb2;							/* 旧ステータス値保存 *//* 未使用 */
	time_t			checkTime;								/* 時刻保持 *//* 未使用 */
	time_t			checkTime2;								/* 時刻保持 *//* 未使用 */
	suseconds_t		checkTimeuSec;							/* 時刻保持（マイクロ秒） */
};

/* 装置種別: パワーコンディショナ */
struct st_pcsData {
	unsigned char	pcsStatus[(PV_MAX_POWCN_SVPOS-1)/8+1];	/* 装置状態（全PCS共通） */
	int				tmFlgData[PV_MAX_POWCN_TMPOS];			/* 計測フラグ（全PCS共通） */
	int				tmValData[PV_MAX_POWCN_TMPOS];			/* 計測値（全PCS共通） */
	int				serialStatus;							/* シリアル回線状態（GSユアサ、山洋電気、田淵電機） */
	int				skipTime;								/* 処理保留時間（KACO用） */
	int				oldStsNumb;								/* 旧ステータス値保存（KACO、田淵電機） */
	int				oldStsNumb2;							/* 旧ステータス値保存（田淵電機） */
	time_t			checkTime;								/* 時刻保持（ダイヘン、GSユアサ、日新電機、山洋電気、田淵電機） */
	time_t			checkTime2;								/* 時刻保持 */
	suseconds_t		checkTimeuSec;							/* 時刻保持（マイクロ秒） */
};

/* 装置種別: マルチメータ計測監視機器 */
struct st_mltData {
	unsigned char	mltStatus[(PV_MAX_MLTMT_SVPOS-1)/8+1];	/* 装置状態 */
	int				tmFlgData[PV_MAX_MLTMT_TMPOS];			/* 計測フラグ */
	int				tmValData[PV_MAX_MLTMT_TMPOS];			/* 計測値 */
	int				serialStatus;							/* シリアル回線状態 */
	int				skipTime;								/* 処理保留時間（KACO用） */
	int				oldStsNumb;								/* 旧ステータス値保存（KACO、田淵電機） */
	int				oldStsNumb2;							/* 旧ステータス値保存（田淵電機） */
	time_t			checkTime;								/* 時刻保持（ダイヘン、GSユアサ、日新電機、山洋電気、田淵電機） */
	time_t			checkTime2;								/* 時刻保持 */
	suseconds_t		checkTimeuSec;							/* 時刻保持（マイクロ秒） */
};

/* 装置種別: バッテリー */
struct st_bttData {
	unsigned char	bttStatus[(PV_MAX_BATTE_SVPOS-1)/8+1];	/* 装置状態（全PCS共通） */
	int				tmFlgData[PV_MAX_BATTE_TMPOS];			/* 計測フラグ（全PCS共通） */
	int				tmValData[PV_MAX_BATTE_TMPOS];			/* 計測値（全PCS共通） */
	int				serialStatus;							/* シリアル回線状態（GSユアサ、山洋電気、田淵電機） */
	int				skipTime;								/* 処理保留時間（KACO用） */
	int				oldStsNumb;								/* 旧ステータス値保存（KACO、田淵電機） */
	int				oldStsNumb2;							/* 旧ステータス値保存（田淵電機） */
	time_t			checkTime;								/* 時刻保持（ダイヘン、GSユアサ、日新電機、山洋電気、田淵電機） */
	time_t			checkTime2;								/* 時刻保持 */
	suseconds_t		checkTimeuSec;							/* 時刻保持（マイクロ秒） */
};

/* 装置種別: 汎用リモートIO装置 */
struct st_remData {
	unsigned char	remStatus[(PV_MAX_REMIO_SVPOS-1)/8+1];	/* 装置状態 */
	int				tmFlgData[PV_MAX_REMIO_TMPOS];			/* 計測フラグ */
	int				tmValData[PV_MAX_REMIO_TMPOS];			/* 計測値 */
	int				serialStatus;							/* シリアル回線状態（GSユアサ、山洋電気、田淵電機） */
	int				skipTime;								/* 処理保留時間（KACO用） */
	int				oldStsNumb;								/* 旧ステータス値保存（KACO、田淵電機） */
	int				oldStsNumb2;							/* 旧ステータス値保存（田淵電機） */
	time_t			checkTime;								/* 時刻保持（ダイヘン、GSユアサ、日新電機、山洋電気、田淵電機） */
	time_t			checkTime2;								/* 時刻保持 */
	suseconds_t		checkTimeuSec;							/* 時刻保持（マイクロ秒） */
};

/* 装置種別: 小水力発電所PLC */
struct st_plcData {
	unsigned char	plcStatus[(PV_MAX_PLCFA_SVPOS-1)/8+1];	/* 装置状態 */
	int				tmFlgData[PV_MAX_PLCFA_TMPOS];			/* 計測フラグ */
	int				tmValData[PV_MAX_PLCFA_TMPOS];			/* 計測値 */
	int				serialStatus;							/* シリアル回線状態（GSユアサ、山洋電気、田淵電機） */
};

///* 装置種別: 仮想情報 */
//struct st_virData {
//	unsigned char	virStatus[(PV_MAX_VIRTL_SVPOS-1)/8+1];	/* 装置状態 */
//	int				tmFlgData[PV_MAX_VIRTL_TMPOS];			/* 計測フラグ */
//	int				tmValData[PV_MAX_VIRTL_TMPOS];			/* 計測値 */
//	int				serialStatus;							/* シリアル回線状態 *//* 未使用 */
//	int				skipTime;								/* 処理保留時間 *//* 未使用 */
//	int				oldStsNumb;								/* 旧ステータス値保存 *//* 未使用 */
//	int				oldStsNumb2;							/* 旧ステータス値保存 *//* 未使用 */
//	time_t			checkTime;								/* 時刻保持 *//* 未使用 */
//	time_t			checkTime2;								/* 時刻保持 *//* 未使用 */
//	suseconds_t		checkTimeuSec;							/* 時刻保持（マイクロ秒） */
//};

/*
 * 計測情報移動平均値算出用
 */
/* 装置種別: 収集装置 */
struct st_sysAvlData {
	int				tmDataAvlFlg[PV_MAX_PVSYS_TMPOS];		/* 移動平均値：計測フラグ */
	int				tmDataAvlVal[PV_MAX_PVSYS_TMPOS];		/* 移動平均値：計測値 */
};
/* 装置種別: バッテリー */
struct st_bttAvlData {
	int				tmDataAvlFlg[PV_MAX_BATTE_TMPOS];		/* 移動平均値：計測フラグ */
	int				tmDataAvlVal[PV_MAX_BATTE_TMPOS];		/* 移動平均値：計測値 */
};
/* 装置種別: パワーコンディショナ */
struct st_pcsAvlData {
	int				tmDataAvlFlg[PV_MAX_POWCN_TMPOS];		/* 移動平均値：計測フラグ */
	int				tmDataAvlVal[PV_MAX_POWCN_TMPOS];		/* 移動平均値：計測値 */
};
/* 装置種別: マルチメータ */
struct st_mltAvlData {
	int				tmDataAvlFlg[PV_MAX_MLTMT_TMPOS];		/* 移動平均値：計測フラグ */
	int				tmDataAvlVal[PV_MAX_MLTMT_TMPOS];		/* 移動平均値：計測値 */
};
/* 装置種別: 汎用リモートIO装置 */
struct st_remAvlData {
	int				tmDataAvlFlg[PV_MAX_REMIO_TMPOS];		/* 移動平均値：計測フラグ */
	int				tmDataAvlVal[PV_MAX_REMIO_TMPOS];		/* 移動平均値：計測値 */
};

/* 装置種別: PLC */
struct st_plcAvlData {
	int				tmDataAvlFlg[PV_MAX_PLCFA_TMPOS];		/* 移動平均値：計測フラグ */
	int				tmDataAvlVal[PV_MAX_PLCFA_TMPOS];		/* 移動平均値：計測値 */
};


/*
 * 計測情報上下限範囲（全装置共通）
 */
struct st_devTmRange {
	int				tmLower;								/* 下限値 */
	int				tmUpper;								/* 上限値 */
};


/* 関数定義(comm_devdata.c) */
int commfunc_devInfo_get( int, int, struct st_devTypeName *, void *, int *, int *, int *, void * );
int commfunc_devAvlData_init( int, int, void * );
int commfunc_devTmData_set( int, int, int, void *, void *, struct st_TMDATINF * ); 
int commfunc_devTmKindData( int, int, int, int *, int * );
int commfunc_tmstsfil_devset( int, int, int, void *, void *, struct st_TMDATINF *, struct st_TMSTSFIL * );
int commfunc_tmstsfil_get( int, int, int, void *, void *, void *, struct st_TMDATINF *, struct st_TMSTSFIL * );
int commfunc_tmrec_set( struct st_TMRECINF *, struct st_TMSTSFIL *);
int commfunc_tmrec_set( struct st_TMRECINF * ps_tmhistinf, struct st_TMSTSFIL * ps_tmstsfil);
int commfunc_tmrec_msggettcp( char (*)[], int  );
int commfunc_svrec_set( struct st_SVRECINF * ps_svrecinf, struct st_SVSTSFIL * ps_svstsfil);
int commfunc_svrec_msggettcp( char (*)[], int  );
int commfunc_rprec_set( struct st_FCRRPINF * ps_rprecinf, struct st_TMSTSFIL * ps_tmstsfil);	/* [20260515] Add */
int commfunc_rprec_msggettcp( char (*)[], int  );	/* [20260515] Add */
