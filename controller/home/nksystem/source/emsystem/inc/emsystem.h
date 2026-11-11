/*
   TAB STOP SIZE = 4
*/
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/

#define EMSYS_SEP_FLG_NORMAL	0x00					/* 通常処理 */
#define EMSYS_SEP_FLG_SPCSKP	0x01					/* 空白、タブ文字はデータとして処理しない */
#define EMSYS_SEP_FLG_CONTIN	0x80					/* 区切り文字が連続した場合はデータカウントしない */

#define EMSYS_UDP_MAX_BUFF		1024					/* ＵＤＰ受信データ最大長 */

#define	EMSYS_UDP_STS_CLOSE		0						/* ＵＤＰポート未オープン */
#define	EMSYS_UDP_STS_OPEN		1						/* ＵＤＰポートオープン済 */
#define	EMSYS_UDP_STS_BIND		2						/* ＵＤＰポートバインド済 */

#define	EMSYS_UDP_ERR_SEQERR	-1						/* ＵＤＰポート操作手順異常 */
#define	EMSYS_UDP_ERR_TIMEOUT	-2						/* ＵＤＰポートタイムアウト */
#define	EMSYS_UDP_ERR_SOCKET	-3						/* ＵＤＰポートソケット異常 */
#define EMSYS_UDP_ERR_IOCTL		-4						/* ＵＤＰポートＩＯＣＴＬ異常 */
#define	EMSYS_UDP_ERR_BIND		-5						/* ＵＤＰポートバインド異常 */
#define	EMSYS_UDP_ERR_SELECT	-6						/* ＵＤＰポートセレクト異常 */
#define EMSYS_UDP_ERR_RECV		-7						/* ＵＤＰポート受信異常 */
#define EMSYS_UDP_ERR_SEND		-8						/* ＵＤＰポート送信異常 */

#define EMSYS_LOG_LV1			1						/* ログレベル１ */
#define EMSYS_LOG_LV2			2						/* ログレベル２ */
#define EMSYS_LOG_LV3			3						/* ログレベル３ */

														/* 運用モード遷移処理シーケンス開始番号 */
#define EMSYS_SEQ_OPERAT		1						/* 待機->運用、準備->運用 開始番号 */
#define	EMSYS_SEQ_WAT2STB		11						/* 準備->待機 開始番号 */
#define	EMSYS_SEQ_OPE2STB		21						/* 運用->待機 開始番号 */
#define	EMSYS_SEQ_STB2WAT		31						/* 待機->準備 開始番号 */
#define	EMSYS_SEQ_STP2WAT		31						/* 停止->準備 開始番号 */
#define EMSYS_SEQ_STB2STP		91						/* 待機->停止 開始番号 */
#define EMSYS_SEQ_WAI2STP		91						/* 準備->停止 開始番号 */

#define EMSYS_DRBD_RECOVERY		5						/* ＤＲＢＤ状態リカバリ間隔 */

struct st_udpPortInfo {
	int				openStatus;							/* オープンステータス */
	int				sock_fd;							/* ファイル(socket)ディスクリプタ */
	u_int32_t		netDev_IPaddr;						/* ネットワークI/FのIPアドレス */
	u_int32_t		yobi1;								/* （未使用） */
	short			portNumb;							/* ポート番号 */
	short			yobi2[7];							/* （未使用） */
};

struct st_udpRecvInfo {
	u_int32_t		srcIPaddr;							/* 送信元IPアドレス */
	short			srcPortNumb;						/* 送信元ポート番号 */
};

struct st_lifChkFrame {									/* 死活監視情報 */
	int				dataKind;							/* 情報種別 */
	time_t			dataTime;							/* 情報時刻（積算秒） */
	time_t			operateLastTime;					/* 運用モード最終稼働時刻（積算秒） */
	time_t			operateTotalTime;					/* 運用モード正常稼働時間（積算秒） */
	int				sysNumber;							/* 送信元装置：装置番号 */
	int				sysMode;							/* 送信元装置：運転モード */
	int				netDevSts[2];						/* 送信元装置：ネットワークI/F状態 */
	int				ctlKind;							/* 制御依頼種別 */
};

struct st_modTrnChkTbl {								/* モード遷移チェックテーブル */
	int				numb;								/* 番号 */
	int				check[10];							/* チェック項目 */
														/* 配列0:自系の現在のモード */
														/* 配列1:他系の現在のモード */
														/* 配列2:自系ネットワークI/Fの状態（サービス用） */
														/* 配列3:自系ネットワークI/Fの状態（系列間用） */
														/* 配列4:死活監視状態（サービス用ネットワーク） */
														/* 配列5:死活監視状態（系列間用ネットワーク） */
														/* 配列6:他系ネットワークI/Fの状態（サービス用） */
														/* 配列7:他系ネットワークI/Fの状態（系列間用） */
														/* 配列8:運用モード最終稼働時刻が新しい */
														/* 配列9:運用モード正常稼働時間が長い */
	int				newMode;							/* 遷移先モード */
};

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
