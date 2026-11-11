/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260423] 2026.04.23
 * ・バッテリメータの計測情報を別スレッドで取得するように修正（一次調整力対応）
 * [20231023] 2023.10.23
 * ・出力制御代表機試験中の変更(ランプレート設定のレジスタ誤り)
 * [20230814] 2023.08.14
 * ・出力制御対応に伴いレジスタ情報を追加
 * [20230721] 2023.07.21
 * ・無効電力モードの力率設定対応
 * [20230312] 2023.03.12
 * ・バッテリメータとの通信が切れた場合、メータの全ての計測値が0となる為、
 *   積算値となる計測値を欠測にする
 * [20200824] 2020.08.24
 * ・有効電力,無効電力から算出した力率(平均)を最大充電電力,最大放電電力に適用する
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
#define	PV_BTTTESLA1_TYPE2		2					/* 通信タイプ２（＋SEL） */
#define	PV_BTTTESLA1_TYPE3		3					/* 通信タイプ３（未使用） */

/* 要求タイプ */
#define	PV_BTTTESLA1_FUNC_01 0x01					/* ファンクション１（コイルの読出し） */
#define	PV_BTTTESLA1_FUNC_02 0x02					/* ファンクション２（入力ステータスの読出し） */
#define	PV_BTTTESLA1_FUNC_03 0x03					/* ファンクション３（保持レジスタの読出し） */
#define	PV_BTTTESLA1_FUNC_04 0x04					/* ファンクション４（入力レジスタの読出し） */
#define	PV_BTTTESLA1_FUNC_06 0x06					/* ファンクション６（単レジスタの書込み） */
#define	PV_BTTTESLA1_FUNC_10 0x10					/* ファンクション１０（複数レジスタの書込み） */

#define	PV_BTTTESLA1_REQ_CB			0x01			/* 通信復帰時 */
#define	PV_BTTTESLA1_REQ_NOR		0x02			/* 通常時 */
#define	PV_BTTTESLA1_REQ_AW			(PV_BTTTESLA1_REQ_CB|PV_BTTTESLA1_REQ_NOR)	/* 通常時＆通信復帰時 */

#define	CMD_GET_ISLAND					1
#define	CMD_GET_HEART_BEAT				2
#define	CMD_GET_SYSTEM_METER			4
#define	CMD_GET_BATTERY_METER			5
#define	CMD_GET_ALARM					6
#define	CMD_GET_REAL_MODE				7
#define	CMD_SET_POWER					101
#define	CMD_SET_POWER_FACT				102
#define	CMD_SET_RAMP_RATE				103
#define	CMD_SET_REACTIVE_MODE			104
#define	CMD_SET_REAL_MODE				105
#define	CMD_SET_ISLAND_MODE				106

#define PV_BTTTESLA1_HEARTBEAT_TO	60				/* ハートビートタイムアウト(秒) */

/* 最大処理計測番号 */
#define	PV_BTTTESLA1_MAXTM		25					/* 計測情報数 *//* [20200824] Chg */

#define PV_SVTYPE_BIT			1					/* 状態取得タイプ（8ビットタイプ） */
#define PV_SVTYPE_NUM			2					/* 状態取得タイプ（数値タイプ） */
#define PV_SVTYPE_MSK			3					/* 状態取得タイプ（マスクタイプ） */

#define PV_SET_POWER_EXP		0					/* 充放電電力設定値乗率 */

#define PV_BTTTESLA1_SVNUMB_REALPOWER_STOP	121			/* 出力停止 */
#define PV_BTTTESLA1_SVNUMB_REALPOWER_DIR	122			/* 手動制御 */

#define PV_BTTTESLA1_SV_AUTO_ACTIVE         113         /* Auto Active        */
#define PV_BTTTESLA1_SV_INTENTIONAL_READY	114			/* Intentional Ready  */
#define PV_BTTTESLA1_SV_INTENTIONAL_ISLAND	115			/* Intentional Island */
#define PV_BTTTESLA1_SV_PQ_ISLAND			116			/* PQ Island          */
#define PV_BTTTESLA1_SV_ISLANDOFF			117			/* Island Off         */

/* [20230721] Add St */
#define PV_BTTTESLA1_PARAM1					"PowerFactorChargeSetpoint"
#define PV_BTTTESLA1_PARAM2					"PowerFactorChargeLead"
#define PV_BTTTESLA1_PARAM3					"PowerFactorDischargeSetpoint"
#define PV_BTTTESLA1_PARAM4					"PowerFactorDischargeLead"
/* [20230721] Add Ed */

#define PV_BTTTESLA1_NORMAL_THREAD			0			/* 通常スレッド       *//* [20260423] Add */
#define PV_BTTTESLA1_METER_THREAD			1			/* メータ取得スレッド *//* [20260423] Add */

/* [20200824] Add St */
struct st_bttTeslaFactInf {
	int					powerNumb;					/* 有効電力 */
	int					varNumb;					/* 無効電力 */
	int					factNumb;					/* 力率 */
	int					avaChgNumb;					/* 充電可能電力 */
	int					avaDisNumb;					/* 放電可能電力 */
	int					avaChgFactNumb;				/* 充電可能電力(力率補正) */
	int					avaDisFactNumb;				/* 放電可能電力(力率補正) */
};

struct st_bttFactTm {
	int					index;						/* 周期Index */
	int					minDataNum;					/* 1分間のデータ数 */
	struct st_tmData {								/* 取得周期の計測情報 */
		int				tmFlg;						/* 計測フラグ */
		int				tmVal;						/* 計測値 */
	} s_tmData[600];
};
/* [20200824] Add Ed */

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
	unsigned char		threadNo;					/* スレッド番号 *//* [20260423] Add */
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

/* [20230312] Add St */
struct st_stsMeterTmTbl {							/* メータ計測情報 */
	int					tmNumb;						/* 計測番号 */
	int					tntegratedFlg;				/* 積算値フラグ */
};
/* [20230312] Add Ed */

struct st_threadArg {
	int					indexNumb;					/* 処理中インデックス番号 */
	int					stFlg;						/* スレッド開始フラグ */
	int					edFlg;						/* スレッド終了フラグ */
	int					threadNumb;					/* スレッド番号 *//* [20260423] Add */
};

/* [20230721] Add St */
struct st_paraReactivePowerFact {
	int					statFlag;					/* 実装有無 */
	int					chargeSetpoint;				/* 充電力率設定値 */
	int					chargeLead;					/* 充電力率進み/遅れ(0:遅れ、1:進み) */
	int					dischargeSetpoint;			/* 放電力率設定値 */
	int					dischargeLead;              /* 放電力率進み/遅れ(0:遅れ、1:進み) */
};
/* [20230721] Add Ed */

/* 対応機種情報 */
/* 対応機種情報：型式名称 */
struct st_devTypeName	s_bttTypeName[] =
{
	/*	      +----------------------------------- 型式名称     */
	/*	      |                   +--------------- 通信処理方式 */
	/*	      |                   |           +--- 基本要求回数 */
	/*-1234567890123456           |           |    （型式名称は16文字以内で定義） */
	{ "TESLA-POWERPK20",         PV_BTTTESLA1_TYPE1,  3 },
	{ "TESLA-POWERPK20+SEL",     PV_BTTTESLA1_TYPE2,  3 },	/* [20230529] Add St */
	{ "-STOPPER-",                       -1, -1 }
};

/* SoC情報 */
struct st_bttTeslaCalcInf	s_typeCalcInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +-------------------------------------------  フル充電エネルギー */
	/*   |    +--------------------------------------  残電力量 */
	/*   |    |    +---------------------------------  SoC */
	/*   |    |    |    +----------------------------  有効電力 */
	{    1,   2,   8,   3 },								/* Type1：算出計測情報 */
	{    1,   2,   8,   3 },								/* Type2：算出計測情報 */
	{   -1,  -1,  -1,  -1 },
};

/* [20200824] Add St */
/* 力率情報 */
struct st_bttTeslaFactInf	s_typeFactInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +-------------------------------------------  有効電力 */
	/*   |    +--------------------------------------  無効電力 */
	/*   |    |    +---------------------------------  力率 */
	/*   |    |    |    +----------------------------  充電可能電力 */
	/*   |    |    |    |    +-----------------------  放電可能電力 */
	/*   |    |    |    |    |    +------------------  充電可能電力(力率補正) */
	/*   |    |    |    |    |    |    +-------------  放電可能電力(力率補正) */
	{    3,   9,   13,  5,   6,  14,  15},				/* Type1：力率算出計測情報 */
	{    3,   9,   13,  5,   6,  14,  15},				/* Type2：力率算出計測情報 */
};
/* [20200824] Add Ed */

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

/* [20230312] Add St */
/* バッテリメータ計測情報 */
struct st_stsMeterTmTbl	s_stsMeterTbl_Type1_Battery[] =
{
    /* +-------------------------- 計測番号                             */
    /* |   +---------------------- 積算値フラグ                         */
    {  3,  0 },                   /* 03 : 有効電力 (w)					*/
    {  7,  0 },                   /* 07 : ＡＣ周波数 (Hz)				*/
    {  9,  0 },                   /* 09 : 無効電力 (w)					*/
    { 10,  0 },                   /* 10 : 皮相電力 (w)					*/
    { 11,  0 },                   /* 11 : 平均交流電圧 (V)				*/
    { 12,  0 },                   /* 12 : 交流電流合計 (A)				*/
    { 16,  1 },                   /* 16 : 放電電力量 (Wh)				*/
    { 17,  1 },                   /* 17 : 充電電力量 (Wh)				*/
	{ -1, -1 }					  /* STOPPER */
};

/* サイトメータ計測情報 */
struct st_stsMeterTmTbl	s_stsMeterTbl_Type1_Site[] =
{
    /* +-------------------------- 計測番号                             */
    /* |   +---------------------- 積算値フラグ                         */
    { 18,  0 },                   /* 18 : 受電点：有効電力 (W)			*/
    { 19,  0 },                   /* 19 : 受電点：無効電力 (VAR)		*/
    { 20,  0 },                   /* 20 : 受電点：皮相電力 (VA)			*/
    { 21,  0 },                   /* 21 : 受電点：電圧 (V)				*/
    { 22,  0 },                   /* 22 : 受電点：電流 (A)				*/
    { 23,  0 },                   /* 23 : 受電点：周波数 (Hz)			*/
    { 24,  1 },                   /* 24 : 受電点：放電電力量 (Wh)		*/
    { 25,  1 },                   /* 25 : 受電点：充電電力量 (Wh)		*/
	{ -1, -1 }					  /* STOPPER */
};
/* [20230312] Add Ed */

/* 計測値レンジ１（PV_BTTTESLA1_TYPE1、PV_BTTTESLA1_TYPE2 用）*/
struct st_devTmRange	s_type1TmRange[] =
{
	/*      +------------------------------------------- 最小計測値 */
	/*      |           +------------------------------- 最大計測値 */
	{ -2147483647, 2147483647 },						/* 01 : フル充電エネルギー（ｗｈ）	*/
	{ -2147483647, 2147483647 },						/* 02 : 残電力量（ｗｈ）			*/
	{ -2147483647, 2147483647 },						/* 03 : 有効電力 (w)						*/
	{ -2147483647, 2147483647 },						/* 04 : ＳＭＣからバッテリへの電力値 (w)	*/
	{ -2147483647, 2147483647 },						/* 05 : 最大充電電力 (w)					*/
	{ -2147483647, 2147483647 },						/* 06 : 最大放電電力 (w)					*/
	{           0,      65535 },						/* 07 : ＡＣ周波数 (Hz)						*/
	{         100,          0 },						/* 08 : ＳＯＣ (%) */
/* [20200824] Add St */
	{ -2147483647, 2147483647 },						/* 09 : 無効電力 (w)						*/
	{ -2147483647, 2147483647 },						/* 10 : 皮相電力 (w)						*/
	{ -2147483647, 2147483647 },						/* 11 : 平均交流電圧 (V)					*/
	{ -2147483647, 2147483647 },						/* 12 : 交流電流合計 (A)					*/
	{          30,        170 },						/* 13 : 力率								*/
	{ -2147483647, 2147483647 },						/* 14 : 最大充電電力(力率補正) (w)			*/
	{ -2147483647, 2147483647 },						/* 15 : 最大放電電力(力率補正) (w)			*/
	{           0, 2147483647 },						/* 16 : 放電電力量 (Wh)						*/
	{           0, 2147483647 },						/* 17 : 充電電力量 (Wh)						*/
/* [20200824] Add Ed */
	{ -2147483647, 2147483647 },						/* 18 : 受電点：有効電力 (W)				*/
	{ -2147483647, 2147483647 },						/* 19 : 受電点：無効電力 (VAR)				*/
	{ -2147483647, 2147483647 },						/* 20 : 受電点：皮相電力 (VA)				*/
	{ -2147483647, 2147483647 },						/* 21 : 受電点：電圧 (V)					*/
	{ -2147483647, 2147483647 },						/* 22 : 受電点：電流 (A)					*/
	{           0,      65535 },						/* 23 : 受電点：周波数 (Hz)					*/
	{           0, 2147483647 },						/* 24 : 受電点：放電電力量 (Wh)				*/
	{           0, 2147483647 },						/* 25 : 受電点：充電電力量 (Wh)				*/
};

/* 要求送信設定 */
struct st_bttTeslaSdInf		s_type1SdInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +--------------------------------------------------------------------------------------------------- 要求時のファンクションコード */
	/*   |                  +-------------------------------------------------------------------------------- 開始アドレス（上位バイト）   */
	/*   |                  |     +-------------------------------------------------------------------------- 開始アドレス（下位バイト）   */
	/*   |                  |     |     +-------------------------------------------------------------------- 要求データ数（上位バイト）   */
	/*   |                  |     |     |     +-------------------------------------------------------------- 要求データ数（下位バイト）   */
	/*   |                  |     |     |     |     +-------------------------------------------------------- 送信データ長                 */
	/*   |                  |     |     |     |     |     +-------------------------------------------------- スレッド番号                 */
	/*   |                  |     |     |     |     |     |                           +---------------------- 要求種別                     */
	/*   |                  |     |     |     |     |     |                           |                    +- レジスタ情報                 */
	/*   |                  |     |     |     |     |     |                           |                    |                               */
/* [20260423] Chg St */
	{ PV_BTTTESLA1_FUNC_03, 0x07, 0x08, 0x00, 0x01, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_ISLAND			},			/* 1回目：Island Control Modeの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x03, 0xFE, 0x00, 0x01, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_CB, CMD_GET_HEART_BEAT		},			/* 2回目：ハートビートパターンの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x03, 0xE8, 0x00, 0x03, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_REAL_MODE		},			/* 3回目：Real Power Command Modeの取得 */
	{ PV_BTTTESLA1_FUNC_10, 0x04, 0xCE, 0x00, 0x06, 0x19, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_CB, CMD_SET_POWER_FACT		},			/* 4回目：力率設定 *//* [20230721] Add */
/* [20230721] Chg St */
	{ PV_BTTTESLA1_FUNC_10, 0x04, 0xB0, 0x00, 0x01, 0x0F, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_CB, CMD_SET_REACTIVE_MODE	},			/* 5回目：通信セッション確立直後の設定(Reactive Power Command Mode) */
	{ PV_BTTTESLA1_FUNC_10, 0x03, 0xE8, 0x00, 0x03, 0x13, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_REAL_MODE		},			/* 6回目：通信セッション確立直後の設定(Real Power Command Mode, Always Active , PeakPower Mode) */
	{ PV_BTTTESLA1_FUNC_03, 0x00, 0xC8, 0x00, 0x12, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_SYSTEM_METER		},			/* 7回目：システムステータスの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x01, 0x2C, 0x00, 0x77, 0x0C, PV_BTTTESLA1_METER_THREAD,  PV_BTTTESLA1_REQ_AW, CMD_GET_BATTERY_METER 	},			/* 8回目：バッテリーメータ、サイトメータデータの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x09, 0xC4, 0x00, 0x06, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_ALARM		 	},			/* 9回目：状態情報の取得 */
	{ PV_BTTTESLA1_FUNC_10, 0x04, 0x00, 0x00, 0x04, 0x15, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_RAMP_RATE	 	},			/*10回目：ランプレートの設定 *//* [20231023] Chg */
	{ PV_BTTTESLA1_FUNC_10, 0x03, 0xFC, 0x00, 0x04, 0x15, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_POWER		 	},			/*11回目：充放電の設定 */
	{ PV_BTTTESLA1_FUNC_10, 0x07, 0x08, 0x00, 0x01, 0x0F, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_ISLAND_MODE		},			/*12回目：Island Control Modeの設定 */
/* [20230721] Chg Ed */
	{ 0xFF,                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                       0xFF,                0xFF }	/* STOPPER */
/* [20260423] Chg Ed */
};

/* 要求送信設定 */
struct st_bttTeslaSdInf		s_type2SdInf[] =		/* 処理タイプ１の要求情報 */
{
	/*   +--------------------------------------------------------------------------------------------------- 要求時のファンクションコード */
	/*   |                  +-------------------------------------------------------------------------------- 開始アドレス（上位バイト）   */
	/*   |                  |     +-------------------------------------------------------------------------- 開始アドレス（下位バイト）   */
	/*   |                  |     |     +-------------------------------------------------------------------- 要求データ数（上位バイト）   */
	/*   |                  |     |     |     +-------------------------------------------------------------- 要求データ数（下位バイト）   */
	/*   |                  |     |     |     |     +-------------------------------------------------------- 送信データ長                 */
	/*   |                  |     |     |     |     |     +-------------------------------------------------- スレッド番号                 */
	/*   |                  |     |     |     |     |     |                           +---------------------- 要求種別                     */
	/*   |                  |     |     |     |     |     |                           |                    +- レジスタ情報                 */
	/*   |                  |     |     |     |     |     |                           |                    |                               */
/* [20260423] Chg St */
	{ PV_BTTTESLA1_FUNC_03, 0x07, 0x08, 0x00, 0x01, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_ISLAND			},			/* 1回目：Island Control Modeの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x03, 0xFE, 0x00, 0x01, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_CB, CMD_GET_HEART_BEAT		},			/* 2回目：ハートビートパターンの取得 */
	{ PV_BTTTESLA1_FUNC_10, 0x03, 0xFC, 0x00, 0x04, 0x15, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_POWER			},			/* 3回目：充放電の設定 */
	{ PV_BTTTESLA1_FUNC_10, 0x04, 0xB0, 0x00, 0x01, 0x0F, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_CB, CMD_SET_REACTIVE_MODE	},			/* 4回目：通信セッション確立直後の設定(Reactive Power Command Mode) */
	{ PV_BTTTESLA1_FUNC_10, 0x03, 0xE8, 0x00, 0x03, 0x13, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_REAL_MODE	 	},			/* 5回目：通信セッション確立直後の設定(Real Power Command Mode, Always Active , PeakPower Mode) */
	{ PV_BTTTESLA1_FUNC_03, 0x00, 0xC8, 0x00, 0x12, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_SYSTEM_METER		},			/* 6回目：システムステータスの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x01, 0x2C, 0x00, 0x77, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_BATTERY_METER	},			/* 7回目：バッテリーメータ、サイトメータデータの取得 */
	{ PV_BTTTESLA1_FUNC_03, 0x09, 0xC4, 0x00, 0x06, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_ALARM		 	},			/* 8回目：状態情報の取得 */
	{ PV_BTTTESLA1_FUNC_10, 0x07, 0x08, 0x00, 0x01, 0x0F, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_SET_ISLAND_MODE		},			/* 9回目：Island Control Modeの設定 */
	{ PV_BTTTESLA1_FUNC_03, 0x03, 0xE8, 0x00, 0x03, 0x0C, PV_BTTTESLA1_NORMAL_THREAD, PV_BTTTESLA1_REQ_AW, CMD_GET_REAL_MODE	 	},			/*10回目：Real Power Command Modeの取得 */
	{ 0xFF,                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                       0xFF,                0xFF }	/* STOPPER */
/* [20260423] Chg Ed */
};

/* 応答情報取得（計測情報） */
struct st_bttTeslaTmInf		s_type1TmInf[] =		/* 処理タイプ１の計測値取得情報 */
{
	/* +---------------------------------------------- 要求ｎ回目                                       */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置（上位バイト） */
	/* |    |   +------------------------------------- 計測値のデータバイト数                           */
	/* |    |   |   +--------------------------------- バッテリー計測番号                     */
	/* |    |   |   |   +----------------------------- データ型(0:unsigned, 1:int, 2:float)             */
/* [20230721] Chg St */
    {  7,  19,  4,  1,  1 },                        /* 01 :  フル充電エネルギー (Wh)                    */
    {  7,  23,  4,  2,  1 },                        /* 02 :  残電力量 (Wh)                              */
    {  8,   9,  4,  3,  1 },                        /* 03 :  バッテリーメータ：有効電力 (W)             */
    {  7,  11,  4,  4,  1 },                        /* 04 :  TSCからバッテリーへの有効電力指示値 (W)    */
    {  7,  31,  4,  5,  1 },                        /* 05 :  最大充電電力 (W)                           */
    {  7,  35,  4,  6,  1 },                        /* 06 :  最大放電電力 (W)                           */
    {  8,  29,  2,  7,  0 },                        /* 07 :  バッテリーメータ：周波数 (Hz)              */
/* [20200824] Add St */                                     
    {  8,  13,  4,  9,  1 },                        /* 09 : バッテリーメータ：無効電力 (VAR)            */
    {  8,  17,  4, 10,  1 },                        /* 10 : バッテリーメータ：皮相電力 (VA)             */
    {  8,  21,  4, 11,  1 },                        /* 11 : バッテリーメータ：電圧 (V)                  */
    {  8,  25,  4, 12,  1 },                        /* 12 : バッテリーメータ：電流 (A)                  */
/* [20200824] Add Ed */
    {  8,  31,  8, 16,  1 },                        /* 16 : バッテリーメータ：放電電力量 (Wh)           */
    {  8,  39,  8, 17,  1 },                        /* 17 : バッテリーメータ：充電電力量 (Wh)           */
    {  8, 209,  4, 18,  1 },                        /* 18 : 受電点：有効電力 (W)                        */
    {  8, 213,  4, 19,  1 },                        /* 19 : 受電点：無効電力 (VAR)                      */
    {  8, 217,  4, 20,  1 },                        /* 20 : 受電点：皮相電力 (VA)                       */
    {  8, 221,  4, 21,  1 },                        /* 21 : 受電点：電圧 (V)                            */
    {  8, 225,  4, 22,  1 },                        /* 22 : 受電点：電流 (A)                            */
    {  8, 229,  2, 23,  0 },                        /* 23 : 受電点：周波数 (Hz)                         */
    {  8, 231,  8, 24,  0 },                        /* 24 : 受電点：放電電力量 (Wh)                     */
    {  8, 239,  8, 25,  0 },                        /* 25 : 受電点：充電電力量 (Wh)                     */
/* [20230721] Chg Ed */
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
/* [20230721] Chg St */
	{  9,   9,  2,  PV_SVTYPE_BIT, 0, 12, NULL   },		/* 通信メータ */
	{  9,  11,  2,  PV_SVTYPE_BIT, 2,  0, NULL   },		/* インバータ状態 */
	{  9,  13,  2,  PV_SVTYPE_BIT, 4,  0, NULL   },		/* 制限 */
	{  9,  15,  2,  PV_SVTYPE_BIT, 6,  0, NULL   },		/* 予備 */
	{  9,  17,  2,  PV_SVTYPE_BIT, 8,  0, NULL   },		/* コマンドタイムアウト */
	{  9,  19,  2,  PV_SVTYPE_BIT,10,  0, NULL   },		/* Island Controller Alert */
	{  7,  43,  2,  PV_SVTYPE_NUM,12,  2, (void *)s_stsCodeTbl_Type1_IslandStatus  },	/* Island Status	*/
	{  1,   9,  2,  PV_SVTYPE_NUM,14,  1, (void *)s_stsCodeTbl_Type1_IslandMode    },	/* Island Mode 		*/
	{  3,   9,  2,  PV_SVTYPE_NUM,15,  1, (void *)s_stsCodeTbl_Type1_RealPowerCommnad    },	/* Real Power Command Mode 		*/
/* [20230721] Chg Ed */
	{ -1,  -1, -1,            -1, -1, -1, NULL   }		/* STOPPER */
};


/* 応答情報取得（計測情報） */
struct st_bttTeslaTmInf		s_type2TmInf[] =		/* 処理タイプ１の計測値取得情報 */
{
	/* +---------------------------------------------- 要求ｎ回目                                       */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置（上位バイト） */
	/* |    |   +------------------------------------- 計測値のデータバイト数                           */
	/* |    |   |   +--------------------------------- バッテリー計測番号                     */
	/* |    |   |   |   +----------------------------- データ型(0:unsigned, 1:int, 2:float)             */
	{  6,  19,  4,  1,  1 },						/* 01 : フル充電エネルギー（ｗｈ）	*/
	{  6,  23,  4,  2,  1 },						/* 02 : 残電力量（ｗｈ）			*/
	{  7,   9,  4,  3,  1 },						/* 03 : 有効電力 (w)						*/
	{  6,  11,  4,  4,  1 },						/* 04 : ＳＭＣからバッテリへの電力値 (w)	*/
	{  6,  31,  4,  5,  1 },						/* 05 : 最大充電電力 (w)					*/
	{  6,  35,  4,  6,  1 },						/* 06 : 最大放電電力 (w)					*/
	{  7,  29,  2,  7,  0 },						/* 07 : ＡＣ周波数 (Hz)						*/
/* [20200824] Add St */
	{  7,  13,  4,  9,  1 },						/* 09 : 無効電力 (w)						*/
	{  7,  17,  4, 10,  1 },						/* 10 : 皮相電力 (w)						*/
	{  7,  21,  4, 11,  1 },						/* 11 : 平均交流電圧 (V)					*/
	{  7,  25,  4, 12,  1 },						/* 12 : 交流電流合計 (A)					*/
/* [20200824] Add Ed */
	{  7,  31,  8, 16,  1 },						/* 16 : 放電電力量 (Wh)						*/
	{  7,  39,  8, 17,  1 },						/* 17 : 充電電力量 (Wh)						*/
    {  7, 209,  4, 18,  1 },                        /* 18 : 受電点：有効電力 (W)                */
    {  7, 213,  4, 19,  1 },                        /* 19 : 受電点：無効電力 (VAR)              */
    {  7, 217,  4, 20,  1 },                        /* 20 : 受電点：皮相電力 (VA)               */
    {  7, 221,  4, 21,  1 },                        /* 21 : 受電点：電圧 (V)                    */
    {  7, 225,  4, 22,  1 },                        /* 22 : 受電点：電流 (A)                    */
    {  7, 229,  2, 23,  0 },                        /* 23 : 受電点：周波数 (Hz)                 */
    {  7, 231,  8, 24,  0 },                        /* 24 : 受電点：放電電力量 (Wh)             */
    {  7, 239,  8, 25,  0 },                        /* 25 : 受電点：充電電力量 (Wh)             */
	{ -1,  -1, -1, -1, -1 }							/* STOPPER */
};

/* 応答情報取得（状態情報） */
struct st_bttTeslaSvInf		s_type2SvInf[] =		/* 処理タイプ１の状態取得情報               */
{
	/* +---------------------------------------------- 要求ｎ回目                               */
	/* |    +----------------------------------------- 応答データ（受信バッファ）格納位置       */
	/* |    |   +------------------------------------- 処理対象データサイズ                     */
	/* |    |   |        +---------------------------- 処理タイプ（1:ビットタイプ/2:数値タイプ）*/
	/* |    |   |        |         +------------------ 状態情報格納開始位置                     */
	/* |    |   |        |         |   +-------------- 状態初期化サイズ（数値タイプの場合）     */
	/* |    |   |        |         |   |     +-------- テーブルポインタ                         */
	/* |    |   |        |         |   |     |                                                  */
	{  8,   9,  2,  PV_SVTYPE_BIT, 0, 12, NULL   },		/* 通信メータ */
	{  8,  11,  2,  PV_SVTYPE_BIT, 2,  0, NULL   },		/* インバータ状態 */
	{  8,  13,  2,  PV_SVTYPE_BIT, 4,  0, NULL   },		/* 制限 */
	{  8,  15,  2,  PV_SVTYPE_BIT, 6,  0, NULL   },		/* 予備 */
	{  8,  17,  2,  PV_SVTYPE_BIT, 8,  0, NULL   },		/* コマンドタイムアウト */
	{  8,  19,  2,  PV_SVTYPE_BIT,10,  0, NULL   },		/* Island Controller Alert */
	{  6,  43,  2,  PV_SVTYPE_NUM,12,  2, (void *)s_stsCodeTbl_Type1_IslandStatus  },	/* Island Status	*/
	{  1,   9,  2,  PV_SVTYPE_NUM,14,  1, (void *)s_stsCodeTbl_Type1_IslandMode    },	/* Island Mode 		*/
	{ 10,   9,  2,  PV_SVTYPE_NUM,15,  1, (void *)s_stsCodeTbl_Type1_RealPowerCommnad    },	/* Real Power Command Mode 		*/
	{ -1,  -1, -1,            -1, -1, -1, NULL   }		/* STOPPER */
};
