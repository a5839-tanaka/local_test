#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <math.h>

#define NTP_PACKET_SIZE 48
#define NTP_FPATH_LOG			"/home/nksystem/log/"		/* ログファイルPATH */
#define NTP_FNAME_EMSYSLOG		"ntp_sync"					/* ログファイル */
#define NTP_FNEXT_EMSYSLOG		"log"						/* ログファイル拡張子 */
#define NTP_SEP_FLG_NORMAL		0x00					/* 通常処理 */
#define NTP_SEP_FLG_SPCSKP		0x01					/* 空白、タブ文字はデータとして処理しない */
#define NTP_SEP_FLG_CONTIN		0x80					/* 区切り文字が連続した場合はデータカウントしない */
#define NTP_LOGMSG_SIZE			2048						/* ログメッセージ長 */
#define NTP_LOG_LV1				1						/* ログレベル１ */
#define NTP_LOG_LV2				2						/* ログレベル２ */
#define NTP_LOG_LV3				3						/* ログレベル３ */

static char						logMsg[NTP_LOGMSG_SIZE];				/* ログメッセージ */

struct st_sepStr {												/* 文字列データ分割用構造体 */
	char		data[16][128];									/*  128文字 16個分 */
};

long diff_timeofday( struct timeval * start, struct timeval * end );
int localtime_r_OffsetRef(time_t *sec, struct tm *timeData );
int func_log_msgset( int msgLevel, char *msgStr, char *dtBuff, int dtLength );
int func_sep_data( char *pc_data, char c_delim, int i_flg, struct st_sepStr *ps_sepStr );
long func_get_data_bg( unsigned char *pc_data, int i_byte );
int func_ntp_data_analize_LI( unsigned int packetData );
int func_ntp_data_analize_VN( unsigned int packetData );
int func_ntp_data_analize_Mode( unsigned int packetData );
int func_ntp_data_analize_Stratum( unsigned int packetData );
int func_ntp_data_analize_Poll( unsigned int packetData );
int func_ntp_data_analize_Precision( unsigned int packetData );
int func_ntp_data_analize_Timestamp( long sec_data, long decimal_fraction_data, struct timeval * pt_timeVal );
int chronyc_sources_SyncCheck( void );
int func_ntp_sync( char *ntp_host, char *ntp_port );


// 時刻同期サービス：chronyd
// systemctl status chronyd.service
// systemctl stop chronyd.service
// systemctl start chronyd.service
// timedatectl set-ntp no
// timedatectl set-ntp yes
// ntpdate -b ntp.nkwebserv.jp
// chronyc sources
// chronyc tracking

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期サービス
 *
 * 機能：
 * 		指定された 
 * 			引数(argv[1])：ntpサーバ
 * 			引数(argv[2])：ポート番号
 *		
 * 戻り値：
 * 		  0：正常終了
 * 		≠0：異常終了
 */
int main( int argc, char *argv[])
{
	int				retCode=0;						/* 関数戻り値 */
	int				loopCnt1;						/* ループカウント */
	char			*ptPortNumb;

	sprintf(logMsg, "main ： START");
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	/* 引数チェック */
	if( argc != 3 )
	{
		sprintf(logMsg, "main ： argc != 3(%d)", argc );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
		retCode = -1;
	}
	else
	{
		/* 先頭アドレスを設定 */
		ptPortNumb = argv[2];
		for( loopCnt1 = 0; loopCnt1 < strlen(argv[2]); loopCnt1++ )
		{
			/* 数字の場合 */
			if( ( *( ptPortNumb + loopCnt1 ) >= 0x30 ) &&
				( *( ptPortNumb + loopCnt1 ) <= 0x39 ) )
			{
				;
			}
			else
			{
				sprintf(logMsg, "main ： portNumber Error(%s)", ptPortNumb );
				func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
				retCode = -1;
			}
		}

		/* ポート番号の入力データが正常な場合 */
		if( retCode == 0 )
		{
			sprintf(logMsg, "main ： call func_ntp_sync(%s,%s)", argv[1], argv[2]);
			func_log_msgset( NTP_LOG_LV2, logMsg, NULL, 0 );

			/* 時刻同期関数コール */
			retCode = func_ntp_sync( argv[1], argv[2]);

			sprintf(logMsg, "main ： func_ntp_sync retCode=%d", retCode );
			func_log_msgset( NTP_LOG_LV2, logMsg, NULL, 0 );
		}
	}

	sprintf(logMsg, "main ： END");
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻差分
 *
 * 機能：
 * 		引数 start で指定されたstruct timeval型の時刻と、
 * 		引数 end で指定されたstruct timeval型の時刻の差分を
 *		us単位で返却する。
 * 戻り値：
 * 		時刻差分us単位
 */
long diff_timeofday( struct timeval * start, struct timeval * end )
{
	long delta;

	delta = ( end->tv_sec  - start->tv_sec  ) * 1000000ul
	      + ( end->tv_usec - start->tv_usec );
	return( delta );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 年月日形式変換
 *
 * 機能：
 * 		引数 sec で指定された1900.1.1 0:00からの積算秒を
 * 		西暦に変換し 引数 timeDataへ格納する
 * 戻り値：
 * 		0：正常
 */
int localtime_r_OffsetRef(time_t *sec, struct tm *timeData )
{
	memset(timeData, 0, sizeof(struct tm));

	localtime_r( sec, timeData );

	timeData->tm_year += 1900;
	timeData->tm_mon+= 1;

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ログ出力
 *
 * 機能：
 *		ログファイル（テキストファイル）への出力を行う。
 *		引数 msgStr に指定された文字列をログファイルに出力する。また、引数 dtBuff
 *		に指定されているアドレスから dtLength で指定されるバイト数分をログファイi
 *		ルに16進ダンプ出力する。ただし、dtBuff に NULL が指定されている場合は、
 *		ログファイルへの16進ダンプ出力は行わない。
 *		ログファイルのサイズが 1MByte を超過する場合は、世代管理を行う。
 * 戻り値：
 * 		 0: 正常
 *		-1: 異常
 * 備考：
 *		スレッド動作を考慮し、ログ出力は排他制御（ミューテックスロック）を行う。
 */
int func_log_msgset( int msgLevel, char *msgStr, char *dtBuff, int dtLength )
{
	FILE					*fp;						/* ファイルポインタ */	
	char					fileName1[512];				/* ファイル名 */
	char					fileName2[512];				/* ファイル名 */
	int						fd;							/* ファイルディスクリプタ */
	int						loopCnt1;					/* ループカウンタ */
	long					fileSize;					/* ファイルサイズ */
	time_t					nowTime_s;					/* 現在時刻（積算秒） */
	struct timeval			nowTime;					/* 現在時刻（積算秒） */
	struct tm				localTime;					/* 年月日時分秒 */

	/* ログレベルの確認 */
	if( msgLevel < NTP_MSGLEVEL ) return( 0 );


	/* ログファイル名称 */
	sprintf( fileName1, "%s%s.%s", NTP_FPATH_LOG, NTP_FNAME_EMSYSLOG, NTP_FNEXT_EMSYSLOG );

	/* ファイルが存在しない場合 */
	if( access( fileName1, F_OK ) != 0 )
	{
		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
		{
			printf("open Error.\n");
			return( -1 );
		}
		close( fd );
	}

	/* システムログファイルをオープン */
	if( (fp = fopen( fileName1, "a" )) == NULL )
	{
		printf("fopen Error.\n");
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
		for(loopCnt1 = 10 ; loopCnt1 > 1 ; loopCnt1-- )
		{
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s_%d.%s", NTP_FPATH_LOG, NTP_FNAME_EMSYSLOG, loopCnt1 - 1, NTP_FNEXT_EMSYSLOG );
			sprintf( fileName2, "%s%s_%d.%s", NTP_FPATH_LOG, NTP_FNAME_EMSYSLOG, loopCnt1, NTP_FNEXT_EMSYSLOG );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			if( access( fileName1, F_OK ) == 0 )
				rename( fileName1, fileName2 );
		}
		/* ファイル名の設定 */
		sprintf( fileName1, "%s%s.%s", NTP_FPATH_LOG, NTP_FNAME_EMSYSLOG, NTP_FNEXT_EMSYSLOG );
		sprintf( fileName2, "%s%s_1.%s", NTP_FPATH_LOG, NTP_FNAME_EMSYSLOG, NTP_FNEXT_EMSYSLOG );
		/* 古いファイルの削除 */
		if( access( fileName2, F_OK ) == 0 )
			unlink( fileName2 );
		/* ローテーションの実施 */
		rename( fileName1, fileName2 );

		/* ファイルを生成 */
		if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
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

	/* 年月日時分秒を取得 */
	gettimeofday( &nowTime, NULL );
	nowTime_s = nowTime.tv_sec;
	localtime_r( &nowTime_s, &localTime );
	
	/* ログメッセージを出力 */
	fprintf( fp, "%04d/%02d/%02d %02d:%02d:%02d.%06d %s\n",
		localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
		localTime.tm_hour, localTime.tm_min, localTime.tm_sec, nowTime.tv_usec,
		msgStr );

	/* ダンプデータが指定されている場合 */
	if( ( dtBuff != NULL ) && ( dtLength > 0 ) )
	{
		fprintf( fp, "    0000(00000):");
		for( loopCnt1 = 1 ; loopCnt1 <= dtLength ; loopCnt1++ )
		{
			fprintf( fp, " %02X", (unsigned char)dtBuff[ loopCnt1 - 1 ] );
			if( ( ( loopCnt1 % 16 ) == 0 ) && ( loopCnt1 != dtLength ) )
			{
				fprintf( fp, "\n" );
				fprintf( fp, "    %04X(%05d):", loopCnt1, loopCnt1 );
			}
		}
		fprintf( fp, "\n" );
	}

	/* ファイルを閉じる */
	fclose( fp );

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶ分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列を、引数 c_delim で指定された文字で区切り、
 *		ps_sepStr に格納する。
 * 戻り値：
 * 		引数 c_delim で区切られたデー多数を返す。
 */
int func_sep_data( char *pc_data, char c_delim, int i_flg, struct st_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	/* 分割データ格納領域を初期化 */
	memset( ps_sepStr, 0, sizeof(struct st_sepStr) );

	/* 初期ポインタを設定 */
	pt = pc_data;
	/* 文字列の終わりまで処理を実施する */
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		/* 空白文字とタブはデータとして扱わない場合 */
		if( (i_flg & NTP_SEP_FLG_SPCSKP) && (*pt == ' ') && (c_delim != ' ') ) continue;
		if( (i_flg & NTP_SEP_FLG_SPCSKP) && (*pt == '\t') && (c_delim != '\t') ) continue;

		/* 指定されたデリミタの場合 */
		if( *pt == c_delim )
		{
			/* 連続する区切り文字が終わるまで新たなデータとして処理しない場合 */ 
			if( (i_flg & NTP_SEP_FLG_CONTIN) && ( *(pt + 1) == c_delim ) ) continue;
			ps_sepStr->data[idx1][idx2] = 0;
			idx2 = 0;
			idx1++;
			continue;
		}
		ps_sepStr->data[idx1][idx2] = *pt;
		idx2++;
	}
	ps_sepStr->data[idx1][idx2] = 0;
	if(idx2) idx1++;

	return( idx1 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ビックエンディアンデータ取得
 *
 * 機能：
 * 		引数 pc_data で指定されたポインタから、引数 i_byte で指定されたデータ数を
 *		ビックエンディアンで変換し、unsigned int で返す。
 * 戻り値：
 * 		引数 c_delim で区切られたデー多数を返す。
 */
long func_get_data_bg( unsigned char *pc_data, int i_byte )
{
	long		    bgData=0;
	int				loopCnt1;

	bgData = 0;
	for( loopCnt1 = 0; loopCnt1 < i_byte; loopCnt1++ )
	{
		bgData *= 0x100;
		bgData += pc_data[loopCnt1];
	}
	return bgData;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 LI (Leap Indicator)
 *
 * 機能：
 * 		2 bit。その日の最後の 1 分が 1 秒追加 (うるう秒=Leap Second) されるか、
 *		もしくは 1 秒削除されるかを事前に予告するフィールド。
 *		  0. 予告なし (通常状態)
 *		  1. その日の最後の 1 分が 61 秒となることを予告
 *		  2. その日の最後の 1 分が 59 秒となることを予告
 *		  3. 不明 (時刻同期無し)
 * 戻り値：
 * 		Leap Indicatorを返す。
 */
int func_ntp_data_analize_LI( unsigned int packetData )
{
	unsigned int    targetBit=0;
	int				dataLI;

	targetBit =  packetData >> 30;
	dataLI = targetBit & 0x00000003;

	sprintf(logMsg, "LI (Leap Indicator) = %d", dataLI);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return dataLI;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 VN (Version Number)
 *
 * 機能：
 * 		3 bit。現在の NTP バージョンは 4 (RFC5905 で規定)。
 *		NTP バージョン 4 は、NTP バージョン 3 (RFC1305) および 
 *		SNTP (Simple Network Time Protocol) バージョン 4 (RFC4330) の機能を統合し、
 *		それ以外にもさらに機能を追加したものになります。
 *		また、NTP バージョン 4 は NTP バージョン 1～3 と下位互換があります。
 * 戻り値：
 * 		Version Numberを返す。
 */
int func_ntp_data_analize_VN( unsigned int packetData )
{
	unsigned int    targetBit=0;
	int				dataVN;

	targetBit =  packetData >> 27;
	dataVN = targetBit & 0x00000007;

	sprintf(logMsg, "VN (Version Number) = %d", dataVN);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return dataVN;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 Mode
 *
 * 機能：
 * 		3 bit。アソシエーションのモードを示します。
 *		アソシエーションとは、NTP の時刻情報を提供する形態を示したものです。
 *		アソシエーションモードには以下の値が入ります。
 *		    0. 予約
 *		    1. Symmetric Active
 *		    2. Symmetric Passive
 *		    3. Client
 *		    4. Server
 *		    5. Broadcast
 *		    6. NTP control message (制御クエリ)
 *		    7. プライベート利用に予約
 *		NTP パケットの Mode は、RFC 上は相手のアソシエーションモードを想定して
 *		セットするとあるのですが、Windows, Linux, Cisco どの実装を見ても、
 *		自分のアソシエーションモードをパケットに含めて送信しています。
 *		つまり、自分が Symmetric Active であれば Mode=1 , 
 *		Symmetric Passive であれば Mode=2 , Client であれば Mode=3 , ...となっています。
 * 戻り値：
 * 		Modeを返す。
 */
int func_ntp_data_analize_Mode( unsigned int packetData )
{
	unsigned int    targetBit=0;
	int				dataVN;

	targetBit =  packetData >> 24;
	dataVN = targetBit & 0x00000007;

	sprintf(logMsg, "Mode = %d", dataVN);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return dataVN;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 Stratum
 *
 * 機能：
 * 		8 bit。0 は原子時計や GPS の時刻を意味しますが、NTP パケット上は取り扱いません。
 * 		1 は原子時計や GPS に物理的に直結した NTP サーバを意味します。
 * 		2 以降は、それより 1 つ上の Stratum の NTP サーバから時刻同期したことを意味します。
 * 		16 は時刻同期していないことを意味します。
 * 		17 以降は予約とされ、使われていません。
 * 戻り値：
 * 		Stratumを返す。
 */
int func_ntp_data_analize_Stratum( unsigned int packetData )
{
	unsigned int    targetBit=0;
	int				dataStratum;

	targetBit =  packetData >> 16;
	dataStratum = targetBit & 0x000000FF;

	sprintf(logMsg, "Stratum = %d", dataStratum);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return dataStratum;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 Poll
 *
 * 機能：
 * 		8 bit。次の NTP パケット送出までの最大時間間隔 (秒) に対して 
 * 		log2 の対数を取った数値が入ります。
 * 		例えば 10 (RFC で推奨されているデフォルト値) の場合は 
 * 		2 の 10 乗 = 1024 秒となります。
 * 		この値は NTP で定義されている discipline algorithm の中で決定されます。
 * 		このアルゴリズムは自身とリモート NTP アソシエーションから
 * 		『時刻およびクロック周波数』の誤差を予測するもので、
 * 		この誤差が小さければ Poll は大きめ (つまり間隔が長め) に取るし、
 * 		誤差が大きければ小さめ (間隔が短め) に取ります。
 * 戻り値：
 * 		Pollを返す。
 */
int func_ntp_data_analize_Poll( unsigned int packetData )
{
	unsigned int    targetBit=0;
	int				dataPoll;

	targetBit =  packetData >> 8;
	dataPoll = targetBit & 0x000000FF;

	sprintf(logMsg, "Poll = %d", dataPoll);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return dataPoll;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 Precision
 *
 * 機能：
 * 		8 bit。NTP を扱う機器のシステムクロックが扱える精度を相手に示します。
 * 		Poll と同じく、log2 の対数を取った数値が入ります。
 * 		例えば -18 という数値の場合は約 1 マイクロ秒になります。
 * 戻り値：
 * 		Precisionを返す。
 */
int func_ntp_data_analize_Precision( unsigned int packetData )
{
	unsigned int    targetBit=0;
	int				dataPrecision;

	targetBit =  packetData;
	dataPrecision = targetBit & 0x000000FF;

	sprintf(logMsg, "Precision = %d", dataPrecision);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	return dataPrecision;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ntpデータ 解析 Timestamp
 *
 * 機能：
 * 		nptデータのTimestamp情報をstruct timevalの形式に変換する
 * 戻り値：
 * 		 0: 正常
 */
int func_ntp_data_analize_Timestamp( long sec_data, long decimal_fraction_data, struct timeval * pt_timeVal )
{
    double 			syousu;

	memset(pt_timeVal, 0, sizeof(struct timeval));
	/* 秒を設定 */
	sec_data -= 2208988800UL;
	pt_timeVal->tv_sec = sec_data;

	/* 秒以下を設定 */
	syousu = decimal_fraction_data / pow(2, 32);

	/* usec以下を切り捨て */
	pt_timeVal->tv_usec = (unsigned int)(syousu * 1000000ul);

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期状況チェック
 *
 * 機能：
 * 		chronyc sourcesコマンドにて時刻同期しているサーバを検索する
 *		ps_sepStr に格納する。
 * 戻り値：
 * 		 1: 正常（時刻同期あり）
 * 		 0: 正常（時刻同期なし）
 *		-1: 異常
 */
int chronyc_sources_SyncCheck( void )
{
	FILE				*fp;						/* ファイルポインタ */
	char				cmdStr[1024];				/* コマンド文字列 */
	char				checkData[1024];			/* チェック用文字列 */
	int					sepCount;					/* セパレータデータ数 */
	int					retCode;					/* 戻り値 */
	struct st_sepStr	s_sepStr;					/* 分割データ格納領域 */

	/* 時刻同期状況を取得するためのコマンドの作成 */
	sprintf( cmdStr, "chronyc sources 2> /dev/null");

	/* 時刻同期状況を取得する */
	if( (fp = popen( cmdStr, "r")) == NULL )
	{
		/* エラー */
		/* ログを出力 */
		sprintf(logMsg, "chronyc_sources_SyncCheck(): chronyc sources Error");
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
		return( -1 );
	}
	else
	{
		sprintf(logMsg, "chronyc_sources_SyncCheck(): %s -> OK", cmdStr);
		func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

		/* 出力がなくなるまで繰り返す */
		while( fgets( checkData, 512, fp ) != NULL )
		{
			/* 出力内容をスペースで区切る（空白が連続しても一つの区切りとする） */
			sepCount = func_sep_data( checkData, ' ', NTP_SEP_FLG_CONTIN, &s_sepStr );

			checkData[strlen(checkData)-1]=0;
			sprintf(logMsg, "chronyc_sources_SyncCheck(): %s", checkData);
			func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

			/* 時刻同期しているsourceが見つかった場合 */
			/*  ^：サーバー */
			/*  *：は、chronyd が現在同期しているソースを表す。 */
			if( strcmp( s_sepStr.data[0], "^*" ) == 0 )
			{
				return 1;
			}
		}
	}
	/* ファイルオープン中の場合 */
	if( fp != NULL )
	{
		/* ファイルクローズ */
		fclose( fp );
	}

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期状況チェック
 *
 * 機能：
 * 		chronyc sourcesコマンドにて時刻同期しているサーバを検索する
 *		ps_sepStr に格納する。
 * 戻り値：
 * 		 0: 正常
 *		-1: 異常
 */
int func_ntp_sync( char *ntp_host, char *ntp_port )
{
    int 					sock_fd;
	int						readCnt = 0;				/* データ受信バイト数 */
    unsigned char 			udpSendBuff[NTP_PACKET_SIZE];
    unsigned char 			udpRecvBuff[NTP_PACKET_SIZE];
    unsigned char 			*pt_rcvbuff;
    struct hostent      	*servhost;
    struct sockaddr_in 		sndAddr;
    unsigned int			packet32bitu;					/* 32bit符号なしデータ */
	int						retCode;
	int						loopCnt1;
	int						ntp_LI;
	int						ntp_VN;
	int						ntp_Mode;
	int						ntp_Stratum;
	int						ntp_Poll;
	int						ntp_Precision;
	unsigned int			ntp_RootDelay;
	unsigned int			ntp_RootDispersion;
	unsigned int			ntp_ReferenceID;
	long					decimal_fraction_data;
	long					sec_data;
	struct timeval			timeValRef;
	struct timeval			timeValOrigin;
	struct timeval			timeValRcv;
	struct timeval			timeValTrans;
	struct timeval			snd_timeVal;
	struct timeval			rcv_timeVal;
	struct timeval			now_timeVal;
    long					diffusecEMS;
    long					diffusecNTP;
    long					clientDiff;
    long					clientDiff1;
    long					clientDiff2;
	char					command[256];
	struct tm				timeData;					/* 時刻情報 */
	struct timespec			sleepTm;					/* nanosleep用 */
	
	/* socketオープン */
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
		/* ログを出力 */
		sprintf(logMsg, "func_ntp_sync(): Socket Error RET[ %d ]", sock_fd );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
        return(-1);
    }

	/* ホスト名からIPアドレス情報を取得 */
    if( (servhost = gethostbyname(ntp_host)) == NULL )
    {
		/* ログを出力 */
		sprintf(logMsg, "func_ntp_sync(): ホスト名チェックエラー Hostname:%s", ntp_host);
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
        return(-1);
	}

    // サーバのアドレス情報を設定
    memset(&sndAddr, 0, sizeof(sndAddr));
    sndAddr.sin_family = AF_INET;
    sndAddr.sin_port = htons(atoi(ntp_port));
    memcpy(&sndAddr.sin_addr, servhost->h_addr, servhost->h_length);

    /*  NTPパケットの作成 */
    memset(udpSendBuff, 0, NTP_PACKET_SIZE);
    udpSendBuff[0] = 0b00100011; /* LI=0, VN=3, Mode=3 (client) */

	/* 現在時刻(T1)の取得 */
	gettimeofday(&snd_timeVal, NULL);

	/* データを送信する */
	retCode = sendto( sock_fd,
	                  udpSendBuff,
	                  NTP_PACKET_SIZE,
	                  0,
	                  (struct sockaddr *)&sndAddr,
	                  sizeof( struct sockaddr_in) );

	/* 送信エラーの場合 */
	if( retCode <= 0 )
	{
		/* ログを出力 */
		sprintf(logMsg, "func_ntp_sync(): Lsendto error errno:%d", errno );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );

		/* ポートをクローズ */
		close( sock_fd );

		return (-1);
	}

	/* ポートからデータ読み出し */
    readCnt = recvfrom(sock_fd, udpRecvBuff, NTP_PACKET_SIZE, 0, NULL, NULL);

	if( readCnt < 0 )
	{
		/* ログを出力 */
		sprintf(logMsg, "func_ntp_sync(): recvfrom error errno:%d", errno );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );

		/* ポートをクローズ */
		close( sock_fd );

		return (-1);
	}
	/* ポートをクローズ */
    close(sock_fd);

	/* 現在時刻(T4)の取得 */
	gettimeofday(&rcv_timeVal, NULL);

    /* 受信データの確認 */
    if( readCnt != NTP_PACKET_SIZE) {
		/* ログを出力 */
		sprintf(logMsg, "func_ntp_sync(): recvfrom readCnt(%d) Error", readCnt );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
		/* ポートをクローズ */
		close( sock_fd );

		return (-1);
    }
    // 受信データのログ出力
	sprintf(logMsg, "func_ntp_sync(): recvfrom Data[", errno );

	/* 先頭ポインタを保持 */
	pt_rcvbuff = udpRecvBuff;
	packet32bitu = func_get_data_bg(pt_rcvbuff, 4);
	ntp_LI = func_ntp_data_analize_LI(packet32bitu);
	ntp_VN = func_ntp_data_analize_VN(packet32bitu);
	ntp_Mode = func_ntp_data_analize_Mode(packet32bitu);
	ntp_Stratum = func_ntp_data_analize_Stratum(packet32bitu);
	ntp_Poll = func_ntp_data_analize_Poll(packet32bitu);
	ntp_Precision = func_ntp_data_analize_Precision(packet32bitu);
	pt_rcvbuff += 4;

	/****************
	*  Root Delay
	*   Primary サーバ (Stratum 1 の NTP サーバ) との
	*   ラウンドトリップ遅延 (Round-Trip Delay) を表しています。
	*   NTP サーバは上位 NTP サーバからこの値を受け取ると、
	*   自身の RTT を加え、下位 NTP クライアントへ伝搬します。
	*   この値が大きいほど、ネットワークの距離が遠い=精度が低くなる、
	*   ということを意味します。
	****************/
	ntp_RootDelay = func_get_data_bg(pt_rcvbuff, 4);
	sprintf(logMsg, "Root Delay = %d", ntp_RootDelay);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );
	pt_rcvbuff += 4;

	/****************
	*  Reference ID
	*   Primary サーバ (Stratum 1 の NTP サーバ) との通信における揺らぎ (Jitter) を表しています。
	*   NTP サーバは上位 NTP サーバからこの値を受け取ると、自身の揺らぎ (Jitter) を加え、
	*   下位 NTP クライアントへ伝搬します。
	*   この値が大きいほど、ネットワーク遅延の予測が荒い=精度が低くなる、ということを意味します。
	****************/
	ntp_RootDispersion = func_get_data_bg(pt_rcvbuff, 4);
	sprintf(logMsg, "Root Dispersion = %d", ntp_RootDispersion);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );
	pt_rcvbuff += 4;

	/****************
	*  Reference ID
	*   32 bit。Reference Clock (相手先) がどの NTP サーバを
	*   参照しているかを表しています。
	*   Stratum 2～14 の NTP サーバは、上位の NTP サーバの IP アドレスをセットします。
	*   Stratum 1 の NTP サーバは、参照先を暗示する任意の 1～4 文字の ASCII 文字列をセットします。(GPS、WWVB 等)
	****************/
	ntp_ReferenceID = func_get_data_bg(pt_rcvbuff, 4);
	sprintf(logMsg, "Reference ID = %u", ntp_ReferenceID);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );
	pt_rcvbuff += 4;

	/****************
	*  Reference Timestamp
	*   自分が最後に同期した時刻を示します。
	*   このタイムスタンプは、複数の NTP サーバから情報を取得し平均した結果なので、
	*   時刻同期の計算には使われません。単に相手へのフィードバックとして送っているだけです。
	****************/
	sec_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	decimal_fraction_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	func_ntp_data_analize_Timestamp(sec_data, decimal_fraction_data, &timeValRef);


	/****************
	*  Origin Timestamp
	*   前回相手が NTP パケットを送信した時刻
	*    (前回受信した NTP パケットの Transmit Timestamp) を示します。
	*   時刻同期の計算に使われます。
	****************/
	sec_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	decimal_fraction_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	func_ntp_data_analize_Timestamp(sec_data, decimal_fraction_data, &timeValOrigin);


	/****************
	*  Receive Timestamp
	*   自身が前回相手からNTPパケットを受信した時刻を示します。
	*   時刻同期の計算に使われます。
	****************/
	sec_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	decimal_fraction_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	func_ntp_data_analize_Timestamp(sec_data, decimal_fraction_data, &timeValRcv);


	/****************
	*  Transmit Timestamp
	*   自身が、今回相手へ NTP パケットを送信した時刻を示します。
	*   時刻同期の計算に使われます。
	****************/
	sec_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	decimal_fraction_data = func_get_data_bg(pt_rcvbuff, 4);
	pt_rcvbuff += 4;
	func_ntp_data_analize_Timestamp(sec_data, decimal_fraction_data, &timeValTrans);


	now_timeVal = rcv_timeVal;
	// T1 :snd_timeVal
	// T2 :timeValRcv
	// T3 :timeValTrans
	// T4 :rcv_timeVal
	//   ( ( T3 + T2 ) - (T1 + T4 ) ) / 2
	//   ⇒( ( T3 - T1 ) + (T2 - T4 ) ) / 2
	clientDiff1 = diff_timeofday(&snd_timeVal, &timeValTrans);
	clientDiff2 = diff_timeofday(&rcv_timeVal, &timeValRcv);
	clientDiff = ( clientDiff1 + clientDiff2 ) / 2;

	// 誤差がマイナス（EMS側が進んでいる）の場合
	if( clientDiff < 0 )
	{
		clientDiff *= -1;
		// 秒以下の差分はnanosleep待つ
		if( (clientDiff % 1000000) == 0 )
		{
			/* nanosec単位なので、×1000 を待ち時間に設定 */
			sleepTm.tv_sec = 0;
			sleepTm.tv_nsec = ((clientDiff % 1000000) * 1000);
			nanosleep( &sleepTm, NULL);
		}
		/* 秒単位での時刻設定 */
		now_timeVal.tv_sec -= (unsigned int)(clientDiff / 1000000);

	}
	// 誤差がプラス（EMS側が進んでいる）の場合
	else if( clientDiff > 0 )
	{
		// 秒以下の差分はnanosleep
		if( (clientDiff % 1000000) == 0 )
		{
			// !!!!!!次の秒までスリープ!!!!!!
			/* nanosec単位なので、×1000 を待ち時間に設定 */
			sleepTm.tv_sec = 0;
			sleepTm.tv_nsec = 1000000000 -  ((clientDiff % 1000000) * 1000);
			nanosleep( &sleepTm, NULL);
		}

		/* 1秒切り上げた秒で時刻設定 */
		now_timeVal.tv_sec += (unsigned int)((clientDiff+999999) / 1000000);

	}
	localtime_r_OffsetRef( &now_timeVal.tv_sec, &timeData );
	sprintf(command, "timedatectl set-time \"%04d-%02d-%02d %02d:%02d:%02d\"", timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
	func_log_msgset( NTP_LOG_LV3, command, NULL, 0 );

	/* SYSTEMコマンドにより時刻同期を実施 */
	retCode = system( command );
	/* SYSTEMコマンド異常終了の場合 */
	if( retCode != 0 )
	{
		sprintf(logMsg, "timedatectl set-time Error(%d)\n", retCode );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
	}
	else
	{
		sprintf(logMsg, "timedatectl set-time OK");
		func_log_msgset( NTP_LOG_LV2, logMsg, NULL, 0 );
	}
	gettimeofday(&now_timeVal, NULL);

	/* SYSTEMコマンドによりシステム時刻をRTCに設定 */
	retCode = system( command );
	if( retCode != 0 )
	{
		sprintf(logMsg, "hwclock  --systohc Error(%d)\n", retCode );
		func_log_msgset( NTP_LOG_LV3, logMsg, NULL, 0 );
	}
	else
	{
		sprintf(logMsg, "[hwclock  --systohc] -> OK");
		func_log_msgset( NTP_LOG_LV2, logMsg, NULL, 0 );
	}

	//受信データログ出力
    for( loopCnt1 = 0; loopCnt1 < NTP_PACKET_SIZE; loopCnt1++ )
    {
		sprintf(&logMsg[strlen(logMsg)], "0x%02X ", udpRecvBuff[loopCnt1]);
	}
	sprintf(&logMsg[strlen(logMsg)], "]");
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	localtime_r_OffsetRef( &timeValRef.tv_sec, &timeData );
	sprintf(logMsg, "Reference Timestamp        = %04d/%02d/%02d %02d:%02d:%02d.%06d", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, timeValRef.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	localtime_r_OffsetRef( &timeValOrigin.tv_sec, &timeData );
	sprintf(logMsg, "Origin Timestamp           = %04d/%02d/%02d %02d:%02d:%02d.%06d", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, timeValOrigin.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	localtime_r_OffsetRef( &snd_timeVal.tv_sec, &timeData );
	sprintf(logMsg, "┏━T1 : EMS SendTime      = %04d/%02d/%02d %02d:%02d:%02d.%06d", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, snd_timeVal.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	localtime_r_OffsetRef( &timeValRcv.tv_sec, &timeData );
	sprintf(logMsg, "┃┏T2 : RecieveTimeStamp  = %04d/%02d/%02d %02d:%02d:%02d.%06d", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, timeValRcv.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	localtime_r_OffsetRef( &timeValTrans.tv_sec, &timeData );
	sprintf(logMsg, "┃┗T3 : TransmitTimeStamp = %04d/%02d/%02d %02d:%02d:%02d.%06d", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, timeValTrans.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	localtime_r_OffsetRef( &rcv_timeVal.tv_sec, &timeData );
	sprintf(logMsg, "┗━T4 : EMS RecieveTime   = %04d/%02d/%02d %02d:%02d:%02d.%06d", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, rcv_timeVal.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	diffusecEMS = diff_timeofday( &snd_timeVal, &rcv_timeVal);
	sprintf(logMsg, "diffusecEMS = %d usec", diffusecEMS);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	diffusecNTP = diff_timeofday( &timeValRcv, &timeValTrans);
	sprintf(logMsg, "diffusecNTP = %d usec", diffusecNTP);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	sprintf(logMsg, "clientDiff1(T3-T1)=%d, clientDiff2(T2-T4)=%d usec", clientDiff1, clientDiff2 );
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	sprintf(logMsg, "clientDiff = %d usec", clientDiff );
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

	/* 現在時刻の取得 */
	localtime_r_OffsetRef( &now_timeVal.tv_sec, &timeData );
	sprintf(logMsg, "Now Time = %04d/%02d/%02d %02d:%02d:%02d.%06d\n", 
			timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, now_timeVal.tv_usec);
	func_log_msgset( NTP_LOG_LV1, logMsg, NULL, 0 );

    return 0;

}
