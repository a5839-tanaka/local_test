/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
 * @file aws_iot_config.h
 * @brief AWS IoT specific configuration file
 */

#ifndef AWS_IOT_SDK_SRC_IOT_CONFIG_H_
#define AWS_IOT_SDK_SRC_IOT_CONFIG_H_

// Get from console
// =================================================
#if 0
	#if 1
	#define AWS_IOT_MQTT_HOST              "a1cfzct9quodsi-ats.iot.ap-northeast-1.amazonaws.com" ///< Customer specific MQTT HOST. The same will be used for Thing Shadow
	#define AWS_IOT_MQTT_PORT              443 ///< default port for MQTT/S
	#define AWS_IOT_MQTT_CLIENT_ID         "A7-c-sdk-client-id" ///< MQTT client ID should be unique for every device
	#define AWS_IOT_MY_THING_NAME          "A7-AWS-IoT-C-SDK" ///< Thing Name of the Shadow this device is associated with
	#define AWS_IOT_ROOT_CA_FILENAME       "root-CA.crt" ///< Root CA file name
	#define AWS_IOT_CERTIFICATE_FILENAME   "a88cb6730d-certificate.pem.crt"
	#define AWS_IOT_PRIVATE_KEY_FILENAME   "a88cb6730d-private.pem.key"
	#else
	#define AWS_IOT_MQTT_HOST              "" ///< Customer specific MQTT HOST. The same will be used for Thing Shadow
	#define AWS_IOT_MQTT_PORT              443 ///< default port for MQTT/S
	#define AWS_IOT_MQTT_CLIENT_ID         "c-sdk-client-id" ///< MQTT client ID should be unique for every device
	#define AWS_IOT_MY_THING_NAME 		   "AWS-IoT-C-SDK" ///< Thing Name of the Shadow this device is associated with
	#define AWS_IOT_ROOT_CA_FILENAME       "rootCA.crt" ///< Root CA file name
	#define AWS_IOT_CERTIFICATE_FILENAME   "cert.pem" ///< device signed certificate file name
	#define AWS_IOT_PRIVATE_KEY_FILENAME   "privkey.pem" ///< Device private key filename
	#endif
#endif
// =================================================

/////// MQTT PubSub ///////
#define AWS_IOT_MQTT_TX_BUF_LEN 128*1024 ///< Any time a message is sent out through the MQTT layer. The message is copied into this buffer anytime a publish is done. This will also be used in the case of Thing Shadow.
										/*** メッセージがMQTT層を介して送信されるときはいつでも。 メッセージは、発行が行われるたびにこのバッファにコピーされます。 これはThing Shadowの場合にも使用されます。***/
#define AWS_IOT_MQTT_RX_BUF_LEN 128*1024 ///< Any message that comes into the device should be less than this buffer size. If a received message is bigger than this buffer size the message will be dropped.
										/*** デバイスに入ってくるメッセージはすべてこのバッファーサイズより小さくなければなりません。 受信したメッセージがこのバッファサイズより大きい場合、メッセージは破棄されます。 ***/
#define AWS_IOT_MQTT_NUM_SUBSCRIBE_HANDLERS 5 ///< Maximum number of topic filters the MQTT client can handle at any given time. This should be increased appropriately when using Thing Shadow
										/*** MQTTクライアントが常に処理できるトピックフィルターの最大数。 Thing Shadowを使用するときは、これを適切に増やす必要があります。 ***/

/////// Thing Shadow specific configs ///////
#define SHADOW_MAX_SIZE_OF_RX_BUFFER (AWS_IOT_MQTT_RX_BUF_LEN+1) ///< Maximum size of the SHADOW buffer to store the received Shadow message, including terminating NULL byte.
																/*** 受信したShadowメッセージを格納するためのSHADOWバッファの最大サイズ（終端NULLバイトを含む）。 ***/
#define MAX_SIZE_OF_UNIQUE_CLIENT_ID_BYTES 80  ///< Maximum size of the Unique Client Id. For More info on the Client Id refer \ref response "Acknowledgments"
												/*** 一意のクライアントIDの最大サイズ。 クライアントIDの詳細については、\ ref response "Acknowledgements"を参照してください。 ***/
#define MAX_SIZE_CLIENT_ID_WITH_SEQUENCE MAX_SIZE_OF_UNIQUE_CLIENT_ID_BYTES + 10 ///< This is size of the extra sequence number that will be appended to the Unique client Id
																					/*** これは、Unique client Idに追加される追加のシーケンス番号のサイズです。 ***/
#define MAX_SIZE_CLIENT_TOKEN_CLIENT_SEQUENCE MAX_SIZE_CLIENT_ID_WITH_SEQUENCE + 20 ///< This is size of the the total clientToken key and value pair in the JSON
																					/*** これは、JSON内の合計clientTokenキーと値のペアのサイズです。 ***/
#define MAX_ACKS_TO_COMEIN_AT_ANY_GIVEN_TIME 10 ///< At Any given time we will wait for this many responses. This will correlate to the rate at which the shadow actions are requested
												/*** いつでも私たちはこのたくさんの反応を待ちます。 これは、シャドーアクションが要求される割合と相関します。 ***/
#define MAX_THINGNAME_HANDLED_AT_ANY_GIVEN_TIME 10 ///< We could perform shadow action on any thing Name and this is maximum Thing Names we can act on at any given time
													/*** 私たちはあらゆる事柄Nameに対してシャドーアクションを実行することができ、これは私たちがいつでも行動できる最大のThing Namesです。 ***/
#define MAX_JSON_TOKEN_EXPECTED 120 ///< These are the max tokens that is expected to be in the Shadow JSON document. Include the metadata that gets published
									/*** これらは、Shadow JSONドキュメントに含まれると予想される最大トークンです。 公開されるメタデータを含めます。 ***/
#define MAX_SHADOW_TOPIC_LENGTH_WITHOUT_THINGNAME 60 ///< All shadow actions have to be published or subscribed to a topic which is of the format $aws/things/{thingName}/shadow/update/accepted. This refers to the size of the topic without the Thing Name
													/*** すべてのシャドウアクションは、$ aws / things / {thingName} / shadow / update / acceptの形式のトピックにパブリッシュまたはサブスクライブする必要があります。 これは、Thing Nameを除いたトピックのサイズを表します。 ***/
#define MAX_SIZE_OF_THING_NAME 20 ///< The Thing Name should not be bigger than this value. Modify this if the Thing Name needs to be bigger
								/*** シングネームはこの値より大きくてはいけません。 Thing Nameを大きくする必要がある場合は、これを変更してください。 ***/
#define MAX_SHADOW_TOPIC_LENGTH_BYTES MAX_SHADOW_TOPIC_LENGTH_WITHOUT_THINGNAME + MAX_SIZE_OF_THING_NAME ///< This size includes the length of topic with Thing Name
																										/*** このサイズには、Thing Nameのトピックの長さが含まれています。 ***/

// Auto Reconnect specific config
#define AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL 10 ///< Minimum time before the First reconnect attempt is made as part of the exponential back-off algorithm
													/*** 指数関数的バックオフアルゴリズムの一部として最初の再接続が試行されるまでの最小時間。 ***/
#define AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL 60 ///< Maximum time interval after which exponential back-off will stop attempting to reconnect.
												/*** 指数関数的バックオフが再接続の試行を停止するまでの最大時間間隔。 ***/

#define DISABLE_METRICS false ///< Disable the collection of metrics by setting this to true
								/*** これをtrueに設定してメトリックの収集を無効にします。 ***/

#endif /* AWS_IOT_SDK_SRC_IOT_CONFIG_H_ */
