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
 * ＵＤＰ通信関連定義
 *
 */

#define	PV_UDP_SEND_CYCLE			2				/* ＵＤＰ送信周期 */

#define PV_UDP_RCV_TIMEOUT			10				/* ＵＤＰ受信タイムアウト */
#define PV_UDP_RCV_BUFF				4096			/* ＵＤＰ受信バッファサイズ */

#define	PV_STAT_UDP_CLOSE			0				/* ＵＤＰポート未オープン */
#define	PV_STAT_UDP_OPEN			1				/* ＵＤＰポートオープン済 */
#define	PV_STAT_UDP_BIND			2				/* ＵＤＰポートバインド済 */

#define	PV_UDP_ERR_SEQERR			-1				/* ＵＤＰポート操作手順異常 */
#define	PV_UDP_ERR_TIMEOUT			-2				/* ＵＤＰポートタイムアウト */
#define	PV_UDP_ERR_SOCKET			-3				/* ＵＤＰポートソケット異常 */
#define PV_UDP_ERR_IOCTL			-4				/* ＵＤＰポートＩＯＣＴＬ異常 */
#define	PV_UDP_ERR_BIND				-5				/* ＵＤＰポートバインド異常 */
#define	PV_UDP_ERR_SELECT			-6				/* ＵＤＰポートセレクト異常 */
#define PV_UDP_ERR_RECV				-7				/* ＵＤＰポート受信異常 */
#define PV_UDP_ERR_SEND				-8				/* ＵＤＰポート送信異常 */

#define	PV_UDP_LOG_SEND				0				/* 送信 */
#define	PV_UDP_LOG_RECV				1				/* 受信 */

#define	PV_UDP_LOG_OFF				0				/* ログ出力なし */
#define	PV_UDP_LOG_ON				1				/* ログ強制出力 */


/*
 * ＵＤＰポートアクセス用共通構造体
 */
struct st_udpPortInfo {
	int				openStatus;					/* オープンステータス */
	int				sock_fd;					/* ファイル(socket)ディスクリプタ */
	u_int32_t		eth0_IPaddr;				/* eth0 デバイスのIPアドレス */
	u_int32_t		yobi1;						/* （未使用） */
	short			portNumb;					/* ポート番号 */
	short			yobi2[7];					/* （未使用） */
};

struct st_udpRecvInfo {
	u_int32_t		srcIPaddr;					/* 送信元IPアドレス */
	short			srcPortNumb;				/* 送信元ポート番号 */
	short			yobi;						/* （未使用） */
};
