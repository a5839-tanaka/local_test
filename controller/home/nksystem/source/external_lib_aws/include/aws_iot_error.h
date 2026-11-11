/*
 * Copyright 2010-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file aws_iot_error.h
 * @brief Definition of error types for the SDK.
 */

#ifndef AWS_IOT_SDK_SRC_IOT_ERROR_H_
#define AWS_IOT_SDK_SRC_IOT_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Used to avoid warnings in case of unused parameters in function pointers */
#define IOT_UNUSED(x) (void)(x)

/*! \public
 * @brief IoT Error enum
 *
 * Enumeration of return values from the IoT_* functions within the SDK.
 * Values less than -1 are specific error codes
 * Value of -1 is a generic failure response
 * Value of 0 is a generic success response
 * Values greater than 0 are specific non-error return codes
 */
typedef enum {
	/*** ネットワーク物理層が接続されたときに返されます。***//** Returned when the Network physical layer is connected */
			NETWORK_PHYSICAL_LAYER_CONNECTED = 6,
	/*** ネットワークが手動で切断されたときに返されます。***//** Returned when the Network is manually disconnected */
			NETWORK_MANUALLY_DISCONNECTED = 5,
	/*** ネットワークが切断され、再接続が試行されているときに返されます。***//** Returned when the Network is disconnected and the reconnect attempt is in progress */
			NETWORK_ATTEMPTING_RECONNECT = 4,
	/*** 自動再接続が成功したことを示すyield関数の戻り値***//** Return value of yield function to indicate auto-reconnect was successful */
			NETWORK_RECONNECTED = 3,
	/*** TLSバッファで読み取りが試行され、それが空の場合に返されます。***//** Returned when a read attempt is made on the TLS buffer and it is empty */
			MQTT_NOTHING_TO_READ = 2,
	/*** 接続要求が成功し、パケット応答が接続受け入れ済みの場合に返されます。***//** Returned when a connection request is successful and packet response is connection accepted */
			MQTT_CONNACK_CONNECTION_ACCEPTED = 1,
	/*** 成功の戻り値 - エラーは発生しませんでした。***//** Success return value - no error occurred */
			SUCCESS = 0,
	/*** 一般的なエラーです。特定のエラーコードに対して十分な情報がありません。***//** A generic error. Not enough information for a specific error code */
			FAILURE = -1,
	/*** 必須パラメーターがヌルとして渡されました。***//** A required parameter was passed as null */
			NULL_VALUE_ERROR = -2,
	/*** TCPソケットを確立できませんでした。***//** The TCP socket could not be established */
			TCP_CONNECTION_ERROR = -3,
	/*** TLSハンドシェイクが失敗しました。***//** The TLS handshake failed */
			SSL_CONNECTION_ERROR = -4,
	/*** ソケットのパラメータ設定に関連するエラー。***//** Error associated with setting up the parameters of a Socket */
			TCP_SETUP_ERROR = -5,
	/*** TLSハンドシェイクが完了するのを待っている間にタイムアウトが発生しました。***//** A timeout occurred while waiting for the TLS handshake to complete. */
			NETWORK_SSL_CONNECT_TIMEOUT_ERROR = -6,
	/*** 使用されているプラットフォームに基づく一般的な書き込みエラー。***//** A Generic write error based on the platform used */
			NETWORK_SSL_WRITE_ERROR = -7,
	/*** TLS層でのSSL初期化エラー。***//** SSL initialization error at the TLS layer */
			NETWORK_SSL_INIT_ERROR = -8,
	/*** 証明書のロード中にエラーが発生しました。証明書が見つからないか、形式が正しくありません。***//** An error occurred when loading the certificates.  The certificates could not be located or are incorrectly formatted. */
			NETWORK_SSL_CERT_ERROR = -9,
	/*** SSL書き込みがタイムアウトしました。***//** SSL Write times out */
			NETWORK_SSL_WRITE_TIMEOUT_ERROR = -10,
	/*** SSL読み取りがタイムアウトしました。***//** SSL Read times out */
			NETWORK_SSL_READ_TIMEOUT_ERROR = -11,
	/*** 使用されているプラットフォームに基づく一般的なエラー。***//** A Generic error based on the platform used */
			NETWORK_SSL_READ_ERROR = -12,
	/*** ネットワークが切断され、再接続が無効になったか、物理層が切断された場合に返されます。***//** Returned when the Network is disconnected and reconnect is either disabled or physical layer is disconnected */
			NETWORK_DISCONNECTED_ERROR = -13,
	/*** ネットワークが切断され、再接続の試行がタイムアウトしたときに返されます。***//** Returned when the Network is disconnected and the reconnect attempt has timed out */
			NETWORK_RECONNECT_TIMED_OUT_ERROR = -14,
	/*** ネットワーク層乱数ジェネレータのシードが失敗しました。***//** Returned when the Network is already connected and a connection attempt is made */
			NETWORK_ALREADY_CONNECTED_ERROR = -15,

	/** Network layer Error Codes */
	/*** ネットワーク層乱数ジェネレータのシードが失敗しました。***//** Network layer Random number generator seeding failed */
			NETWORK_MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED = -16,
	/*** ネットワーク層エラーの一般的なエラーコード。***//** A generic error code for Network layer errors */
			NETWORK_SSL_UNKNOWN_ERROR = -17,
	/*** 物理層が切断されたときに返されます。***//** Returned when the physical layer is disconnected */
			NETWORK_PHYSICAL_LAYER_DISCONNECTED = -18,
	/*** ルート証明書が無効な場合に返されます。***//** Returned when the root certificate is invalid */
			NETWORK_X509_ROOT_CRT_PARSE_ERROR = -19,
	/*** デバイス証明書が無効な場合に返されます。***//** Returned when the device certificate is invalid */
			NETWORK_X509_DEVICE_CRT_PARSE_ERROR = -20,
	/*** 秘密鍵の解析に失敗したときに返されます。***//** Returned when the private key failed to parse */
			NETWORK_PK_PRIVATE_KEY_PARSE_ERROR = -21,
	/*** ネットワーク層がソケットを開けなかったときに返されます。***//** Returned when the network layer failed to open a socket */
			NETWORK_ERR_NET_SOCKET_FAILED = -22,
	/*** サーバーが不明な場合に返されます。***//** Returned when the server is unknown */
			NETWORK_ERR_NET_UNKNOWN_HOST = -23,
	/*** 接続要求が失敗したときに返されます。***//** Returned when connect request failed */
			NETWORK_ERR_NET_CONNECT_FAILED = -24,
	/*** TLS読み込みバッファに読み込むものが何もないときに返されます。***//** Returned when there is nothing to read in the TLS read buffer */
			NETWORK_SSL_NOTHING_TO_READ = -25,
	/*** 接続ができませんでした..***//** A connection could not be established. */
			MQTT_CONNECTION_ERROR = -26,
	/*** TLSハンドシェイクの完了を待っている間にタイムアウトが発生しました。***//** A timeout occurred while waiting for the TLS handshake to complete */
			MQTT_CONNECT_TIMEOUT_ERROR = -27,
	/*** TLS要求の完了を待っている間にタイムアウトが発生しました。***//** A timeout occurred while waiting for the TLS request complete */
			MQTT_REQUEST_TIMEOUT_ERROR = -28,
	/*** 現在のクライアントの状態が予想値と一致しません。***//** The current client state does not match the expected value */
			MQTT_UNEXPECTED_CLIENT_STATE_ERROR = -29,
	/*** 要求が行われているとき、クライアント状態はアイドルではありません。***//** The client state is not idle when request is being made */
			MQTT_CLIENT_NOT_IDLE_ERROR = -30,
	/*** MQTT RXバッファーが破損または予期しないメッセージを受け取りました。***//** The MQTT RX buffer received corrupt or unexpected message  */
			MQTT_RX_MESSAGE_PACKET_TYPE_INVALID_ERROR = -31,
	/*** MQTT RXバッファーがより大きなメッセージを受信しました。メッセージは破棄されます。***//** The MQTT RX buffer received a bigger message. The message will be dropped  */
			MQTT_RX_BUFFER_TOO_SHORT_ERROR = -32,
	/*** MQTT TXバッファーが発信メッセージに対して短すぎます。要求は失敗します。***//** The MQTT TX buffer is too short for the outgoing message. Request will fail  */
			MQTT_TX_BUFFER_TOO_SHORT_ERROR = -33,
	/*** クライアントは、可能な限り最大数のサブスクリプションにサブスクライブされています。***//** The client is subscribed to the maximum possible number of subscriptions  */
			MQTT_MAX_SUBSCRIPTIONS_REACHED_ERROR = -34,
	/*** 着信パケットの残りのパケット長をデコードできませんでした。***//** Failed to decode the remaining packet length on incoming packet */
			MQTT_DECODE_REMAINING_LENGTH_ERROR = -35,
	/*** 接続要求が失敗し、サーバーから不明なエラーが返されました。***//** Connect request failed with the server returning an unknown error */
			MQTT_CONNACK_UNKNOWN_ERROR = -36,
	/*** 接続要求が失敗し、サーバーから受け入れられないプロトコルバージョンエラーが返されました。***//** Connect request failed with the server returning an unacceptable protocol version error */
			MQTT_CONNACK_UNACCEPTABLE_PROTOCOL_VERSION_ERROR = -37,
	/*** 接続要求が失敗し、サーバーは識別子を拒否してエラーを返しました。***//** Connect request failed with the server returning an identifier rejected error */
			MQTT_CONNACK_IDENTIFIER_REJECTED_ERROR = -38,
	/*** 接続要求が失敗し、サーバーは利用できないエラーを返しました。***//** Connect request failed with the server returning an unavailable error */
			MQTT_CONNACK_SERVER_UNAVAILABLE_ERROR = -39,
	/*** 接続要求が失敗し、サーバーから不正なユーザーデータエラーが返されました。***//** Connect request failed with the server returning a bad userdata error */
			MQTT_CONNACK_BAD_USERDATA_ERROR = -40,
	/*** 接続要求が失敗し、サーバーは要求の認証に失敗しました。***//** Connect request failed with the server failing to authenticate the request */
			MQTT_CONNACK_NOT_AUTHORIZED_ERROR = -41,
	/*** JSON文字列の解析中にエラーが発生しました。通常不正なJSON ..***//** An error occurred while parsing the JSON string.  Usually malformed JSON. */
			JSON_PARSE_ERROR = -42,
	/*** 影：応答確認テーブルは現在、以前に発行された更新を完全に待っています。***//** Shadow: The response Ack table is currently full waiting for previously published updates */
			SHADOW_WAIT_FOR_PUBLISH = -43,
	/*** snprintfがサイズを超える値を書き込むたびに、このエラーが返されます。***//** Any time an snprintf writes more than size value, this error will be returned */
			SHADOW_JSON_BUFFER_TRUNCATED = -44,
	/*** snprintfがエンコードエラーを起こしたとき、または与えられたバッファに十分なスペースがないとき。***//** Any time an snprintf encounters an encoding error or not enough space in the given buffer */
			SHADOW_JSON_ERROR = -45,
	/*** ミューテックスの初期化に失敗しました。***//** Mutex initialization failed */
			MUTEX_INIT_ERROR = -46,
	/*** ミューテックスロック要求が失敗しました。***//** Mutex lock request failed */
			MUTEX_LOCK_ERROR = -47,
	/*** ミューテックスのロック解除要求が失敗しました。***//** Mutex unlock request failed */
			MUTEX_UNLOCK_ERROR = -48,
	/*** ミューテックス破壊は失敗しました。***//** Mutex destroy failed */
			MUTEX_DESTROY_ERROR = -49,
	/*** 入力引数が許容最大サイズを超えました。***//** Input argument exceeded the allowed maximum size */
			MAX_SIZE_ERROR = -50,
	/*** いくつかの制限を超えています。サブスクリプションの最大数に達しました。***//** Some limit has been exceeded, e.g. the maximum number of subscriptions has been reached */
			LIMIT_EXCEEDED_ERROR = -51,
	/*** 入力トピックタイプが無効です。***//** Invalid input topic type */
			INVALID_TOPIC_TYPE_ERROR = -52
} IoT_Error_t;

#ifdef __cplusplus
}
#endif

#endif /* AWS_IOT_SDK_SRC_IOT_ERROR_H_ */
