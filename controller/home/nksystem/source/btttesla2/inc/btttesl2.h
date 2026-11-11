/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251008] 2025.10.08
 * ・ハートビート設定を追加
 * [20250421] 2025.04.21
 * ・RJ [20250206]版の通信プロセスをベースに新規作成
 * [20230312] 2023.03.12
 * ・バッテリメータとの通信が切れた場合、メータの全ての計測値が0となる為、
 *   積算値となる計測値を欠測にする
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
#define	PV_BTTTESLA2_TYPE1		1					/* 通信タイプ１ */
#define	PV_BTTTESLA2_TYPE2		2					/* 通信タイプ２（未使用） */
#define	PV_BTTTESLA2_TYPE3		3					/* 通信タイプ３（未使用） */

/* 要求タイプ */
#define	PV_BTTTESLA2_FUNC_01 0x01					/* ファンクション１（コイルの読出し） */
#define	PV_BTTTESLA2_FUNC_02 0x02					/* ファンクション２（入力ステータスの読出し） */
#define	PV_BTTTESLA2_FUNC_03 0x03					/* ファンクション３（保持レジスタの読出し） */
#define	PV_BTTTESLA2_FUNC_04 0x04					/* ファンクション４（入力レジスタの読出し） */
#define	PV_BTTTESLA2_FUNC_06 0x06					/* ファンクション６（単レジスタの書込み） */
#define	PV_BTTTESLA2_FUNC_10 0x10					/* ファンクション１０（複数レジスタの書込み） */

#define	PV_BTTTESLA2_REQ_CB			0x01			/* 通信復帰時 */
#define	PV_BTTTESLA2_REQ_NOR		0x02			/* 通常時 */
#define	PV_BTTTESLA2_REQ_AW			(PV_BTTTESLA2_REQ_CB|PV_BTTTESLA2_REQ_NOR)	/* 通常時＆通信復帰時 */

#define	CMD_GET_BATTERY_SV				4
#define	CMD_GET_BATTERY_METER			5
#define	CMD_GET_ALARM					6
#define	CMD_GET_REAL_POWER				8
#define	CMD_GET_BATT_INFO				9
#define	CMD_GET_FREQ_DROOP				10
#define	CMD_GET_BATT_SWITCH				11
#define	CMD_SET_POWER					101
#define	CMD_SET_POWER_FACT				102

#define	CMD_SET_MAXMIN_POWER			107
#define	CMD_SET_BTT_BLK_START			108
#define	CMD_SET_FREQ_DROOP				109
#define	CMD_SET_LOWPASS_FILTER			110
#define	CMD_SET_STATE_REQUEST			111
#define	CMD_SET_HEART_BEAT				112			/* [20251008] Add */

#define PV_BTTTESLA2_HEARTBEAT_TO	60				/* ハートビートタイムアウト(秒) */

/* 最大処理計測番号 */
#define	PV_BTTTESLA2_MAXTM		29					/* 計測情報数 */

#define PV_SVTYPE_BIT			1					/* 状態取得タイプ（8ビットタイプ） */
#define PV_SVTYPE_NUM			2					/* 状態取得タイプ（数値タイプ） */
#define PV_SVTYPE_MSK			3					/* 状態取得タイプ（マスクタイプ） */

#define PV_SET_POWER_EXP		0					/* 充放電電力設定値乗率 */

#define PV_BTTTESLA2_SVNUMB_REALPOWER_STOP	121			/* 出力停止 */
#define PV_BTTTESLA2_SVNUMB_REALPOWER_DIR	122			/* 手動制御 */

#define PV_BTTTESLA2_SV_AUTO_ACTIVE         113         /* Auto Active        */
#define PV_BTTTESLA2_SV_INTENTIONAL_READY	114			/* Intentional Ready  */
#define PV_BTTTESLA2_SV_INTENTIONAL_ISLAND	115			/* Intentional Island */
#define PV_BTTTESLA2_SV_PQ_ISLAND			116			/* PQ Island          */
#define PV_BTTTESLA2_SV_ISLANDOFF			117			/* Island Off         */

#define PV_BTTTESLA2_SV_ONGLID				113			/* 系統連系         */
#define PV_BTTTESLA2_SV_OFFGLID				114			/* 自立運転         */

#define PV_BTTTESLA2_PARAM1					"PowerFactorChargeSetpoint"
#define PV_BTTTESLA2_PARAM2					"PowerFactorDischargeSetpoint"
#define PV_BTTTESLA2_PARAM3					"DroopTimeResponse"


struct st_bttFactTm {
	int					index;						/* 周期Index */
	int					minDataNum;					/* 1分間のデータ数 */
	struct st_tmData {								/* 取得周期の計測情報 */
		int				tmFlg;						/* 計測フラグ */
		int				tmVal;						/* 計測値 */
	} s_tmData[600];
};

struct st_bttTeslaCalcInf {
	int					fullenNumb;					/* フル充電エネルギー */
	int					remainNumb;					/* 残電力量 */
	int					socNumb;					/* SoC */
	int					actPowNumb;					/* 有効電力 */
};

struct st_bttTeslaSdInf {
	unsigned char		funcCode;					/* ファンクションコード */
	unsigned char		startAdU;					/* 開始アドレス（上位バイト） */
	unsigned char		startAdL;					/* 開始アドレス（下位バイト） */
	unsigned char		dataCntU;					/* 要求データ数（上位バイト） */
	unsigned char		dataCntL;					/* 要求データ数（下位バイト） */
	unsigned char		dataLength;					/* 送信データ長 */
	unsigned char		reqKind;					/* 要求種別(0:電源ON時、1:TCP/IP接続時、2:通常時) */
	unsigned char		addrType;					/* レジスタ情報 */
};

struct st_stsCodeTbl {								/* コード番号状態変換用 */
	int					codNumb;					/* コード番号 */
	int					stsNumb;					/* 装置状態番号 */
};

struct st_stsMaskTbl {
	int					buffLink;					/* 応答データ（受信バッファ）格納位置 */
	int					svMask;						/* 状態取得マスク値 */
	int					bttSvLink;					/* 収集装置状態格納位置 */
	int					setMask;					/* 状態設定マスク値 */					
};

struct st_bttTeslaTmInf {
	int					reqCnt;						/* 要求回数（ｎ回目） */
	int 				buffLink;					/* 応答データ（受信バッファ）格納位置 */
	int 				dataSize;					/* 計測値データサイズ（バイト数） */
	int					bttTmNumb;					/* 計測番号 */
	int					type;						/* 0:unsigned, 1:int, 2:float */
};

struct st_bttTeslaSvInf {
	int					reqCnt;						/* 要求回数（ｎ回目） */
	int					buffLink;					/* 応答データ（受信バッファ）格納位置 */
	int					dataSize;					/* 処理対象データサイズ */
	int					svType;						/* 処理タイプ（0:ビットタイプ/1:数値タイプ） */
	int					bttSvLink;					/* 収集装置状態格納位置 */
	int					iniSize;					/* 初期化サイズ */
	void 				*ps_tblPointer;				/* テーブルポインタ */
};

struct st_stsMeterTmTbl {							/* メータ計測情報 */
	int					tmNumb;						/* 計測番号 */
	int					tntegratedFlg;				/* 積算値フラグ */
};

struct st_threadArg {
	int					indexNumb;					/* 処理中インデックス番号 */
	int					stFlg;						/* スレッド開始フラグ */
	int					edFlg;						/* スレッド終了フラグ */
	int					threadNumb;					/* スレッド番号 */
	int					lineNo;						/* 回線番号 */
};

struct st_extraParam {
	int					statFlag;					/* 実装有無 */
	float				chargeSetpoint;				/* 充電力率設定値 */
	int					chargeLead;					/* 充電力率進み/遅れ(進み:1,遅れ:-1) */
	float				dischargeSetpoint;			/* 放電力率設定値 */
	int					dischargeLead;              /* 放電力率進み/遅れ(進み:1,遅れ:-1) */
	float				droopTimeResponse;			/* 応答時間*/
};

/* 対応機種情報 */
/* 対応機種情報：型式名称 */
struct st_devTypeName	s_bttTypeName[] =
{
	/*	      +----------------------------------- 型式名称     */
	/*	      |                   +--------------- 通信処理方式 */
	/*	      |                   |           +--- 基本要求回数 */
	/*-1234567890123456           |           |    （型式名称は16文字以内で定義） */
	{ "TESLA-CONTROLS-V2",       PV_BTTTESLA2_TYPE1,  3 },/* TESLA製制御システム(Controls V2) */
	{ "-STOPPER-",                       -1, -1 }
};

/* SoC情報 */
struct st_bttTeslaCalcInf	s_typeCalcInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +-------------------------------------------  フル充電エネルギー */
	/*   |    +--------------------------------------  残電力量 */
	/*   |    |    +---------------------------------  SoC */
	/*   |    |    |    +----------------------------  有効電力 */
	{   11,  12,  29,  22 },								/* Type1：算出計測情報 */
	{   -1,  -1,  -1,  -1 },
};


/* IslandMode */
struct st_stsCodeTbl	s_stsCodeTbl_Type1_IslandStatus[] =
{
    /* +-------------------------- 取得値                                */
    /* |    +---------------------- 状態番号                             */
    /* |    |                                                            */
    /* |    |                               +---------  状態番号         */
    /* |    |                               |      +--- 状態名称         */
    /* |    |                               |      |                     */
    /* |    |                               |      |                     */
    {  0,  97 },                      /* 097 : On-Grid                   */
    {  1,  98 },                      /* 098 : Islanded - Active         */
    {  2,  99 },                      /* 099 : Islanded - Standby        */
    {  3, 100 },                      /* 100 : Off-grid to On-grid       */
    {  4, 101 },                      /* 101 : On-grid to Off-grid       */
    {  5, 102 },                      /* 102 : Islanded - Overload       */
    {  6, 103 },                      /* 103 : Islanded - Wait for Solar */
    {  7, 104 },                      /* 104 : Islanded - Wait for User  */
	{ -1,  -1 }					    /* STOPPER */
};

/* IslandStatus */
struct st_stsCodeTbl	s_stsCodeTbl_Type1_IslandMode[] =
{
    /* +-------------------------- 取得値                                */
    /* |    +---------------------- 状態番号                             */
    /* |    |                                                            */
    /* |    |                               +---------  状態番号         */
    /* |    |                               |      +--- 状態名称         */
    /* |    |                               |      |                     */
    /* |    |                               |      |                     */
    {  1, 113 },                      /* 113 :Auto Active                */
    {  2, 114 },                      /* 114 :Intentional Ready          */
    {  3, 115 },                      /* 115 :Intentional Island         */
    {  4, 116 },                      /* 116 :PQ Island                  */
    {  5, 117 },                      /* 117 :Island Off                 */
	{ -1,  -1 }					    /* STOPPER */
};

/* real power command mode */
struct st_stsCodeTbl	s_stsCodeTbl_Type1_RealPowerCommnad[] =
{
    /* +-------------------------- 取得値                                */
    /* |    +---------------------- 状態番号                             */
    /* |    |                                                            */
    /* |    |                               +---------  状態番号         */
    /* |    |                               |      +--- 状態名称         */
    /* |    |                               |      |                     */
    /* |    |                               |      |                     */
    {  0, 121 },                      /* 121 :出力無し					 */
    {  1, 122 },                      /* 122 :手動制御					 */
    {  2, 123 },                      /* 123 :サイト管理制御			 */
    {  3, 124 },                      /* 124 :エネルギー目標制御		 */
    {  4, 125 },                      /* 125 :テスラのオプティキャスター */
    {  5, 126 },                      /* 126 :スケジュール制御			 */
	{ -1,  -1 }					    /* STOPPER */
};

/* メータ無効 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_meterInvalid[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,      1 },                  /*   1 : バッテリメータ：メータ無効なし                              */
    {    1,      2 },                  /*   2 : バッテリメータ：メータ無効あり                              */
    {   -1,     -1 }                   /* STOPPER */
};

/* TSC間 通信障害 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_meterMia[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,      9 },                  /*   9 : バッテリメータ：TSC間 通信障害なし                          */
    {    1,     10 },                  /*  10 : バッテリメータ：TSC間 通信障害あり                          */
    {   -1,     -1 }                   /* STOPPER */
};

/* CT/PT 障害 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_meterCtPtFailure[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     17 },                  /*  17 : バッテリメータ：CT/PT 障害なし                           */
    {    1,     18 },                  /*  18 : バッテリメータ：CT/PT 障害あり                           */
    {   -1,     -1 }                   /* STOPPER */
};

/* 電圧／周波数範囲外 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_meterOutOfBounds[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     25 },                  /*  25 : バッテリメータ：電圧／周波数範囲外なし                          */
    {    1,     26 },                  /*  26 : バッテリメータ：電圧／周波数範囲外あり                          */
    {   -1,     -1 }                   /* STOPPER */
};

/* 電圧／周波数基準値範囲超過 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_meterUnreasonable[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     33 },                  /*  33 : バッテリメータ：電圧／周波数基準値範囲超過なし                      */
    {    1,     34 },                  /*  34 : バッテリメータ：電圧／周波数基準値範囲超過あり                      */
    {   -1,     -1 }                   /* STOPPER */
};

/* Battery Meter Is part of aggregate */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_meterIsPartOfAggregate[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     41 },                  /*  41 : バッテリメータ：Battery Meter Is part of aggregateなし */
    {    1,     42 },                  /*  42 : バッテリメータ：Battery Meter Is part of aggregateあり */
    {   -1,     -1 }                   /* STOPPER */
};

/* 有効電力上限超過 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_realPowerAvailableLimited[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     49 },                  /*  49 : 有効電力上限：有効電力上限超過なし                            */
    {    1,     50 },                  /*  50 : 有効電力上限：有効電力上限超過あり                            */
    {   -1,     -1 }                   /* STOPPER */
};

/* 無効電力上限超過 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_reactivePowerAvailableLimited[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     57 },                  /*  57 : 無効電力上限：無効電力上限超過なし                            */
    {    1,     58 },                  /*  58 : 無効電力上限：無効電力上限超過あり                            */
    {   -1,     -1 }                   /* STOPPER */
};

/* 皮相電力低下 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_notSubstantiallyAvailable[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     65 },                  /*  65 : 皮相電力低下：皮相電力低下なし                              */
    {    1,     66 },                  /*  66 : 皮相電力低下：皮相電力低下あり                              */
    {   -1,     -1 }                   /* STOPPER */
};

/* 無通信コントローラ停止 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_customerCommandTimedOut[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     73 },                  /*  73 : 無通信コントローラ停止：無通信コントローラ停止なし                    */
    {    1,     74 },                  /*  74 : 無通信コントローラ停止：無通信コントローラ停止あり                    */
    {   -1,     -1 }                   /* STOPPER */
};

/* 周波数ドループ */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_frequencyDroopEnable[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     81 },                  /*  81 : 周波数ドループ：無効                                   */
    {    1,     82 },                  /*  82 : 周波数ドループ：有効                                   */
    {   -1,     -1 }                   /* STOPPER */
};

/* 最大電力 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_peakPowerEnable[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     89 },                  /*  89 : 最大電力：無効                                      */
    {    1,     90 },                  /*  90 : 最大電力：有効                                      */
    {   -1,     -1 }                   /* STOPPER */
};

/* ヒートモード */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_heatModeEnable[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,     97 },                  /*  97 : ヒートモード：無効                                    */
    {    1,     98 },                  /*  98 : ヒートモード：有効                                    */
    {   -1,     -1 }                   /* STOPPER */
};

/* バッテリーの状態 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_stateRequest[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,    105 },                  /* 105 : バッテリーの状態：Off                                 */
    {    1,    106 },                  /* 106 : バッテリーの状態：On                                  */
    {   99,    107 },                  /* 107 : バッテリーの状態：緊急 Off                              */
    {   -1,     -1 }                   /* STOPPER */
};

/* バッテリーのモード */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_modeRequest[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    1,    113 },                  /* 113 : バッテリーのモード：グリッド追従(系統連系)                       */
    {    2,    114 },                  /* 114 : バッテリーのモード：グリッド形成(自立運転)                       */
    {   -1,     -1 }                   /* STOPPER */
};

/* 電力系統をブラック スタート */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_blackStartRequest[] =
{
    /*   +------------------------ 取得値                                     */
    /*   |       +---------------- 状態番号                                   */
    /*   |       |                                                            */
    /*   |       |                          +--- 状態番号                     */
    /*   |       |                          |      +--- 状態名称              */
    /*   |       |                          |      |                          */
    /*   |       |                          |      |                          */
    {    0,    121 },                  /* 121 : 電力系統をブラック スタート：ブラック スタートなし                   */
    {    1,    122 },                  /* 122 : 電力系統をブラック スタート：ブラック スタート                     */
    {   -1,     -1 }                   /* STOPPER */
};

/* [20230312] Add St */
/* バッテリメータ計測情報 */
struct st_stsMeterTmTbl	s_stsMeterTbl_Type1_Battery[] =
{
    /* +-------------------------- 計測番号                             */
    /* |   +---------------------- 積算値フラグ                         */
    { 22,  0 },                   /* 22 : バッテリメータ：3相有効電力合計    */
    { 23,  0 },                   /* 23 : バッテリメータ：3相無効電力合計    */
    { 24,  0 },                   /* 24 : バッテリメータ：3相皮相電力合計    */
    { 25,  0 },                   /* 25 : バッテリメータ：平均線間 RMS 電圧    */
    { 26,  1 },                   /* 26 : バッテリメータ：周波数    */
    { 27,  1 },                   /* 27 : バッテリメータ：充電電力(積算)    */
    { 28,  1 },                   /* 28 : バッテリメータ：放電電力(積算)    */
	{ -1, -1 }					  /* STOPPER */
};
/* [20230312] Add Ed */

/* 計測値レンジ１（PV_BTTTESLA2_TYPE1、PV_BTTTESLA2_TYPE2 用）*/
struct st_devTmRange	s_type1TmRange[] =
{
	/*      +------------------------------------------- 最小計測値 */
	/*      |           +------------------------------- 最大計測値 */
    { -2147483647, 2147483647 },        /* 01 : 有効電力指令値    */
    { -2147483647, 2147483647 },        /* 02 : 有効電力変化率    */
    { -2147483647, 2147483647 },        /* 03 : 実有効電力    */
    { -2147483647, 2147483647 },        /* 04 : 周波数ドループ：基準周波数    */
    { -2147483647, 2147483647 },        /* 05 : 周波数ドループ：不感帯(上限)    */
    { -2147483647, 2147483647 },        /* 06 : 周波数ドループ：不感帯(下限)    */
    { -2147483647, 2147483647 },        /* 07 : 周波数ドループ：比例ゲイン(上限)    */
    { -2147483647, 2147483647 },        /* 08 : 周波数ドループ：比例ゲイン(下限)    */
    { -2147483647, 2147483647 },        /* 09 : 周波数ドループ：周波数出力    */
    { -2147483647, 2147483647 },        /* 10 : 周波数ドループ：ローパスフィルター応答時間    */
    { -2147483647, 2147483647 },        /* 11 : バッテリー情報：フル充電エネルギー    */
    { -2147483647, 2147483647 },        /* 12 : バッテリー情報：残電力量    */
    { -2147483647, 2147483647 },        /* 13 : バッテリー情報：充電可能電力    */
    { -2147483647, 2147483647 },        /* 14 : バッテリー情報：放電可能電力    */
    { -2147483647, 2147483647 },        /* 15 : バッテリー情報：皮相電力    */
    {      -32768,      32767 },        /* 16 : バッテリー情報：利用可能Megapack台数    */
    { -2147483647, 2147483647 },        /* 17 : バッテリー情報：補助負荷電力    */
    { -2147483647, 2147483647 },        /* 18 : バッテリー情報：ブラックスタート皮相電力    */
    { -2147483647, 2147483647 },        /* 19 : バッテリー情報：ハードウェア有効充電電力    */
    { -2147483647, 2147483647 },        /* 20 : バッテリー情報：ハードウェア有効放電電力    */
    { -2147483647, 2147483647 },        /* 21 : バッテリー情報：ハードウェア有効皮相電力    */
    { -2147483647, 2147483647 },        /* 22 : バッテリメータ：3相有効電力合計    */
    { -2147483647, 2147483647 },        /* 23 : バッテリメータ：3相無効電力合計    */
    { -2147483647, 2147483647 },        /* 24 : バッテリメータ：3相皮相電力合計    */
    { -2147483647, 2147483647 },        /* 25 : バッテリメータ：平均線間 RMS 電圧    */
    { -2147483647, 2147483647 },        /* 26 : バッテリメータ：周波数    */
    { -2147483647, 2147483647 },        /* 27 : バッテリメータ：充電電力(積算)    */
    { -2147483647, 2147483647 },        /* 28 : バッテリメータ：放電電力(積算)    */
    {           0,        100 },        /* 29 : SoC    */
};

/* 要求送信設定 */
struct st_bttTeslaSdInf		s_type1SdInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +-------------------------------------------------- 要求時のファンクションコード */
	/*   |                  +------------------------------- 開始アドレス（上位バイト）   */
	/*   |                  |     +------------------------- 開始アドレス（下位バイト）   */
	/*   |                  |     |     +------------------- 要求データ数（上位バイト）   */
	/*   |                  |     |     |     +------------- 要求データ数（下位バイト）   */
	/*   |                  |     |     |     |     +------- 送信データ長                 */
	/*   |                  |     |     |     |     |     +- 要求種別                     */
	/*   |                  |     |     |     |     |     |                     +- レジスタ情報        */
	/*   |                  |     |     |     |     |     |                     |         */
	{ PV_BTTTESLA2_FUNC_10, 0x50, 0x15, 0x00, 0x04, 0x15, PV_BTTTESLA2_REQ_CB, CMD_SET_MAXMIN_POWER		 },	/*   1回目：有効充放電電力の設定						*//* Droop機能を使用しない為、スキップ */
	{ PV_BTTTESLA2_FUNC_03, 0x4E, 0xE9, 0x00, 0x06, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_REAL_POWER		 },	/*   2回目：有効電力の取得								*/
	{ PV_BTTTESLA2_FUNC_03, 0x61, 0xA9, 0x00, 0x15, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_BATT_INFO		 },	/*   3回目：バッテリー情報の取得						*/
	{ PV_BTTTESLA2_FUNC_03, 0x7D, 0x0E, 0x00, 0x06, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_BATTERY_SV		 },	/*   4回目：バッテリメータ状態情報の取得				*/
	{ PV_BTTTESLA2_FUNC_03, 0x69, 0x79, 0x00, 0x04, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_ALARM			 },	/*   5回目：制御アラートの取得							*/
	{ PV_BTTTESLA2_FUNC_03, 0x4F, 0xB1, 0x00, 0x68, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_FREQ_DROOP		 },	/*   6回目：周波数ドループ、有効電力制限の取得			*//* Droop機能を使用しない為、スキップ */
	{ PV_BTTTESLA2_FUNC_03, 0x52, 0x09, 0x00, 0x05, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_BATT_SWITCH		 },	/*   7回目：バッテリースイッチの取得					*/
	{ PV_BTTTESLA2_FUNC_03, 0x7D, 0x00, 0x00, 0x0E, 0x0C, PV_BTTTESLA2_REQ_AW, CMD_GET_BATTERY_METER	 },	/*   8回目：バッテリメータ状態情報計測値の取得			*/
	{ PV_BTTTESLA2_FUNC_10, 0x4E, 0xE9, 0x00, 0x04, 0x15, PV_BTTTESLA2_REQ_AW, CMD_SET_POWER			 },	/*   9回目：有効電力の設定								*/
	{ PV_BTTTESLA2_FUNC_10, 0x4F, 0xCA, 0x00, 0x02, 0x11, PV_BTTTESLA2_REQ_CB, CMD_SET_LOWPASS_FILTER	 },	/*  10回目：周波数ドループ(ローパスフィルター応答時間)の設定	*//* Droop機能を使用しない為、スキップ */
	{ PV_BTTTESLA2_FUNC_10, 0x4F, 0xB1, 0x00, 0x0B, 0x23, PV_BTTTESLA2_REQ_AW, CMD_SET_FREQ_DROOP		 },	/*  11回目：周波数ドループの設定						*//* Droop機能を使用しない為、スキップ */
	{ PV_BTTTESLA2_FUNC_10, 0x51, 0x41, 0x00, 0x07, 0x1B, PV_BTTTESLA2_REQ_CB, CMD_SET_POWER_FACT		 },	/*  12回目：力率の設定									*/
	{ PV_BTTTESLA2_FUNC_10, 0x52, 0x0B, 0x00, 0x01, 0x0F, PV_BTTTESLA2_REQ_AW, CMD_SET_STATE_REQUEST	 },	/*  13回目：状態指令の設定   							*//* Droop機能を使用しない為、スキップ */
	{ PV_BTTTESLA2_FUNC_10, 0x6D, 0x63, 0x00, 0x01, 0x0F, PV_BTTTESLA2_REQ_AW, CMD_SET_HEART_BEAT		 },	/*  14回目：ハートビートの設定							*//* [20251008] Add */
	{ 0xFF,                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                0xFF }	/* STOPPER */
};


/* 応答情報取得（計測情報） */
struct st_bttTeslaTmInf		s_type1TmInf[] =		/* 処理タイプ１の計測値取得情報 */
{
	/* +---------------------------------------------- 要求ｎ回目                                       */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置（上位バイト） */
	/* |    |   +------------------------------------- 計測値のデータバイト数                           */
	/* |    |   |   +--------------------------------- バッテリー計測番号                     */
	/* |    |   |   |   +----------------------------- データ型(0:unsigned, 1:int, 2:float)             */
    {  2,   9,  4,  1,  2 },                        /* 001 : バッテリーダイレクト：有効電力指令値       */
    {  2,  13,  4,  2,  2 },                        /* 002 : バッテリーダイレクト：有効電力変化率       */
    {  2,  17,  4,  3,  2 },                        /* 003 : バッテリーダイレクト：実有効電力           */
    {  6,  11,  4,  4,  2 },                        /* 004 : 周波数ドループ：基準周波数                 */
    {  6,  15,  4,  5,  2 },                        /* 005 : 周波数ドループ：不感帯(上限)               */
    {  6,  19,  4,  6,  2 },                        /* 006 : 周波数ドループ：不感帯(下限)               */
    {  6,  23,  4,  7,  2 },                        /* 007 : 周波数ドループ：比例ゲイン(上限)           */
    {  6,  27,  4,  8,  2 },                        /* 008 : 周波数ドループ：比例ゲイン(下限)           */
    {  6,  31,  4,  9,  2 },                        /* 009 : 周波数ドループ：周波数出力                 */
    {  6,  59,  4, 10,  2 },                        /* 010 : 周波数ドループ：ローパスフィルター応答時間 */
    {  3,   9,  4, 11,  2 },                        /* 011 : バッテリー情報：フル充電エネルギー         */
    {  3,  13,  4, 12,  2 },                        /* 012 : バッテリー情報：残電力量                   */
    {  3,  17,  4, 13,  2 },                        /* 013 : バッテリー情報：充電可能電力               */
    {  3,  21,  4, 14,  2 },                        /* 014 : バッテリー情報：放電可能電力               */
    {  3,  25,  4, 15,  2 },                        /* 015 : バッテリー情報：皮相電力                   */
    {  3,  29,  2, 16,  1 },                        /* 016 : バッテリー情報：利用可能Megapack台数       */
    {  3,  31,  4, 17,  2 },                        /* 017 : バッテリー情報：補助負荷電力               */
    {  3,  35,  4, 18,  2 },                        /* 018 : バッテリー情報：ブラックスタート皮相電力   */
    {  3,  39,  4, 19,  2 },                        /* 019 : バッテリー情報：ハードウェア有効充電電力   */
    {  3,  43,  4, 20,  2 },                        /* 020 : バッテリー情報：ハードウェア有効放電電力   */
    {  3,  47,  4, 21,  2 },                        /* 021 : バッテリー情報：ハードウェア有効皮相電力   */
    {  8,   9,  4, 22,  2 },                        /* 022 : バッテリメータ：3相有効電力合計            */
    {  8,  13,  4, 23,  2 },                        /* 023 : バッテリメータ：3相無効電力合計            */
    {  8,  17,  4, 24,  2 },                        /* 024 : バッテリメータ：3相皮相電力合計            */
    {  8,  21,  4, 25,  2 },                        /* 025 : バッテリメータ：平均線間 RMS 電圧          */
    {  8,  25,  4, 26,  2 },                        /* 026 : バッテリメータ：周波数                     */
    {  8,  29,  4, 27,  2 },                        /* 027 : バッテリメータ：充電電力(積算)             */
    {  8,  33,  4, 28,  2 },                        /* 028 : バッテリメータ：放電電力(積算)             */
    { -1,  -1, -1, -1, -1 }                         /* STOPPER */
};


/* 応答情報取得（状態情報） */
struct st_bttTeslaSvInf		s_type1SvInf[] =		/* 処理タイプ１の状態取得情報               */
{
	/* +---------------------------------------------- 要求ｎ回目                               */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置       */
	/* |    |   +------------------------------------- 処理対象データサイズ                     */
	/* |    |   |        +---------------------------- 処理タイプ（1:ビットタイプ/2:数値タイプ）*/
	/* |    |   |        |         +------------------ 状態情報格納開始位置                     */
	/* |    |   |        |         |   +-------------- 状態初期化サイズ（数値タイプの場合）     */
	/* |    |   |        |         |   |     +-------- テーブルポインタ                         */
	/* |    |   |        |         |   |     |                                                  */
	{  4,   9,  2, PV_SVTYPE_NUM,  0,  1, (void *)s_stsCodeTbl_Type1_meterInvalid					 },	/* バッテリメータ：無効									*/
	{  4,  11,  2, PV_SVTYPE_NUM,  1,  1, (void *)s_stsCodeTbl_Type1_meterMia						 },	/* バッテリメータ：TSC間 通信障害						*/
	{  4,  13,  2, PV_SVTYPE_NUM,  2,  1, (void *)s_stsCodeTbl_Type1_meterCtPtFailure				 },	/* バッテリメータ：CT/PT 障害							*/
	{  4,  15,  2, PV_SVTYPE_NUM,  3,  1, (void *)s_stsCodeTbl_Type1_meterOutOfBounds				 },	/* バッテリメータ：電圧／周波数範囲外					*/
	{  4,  17,  2, PV_SVTYPE_NUM,  4,  1, (void *)s_stsCodeTbl_Type1_meterUnreasonable				 },	/* バッテリメータ：電圧／周波数基準値範囲超過			*/
	{  4,  19,  2, PV_SVTYPE_NUM,  5,  1, (void *)s_stsCodeTbl_Type1_meterIsPartOfAggregate			 },	/* バッテリメータ：Battery Meter Is part of aggregate	*/
	{  5,   9,  2, PV_SVTYPE_NUM,  6,  1, (void *)s_stsCodeTbl_Type1_realPowerAvailableLimited		 },	/* 制御アラート：有効電力上限							*/
	{  5,  11,  2, PV_SVTYPE_NUM,  7,  1, (void *)s_stsCodeTbl_Type1_reactivePowerAvailableLimited	 },	/* 制御アラート：無効電力上限							*/
	{  5,  13,  2, PV_SVTYPE_NUM,  8,  1, (void *)s_stsCodeTbl_Type1_notSubstantiallyAvailable		 },	/* 制御アラート：皮相電力低下							*/
	{  5,  15,  2, PV_SVTYPE_NUM,  9,  1, (void *)s_stsCodeTbl_Type1_customerCommandTimedOut		 },	/* 制御アラート：無通信コントローラ停止					*/
	{  6,   9,  2, PV_SVTYPE_NUM, 10,  1, (void *)s_stsCodeTbl_Type1_frequencyDroopEnable			 },	/* 周波数ドループ：有効/無効							*/
	{  7,   9,  2, PV_SVTYPE_NUM, 11,  1, (void *)s_stsCodeTbl_Type1_peakPowerEnable				 },	/* バッテリースイッチ：最大電力有効/無効				*/
	{  7,  11,  2, PV_SVTYPE_NUM, 12,  1, (void *)s_stsCodeTbl_Type1_heatModeEnable					 },	/* バッテリースイッチ：ヒートモード有効/無効			*/
	{  7,  13,  2, PV_SVTYPE_NUM, 13,  1, (void *)s_stsCodeTbl_Type1_stateRequest					 },	/* バッテリースイッチ：状態指令							*/
	{  7,  15,  2, PV_SVTYPE_NUM, 14,  1, (void *)s_stsCodeTbl_Type1_modeRequest					 },	/* バッテリースイッチ：モード指令						*/
	{  7,  17,  2, PV_SVTYPE_NUM, 15,  1, (void *)s_stsCodeTbl_Type1_blackStartRequest				 },	/* バッテリースイッチ：ブラックスタート指令				*/
	{ -1,  -1, -1,            -1, -1, -1, NULL   }		/* STOPPER */
};
