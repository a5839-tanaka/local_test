/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20250110] 2025.01.10
 * ・AWSのKeepAliveの時間を20秒->60秒に変更（RJからの水平展開）
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ミツウロコ
 * -----------------------------------------------------------------------------
 * 
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>	/* for mmap */
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <pwd.h>		/* for getpwuid */
#include <time.h>
#include <fcntl.h>		/* for open */
#include <termios.h>
#include <stdlib.h>		/* for atoi */
#include <unistd.h>		/* for ftruncate */
#include <string.h>		/* for memset, memcpy, strlen, strnlen */
#include <stdio.h>		/* for fopen , fgets */
#include <dirent.h>		/* for scandir */
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvcomm.h"
#include "pvdevdata.h"
#include "pvctldata.h"
#include "pvjson.h"

#define	PV_OWN_NAME		"aws_iot_response"					/* 自プログラム名 */

/********************************/
/* その他                       */
/********************************/
/* ログレベル */
#define	AWSIOT_MSGLEVEL_LOW		1
#define	AWSIOT_MSGLEVEL_MID		2
#define	AWSIOT_MSGLEVEL_HIG		3
#define	AWSIOT_MSGLEVEL_ERR		4
#define	AWSIOT_MSGLEVEL_NONE	5

#define	AWSIOT_MSGLEVEL			AWSIOT_MSGLEVEL_LOW

/* ログ出力 */
#if (AWSIOT_MSGLEVEL <= AWSIOT_MSGLEVEL_LOW)
	#define AWSIOT_MSG_LOW (printf("[LOW]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define AWSIOT_MSG_LOW( msg, ... )
#endif

#if (AWSIOT_MSGLEVEL <= AWSIOT_MSGLEVEL_MID)
	#define AWSIOT_MSG_MID (printf("[MID]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define AWSIOT_MSG_MID( msg, ... )
#endif

#if (AWSIOT_MSGLEVEL <= AWSIOT_MSGLEVEL_HIG)
	#define AWSIOT_MSG_HIG (printf("[HIG]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define AWSIOT_MSG_HIG( msg, ... )
#endif

#if (AWSIOT_MSGLEVEL <= AWSIOT_MSGLEVEL_ERR)
	#define AWSIOT_MSG_ERR (printf("[ERR]%-10s(%4d):%-25s:", __FILE__, __LINE__, __func__), printf)
#else
	#define AWSIOT_MSG_ERR( msg, ... )
#endif


#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_THRED								/* デバッグフラグ：ＳＦＴＰ設定情報表示 */
#define	PV_DEBUG_CDT								/* デバッグフラグ： */
#define PV_DEBUG_TCP
#endif

#define		DEVICE_MAX				100
#define		DEVICE_LIST_FILE		"DEVICE_LIST"
#define		AWS_HOST_ADDRESS		"a3pdolhj0mp40d-ats.iot.ap-northeast-1.amazonaws.com"
#define		AWS_PORT_NUMBER			443



struct st_iot_ems
{
    int   	jisso;        /* 実装 */
	char	deviceId[32];					/* デバイスID */
	char	rcvTopicName[128];				/* 受信トピック名 */
	int		notSend;						/* */
	AWS_IoT_Client 			rcv_client;		/* クライアント情報 */
	struct st_pvJsonParse	rcv_pvJsonparse;				/* 受信解析情報 */
	struct st_ORDERPARA		s_orderpara[AWS_ORDER_STOCK];	/* Web-APIパラメータ *//* 0：現在、1：次回 */
	struct st_evtdata		s_evtdata;						/* 制御データ */
	int						resOrder[AWS_NOTIFY_STOCK];		/* 応答送信順序（Queue構造がよかったが、EMSを停止するとキューポインタが壊れてしまい順序が崩れる為、送信順序で対応） */
	struct st_resinfo		s_resinfo[AWS_NOTIFY_STOCK];	/* 応答情報 */
	struct st_setdata		s_setdata;						/* 制御データ */
} s_iot_ems[DEVICE_MAX];

struct st_threadArg {
	pthread_t			threadID;							/* スレッドID */
	int					indexNumb;					/* 処理中インデックス番号 */
	int					stFlg;						/* スレッド開始フラグ */
	int					edFlg;						/* スレッド終了フラグ */
};

static struct st_threadArg	s_threadArg[DEVICE_MAX];			/* スレッド管理情報 */
static char					pv_own_name[128];			/* 自プロセス情報 */
static char					errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int					threadStop = 0;					/* スレッドストップフラグ */
time_t						devFileReadTime=0;				/* ファイル更新時刻 */


int deviceListFileRead( time_t * );
void *func_aws_iot_rcv_thread( void *args );
int	func_aws_rcv_init( struct st_iot_ems * );
int	func_aws_rcv_quit( struct st_iot_ems * );
static void func_subscribe_callback_handler_rcv(AWS_IoT_Client *, char *, uint16_t , IoT_Publish_Message_Params *, void *);
void func_disconnect_handler(AWS_IoT_Client *, void *);
int func_subscribe_order_info_der( int, struct st_pvJsonParse *, char *, struct st_ORDERPARA  *);
int func_subscribe( int, struct st_pvJsonParse *, int, char * );
int func_order_check( int );
int func_order_update( int );
int commfunc_json_logout_local( char *, char * );
int	func_cmn_setNotify( int, struct st_resinfo * );
int	func_cmn_publisResCheck( int, char *, int  );
int	func_cmn_publishNotify( int );
void func_order_info_set( int, struct st_ORDERPARA * );
int pvctl_cmn_get_process_tim( struct timeval *, struct tm * );
long long func_pow10( int );
int  csv_split( char *, char **, int  );


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * AWS IoT 自動応答ツール
 *
 */
int main(int argc, char *argv[])
{
	int						procNo;						/* プログラム番号 */
	int						retCode;					/* 関数戻り値格納用 */
	int						stopFlag = 0;				/* 停止通知受信済みフラグ */
	time_t 					u_time;
	struct tm 				timeData;
	int						ntpFlg = 0;					/* 初期起動フラグ */
	int						reqType;
/* [20200814] Add St */
	int						loopCnt1;						/* ループカウンタ */
	int						loopCnt2;						/* ループカウンタ */
	int						threadJoinCnt;					/* スレッド終了カウント */
	int						devCnt;						/* デバイス数 */

	AWSIOT_MSG_HIG("START\n");

	/* スレッド管理情報を初期化 */
	memset( s_threadArg, 0, sizeof(s_threadArg));

	/* 自プロセス情報を設定 */
	sprintf( pv_own_name, "%s", PV_OWN_NAME );

	/* 初期化 */
	memset(s_iot_ems, 0, sizeof(s_iot_ems));

	/* デバイスリスト読み込み */
	devCnt = deviceListFileRead(&devFileReadTime);

	/* 異常だった場合 */
	if( devCnt < 0 )
	{
		AWSIOT_MSG_ERR("devCnt=%d\n", devCnt);
		return -1;
	}

	/* デバイスリスト数分ループ */
	for( loopCnt1=0; loopCnt1 < DEVICE_MAX; loopCnt1++ )
	{
		/* 送信順序を設定する（初期時は、添え字順） */
		for( loopCnt2 = 0; loopCnt2 < AWS_NOTIFY_STOCK; loopCnt2++ )
		{
			s_iot_ems[loopCnt1].resOrder[loopCnt2] = loopCnt2;
			s_iot_ems[loopCnt1].s_resinfo[loopCnt2].resId = loopCnt2 + 1;
		}

		s_threadArg[loopCnt1].indexNumb = loopCnt1;
		s_threadArg[loopCnt1].stFlg = 1;

#ifdef PV_DEBUG_THRED
		printf("%s : Index[%02d] OPEN-Thread EXECUTE\n", pv_own_name, s_threadArg[loopCnt1].indexNumb );
#endif
		/* スレッドを起動 */
		if( pthread_create( &s_threadArg[loopCnt1].threadID, NULL, func_aws_iot_rcv_thread, &s_threadArg[loopCnt1] ) != 0 )
		{

#ifdef PV_DEBUG_THRED
			printf("%s : Index[%02d] OPEN-Thread EXECUTE Error\n", pv_own_name, s_threadArg[loopCnt1].indexNumb );
#endif
			sprintf( errMsg, "Thread Execute Error" );
			AWSIOT_MSG_ERR("%s\n", errMsg );

			/* プロセス異常終了処理 */
			return( -1 );
		}
	}


	while( 1 )
	{
		/* 停止通知受信済みフラグリセット */
		stopFlag = 0;

		sleep(1);

		/* デバイスリスト更新チェック */
		deviceListFileRead(&devFileReadTime);

		/* デバイスリスト数分ループ */
		for( loopCnt1=0; loopCnt1 < DEVICE_MAX; loopCnt1++ )
		{
			if( s_iot_ems[loopCnt1].deviceId[0] != 0 )
			{
				/* 指示情報更新 */
				func_order_update( loopCnt1 );

				/* IoT制御チェック */
				func_order_check( loopCnt1 );
			}
		}
		/* 指定周期になるまで待機する */
		//if( commfunc_wait( 1, 0 ) < 0 )
		//{
			/* 停止通知受信済みフラグ設定 */
		//	stopFlag = 1;
		//	break;
		//}
	}

	/* スレッドの終了を待つ（4秒ｘ5回） */
	threadStop = 1;
	for(loopCnt2 = 0 ; loopCnt2 < 5; loopCnt2++)
	{
		threadJoinCnt = 0;
		/* 当該通信回線に所属するマルチメータの台数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < devCnt ; loopCnt1++ )
		{
			/* スレッドが終了していない場合 */
			if( s_threadArg[loopCnt1].threadID != 0 )
			{
				/* スレッドの終了を待つ */
				if( pthread_join( s_threadArg[loopCnt1].threadID, NULL ) == 0 )
				{
					s_threadArg[loopCnt1].threadID = 0;
					threadJoinCnt++;
				}
			}
			else
			{
				threadJoinCnt++;
			}
		}
		/* 全てのスレッドの終了を回収した場合 */
		if( threadJoinCnt == devCnt )
		{
			break;
		}
		sleep(4);
	}

	/* スレッドが終了していない場合 */
	if( threadJoinCnt != devCnt )
	{
		/* 強制終了を行う */
		/* 当該通信回線に所属するマルチメータの台数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < devCnt ; loopCnt1++ )
		{
			/* スレッドが終了していない場合 */
			if( s_threadArg[loopCnt1].threadID != 0 )
			{
				sprintf( errMsg, "pthread_detach (tid=%d)", s_threadArg[loopCnt1].threadID	 );
				/* スレッドのリソースをデタッチする */
				retCode = pthread_detach( s_threadArg[loopCnt1].threadID );
				if( retCode != 0 )
				{
					sprintf( errMsg, "pthread_detach Error(%d)", retCode );
					AWSIOT_MSG_ERR("%s\n", errMsg );
				}
			}
		}
	}

	printf("main : END\n");

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * デバイスリスト読み込み
 *
 */
int deviceListFileRead( time_t *checkTime )
{
	FILE					*fp;							/* ファイルディスクリプタ */
	struct stat				statBuff;						/* ファイル状態 */
	char					filePath[256];
	char					readBuff[1024];					/* リードバッファ */
	int						devCnt=0;
	int						loopCnt1;
	int						index=0;
    int            			l_lcnt;
    char					*pc_lcsvdat[1024];


	sprintf(filePath, "./%s", DEVICE_LIST_FILE );

	/* 設定ファイルが存在しない場合 */
	if( access( filePath, R_OK ) != 0 )
	{
		sprintf( errMsg, "file is no exist. [%s]", filePath );
		AWSIOT_MSG_ERR("%s\n", errMsg );
		return -1;
	}

	/* ファイル状態の取得に失敗 */
	if( stat( filePath, &statBuff ) != 0 )
	{
		sprintf( errMsg, "Configuration File Access Error!! [%s]", filePath );
		AWSIOT_MSG_ERR("%s\n", errMsg );
		return -1;
	}

	/* チェック時刻が設定されている場合 */
	if( checkTime != NULL )
	{
		/* ファイルが変更されていない場合 */
		if( *checkTime == statBuff.st_mtime )
		{
			return 0;
		}
	}

	/* デバイスリスト */
	if( (fp = fopen(filePath, "r")) != NULL)
	{
		/* 設定ファイルの終端まで繰り返す */
		while( fgets( readBuff, sizeof(readBuff), fp) != NULL )
		{
			/* 送信不可設定をON */
			s_iot_ems[index].notSend = 1;

			if( readBuff[0] != '#' )
			{

	            /* ＣＳＶ分割 */
	            l_lcnt = csv_split(readBuff, pc_lcsvdat, 1024);

	            if(l_lcnt <= 0)
	                continue;

				if( strlen(pc_lcsvdat[0]) < 8 )
	                continue;

				/* 実装フラグをON */
				s_iot_ems[index].jisso = 1;

				/* デバイスIDを保持 */
				memcpy(s_iot_ems[index].deviceId, pc_lcsvdat[0], strlen(pc_lcsvdat[0]));

				/* トピック名を生成 */
				sprintf(s_iot_ems[index].rcvTopicName, "MGE/%s/order", s_iot_ems[index].deviceId );

				/* 送信不可設定をOFF */
				s_iot_ems[index].notSend = 0;

				sprintf(errMsg, "[%d] device_id=[%s], topic=[%s]", index, s_iot_ems[index].deviceId, s_iot_ems[index].rcvTopicName);
				AWSIOT_MSG_HIG("%s\n", errMsg );

				devCnt++;
			}
			index++;

			if( DEVICE_MAX == index )
			{
				sprintf(errMsg, "st_iot_ems resource(%d) Over.", DEVICE_MAX );
				AWSIOT_MSG_ERR("%s\n", errMsg );
				break;
			}
		}
		/* ファイル時刻を更新 */
		*checkTime = statBuff.st_mtime;

		/* ファイルをクローズ */
		fclose(fp);
	}
	else
	{
		sprintf(errMsg, "File Open Error. File:%s", filePath );
		AWSIOT_MSG_ERR("%s\n", errMsg );
		return -1;
	}

	AWSIOT_MSG_LOW("END(%d)\n", devCnt);

	return devCnt;
}

void *func_aws_iot_rcv_thread( void *args )
{
	struct st_threadArg		*ps_threadArg;				/* スレッド引数処理用 */
	int							aws_init=-1;
	IoT_Error_t 				rc = FAILURE;
	IoT_Publish_Message_Params 	paramsQOS;
	struct tm					timeData;					/* 時刻情報 */
	struct timeval				nowTime;					/* 現在時刻(積算秒) */
	char 						cPayload[512];
	int							index;
	char 						errMsg[512];

	ps_threadArg = (struct st_threadArg *)args;

	/* 処理インデックス番号の取得 */
	index = ps_threadArg->indexNumb;

	if( s_iot_ems[index].deviceId[0] != 0 )
	{
		sprintf(errMsg, "Thread START [%d] : device_id=[%s]", index, s_iot_ems[index].deviceId );
		AWSIOT_MSG_HIG("%s\n", errMsg );
	}

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( threadStop == 0 )
	{
		/* スレッドの動作周期を1秒に変更 */
		commfunc_mswait2(1000);

		/* デバイスIDが設定済みの場合 */
		if( s_iot_ems[index].deviceId[0] != 0 )
		{
			if( aws_init != 0 )
			{
				/* AWS 初期化処理 */
				aws_init = func_aws_rcv_init( &s_iot_ems[index]  );
				if( aws_init == 0 )
				{
				}
				else
				{
					continue;
				}
			}

			if( aws_init == 0 )
			{
				rc = aws_iot_mqtt_yield(&s_iot_ems[index].rcv_client, 500);

				/* 切断された場合 */
				if( ( NETWORK_ATTEMPTING_RECONNECT == rc) ||
				    ( NETWORK_MANUALLY_DISCONNECTED == rc ) )
				{
					/* IOT通信異常(受信)：発生 */
					// printf？
					continue;
				}
				else if ( rc >= SUCCESS)
				{
					/* 正常の場合 */
					if( rc == SUCCESS )
					{
					}
				}
				else
				{

					AWSIOT_MSG_ERR("func_iot_rcv_thread ; aws_iot_mqtt_yield is Error(%d)\n", rc);
					/* 装置状態状変：IOT通信異常(受信)：発生 */
					// printf？

					/* 再接続以外の以上の場合は、一度終了する */
					func_aws_rcv_quit( &s_iot_ems[index] );
					aws_init=-1;
					continue;
				}

				/* 通知情報を送信 */
				aws_init = func_cmn_publishNotify( index );
			}
		}
	}

	/* AWS 終了処理 */
	if( aws_init == 0 )
	{
		func_aws_rcv_quit( &s_iot_ems[index] );
	}

#ifdef PV_DEBUG
	printf("%s : Thread END\n", pv_own_name);
#endif

	sprintf(errMsg, "Thread END [%d] : device_id=[%s]", index, s_iot_ems[index].deviceId );
	AWSIOT_MSG_HIG("%s\n", errMsg );

	return( NULL );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * AWS-IoT初期化処理
 *
 * 機能：
 * 		AWS-IoTの初期化処理を行う。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int func_aws_rcv_init( struct st_iot_ems *ps_iot_ems )
{
	char			rootCA[PATH_MAX + 1];
	char			clientCRT[PATH_MAX + 1];
	char			clientKey[PATH_MAX + 1];
	char			rcv_clientid[PATH_MAX + 1];
	char			rcvTopicName[256];			/* トピック名 */

	IoT_Error_t rc = FAILURE;

	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	sprintf(rootCA, "%s%s", PV_PATH_CERT, PV_AWS_IOT_ROOT_CA_FILENAME);
	sprintf(clientCRT, "%s%s", PV_PATH_CERT, PV_AWS_IOT_CERTIFICATE_FILENAME);
	sprintf(clientKey, "%s%s", PV_PATH_CERT, PV_AWS_IOT_PRIVATE_KEY_FILENAME);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = AWS_HOST_ADDRESS;
	mqttInitParams.port = AWS_PORT_NUMBER;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 10000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = func_disconnect_handler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&ps_iot_ems->rcv_client, &mqttInitParams);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_init returned error : %d ", rc);
		AWSIOT_MSG_ERR("%s\n", errMsg );
		return -1;
	}

	connectParams.keepAliveIntervalInSec = 60;		/* [20250110] Chg */
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	sprintf(rcv_clientid, "%s-rcv", ps_iot_ems->deviceId);
	connectParams.pClientID = rcv_clientid;
	connectParams.clientIDLen = (uint16_t) strlen(rcv_clientid);
	connectParams.isWillMsgPresent = false;

	rc = aws_iot_mqtt_connect(&ps_iot_ems->rcv_client, &connectParams);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_connect : device_id=[%s], Error(%d)", ps_iot_ems->deviceId, rc);
		AWSIOT_MSG_ERR("%s\n", errMsg );
		rc = aws_iot_mqtt_free( &ps_iot_ems->rcv_client );
		return -1;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&ps_iot_ems->rcv_client, true);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_autoreconnect_set_status : device_id=[%s], Unable to set Auto Reconnect to true - %d", ps_iot_ems->deviceId, rc);
		AWSIOT_MSG_ERR("%s\n", errMsg );
		rc = aws_iot_mqtt_disconnect( &ps_iot_ems->rcv_client );
		rc = aws_iot_mqtt_free( &ps_iot_ems->rcv_client );
		return -1;
	}

	sprintf(rcvTopicName, "%s", ps_iot_ems->rcvTopicName);
	rc = aws_iot_mqtt_subscribe(&ps_iot_ems->rcv_client, rcvTopicName, strlen(rcvTopicName), QOS0, func_subscribe_callback_handler_rcv, NULL);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_subscribe : topic[%s] Error subscribing : %d", rcvTopicName, rc);
		AWSIOT_MSG_ERR("%s\n", errMsg );
		rc = aws_iot_mqtt_disconnect( &ps_iot_ems->rcv_client );
		rc = aws_iot_mqtt_free( &ps_iot_ems->rcv_client );
		return( -1 );
	}

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * AWS-IoT終了処理
 *
 * 機能：
 * 		AWS-IoTの終了処理を行う。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int func_aws_rcv_quit( struct st_iot_ems *ps_iot_ems )
{
	IoT_Error_t rc = FAILURE;

	commfunc_json_free( &ps_iot_ems->rcv_pvJsonparse );

	rc = aws_iot_mqtt_disconnect( &ps_iot_ems->rcv_client );
	if( ( SUCCESS != rc) && ( NETWORK_DISCONNECTED_ERROR != rc) ) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_disconnect : Unable to set Auto Reconnect to true - %d", rc);
		AWSIOT_MSG_ERR("%s\n", errMsg );
	}

	rc = aws_iot_mqtt_free( &ps_iot_ems->rcv_client );
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_free : Unable to set Auto Reconnect to true - %d", rc);
		AWSIOT_MSG_ERR("%s\n", errMsg );
		return -1;
	}

	return 0;
}

int	funcSearchDevList( char *topicName )
{
	char		errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	int			loopCnt1;

	for(loopCnt1 = 0; loopCnt1 < DEVICE_MAX; loopCnt1 ++ )
	{
		if( strcmp(s_iot_ems[loopCnt1].rcvTopicName, topicName) == 0 )
		{
			return loopCnt1;
		}
	}

	AWSIOT_MSG_ERR(errmsg, "%s is not Found\n", topicName );
	
	return -1;
}

static void func_subscribe_callback_handler_rcv(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen, IoT_Publish_Message_Params *params, void *pData)
{
	int			retCode;
	char					errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	char					logFile[128];				/* AWS受信ログファイル名 */
	char					logOldFile[128];			/* AWS受信ログファイル名 */
	FILE					*fp;						/* AWS受信ログファイルポインタ */
	time_t					nowTime;					/* 現在時刻 */
	struct tm				dayTime;					/* 日付情報 */
	int						fileSize;					/* ファイルサイズ */
	char					*p_payload;					/* ペイロードポインタ */
	int						index;
	struct st_pvJsonParse	*ps_pvJsonParse;
	char					fileName[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	char					topicNameStr[128];

	//IOT_UNUSED(pData);
	//IOT_UNUSED(pClient);
	//printf("Subscribe callback\n");
	//printf("%.*s\t%.*s\n", topicNameLen, topicName, (int) params->payloadLen, (char *) params->payload);
	//sprintf(errmsg, "topick(%d)=[%s]", topicNameLen, topicName);
	//AWSIOT_MSG_HIG("%s\n", errmsg );
	//sprintf(errmsg, "paylod(%d)=[%s]", (int) params->payloadLen, (char *) params->payload);
	//AWSIOT_MSG_HIG("%s\n", errmsg );
	memset(topicNameStr, 0, sizeof(topicNameStr));
	memcpy(topicNameStr, topicName, topicNameLen);


	index = funcSearchDevList(topicNameStr);

	if( index < 0 )
	{
		return;
	}
	ps_pvJsonParse = &s_iot_ems[index].rcv_pvJsonparse;

	/* 終端を設定 */
	p_payload = (char *)params->payload;
	p_payload[params->payloadLen] = 0;
	AWSIOT_MSG_LOW("payload=[%s]\n", p_payload );

	/* 受信ログ出力 */
	sprintf(fileName, "%s_%s", PV_FNAME_AWSRCV_LOG, s_iot_ems[index].deviceId);
	commfunc_json_logout_local(fileName, (char *)params->payload);

	/* 受信データ解析 */
	retCode = commfunc_json_parse(ps_pvJsonParse, 0, (int)params->payloadLen, (char *) params->payload, errmsg );

	/* 処理結果 */
	if( retCode != PV_JSON_RET_OK )
	{
		commfunc_json_free( ps_pvJsonParse );
		return;
	}

	/* 受信データ取得 */
	retCode = func_subscribe( index, ps_pvJsonParse, retCode, errmsg );


	commfunc_json_free( ps_pvJsonParse );

	return;
}

void func_disconnect_handler(AWS_IoT_Client *pClient, void *pData)
{

	AWSIOT_MSG_HIG("func_disconnect_handler is called\n");

	return;
}

int func_subscribe_order_info_der( int index, struct st_pvJsonParse *ps_jsonParse, char *pc_errMsg, struct st_ORDERPARA  *ps_orderpara)
{
	char						tag[PV_JSON_DATALEN_MAX];
	int							retCode = 0;
	int							loopCnt1;
	struct timeval				intervalStart;
	char						errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	bool						durationFound;
	bool						valueFound;
	bool						baseFound;
	int							valueCount;
	struct st_resinfo			resInfo;
	struct timeval				nowTime;
	struct st_jsonParseRsult	jsonParseRsult;


	memset( &resInfo, 0, sizeof(resInfo));

	/* 初期値設定 */
	sprintf( ps_orderpara->timeUnit, "min" );
	ps_orderpara->timeConv = 60;
	sprintf( ps_orderpara->valueUnit, "kW" );
	ps_orderpara->valueConv = func_pow10(3);
	
	//*******************
	// jsonデータ取得
	//*******************

	/* deviceID取得 */
	sprintf(tag, "device_id");
	retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
	if( retCode != PV_JSON_RET_OK )
	{
		sprintf(pc_errMsg, "[%s] is not found.", tag);
		AWSIOT_MSG_HIG("%s\n", pc_errMsg );
		retCode = PV_JSON_ERR_FORMAT;
	}
	else
	{
		/* デバイスIDが不一致の場合 */
		if( strcmp( s_iot_ems[index].deviceId, jsonParseRsult.value ) != 0 )
		{
			sprintf(pc_errMsg, "[%s] does not match (%s!=%s).", tag, s_iot_ems[index].deviceId, jsonParseRsult.value);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			retCode = PV_JSON_ERR_PARAM;
		}
		memcpy( ps_orderpara->deviceId, jsonParseRsult.value, sizeof( ps_orderpara->deviceId ));
	}

/* [20240221] Add Ed */
	if( retCode == PV_JSON_RET_OK )
	{
		/* 商品区分取得 */
		sprintf(tag, "category");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
		}
		else
		{
			/* 商品区分のチェック */
			if( ( strcmp( jsonParseRsult.value, AWS_CATEGORY_REPLACEMENT_RESERVE_FOR_FIT ) == 0 ) ||
				( strcmp( jsonParseRsult.value, AWS_CATEGORY_REPLACEMENT_RESERVE		 ) == 0 ) ||
				( strcmp( jsonParseRsult.value, AWS_CATEGORY_FREQUENCY_CONTAINMENT		 ) == 0 ) ||
				( strcmp( jsonParseRsult.value, AWS_CATEGORY_SYNC_FREQUENCY_RESTORATION	 ) == 0 ) ||
				( strcmp( jsonParseRsult.value, AWS_CATEGORY_FREQUENCY_RESTORATION		 ) == 0 ) )
			{
				ps_orderpara->category = AWS_CATEGORY_RR;
			}
			else if( strcmp( jsonParseRsult.value, AWS_CATEGORY_CAPACITYMARKET ) == 0 )
			{
				ps_orderpara->category = AWS_CATEGORY_CM;
			}
			else
			{
				sprintf(pc_errMsg, "[%s] is not supported.", tag);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			}
			memcpy( ps_orderpara->categoryStr, jsonParseRsult.value, sizeof( ps_orderpara->categoryStr ));
		}

		retCode = PV_JSON_RET_OK;
	}
/* [20240221] Add Ed */

	if( retCode == PV_JSON_RET_OK )
	{
		/* timestamp取得 */
		sprintf(tag, "timestamp");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
		}
		/* タイムスタンプは異常でも制御には影響ないので正常として扱う */
		retCode = PV_JSON_RET_OK;
		memcpy( ps_orderpara->timeStamp, jsonParseRsult.value, sizeof( ps_orderpara->timeStamp ));
	}



	if( retCode == PV_JSON_RET_OK )
	{
		/* duration取得 */
		sprintf(tag, "duration");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
		}
		else
		{
			memcpy( ps_orderpara->duration, jsonParseRsult.value, sizeof( ps_orderpara->duration ));
		}
		
		/* durationは異常でも制御には影響ないので正常として扱う */
		retCode = PV_JSON_RET_OK;
	}

	if( retCode == PV_JSON_RET_OK )
	{
		/* date取得 */
		sprintf(tag, "date");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			retCode = PV_JSON_ERR_FORMAT;
		}
		else
		{
			memcpy( ps_orderpara->eventTimeStr, jsonParseRsult.value, sizeof( ps_orderpara->eventTimeStr ));

			/* 時刻文字列変換 */
			retCode = func_date_format_check( ps_orderpara->eventTimeStr );
			if( retCode != PV_JSON_RET_OK )
			{
				sprintf(pc_errMsg, "date[%s] is format Error.", ps_orderpara->eventTimeStr);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				retCode = PV_JSON_ERR_PARAM;
			}
			else
			{
			retCode = func_json_conv_strTotimeval( ps_orderpara->eventTimeStr, &ps_orderpara->eventStTimeSec );

				if( retCode != PV_JSON_RET_OK )
				{
					sprintf(pc_errMsg, "date[%s] is format Error.", ps_orderpara->eventTimeStr);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
				}
			}
		}
	}

	if( retCode == PV_JSON_RET_OK )
	{
		/* time_unit取得 */
		sprintf(tag, "time_unit");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			/* time_unit情報は任意なので、見つからなくても正常として扱う */
			retCode = PV_JSON_RET_OK;
		}
		else
		{
			/* 文字列を保持 */
			memcpy( ps_orderpara->timeUnit, jsonParseRsult.value, sizeof( ps_orderpara->timeUnit ));

			if( strcmp( "min", jsonParseRsult.value ) == 0 )
			{
				ps_orderpara->timeConv = 60;
			}
			else if( strcmp( "sec", jsonParseRsult.value ) == 0 )
			{
				ps_orderpara->timeConv = 1;
			}
			else
			{
				/* time_unit情報があるけど、該当しない場合は異常として扱う */
				sprintf(pc_errMsg, "[%s(%s)] is not supported.", tag, jsonParseRsult.value);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				retCode = PV_JSON_ERR_UNSUPPORTED;
			}
		}
	}

	if( retCode == PV_JSON_RET_OK )
	{
		/* value_unit取得 */
		sprintf(tag, "value_unit");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			retCode = PV_JSON_ERR_FORMAT;
			/* value_unit情報は任意なので、見つからなくても正常として扱う */
			retCode = PV_JSON_RET_OK;
		}
		else
		{
			/* 文字列を保持 */
			memcpy( ps_orderpara->valueUnit, jsonParseRsult.value, sizeof( ps_orderpara->valueUnit ));

			if( strcmp( "kW", jsonParseRsult.value ) == 0 )
			{
				ps_orderpara->valueConv = func_pow10(3);
			}
			else if( strcmp( "MW", jsonParseRsult.value ) == 0 )
			{
				ps_orderpara->valueConv = func_pow10(6);
			}
			else if( strcmp( "W", jsonParseRsult.value ) == 0 )
			{
				ps_orderpara->valueConv = func_pow10(0);
			}
			else
			{
				/* value_unit情報があるけど、該当しない場合は異常として扱う */
				sprintf(pc_errMsg, "[%s(%s)] is not supported.", tag, jsonParseRsult.value);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				retCode = PV_JSON_ERR_UNSUPPORTED;
			}
		}
	}

	if( retCode == PV_JSON_RET_OK )
	{
		/* Interval情報を取得 */
		for( loopCnt1 = 0; loopCnt1 < AWS_INTERVAL_MAX; loopCnt1++ )
		{
			durationFound = false;
			valueFound = false;
			baseFound = false;
			
			sprintf(tag, "interval.interval%d.duration", loopCnt1 );
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode == PV_JSON_RET_OK )
			{
				func_numb_conv( jsonParseRsult.value, &ps_orderpara->s_interval[loopCnt1].duration);

				/* 時間が負の数 */
				if( ps_orderpara->s_interval[loopCnt1].duration.tv_sec < 0 )
				{
					sprintf(pc_errMsg, "[%s] is less than 0(%s).", tag, jsonParseRsult.value );
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
/* [20201224] Add St */
				else if( ps_orderpara->s_interval[loopCnt1].duration.tv_sec > (1440 * 60 / ps_orderpara->timeConv) )	/* [20211207] Chg */
				{
					sprintf(pc_errMsg, "[%s] is more than %d(%s).", tag, (1440 * 60 / ps_orderpara->timeConv), jsonParseRsult.value );	/* [20211207] Chg */
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
/* [20201224] Add Ed */

				retCode = commfunc_json_getValue2( ps_jsonParse, (jsonParseRsult.dataPos+1), 0, tag, &jsonParseRsult );
				/* 該当するタグが複数あった場合 */
				if( retCode == PV_JSON_RET_OK )
				{
					sprintf(pc_errMsg, "[%s] is already implemented.", tag );
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
				durationFound = true;
			}

			sprintf(tag, "interval.interval%d.value", loopCnt1 );
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode == PV_JSON_RET_OK )
			{
				/* 該当するタグが複数あった場合 */
				if( valueCount > 1 )
				{
					sprintf(pc_errMsg, "[%s] is already implemented.", tag );
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
				/* 三次調整力 以外の場合は、負の数はNG */
				if(( ps_orderpara->orderType != AWS_ORDER_TYPE_AC))
				{
					if( atoi(jsonParseRsult.value) < 0 )
					{
						sprintf(pc_errMsg, "[%s] is less than 0(%s).", tag, jsonParseRsult.value );
						AWSIOT_MSG_HIG("%s\n", pc_errMsg );
						retCode = PV_JSON_ERR_PARAM;
						break;
					}
				}

				valueFound = true;
				ps_orderpara->s_interval[loopCnt1].value = atoi(jsonParseRsult.value);
			}

			sprintf(tag, "interval.interval%d.base", loopCnt1 );
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode == PV_JSON_RET_OK )
			{
				/* 該当するタグが複数あった場合 */
				if( valueCount > 1 )
				{
					sprintf(pc_errMsg, "[%s] is already implemented.", tag );
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}

				/* 三次調整力 以外の場合は、負の数はNG */
				if(( ps_orderpara->orderType != AWS_ORDER_TYPE_AC))
				{
					if( atoi(jsonParseRsult.value) < 0 )
					{
						sprintf(pc_errMsg, "[%s] is less than 0(%s).", tag, jsonParseRsult.value );
						AWSIOT_MSG_HIG("%s\n", pc_errMsg );
						retCode = PV_JSON_ERR_PARAM;
						break;
					}
				}
#if 0
				/* 手動制御の場合は、0固定 */
				else if( ps_orderpara->orderType == AWS_ORDER_TYPE_POWER )
				{
					if( atoi(jsonParseRsult.value) != 0 )
					{
						sprintf(pc_errMsg, "[%s] is not 0(%s).", tag, jsonParseRsult.value );
						AWSIOT_MSG_HIG("%s\n", pc_errMsg );
						retCode = PV_JSON_ERR_PARAM;
						break;
					}
				}
#endif
				baseFound = true;
				ps_orderpara->s_interval[loopCnt1].base = atoi(jsonParseRsult.value);
			}

			/* すべてある場合 */
			if( ( true == durationFound ) &&
				( true == baseFound ) &&
				( true == valueFound ))
			{
				/* 実装にする */
				ps_orderpara->s_interval[loopCnt1].statFlag = 1;
			}
			/* すべてない場合 */
			else if( ( false == durationFound ) &&
					 ( false == baseFound ) &&
					 ( false == valueFound ))
			{
				/* 未実装にする */
				ps_orderpara->s_interval[loopCnt1].statFlag = 0;
			}
			else
			{
				/* durationがない場合 */
				if( false == durationFound )
				{
					sprintf(tag, "interval.interval%d.duration", loopCnt1 );
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				}
				
				/* valueがない場合 */
				if( false == valueFound )
				{
					sprintf(tag, "interval.interval%d.value", loopCnt1 );
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				}

				/* baseがない場合 */
				if( false == baseFound )
				{
					sprintf(tag, "interval.interval%d.base", loopCnt1 );
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				}

				retCode = PV_JSON_ERR_FORMAT;
				break;
			}

			/* Interval情報は任意なので、どちらも見つからなくても正常として扱う */
			retCode = PV_JSON_RET_OK;
		}
	}


	/* エネルギーマネジメント管理ファイルのミューテックスをロック */
	//commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

	if( retCode == PV_JSON_RET_OK )
	{
		intervalStart = ps_orderpara->eventStTimeSec;
		/* Interval毎の開始時刻を算出 */
		for( loopCnt1 = 0; loopCnt1 < AWS_INTERVAL_MAX; loopCnt1++ )
		{
			if( ps_orderpara->s_interval[loopCnt1].statFlag == 1 )
			{
				ps_orderpara->s_interval[loopCnt1].startSec = intervalStart;
				intervalStart.tv_sec += (ps_orderpara->s_interval[loopCnt1].duration.tv_sec*ps_orderpara->timeConv);
				intervalStart.tv_usec += ps_orderpara->s_interval[loopCnt1].duration.tv_usec;
				intervalStart.tv_sec += (intervalStart.tv_usec/1000000);
				intervalStart.tv_usec = (intervalStart.tv_usec%1000000);
				ps_orderpara->s_interval[loopCnt1].endSec = intervalStart;
			}
		}

		/* 現在時刻を取得 */
		pvctl_cmn_get_process_tim(&nowTime, NULL);

		/* 開始時刻と終了時刻が不一致の場合(Intervalが登録している場合)、且つ、 */
		/* 終了時刻が現在時刻より過去の場合 */
		if( (( intervalStart.tv_sec != ps_orderpara->eventStTimeSec.tv_sec ) ||
			 ( intervalStart.tv_usec != ps_orderpara->eventStTimeSec.tv_usec )) &&
			( (intervalStart.tv_sec <= nowTime.tv_sec ) ||
			  ((intervalStart.tv_sec == nowTime.tv_sec ) &&
			   (intervalStart.tv_usec <= nowTime.tv_usec ))))
		{
			sprintf(pc_errMsg, "The command time has passed.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			retCode = PV_JSON_ERR_PARAM;
		}
		else
		{
			/* Intervalの実行時間がない場合 */
			if( ( intervalStart.tv_sec == ps_orderpara->eventStTimeSec.tv_sec ) &&
				( intervalStart.tv_usec == ps_orderpara->eventStTimeSec.tv_usec ) )
			{
				sprintf(pc_errMsg, "all interval duration is 0.");
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			}

			/* イベント終了時刻を設定 */
			ps_orderpara->eventEdTimeSec = intervalStart;

/* [20240202] Chg Start */
			/* 指示情報を管理情報へコピー */
			func_order_info_set( index, ps_orderpara );
/* [20240202] Chg End */
		}
	}

	/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
	//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	AWSIOT_MSG_LOW( "retCode=%d\n", retCode);

	return retCode;
}

int func_subscribe( int index, struct st_pvJsonParse *ps_jsonParse, int parseRet, char *pc_errMsg )
{
	char						value[PV_JSON_DATALEN_MAX];
	char						tag[PV_JSON_DATALEN_MAX];
	int							retCode = 0;
	int							loopCnt1;
	struct st_EMCTLFIL			s_emctlfil;
	struct timeval				intervalStart;
	char						errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	bool						durationFound;
	bool						valueFound;
	bool						baseFound;
	int							valueCount;
	struct st_ORDERPARA			orderpara;
	struct st_resinfo			resInfo;
	int							messageKind;
	int							johen_Cnt;
	int							unctl_sv;
	int							set_sv;
	int							set_tm;
	int							res_sv;
	int							res_tm;
	struct st_jsonParseRsult	jsonParseRsult;

	memset( &orderpara, 0, sizeof( orderpara ));
	memset( &resInfo, 0, sizeof(resInfo));

	//*******************
	// jsonデータ取得
	//*******************
	sprintf(tag, "message_type");
	retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
	if( retCode != PV_JSON_RET_OK )
	{
		sprintf(pc_errMsg, "[%s] is not found.[retCode=%d]", tag, retCode);
		AWSIOT_MSG_HIG("%s\n", pc_errMsg );
		retCode = PV_JSON_ERR_FORMAT;
		/* メッセージ種別が見つからない場合は、何もしない */
		return retCode;
	}

	/* イベント種別による分岐 */
	if( strcmp( "Order", jsonParseRsult.value ) == 0 )
	{
		/********************/
		// イベント受信
		/********************/

		/* イベントID取得 */
		sprintf(tag, "event_id");
		retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(pc_errMsg, "[%s] is not found.", tag);
			AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			retCode = PV_JSON_ERR_FORMAT;
			/* イベントIDが見つからない場合は、応答メッセージの情報がないので何もしない */
			return retCode;
		}
		/* イベントIDを保持する */
		memcpy( orderpara.eventId, jsonParseRsult.value, sizeof( orderpara.eventId ));
		memcpy( resInfo.eventId, jsonParseRsult.value, sizeof( resInfo.eventId ));

		/* メッセージ種別、イベントIDが設定されている場合 */
		/* 応答メッセージは送信する */


		/* エラーメッセージを初期化*/
		sprintf(pc_errMsg, "OK.");

		/* json解析結果が正常以外の場合 */
		if( parseRet != PV_JSON_RET_OK )
		{
			/* 応答メッセージ結果に解析結果を設定する */
			retCode = parseRet;

			/* エラーメッセージを設定 */
			memcpy( pc_errMsg, pc_errMsg, PV_MSG_SIZE_LOG);
		}

		if( retCode == PV_JSON_RET_OK )
		{
			/* deviceID取得 */
			sprintf(tag, "device_id");
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode != PV_JSON_RET_OK )
			{
				sprintf(pc_errMsg, "[%s] is not found.", tag);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				retCode = PV_JSON_ERR_FORMAT;
			}
			else
			{
				/* デバイスIDが不一致の場合 */
				if( strcmp( s_iot_ems[index].deviceId, jsonParseRsult.value ) != 0 )
				{
					sprintf(pc_errMsg, "[%s] does not match (%s!=%s).", tag, s_iot_ems[index].deviceId, jsonParseRsult.value);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
				}
				memcpy( orderpara.deviceId, jsonParseRsult.value, sizeof( orderpara.deviceId ));
			}
		}

		if( retCode == PV_JSON_RET_OK )
		{
			/* timestamp取得 */
			sprintf(tag, "timestamp");
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode != PV_JSON_RET_OK )
			{
				sprintf(pc_errMsg, "[%s] is not found.", tag);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			}
			/* タイムスタンプは異常でも制御には影響ないので正常として扱う */
			retCode = PV_JSON_RET_OK;
			memcpy( orderpara.timeStamp, jsonParseRsult.value, sizeof( orderpara.timeStamp ));
		}

		if( retCode == PV_JSON_RET_OK )
		{
			/* orderType取得 */
			sprintf(tag, "order_type");
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode != PV_JSON_RET_OK )
			{
				sprintf(pc_errMsg, "[%s] is not found.", tag);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				retCode = PV_JSON_ERR_FORMAT;
			}
			else
			{
				memcpy( orderpara.orderStr, jsonParseRsult.value, sizeof( orderpara.orderStr ));

				/* 指令種別のチェック */
				if( strcmp( jsonParseRsult.value, AWS_ORDER_TYPE_AC_STR ) == 0 )
				{
					orderpara.orderType = AWS_ORDER_TYPE_AC;
					retCode = func_subscribe_order_info_der( index, ps_jsonParse, pc_errMsg, &orderpara);
				}
				else
				{
					sprintf(pc_errMsg, "[%s] is not supported.", tag);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					retCode = PV_JSON_ERR_UNSUPPORTED;
				}
			}
		}

		/* エネルギーマネジメント管理ファイルのミューテックスをロック */
		//commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

		/* メッセージ種別を設定 */
		resInfo.messageKind = AWS_NOTIFY_RES;
		/* リトライ回数を設定 */
		//resInfo.retryCnt = s_iot_ems[index].sndRetry;
		resInfo.retryCnt = 3;
		/* 応答結果、エラーメッセージを設定 */
		resInfo.resResut = retCode;
		memcpy( resInfo.errMsg, pc_errMsg, sizeof(resInfo.errMsg));
		/* 応答情報登録 */
		func_cmn_setNotify( index, &resInfo );

		/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
		//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	}
	else if( ( strcmp( "Response", jsonParseRsult.value ) == 0 ) ||
			 ( strcmp( "Notify", jsonParseRsult.value ) == 0 ) )
	{
		//if( s_SubscribeSkip == 1 )
		if( 1 )
		{
			/* Subscribeによる送信チェックをスキップする */
		}
		else
		{
			/* 通知の場合 */
			if( strcmp( "Notify", jsonParseRsult.value ) == 0 )
			{
			 	/* イベント状態を取得 */
				sprintf(tag, "event_status");
				retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
				if( retCode != PV_JSON_RET_OK )
				{
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				}
				if( strcmp( "start", jsonParseRsult.value ) == 0 )
				{
					messageKind = AWS_NOTIFY_NOTIFY_S;
				}
				else if( strcmp( "stop", jsonParseRsult.value ) == 0 )
				{
					messageKind = AWS_NOTIFY_NOTIFY_E;
				}
				else
				{
					sprintf(pc_errMsg, "[%s] is not supported(%s).", tag, jsonParseRsult.value);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				}
			}
			else
			{
				messageKind = AWS_NOTIFY_RES;
			}

			/* deviceID取得 */
			sprintf(tag, "device_id");
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode != PV_JSON_RET_OK )
			{
				sprintf(pc_errMsg, "%s is not found.[%s]", tag, jsonParseRsult.value);
				AWSIOT_MSG_HIG("%s\n", pc_errMsg );
			}
			else
			{
				/* デバイスIDが不一致の場合 */
				if( strcmp( s_iot_ems[index].deviceId, jsonParseRsult.value ) != 0 )
				{
					sprintf(pc_errMsg, "[%s] is not supported.", tag);
					AWSIOT_MSG_HIG("%s\n", pc_errMsg );
				}
				else
				{
					/* イベントID取得 */
					sprintf(tag, "event_id");
					retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
					if( retCode != PV_JSON_RET_OK )
					{
						sprintf(pc_errMsg, "[%s] is not found.", tag);
						AWSIOT_MSG_HIG("%s\n", pc_errMsg );
					}
					else
					{
						/* メッセージ送信完了 */
						func_cmn_publisResCheck( index, jsonParseRsult.value, messageKind );
					}
				}
			}
		}
	}
	else
	{
		sprintf(pc_errMsg, "%s[%s] is not supported.", tag, jsonParseRsult.value);
		AWSIOT_MSG_HIG("%s\n", pc_errMsg );
		retCode = -1;
	}

	return retCode;
}


int func_order_update( int index )
{
	struct timeval			nowTime;
	struct tm				timeData;
	struct st_ORDERPARA		*ps_orderpara;
	int						actMode;
	int						orderType;
	struct st_resinfo		resInfo;

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);

	/* 初期設定 */
	memset(&resInfo, 0, sizeof(resInfo));
	/* 現在時刻を設定 */
	resInfo.eventTimeSec = nowTime;
	/* リトライ回数を設定 */
	resInfo.retryCnt = 0;

	/* エネルギーマネジメント管理ファイルのミューテックスをロック */
	//commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

	/* 現在のモード・指令種別を基準に設定*/
	actMode = s_iot_ems[index].s_setdata.actMode;
	orderType = s_iot_ems[index].s_setdata.orderType;
	
	ps_orderpara = &s_iot_ems[index].s_orderpara[AWS_ORDER_NEXT];
	/* 次回の開始時刻が有効な場合 */
	if( ps_orderpara->eventStTimeSec.tv_sec != 0 )
	{
		/* 開始時刻を過ぎている場合、指示情報を更新する */
		if( (ps_orderpara->eventStTimeSec.tv_sec < nowTime.tv_sec ) ||
			((ps_orderpara->eventStTimeSec.tv_sec == nowTime.tv_sec ) &&
			 (ps_orderpara->eventStTimeSec.tv_usec <= nowTime.tv_usec )))
		{
			/* 現在運用中の場合 */
			if( actMode == AWS_MODE_OPE )
			{
				/* 現在運用中のイベントの終了通知を行う */
				resInfo.messageKind = AWS_NOTIFY_NOTIFY_E;
				memcpy( resInfo.eventId, s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].eventId, sizeof(resInfo.eventId));
				/* 応答情報登録 */
				func_cmn_setNotify( index, &resInfo );
			}

			/* 指示情報を更新 */
			memmove( &s_iot_ems[index].s_orderpara[AWS_ORDER_NOW], &s_iot_ems[index].s_orderpara[AWS_ORDER_NEXT], sizeof(struct st_ORDERPARA ) * (AWS_ORDER_STOCK - 1) );/* [20240202] Chg */
			memset( &s_iot_ems[index].s_orderpara[AWS_ORDER_STOCK - 1], 0, sizeof(struct st_ORDERPARA ));/* [20240202] Chg */

			orderType = s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].orderType;

			sprintf(errMsg, "deviceId[%s] : order update : %04d-%02d-%02d %02d:%02d:%02d.%03d", 
					s_iot_ems[index].deviceId,
					timeData.tm_year,timeData.tm_mon,timeData.tm_mday,
					timeData.tm_hour, timeData.tm_min, timeData.tm_sec, nowTime.tv_usec/1000);
			AWSIOT_MSG_HIG( "%s\n", errMsg );
			s_iot_ems[index].s_evtdata.intervalPos = 0;

			/* 開始時刻と終了時刻が同じ場合は、イベントの開始/終了通知を行わない */
			if( ( s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].eventStTimeSec.tv_sec == s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].eventEdTimeSec.tv_sec ) &&
				( s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].eventStTimeSec.tv_usec == s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].eventEdTimeSec.tv_usec ))
			{
				actMode = AWS_MODE_IDLE;
			}
			else
			{
				actMode = AWS_MODE_OPE;

				/* 次のイベントの開始通知を行う */
				resInfo.messageKind = AWS_NOTIFY_NOTIFY_S;
				memcpy( resInfo.eventId, s_iot_ems[index].s_orderpara[AWS_ORDER_NOW].eventId, sizeof(resInfo.eventId));
				/* 応答情報登録 */
				func_cmn_setNotify( index, &resInfo );
			}
		}
	}

	ps_orderpara = &s_iot_ems[index].s_orderpara[AWS_ORDER_NOW];
	/* 現在の終了時刻が有効な場合 */
	if( ps_orderpara->eventEdTimeSec.tv_sec != 0 )
	{
		/* 終了時刻を過ぎている場合、指示情報を更新する */
		if( (ps_orderpara->eventEdTimeSec.tv_sec < nowTime.tv_sec ) ||
			((ps_orderpara->eventEdTimeSec.tv_sec == nowTime.tv_sec ) &&
			 (ps_orderpara->eventEdTimeSec.tv_usec <= nowTime.tv_usec )))
		{
			/* イベントIDの終了通知を行う */
			if( actMode == AWS_MODE_OPE )
			{
				/* イベントの終了通知を行う */
				resInfo.messageKind = AWS_NOTIFY_NOTIFY_E;
				memcpy( resInfo.eventId, ps_orderpara->eventId, sizeof(resInfo.eventId));
				/* 応答情報登録 */
				func_cmn_setNotify( index, &resInfo );
			}

			actMode = AWS_MODE_IDLE;

			/* 運用中の場合のみログ出力 */
			if( s_iot_ems[index].s_setdata.actMode == AWS_MODE_OPE )
			{
				sprintf(errMsg, "deviceId[%s] : order end : %04d-%02d-%02d %02d:%02d:%02d.%03d", 
						s_iot_ems[index].deviceId,
						timeData.tm_year,timeData.tm_mon,timeData.tm_mday,
						timeData.tm_hour, timeData.tm_min, timeData.tm_sec, nowTime.tv_usec/1000);
				AWSIOT_MSG_HIG( "%s\n", errMsg );
			}
		}
	}

	/* 動作モード・指令種別を更新する */
	s_iot_ems[index].s_setdata.actMode = actMode;
	s_iot_ems[index].s_setdata.orderType = orderType;
	
	/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
	//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );
	
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IoT 制御チェック
 *
 * 機能：
 * 		IoT 制御のチェックと実行を行う
 * 戻り値：
 * 		-1：制御なし
 * 		 0：制御正常
 */
int func_order_check( int index )
{
	int						loopCnt1;					/* ループカウンタ */
	struct timeval			nowTime;
	struct tm				timeData;
	int						retCode = 0;
	int						targetPow = 0;
	int						targetBase = 0;
	int						valueConv = 0;
	int						tmFlag = PV_TMFLG_DROPTM;
	struct st_ORDERPARA		*ps_orderpara;

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);

	if( s_iot_ems[index].s_setdata.actMode == AWS_MODE_OPE )
	{
		ps_orderpara = &s_iot_ems[index].s_orderpara[AWS_ORDER_NOW];

		/* 期間数分ループ */
		for( loopCnt1 = s_iot_ems[index].s_evtdata.intervalPos; loopCnt1 < AWS_INTERVAL_MAX; loopCnt1++ )
		{
			/* 実装済みチェック */
			if( ps_orderpara->s_interval[loopCnt1].statFlag == 1 )
			{
				/* 現在時刻が期間内の場合 */
				if( ( (ps_orderpara->s_interval[loopCnt1].startSec.tv_sec < nowTime.tv_sec ) ||
					  ((ps_orderpara->s_interval[loopCnt1].startSec.tv_sec == nowTime.tv_sec ) &&
					   (ps_orderpara->s_interval[loopCnt1].startSec.tv_usec <= nowTime.tv_usec ))) &&
					( (nowTime.tv_sec < ps_orderpara->s_interval[loopCnt1].endSec.tv_sec) ||
					  ((nowTime.tv_sec == ps_orderpara->s_interval[loopCnt1].endSec.tv_sec ) &&
					   (nowTime.tv_usec <= ps_orderpara->s_interval[loopCnt1].endSec.tv_usec ))) )
				{
					/* 切り替わり時はログを出力 */
					if( ps_orderpara->s_interval[loopCnt1].setFlag == 0 )
					{
						sprintf(errMsg, "deviceId[%s] : IoT info[%d] : %04d-%02d-%02d %02d:%02d:%02d.%03d -> base[%d], value[%d].", 
								s_iot_ems[index].deviceId,
								loopCnt1,
								timeData.tm_year,timeData.tm_mon,timeData.tm_mday,
								timeData.tm_hour, timeData.tm_min, timeData.tm_sec, nowTime.tv_usec/1000,
								ps_orderpara->s_interval[loopCnt1].base,
								ps_orderpara->s_interval[loopCnt1].value);
						AWSIOT_MSG_HIG( "%s\n", errMsg );
					}
					
					/* 実行済みに設定 */
					ps_orderpara->s_interval[loopCnt1].setFlag = 1;
					s_iot_ems[index].s_evtdata.intervalPos = loopCnt1;

					targetPow = ps_orderpara->s_interval[loopCnt1].value;
					targetBase= ps_orderpara->s_interval[loopCnt1].base;
					valueConv = ps_orderpara->valueConv;
					break;
				}
			}
		}
	}

	/* 制御値(kw)を設定 */
	s_iot_ems[index].s_evtdata.targetPow = targetPow * valueConv;
	s_iot_ems[index].s_evtdata.targetBase = targetBase * valueConv;

	//AWSIOT_MSG_LOW("END\n");

	return retCode;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通知情報登録
 *
 * 機能：
 * 		通知情報をキューに登録する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int	func_cmn_setNotify( int index, struct st_resinfo *ps_resInfo )
{
	struct timeval			nowTime;
	int						retCode = 0;				/* 戻り値 */
	struct st_resinfo		*ps_SetInfo;
	int						loopCnt1;
	int						tempCnt = AWS_NOTIFY_STOCK;
	int						tempRes;

	sprintf(errMsg, "START, eventId=%s, messageKind=%d", ps_resInfo->eventId, ps_resInfo->messageKind );
	AWSIOT_MSG_MID("%s\n", errMsg );


	/* フェールセーフ（基本的にはありえない） */
	for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
	{
		ps_SetInfo = &s_iot_ems[index].s_resinfo[s_iot_ems[index].resOrder[loopCnt1]];

		/* 送信待ちの応答情報で、イベントID、イベント開始時刻、メッセージ種別が一致している場合は、上書きする */
		if( ( strncmp( ps_SetInfo->eventId, ps_resInfo->eventId, strlen(ps_SetInfo->eventId)+1 ) == 0 ) &&
			( ( ps_SetInfo->eventStTimeSec.tv_sec == ps_resInfo->eventStTimeSec.tv_sec ) &&
			  ( ps_SetInfo->eventStTimeSec.tv_usec == ps_resInfo->eventStTimeSec.tv_usec ) ) && /* [20240207] Add */
			( ps_SetInfo->sndstatus == AWS_NOTIFY_ST_WAIT ) &&
			( ps_SetInfo->messageKind == ps_resInfo->messageKind ))
		{
			tempCnt = loopCnt1;
			/* SYSログに出力 */
			sprintf(errMsg, "duplicate resInfo : array=%d, eventId[%s], eventTimeSec[%d], messageKind[%d], Msg[%s]", /* [20240207] Chg */
					s_iot_ems[index].resOrder[loopCnt1],
					ps_SetInfo->eventId,
					ps_SetInfo->eventStTimeSec.tv_sec,/* [20240207] Add */
					ps_SetInfo->messageKind,
					ps_SetInfo->errMsg);
			AWSIOT_MSG_ERR("%s\n", errMsg );
			break;
		}
	}

	/* 未設定のキューがない場合、未設定の応答情報を検索する */
	if( loopCnt1 == AWS_NOTIFY_STOCK )
	{
		for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
		{
			ps_SetInfo = &s_iot_ems[index].s_resinfo[s_iot_ems[index].resOrder[loopCnt1]];

			/* 送信情報が未設定の場合 */
			if( ps_SetInfo->sndstatus == AWS_NOTIFY_ST_NON )
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "func_cmn_setNotify : Set [%d] resId[%d]", loopCnt1+1, ps_SetInfo->resId );
				AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
				tempCnt = loopCnt1;
				break;
			}
			/* 送信情報が送信待ち以外の場合 */
			if( ( ps_SetInfo->sndstatus != AWS_NOTIFY_ST_WAIT ) &&
				( tempCnt == AWS_NOTIFY_STOCK ))
			{
				tempCnt = loopCnt1;
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "func_cmn_setNotify : NotWait [%d] resId[%d]", loopCnt1+1, ps_SetInfo->resId );
				AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
			}
		}

		/* 未設定のキューがない場合、送信待ち以外の応答情報を検索する */
		if( loopCnt1 == AWS_NOTIFY_STOCK )
		{
			/* 送信待ち以外の応答情報が見つからなかった場合 */
			if( tempCnt == AWS_NOTIFY_STOCK )
			{
				/* 全て送信待ちの場合は、一番古い(先頭)の応答情報を上書きする */
				ps_SetInfo = &s_iot_ems[index].s_resinfo[s_iot_ems[index].resOrder[0]];
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "func_cmn_setNotify : AWS_NOTIFY_ST_NON resInfo is not found.");
				AWSIOT_MSG_ERR("%s\n", errMsg );
				/* SYSログに出力 */
				sprintf(errMsg, "func_cmn_setNotify : delete [1] resId[%d] eventId[%s], eventTimeSec[%d], messageKind[%d], Msg[%s]", /* [20240207] Chg */
						ps_SetInfo->resId,
						ps_SetInfo->eventId,
						ps_SetInfo->eventStTimeSec.tv_sec,/* [20240207] Add */
						ps_SetInfo->messageKind,
						ps_SetInfo->errMsg);
				AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
			}
		}
	}

	/* 応答情報に追加する */
	if( tempCnt == AWS_NOTIFY_STOCK )
	{
		/* 一番古い応答情報に上書きする */
		tempRes = s_iot_ems[index].resOrder[0];
		for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK-1; loopCnt1++ )
		{
			s_iot_ems[index].resOrder[loopCnt1] = s_iot_ems[index].resOrder[loopCnt1+1];
		}
		s_iot_ems[index].resOrder[loopCnt1] = tempRes;
	}
	else
	{
		/* 設定可能な応答情報を更新し、送信待ちの最後に設定する */
		tempRes = s_iot_ems[index].resOrder[tempCnt];
		for( loopCnt1 = tempCnt-1; loopCnt1 >= 0; loopCnt1-- )
		{
			if( s_iot_ems[index].s_resinfo[s_iot_ems[index].resOrder[loopCnt1]].sndstatus == AWS_NOTIFY_ST_WAIT )
			{
				break;
			}
			s_iot_ems[index].resOrder[loopCnt1+1] = s_iot_ems[index].resOrder[loopCnt1];
			tempCnt--;
		}
		s_iot_ems[index].resOrder[tempCnt] = tempRes;
	}
	ps_SetInfo = &s_iot_ems[index].s_resinfo[tempRes];

	/* 送信待ちに設定 */
	ps_SetInfo->sndstatus = AWS_NOTIFY_ST_WAIT;
	ps_SetInfo->resSndCnt = 0;
	/* 応答情報を設定する */
	ps_SetInfo->retryCnt = ps_resInfo->retryCnt;
	ps_SetInfo->messageKind = ps_resInfo->messageKind;
	ps_SetInfo->eventTimeSec = ps_resInfo->eventTimeSec;
	ps_SetInfo->resResut = ps_resInfo->resResut;
	memcpy( ps_SetInfo->eventId, ps_resInfo->eventId, sizeof(ps_SetInfo->eventId) );
	memcpy( ps_SetInfo->errMsg, ps_resInfo->errMsg, sizeof(ps_SetInfo->errMsg) );
	ps_SetInfo->eventStTimeSec = ps_resInfo->eventStTimeSec;/* [20240207] Add */

	sprintf(errMsg, "END");
	AWSIOT_MSG_MID("%s\n", errMsg );

	return 0;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通知情報の送信完了チェック
 *
 * 機能：
 * 		通知情報がキューにあるかチェックし、更新する
 */
int	func_cmn_publisResCheck( int index, char *event_id, int messageKind )
{
	int						retCode=-1;
	struct st_resinfo		*ps_SetInfo;
	struct st_resinfo		*ps_preInfo;
	int						hitCnt=0;
	int						loopCnt1;
	int						tempCnt;

	/* エネルギーマネジメント管理ファイルのミューテックスをロック */
	//commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "func_cmn_publisResCheck : START, eventId=%s, messageKind=%d", event_id, messageKind );
	AWSIOT_MSG_ERR("%s\n", errMsg );
#endif

	/* 設定する応答情報を検索する */
	for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
	{
		ps_SetInfo = &s_iot_ems[index].s_resinfo[s_iot_ems[index].resOrder[loopCnt1]];

		/* イベントID、メッセージ種別を比較（一致する全てのイベントを更新） */
		if( ( strncmp( ps_SetInfo->eventId, event_id, strlen(ps_SetInfo->eventId)+1 ) == 0 ) &&
			( ps_SetInfo->messageKind == messageKind ))
		{
			/* 該当するイベントをカウントする */
			hitCnt++;

			/* 状態が送信待ちの場合 */
			if( ps_SetInfo->sndstatus == AWS_NOTIFY_ST_WAIT )
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "func_cmn_publisResCheck : [%d] resId[%d] -> complete", loopCnt1+1, ps_SetInfo->resId);
				AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
				break;
			}
		}
	}

	/* 応答情報順序を更新する */
	if( loopCnt1 != AWS_NOTIFY_STOCK )
	{
		/* 送信状態を更新する */
		ps_SetInfo->sndstatus = AWS_NOTIFY_ST_COMP;

		/* 応答情報に追加する */
		for( tempCnt = s_iot_ems[index].resOrder[loopCnt1]; loopCnt1 < AWS_NOTIFY_STOCK-1; loopCnt1++ )
		{
			s_iot_ems[index].resOrder[loopCnt1] = s_iot_ems[index].resOrder[loopCnt1+1];
		}
		s_iot_ems[index].resOrder[loopCnt1] = tempCnt;
	}

	/* 一致する情報が見つからなかった場合 */
	if( hitCnt == 0 )
	{
		sprintf(errMsg, "Notify info is not found eventId[%s], messageKind[%d]", event_id, messageKind );
		AWSIOT_MSG_ERR("%s\n", errMsg );
		
	}

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "func_cmn_publisResCheck : END");
	AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
	/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
	//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通知情報送信
 *
 * 機能：
 * 		通知情報を送信する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int	func_cmn_publishNotify( int index )
{
	IoT_Error_t 				rc = FAILURE;
	char 						cPayload[512];
	struct st_resinfo			*ps_SetInfo;
	IoT_Publish_Message_Params 	paramsQOS;
	struct tm					evTimeData;					/* 時刻情報 */
	int							retCode=0;
	int							loopCnt1;
	int							loopCnt2;
	int							tempCnt;
	struct timeval				nowTime;
	struct tm					timeData;					/* 時刻情報 */
	int							messageKind;					/* メッセージ種別(応答/開始/終了) */
	char						eventId[PV_JSON_DATALEN_MAX];	/* イベントID */

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "func_cmn_publishNotify : START");
	AWSIOT_MSG_ERR("%s\n", errMsg );
#endif

	for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
	{
		/* 積算秒を取得 */
		gettimeofday( &nowTime, NULL );
		/* 時刻情報に変換 */
		localtime_r( &nowTime.tv_sec, &timeData );

		/* エネルギーマネジメント管理ファイルのミューテックスをロック */
		//commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

		ps_SetInfo = &s_iot_ems[index].s_resinfo[s_iot_ems[index].resOrder[loopCnt1]];

#ifdef PV_DEBUG_QUEUE
		sprintf(errMsg, "func_cmn_publishNotify : [%d] resId[%d] sndstate[%d]", loopCnt1+1, ps_SetInfo->resId, ps_SetInfo->sndstatus );
		AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
		/* 送信情報が送信待ちの場合 */
		if( ps_SetInfo->sndstatus == AWS_NOTIFY_ST_WAIT )
		{
			/* リトライ回数があり、且つ、送信回数がリトライ回数に到達した場合  */
			if( ( ps_SetInfo->retryCnt != 0 ) &&
				( ps_SetInfo->resSndCnt >= ps_SetInfo->retryCnt ) )
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "func_cmn_publishNotify : [%d] resId[%d] -> failde", loopCnt1+1, ps_SetInfo->resId);
				AWSIOT_MSG_ERR("%s\n", errMsg );
#endif
				/* 送信状態を更新しする */
				ps_SetInfo->sndstatus = AWS_NOTIFY_ST_FAIL;

				/* 応答送信順序を更新する（最後に移動） */
				tempCnt = s_iot_ems[index].resOrder[loopCnt1];
				for( loopCnt2 = loopCnt1; loopCnt2 < AWS_NOTIFY_STOCK-1; loopCnt2++ )
				{
					s_iot_ems[index].resOrder[loopCnt2] = s_iot_ems[index].resOrder[loopCnt2+1];
				}
				s_iot_ems[index].resOrder[loopCnt2] = tempCnt;
				/* 応答情報送信順序を前にずらしたため、次の情報(同じ番号)からやり直し */
				loopCnt1--;
				/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
				//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );
				continue;
			}
			else
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "func_cmn_publishNotify : [%d] resId[%d] publish, eventId[%s], messageKind[%d]", loopCnt1+1, ps_SetInfo->resId, ps_SetInfo->eventId, ps_SetInfo->messageKind );
				AWSIOT_MSG_ERR("%s\n", errMsg );
#endif

				/* 送信回数をインクリメントする */
				ps_SetInfo->resSndCnt++;

				/* メッセージ種別毎に分ける */
				switch( ps_SetInfo->messageKind )
				{
					case AWS_NOTIFY_RES :
						/* 送信データ生成 */
						sprintf(cPayload, "{\n\"message_type\":\"Response\",\n\"event_id\": \"%s\",\n\"device_id\": \"%s\",\n\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\",\n\"response_code\":%d,\n\"response_message\":\"%s\"\n}", 
								ps_SetInfo->eventId, s_iot_ems[index].deviceId,
								timeData.tm_year+1900,timeData.tm_mon+1,timeData.tm_mday,timeData.tm_hour, timeData.tm_min, timeData.tm_sec,(nowTime.tv_usec/1000),ps_SetInfo->resResut, ps_SetInfo->errMsg);
						break;
					case AWS_NOTIFY_NOTIFY_S :
						localtime_r( &ps_SetInfo->eventTimeSec.tv_sec, &evTimeData );

						/* 送信データ生成 */
/* [20230711] Chg St */
						sprintf(cPayload, "{\n\"message_type\":\"Notify\",\n\"event_id\": \"%s\",\n\"device_id\": \"%s\",\n\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\",\n\"date\":\"%04d-%02d-%02d %02d:%02d:%02d.%d00\",\n\"event_status\":\"start\"\n}", 
								ps_SetInfo->eventId, s_iot_ems[index].deviceId,
								timeData.tm_year+1900,timeData.tm_mon+1,timeData.tm_mday,timeData.tm_hour, timeData.tm_min, timeData.tm_sec,(nowTime.tv_usec/1000),
								evTimeData.tm_year+1900,evTimeData.tm_mon+1,evTimeData.tm_mday,evTimeData.tm_hour, evTimeData.tm_min, evTimeData.tm_sec, ps_SetInfo->eventTimeSec.tv_usec/100000);
/* [20230711] Chg Ed */
						break;
					case AWS_NOTIFY_NOTIFY_E :
						localtime_r( &ps_SetInfo->eventTimeSec.tv_sec, &evTimeData );

						/* 送信データ生成 */
						sprintf(cPayload, "{\n\"message_type\":\"Notify\",\n\"event_id\": \"%s\",\n\"device_id\": \"%s\",\n\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\",\n\"date\":\"%04d-%02d-%02d %02d:%02d:%02d.%d00\",\n\"event_status\":\"stop\"\n}", 
								ps_SetInfo->eventId, s_iot_ems[index].deviceId,
								timeData.tm_year+1900,timeData.tm_mon+1,timeData.tm_mday,timeData.tm_hour, timeData.tm_min, timeData.tm_sec,(nowTime.tv_usec/1000),
								evTimeData.tm_year+1900,evTimeData.tm_mon+1,evTimeData.tm_mday,evTimeData.tm_hour, evTimeData.tm_min, evTimeData.tm_sec, ps_SetInfo->eventTimeSec.tv_usec/100000);
						break;
				}
				messageKind = ps_SetInfo->messageKind;		/* メッセージ種別保持*/
				sprintf(eventId, ps_SetInfo->eventId);		/* イベントID保持 */
				
				/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
				//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

				/* 送信不可設定の場合 */
				if( s_iot_ems[index].notSend == 1 )
				{
					/* 送信完了チェック */
					func_cmn_publisResCheck( index, eventId, messageKind );
					/* 応答情報送信順序を前にずらしたため、次の情報(同じ番号)からやり直し *//* [20240208] Add */
					loopCnt1--;
					
				}
				else
				{
					paramsQOS.qos = QOS0;
					paramsQOS.payload = (void *) cPayload;
					paramsQOS.payloadLen = strlen(cPayload);
					paramsQOS.isRetained = 0;
					rc = aws_iot_mqtt_publish(&s_iot_ems[index].rcv_client, s_iot_ems[index].rcvTopicName, strlen(s_iot_ems[index].rcvTopicName), &paramsQOS);

					if(SUCCESS != rc)
					{
						sprintf(errMsg, "func_iot_rcv_thread : aws_iot_mqtt_publish is Error(%d). deviceId=%s, eventId=%s, resSndCnt=%d", rc, s_iot_ems[index].deviceId, eventId, ps_SetInfo->resSndCnt);
						AWSIOT_MSG_ERR("%s\n", errMsg );
						/* 装置状態状変：IOT通信異常(受信)：発生 */
						//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

						if( rc < SUCCESS)
						{
							/* aws再起動 */
							func_aws_rcv_quit( &s_iot_ems[index] );
							retCode=-1;
							break;
						}
					}
					else
					{
						AWSIOT_MSG_HIG("aws_iot_mqtt_publish is SUCCESS (deviceId,messageKind,event_id)=(%s,%d,%s)\n", s_iot_ems[index].deviceId, messageKind, eventId);

						/* 装置状態状変：IOT通信異常(受信)：復帰 */
						//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

						/* Subscribeによる送信チェックをスキップする */
						//if( s_SubscribeSkip == 1 )
						{
							/* 送信完了チェック */
							func_cmn_publisResCheck( index, eventId, messageKind );
							/* 応答情報送信順序を前にずらしたため、次の情報(同じ番号)からやり直し *//* [20240208] Add */
							loopCnt1--;
						}
					}
					/* 送信データがAWSライブラリの受信バッファを超える場合 */
					if( paramsQOS.payloadLen > AWS_IOT_MQTT_RX_BUF_LEN )
					{
						sprintf(errMsg, "aws_iot_mqtt_publish data size over (%d > %d).", paramsQOS.payloadLen, AWS_IOT_MQTT_RX_BUF_LEN);
						AWSIOT_MSG_ERR("%s\n", errMsg );

						/* 装置状態状変：設定異常：発生 */
						//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
					}
				}
			}
		}
		else
		{
			/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
			//commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );
		}
	}

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "func_cmn_publishNotify : END");
	AWSIOT_MSG_ERR("%s\n", errMsg );
#endif


	return retCode;
}
/* [20230529] Add Ed */

/* [20240207] Add Start */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 指令情報設定
 *
 * 指令情報設定処理(上書き、差し込み)
 *
 * 機能：
 * 		EMCTLFILの指令情報を未来ストック1～5に設定する。1→5方向に検索
 * 		・空きストックがあれば設定
 * 		・イベント開始時刻が、該当ストックのイベント開始時刻より早い場合、
 * 		  該当ストックに差し込み、該当ストックから末端-1のストックを1つ後方にずらす。
 * 		・同一イベントID、同一イベント開始時刻の指令は、該当ストックに上書き
 * 		  同一イベントID、別イベント開始時刻の指令は、別指令として後方のストックを検索
 * 戻り値：
 * 		なし
 */
void func_order_info_set( int index, struct st_ORDERPARA *ps_orderpara )
{
	char						errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	bool						stIsMutch = false;			/* 開始時刻一致 */
	bool						idIsMutch = false;			/* イベントID一致 */
	bool						writePtCnf = false;			/* 書込み位置確定 */
	int							wpCnt;						/* 書込み位置ループカウンタ */
	int							cntInterval;				/* インターバルループカウンタ */
	bool						rcvStMin = false;			/* 受信イベント開始時刻＜保持イベント開始時刻 */
	struct timeval				tmpEventStTimeSec;			/* イベント開始時刻(sec)(ログ出力判断に使用」) */

	/* ログ出力判定用にストック端-2(未来ストック4)の情報を保持 */
	memcpy( &tmpEventStTimeSec, &s_iot_ems[index].s_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec, sizeof( tmpEventStTimeSec ) );
	/* 未来1～5ストックへの格納場所を検索 */
	/* イベントID、イベント開始時刻の一致は、同一指令と判断する */
	/* 受信指令のイベント開始時刻がストックのイベント開始時刻より小さい場合、そのストックに受信指令を差し込む */
	/* 空きストック＞イベント開始時刻が早い＞同一指令の優先順位で判定する */
	for ( wpCnt = AWS_ORDER_NEXT; wpCnt < AWS_ORDER_STOCK; wpCnt++ ) {
		/* フラグ初期化 */
		idIsMutch = false;
		stIsMutch = false;
		/* イベントIDが設定されているか */
		if ( strlen( s_iot_ems[index].s_orderpara[wpCnt].eventId ) == 0 )
		{
			/* 空きストック(イベントIDなし) */
			/* 書込み位置確定 */
			writePtCnf = true;
			break;
		}
		/* イベントIDが一致するか */
		if ( strcmp( s_iot_ems[index].s_orderpara[wpCnt].eventId, ps_orderpara->eventId ) == 0 )
		{
			/* イベントID一致 */
			idIsMutch = true;
		}

		/* イベント開始時刻が一致するか */
		if ( ( s_iot_ems[index].s_orderpara[wpCnt].eventStTimeSec.tv_sec == ps_orderpara->eventStTimeSec.tv_sec ) &&
			 ( s_iot_ems[index].s_orderpara[wpCnt].eventStTimeSec.tv_usec == ps_orderpara->eventStTimeSec.tv_usec ) )
		{
			/* イベント開始時刻が一致の場合 */
			stIsMutch = true;
		}
		/* イベント開始時刻がストックより早いか */
		else if ( ( s_iot_ems[index].s_orderpara[wpCnt].eventStTimeSec.tv_sec > ps_orderpara->eventStTimeSec.tv_sec ) ||
				( ( s_iot_ems[index].s_orderpara[wpCnt].eventStTimeSec.tv_sec == ps_orderpara->eventStTimeSec.tv_sec ) &&
				  ( s_iot_ems[index].s_orderpara[wpCnt].eventStTimeSec.tv_usec > ps_orderpara->eventStTimeSec.tv_usec ) ) )
		{
			/* イベント開始時刻がストックのイベント開始時刻より小さい(=早い) */
			if ( wpCnt != ( AWS_ORDER_STOCK - 1 ) )
			{
				/* ストックの終端以外は、受信指令差し込み */
				/* ストックの終端の場合は、上書きするだけで良い */
				rcvStMin = true;
			}
			/* 書込み位置確定 */
			writePtCnf = true;
			break;
		}

		/* イベントID、イベント開始時刻が一致するか */
		if ( idIsMutch == true && stIsMutch == true )
		{
			/* イベントID、イベント開始時刻(文字列)が一致(上書き対象) */
			/* 書込み位置確定 */
			writePtCnf = true;
			break;
		}
	}

	/* 書込み位置が確定しているか */
	if ( writePtCnf == true )
	{
		/* 差し込みか */
		if ( rcvStMin == true )
		{
			/* 受信指令を差し込む場合、差し込むストックから末尾-1を次のストックから末尾まで移動する */
			memmove( &s_iot_ems[index].s_orderpara[wpCnt + 1], &s_iot_ems[index].s_orderpara[wpCnt], sizeof(struct st_ORDERPARA) * ( AWS_ORDER_STOCK - 1 - wpCnt ) );
		}
	}
	else
	{
		/* 同一指令、空きが見つからなかった場合、最後尾の指令を上書き */
		wpCnt = AWS_ORDER_STOCK -1;
	}

	/* 受信指令をストックに保存する */
	memcpy(&s_iot_ems[index].s_orderpara[wpCnt], ps_orderpara, sizeof(struct st_ORDERPARA));

	/* ログ出力対象の動作か(未来4に上書き、または、未来4の内容が変わる差し込み) */
	if ( ( wpCnt == ( AWS_ORDER_STOCK - 2 ) ) || ( tmpEventStTimeSec.tv_sec != s_iot_ems[index].s_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec.tv_sec ) ||
		( ( tmpEventStTimeSec.tv_sec == s_iot_ems[index].s_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec.tv_sec ) && ( tmpEventStTimeSec.tv_usec != s_iot_ems[index].s_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec.tv_usec ) ) )
	{
		/* 最大使用想定の未来ストック4に書込みをした場合、または差し込みで未来ストック4の値が変更された場合、未来ストックの中身をログ出力 */
		/* 応答に乗せない様ローカル変数を使用 */
		sprintf(errMsg, "orderpara nextslot No4 used");
		AWSIOT_MSG_ERR("%s\n", errMsg );
		/* 指令ストックを現在を含め全てログ出力 */
		for ( wpCnt = AWS_ORDER_NOW; wpCnt < AWS_ORDER_STOCK; wpCnt++ )
		{
			/* ストックの出力 */
			sprintf(errMsg, "stock No=%d eventId=%s orderType=%s eventTimeStr=%s", wpCnt, s_iot_ems[index].s_orderpara[wpCnt].eventId,
				s_iot_ems[index].s_orderpara[wpCnt].orderStr, s_iot_ems[index].s_orderpara[wpCnt].eventTimeStr );
			AWSIOT_MSG_ERR("%s\n", errMsg );
			/* インターバルの内容を最大10個出力 */
			for ( cntInterval = 0; cntInterval < 10; cntInterval++ )
			{
				/* インターバルが設定されているか */
				if ( s_iot_ems[index].s_orderpara[wpCnt].s_interval[cntInterval].duration.tv_sec == 0 &&
					 s_iot_ems[index].s_orderpara[wpCnt].s_interval[cntInterval].duration.tv_usec == 0 )
				{
					/* スキップ */
					continue;
				}
				/* インターバルのログ出力 */
				sprintf(errMsg, "interval No=%ld intervaltm=%d base=%d value=%4d", cntInterval,
					(s_iot_ems[index].s_orderpara[wpCnt].s_interval[cntInterval].endSec.tv_sec - s_iot_ems[index].s_orderpara[wpCnt].s_interval[cntInterval].startSec.tv_sec) / 60,
					s_iot_ems[index].s_orderpara[wpCnt].s_interval[cntInterval].base, s_iot_ems[index].s_orderpara[wpCnt].s_interval[cntInterval].value );
				AWSIOT_MSG_ERR("%s\n", errMsg );
			}
		}
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * JSONログ出力処理
 *
 * 機能：
 *		
 *
 * 入力：
 *
 * 出力：
 *
 */
int commfunc_json_logout_local( char *pc_fileName, char *pc_payload )
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
	time_t					nowTime;				/* 現在時刻 */
	int						logMode = PV_LOG_OFF;	/* ログ出力 */


	//if( logMode == PV_LOG_ON )
	{
		/* 現在時刻を取得 */
		nowTime = time( NULL );

		/* 年月日を取得 */
		localtime_r( &nowTime, &localTime );

		/* 出力用ログファイル名の設定 */
		//sprintf( fileName1, "%s%s.log", PV_PATH_LOG, pc_fileName );
		sprintf( fileName1, "%s%s.log", "./", pc_fileName );

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

		/* ログファイルをオープン */
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
			for(loopCnt1 = PV_LOG_STOCK_NUM ; loopCnt1 > 1 ; loopCnt1-- )	/* [20230902] */
			{
				/* ファイル名の設定 */
//				sprintf( fileName1, "%s%s_%d.log", PV_PATH_LOG, pc_fileName, loopCnt1 - 1 );
//				sprintf( fileName2, "%s%s_%d.log", PV_PATH_LOG, pc_fileName, loopCnt1 );
				sprintf( fileName1, "%s%s_%d.log", "./", pc_fileName, loopCnt1 - 1 );
				sprintf( fileName2, "%s%s_%d.log", "./", pc_fileName, loopCnt1 );
				/* 古いファイルの削除 */
				if( access( fileName2, F_OK ) == 0 )
					unlink( fileName2 );
				/* ローテーションの実施 */
				if( access( fileName1, F_OK ) == 0 )
					rename( fileName1, fileName2 );
			}
			/* ファイル名の設定 */
//			sprintf( fileName1, "%s%s.log", PV_PATH_LOG, pc_fileName );
//			sprintf( fileName2, "%s%s_1.log", PV_PATH_LOG, pc_fileName );
			sprintf( fileName1, "%s%s.log", "./", pc_fileName );
			sprintf( fileName2, "%s%s_1.log", "./", pc_fileName );
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

			/* ログファイルをオープン */
			if( (fp = fopen( fileName1, "a" )) == NULL )
			{
				return( -1 );
			}
		}

		/* メッセージ出力処理 */
		fprintf( fp, "===== AWS SUBSCRIBE START [ %04d/%02d/%02d/ %02d:%02d:%02d ] =====\n",
			localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
			localTime.tm_hour, localTime.tm_min, localTime.tm_sec );
		fprintf( fp, "%s\n", pc_payload );

		/* ファイルを閉じる */
		fclose( fp );
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス動作時刻取得処理
 *
 * 機能：
 * 		プロセス動作時刻を取得する。
 * 戻り値：
 * 		正常終了
 */
int pvctl_cmn_get_process_tim( struct timeval *p_nowTime, struct tm *p_timData )
{
	struct tm					timeData;					/* 時刻情報 */
	struct timeval				nowTime;					/* 現在時刻(積算秒) */

	/* 積算秒を取得 */
	gettimeofday( &nowTime, NULL );

	if( p_nowTime != NULL )
	{
		/* 積算秒の引数がNULL以外の場合、設定する */
		*p_nowTime = nowTime;
	}
	if( p_timData != NULL )
	{
		/* 時刻情報に変換 */
		localtime_r( &nowTime.tv_sec, &timeData );

		timeData.tm_year += 1900;
		timeData.tm_mon+= 1;
		/* 時刻情報の引数がNULL以外の場合、設定する */
		*p_timData = timeData;
	}

	return(0);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * オンラインデバッガ
 * -----------------------------------------------------------------------------
 * １０のべき乗の絶対値
 *
 * 機能：
 *		基数１０、指数 aData のべき乗を求め、絶対値を返送する。
 */
long long func_pow10( int aData )
{
	int				loopCnt;
	long long		retData = 1;

	if( aData < 0 ) aData *= -1;

	for( loopCnt = 0 ; loopCnt < aData ; loopCnt++ )
		retData *= 10;

	return( retData );
}


/******************************************************************************/
/*                                                                            */
/******************************************************************************/
int  csv_split( char *pc_abuf, char **pc_apt, int l_acnt )
{
    char  *pc_lpt1;
    char  *pc_lpt2;
    int   loopCnt1;

    /* カンマ区切りデータを分割する */
    pc_lpt1 = pc_lpt2 = pc_abuf;
    for(loopCnt1 = 0; (pc_lpt2 != NULL) && (loopCnt1 < l_acnt); loopCnt1++)
    {
        pc_apt[loopCnt1] = pc_lpt1;

        if((pc_lpt2 = strchr(pc_lpt1, (int)',')) != NULL)
            *pc_lpt2 = 0;

        if(pc_lpt2 != NULL)
            pc_lpt1 = pc_lpt2 + 1;
    }

    return(loopCnt1);
}

