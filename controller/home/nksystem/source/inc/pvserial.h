/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * シリアル通信関連定義
 *
 */

/*
 * シリアルアクセス関連定義
 */
#define	PV_STYPE_TTY			1				/* デバイスタイプ（４８５回線ＴＴＹアクセス） */
#define	PV_STYPE_TCP			2				/* デバイスタイプ（４８５回線ＴＣＰアクセス） */
#define	PV_STYPE_DIRTCP			3				/* デバイスタイプ（直接ＴＣＰ／ＩＰアクセス） */
#define PV_STYPE_UDPDIR			4				/* デバイスタイプ（ＵＤＰアクセス送信元のポートへ返送するタイプ） */
#define PV_STYPE_UDPOTH			5				/* デバイスタイプ（ＵＤＰアクセス送信元とは別のポートへ返送するタイプ） */

#define	PV_STAT_COM_CLOSE		0				/* シリアルポート未オープン */
#define	PV_STAT_COM_OPEN		1				/* シリアルポートオープン済 */
#define	PV_STAT_COM_CONNECT		2				/* シリアルポートアクセス可能 */

#define	PV_SERIAL_ERRNOTREADY	-1				/* シリアル未設定 */
#define	PV_SERIAL_ERRTIMEOUT	-2				/* シリアルタイムアウト */
#define	PV_SERIAL_ERRACCESS		-3				/* シリアルアクセス異常 */
#define PV_SERIAL_ERRBCCCRC		-4				/* シリアルＢＣＣ・ＣＲＣ異常 */
#define	PV_SERIAL_ERRINTVL		-5				/* シリアル間隔異常（子側検出） */
#define	PV_SERIAL_ERROTHER		-99				/* シリアルその他エラー（プロトコル関連） */

#define	PV_SERIAL_LOGSIZ		512				/* シリアルログ一時保持領域サイズ */
#define	PV_SERIAL_LOGLEN		1024			/* シリアルログデータ長 */
#define	PV_SERIAL_LOG_SEND		0				/* 送信 */
#define	PV_SERIAL_LOG_READ		1				/* 受信 */
#define	PV_SERIAL_LOG_SET		2				/* 設定 */
#define	PV_SERIAL_LOG_ERR		3				/* エラー */
#define	PV_SERIAL_LOG_TOUT		4				/* タイムアウト */
#define	PV_SERIAL_LOG_BFOV		5				/* ログバッファオーバー */

#define	PV_SERIAL_LOG_AUTO		0				/* ログ出力自動判定 */
#define	PV_SERIAL_LOG_ON		1				/* ログ強制出力 */

#define	PV_SERIAL_WRITE_OFF		0				/* 書き込みしない */
#define	PV_SERIAL_WRITE_ON		1				/* 書き込みする */

#define	PV_SERIAL_REQUEST_MAX	3				/* シリアル通信最大要求回数 */


/*
 * シリアルポートアクセス用共通構造体
 */
struct st_serialinf {							/* シリアル回線情報 */
	int				status;						/* ステータス */
	int				requestCount;				/* 要求実施回数 */
	int				fd_sockfd;					/* ファイル(sock)ディスクリプタ */
	int				ttyUSBFlag;					/* USB-485変換器フラグ */
	int				lineNo;						/* 通信回線番号     */
	int				deviceType;					/* シリアルのタイプ                */
												/*   1:Serial                      */
												/*   2:TCP (LAN-485変換器タイプ) */
												/*   3:TCP (直接TCP/IP接続タイプ) */
												/*   4:UDP (送信元のポート番号に返送するタイプ) */
												/*   5:UDP (送信元とは別のポート番号に返送するタイプ) */
	char			targetName[1024];			/* デバイス名 or IPアドレス */
	char			log_open;					/* ログ出力抑止（オープン時） */
	char			yobi[15];					/* （予備） */
	unsigned int	locIPAddr;					/* 収集装置側IPアドレス（ネットワークバイトオーダー） */
	unsigned int	dstIPAddr;					/* 相手先装置IPアドレス（ネットワークバイトオーダー） */
	int				portNumb;					/* 送信先ポート番号(TCPorUDP) */
	int				portNumbLoc;				/* 送信元ポート番号(TCPorUDP) */
	int				fd_sockfdUdp;				/* シリアルタイプが5の場合のUDP受信ファイル(sock)ディスクリプタ */
	int				baudRate;					/* ボーレート（デバイスタイプが5の場合はUDP受信ポート番号として使用[20140910]） */
	int				dataSize;					/* データサイズ(7 or 8) */
	int				parity;						/* パリティ(0:NONE, 1:ODD, 2:EVEN) */
	int				stopBit;					/* ストップビット(1 or 2) */
	int				rcvTime;					/* 応答待ちタイマ(msec) */
	int				sndTime;					/* 送信待ちタイマ(msec) */
	int				cycleTime;					/* 収集周期タイマ(sec) */
};

/*
 * シリアル通信ログ用構造体
 */
struct st_serialLog {
	int				dataCnt;					/* ログデータ数 */
	int				yobi[3];					/* 未使用 */
	struct st_logData {
		time_t			sec;					/* ログ発生時間（積算秒） */
		suseconds_t		uSec;					/* ログ発生時間（マイクロ秒） */
		int				kind;					/* 送受信種別（0:送信/1:受信） */
		int				dataLen;				/* データ長 */
		unsigned int	locIPAddr;				/* ローカルIPアドレス（ネットワークバイトオーダー） */
		unsigned int	dstIPAddr;				/* 通信先IPアドレス（ネットワークバイトオーダー） */
		unsigned short	locPort;				/* ローカルポート番号（ホストバイトオーダー） */
		unsigned short	dstPort;				/* 通信先ポート番号（ホストバイトオーダー） */
		unsigned short	yobi[2];				/* 未使用 */
		char			data[PV_SERIAL_LOGLEN];	/* 送受信データ */
	} s_logData[PV_SERIAL_LOGSIZ];
};


/* 関数定義(comm_serial.c) */
int	commfunc_serial_open( struct st_serialinf * );
int	commfunc_serial_send( struct st_serialinf *, char *, int, struct st_serialLog * );
int	commfunc_serial_sendto( struct st_serialinf *, char *, int, unsigned int, struct st_serialLog * );	/* */
int	commfunc_serial_recv( struct st_serialinf *, char *, struct timeval *, struct st_serialLog * );
int	commfunc_serial_recvfrom( struct st_serialinf *, char *, unsigned int *, struct timeval *, struct st_serialLog * );
int	commfunc_serial_close( struct st_serialinf * );
int	commfunc_serial_infoget( char, struct st_serialinf *, struct st_COMLIINF *, char * );
int	commfunc_serial_cycletmchk( struct st_serialinf *, int *, struct st_COMLIINF * );
int	commfunc_serial_crc16itut( char *, int, int * );
int	commfunc_serial_crc16modbus( unsigned char *, int, unsigned char *, unsigned char * );
int	commfunc_serial_logout( int, struct st_serialinf *, struct st_serialLog * );
int commfunc_snmp_logout( int logMode, struct st_serialinf *ps_serialinf, struct st_serialLog *ps_serialLog );

