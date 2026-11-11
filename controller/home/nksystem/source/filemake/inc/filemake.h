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
 * オンラインファイル生成処理
 *
 */

#define	PV_FILEMAKE_STOPPER	"-STOPPER-"				/* ストッパー文字列 */

struct st_sepStr {									/* CSVデータ分割用構造体 */
	char				data[1024][128];			/* 1024データ（1データあたり128バイト） */
};

struct st_linkPoint {								/* 開始ポインタ情報 */
	int					statCnt;					/* 実装/未実装情報 */
	int					maxMachine;					/* 装置最大数（諸元値） */
	int					maxTm;						/* 計測情報最大数（諸元値：１装置あたり） */
	int					maxSv;						/* 装置状態情報最大数（諸元値：１装置あたり） */
	int					tmIfLink;					/* 計測情報開始ポインタ */
	int					stStLink;					/* 通信ステータス状態開始ポインタ */
	int					svIfLink;					/* 装置状態情報開始ポインタ */
	int					svStLink;					/* 装置状態開始ポインタ */
	unsigned char		statFlag[32];				/* 実装/未実装 */
};

struct st_machineDef {								/* 装置種別毎の諸元格納用 */
	int					maxMNumb;					/* 装置の最大数 */
	int					maxTmNumb;					/* 装置1台あたりの最大計測情報数 */
	int					maxSvNumb;					/* 装置1台あたりの最大装置状態数 */
	char				fileName[12];				/* ファイル名 */
};

struct st_typeName {								/* 型式名称 */
	char				typeName[64];				/* 名称 */
	int					minID;						/* 最小ID番号（通信プロトコルの場合は利用可能なデバイスタイプの最小値） */
	int					maxID;						/* 最大ID番号（通信プロトコルの場合は利用可能なデバイスタイプの最大値） */
};

/* 関数定義 */
int	func_COMLIINF_Create( int *);
int func_sep_data( char *, struct st_sepStr * );
int	func_BATTEINF_Create( int *, struct st_linkPoint * );
int	func_CEMSPINF_Create( );
int	func_CTLJHFIL_Create( );
int	func_DEMNDFIL_Create( void );
int	func_DEMNDINF_Create( );
int	func_EMCTLFIL_Create( void );
/* [20250807] Add St */
int	func_FCRSTINF_Create( void );
int	func_FCRRPINF_Create( void );
/* [20250807] Add Ed */
int	func_IOTCNINF_Create( );
int	func_JOHENFIL_Create( );
int	func_MDCTLINF_Create( struct st_linkPoint *, struct st_machineDef * );
int	func_MLTMTINF_Create( int *, struct st_linkPoint * );
int	func_OVRCHINF_Create( );
int	func_PGCONINF_Create( );
int	func_PLCFAINF_Create( int *, struct st_linkPoint * );
int	func_POWCNINF_Create( int *, struct st_linkPoint * );
int	func_PVCTLFIL_Create( );
int	func_PVSVRINF_Create( );
int	func_PVSYSINF_Create( struct st_linkPoint * );
int	func_PWPLNFIL_Create( void );
int	func_PWPLNINF_Create( void );
int	func_RCVPWINF_Create( void );
int	func_REMIOINF_Create( int *, struct st_linkPoint * );
int	func_RTCTLINF_Create( void );
int	func_SCHCTLFIL_Create( void );
int	func_STDATINF_Create( );
int	func_STSTSFIL_Create( );
int	func_SVAGGFIL_Create( void );
int	func_SVAGGINF_Create( struct st_linkPoint *, struct st_machineDef * );
int	func_SVDATINF_Create( struct st_linkPoint *, struct st_machineDef * );
int	func_SVRECFIL_Create( );
int	func_SVSTSFIL_Create( );
int	func_TMAGGINF_Create( struct st_linkPoint *, struct st_machineDef * );
int	func_TMDATINF_Create( struct st_linkPoint *, struct st_machineDef * );
int	func_TMMONINF_Create( struct st_linkPoint *, struct st_machineDef * );
int	func_TMRECFIL_Create( );
int	func_RPRECFIL_Create( );	/* [20260515] Add */
int	func_TMSTOFIL_Create( );
int	func_TMSTSFIL_Create( );
