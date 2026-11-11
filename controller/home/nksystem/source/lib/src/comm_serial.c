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
 * 共通関数：シリアルアクセス関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・シリアルポートのオープン
 * 		・シリアルポートへのデータ出力
 * 		・シリアルポートからのデータ入力
 * 		・シリアルポートのクローズ
 *		・シリアルポートの通信ログ処理
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvserial.h"

#if 0
#define PV_DEBUG
#endif

/* MODBUS CRC16 ハッシュ計算用テーブル（下位バイト用） */
const unsigned char modbus_CRCtable_LO[] =
{
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
};
/* MODBUS CRC16 ハッシュ計算用テーブル（上位バイト用） */
const unsigned char modbus_CRCtable_HI[] =
{
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04,
	0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8,
	0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
	0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10,
	0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
	0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
	0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C,
	0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0,
	0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
	0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
	0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C,
	0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
	0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54,
	0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98,
	0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40,
};


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ポートオープン
 *
 * 機能：
 * 		ps_serialで指定された情報に基づき、ポートをオープンする。
 * 		ps_serialのdeviceTypeにPV_STYPE_TTYが設定されている場合は、ハードウェア
 * 		内のシリアルポート（ＵＳＢの場合もある）をオープンする。
 * 		PV_STYPE_TCPまたはPV_STYPE_DIRTCPが設定されている場合は、ネットワークポ
 *		ートのオープンとTCPによる接続を行う。
 *		PV_STYPE_UDPDIRまたはPV_STYPE_UDPOTHが設定されている場合は、ネットワーク
 *		のポートオープンを行う。
 *		ps_serialのボーレートなどの設定について、
 * 		PV_STYPE_TTYの場合は、シリアルポートオープン成功した場合は、ps_serialの
 * 		設定情報に基づき通信速度や通信条件をポートに対して設定する。
 * 		PV_STYPE_TCPの場合は、接続先の変換器で通信速度等が設定されているため、参
 *		考程度の情報として扱う。
 *		PV_STYPE_DIRTCP、PV_STYPE_UDPDIR、PV_STYPE_UDPOTHの場合は、相手先装置と
 *		の直接通信となるため使用しない。
 * 戻り値：
 *		 1：正常終了（オープン実施正常）
 * 		 0：正常終了（オープン済み）
 * 		-1：異常終了
 */
int	commfunc_serial_open( struct st_serialinf *ps_serial )
{
	int						serialSet;					/* シリアル設定 */
	int						sockOptVal = 1;				/* ソケットオプション設定値 */
	int						retCode;					/* 戻り値 */
	struct termios			termio;						/* シリアルポート設定用 */
	struct sockaddr_in		dstAddr;					/* TCP/UDP接続用 */
	struct sockaddr_in		chkAddr;					/* ローカルポートIPアドレス取得用 */
	int						chkLength;					/* データサイズ */
	char					errMsg[PV_MSG_SIZE_LOG];

	/* シリアルポートはオープン済み？ */
	if( ps_serial->status != PV_STAT_COM_CLOSE )
	{
		return( 0 );
	}

	/* USB-485変換器フラグの初期化 */
	ps_serial->ttyUSBFlag = 0;

	/* デバイスタイプはシリアルポート（EIA232経由）？ */
	if( ps_serial->deviceType == PV_STYPE_TTY )
	{
		/* シリアルポートが実在するか？ */
		if( (retCode = access( ps_serial->targetName, F_OK )) != 0 )
		{
			sprintf( errMsg, "ERROR Serial Device Not Found (%s) Line(%02d) Errno(%d)", ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* シリアルポートオープン */
		ps_serial->fd_sockfd = open(ps_serial->targetName, O_RDWR | O_NOCTTY);
		if( ps_serial->fd_sockfd == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR Serial Device Open Error (%s) Line(%02d) Errno(%d)", ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		sprintf( errMsg, "OPEN SerialType(Direct:EIA232->EIA485:%s) Line(%02d)", ps_serial->targetName, ps_serial->lineNo );
		commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );

		/* ステータスにオープンを設定	*/
		ps_serial->status = PV_STAT_COM_OPEN;

		/* シリアルポートの設定 */
		termio.c_iflag = 0; /* 入力モードの設定 */
		termio.c_oflag = 0; /* 出力モードの設定 */
		termio.c_cflag = 0; /* 制御モードの設定 */
		termio.c_lflag = 0; /* ローカルモードの設定初期化 */ 
	
		/* 入力モードの設定 */
		/* （IGNPAR : パリティチェックを無効） */
		termio.c_iflag = IGNPAR ; 
		/* 出力モードの設定 */
		/* （RAWモードで出力を行う） */
		termio.c_oflag = 0;
		/* 制御モードの設定 */
		termio.c_cflag = 0 ;
		/* （CLOCAL : モデムの制御線を無視） */
		termio.c_cflag |= CLOCAL ;
		/* （CREAD  : 受信を有効に設定） */
		termio.c_cflag |= CREAD ;
		/* 通信速度の設定 */
		switch(ps_serial->baudRate)
		{
			case 2400:
					termio.c_cflag |= B2400;
					break;
			case 4800:
					termio.c_cflag |= B4800;
					break;
			case 9600:
					termio.c_cflag |= B9600;
					break;
			case 19200:
					termio.c_cflag |= B19200;
					break;
			case 38400:
					termio.c_cflag |= B38400;
					break;
			case 57600:
					termio.c_cflag |= B57600;
					break;
			case 115200:
					termio.c_cflag |= B115200;
					break;
			default:
					termio.c_cflag |= B9600;
					break;
		}
		/* データサイズの設定 */
		switch(ps_serial->dataSize)
		{
			case 7:
					termio.c_cflag |= CS7;
					break;
			case 8:
					termio.c_cflag |= CS8;
					break;
			default:
					termio.c_cflag |= CS8;
					break;
		}
		/* ストップビットの設定 */
		switch(ps_serial->stopBit)
		{
			case 2:
					/* CSTOPB : ストップビットを1->2に変更 */
					termio.c_cflag |= CSTOPB;
					break;
			default:
					break;
		}
		/* パリティの設定 */
		switch(ps_serial->parity)
		{
			case 0:
					/* パリティ設定なし */
					break;
			case 1:
					/* 奇数パリティを設定 */
					termio.c_cflag |= (PARENB | PARODD);
					break;
			case 2:
					/* 偶数パリティを設定 */
					termio.c_cflag |= PARENB;
					break;
			default:
					break;
		}
		/* ローカルモードの設定 */
		termio.c_lflag = 0;
		/* キャラクタ間タイマを使わない */
		termio.c_cc[VTIME] = 0;
		/* 読み込みブロックを行わない */
		termio.c_cc[VMIN] = 0;
	
		/* シリアルポートの設定を反映する */
		tcsetattr( ps_serial->fd_sockfd, TCSANOW,  &termio );

		/* ステータスに通信可を設定	*/
		ps_serial->status = PV_STAT_COM_CONNECT;

		/* ttyタイプが /dev/ttyUSB* の場合 */
		if( strstr( ps_serial->targetName, "ttyUSB" ) != NULL )
		{
			/* ttyUSBフラグを設定 */
			ps_serial->ttyUSBFlag = 1;

			/* DTR信号を ON  に設定し、受信用トランシーバを有効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_DTR;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIS, &serialSet );
			if( retCode != 0 ) printf("ioctl Error Ret(%d) Errno(%d)\n", retCode, errno);

			/* RTS信号を OFF に設定し、送信用トランシーバを無効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_RTS;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIC, &serialSet );
			if( retCode != 0 ) printf("ioctl Error Ret(%d) Errno(%d)\n", retCode, errno);

		}
	}
	/* デバイスタイプはＬＡＮ－４８５変換器タイプまたは直接ＴＣＰ接続タイプ？ */
	else if( ( ps_serial->deviceType == PV_STYPE_TCP ) || ( ps_serial->deviceType == PV_STYPE_DIRTCP ) )
	{
		/* ソケットオープン */
		/* （IPv4、ストリーム） */
		ps_serial->fd_sockfd = socket( AF_INET, SOCK_STREAM, 0 );
		if( ps_serial->fd_sockfd == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR TCP Socket Error (%s) Line(%02d) Errno(%d)",
				ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* ソケットオプションの設定（KEEPALIVEによる接続監視） */
		if( setsockopt( ps_serial->fd_sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&sockOptVal, sizeof(sockOptVal) ) != 0 )
		{
			sprintf( errMsg, "ERROR TCP Socket Option Setting Error (%s) Line(%02d) Errno(%d)",
				ps_serial->targetName, ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
		}

		/* ステータスにオープンを設定	*/
		ps_serial->status = PV_STAT_COM_OPEN;

		/* アドレス情報を初期化 */
		memset( &dstAddr, 0, sizeof( dstAddr ) );
		/* ネットワークファミリの設定 */
		dstAddr.sin_family = AF_INET;
		/* アクセスポートの設定 */
		dstAddr.sin_port   = htons( ps_serial->portNumb );
		/* IPアドレスの設定 */
		if( inet_aton( ps_serial->targetName, &(dstAddr.sin_addr)) == 0 )
		{
			/* IPアドレスが無効な場合 */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;

			sprintf( errMsg, "ERROR TCP Device IP-Addr Error (%s:%d) Line(%02d)",
				ps_serial->targetName, ps_serial->portNumb, ps_serial->lineNo );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* シリアル変換機器（変換サーバ）または通信対象装置に接続 */
		if( connect( ps_serial->fd_sockfd, (struct sockaddr *)&dstAddr, sizeof( dstAddr ) ) == -1 )
		{
			/* 接続エラー */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR TCP Device CONNECT Error (%s:%d) Line(%02d) Errno(%d)",
				ps_serial->targetName, ps_serial->portNumb, ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* ローカルポートに関する情報の取得 */
		chkLength = sizeof( chkAddr );
		memset( &chkAddr, 0, chkLength );
		if( getsockname( ps_serial->fd_sockfd, (struct sockaddr *)&chkAddr, &chkLength ) == -1 )
		{
			/* パラメータ取得エラー */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR TCP GETSOCKNAME Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* 相手先装置のIPアドレス（ネットワークバイトオーダー）を格納 */
		ps_serial->dstIPAddr = dstAddr.sin_addr.s_addr;
		/* ローカルポートのIPアドレス（ネットワークバイトオーダー）を格納 */
		ps_serial->locIPAddr = chkAddr.sin_addr.s_addr;
		/* ローカルポート番号（ホストバイトオーダー）を格納 */
		ps_serial->portNumbLoc = ntohs(chkAddr.sin_port);

		if( ps_serial->log_open == 0 )
		{
			sprintf( errMsg, "OPEN SerialType(TCP:Etherner:%d->%s:%d) Line(%02d)",
				ps_serial->portNumbLoc, ps_serial->targetName, ps_serial->portNumb, ps_serial->lineNo );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
		}

		/* ステータスに通信可を設定	*/
		ps_serial->status = PV_STAT_COM_CONNECT;
	}
	/* デバイスタイプはＵＤＰ接続タイプ？ */
	else if( ( ps_serial->deviceType == PV_STYPE_UDPDIR ) || ( ps_serial->deviceType == PV_STYPE_UDPOTH ) )
	{
		/* ソケットオープン（送信先ポート）（IPv4、データグラム） */
		ps_serial->fd_sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
		if( ps_serial->fd_sockfd == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR UDP(SEND) Socket Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* 送信用ポートの設定 */
		memset( &dstAddr, 0, sizeof( dstAddr ) );
		/* ネットワークファミリの設定 */
		dstAddr.sin_family      = AF_INET;
		/* ポート番号の指定（自動割当を使用するため0を指定） */
		dstAddr.sin_port        = 0;
		/* インターフェイスの指定（自動選択のためINADDR_ANYを指定） */
		dstAddr.sin_addr.s_addr = INADDR_ANY;

		/* 送信用ポートをバインド */
		if( bind( ps_serial->fd_sockfd, (struct sockaddr *)&dstAddr, sizeof(dstAddr) ) == -1 )
		{
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR UDP(SEND) BIND Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}

		/* ローカルポートに関する情報の取得 */
		chkLength = sizeof( chkAddr );
		memset( &chkAddr, 0, chkLength );
		if( getsockname( ps_serial->fd_sockfd, (struct sockaddr *)&chkAddr, &chkLength ) == -1 )
		{
			/* パラメータ取得エラー */
			/* ソケットをクローズ */
			close( ps_serial->fd_sockfd );
			/* ステータスに未オープンを設定	*/
			ps_serial->status = PV_STAT_COM_CLOSE;
			sprintf( errMsg, "ERROR UDP(SEND) GETSOCKNAME Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
			return( -1 );
		}
		/* ローカルポートのIPアドレス（ネットワークバイトオーダー）を格納 */
		ps_serial->locIPAddr = chkAddr.sin_addr.s_addr;
		/* ローカルポート番号（ホストバイトオーダー）を格納 */
		ps_serial->portNumbLoc = ntohs(chkAddr.sin_port);

		/* ＵＤＰアクセスが送信元とは異なるポートに返送するタイプの場合 */
		if( ps_serial->deviceType == PV_STYPE_UDPOTH )
		{
			/* 受信専用ソケットオープン（送信先ポート）（IPv4、データグラム） */
			ps_serial->fd_sockfdUdp = socket( AF_INET, SOCK_DGRAM, 0 );
			if( ps_serial->fd_sockfdUdp == -1 )
			{
				/* 送信用ソケットをクローズ */
				close( ps_serial->fd_sockfd );
				/* ステータスに未オープンを設定	*/
				ps_serial->status = PV_STAT_COM_CLOSE;
				sprintf( errMsg, "ERROR UDP(RCV) Socket Error (%s) Line(%02d) Errno(%d)",
					ps_serial->targetName, ps_serial->lineNo, errno );
				commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
				return( -1 );
			}

			/* 受信専用ポートの設定 */
			memset( &dstAddr, 0, sizeof( dstAddr ) );
			/* ネットワークファミリの設定 */
			dstAddr.sin_family      = AF_INET;
			/* ポート番号の指定（受信ポート番号を設定） */
			dstAddr.sin_port        = htons( ps_serial->baudRate );
			/* インターフェイスの指定（自動選択のためINADDR_ANYを指定） */
			dstAddr.sin_addr.s_addr = INADDR_ANY;

			/* 受信専用ポートをバインド */
			if( bind( ps_serial->fd_sockfdUdp, (struct sockaddr *)&dstAddr, sizeof(dstAddr) ) == -1 )
			{
				/* 送信用ソケットをクローズ */
				close( ps_serial->fd_sockfd );
				/* 受信用ソケットをクローズ */
				close( ps_serial->fd_sockfdUdp );
				/* ステータスに未オープンを設定	*/
				ps_serial->status = PV_STAT_COM_CLOSE;
				sprintf( errMsg, "ERROR UDP(RECV) BIND Error Line(%02d) Errno(%d)", ps_serial->lineNo, errno );
				commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
				return( -1 );
			}
		}

		/* ステータスに通信可を設定	*/
		ps_serial->status = PV_STAT_COM_CONNECT;

		/* ＵＤＰアクセスが送信元ポートに返送するタイプの場合 */
		if( ps_serial->deviceType == PV_STYPE_UDPDIR )
		{
			sprintf( errMsg, "OPEN SerialType(UDP:Etherner:%d->DST:%d) Line(%02d)",
				ps_serial->portNumbLoc, ps_serial->portNumb, ps_serial->lineNo );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
		}
		else
		{
			sprintf( errMsg, "OPEN SerialType(UDP:Etherner:%d->DST:%d) (RECV:Ethernet:%d) Line(%02d)",
				ps_serial->portNumbLoc, ps_serial->portNumb, ps_serial->baudRate, ps_serial->lineNo );
			commfunc_syslog_msgset( "commfunc_serial_open()", errMsg );
		}
	}
	else
	{
		/* ステータスに未オープンを設定	*/
		ps_serial->status = PV_STAT_COM_CLOSE;
		return( -1 );
	}

	return( 1 );
}
	
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 回線クローズ
 *
 * 機能：
 * 		ps_serialで指定された通信回線（シリアルポート）をクローズする。
 * 戻り値：
 * 		 0：正常終了
 */
int	commfunc_serial_close( struct st_serialinf *ps_serial )
{
	if( ps_serial->status != PV_STAT_COM_CLOSE )
	{
		close( ps_serial->fd_sockfd );

		/* デバイスタイプがＵＤＰアクセス（送信元ポートと異なるポートに返送される場合） */
		if( ps_serial->deviceType == PV_STYPE_UDPOTH )
		{
			close( ps_serial->fd_sockfdUdp );
		}

		ps_serial->status = PV_STAT_COM_CLOSE;
	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ送信（シリアルまたはストリームポート）
 *
 * 書式：
 * 		commfunc_serial_send( struct st_serialinf *ps_serial, char *psendBuff, int buffLength, struct st_serialLog *ps_serialLog );
 * 機能：
 * 		ps_serialinfのfd_sockfdに格納されたファイルディスクリプタからpsendBuffに
 * 		格納されたシリアルデータをbuffLengthバイト分送信する。
 * 		送信処理時にエラーが発生した場合は受信処理を中止し、ファイルディスクリプ
 * 		タのポートをcloseする。
 *		送信データをps_serialLogに格納する。
 * 戻り値：
 * 		データを正常送信した場合は、送信データのバイト数を返す。
 * 		ファイルディスクリプタで指定するポートが未OPENの場合は
 * 		PV_SERIAL_ERRNOTREADY(-1)を返す。
 * 		ライトエラーが発生した場合は PV_SERIAL_ERRACCESS(-3)を返す。
 * 入力：
 * 		ps_serial
 * 			シリアルポートに関する情報を指定する
 * 		psendBuff
 * 			送信データを格納するバッファを指定する
 */
int	commfunc_serial_send( struct st_serialinf *ps_serial, char *psendBuff, int buffLength, struct st_serialLog *ps_serialLog )
{
	char			recvBuff[4096];
	int				serialSet;
	int				loopCnt;
	int				retCode;
	struct timeval	nowtime;
	struct timespec	sleepTm;
	char			errMsg[PV_MSG_SIZE_LOG];

	/* 送信完了後の待ち時間を設定（非インテリジェントのEIA232-EIA485装置が送信から受信への切替時間を考慮） */
	sleepTm.tv_sec = 0;
	/* 通信速度により送信後待ち時間を設定 */
	switch(ps_serial->baudRate)
	{
		case 2400:
				/* 4.587msec/Byte */
				sleepTm.tv_nsec = 4587000 * buffLength;
				break;
		case 4800:
				/* 2.294msec/Byte */
				sleepTm.tv_nsec = 2294000 * buffLength;
				break;
		case 9600:
				/* 1.148msec/Byte */
				sleepTm.tv_nsec = 1148000 * buffLength;
				break;
		case 19200:
				/* 0.574msec/Byte */
				sleepTm.tv_nsec =  574000 * buffLength;
				break;
		case 38400:
				/* 0.287msec/Byte */
				sleepTm.tv_nsec =  287000 * buffLength;
				break;
		case 57600:
				/* 0.191msec/Byte */
				sleepTm.tv_nsec =  191000 * buffLength;
				break;
		case 115200:
				/* 0.095msec/Byte */
				sleepTm.tv_nsec =   95000 * buffLength;
				break;
		default:
				break;
	}

	/* ポートのステータスが通信可以外の場合 */
	if( ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* 送信データが存在する場合 */
	if( buffLength > 0 )
	{
		/*----------------------------------*/
		/* 送信前に受信バッファをクリアする */
		/*----------------------------------*/

		/* デバイスタイプがＴＴＹの場合 */
		if( ps_serial->deviceType == PV_STYPE_TTY )
		{
			/* ＴＴＹ用受信バッファをクリア */
			tcflush( ps_serial->fd_sockfd, TCIFLUSH );
		}
		else
		{
			/* TCPポートから未受信分のデータを全て読み出す（破棄する） */
			while( 1 )
			{
				/* 非停止モードで読み出す */
				retCode = recv( ps_serial->fd_sockfd, recvBuff, 1, MSG_DONTWAIT );
				/* 戻り値が0以下の場合は全てデータを読み出したと判定し、終了する */
				if( retCode <= 0 ) break;
			}
		}

		/* RATOC社製USB-EIA485変換器は、RTS/DTRの信号線のOn/OffによりEIA485の送受信ドライバを切替る */
		/* LINEEYE社製のEIA232-EIA485変換器はTxDのストップビットをチェックして送受信を切り替えるためこの処理は不要 */
		/* 一般的なPL2303を使用したUSB-EIA485変換器もLINEEYE社製品と同様の処理 */
		/* RTS/DTR信号の切替はRATOC社以外の製品には影響がないため、全機器共通処理としたかったが通常のEIA232ポートに */
		/* 対してioctlで設定変更を行うと切替処理に1秒以上かかってしまうため、USB-EIA485変換器のみに適用する */

		/* ポートがUSB-EIA485変換器タイプ？ */
		if( ps_serial->ttyUSBFlag )
		{
			/* RTS信号を ON  に設定し、送信用トランシーバを有効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_RTS;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIS, &serialSet );
			if( retCode != 0 ) printf("ioctl Error ret:%d errno:%d\n", retCode, errno);

			/* DTR信号を OFF に設定し、受信用トランシーバを無効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			/* （REX-USB70は送信データもローカルエコーバックするため、受信を無効にしてローカルエコーバックを排除） */
			serialSet = TIOCM_DTR;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIC, &serialSet );
			if( retCode != 0 ) printf("ioctl Error ret:%d errno:%d\n", retCode, errno);

#ifdef DEBUG_SERIAL
			ioctl( ps_serial->fd_sockfd, TIOCMGET, &serialSet );
			if( serialSet & TIOCM_DTR ) printf("[DTR] ON\n"); else printf("[DTR] OFF\n");
			if( serialSet & TIOCM_RTS ) printf("[RTS] ON\n"); else printf("[RTS] OFF\n");
#endif
		}

		/* ポートはダイレクト（EIA232経由）？ */
		if( ps_serial->deviceType == PV_STYPE_TTY )
		{
			/* 送信データを出力する */
			retCode = write(ps_serial->fd_sockfd, psendBuff, buffLength);
		}
		else
		{
			/* 送信データを出力する */
			retCode = send(ps_serial->fd_sockfd, psendBuff, buffLength, MSG_NOSIGNAL );
		}
		/* 書き込み異常が発生したか？ */
		if( retCode <= 0 )
		{
			/* ポートをクローズ */
			commfunc_serial_close( ps_serial );
			/* ポート異常 */
			return( PV_SERIAL_ERRACCESS );
		}
		else
		{
			/* ログを格納 */
			/* 現在時刻を取得 */
			gettimeofday( &nowtime, NULL );
			/* 送信情報を記録 */
			if( PV_SERIAL_LOGSIZ > ps_serialLog->dataCnt )
			{
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = nowtime.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = nowtime.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_SEND;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = buffLength;
				memcpy( ps_serialLog->s_logData[ps_serialLog->dataCnt].data, psendBuff, buffLength );
				/* ポートはダイレクト（EIA232経由）？ */
				if( ps_serial->deviceType == PV_STYPE_TTY )
				{
					ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
					ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort   = 0;
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort   = 0;
				}
				else
				{
					/* ローカルおよび通信先のIPアドレスとポート番号を格納 */
					ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = ps_serial->locIPAddr;
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = ps_serial->dstIPAddr;
					ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort   = ps_serial->portNumbLoc;
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort   = ps_serial->portNumb;
				}
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_send(): ERROR LOG BUFFER OVER!!\n");
			}
		}

		/* ポートがUSB-EIA485変換器タイプ？ */
		if( ps_serial->ttyUSBFlag )
		{
			/* REX-USB70の送受信切替対応のため指定時間待機する */
			nanosleep( &sleepTm, NULL );

			/* DTR信号を ON  に設定し、受信用トランシーバを有効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_DTR;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIS, &serialSet );
			if( retCode != 0 ) printf("ioctl Error Ret(%d) Errno(%d)\n", retCode, errno);

			/* RTS信号を OFF に設定し、送信用トランシーバを無効に設定する（RATOC社製USB-EIA485変換器 REX-USB70対応） */
			serialSet = TIOCM_RTS;
			retCode = ioctl( ps_serial->fd_sockfd, TIOCMBIC, &serialSet );
			if( retCode != 0 ) printf("ioctl Error Ret(%d) Errno(%d)\n", retCode, errno);

#ifdef DEBUG_SERIAL 
			ioctl( ps_serial->fd_sockfd, TIOCMGET, &serialSet );
			if( serialSet & TIOCM_DTR ) printf("[DTR] ON\n"); else printf("[DTR] OFF\n");
			if( serialSet & TIOCM_RTS ) printf("[RTS] ON\n"); else printf("[RTS] OFF\n");
#endif
		}
	}
	else
	{
		retCode = 0;
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ送信（ＵＤＰ送信）
 *
 * 書式：
 * 		commfunc_serial_sendto( struct st_serialinf *ps_serial, char *psendBuff, int buffLength, unsigned int dstIPAddr, struct st_serialLog *ps_serialLog );
 * 機能：
 * 		ps_serialinfのfd_sockfdに格納されたファイルディスクリプタからpsendBuffに
 * 		格納されたシリアルデータをbuffLengthバイト分をＵＤＰで送信する。
 *		ＵＤＰの送信先ホストは、引数dstIPAddr（ネットワークバイトオーダー形式）
 *		で指定されたＩＰアドレスとし、送信先ポートはps_serialのportNumbで指定さ
 *		れたポートとする。
 * 		送信処理時にエラーが発生した場合は受信処理を中止し、ファイルディスクリプ
 * 		タのポートをcloseする。
 *		送信データをps_serialLogに格納する。
 * 戻り値：
 * 		データを正常送信した場合は、送信データのバイト数を返す。
 * 		ファイルディスクリプタで指定するポートが未OPENの場合は
 * 		PV_SERIAL_ERRNOTREADY(-1)を返す。
 * 		ライトエラーが発生した場合は PV_SERIAL_ERRACCESS(-3)を返す。
 * 入力：
 * 		ps_serial
 * 			シリアルポートに関する情報を指定する
 * 		psendBuff
 * 			送信データを格納するバッファを指定する
 */
int	commfunc_serial_sendto( struct st_serialinf *ps_serial, char *psendBuff, int buffLength, unsigned int dstIPAddr, struct st_serialLog *ps_serialLog )
{
	char				recvBuff[2];			/* 受信バッファ */
	int					fd_sock;				/* ファイルディスクリプタ */
	int					loopCnt;				/* ループカウンタ */
	int					retCode;				/* 関数戻り値 */
	int					infoSize;				/* 情報サイズ */
	struct timeval		nowtime;				/* 現在時刻 */
	struct sockaddr_in	sndInfo;				/* 送信先情報 */
	struct sockaddr_in	rcvInfo;				/* 送信元情報 */
	char				errMsg[PV_MSG_SIZE_LOG];

	/* ポートのステータスが通信可以外の場合 */
	if( ps_serial->status != PV_STAT_COM_CONNECT)
	{
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* 送信データが存在する場合 */
	if( buffLength > 0 )
	{
		/*------------------------------------------------------------------*/
		/* 送信前に受信バッファをクリアする（受信済みデータは全て読み出す） */
		/*------------------------------------------------------------------*/
		/* ＵＤＰアクセスタイプによりファイルディスクリプタを設定 */
		if( ps_serial->deviceType == PV_STYPE_UDPDIR )
		{
			fd_sock = ps_serial->fd_sockfd;
		}
		else
		{
			fd_sock = ps_serial->fd_sockfdUdp;
		}
		/* 受信情報格納領域の初期化 */
		infoSize = sizeof( rcvInfo );
		memset( &rcvInfo, 0, infoSize );
		/* 受信用ポートから未受信分のデータを全て読み出す（破棄する） */
		while( 1 )
		{
			/* 非停止モードで読み出す */
			retCode = recvfrom( fd_sock, recvBuff, 1, MSG_DONTWAIT, (struct sockaddr *)&rcvInfo, &infoSize );
			/* 戻り値がエラーの場合は全てデータを読み出したと判定し、終了する */
			if( retCode == -1 ) break;
		}

		/* 送信先情報を設定する */
		memset( &sndInfo, 0, sizeof(sndInfo) );
		sndInfo.sin_family      = AF_INET;
		sndInfo.sin_port        = htons( ps_serial->portNumb );
		sndInfo.sin_addr.s_addr = dstIPAddr;

		/* 送信データを出力する */
		retCode = sendto(ps_serial->fd_sockfd, psendBuff, buffLength, 0, (struct sockaddr *)&sndInfo, sizeof(sndInfo) );

		/* 書き込み異常が発生したか？ */
		if( retCode <= 0 )
		{
			sprintf( errMsg, "sendto() Error ret:%d (errno:%d)", retCode, errno );
			commfunc_syslog_msgset( "commfunc_serial_sendto()", errMsg );
			/* ポートをクローズ */
			commfunc_serial_close( ps_serial );
			/* アクセス異常 */
			return( PV_SERIAL_ERRACCESS );
		}
		else
		{
			/* ログを格納 */
			/* 現在時刻を取得 */
			gettimeofday( &nowtime, NULL );
			/* 状変情報を記録 */
			if( PV_SERIAL_LOGSIZ > ps_serialLog->dataCnt )
			{
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = nowtime.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = nowtime.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_SEND;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = buffLength;
				memcpy( ps_serialLog->s_logData[ps_serialLog->dataCnt].data, psendBuff, buffLength );
				/* ローカルおよび通信先のIPアドレスとポート番号を格納 */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = ps_serial->locIPAddr;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = dstIPAddr;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort   = ps_serial->portNumbLoc;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort   = ps_serial->portNumb;
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_sendto(): ERROR LOG BUFFER OVER!!\n");
			}
		}
	}
	else
	{
		retCode = 0;
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ受信（シリアルまたはストリームポート）
 *
 * 書式：
 * 		commfunc_serial_recv( struct st_serialinf *ps_serial, char *precvBuff, struct timeval *plimit, struct st_serialLog *ps_serialLog );
 * 機能：
 * 		ps_serialinfのfd_sockfdに格納されたファイルディスクリプタからデータを受
 *		信する。
 * 		受信したデータは引数precvBuffのアドレスに格納する。
 * 		一度に受信可能なデータは4096バイトまでとし、最大データを受信する場合は引
 * 		数precvBuffの受信バッファを4096バイト以上確保しておく必要がある。
 * 		データが未受信の場合には、引数plimitで指定した時刻までデータ着信を待つ。
 * 		データを1バイトも受信することなく指定時刻に達した場合は受信処理を中止する。
 * 		受信処理時にエラーが発生した場合は受信処理を中止し、ファイルディスクリプ
 * 		タのポートをcloseする。
 * 戻り値：
 * 		データを受信した場合は、受信データのバイト数を返す。
 * 		ファイルディスクリプタで指定するポートが未OPENの場合は 
 * 		PV_SERIAL_ERRNOTREADY(-1)を返す。
 * 		タイムアウトが発生した場合は PV_SERIAL_ERRTIMEOUT(-2)を返す。
 * 		リードエラーが発生した場合は PV_SERIAL_ERRACCESS(-3)を返す。
 * 入力：
 * 		ps_serial
 * 			ポートに関する情報を指定する
 * 		precvBuff
 * 			受信したデータを格納するバッファを指定する
 * 			（4096バイト以上の領域を確保する）
 * 		plimit
 * 			struct timeval 形式で受信期限を将来時刻で指定する（タイムアウトを30秒
 * 			とした場合は、現在時刻から30秒後の時刻をplimitに指定して呼び出す）
 */
int	commfunc_serial_recv(struct st_serialinf *ps_serial, char *precvBuff, struct timeval *plimit, struct st_serialLog *ps_serialLog )
{
	fd_set			rfds;					/* ファイルディスクリプタ集合 */
	char			readBuff[4096];			/* リードバッファ */
	int				readCnt = 0;			/* データ受信バイト数 */
	struct timeval	timedata;				/* 現在時刻取得用 */
	struct timeval	timeout;				/* selectタイムアウト設定用 */
	long			limit_sec;				/* 応答タイムアウト時刻保存用（sec） */
	long			limit_usec;				/* 応答タイムアウト時刻保存用（usec） */
	int				retCode;				/* 関数戻り値 */
	int				loopCnt;				/* ループカウンタ */

	/* ファイルディスクリプタ集合にポートのファイルディスクリプタを登録 */
	FD_ZERO(&rfds);
	FD_SET(ps_serial->fd_sockfd, &rfds);
	
	/* ポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
#if 0
		printf("commfunc_serial_recv(): ERROR port not ready\n");
#endif
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* 現在時刻の取得 */
	gettimeofday(&timedata, NULL);

#if 0
	printf("commfunc_serial_recv() : NOW:%d.%06d , LIM:%d.%06d\n",
		timedata.tv_sec, timedata.tv_usec, plimit->tv_sec, plimit->tv_usec );
#endif

	/* 以下の何れかの条件を満たすまでポートからデータを読み込む */
	/* ・データを受信 */
	/* ・タイムアウト */
	if( (plimit->tv_sec > timedata.tv_sec) ||
		( (plimit->tv_sec == timedata.tv_sec) && (plimit->tv_usec > timedata.tv_usec) )  )
	{
		/* 現在時刻とタイムアウト時刻を元にselectのタイムアウト時間を設定 */
		if(timedata.tv_usec > plimit->tv_usec)
		{
			timeout.tv_usec = plimit->tv_usec + 1000000 - timedata.tv_usec;
			timeout.tv_sec  = plimit->tv_sec - 1 - timedata.tv_sec;
		}
		else
		{
			timeout.tv_usec = plimit->tv_usec - timedata.tv_usec;
			timeout.tv_sec  = plimit->tv_sec - timedata.tv_sec;
		}

		/* データを受信するまでselectを使用して待機する */
		retCode = select(ps_serial->fd_sockfd + 1, &rfds, NULL, NULL, &timeout);

		/* select異常（ポート異常）の場合 */
		if(retCode == -1)
		{
			/* ポートをクローズ */
			commfunc_serial_close( ps_serial );
			/* ポート異常 */
#ifdef PV_DEBUG
			printf("commfunc_serial_recv(): ERROR select error\n");
#endif
			return( PV_SERIAL_ERRACCESS );
		}
		/* selectタイムアウト（データ未受信の場合） */
		else if( retCode == 0)
		{
			/* タイムアウト	*/
#ifdef PV_DEBUG
			printf("commfunc_serial_recv(): ERROR timeout (select)\n");
#endif

			/* ログ領域が格納可能な場合 */
			if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
			{
				/* 現在時刻の取得 */
				gettimeofday(&timedata, NULL);
				/* ログを格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_TOUT;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = 0;
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_recv(): ERROR LOG BUFFER OVER!!\n");
			}

			return( PV_SERIAL_ERRTIMEOUT );
		}

		/* シリアルポートからデータ読み出し */
		readCnt = read(ps_serial->fd_sockfd, precvBuff, 4096);

		/* 負値（リードエラー）または０（ファイル終端＝デバイス異常）の場合 */
		if( readCnt <= 0 )
		{
			/* ポートをクローズ */
			commfunc_serial_close( ps_serial );
			/* ポート異常 */
#ifdef PV_DEBUG
			printf("commfunc_serial_recv(): ERROR read error\n");
#endif

			/* ログ領域が格納可能な場合 */
			if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
			{
				/* 現在時刻の取得 */
				gettimeofday(&timedata, NULL);
				/* ログを格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_ERR;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = 0;
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_recv(): ERROR LOG BUFFER OVER!!\n");
			}

			return( PV_SERIAL_ERRACCESS );
		}
		else
		{
			/* ログ領域が格納可能な場合 */
			if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
			{
				/* 現在時刻の取得 */
				gettimeofday(&timedata, NULL);
				/* ログを格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_READ;
				/* データ長が保存バッファ以内の場合 */
				if( readCnt < PV_SERIAL_LOGLEN )
				{
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = readCnt;
					memcpy( ps_serialLog->s_logData[ps_serialLog->dataCnt].data, precvBuff, readCnt );
				}
				else
				{
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = PV_SERIAL_LOGLEN;
					memcpy( ps_serialLog->s_logData[ps_serialLog->dataCnt].data, precvBuff, PV_SERIAL_LOGLEN );
				}
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = ps_serial->locIPAddr;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = ps_serial->dstIPAddr;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = ps_serial->portNumbLoc;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = ps_serial->portNumb;
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_recv(): ERROR LOG BUFFER OVER!!\n");
			}
		}
	}
	else
	{
		/* タイムアウト */
#ifdef PV_DEBUG
		printf("commfunc_serial_recv(): ERROR timeout (normal)\n");
#endif

		/* ログ領域が格納可能な場合 */
		if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
		{
			/* 現在時刻の取得 */
			gettimeofday(&timedata, NULL);
			/* ログを格納する */
			ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_TOUT;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = 0;
			ps_serialLog->dataCnt += 1;
		}
		else
		{
			printf("commfunc_serial_recv(): ERROR LOG BUFFER OVER!!\n");
		}
		return( PV_SERIAL_ERRTIMEOUT );
	}
	/* 正常受信完了 */
	return( readCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * データ受信（ＵＤＰ受信）
 *
 * 書式：
 * 		commfunc_serial_recvfrom( struct st_serialinf *ps_serial,
 *		                          char *precvBuff,
 *		                          unsigned int rcvIPAddr,
 *		                          struct timeval *plimit,
 *		                          struct st_serialLog *ps_serialLog );
 * 機能：
 * 		ps_serialinfのfd_sockfdまたはfd_sockfdUdpに格納されたファイルディスクリ
 *		プタからデータを受信する。
 * 		受信したデータは引数precvBuffのアドレスに格納する。
 * 		一度に受信可能なデータは4096バイトまでとし、最大データを受信する場合は引
 * 		数precvBuffの受信バッファを4096バイト以上確保しておく必要がある。
 * 		データが未受信の場合には、引数plimitで指定した時刻までデータ着信を待つ。
 * 		データを1バイトも受信することなく指定時刻に達した場合は受信処理を中止する。
 * 		受信処理時にエラーが発生した場合は受信処理を中止し、ファイルディスクリプ
 * 		タのポートをcloseする。
 * 戻り値：
 * 		データを受信した場合は、受信データのバイト数を返す。
 * 		ファイルディスクリプタで指定するポートが未OPENの場合は 
 * 		PV_SERIAL_ERRNOTREADY(-1)を返す。
 * 		タイムアウトが発生した場合は PV_SERIAL_ERRTIMEOUT(-2)を返す。
 * 		リードエラーが発生した場合は PV_SERIAL_ERRACCESS(-3)を返す。
 * 入力：
 * 		ps_serial
 * 			ポートに関する情報を指定する
 * 		precvBuff
 * 			受信したデータを格納するバッファを指定する
 * 			（4096バイト以上の領域を確保する）
 * 		plimit
 * 			struct timeval 形式で受信期限を将来時刻で指定する（タイムアウトを30秒
 * 			とした場合は、現在時刻から30秒後の時刻をplimitに指定して呼び出す）
 * 出力：
 *		*rcvIPAddr
 *			受信データ送信元のIPアドレス（ネットワークバイトオーダー形式）を格納
 */
int	commfunc_serial_recvfrom(struct st_serialinf *ps_serial, char *precvBuff, unsigned int *rcvIPAddr, struct timeval *plimit, struct st_serialLog *ps_serialLog )
{
	fd_set				rfds;				/* ファイルディスクリプタ集合 */
	char				readBuff[4096];		/* リードバッファ */
	int					fd_sock;			/* ファイルディスクリプタ */
	int					readCnt = 0;		/* データ受信バイト数 */
	int					infoLength;			/* 情報格納領域サイズ */
	struct timeval		timedata;			/* 現在時刻取得用 */
	struct timeval		timeout;			/* selectタイムアウト設定用 */
	struct sockaddr_in	rcvInfo;			/* 受信情報 */
	long				limit_sec;			/* 応答タイムアウト時刻保存用（sec） */
	long				limit_usec;			/* 応答タイムアウト時刻保存用（usec） */
	int					retCode;			/* 関数戻り値 */
	int					loopCnt;			/* ループカウンタ */

	/* 受信IPアドレスを初期化 */
	*rcvIPAddr = 0;

	/* ＵＤＰアクセスタイプによりファイルディスクリプタを決定 */
	if( ps_serial->deviceType == PV_STYPE_UDPOTH )
		fd_sock = ps_serial->fd_sockfdUdp;
	else
		fd_sock = ps_serial->fd_sockfd;

	/* ファイルディスクリプタ集合にシリアルポートのファイルディスクリプタを登録 */
	FD_ZERO(&rfds);
	FD_SET(fd_sock, &rfds);
	
	/* ポートが受信可能状態ではない？ */
	if(ps_serial->status != PV_STAT_COM_CONNECT)
	{
#if 0
		printf("commfunc_serial_recvfrom(): ERROR port not ready\n");
#endif
		return( PV_SERIAL_ERRNOTREADY );
	}

	/* 現在時刻の取得 */
	gettimeofday(&timedata, NULL);

#if 0
	printf("commfunc_serial_recvfrom() : NOW:%d.%06d , LIM:%d.%06d\n",
		timedata.tv_sec, timedata.tv_usec, plimit->tv_sec, plimit->tv_usec );
#endif

	/* 以下の何れかの条件を満たすまでポートからデータを読み込む */
	/* ・データを受信 */
	/* ・タイムアウト */
	if( (plimit->tv_sec > timedata.tv_sec) ||
		( (plimit->tv_sec == timedata.tv_sec) && (plimit->tv_usec > timedata.tv_usec) )  )
	{
		/* 現在時刻とタイムアウト時刻を元にselectのタイムアウト時間を設定 */
		if(timedata.tv_usec > plimit->tv_usec)
		{
			timeout.tv_usec = plimit->tv_usec + 1000000 - timedata.tv_usec;
			timeout.tv_sec  = plimit->tv_sec - 1 - timedata.tv_sec;
		}
		else
		{
			timeout.tv_usec = plimit->tv_usec - timedata.tv_usec;
			timeout.tv_sec  = plimit->tv_sec - timedata.tv_sec;
		}

		/* データを受信するまでselectを使用して待機する */
		retCode = select(fd_sock + 1, &rfds, NULL, NULL, &timeout);

		/* select異常（ポート異常）の場合 */
		if(retCode == -1)
		{
			/* ポートをクローズ */
			commfunc_serial_close( ps_serial );
			/* ポート異常 */
#ifdef PV_DEBUG
			printf("commfunc_serial_recvfrom(): ERROR select error\n");
#endif
			return( PV_SERIAL_ERRACCESS );
		}
		/* selectタイムアウト（データ未受信の場合） */
		else if( retCode == 0)
		{
			/* タイムアウト	*/
#ifdef PV_DEBUG
			printf("commfunc_serial_recvfrom(): ERROR timeout (select)\n");
#endif

			/* ログ領域が格納可能な場合 */
			if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
			{
				/* 現在時刻の取得 */
				gettimeofday(&timedata, NULL);
				/* ログを格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_TOUT;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = 0;
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_recvfrom(): ERROR LOG BUFFER OVER!!\n");
			}

			return( PV_SERIAL_ERRTIMEOUT );
		}

		/* 受信情報の初期化 */
		infoLength = sizeof( rcvInfo );
		memset( &rcvInfo, 0, infoLength );
		/* ポートからデータ読み出し */
		readCnt = recvfrom(fd_sock, precvBuff, 4096, 0, (struct sockaddr *)&rcvInfo, &infoLength );

		/* 負値（リードエラー）または０（ファイル終端＝デバイス異常）の場合 */
		if( readCnt <= 0 )
		{
			/* ポートをクローズ */
			commfunc_serial_close( ps_serial );
			/* シリアルポート異常 */
#ifdef PV_DEBUG
			printf("commfunc_serial_recvfrom(): ERROR read error\n");
#endif

			/* ログ領域が格納可能な場合 */
			if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
			{
				/* 現在時刻の取得 */
				gettimeofday(&timedata, NULL);
				/* ログを格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_ERR;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = 0;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = 0;
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_recvfrom(): ERROR LOG BUFFER OVER!!\n");
			}

			return( PV_SERIAL_ERRACCESS );
		}
		else
		{
			/* 送信元IPアドレスを設定 */
			*rcvIPAddr = rcvInfo.sin_addr.s_addr;

			/* ログ領域が格納可能な場合 */
			if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
			{
				/* 現在時刻の取得 */
				gettimeofday(&timedata, NULL);
				/* ログを格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_READ;
				/* データ長が保存バッファ以内の場合 */
				if( readCnt < PV_SERIAL_LOGLEN )
				{
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = readCnt;
					memcpy( ps_serialLog->s_logData[ps_serialLog->dataCnt].data, precvBuff, readCnt );
				}
				else
				{
					ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = PV_SERIAL_LOGLEN;
					memcpy( ps_serialLog->s_logData[ps_serialLog->dataCnt].data, precvBuff, PV_SERIAL_LOGLEN );
				}
				/* 送信元送信先IPアドレス・ポート番号を格納する */
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = ps_serial->locIPAddr;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = rcvInfo.sin_addr.s_addr;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = ps_serial->portNumbLoc;
				ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = ntohs(rcvInfo.sin_port);
				ps_serialLog->dataCnt += 1;
			}
			else
			{
				printf("commfunc_serial_recvfrom(): ERROR LOG BUFFER OVER!!\n");
			}
		}
	}
	else
	{
		/* タイムアウト */
#ifdef PV_DEBUG
		printf("commfunc_serial_recvfrom(): ERROR timeout (normal)\n");
#endif

		/* ログ領域が格納可能な場合 */
		if( ps_serialLog->dataCnt < PV_SERIAL_LOGSIZ )
		{
			/* 現在時刻の取得 */
			gettimeofday(&timedata, NULL);
			/* ログを格納する */
			ps_serialLog->s_logData[ps_serialLog->dataCnt].sec     = timedata.tv_sec;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].uSec    = timedata.tv_usec;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].kind    = PV_SERIAL_LOG_TOUT;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].dataLen = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].locIPAddr = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].dstIPAddr = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].locPort = 0;
			ps_serialLog->s_logData[ps_serialLog->dataCnt].dstPort = 0;
			ps_serialLog->dataCnt += 1;
		}
		else
		{
			printf("commfunc_serial_recvfrom(): ERROR LOG BUFFER OVER!!\n");
		}
		return( PV_SERIAL_ERRTIMEOUT );
	}
	/* 正常受信完了 */
	return( readCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 回線情報取得
 *
 * 機能：
 * 		引数 lineNo で指定された通信回線番号のシリアル設定情報を通信回線
 * 		情報ファイル（COMLIINF.dat）から取得し、引数 ps_serial で指定されたアド
 * 		レスへセットする。
 * 		引数 pname で指定された通信プロトコル名称と通信回線情報ファイルに
 * 		設定されている通信プロトコル名称が一致しない場合は異常とする。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_serial_infoget( char lineNo, struct st_serialinf *ps_serial, struct st_COMLIINF *ps_comlininf, char *pname)
{
											/* デバイスタイプ */
	char		devType[5][32] = { "Serial", "TCP485", "TCP/IP", "UDP/IP", "UDP/IP" };
	char		errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	/* 通信回線番号が不正の場合 */
	if( (lineNo < 1) || (lineNo > PV_MAX_COMLINE) )
	{
#if 0
		printf("commfunc_serial_infoget(): ERROR line number range (line:%d)\n", lineNo );
#endif
		return( -1 );
	}

	/* 通信回線が未実装の場合 */
	if( (ps_comlininf + lineNo - 1)->statFlag == 0 )
	{
#if 0
		printf("commfunc_serial_infoget(): ERROR non-exist line (line:%d)\n", lineNo );
#endif
		return( -1 );
	}

	/* 通信プロトコル名称が一致していない場合 */
	if( strncmp( (ps_comlininf + lineNo - 1)->protoName, pname, 12) != 0 )
	{
#if 0
		printf("commfunc_serial_infoget(): ERROR protocol different (line:%d)\n", lineNo);
#endif
		return( -1 );
	}

	/* シリアル設定情報を格納 */
	ps_serial->lineNo     = (int)lineNo;
	ps_serial->deviceType = (int)(ps_comlininf + lineNo - 1)->deviceType;
	strncpy(ps_serial->targetName, (ps_comlininf + lineNo - 1)->targetName, 20);
	ps_serial->portNumb   = (int)(ps_comlininf + lineNo - 1)->portNumb;
	ps_serial->baudRate   = (int)(ps_comlininf + lineNo - 1)->baudRate;
	ps_serial->dataSize   = (int)(ps_comlininf + lineNo - 1)->dataSize;
	ps_serial->parity     = (int)(ps_comlininf + lineNo - 1)->parity;
	ps_serial->stopBit    = (int)(ps_comlininf + lineNo - 1)->stopBit;
	ps_serial->rcvTime    = (int)(ps_comlininf + lineNo - 1)->rcvTime;
	ps_serial->sndTime    = (int)(ps_comlininf + lineNo - 1)->sndTime;
	ps_serial->cycleTime    = (int)(ps_comlininf + lineNo - 1)->cycleTime;

#ifdef PV_DEBUG
    printf("commfunc_serial_infoget() : SERIAL DEVICE INFO [SERIAL LINE:%d]\n", lineNo);
    printf("commfunc_serial_infoget() :   DeviceType : %d\n", ps_serial->deviceType);
    printf("commfunc_serial_infoget() :   DeviceName : %s\n", ps_serial->targetName );
    printf("commfunc_serial_infoget() :   Port       : %d\n", ps_serial->portNumb );
    printf("commfunc_serial_infoget() :   Baudrate   : %d\n", ps_serial->baudRate );
    printf("commfunc_serial_infoget() :   Datasize   : %d\n", ps_serial->dataSize );
    printf("commfunc_serial_infoget() :   Parity     : %d\n", ps_serial->parity );
    printf("commfunc_serial_infoget() :   Stopbit    : %d\n", ps_serial->stopBit );
    printf("commfunc_serial_infoget() :   RcvTime    : %d\n", ps_serial->rcvTime );
    printf("commfunc_serial_infoget() :   SndTime    : %d\n", ps_serial->sndTime );
    printf("commfunc_serial_infoget() :   CycleTime  : %d\n", ps_serial->cycleTime );
#endif

	sprintf( errMsg, "Line:%02d [ Type:%d(%s) Dev/Net:%s Port:%d Bps:%d Size:%d Pari:%d StopB:%d RcvTim:%dms SndTim:%dms CycleTim:%ds ]",
		ps_serial->lineNo,
		ps_serial->deviceType, devType[ps_serial->deviceType - 1],
		ps_serial->targetName, ps_serial->portNumb,
		ps_serial->baudRate, ps_serial->dataSize, ps_serial->parity, ps_serial->stopBit,
		ps_serial->rcvTime, ps_serial->sndTime, ps_serial->cycleTime ); 
	commfunc_syslog_msgset( "commfunc_serial_infoget()", errMsg );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測周期確認処理
 *
 * 機能：
 * 		引数 ps_serial に格納されている通信回線の設定値と、引数 targetCnt で指定
 * 		された通信対象機器数、引数 requestCnt で指定された１台あたりのリクエスト
 * 		回数を元に、収集周期が適正値であるか確認する。
 * 		収集周期が適正値ではない場合は、収集周期を変更する。
 * 		収集周期から１分間の移動平均算出に必要なデータ数を引数 stockCnt に設定す
 * 		る。
 */
int	commfunc_serial_cycletmchk(
	struct st_serialinf *ps_serial,			/* シリアル設定情報 */
	int *minDataCnt,						/* １分間の移動平均算出に必要なデータ数 */
	struct st_COMLIINF *ps_COMLIINF			/* 通信回線情報ファイルポインタ */
)
{
	int			oldTime = 0;				/* 変更前タイマ */
	int			workTime = 0;				/* ワーク */
	char		errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	/* 変更前タイマを取得 */
	oldTime = ps_serial->cycleTime;

	/* 収集周期を本装置で処理可能な周期に調整 */
	/* 設定可能な値は 10000,6000,5000,4000,3000,2000,1000, 秒 */
	if( oldTime > 6000 )
		ps_serial->cycleTime = 10000;
	else if( oldTime > 5000 )
		ps_serial->cycleTime = 6000;
	else if( oldTime > 4000 )
		ps_serial->cycleTime = 5000;
	else if( oldTime > 3000 )
		ps_serial->cycleTime = 4000;
	else if( oldTime > 2000 )
		ps_serial->cycleTime = 3000;
	else if( oldTime > 1000 )
		ps_serial->cycleTime = 2000;
	else if( oldTime > 500 )
		ps_serial->cycleTime = 1000;
	else if( oldTime > 250 )
		ps_serial->cycleTime = 500;
	else if( oldTime > 200 )
		ps_serial->cycleTime = 250;
	else
		ps_serial->cycleTime = 100;

    /* 収集周期から移動平均ストック数を設定 */
	*minDataCnt = 60000 / ps_serial->cycleTime;

	sprintf( errMsg, "CycleTim Adjust [ Line:%02d CycleTim:%d->%ds minDataCnt:%d]",
		ps_serial->lineNo, oldTime, ps_serial->cycleTime, *minDataCnt);
	commfunc_syslog_msgset( "commfunc_serial_cycletmchk()", errMsg );

	/* 通信回線情報に収集周期を格納 */
	(ps_COMLIINF + ps_serial->lineNo - 1)->cycleTime = ps_serial->cycleTime;

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＲＣ演算処理（ＣＣＩＴＴ）
 *
 * 機能：
 * 		引数 *dataPoint で指定されたアドレスから dataLength バイトの範囲のＣＲＣ
 * 		を演算し、*crcData に格納する。
 * 		ＣＲＣはＣＲＣ１６－ＩＴＵ－Ｔ（ＣＣＩＴＴ）に準拠するものとし、ＣＲＣの
 * 		初期値を 0 、反転出力は行わないこととする。
 *
 * 		次の１２バイト分のＣＲＣは 0xCD56 となる。
 * 		 0x01,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00
 *
 * 戻り値：
 * 		0：正常処理
 */
int commfunc_serial_crc16itut( char *dataPoint, int dataLength, int *crcData )
{
	int					inData;			/* 処理データ格納用 */
	int					crc16CCITT;		/* 生成多項式の値 */
	int					loopCnt;		/* ループカウンタ */
	int					loopBit;		/* ループカウンタ */

	/* CRCの初期値を設定 */
	*crcData = 0;

	/* 生成多項式の値を設定(2^16+2^12+2^5+1) */
	crc16CCITT = 0x11021;

	for( loopCnt = 0 ; loopCnt < dataLength ; loopCnt++, dataPoint++ )
	{
		/* １バイト分のデータを取得 */
		inData = *dataPoint;
		/* CRCデータと処理対象データをMSBを揃えてXOR演算を実施 */
		*crcData ^= (inData << 8);
		/* １バイト分ループ */
		for( loopBit = 0 ; loopBit < 8 ; loopBit++ )
		{
			/* CRCデータを左シフト */
			*crcData <<= 1;
			/* キャリーフラグONの場合 */
			if( *crcData & 0x10000 )
				/* CRCデータに生成多項式の値をXOR */
				*crcData ^= crc16CCITT;
		}
	}
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＲＣ演算処理（ＭＯＤＢＵＳプロトコル用）
 *
 * 機能：
 * 		引数 *dataPoint で指定されたアドレスから dataLength バイトの範囲のＣＲＣ
 * 		を演算し、ＣＲＣの上位バイトデータを *crcHi、下位バイトデータを *crcLo 
 *		に格納する。
 * 		ＣＲＣはＭＯＤＢＵＳ仕様に準拠するものとし、ＣＲＣの初期値を 0xFFFF 、反
 *		転出力は行わないこととする。
 *		ＣＲＣ演算はハッシュテーブルを用いた高速演算方式によりおこなう。
 *
 * 		次の６バイト分のＣＲＣは 0xCA31 となる。(crcHi:0xCA / crcLo:0x31)
 * 		 0x01,0x04,0x00,0x00,0x00,0x01
 *
 * 戻り値：
 *		0 : 正常処理
 *
 * 入力：
 *		*dataPoint : CRC対象データ開始位置
 *		dataLength : CRC対象データ長
 *
 * 出力：
 *		*crcHi     : CRC上位バイト
 *		*crcLo     : CRC下位バイト
 *
 * 備考：
 *		ＭＯＤＢＵＳプロトコルでは通常のレジスタデータは上位バイトから先に送信
 *		するが、ＣＲＣデータは下位バイトから送信する点に注意！
 *		例）ＣＲＣが０ｘＣＡ３１の場合は０ｘ３１，０ｘＣＡの順に送信される
 *
 */
int commfunc_serial_crc16modbus( unsigned char *dataPoint, int dataLength, unsigned char *crcHi, unsigned char *crcLo)
{
	int		 tableIndex;				/* CRCテーブルインデックス用 */


	/* CRCデータの初期化（0xFFに設定） */
	*crcHi = 0xFF; 
	*crcLo = 0xFF;

	/* 対象データ長分繰り返す */
	while( dataLength-- )
	{
		/* CRC演算を行う */
		tableIndex = *crcLo ^ *dataPoint++;
		*crcLo = *crcHi ^ modbus_CRCtable_LO[tableIndex];
		*crcHi = modbus_CRCtable_HI[tableIndex];
	}
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ログ出力処理
 *
 * 機能：
 *		引数 logMode の設定により、ps_serialLog に格納されている通信ログを出力す
 * 		る。　logMode に PV_SERIAL_LOG_ON が設定されている場合は、強制的にログを
 *		出力する。PV_SERIAL_LOG_AUTO が設定されている場合は、管理情報ファイルの
 *		通信回線毎のログ出力設定が ON の場合に出力を行う。
 *		出力されるログファイルは通信回線毎に作成され、履歴は過去３回分まで保持さ
 *		れる。
 *
 * 入力：
 *		logMode       : ログ出力モード
 *		               1 (PV_SERIAL_LOG_ON)   : 強制ログ出力
 *		               0 (PV_SERIAL_LOG_AUTO) : 出力設定による自動判断
 *		*ps_serialinf : シリアルポート設定情報ポインタ
 *		*ps_serialLog : ログ格納領域ポインタ
 *
 * 出力：
 *		0             : 正常処理
 *		-1            : 異常終了
 *
 */
int commfunc_serial_logout( int logMode, struct st_serialinf *ps_serialinf, struct st_serialLog *ps_serialLog )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
	unsigned char			*pt1;					/* ポインタ */
	unsigned char			*pt2;					/* ポインタ */
	int						fd;						/* ファイルディスクリプタ */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */
	struct tm				localTime;				/* 年月日時分秒 */
	char					kindName[6][5] = { "SEND", "RECV", "SETD", "ERRO", "TOUT", "OVER" };

	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_serial_logout(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ログ出力を行う必要があるか？ */
	if( logMode == PV_SERIAL_LOG_AUTO )
	{
		/* ログ出力フラグが設定されている場合 */
		if( ps_gpt_PVSYSINF->lineLogFlag[ ps_serialinf->lineNo - 1 ] != 0 )
		{
			/* ログ出力対象に設定 */
			logMode = PV_SERIAL_LOG_ON;
		}
	}

	/* ログ出力が必要な場合 */
	if( logMode == PV_SERIAL_LOG_ON )
	{
		/* 出力用ログファイル名の設定 */
		sprintf( fileName1, "%s%s%02d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );

		/* ファイルが存在しない場合 */
		if( access( fileName1, F_OK ) != 0 )
		{
			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
			{
				return( -1 );
			} 
			close( fd );
		}

		/* システムログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			return( -1 );
		}

		/* ファイルサイズの取得 */
		fileSize = ftell( fp );

		/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
		if(fileSize > 1000000 )
		{ 
			/* ファイルを閉じる */
			fclose( fp );

			/* ログファイルの保存数分繰り返す */
			for(loopCnt1 = PV_LOG_STOCK_NUM ; loopCnt1 > 1 ; loopCnt1-- )	/* [20230902] Chg */
			{
				/* ファイル名の設定 */
				sprintf( fileName1, "%s%s%02d_%d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo, loopCnt1 - 1 );
				sprintf( fileName2, "%s%s%02d_%d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo, loopCnt1 );
				/* 古いファイルの削除 */
				if( access( fileName2, F_OK ) == 0 )
					unlink( fileName2 );
				/* ローテーションの実施 */
				if( access( fileName1, F_OK ) == 0 )
					rename( fileName1, fileName2 );
			}
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s%02d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );
			sprintf( fileName2, "%s%s%02d_1.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			rename( fileName1, fileName2 );

			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH ) ) == -1 )
			{
				return( -1 );
			} 
			close( fd );

			/* システムログファイルをオープン */
			if( (fp = fopen( fileName1, "a" )) == NULL )
			{
				return( -1 );
			}
		}

		/* シリアルログメッセージ出力処理 */
		for( loopCnt1 = 0 ; loopCnt1 < ps_serialLog->dataCnt ; loopCnt1++ )
		{
			/* 年月日時分秒を取得 */
			localtime_r( &ps_serialLog->s_logData[loopCnt1].sec, &localTime );
			fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s[%3d] ",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				ps_serialLog->s_logData[loopCnt1].uSec,
				kindName[ps_serialLog->s_logData[loopCnt1].kind],
				ps_serialLog->s_logData[loopCnt1].dataLen );

			if( ( ( ps_serialinf->deviceType == PV_STYPE_DIRTCP ) ||
			      ( ps_serialinf->deviceType == PV_STYPE_UDPDIR ) ||
			      ( ps_serialinf->deviceType == PV_STYPE_UDPOTH ) ) &&
			    ( ( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_SEND ) ||
			      ( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_READ ) ) )
			{
				pt1 = (unsigned char *)&ps_serialLog->s_logData[loopCnt1].locIPAddr;
				pt2 = (unsigned char *)&ps_serialLog->s_logData[loopCnt1].dstIPAddr;
				fprintf( fp, "(%d.%d.%d.%d:%d)-(%d.%d.%d.%d:%d)",
					*(pt1), *(pt1 + 1), *(pt1 + 2), *(pt1 + 3), ps_serialLog->s_logData[loopCnt1].locPort,
					*(pt2), *(pt2 + 1), *(pt2 + 2), *(pt2 + 3), ps_serialLog->s_logData[loopCnt1].dstPort );

				for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
				{
					if( ( loopCnt2 % 16 ) == 0 )
						fprintf( fp, "\n  %03Xh(%04d): ", loopCnt2, loopCnt2);

					fprintf( fp, "%02X ", (unsigned char)ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}

				fprintf( fp, "\n");
			}
			else
			{
				for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
				{
					fprintf( fp, "%02X ", (unsigned char)ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}
				fprintf( fp, "\n");
			}
		}

		/* ファイルを閉じる */
		fclose( fp );

		/* カウンタをリセット */
		ps_serialLog->dataCnt = 0;
	}

	return( 0 );
}

int commfunc_snmp_logout( int logMode, struct st_serialinf *ps_serialinf, struct st_serialLog *ps_serialLog )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
	char					*pt1;					/* ポインタ */
	char					*pt2;					/* ポインタ */
	int						fd;						/* ファイルディスクリプタ */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */
	struct tm				localTime;				/* 年月日時分秒 */
	char					kindName[6][5] = { "SEND", "RECV", "SETD", "ERRO", "TOUT", "OVER" };

	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_snmp_logout(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* ログ出力を行う必要があるか？ */
	if( logMode == PV_SERIAL_LOG_AUTO )
	{
		/* ログ出力フラグが設定されている場合 */
		if( ps_gpt_PVSYSINF->lineLogFlag[ ps_serialinf->lineNo - 1 ] != 0 )
		{
			/* ログ出力対象に設定 */
			logMode = PV_SERIAL_LOG_ON;
		}
	}

	/* ログ出力が必要な場合 */
	if( logMode == PV_SERIAL_LOG_ON )
	{
		/* 出力用ログファイル名の設定 */
		sprintf( fileName1, "%s%s%02d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );

		/* ファイルが存在しない場合 */
		if( access( fileName1, F_OK ) != 0 )
		{
			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
			{
				return( -1 );
			} 
			close( fd );
		}

		/* システムログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			return( -1 );
		}

		/* ファイルサイズの取得 */
		fileSize = ftell( fp );

		/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
		if(fileSize > 1000000 )
		{ 
			/* ファイルを閉じる */
			fclose( fp );

			/* ログファイルの保存数分繰り返す */
			for(loopCnt1 = PV_LOG_STOCK_NUM ; loopCnt1 > 1 ; loopCnt1-- )	/* [20230902] Chg */
			{
				/* ファイル名の設定 */
				sprintf( fileName1, "%s%s%02d_%d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo, loopCnt1 - 1 );
				sprintf( fileName2, "%s%s%02d_%d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo, loopCnt1 );
				/* 古いファイルの削除 */
				if( access( fileName2, F_OK ) == 0 )
					unlink( fileName2 );
				/* ローテーションの実施 */
				if( access( fileName1, F_OK ) == 0 )
					rename( fileName1, fileName2 );
			}
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s%02d.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );
			sprintf( fileName2, "%s%s%02d_1.log", PV_PATH_LOG, PV_FNAME_SERIAL_LOG, ps_serialinf->lineNo );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			rename( fileName1, fileName2 );

			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH ) ) == -1 )
			{
				return( -1 );
			} 
			close( fd );

			/* システムログファイルをオープン */
			if( (fp = fopen( fileName1, "a" )) == NULL )
			{
				return( -1 );
			}
		}

		/* シリアルログメッセージ出力処理 */
		for( loopCnt1 = 0 ; loopCnt1 < ps_serialLog->dataCnt ; loopCnt1++ )
		{
			/* 年月日時分秒を取得 */
			localtime_r( &ps_serialLog->s_logData[loopCnt1].sec, &localTime );
			fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s[%3d] ",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				ps_serialLog->s_logData[loopCnt1].uSec,
				kindName[ps_serialLog->s_logData[loopCnt1].kind],
				ps_serialLog->s_logData[loopCnt1].dataLen );

			if( ( ( ps_serialinf->deviceType == PV_STYPE_DIRTCP ) ||
			      ( ps_serialinf->deviceType == PV_STYPE_UDPDIR ) ||
			      ( ps_serialinf->deviceType == PV_STYPE_UDPOTH ) ) &&
			    ( ( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_SEND ) ||
			      ( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_READ ) ) )
			{
				pt1 = (char *)&ps_serialLog->s_logData[loopCnt1].locIPAddr;
				pt2 = (char *)&ps_serialLog->s_logData[loopCnt1].dstIPAddr;
				if( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_SEND )
				{
					fprintf( fp, "->%s", ps_serialinf->targetName );
				}
				else if( ps_serialLog->s_logData[loopCnt1].kind == PV_SERIAL_LOG_READ )
				{
					fprintf( fp, "<-%s", ps_serialinf->targetName );
				}
				else
				{
					fprintf( fp, "%s", ps_serialinf->targetName );
				}

				if( ps_serialLog->s_logData[loopCnt1].dataLen > 0 )
				{
					fprintf( fp, "\n%s", ps_serialLog->s_logData[loopCnt1].data);
				}

				fprintf( fp, "\n");
			}
			else
			{
				for( loopCnt2 = 0 ; loopCnt2 < ps_serialLog->s_logData[loopCnt1].dataLen ; loopCnt2++ )
				{
					fprintf( fp, "%02X ", (unsigned char)ps_serialLog->s_logData[loopCnt1].data[loopCnt2] );
				}
				fprintf( fp, "\n");
			}
		}

		/* ファイルを閉じる */
		fclose( fp );

		/* カウンタをリセット */
		ps_serialLog->dataCnt = 0;
	}

	return( 0 );
}

