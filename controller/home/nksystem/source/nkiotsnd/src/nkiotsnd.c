/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20250110] 2025.01.10
 * ・AWSのKeepAliveの時間を20秒->60秒に変更（RJからの水平展開）
 * [20211228] 2021.12.28
 * ・プロセス動作周期を変更
 * ・プロセス動作周期にIoT-Coreへ送信する収集データファイルの情報の上限を
 *   1ファイルにするように修正
 * [20211224] 2021.12.24
 * ・PublishしたデータをSubscribeでチェックする/しないを切り替えられるように対応
 * [20210917] 2021.09.17
 * ・送信ファイル(データ)を日付毎にディレクトリに分ける
 * [20210107] 2021.01.07
 * ・送信データにdevice_id,dateが含まれていない場合、永久に送信してしまう為、
 *   どちらもない場合は、削除するように修正
 * [20201225] 2020.12.25
 * ・デバイスＩＤのタグ名の誤りを修正
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * VirtualPowrPlantシステム
 * -----------------------------------------------------------------------------
 * モニタリングサーバ送信プロセス
 *
 * 主な機能
 * 		（１）通信機能
 * 		１分周期でシステムデファインに定義されたディレクトリに格納されている規程
 * 		の名称のファイルをIOT情報ファイル（IOTCNINF.dat）に登録されている情報に
 * 		従い、AWS-IoTを使用して送信する。
 *
 * 		（２）ファイル保存機能
 * 		AWS-IoTによる送信ができなかったファイルについて、一定期間保存する。
 * 		保存したファイルは送信契機の度に古いものから優先して送信を行う。
 * 		一定期間送信ができなかったファイルは破棄する。
 * 		保持期間はIOTCNINF.datファイルの設定に従う。（最長7日まで1時間単位で指定）
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvserial.h"
#include "pvjson.h"
//#include "pvctldata.h"

#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_DT1								/* デバッグフラグ：ＳＦＴＰ設定情報表示 */
#define	PV_DEBUG_DT2								/* デバッグフラグ： */
#define	PV_DEBUG_DT3								/* デバッグフラグ： */
#define	PV_BENCHMARK								/* 処理時間測定 */
#endif
#define	PV_BENCHMARK								/* 処理時間測定 */

#define PV_AWS_SUBSCRIBE_SKIP_FILENAME				"AWS_SUBSCRIBE_SKIP"		/* 外部データ */
#define	PV_OWN_NAME		"NKIOTSND"					/* 自プログラム名 */

#define	PV_FTP_FILES	64							/* 一度に送信する最大ファイル数 */
#define	PV_FTP_ERR_MAX	1180						/* 連続送信失敗時間（19分40秒） */
#define PV_FTP_FILES_STD	2						/* 一度に送信する標準ファイル数 */
#define PV_FTP_JSON_PARSE_LENGTH	64				/* jsonデータ解析文字数 */
#define PV_IOT_CONT_SEND_TIME	4					/* IOT連続送信時間（秒） */
#define	PV_IOT_FILES	1							/* 一度に送信する最大ファイル数 *//* [20211228] Chg */
#define	PV_STOP_SND_CNT		2						/* 停止時に送信する回数 */

struct st_PVSYSINF		*ps_g_PVSYSINF;				/* 管理ファイルポインタ */
struct st_IOTCNINF		*ps_g_IOTCNINF;				/* IoT設定情報ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;				/* システムログファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;				/* 装置状態状変マスク情報ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;				/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;				/* 状変蓄積ファイルポインタ */

													/* 使用ファイルインデックス */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_IOTCNINF, (void **)&ps_g_IOTCNINF, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
    {   PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
    {   PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
    {   PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0 }
};

static char				pv_own_name[128];			/* 自プロセス情報 */
static char				errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int				limitFiles = PV_FTP_FILES_STD;/* 一度に送信するファイル数 */
static time_t			sendChkTime;				/* 送信確認時間 */
static char				curdir[128];				/* カレントディレクトリPath */
static AWS_IoT_Client 			s_client;
static struct st_pvJsonParse	s_pvJsonparse;
static int						awsInitFlg=-1;
static char						s_sndTopicName[256];				/* トピック名 */
static char						s_SubscribeSkip = 0;				/* Subscribeスキップフラグ *//* [20211224] Add */

/* 内部関数定義 */
int	func_pvserver_ftpsend();
int	func_abnormal_exit( int );
int	func_delete_dir( char * );
int	func_send_file_check( char * );
int	func_JSON_dir_delete( void );
int	func_aws_init( void );
int	func_aws_quit( void );
int	func_aws_publish( void );
int	func_choice_iot_filename( const struct dirent * );
int	func_choice_dirname( const struct dirent * );
static void iotsnd_subscribe_callback_handler(AWS_IoT_Client *, char *, uint16_t , IoT_Publish_Message_Params *, void *);

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モニタリングサーバ送信処理
 *
 * 機能：
 * 		5秒周期に下記処理を行う
 * 		システムデファイン PV_PATH_IOT に定義されたディレクトリに格納されている
 * 		規程の名称のファイルをIOT設定情報ファイル（IOTCNINF.dat）に登録されている
 * 		情報に従い、ＡＷＳを使用して送信する。
 * 		規程ファイル名は 26文字の以下の名称ファイル
 * 		XXXXXXXXYYYYMMDDHHmm.DAT
 * 		　XXXXXXXX　：　収集装置ID名称（8文字）
 * 		　YYYY　　　：　西暦（4文字）
 * 		　MM　　　　：　月（2文字）
 * 		　DD　　　　：　日（2文字）
 * 		　HH　　　　：　時（2文字）
 * 		　mm　　　　：　分（2文字）
 * 		　ss　　　　：　秒（2文字）
 * 		　.DAT　　　：　拡張子（4文字）
 * 		ファイルが複数存在する場合は古いファイルを優先して送信する。
 * 		送信先のホストアドレスおよびポート番号、トピック名は、IOT設定情報ファイル
 * 		（IOTCNINF.dat）に登録されているものを使用する。
 *
 * 		（２）ファイル保存機能
 * 		ファイルは、送信結果に関わらず一定期間保存する。(送信に成功したファイルは
 * 		拡張子を.jsonに変更する）
 * 		未送信のファイルは送信契機の度に古いものから優先して送信を行う。
 * 		一定期間経過したファイルは破棄する。
 * 		保持期間はIOTCNINF.datファイルの設定に従う。（最長7日まで1時間単位で指定）
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int main(int argc, char *argv[])
{
	int						procNo;						/* プログラム番号 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						selectSvr = 0;				/* サーバ切替用 */
	int						stopFlag = 0;				/* 停止通知受信済みフラグ */
	int						aws_init=-1;
	time_t					nowTime;
	char					tmpFile1[128];				/* 一時ファイル名 */
	char					tmpFile2[128];				/* 一時ファイル名 */
	int						rebootFlag;					/* リブート（データ・プログラム更新あり）フラグ */
	char					command[512];				/* コマンド */
#ifdef PV_DEBUG
	struct timeval			time1;
	struct timeval			time2;
#endif
	char					filePath[128];			/* 外部データパス *//* [20211224] Add */

	/* プロセスの優先度を他の業務プロセスよりも低く設定する(nice = 5) */
	setpriority( PRIO_PROCESS, 0, 5 );

	/* プロセス起動時の引数からプログラム番号を取得 */
	if( argc != 2 )
	{
		/* 起動時の引数に間違いがある場合 */
		printf("Usage : %s procNo\n", argv[0]);
		return( -1 );
	}
	else
	{
		/* プログラム番号を取得 */
		procNo = (char)atoi(argv[1]);
		/* プログラム番号が既定値範囲外の場合 */
		if( (procNo < 11) || (procNo > PV_MAX_PROC) )
		{
			printf("Usage : %s procNo\n", argv[0]);
			return( -1 );
		}
	}

	/* 自プロセス情報を設定 */
	sprintf( pv_own_name, "%s(%02d)", PV_OWN_NAME, procNo );

	/* ファイルをオープンしてＭＭＡＰ化する */
	if( (retCode = commfunc_file_open( s_fileIndex )) < 0 )
	{
		printf("NKFTPSND : File Open Error\n");
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_SET );

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_RESET );

	/* 送信確認時刻を初期化 */
	sendChkTime = time( NULL );

	/* 外部データを取得 */
	sprintf(filePath, "%s%s", PV_PATH_CSV, PV_AWS_SUBSCRIBE_SKIP_FILENAME );	/* [20211224] Add */

	while( 1 )
	{
/* [20211224] Add St */
		/* 設定ファイルが存在しない場合 */
		if( access( filePath, R_OK ) != 0 )
		{
			if( s_SubscribeSkip == 1 )
			{
				sprintf( errMsg, "UPDATE : AWS Subscribe Check !!" );
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
			s_SubscribeSkip = 0;
		}
		else
		{
			if( s_SubscribeSkip == 0 )
			{
				sprintf( errMsg, "UPDATE : AWS Subscribe Skip !!" );
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
			s_SubscribeSkip = 1;
		}
/* [20211224] Add Ed */

		/* 3秒周期でアクセスする待機する */
		if( commfunc_wait( 3, procNo ) < 0 )	/* [20211228] Chg */
		{
#ifdef PV_DEBUG
			printf("NKFTPSND : PROCESS STOP COMMAND RECV (PROC:%d)\n", procNo );
#endif
			/* 停止通知受信済みフラグをインクリメント */
			stopFlag++;

			/* 停止チェックはウォッチドックのチェック(PV_WDOG_CYCLE秒間隔)で行っている為、ここでの待機は不要 */
			/* repoeditより起動した場合、ファイルが作成されておらず、次のwaitでも同じ秒の為、 */
			/* 即時にタイムアウトしてしまうので、必ず1秒待つ */
			sleep( 1 );
		}


#ifdef PV_DEBUG
		gettimeofday( &time1, NULL );
		printf("NKFTPSND : ST:%d.%06d\n",time1.tv_sec, time1.tv_usec );
#endif
		/* AWS送信処理 */
		func_aws_publish();

		/* JSONディレクトリのチェック */
		func_JSON_dir_delete();


#ifdef PV_DEBUG
		gettimeofday( &time2, NULL );
		printf("NKFTPSND : ED:%d.%06d\n",time2.tv_sec, time2.tv_usec );
#endif
		/* 停止通知受信済フラグが設定されている場合 */
		if( ( stopFlag > 0 ) &&
			( ( stopFlag >= PV_STOP_SND_CNT ) ||
			  ( retCode < 0 )) )
		{
			/* 処理を抜ける */
			break;
		}
	}

	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );

	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス異常終了処理
 *
 * 機能：
 * 		プロセスの異常終了をシステム監視ファイルに登録し、ファイルリソースを開放
 * 		する。
 * 戻り値：
 * 		0：正常処理
 */
int	func_abnormal_exit( int procNo )
{
		/* プロセス異常中フラグを設定 */
		commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_SET );
		/* プロセス動作中フラグを解除 */
		commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );
		/* ファイルをクローズする */
		commfunc_file_close( s_fileIndex );

		return( 0 );
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
int func_aws_init( void )
{
	char			rootCA[PATH_MAX + 1];
	char			clientCRT[PATH_MAX + 1];
	char			clientKey[PATH_MAX + 1];
	char 			clientid[PATH_MAX + 1];

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
	mqttInitParams.pHostURL = ps_g_IOTCNINF->hostAddress;
	mqttInitParams.port = ps_g_IOTCNINF->portNumb;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 10000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = NULL;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&s_client, &mqttInitParams);
	if(SUCCESS != rc) {
		sprintf(errMsg, "aws_iot_mqtt_init returned error : %d ", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return -1;
	}

	connectParams.keepAliveIntervalInSec = 60;		/* [20250110] Chg */
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	sprintf(clientid, "%s-snd", ps_g_IOTCNINF->deviceId);
	connectParams.pClientID = clientid;
	connectParams.clientIDLen = (uint16_t) strlen(clientid);
	connectParams.isWillMsgPresent = false;

	rc = aws_iot_mqtt_connect(&s_client, &connectParams);
	if(SUCCESS != rc) {
		sprintf(errMsg, "aws_iot_mqtt_connect : Error(%d)", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		rc = aws_iot_mqtt_free( &s_client );
		return -1;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&s_client, true);
	if(SUCCESS != rc) {
		sprintf(errMsg, "aws_iot_mqtt_autoreconnect_set_status : Unable to set Auto Reconnect to true - %d", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		rc = aws_iot_mqtt_disconnect( &s_client );
		rc = aws_iot_mqtt_free( &s_client );
		return -1;
	}

	sprintf(s_sndTopicName, "%s", ps_g_IOTCNINF->sndTopicName);
	rc = aws_iot_mqtt_subscribe(&s_client, s_sndTopicName, strlen(s_sndTopicName), QOS0, iotsnd_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		sprintf(errMsg, "aws_iot_mqtt_subscribe : Error subscribing : %d\n", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		rc = aws_iot_mqtt_disconnect( &s_client );
		rc = aws_iot_mqtt_free( &s_client );
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
int func_aws_quit( void )
{
	IoT_Error_t rc = FAILURE;

	/* 実績用トピックが無効な場合は、何もしない */
	if( strlen( ps_g_IOTCNINF->sndTopicName ) == 0 )
	{
		return SUCCESS;
	}

	/* 起動していない場合は、何もしない*/
	if( awsInitFlg != 0 )
	{
		return SUCCESS;
	}

	commfunc_json_free( &s_pvJsonparse );

	rc = aws_iot_mqtt_disconnect( &s_client );
	if(SUCCESS != rc) {
		sprintf(errMsg, "aws_iot_mqtt_disconnect : Unable to set Auto Reconnect to true - %d", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return rc;
	}

	rc = aws_iot_mqtt_free( &s_client );
	if(SUCCESS != rc) {
		sprintf(errMsg, "aws_iot_mqtt_free : Unable to set Auto Reconnect to true - %d", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return rc;
	}

	return SUCCESS;
}

static void iotsnd_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData)
{
	char					value[PV_JSON_DATALEN_MAX];
	char					tag[PV_JSON_DATALEN_MAX];
	int						retCode = 0;
	int						valueCount;
	char					tmpBuff[128];				/* 一時バッファ */
	char					tmpFile1[128];				/* 一時ファイル名 */
	char					tmpFile2[128];				/* 一時ファイル名 */
	char					errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	char					prefix[8];					/* prefix */

/* [20211224] Add St */
	if( s_SubscribeSkip == 1 )
	{
		/* Subscribeによる送信チェックをスキップする */
		return;
	}
/* [20211224] Add Ed */


#ifdef PV_DEBUG_DT2
	printf("%s : Subscribe callback\n", PV_OWN_NAME );
#endif
	//printf("nkftpsnd : Subscribe callback\n");
	//printf("Subscribe callback\n");
	//printf("%.*s\t%.*s\n", topicNameLen, topicName, (int) params->payloadLen, (char *) params->payload);

	/* 受診データがAWSライブラリの受信バッファを超える場合 */
	if( params->payloadLen > AWS_IOT_MQTT_RX_BUF_LEN )
	{
		sprintf(errMsg, "iotsnd_subscribe_callback_handler data size over (%d > %d).", params->payloadLen, AWS_IOT_MQTT_RX_BUF_LEN);
		commfunc_syslog_msgset( pv_own_name, errMsg );

		/* 装置状態状変：設定異常：発生 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);

	retCode = commfunc_json_parse(&s_pvJsonparse, 128, (int)params->payloadLen, (char *) params->payload, errmsg );


	/* 処理結果 */
	if( retCode != PV_JSON_RET_OK )
	{
		commfunc_json_free( &s_pvJsonparse );
		return;
	}

	while(1)
	{
		/* deviceid取得 */
		sprintf(tag, "device_id");			/* [20201225] Chg */
		retCode = commfunc_json_getValue(&s_pvJsonparse, &valueCount, tag, value );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(errmsg, "[%s] is not found.", tag);
			commfunc_syslog_msgset( pv_own_name, errmsg );
			retCode = PV_JSON_ERR_FORMAT;
			break;
		}
		else
		{
			/* デバイスIDが不一致の場合 */
			if( strcmp( ps_g_IOTCNINF->deviceId, value ) != 0 )
			{
				sprintf(errmsg, "[%s] does not match (%s!=%s).", tag, ps_g_IOTCNINF->deviceId, value);
				commfunc_syslog_msgset( pv_own_name, errmsg );
				retCode = PV_JSON_ERR_PARAM;
				break;
			}
		}

		/* message_type取得 */
		sprintf(tag, "message_type");
		retCode = commfunc_json_getValue(&s_pvJsonparse, &valueCount, tag, value );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(errmsg, "[%s] is not found.", tag);
			commfunc_syslog_msgset( pv_own_name, errmsg );
			retCode = PV_JSON_ERR_FORMAT;
			break;
		}
		/* 記録データの場合 */
		if( strncmp( value, "tm_record", sizeof("tm_record") ) == 0 )
		{
			sprintf( prefix, "%s", PV_AWS_IOT_FILE_PREFIX_TMREC);
		}
		else if( strncmp( value, "sv_record", sizeof("sv_record") ) == 0 )
		{
			sprintf( prefix, "%s", PV_AWS_IOT_FILE_PREFIX_SVREC);
		}
		else if( strncmp( value, "measure", sizeof("measure") ) == 0 )
		{
			sprintf( prefix, "%s", PV_AWS_IOT_FILE_PREFIX);
		}
		else
		{
			sprintf(errmsg, "[%s] is not supported(%s).", tag, value);
			commfunc_syslog_msgset( pv_own_name, errmsg );
			retCode = PV_JSON_ERR_FORMAT;
			break;
		}
		

		/* date取得 */
		sprintf(tag, "date");
		retCode = commfunc_json_getValue(&s_pvJsonparse, &valueCount, tag, value );
		if( retCode != PV_JSON_RET_OK )
		{
			sprintf(errmsg, "[%s] is not found.", tag);
			commfunc_syslog_msgset( pv_own_name, errmsg );
			retCode = PV_JSON_ERR_FORMAT;
		}
		else
		{
			//0123456789012345678
			//2019-06-14 15:51:00
			memset( tmpBuff, 0, sizeof(tmpBuff));

			/* dateからファイル名を取得 */
			value[4] = 0;
			sprintf( tmpBuff, "%s", &value[0]);
			value[7] = 0;
			sprintf( &tmpBuff[strlen(tmpBuff)], "%s", &value[5]);
			value[10] = 0;
			sprintf( &tmpBuff[strlen(tmpBuff)], "%s", &value[8]);
			value[13] = 0;
			sprintf( &tmpBuff[strlen(tmpBuff)], "%s", &value[11]);
			value[16] = 0;
			sprintf( &tmpBuff[strlen(tmpBuff)], "%s", &value[14]);
			value[19] = 0;
			sprintf( &tmpBuff[strlen(tmpBuff)], "%s", &value[17]);
			strcat( tmpBuff, "_" );
			strcat( tmpBuff, prefix );

#ifdef PV_DEBUG_DT2
			printf("%s : iotsnd_subscribe_callback_handler [%s.DAT]\n", PV_OWN_NAME, tmpBuff );
#endif

/* [20210917] Chg St */
			/* 送信完了ファイル名を生成 */
			sprintf( tmpFile1, "%s_%s.DAT", ps_g_IOTCNINF->deviceId, tmpBuff );

			/* 送信完了ファイルチェック */
			func_send_file_check( tmpFile1 );

/* [20210917] Chg Ed */
		}
		break;
	}

	commfunc_json_free( &s_pvJsonparse );

	return;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * AWS-IoTデータ送信処理
 *
 * 機能：
 * 		AWS-IoTの初期化処理を行う。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int func_aws_publish( void )
{
	struct dirent				**fileList1;				/* ディレクトリ内ファイルリスト用ポインタ */
	struct dirent				**fileList2;				/* ディレクトリリスト用ポインタ */
	int							retCode1;					/* 関数戻り値 */
	int							retCode2;					/* 関数戻り値 */
	char						datFilePath[1024];			/* DATファイルパス（通信ステータス情報） */
	char						readBuff[128];				/* ファイル名読み込みバッファ */
	FILE						*fp;						/* ファイルポインタ */
	struct stat					statBuff;					/* ファイル状態 */
	char 						*pc_payload=NULL;			/* 送信データバッファ */
	int							payloadLen=0;
	time_t						timeSec = 0;				/* 積算秒 */
	struct tm					timeData;
	IoT_Error_t 				rc = SUCCESS;
	IoT_Publish_Message_Params 	paramsQOS;
	int							aws_init=-1;
	time_t						nowTime = 0;				/* 積算秒 */
	int							loopCnt1;					/* ループカウンタ */
	int							loopCnt2;					/* ループカウンタ */
	int							svStat;
/* [20210107] Add St */
	int							deviceIdFlg;				/* デバイスＩＤ検出 */
	int							dateFlg;					/* 時刻情報検出 */
/* [20210107] Add Ed */
	int							sendStopFlg=1;				/* 送信終了フラグ */
	int							sendDataNum;				/* 送信データ数 */
	char						today[16];					/* 確認用ファイル名 */
	struct timeval 				startTime;					/* 開始時間 */
	struct timeval 				endTime;					/* 終了時間 */


#ifdef PV_DEBUG_DT2
	printf("%s : func_aws_publish : START\n", PV_OWN_NAME );
#endif

	/* 現在時刻を取得 */
	timeSec = time( NULL );

	/* 実績用トピックが無効な場合 */
	if( strlen( ps_g_IOTCNINF->sndTopicName ) == 0 )
	{
		return 0;
	}

	if( awsInitFlg != 0 )
	{
		/* AWS 初期化処理 */
		awsInitFlg = func_aws_init();
		if( awsInitFlg == 0 )
		{
			/* 装置状態状変：IOT通信異常(送信)：復帰 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
		else
		{
			/* 装置状態状変：IOT通信異常(送信)：発生 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
			rc = FAILURE;;
		}
	}

	if( rc == SUCCESS )
	{
		rc = aws_iot_mqtt_yield(&s_client, 500);

		/* 切断された場合 */
		if( ( NETWORK_ATTEMPTING_RECONNECT == rc) ||
		    ( NETWORK_MANUALLY_DISCONNECTED == rc ) )
		{
			sprintf(errMsg, "aws_iot_mqtt_yield(1) is Error(%d).", rc);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			/* 装置状態状変：IOT通信異常(送信)：発生 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
		else if ( rc < SUCCESS)
		{
			if( rc != FAILURE )
			{
				sprintf(errMsg, "aws_iot_mqtt_yield(1) is Error(%d).", rc);
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}

			/* 装置状態状変：IOT通信異常(送信)：発生 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

			/* 再接続以外の異常の場合は、一度終了する */
			func_aws_quit();
			awsInitFlg = -1;
		}
		else
		{
			/* 装置状態状変：IOT通信異常(送信)：復帰 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
			/* 送信終了フラグをOFF */
			sendStopFlg = 0;
		}
	}


	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	/* （一覧作成時に func_choice_dirname フィルタを使用し、アルファベット順にソートする） */
	retCode1 = scandir( PV_PATH_IOT, &fileList1, func_choice_dirname, alphasort );
	if( retCode1 < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "func_aws_publish : IOT Send Dir List Get Error" );
		return( -1 );
	}
	/* 処理対象ファイルが存在する場合 */
	if( retCode1 > 0 )
	{
		sendDataNum = 0;

		/* ディレクトリ数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < retCode1 ; loopCnt1++ )
		{
			/* 送信用ファイルディレクトリを取得する */
			memset(curdir, 0, sizeof(curdir));
			strcpy( curdir, PV_PATH_IOT );
			strcat( curdir, fileList1[loopCnt1]->d_name);
			strcat( curdir, "/");

			/* IOT送信用ファイルディレクトリ内のファイル一覧を取得する */
			/* （一覧作成時に func_choice_iot_filename フィルタを使用し、アルファベット順にソートする） */
			retCode2 = scandir( curdir, &fileList2, func_choice_iot_filename, alphasort );
			if( retCode2 < 0)
			{
				commfunc_syslog_msgset( pv_own_name, "IOT Send File List Get Error" );
				/* 送信終了フラグをONにする */
				sendStopFlg = 1;
			}
			/* 処理対象ファイルが存在しない場合 */
			if( retCode2 == 0 )
			{
				/* 現在時刻を取得 */
				nowTime = time( NULL );
				localtime_r( &nowTime, &timeData );
				/* 現在日時のフォルダ名を生成 */
				sprintf( today, "%04d%02d%02d", timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday);

				/* 現在日以外の場合は、削除 */
				if( strncmp( fileList1[loopCnt1]->d_name, today, 8 ) != 0 )
				{
					/* 対象ディレクトリを削除する */
					func_delete_dir( curdir );
				}
			}
			/* 処理対象ファイルが存在する場合 */
			else if( retCode2 > 0 )
			{
				/* ファイル数分繰り返す */
				for( loopCnt2 = 0 ; loopCnt2 < retCode2; loopCnt2++ )
				{
					/* 送信データサイズの初期化 */
					payloadLen = 0;

					/* 現在時刻を取得 */
					nowTime = time( NULL );
					localtime_r( &nowTime, &timeData );

					/* 設定異常チェック */
					commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, &svStat, ps_g_SVSTSFIL);

					/* 送信可能時刻範囲内の場合、且つ、設定異常がOFFの場合、且つ、1度に送信するファイル数の場合、且つ、送信終了フラグがOFFの場合 */
					if( ( nowTime < ( timeSec + PV_IOT_CONT_SEND_TIME ) ) &&
						( svStat == 0 ) &&
						( sendDataNum < PV_IOT_FILES ) &&
						( sendStopFlg == 0 ) )
					{
#ifdef PV_DEBUG_DT2
						printf("%s : IOT(%d/%d) FILE [ %s ] send try.\n", PV_OWN_NAME, loopCnt2+1, retCode2, fileList2[loopCnt2]->d_name );
#endif
						/* 送信データ数をインクリメント */
						sendDataNum++;

						/* ファイルパスの展開 */
						strcpy(datFilePath, curdir );
						strcat(datFilePath, fileList2[loopCnt2]->d_name);

						/* ファイルサイズを取得する */
				    	if (stat(datFilePath, &statBuff) == 0)
				    	{
							/* 動的メモリを確保する */
							pc_payload = (char *)malloc( statBuff.st_size+64 );
							memset(pc_payload, 0, statBuff.st_size);
#ifdef PV_DEBUG_DT2
//								printf("filesize=%d\n", statBuff.st_size );
#endif
						}

						/* DATファイルオープン */
						if( (fp = fopen(datFilePath, "r")) != NULL)
						{
/* [20210107] Add St */
							deviceIdFlg = 0;
							dateFlg = 0;
/* [20210107] Add Ed */
							
							/* DATファイルの終端まで繰り返す */
							while( fgets( readBuff, 128, fp) != NULL )
							{
#ifdef PV_DEBUG_DT2
//									printf("%s : fgets[%s]\n", PV_OWN_NAME, readBuff );
#endif
								/* ファイル内容が[timestamp]の場合 */
								if( strncmp( readBuff, "\"timestamp\":", 12 ) == 0 )
								{
									/* 現在時刻に書き換える */
									sprintf( &pc_payload[strlen(pc_payload)], "\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d\",\n", timeData.tm_year+1900, timeData.tm_mon+1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec );
								}
								else
								{
									strcat( pc_payload, readBuff);
								}
/* [20210107] Add St */
								/* ファイル内容が[device_id]の場合 */
								if( strncmp( readBuff, "\"device_id\":", 12 ) == 0 )
								{
									deviceIdFlg = 1;
								}
								/* ファイル内容が[date]の場合 */
								else if( strncmp( readBuff, "\"date\":", 7 ) == 0 )
								{
									dateFlg = 1;
								}
/* [20210107] Add Ed */
							}
							/* CSVファイルをクローズ */
							fclose( fp );

/* [20210107] Add St */
							/* device_idとdateがない場合、永久に送信してしまうため、どちらもない場合は削除する */
							if( ( deviceIdFlg == 0 ) || ( dateFlg == 0 ) )
							{
								unlink(datFilePath);
								sprintf( errMsg, "Tag(device_id,date) is Not Found[%s]. delete.", datFilePath );
								commfunc_syslog_msgset( pv_own_name, errMsg );
								payloadLen = 0;
							}
							else
							{
/* [20210107] Add Ed */
								/* データサイズを取得 */
								payloadLen = strlen(pc_payload);
							}
						}
						else
						{
							sprintf( errMsg, "File[%s] Not Found", datFilePath );
							commfunc_syslog_msgset( pv_own_name, errMsg );
						}

						/* 送信データがある場合 */
						if( ( pc_payload != NULL ) &&
						    ( payloadLen > 0 ) )
						{
#ifdef PV_DEBUG_DT2
//								printf("payloadLen=%d, pc_payload=[%s]\n", payloadLen, pc_payload);
#endif
#ifdef PV_BENCHMARK
							gettimeofday(&startTime, NULL);
							sprintf(errMsg, "send [%s]. Size=%d", fileList2[loopCnt2]->d_name, statBuff.st_size);
							commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
							paramsQOS.qos = QOS0;
							paramsQOS.payload = (void *)pc_payload;
							paramsQOS.payloadLen = payloadLen;
							paramsQOS.isRetained = 0;
							rc = aws_iot_mqtt_publish(&s_client, s_sndTopicName, strlen(s_sndTopicName), &paramsQOS);
#ifdef PV_BENCHMARK
							gettimeofday(&endTime, NULL); 
							sprintf(errMsg, "aws_iot_mqtt_publish=%d, [%lf].", rc, (difftime(endTime.tv_sec, startTime.tv_sec)+(endTime.tv_usec - startTime.tv_usec)*1e-6));
							commfunc_syslog_msgset( pv_own_name, errMsg );
#endif	

							if(SUCCESS != rc)
							{
								sprintf(errMsg, "aws_iot_mqtt_publish is Error(%d).", rc);
								commfunc_syslog_msgset( pv_own_name, errMsg );
								/* 装置状態状変：IOT通信異常(送信)：発生 */
								commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

								if( rc < SUCCESS)
								{
									/* aws再起動 */
									func_aws_quit();
									awsInitFlg = -1;
								}
								/* 送信終了フラグをONにする */
								sendStopFlg = 1;
							}
							else
							{
								/* 装置状態状変：IOT通信異常(送信)：復帰 */
								commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTSNDERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

								/* publishしたデータを確認 */
								rc = aws_iot_mqtt_yield(&s_client, 700);

								if( ( SUCCESS != rc) && ( rc != FAILURE ) )
								{
									sprintf(errMsg, "aws_iot_mqtt_yield(2) is Error(%d).", rc);
									commfunc_syslog_msgset( pv_own_name, errMsg );
								}
/* [20211224] Add St */
								if( s_SubscribeSkip == 1 )
								{
									/* 送信完了ファイルチェック */
									func_send_file_check( fileList2[loopCnt2]->d_name );
								}
/* [20211224] Add Ed */


							}

							/* 送信データがAWSライブラリの送信バッファを超える場合 */
							if( payloadLen > AWS_IOT_MQTT_TX_BUF_LEN )
							{
								sprintf(errMsg, "aws_iot_mqtt_publish data size over (%d > %d).", payloadLen, AWS_IOT_MQTT_TX_BUF_LEN);
								commfunc_syslog_msgset( pv_own_name, errMsg );

								/* 装置状態状変：設定異常：発生 */
								commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
							}

						}
						if( pc_payload != NULL )
						{
							free( pc_payload );
							pc_payload = NULL;
						}
					}
					else
					{
#ifdef PV_DEBUG_DT2
					printf("%s : IOT(%d/%d) FILE [ %s ]\n", PV_OWN_NAME, loopCnt2+1, retCode2, fileList2[loopCnt2]->d_name );
#endif
						sendStopFlg = 1;
					}

					/* リソースを開放 */
					free( fileList2[ loopCnt2 ]);
				}
			}
			/* リソースを開放 */
			free( fileList2 );
			/* リソースを開放 */
			free( fileList1[loopCnt1] );
		}
	}
	/* リソースを開放 */
	free( fileList1 );

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（送信データ用）
 *
 * 機能：
 * 		func_pvserver_ftpsend()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名から収集装置ＩＤの一致するものや、フ
 * 		ァイル名が正規のものだけを処理する。
 * 		ファイル名から一定期間経過したものかを判断し、一定期間を経過したものは
 * 		削除を行う。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_iot_filename( const struct dirent *dir )
{
	time_t					secData;					/* 積算秒 */
	struct tm				timeData;					/* 時刻情報 */
	char					checkNameLast[256];		/* 確認用ファイル名 */
	char					checkNameNext[256];		/* 確認用ファイル名 */
	char					checkName3_iot[256];		/* 確認用ファイル名 */
	char					checkName1_rec[256];		/* 確認用ファイル名 */
	char					checkName2_rec[256];		/* 確認用ファイル名 */
	char					checkName3_rec[256];		/* 確認用ファイル名 */
	char					deleteName[256];			/* 削除ファイル名 */
	char					prefix_pos;					/* prefix位置 */

	/* prefix位置を算出    収集装置ＩＤ           '_' 年月日時分秒   '_'*/
	prefix_pos = strlen(ps_g_IOTCNINF->deviceId) + 1 + 4+2+2+2+2+2 + 1;
	
	/* 現在時刻（積算秒）の取得 */
	secData = time( NULL );
	/* 現在時刻を年月日時分を取得 */
	localtime_r( &secData, &timeData );

	/* 比較用ファイル名(未来時刻)を生成 */
	sprintf( checkNameNext, "%s_%04d%02d%02d%02d%02d%02d",
		ps_g_IOTCNINF->deviceId,
		timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec);

	/* 現在時刻に保持期間を反映 */
	secData -= ps_g_IOTCNINF->stockHour * 3600;
	/* 保持期間の時刻を年月日時分を取得 */
	localtime_r( &secData, &timeData );

	/* 比較用ファイル名(保持時刻)を生成 */
	sprintf( checkNameLast, "%s_%04d%02d%02d%02d%02d%02d",
		ps_g_IOTCNINF->deviceId,
		timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec);

#ifdef PV_DEBUG_DT2
//	printf("%s : checkName1[%s], checkName2[%s]\n", PV_OWN_NAME, checkName1, checkName2 );
#endif

	/* IOTファイル名の最後の４文字が .DATの場合は処理対象 */
	if( strncmp( &dir->d_name[strlen(dir->d_name)-4], ".DAT", 4 ) == 0 )
	{

		/* PREFIXが一致しない場合は、除外 */
		if( ( strncmp( &dir->d_name[prefix_pos], PV_AWS_IOT_FILE_PREFIX, strlen(PV_AWS_IOT_FILE_PREFIX) ) != 0 ) && 
			( strncmp( &dir->d_name[prefix_pos], PV_AWS_IOT_FILE_PREFIX_TMREC, strlen(PV_AWS_IOT_FILE_PREFIX_TMREC) ) != 0 ) && 
			( strncmp( &dir->d_name[prefix_pos], PV_AWS_IOT_FILE_PREFIX_SVREC, strlen(PV_AWS_IOT_FILE_PREFIX_SVREC) ) != 0 ) )
		{
#ifdef PV_DEBUG_DT2
			printf("%s : FILE PREFIX ERROR : ( %s )\n", PV_OWN_NAME, dir->d_name );
#endif
		}

		/* IOTファイル名の先頭８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
		if( strncmp( dir->d_name, ps_g_IOTCNINF->deviceId, 8 ) != 0 )
		{
#ifdef PV_DEBUG_DT2
			printf("%s : CLIENT ID ERROR : ( %s )\n", PV_OWN_NAME, dir->d_name );
#endif
			return( 0 );
		}

		strcpy( deleteName, PV_PATH_IOT );
		strncat( deleteName, &dir->d_name[9], 8);
		strcat( deleteName, "/");

		/* 保存期間を超過したファイルは処理対象外 */
		if( strncmp( dir->d_name, checkNameLast, strlen(checkNameLast) ) < 0 )
		{
			sprintf(errMsg, "STOCK OUT FILE [ %s ] < [ %s ]", dir->d_name, checkNameLast );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#ifdef PV_DEBUG_DT2
			printf("%s : %s\n", PV_OWN_NAME, errMsg );
#endif
			/* 保存期間を超過したファイルを削除 */
			strcat( deleteName, dir->d_name);
			unlink( deleteName );
			return( 0 );
		}

		/* 現在時刻より未来のファイルは削除対象 */
		if( strncmp( dir->d_name, checkNameNext, strlen(checkNameNext) ) > 0 )
		{
			sprintf(errMsg, "FUTURE FILE [ %s ] > [ %s ]", dir->d_name, checkNameNext );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#ifdef PV_DEBUG_DT2
			printf("%s : %s\n", PV_OWN_NAME, errMsg );
#endif
			/* 保存期間を超過したファイルを削除 */
			strcat( deleteName, dir->d_name);
			unlink( deleteName );
			return( 0 );
		}

	}
	else
	{
#ifdef PV_DEBUG_DT2
		printf("%s : FILE NAME ERROR : ( %s )\n", PV_OWN_NAME, dir->d_name );
#endif
		return( 0 );
	}

	/* 処理対象 */
	return( 1 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（送信データ用）
 *
 * 機能：
 * 		func_pvserver_ftpsend()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるフォルダ名から送信対象のファイルの有無を判断し、
 * 		ファイルがあるものだけを処理する。
 * 		フォルダの日付が一定期間経過したものかを判断し、
 * 		一定期間を経過したフォルダはフォルダ内のファイルを含め削除を行う。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_dirname( const struct dirent *dir )
{
	int						loopCnt1;					/* ループカウンタ */

	/* 名前が9文字以外（年月日＝８）以外の場合は除外	*/
	if( strnlen( dir->d_name, 9 ) != 8 )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : DIR NAME ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 拡張子がある場合は除外 */
	if( strstr( dir->d_name, "." ) != NULL )
	{
#ifdef PV_DEBUG_DT1
		printf("NKFTPSND : EXTENSION EXIST ERROR : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* ディレクトリ名が数値以外の場合は除外 */
	for( loopCnt1 = 0; loopCnt1 < strlen(dir->d_name); loopCnt1++ )
	{
		if( ( dir->d_name[loopCnt1] < 0x30 ) ||
			( dir->d_name[loopCnt1] > 0x39 ) )
		{
			return( 0 );
		}
	}

	/* 処理対象 */
	return( 1 );
}



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ディレクトリ削除処理
 *
 * 機能：
 * 		引数のディレクトリ内のファイルを削除し、ディレクトリも削除する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_delete_dir( char *dirPath )
{
	struct dirent			**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int						retCode;					/* 関数戻り値 */
	int						loopCnt1;					/* ループカウンタ */
	char					delfile[256];				/* 一時ファイル名 */

	retCode = scandir( dirPath, &fileList, NULL, alphasort );
	if( retCode < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "func_delete_dir : Dir List Get Error" );
		return( 0 );
	}

	/* 対象ファイルが存在する場合 */
	if( retCode > 0 )
	{
		/* ファイル数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < retCode ; loopCnt1++ )
		{
			/* カレントディレクトリ「.」は、削除対象外 */
			/* 親ディレクトリ「..」は、削除対象外 */
			if( (strcmp(".", fileList[ loopCnt1 ]->d_name) == 0) ||
				(strcmp("..", fileList[ loopCnt1 ]->d_name) == 0))
        	{
				/* 対象外 */
			}
			else
			{
				/* ファイルの削除を実施  */
				strcpy( delfile, dirPath );
				strcat( delfile, fileList[ loopCnt1 ]->d_name);
				unlink( delfile );
#ifdef PV_DEBUG_DT1
				printf("NKFTPSND : DELETE FILE IN THE DELETE DIR [ %s ]\n", fileList[ loopCnt1 ]->d_name );
#endif
			}

			/* リソースを開放 */
			free( fileList[ loopCnt1 ]);
		}
	}
	/* リソースを開放 */
	free( fileList );

#ifdef PV_DEBUG_DT1
	printf("NKFTPSND : DELETE DIR [ %s ]\n", dirPath );
#endif
	/* ディレクトリの削除を実施 */
	rmdir( dirPath );

	return( 0 );
	
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 送信完了ファイルチェック
 *
 * 機能：
 * 		引数のファイルを検索し、PV_PATH_IOTからPV_PATH_JSONに移動する
 * 戻り値：
 * 		0：正常処理
 */
int	func_send_file_check( char *fileName )
{
	struct dirent				**fileList1;				/* ディレクトリ内ファイルリスト用ポインタ */
	struct dirent				**fileList2;				/* ディレクトリリスト用ポインタ */
	int							retCode1;					/* 関数戻り値 */
	int							retCode2;					/* 関数戻り値 */
	int							loopCnt1;					/* ループカウンタ */
	int							loopCnt2;					/* ループカウンタ */
	char						dirName[128];				/* 確認用ディレクトリ名 */
	char						tmpFile1[128];				/* 一時ファイル名 */
	char						tmpFile2[128];				/* 一時ファイル名 */
	int							foundFlag = 0;				/* 検出フラグ */

	memset( dirName, 0, sizeof(dirName));
	/* ファイル名から日付を取得 */
	memcpy( dirName, &fileName[9], 8 );

	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	/* （一覧作成時に func_choice_dirname フィルタを使用し、アルファベット順にソートする） */
	retCode1 = scandir( PV_PATH_IOT, &fileList1, func_choice_dirname, alphasort );
	if( retCode1 < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "func_send_file_check : IOT Send Dir List Get Error" );
		return( -1 );
	}
	/* 処理対象ディレクトリが存在する場合 */
	if( retCode1 > 0 )
	{
		/* ディレクトリ数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < retCode1 ; loopCnt1++ )
		{
			/* ディレクトリが一致する場合、且つ、未検出の場合 */
			if( ( strncmp( fileList1[loopCnt1]->d_name, dirName, 8 ) == 0 ) &&
				( foundFlag == 0 ) )
			{
				/* 送信用ファイルディレクトリを取得する */
				memset(curdir, 0, sizeof(curdir));
				strcpy( curdir, PV_PATH_IOT );
				strcat( curdir, fileList1[loopCnt1]->d_name);
				strcat( curdir, "/");

				/* IOT送信用ファイルディレクトリ内のファイル一覧を取得する */
				/* （一覧作成時に func_choice_iot_filename フィルタを使用し、アルファベット順にソートする） */
				retCode2 = scandir( curdir, &fileList2, func_choice_iot_filename, alphasort );
				if( retCode2 < 0)
				{
					sprintf( errMsg, "Send File Check. directory[%s] File List Get Error", curdir );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
				/* 処理対象ファイルが存在する場合 */
				else if( retCode2 > 0 )
				{
					/* ファイル数分繰り返す */
					for( loopCnt2 = 0 ; loopCnt2 < retCode2; loopCnt2++ )
					{
						/* ファイルが一致する場合、且つ、未検出の場合 */
						if( ( strncmp( fileList2[loopCnt2]->d_name, fileName, 8 ) == 0 ) &&
							( foundFlag == 0 ) )
						{
							/* 検出フラグをONに設定 */
							foundFlag = 1;

							/* 変更前ファイルパスを設定 */
							sprintf( tmpFile1, "%s%s", curdir, fileName );
							
							/* 変更後ファイルパスを設定 */
							strcpy( tmpFile2, PV_PATH_JSON );
							strncat( tmpFile2, dirName, 8 );
							strcat( tmpFile2, "/");
							mkdir( tmpFile2, ( S_IRWXG | S_IRWXU ));	/* 所有者、グループの権限付与 */
							strncat( tmpFile2, fileName, (strlen(fileName)-3));
							strcat( tmpFile2, "json");

							sprintf(errMsg, "sent [%s].", fileName);
							commfunc_syslog_msgset( pv_own_name, errMsg );

							/* ファイルがある場合 */
							if( access( tmpFile1, R_OK | W_OK ) == 0 )
							{
								rename( tmpFile1, tmpFile2 );
#ifdef PV_DEBUG_DT2
								printf("%s : rename [%s]->[%s]\n", PV_OWN_NAME, tmpFile1, tmpFile2 );
#endif
							}
						}
						/* リソースを開放 */
						free( fileList2[ loopCnt2 ]);
					}
				}
				/* リソースを開放 */
				free( fileList2 );
			}
			/* リソースを開放 */
			free( fileList1[loopCnt1] );
		}
	}
	/* リソースを開放 */
	free( fileList1 );

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * JSONファイル削除チェック
 *
 * 機能：
 * 		PV_PATH_JSON内のディレクトリが一定期間経過したものかを判断し、
 * 		一定期間を経過したフォルダはフォルダ内のファイルを含め削除を行う。
 * 戻り値：
 * 		0：正常処理
 * 		1：異常処理
 */
int	func_JSON_dir_delete( void )
{
	int						loopCnt1;					/* ループカウンタ */
	time_t					secData;					/* 積算秒 */
	struct tm				timeData;					/* 時刻情報 */
	char					checkName[256];				/* 確認用ファイル名 */
	struct dirent			**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	char					deldir[256];				/* 一時ディレクトリ名*/
	int						retCode;					/* 関数戻り値 */

	/* 現在時刻（積算秒）の取得 */
	secData = time( NULL );
	/* 現在時刻に保持期間を反映 */
	secData -=  ps_g_IOTCNINF->stockHour * 60 * 60;
	/* 保持期間の時刻を年月日時分を取得 */
	localtime_r( &secData, &timeData );
	/* 比較用フォルダ名を生成 */
	sprintf( checkName, "%04d%02d%02d", timeData.tm_year + 1900, timeData.tm_mon + 1, timeData.tm_mday);


	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	/* （一覧作成時に func_choice_dirname フィルタを使用し、アルファベット順にソートする） */
	retCode = scandir( PV_PATH_JSON, &fileList, func_choice_dirname, alphasort );
	if( retCode < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "func_JSON_dir_delete : IOT Send Dir List Get Error" );
		return( -1 );
	}
	/* 処理対象ディレクトリが存在する場合 */
	if( retCode > 0 )
	{
		/* ディレクトリ数分繰り返す */
		for( loopCnt1 = 0 ; loopCnt1 < retCode ; loopCnt1++ )
		{

			/* 保存期間(日付)を超過したディレクトリは削除対象 */
			if( strncmp( fileList[loopCnt1]->d_name, checkName, 8 ) < 0 )
			{
				strcpy( deldir, PV_PATH_JSON );
				strcat( deldir, fileList[ loopCnt1 ]->d_name);
				strcat( deldir, "/");
				func_delete_dir( deldir );

				sprintf(errMsg, "json dir [%s] delete.", deldir);
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
			/* リソースを開放 */
			free( fileList[ loopCnt1 ]);
		}
	}
	/* リソースを開放 */
	free( fileList );

	return( 0 );
}
