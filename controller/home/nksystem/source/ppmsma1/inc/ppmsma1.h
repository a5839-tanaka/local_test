/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260610] 2026.06.10
 * ・要求3回目の周波数の取得を削除（周波数は要求1回目で取得）
 * [20260123] 2026.01.23
 * ・イベント/失敗コードの状態→計測値変更
 * ・追加レジスタ対応
 * [20251121] 2025.11.21
 * ・制御モードコマンドの読込定義修正
 * [20251121] 2025.11.21
 * ・力率設定値の設定誤りを修正
 * [20251101] 2025.11.01
 * ・北広島[20251031]版の通信プロセスをベースに新規作成
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 研究開発案件
 * -----------------------------------------------------------------------------
 * 	SMA製PPM通信プロセス
 * （タイプ１：Ｅｔｈｅｒｎｅｔ：ＭｏｄｂｕｓＴＣＰ）
 * 対応機種：
 *
 */

/* 通信タイプ（プロトコル判定用） */
#define	PV_PPMSMA01_TYPE1		1					/* 通信タイプ１ */
#define	PV_PPMSMA01_TYPE2		2					/* 通信タイプ２（未使用） */
#define	PV_PPMSMA01_TYPE3		3					/* 通信タイプ３（未使用） */

/* 要求タイプ */
#define	PV_PPMSMA01_FUNC_01 0x01					/* ファンクション１（コイルの読出し） */
#define	PV_PPMSMA01_FUNC_02 0x02					/* ファンクション２（入力ステータスの読出し） */
#define	PV_PPMSMA01_FUNC_03 0x03					/* ファンクション３（保持レジスタの読出し） */
#define	PV_PPMSMA01_FUNC_04 0x04					/* ファンクション４（入力レジスタの読出し） */
#define	PV_PPMSMA01_FUNC_06 0x06					/* ファンクション６（単レジスタの書込み） */
#define	PV_PPMSMA01_FUNC_10 0x10					/* ファンクション１０（複数レジスタの書込み） */

#define	PV_PPMSMA01_REQ_CB			0x01			/* 通信復帰時 */
#define	PV_PPMSMA01_REQ_NOR			0x02			/* 通常時 */
#define	PV_PPMSMA01_REQ_AW			(PV_PPMSMA01_REQ_CB|PV_PPMSMA01_REQ_NOR)	/* 通常時＆通信復帰時 */

#define	CMD_GET_BATTERY_TM				1
#define	CMD_GET_SOC						2
#define	CMD_GET_FREQ					3
#define	CMD_GET_BATTERY_SV				4
#define	CMD_GET_BATTERY_METER			5
#define	CMD_GET_ALARM					6
#define	CMD_GET_REAL_POWER				8
#define	CMD_GET_BATT_INFO				9
#define	CMD_GET_FREQ_DROOP				10
#define	CMD_GET_BATT_SWITCH				11
#define	CMD_SET_POWER					101
#define	CMD_SET_POWER_FACT				102
#define	CMD_SET_REACTIVE_POWER			103

#define	CMD_SET_MAXMIN_POWER			107
#define	CMD_SET_BTT_BLK_START			108
#define	CMD_SET_FREQ_DROOP				109
#define	CMD_SET_LOWPASS_FILTER			110
#define	CMD_SET_STATE_REQUEST			111
#define	CMD_SET_HEART_BEAT				112
#define	CMD_SET_POI_PWR_AT_CTRL_MODE	113			/* [20260123] Add */

/* [20260123] Add St */
#define	CMD_POI_PWR_AT_CTRL_MODE_NONE	0			/* なし */
#define	CMD_POI_PWR_AT_CTRL_MODE_PV		1			/* 太陽光 */
#define	CMD_POI_PWR_AT_CTRL_MODE_BATT	2			/* 蓄電池 */
#define	CMD_POI_PWR_AT_CTRL_MODE_AUTO	3			/* 自動 */
/* [20260123] Add Ed */

#define PV_PPMSMA01_HEARTBEAT_TO	60				/* ハートビートタイムアウト(秒) */

#define	CMD_REACTPOW_OFF							303		/* Off (Q constant): 303 */
#define	CMD_REACTPOW_Q_V_							1069	/* Q(V): 1069 */
#define	CMD_REACTPOW_Q_V_PLUS_Q						1070	/* Q(V) + Q setpoint: 1070 */
#define	CMD_REACTPOW_Q_VOLTAGELIMIT					1071	/* Q setpoint with voltage limitation: 1072 */
#define	CMD_REACTPOW_Q_P_							1073	/* Q(P): 1073 */
#define	CMD_REACTPOW_FIX_COSPHI						1074	/* Fixed CosPhi: 1074 */
#define	CMD_REACTPOW_COSPHI_P_						1076	/* CosPhi(P): 1076 */
#define	CMD_REACTPOW_FIX_COSPHI_PLUS_Q_V_			1077	/* Fixed CosPhi + Q(V): 1077 */
#define	CMD_REACTPOW_FIX_COSPHI_PLUS_VOLTAGELIMIT	1078	/* Fixed CosPhi + Voltage limitation: 1078 */
#define	CMD_REACTPOW_Q_P__PLUS_Q_V_					1079	/* Q(P) + Q(V): 1079 */
#define	CMD_REACTPOW_Q_P__PLUS_VOLTAGELIMIT			1080	/* Q(P) + Voltage limitation: 1080 */
#define	CMD_REACTPOW_COSPHI_P__PLUS_Q_V_			1081	/* CosPhi(P) + Q(V): 1081*/
#define	CMD_REACTPOW_COSPHI_P__PLUS_VOLTAGELIMIT	1082	/* CosPhi(P) + Voltage limitation: 1082 */
#define	CMD_REACTPOW_GRID_VOLTAGE_CTRL				1084	/* Grid voltage control: 1984 */

/* 最大処理計測番号 */
#define	PV_PPMSMA01_MAXTM		30					/* 計測情報数 */

#define PV_SVTYPE_BIT			1					/* 状態取得タイプ（8ビットタイプ） */
#define PV_SVTYPE_NUM			2					/* 状態取得タイプ（数値タイプ） */
#define PV_SVTYPE_MSK			3					/* 状態取得タイプ（マスクタイプ） */

#define PV_SET_POWER_EXP		0					/* 充放電電力設定値乗率 */

/* [20260123] Add St */
/* フラグ制御 */
#define	PV_FLG_OFF				0
#define	PV_FLG_ON				1
/* [20260123] Add Ed */

#define PV_PPMSMA01_SVNUMB_REALPOWER_STOP	121			/* 出力停止 */
#define PV_PPMSMA01_SVNUMB_REALPOWER_DIR	122			/* 手動制御 */

#define PV_PPMSMA01_SV_AUTO_ACTIVE			113         /* Auto Active        */
#define PV_PPMSMA01_SV_INTENTIONAL_READY	114			/* Intentional Ready  */
#define PV_PPMSMA01_SV_INTENTIONAL_ISLAND	115			/* Intentional Island */
#define PV_PPMSMA01_SV_PQ_ISLAND			116			/* PQ Island          */
#define PV_PPMSMA01_SV_ISLANDOFF			117			/* Island Off         */

#define PV_PPMSMA01_SV_ONGLID				35			/* 系統連系         */
#define PV_PPMSMA01_SV_OFFGLID				36			/* 自立運転         */

#define PV_PPMSMA01_PARAM1					"PowerFactorChargeSetpoint"
#define PV_PPMSMA01_PARAM2					"PowerFactorDischargeSetpoint"
/* [20260123] Chg St */
#define PV_PPMSMA01_PARAM3					"WriteCtrlCount"
/* [20260123] Chg Ed */

#define PV_PPMCTMODE_OPERATING				309			/* 制御モードコマンド：運転中 */
#define LIFESIGNMAX							65535		/* ライフサイン上限 */
#define PV_DISCHG_POWERFACTOREXT_OVEREXCITED	1041	/* 放電力率励起：進み *//* [20251121] Chg コメント修正 */
#define PV_DISCHG_POWERFACTOREXT_UNDEREXCITED	1042	/* 放電力率励起：遅れ *//* [20251121] Chg コメント修正 */
#define PV_CHG_POWERFACTOREXT_OVEREXCITED		1041	/* 充電力率励起：進み *//* [20251121] Chg コメント修正 */
#define PV_CHG_POWERFACTOREXT_UNDEREXCITED		1042	/* 充電力率励起：遅れ *//* [20251121] Chg コメント修正 */

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

struct st_bttPpmsmaSdInf {
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

struct st_bttPpmsmaTmInf {
	int					reqCnt;						/* 要求回数（ｎ回目） */
	int 				buffLink;					/* 応答データ（受信バッファ）格納位置 */
	int 				dataSize;					/* 計測値データサイズ（バイト数） */
	int					bttTmNumb;					/* 計測番号 */
	int					type;						/* 0:unsigned, 1:int, 2:float */
};

struct st_bttPpmsmaSvInf {
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
/* [20260123] Chg St */
	int					writeCtrlCount;				/* 書込み経過時間 */
/* [20260123] Chg Ed */
};

/* 対応機種情報 */
/* 対応機種情報：型式名称 */
struct st_devTypeName	s_bttTypeName[] =
{
	/*	      +----------------------------------- 型式名称     */
	/*	      |                   +--------------- 通信処理方式 */
	/*	      |                   |           +--- 基本要求回数 */
	/*-1234567890123456           |           |    （型式名称は16文字以内で定義） */
	{ "PPM-10",           PV_PPMSMA01_TYPE1,  5 },/* SMA製PPM */
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

/* 操作ステータスの健全性 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_operationStatusHealth[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{   35,      1 },				   /*   1 : アラーム					  */
	{  303,      2 },				   /*   2 : OFF							  */
	{  307,      3 },				   /*   3 : OK							  */
	{  455,      4 },				   /*   4 : 警告						  */
	{   -1,     -1 }				/* STOPPER */
};

/* イベント/失敗コード *//* [20260123] Del */

/* 操作状態 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_operationState[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{  381,     17 },				   /*  17 : 停止						  */
	{ 1392,     18 },				   /*  18 : 異常						  */
	{ 1466,     19 },				   /*  19 : 待機						  */
	{ 1467,     20 },				   /*  20 : 運転						  */
	{ 1787,     21 },				   /*  21 : 初期						  */
	{ 3129,     22 },				   /*  22 : 手動						  */
	{ 3743,     23 },				   /*  23 : 無効						  */
	{ 3747,     24 },				   /*  24 : セーフモード				  */
	{ 3752,     25 },				   /*  25 : 起動						  */
	{   -1,     -1 } 				/* STOPPER */
};

/* 系統接続 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_gridConnection[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{  973,     33 },					/*  33 : 無効						  */
	{ 1779,     34 },					/*  34 : 切断						  */
	{ 1780,     35 },					/*  35 : 接続						  */
	{ 1781,     36 },					/*  36 : 自立						  */
	{   -1,     -1 }				/* STOPPER */
};

/* 無効電力制御 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_currentReactivePowerControlMode[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{  303,     41 },				   /*  41 : オフ（無効電力一定）				  */
	{ 1069,     42 },				   /*  42 : Q(V)特性							  */
	{ 1070,     43 },				   /*  43 : Q(V)特性と定値Q						  */
	{ 1072,     44 },				   /*  44 : 電圧制限付きの無効電力(Q)			  */
	{ 1073,     45 },				   /*  45 : Q(P)特性							  */
	{ 1074,     46 },				   /*  46 : 固定力率(CosΦ)						  */
	{ 1076,     47 },				   /*  47 : 有効電力(P)対応力率(CosΦ)			  */
	{ 1077,     48 },				   /*  48 : 固定力率(CosΦ)とQ(V)				  */
	{ 1078,     49 },				   /*  49 : 固定力率(CosΦ)と電圧制限			  */
	{ 1079,     50 },				   /*  50 : Q(P)特性とQ(V)特性					  */
	{ 1080,     51 },				   /*  51 : Q(P)特性と電圧制限					  */
	{ 1081,     52 },				   /*  52 : 有効電力(P)対応力率(CosΦ)とQ(V)特性  */
	{ 1082,     53 },				   /*  53 : 有効電力(P)対応力率(CosΦ)と電圧制限  */
	{ 1984,     54 },				   /*  54 : 電力系統の電圧制御					  */
	{   -1,     -1 } 				/* STOPPER */
};

/* 電力周波数応答機能 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_powerFrequencyResponseFrqDroopActive[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{    0,     57 },				   /*  57 : 無効						  */
	{    1,     58 },				   /*  58 : 有効						  */
	{   -1,     -1 }				/* STOPPER */
};

/* 故障時運転継続機能 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_faultRideThroughActive[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{    0,     65 },				   /*  65 : 無効						  */
	{    1,     66 },				   /*  66 : 有効						  */
	{   -1,     -1 }				/* STOPPER */
};

/* HYSの状態 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_hybridSystemState[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
	{ 1788,     73 },				   /*  73 : アイドル状態				  */
	{ 4453,     74 },				   /*  74 : 発電機の単独運転			  */
	{ 4454,     75 },				   /*  75 : 蓄電池の単独運転			  */
	{ 4455,     76 },				   /*  76 : 系統連系運転				  */
	{   -1,     -1 }				/* STOPPER */
};

/* 制御モードコマンドの読込 */
struct st_stsCodeTbl    s_stsCodeTbl_Type1_readingBackOfControlModeCommand[] =
{
	/*   +------------------------ 取得値                                     */
	/*   |       +---------------- 状態番号                                   */
	/*   |       |                                                            */
	/*   |       |                          +--- 状態番号                     */
	/*   |       |                          |      +--- 状態名称              */
	/*   |       |                          |      |                          */
	/*   |       |                          |      |                          */
/* [20251121] Chg Start 定義修正 */
	{  309,     81 },				   /*  81 : 運転可能					  */
	{  381,     82 },				   /*  82 : 停止中						  */
	{  569,     83 },				   /*  83 : 運転中						  */
	{ 1466,     84 },				   /*  84 : 待機中						  */
	{ 1690,     85 },				   /*  85 : 緊急停止中					  */
	{ 1749,     86 },				   /*  86 : 緩停止中					  */
	{ 1790,     87 },				   /*  87 : 同期						  */
	{ 3923,     88 },				   /*  88 : 非同期						  */
	{ 4451,     89 },				   /*  89 : ブラックスタート			  */
	{ 4452,     90 },				   /*  90 : 遷移中断					  */
	{ 4470,     91 },				   /*  91 : 停止→待機					  */
/* [20251121] Chg End 定義修正 */
	{   -1,     -1 }				/* STOPPER */
};

/* 計測値レンジ１（PV_PPMSMA01_TYPE1、PV_PPMSMA01_TYPE2 用）*/
struct st_devTmRange	s_type1TmRange[] =
{
	/*          +------------------- 最小計測値 */
	/*          |           +------- 最大計測値 */
	{ -2147483647, 2147483647 },		/* 01 : バッテリー無効電力設定値合計							*/
	{           0, 2147483647 },		/* 02 : ハートビートカウント									*/
	{           0, 2147483647 },		/* 03 : 蓄電所全体の総消費エネルギー(充電)						*/
	{           0, 2147483647 },		/* 04 : 蓄電所全体の総供給エネルギー(放電)						*/
	{ -2147483647, 2147483647 },		/* 05 : バッテリー有効電力										*/
	{ -2147483647, 2147483647 },		/* 06 : バッテリー無効電力										*/
	{           0, 2147483647 },		/* 07 : バッテリーインバーターの合計接続公称有効電力			*/
	{           0, 2147483647 },		/* 08 : バッテリーシステムの総放電エネルギー					*/
	{           0, 2147483647 },		/* 09 : バッテリーシステムの総利用可能充電エネルギー			*/
	{ -2147483647, 2147483647 },		/* 10 : バッテリーインバーター最大放電電力						*/
	{ -2147483647, 2147483647 },		/* 11 : バッテリーインバーター最大充電電力						*/
	{ -2147483647, 2147483647 },		/* 12 : POIにおける有効電力測定値								*/
	{           0, 2147483647 },		/* 13 : バッテリーインバータの平均SoC							*/
	{ -2147483647, 2147483647 },		/* 14 : 発電所全体の有効電力制限の上限							*/
	{ -2147483647, 2147483647 },		/* 15 : 発電所全体の有効電力制限の下限							*/
	{ -2147483647, 2147483647 },		/* 16 : バッテリーシステムの有効電力設定値のリードバック		*/
	{ -2147483647, 2147483647 },		/* 17 : 発電所全体の無効電力設定値のリードバック				*/
	{      -32768,      32767 },		/* 18 : 有効電力(充電)の力率設定値のリードバック				*/
	{      -32768,      32767 },		/* 19 : 有効電力(放電)の力率設定値のリードバック				*/
	{           0, 2147483647 },		/* 20 : 発電所全体の最大有効電力率のリードバック 変化率(毎分)	*/
	{           0, 2147483647 },		/* 21 : 発電所全体の最大無効電力率のリードバック 変化率(毎分)	*/
	{ -2147483647, 2147483647 },		/* 22 : 発電所全体の有効電力上限のリードバック					*/
	{ -2147483647, 2147483647 },		/* 23 : 発電所全体の有効電力下限のリードバック					*/
	{ -2147483647, 2147483647 },		/* 24 : 系統の有効電力											*/
	{ -2147483647, 2147483647 },		/* 25 : 系統の無効電力											*/
	{ -2147483647, 2147483647 },		/* 26 : バッテリーインバータの平均DC電圧						*/
	{ -2147483647, 2147483647 },		/* 27 : バッテリーインバータの合計DC電流						*/
	{           0, 2147483647 },		/* 28 : POIにおける周波数測定値									*/
	{           0, 2147483647 },		/* 29 : SoC														*/
	{           0, 2147483647 },		/* 30 : イベント/失敗コード										*//* [20260123] Add */
};

/* 要求送信設定 */
struct st_bttPpmsmaSdInf		s_type1SdInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +-------------------------------------------------- 要求時のファンクションコード              */
	/*   |                  +------------------------------- 開始アドレス（上位バイト）                */
	/*   |                  |     +------------------------- 開始アドレス（下位バイト）                */
	/*   |                  |     |     +------------------- 要求データ数（上位バイト）                */
	/*   |                  |     |     |     +------------- 要求データ数（下位バイト）                */
	/*   |                  |     |     |     |     +------- 送信データ長                              */
	/*   |                  |     |     |     |     |     +- 要求種別                                  */
	/*   |                  |     |     |     |     |     |                     +- レジスタ情報        */
	/*   |                  |     |     |     |     |     |                     |                      */
	{ PV_PPMSMA01_FUNC_03, 0x9C, 0x4B, 0x00, 0x69, 0x0C, PV_PPMSMA01_REQ_AW, CMD_GET_BATTERY_TM				 },	/*  1回目：計測値(有効電力など)の取得   */
	{ PV_PPMSMA01_FUNC_03, 0x9C, 0xC1, 0x00, 0x3B, 0x0C, PV_PPMSMA01_REQ_AW, CMD_GET_BATT_INFO				 },	/*  2回目：計測値(リードバックなど)の取得   */
/* [20260610] Chg St */
	{ PV_PPMSMA01_FUNC_03, 0x9E, 0x4C, 0x00, 0x02, 0x0C, PV_PPMSMA01_REQ_AW, CMD_GET_SOC 					 },	/*  3回目：SoCの取得   */
	{ PV_PPMSMA01_FUNC_10, 0xA0, 0x38, 0x00, 0x02, 0x11, PV_PPMSMA01_REQ_CB, CMD_SET_POWER_FACT				 },	/*  4回目：力率の設定   */
	{ PV_PPMSMA01_FUNC_10, 0xA0, 0x4E, 0x00, 0x05, 0x17, PV_PPMSMA01_REQ_CB, CMD_SET_REACTIVE_POWER			 },	/*  5回目：無効電力設定   */
	{ PV_PPMSMA01_FUNC_10, 0xA0, 0x30, 0x00, 0x02, 0x11, PV_PPMSMA01_REQ_AW, CMD_SET_POWER					 },	/*  6回目：有効電力の設定   */
	{ PV_PPMSMA01_FUNC_10, 0xA0, 0xA7, 0x00, 0x02, 0x11, PV_PPMSMA01_REQ_AW, CMD_SET_POI_PWR_AT_CTRL_MODE	 },	/*  7回目：POI有効電力制御デバイスの設定   *//* [20260123] Add */
	{ PV_PPMSMA01_FUNC_10, 0xA0, 0x2D, 0x00, 0x01, 0x0F, PV_PPMSMA01_REQ_AW, CMD_SET_HEART_BEAT				 },	/*  8回目：ハートビートの設定   *//* [20260123] Chg */
/* [20260610] Chg Ed */
	{ 0xFF,                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,               0xFF							 }	/* STOPPER */
};


/* 応答情報取得（計測情報） */
struct st_bttPpmsmaTmInf	s_type1TmInf[] =		/* 処理タイプ１の計測値取得情報 */
{
	/* +---------------------------------------------- 要求ｎ回目                                       */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置（上位バイト） */
	/* |    |   +------------------------------------- 計測値のデータバイト数                           */
	/* |    |   |   +--------------------------------- バッテリー計測番号                               */
	/* |    |   |   |   +----------------------------- データ型(0:unsigned, 1:int, 2:float)             */
	{  1,  17,  4,  1,  1 },							/* 001 : バッテリー無効電力設定値合計					*/
	{  1,  25,  4,  2,  0 },							/* 002 : ハートビートカウント							*/
	{  1,  33,  4,  3,  0 },							/* 003 : 蓄電所全体の総消費エネルギー(充電)				*/
	{  1,  37,  4,  4,  0 },							/* 004 : 蓄電所全体の総供給エネルギー(放電)				*/
	{  1,  41,  4,  5,  1 },							/* 005 : バッテリー有効電力								*/
	{  1,  45,  4,  6,  1 },							/* 006 : バッテリー無効電力								*/
	{  1,  75,  4,  7,  0 },							/* 007 : バッテリーインバーターの合計接続公称有効電力	*/
	{  1,  79,  4,  8,  0 },							/* 008 : バッテリーシステムの総放電エネルギー			*/
	{  1,  83,  4,  9,  0 },							/* 009 : バッテリーシステムの総利用可能充電エネルギー	*/
	{  1,  87,  4, 10,  1 },							/* 010 : バッテリーインバーター最大放電電力				*/
	{  1,  91,  4, 11,  1 },							/* 011 : バッテリーインバーター最大充電電力				*/
	{  1,  95,  4, 12,  1 },							/* 012 : POIにおける有効電力測定値						*/
	{  2,   9,  4, 13,  0 },							/* 013 : バッテリーインバータの平均SoC					*/
	{  2,  13,  4, 14,  1 },							/* 014 : 発電所全体の有効電力制限の上限					*/
	{  2,  17,  4, 15,  1 },							/* 015 : 発電所全体の有効電力制限の下限					*/
	{  2,  35,  4, 16,  1 },							/* 016 : バッテリーシステムの有効電力設定値の読込		*/
	{  2,  39,  4, 17,  1 },							/* 017 : 発電所全体の無効電力設定値の読込				*/
	{  2,  59,  2, 18,  1 },							/* 018 : 有効電力(充電)の力率設定値の読込				*/
	{  2,  61,  2, 19,  1 },							/* 019 : 有効電力(放電)の力率設定値の読込				*/
	{  2,  63,  4, 20,  0 },							/* 020 : 発電所全体の最大有効電力率の読込 変化率(毎分)	*/
	{  2,  67,  4, 21,  0 },							/* 021 : 発電所全体の最大無効電力率の読込 変化率(毎分)	*/
	{  2,  71,  4, 22,  1 },							/* 022 : 発電所全体の有効電力上限の読込					*/
	{  2,  75,  4, 23,  1 },							/* 023 : 発電所全体の有効電力下限の読込					*/
	{  2, 111,  4, 24,  1 },							/* 024 : 系統の有効電力									*/
	{  2, 115,  4, 25,  1 },							/* 025 : 系統の無効電力									*/
	{  2, 119,  4, 26,  1 },							/* 026 : バッテリーインバータの平均DC電圧				*/
	{  2, 123,  4, 27,  1 },							/* 027 : バッテリーインバータの合計DC電流				*/
/* [20260610] Chg St */
	{  1, 179,  4, 28,  0 },							/* 028 : POIにおける周波数測定値						*/
	{  3,   9,  4, 29,  0 },							/* 029 : SoC											*/
/* [20260610] Chg Ed */
	{  1,  13,  4, 30,  0 },							/* 030 : イベント/失敗コード							*//* [20260123] Add */
	{ -1,  -1, -1, -1, -1 }                         /* STOPPER */
};


/* 応答情報取得（状態情報） */
struct st_bttPpmsmaSvInf	s_type1SvInf[] =		/* 処理タイプ１の状態取得情報               */
{
	/* +---------------------------------------------- 要求ｎ回目                               */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置       */
	/* |    |   +------------------------------------- 処理対象データサイズ                     */
	/* |    |   |        +---------------------------- 処理タイプ（1:ビットタイプ/2:数値タイプ）*/
	/* |    |   |        |         +------------------ 状態情報格納開始位置                     */
	/* |    |   |        |         |   +-------------- 状態初期化サイズ（数値タイプの場合）     */
	/* |    |   |        |         |   |     +-------- テーブルポインタ                         */
	/* |    |   |        |         |   |     |                                                  */
	{  1,   9,  4, PV_SVTYPE_NUM,  0,  1, (void *)s_stsCodeTbl_Type1_operationStatusHealth					  },    /* 操作ステータスの健全性 */
//	{  1,  13,  4, PV_SVTYPE_NUM,  1,  1, (void *)s_stsCodeTbl_Type1_eventOrFailureCode						  },    /* イベント/失敗コード *//* [20260123] Del */
	{  1,  21,  4, PV_SVTYPE_NUM,  2,  2, (void *)s_stsCodeTbl_Type1_operationState							  },    /* 操作状態 */
	{  1,  29,  4, PV_SVTYPE_NUM,  4,  1, (void *)s_stsCodeTbl_Type1_gridConnection							  },    /* 系統接続 */
	{  1,  57,  2, PV_SVTYPE_NUM,  5,  2, (void *)s_stsCodeTbl_Type1_currentReactivePowerControlMode		  },    /* 無効電力制御 */
	{  1,  59,  2, PV_SVTYPE_NUM,  7,  1, (void *)s_stsCodeTbl_Type1_powerFrequencyResponseFrqDroopActive	  },    /* 電力周波数応答機能 */
	{  1,  61,  2, PV_SVTYPE_NUM,  8,  1, (void *)s_stsCodeTbl_Type1_faultRideThroughActive					  },    /* 故障時運転継続機能 */
	{  2,  21,  4, PV_SVTYPE_NUM,  9,  1, (void *)s_stsCodeTbl_Type1_hybridSystemState						  },    /* HYSの状態 */
	{  2,  31,  4, PV_SVTYPE_NUM, 10,  2, (void *)s_stsCodeTbl_Type1_readingBackOfControlModeCommand		  },    /* 制御モードコマンドの読込 */
	{ -1,  -1, -1,            -1, -1, -1, NULL   }		/* STOPPER */
};
