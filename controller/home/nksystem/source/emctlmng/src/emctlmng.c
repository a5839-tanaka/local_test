/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * ・周波数判定タイミングを000ms時にのみ実施するように変更(実績データとの整合性を合わせる為）
 *   ※模擬周波数では実施しない
 * [20260514] 2026.05.14
 * ・一次調整力の系統周波数を平均値を算出し使用するように修正
 * [20260513] 2026.05.13
 * ・一次調整力の指令値を算出する際に充電可能電力をチェックし、補正するように修正
 * ・20251031の周波数異常時動作は下限側のみで行う変更の対処漏れ
 * [20260512] 2026.05.12
 * ・一次調整力異常動作にて継続時間経過前に周波数が正常になった場合に、
 *   平常動作に戻る不具合の対応
 * [20260202] 2026.02.02
 * ・一次調整力異常動作コマ跨ぎで、異常時動作を一から再開する
 *   異常動作開始時刻を初期化、一次調整力異常動作中をOFF
 * ・一次調整力の出力で、出力抑制値以上出力しないように修正
 * [20260130] 2026.01.30
 * ・TM制御判定周波数、異常継続時間の削除
 * [20260114] 2026.01.14
 * ・一次調整力レポート蓄積終了時間を追加
 * [20251215] 2025.12.15 機能追加
 * ・充電応動有無で、充電の実施有無を制御する機能を追加
 * [20251212] 2025.12.12 不具合修正
 * ・一次調整力の出力計算結果が、約定△kWを超える場合、約定△kWに制限
 * [20251211] 2025.12.11 不具合修正
 * ・模擬周波数実行時の乗数合わせを追加
 * [20251204] 2025.12.04
 * ・一次調整力調整コマで、SV 一次調整力SoC調整 ONの対応
 * [20251203] 2025.12.03 機能強化
 * ・残電力量はバックエンドの計画値に従うようにし、SoC制御ではSoC上限・下限は
 *   チェックしないように修正
 * ・太陽光の発電電力目標値の算出処理を削除
 *   (※現状、太陽光の設備はない＆太陽光を設置する際は再度検討する必要があるため）
 * [20251202] 2025.12.02
 * ・受電電力一定制御にて充電電力上限、放電電力上限のチェックが行われない不具合の対応
 * [20251202] 2025.12.02 
 * ・出力制御情報が無効で、出力制御スケジュール値が実装の場合、
 *   出力制御スケジュール値を登録するように修正
 * [20251031] 2025.10.31 
 * ・周波数正常範囲チェックから上限側のチェックを削除
 *   周波数異常時動作は下限側のみで行う変更
 * [20250924] 2025.09.24 
 * ・一次調整コマのSoC制御目標を残電力目標値に修正
 * [20250828] 2025.08.28 
 * ・火力発電所対応（蓄電池が未実装の場合のNULLアクセス回避）
 * [20250807] 2025.08.07 
 * ・一次調整力（オフライン）対応
 *   設定更新ファイル(一次調整力パラメータ設定情報、一次調整力レポート蓄積項目情報)対応
 * [20250305] 2025.03.05 
 * ・AWSで接続先から切断された際にpvsysログに出力するように修正
 *   放電しないように修正（MGE_IPOCC-1101対応）
 * [20250217] 2025.02.17 
 * ・容量市場・需給調整市場の回復コマに残電力量が必要電力量を超えていた場合
 *   放電しないように修正（MGE_IPOCC-1101対応）
 * [20250212] 2025.02.12 
 * ・容量市場動作中に不感帯の上限を適用しないように修正（MGE_IPOCC-1099対応）
 * [20250206] 2025.02.06
 * ・容量市場のパラメータ(cm_exam)を大文字/小文字を区別しないように修正
 *   ※ACシステム間―統合コントローラ間メッセージ情報では小文字で記載しているが対応する
 * [20250205] 2025.02.05
 * ・受電電力、負荷電力の更新を常に更新するように修正
 * ・受電電力一定制御時の受電電力、負荷電力の更新をそれぞれの計測ステータスを使用するように修正
 * [20250131] 2025.01.31
 * ・1つのコマを需給調整、容量市場どちらの回復コマとして使用した場合に
 *   需給調整市場の回復コマとして扱い、容量市場の回復コマとして扱っていない問題の対応（MGE_IPOCC-1096対応）
 * [20250110] 2025.01.10
 * ・AWSのKeepAliveの時間を20秒->60秒に変更（RJからの水平展開）
 * [20241205] 2024.12.05 Ph4.2対応
 * ・単体試験不具合対応（実需給＞実効性テストの優先度で、実効性テストのみ発動すると
 *   指令を受けていない実需給が動作してしまう)
 * [20241204] 2024.12.04
 * ・func_mode_S4での受電電力一定制御有無の不要なSV判定の処理を削除
 *   （受電電力一定制御有でしか本関数に入らない為）
 * ・func_mode_S4でのピークカットの優先度判定を削除
 *   （コマ毎のピークカットの優先度を含めた判定はバックエンド、サーバアプリの需給計画で考慮されているため）
 * [20240918] 2024.09.10 Ph4.2対応
 * ・単体試験不具合対応
 * [20240910] 2024.09.10 Ph4.2対応
 * ・需給調整市場、容量市場中は必要電力量に到達したら放電を停止するように修正
 * ・容量市場の実需給、実効性テストの優先度を外部ファイルから取得するように修正
 * [20240701] 2024.07.01 Ph4.2対応
 * ・需給調整市場(三次①、②）、容量市場(実需給)、容量市場(実効性テスト)で
 *   個々のストックを管理するように修正
 * [20240614] 2024.06.14 Ph4品証指摘No.156対応
 * ・指令情報で現在運用中と未来の指令の商品区分が異なる場合、
 *   優先度の高い場合は、指令情報を切り替えないように修正
 * [20240603] 2024.06.03 Ph4品証指摘No.114対応
 * ・回復コマの充電電力量が0の場合、計画SoCを使用するように修正
 * [20240221] 2024.02.21 Ph4対応
 * ・SoC回復制御対応(func_mode_S4_StlSoC()改修)
 * ・商品区分を追加
 * [20240216] 2024.02.16 Ph4対応
 * ・SoC回復制御対応(func_check_powerPlan()改修)
 * [20240207] 2024.02.07 Ph4対応
 * ・指令情報の未来ストック数変更(1→5)に対応
 * [20231211] 2023.12.11
 * ・起動時に需給調整のデバイスIDが前回起動中のIDと異なる場合、情報を初期化しているが
 *   運用モードを待機にしていない問題の対応
 * [20231009] 2023.10.09
 * ・需給調整中、且つ、不感帯にいる時にスケジュールが変更になった場合、追従しない不具合の対応。
 * [20230928] 2023.09.28
 * ・20230926の実装不備。（重故障(共通)の状態でモード遷移した場合は、一度障害から戻るように修正)
 * [20230926] 2023.09.26
 * ・20230922を改めて設計し修正
 * [20230922] 2023.09.22
 * ・対向試験でのIslandModeの認識違いの対応。(自立なし場合は、IslandMode=5にしかならない)
 * [20230919] 2023.09.19
 * ・電源ON時にAWS IoT-Coreからの指令情報を初期化してしまう不具合の対応
 * [20230908] 2023.09.08
 * ・需給調整の約定ブロック(受電電力一定制御)にて放電禁止状態になり、
 *   約定ブロックを終了後に放電再開SoCを超えても解除されない。
 * [20230907] 2023.09.07
 * ・PublishしたデータをSubscribeでチェックする/しないを切り替えられるように対応
 * ・需給調整市場のACの指令時間待ちの受電電力目標値が計画値の値に設定されている為、
 * [20230905] 2023.09.05
 * ・設定変更ファイル、遠隔制御ファイルの内容をログファイルに出力するように修正
 * [20230831] 2023.08.31
 * ・需給調整市場のACの指令時間待ちの受電電力目標値が計画値の値に設定されている為、
 *   指令時間になった際にその目標値から指令の目標値に充放電を行うようになっていた為、
 *   指令時間待ちの際の受電電力目標値を0にするように修正。
 * [20230830] 2023.08.30
 * ・充電可能電力、放電可能電力を動的に反映できるように修正
 * ・需給調整市場発動時に画面で設定した充電可能電力、放電可能電力を超えて
 *   制御している不具合の対応。(Ph2 品証指摘No.50)
 * [20230828] 2023.08.28
 * ・受電電力一定制御が正しいタイミングでON/OFFされない。(品証試験指摘調査時の発見)
 * [20230823] 2023.08.23
 * ・出力制御オンライン対応
 * ・電源ON時に電源OFF前のモードを引き継ぐ際に「停止」の場合は「待機」にするように修正
 * [20230814] 2023.08.14
 * ・52Rの状態を52-P1異常を含めてチェックするように修正
 * [20230711] 2023.07.11
 * ・誤記修正（Notify）
 * [20230529] 2023.05.29 Ph2対応
 * ・Ph2対応
 * [20230302] 2023.03.02 機能改善
 * ・停電のSV名称をシャットダウンタイマ開始に変更
 * ・制御優先度がピークカット>価格連動制御の場合、受電電力目標値の不感帯がデマンド
 *   制御閾値を超えないように修正
 * ・受電電力目標値の不感帯の上限が最低受電電力を下回らないように補正
 * [20230301] 2023.03.01 仕様変更
 * ・BT出力変化量制御の有効/無効を廃止(常に有効)し、変化量を充電/放電方向に分ける
 * [20230224] 2023.02.24
 * ・SoCの考え方を「下限値となったら」ではなく「下限値を下回ったら」に変更(品証指摘No.193)
 *   同様に復帰についても見直し。
 * ・前回制御の判定条件に、充電禁止状態が継続している場合は含まないように追加
 * ・前回制御の判定条件に、放電禁止状態が継続している場合は含まないように追加
 * ・充電停止の判定条件にパワコンがない(定格が0)条件を追加
 * ・PV発電目標値が0を下回った場合の補正処理を追加
 * [20230222] 2023.02.22
 * ・充放電計画の補正値がある場合の制御を見直し(品証指摘No.190)
 * ・価格連動制御(受電電力一定制御)時にデマンド閾値での補正をしている(品証指摘No.187)
 * [20230220] 2023.02.20 仕様変更
 * ・制御不感帯を%->Wに変更
 * ・需給計画更新時の変数初期化漏れ
 * ・受電電力一定制御、過充電回避制御、デマンド監視の制御情報を削除
 * [20230217] 2023.02.17 結合試験
 * ・需給計画に対する制御を見直し
 * [20230216] 2023.02.16 結合試験
 * ・需給計画検索時に初期値を設定するように修正
 * [20230216] 2023.02.16 品証試験指摘（開発試験前）
 * ・BCPとSoCの下限値のチェックを追加
 * ・遠隔操作指示ファイルのユーザ番号を無視するように修正
 * [20230131] 2023.01.31
 *   ※20230124対応の考慮漏れ
 * [20230125] 2023.01.25
 * ・需給計画設定のSVを追加
 * [20221011] 2022.10.11 仕様変更
 * ・遠隔制御処理時、画面表示中対応
 * [20220810] 2022.08.10
 * ・連系運転中,連系運転中(障害),自立運転中,自立運転中(障害)時のUVR検出時に
 *   全て待機(障害)に遷移していた誤りを修正
 * [20220809] 2022.08.09
 * ・連系運転中,連系運転中(障害),自立運転中,自立運転中(障害)時の強制待機条件を
 *   UVRのみに変更
 * [20220726] 2022.07.26
 * ・統合コントローラを停止する条件を変更
 *   
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 制御管理プロセス（emctlmng）
 *
 * 機能：
 *		MicroGridシステム専用の接点制御プロセス
 * 		以下の機能を提供する
 * 		■XXX機能
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>	/* for mmap */
#include <sys/select.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>		/* for open */
#include <termios.h>
#include <stdlib.h>		/* for atoi */
#include <unistd.h>		/* for ftruncate */
#include <string.h>		/* for memset, memcpy, strlen, strnlen */
#include <stdio.h>		/* for fopen , fgets */
#include <dirent.h>		/* for scandir */
#include <ctype.h>		/* for tolower *//* [20250206] Add */

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
#include "emctlmng.h"

#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_TM									/* 制御判定値構成確認 */
#define	PV_DEBUG_PVCTL
#define PV_DEBUG_AVE								/* 移動平均値確認 */
#define	PV_DEBUG_TMAGG
#define	PV_DEBUG_DMD
#define	DEBUG_PER_VAL								/* PV制御値 */
#define	DEBUG_BT_VAL								/* BT制御値 */
#define	DEBUG_MODE_CMN								/* デバッグ：モード共通 */
#define	DEBUG_MODE_S4								/* デバッグ：連系 */
#define	DEBUG_MODE_S3								/* デバッグ：自立 */
#define	DEBUG_RMT									/* デバッグ：遠隔制御 */
#define	DEBUG_MODESW								/* デバッグ：モード切替制御 */
#define	DEBUG_MODE_PV								/* デバッグ：PV制御 */
#define DEBUG_POWER_PLAN							/* デバッグ：需給計画 */
#define	DEBUG_FCR_CTL_PRM							/* デバッグ：一次調整力（オフライン）パラメータ読込 *//* [20250807] Add */
#define	DEBUG_FCR_CTL_CALC							/* デバッグ：一次調整力（オフライン）出力計算 *//* [20250807] Add */
#define	DEBUG_FCR_FREQ_SIM							/* デバッグ：一次調整力（オフライン）模擬周波数 *//* [20250807] Add */
#define	DEBUG_FCR_FREQ_CTL							/* デバッグ：一次調整力（オフライン）制御 *//* [20250807] Add */
#define	DEBUG_SCH_TM_PER							/* デバッグ：出力制御スケジュール値(外部取得)[20251202] */
#define	DEBUG_SCH_DISCHG							/* デバッグ：一次調整力（オフライン）出力制御での出力[20260203] */
#endif

#define	PV_OWN_NAME		"EMCTLMNG"			/* 自プログラム名 */
#define PV_AWS_SUBSCRIBE_SKIP_FILENAME				"AWS_SUBSCRIBE_SKIP"		/* 外部データ *//* [20230907] Add */

struct st_PVSYSINF		*ps_g_PVSYSINF=NULL;		/* 管理ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF=NULL;		/* 計測情報ファイルポインタ */
struct st_SVDATINF		*ps_g_SVDATINF=NULL;		/* 装置状態情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF=NULL;		/* 状変マスクファイルポインタ */
struct st_TMRECFIL		*ps_g_TMRECFIL=NULL;		/* 計測情報蓄積ファイルポインタ */
struct st_SVRECFIL		*ps_g_SVRECFIL=NULL;		/* 状態情報蓄積ファイルポインタ */
struct st_BATTEINF		*ps_g_BATTEINF=NULL;		/* バッテリー情報ファイルポインタ */
struct st_TMAGGINF		*ps_g_TMAGGINF=NULL;		/* 集約計測情報ファイルポインタ */
struct st_SVAGGINF		*ps_g_SVAGGINF=NULL;		/* 集約状態情報ファイルポインタ */
struct st_TMMONINF		*ps_g_TMMONINF=NULL;		/* 計測監視情報ファイルポインタ */
struct st_CEMSPINF		*ps_g_CEMSPINF=NULL;		/* ＣＥＭＳ制御情報保存ファイル */
struct st_DEMNDINF		*ps_g_DEMNDINF=NULL;		/* デマンド監視情報保存ファイル */
struct st_RCVPWINF		*ps_g_RCVPWINF=NULL;		/* 受電電力一定制御情報保存ファイル */
struct st_OVRCHINF		*ps_g_OVRCHINF=NULL;		/* 過充電回避制御情報保存ファイル */
struct st_MDCTLINF		*ps_g_MDCTLINF=NULL;		/* モード切替時制御情報保存ファイル */
struct st_RTCTLINF		*ps_g_RTCTLINF=NULL;		/* 遠隔制御情報ファイルポインタ */
struct st_DEMNDFIL		*ps_g_DEMNDFIL=NULL;		/* デマンド制御情報保存ファイル */
struct st_POWCNINF		*ps_g_POWCNINF=NULL;		/* パワーコンディショナ情報ファイル */
struct st_TMSTSFIL		*ps_g_TMSTSFIL=NULL;		/* 計測値保存ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL=NULL;		/* 装置状態ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL=NULL;		/* 通信ステータス状態ファイルポインタ */
struct st_SVAGGFIL		*ps_g_SVAGGFIL=NULL;		/* 集約状態保存ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL=NULL;		/* 状変蓄積ファイルポインタ */
struct st_CTLJHFIL		*ps_g_CTLJHFIL=NULL;		/* 制御状変蓄積ファイルポインタ */
struct st_PVCTLFIL		*ps_g_PVCTLFIL=NULL;		/* 制御状態保存ファイルポインタ */
struct st_EMCTLFIL		*ps_g_EMCTLFIL=NULL;		/* エネルギーマネジメント管理ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG=NULL;		/* システムログファイル */
struct st_SVRECINF		*ps_g_SVRECINF=NULL;		/* 状態履歴情報ファイル */
struct st_TMRECINF		*ps_g_TMRECINF=NULL;		/* 計測履歴情報ファイル */
struct st_PWPLNINF		*ps_g_PWPLNINF=NULL;		/* 受電電力計画値情報保存ファイルポインタ */
struct st_PWPLNFIL		*ps_g_PWPLNFIL=NULL;		/* 受電電力計画値保存ファイルポインタ */
struct st_IOTCNINF		*ps_g_IOTCNINF=NULL;		/* IoT設定情報ファイルポインタ [20230529] Add */
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL=NULL;		/* スケジュール管理情報ファイルポインタ [20230529] Add */
struct st_PGCONINF		*ps_g_PGCONINF=NULL;		/* 電力会社向け情報ファイルポインタ *//* [20230814] Add */
/* [20250807] Add St */
struct st_FCRSTINF		*ps_g_FCRSTINF=NULL;		/* 一次調整力パラメータ設定情報ファイルポインタ */
struct st_FCRRPINF		*ps_g_FCRRPINF=NULL;		/* 一次調整力レポート蓄積項目情報ファイルポインタ */
/* [20250807] Add Ed */
struct st_RPRECFIL		*ps_g_RPRECFIL=NULL;		/* 一次調整力レポート蓄積ファイル *//* [20260515] */

													/* 使用ファイルインデックス */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVDATINF, (void **)&ps_g_SVDATINF, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_TMRECFIL, (void **)&ps_g_TMRECFIL, 0, 0, 0 },
	{	PV_FILENO_SVRECFIL, (void **)&ps_g_SVRECFIL, 0, 0, 0 },
    {   PV_FILENO_TMAGGINF, (void **)&ps_g_TMAGGINF, 0, 0, 0 },
    {   PV_FILENO_SVAGGINF, (void **)&ps_g_SVAGGINF, 0, 0, 0 },
    {   PV_FILENO_TMMONINF, (void **)&ps_g_TMMONINF, 0, 0, 0 },
    {   PV_FILENO_CEMSPINF, (void **)&ps_g_CEMSPINF, 0, 0, 0 },
    {   PV_FILENO_DEMNDINF, (void **)&ps_g_DEMNDINF, 0, 0, 0 },
    {   PV_FILENO_RCVPWINF, (void **)&ps_g_RCVPWINF, 0, 0, 0 },
    {   PV_FILENO_OVRCHINF, (void **)&ps_g_OVRCHINF, 0, 0, 0 },
    {   PV_FILENO_MDCTLINF, (void **)&ps_g_MDCTLINF, 0, 0, 0 },
    {   PV_FILENO_RTCTLINF, (void **)&ps_g_RTCTLINF, 0, 0, 0 },
    {   PV_FILENO_DEMNDFIL, (void **)&ps_g_DEMNDFIL, 0, 0, 0 },
    {   PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
    {   PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
    {   PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
    {   PV_FILENO_SVAGGFIL, (void **)&ps_g_SVAGGFIL, 0, 0, 0 },
    {   PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
    {   PV_FILENO_CTLJHFIL, (void **)&ps_g_CTLJHFIL, 0, 0, 0 },
    {   PV_FILENO_PVCTLFIL, (void **)&ps_g_PVCTLFIL, 0, 0, 0 },
	{	PV_FILENO_EMCTLFIL, (void **)&ps_g_EMCTLFIL, 0, 0, 0 },
    {   PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
    {   PV_FILENO_SVRECINF, (void **)&ps_g_SVRECINF, 0, 0, 0 },
    {   PV_FILENO_TMRECINF, (void **)&ps_g_TMRECINF, 0, 0, 0 },
	{	PV_FILENO_PWPLNINF, (void **)&ps_g_PWPLNINF, 0, 0, 0 },
	{	PV_FILENO_PWPLNFIL, (void **)&ps_g_PWPLNFIL, 0, 0, 0 },
	{	PV_FILENO_IOTCNINF, (void **)&ps_g_IOTCNINF, 0, 0, 0 },	/* [20230529] Add */
	{	PV_FILENO_SCHCTLFIL, (void **)&ps_g_SCHCTLFIL, 0, 0, 0 },	/* [20230529] Add */
	{	PV_FILENO_PGCONINF, (void **)&ps_g_PGCONINF, 0, 0, 0 },	/* [20230823] Add */
/* [20250807] Add St */
	{	PV_FILENO_FCRSTINF, (void **)&ps_g_FCRSTINF, 0, 0, 0 },
	{	PV_FILENO_FCRRPINF, (void **)&ps_g_FCRRPINF, 0, 0, 0 },
/* [20250807] Add Ed */
	{	PV_FILENO_RPRECFIL, (void **)&ps_g_RPRECFIL, 0, 0, 0 },	/* [20260515] Add */
	{	0,	                NULL,                    0, 0, 0 }
};

static char						pv_own_name[128];			/* 自プロセス情報 */
static char						errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
struct tm						s_timeData;					/* 時刻情報 */
struct timeval					s_nowTime;					/* 現在時刻(積算秒) */
static struct st_stdDevInfo		s_sysInfo;					/* 収取装置格納領域 */
static struct st_sysData		s_sysDataRef;				/* 計測情報（瞬時値格納用） */
static struct st_sysData		s_sysDataAv;				/* 計測情報（瞬時値格納用）移動平均対応 */
static struct st_sysAvlData		s_sysAvlData;				/* 計測情報（移動平均演算用） */
static struct st_sysAvlData		s_sysAvlDataTemp;			/* 計測情報（移動平均演算用） */
static char						threadMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
static int						threadStop = 0;					/* スレッドストップフラグ */
static int						s_pvSetVal=-1;					/* PV発電量 */
static int						s_blcSetPow=0;					/* バッテリ充放電電力 */
static int						s_bttReq=0;						/* バッテリ制御指示(0:未指示,1:指示済み) */
static int						s_pvchgTime=0;					/* 出力変化時間 */
static time_t					s_updateSoCTime=0;				/* 目標SoC更新時刻 */
static int						s_ctlAveTime=0;					/* 制御用平均値 */
static char						s_updateConfFile[64];			/* 起動後設定変更ファイル名 */
static struct st_avlData		s_rcvPowAve;					/* 受電電力平均値 */
static struct st_avlData		s_loadPowAve;					/* 負荷電力平均値 */
static struct st_avlData		s_freqAve;						/* 周波数平均値 *//* [20260514] Add */
/* [20230529] Add St */
static AWS_IoT_Client 			s_rcv_client;					/* クライアント情報 */
static struct st_pvJsonParse	s_rcv_pvJsonparse;				/* 受信解析情報 */
static char						s_rcvTopicName[256];			/* トピック名 */
/* [20230529] Add Ed */
static char						s_SubscribeSkip = 0;			/* Subscribeスキップフラグ *//* [20230907] Add */
static int						s_schDataPre=-1;				/* 前回制御値 *//* [20231009] Add */


/* 内部関数定義 */
void pvctl_ManualMode( void );
int func_mode_ctl_main( int );
/* [20250807] Chg St */
int func_check_powerPlan( int, struct timeval *, int * );	/* [20260202] Chg */
/* [20250807] Chg Ed */
int	pvctl_cmn_init( void );
int	pvctl_cmn_abnormal_exit( int );
int pvctl_cmn_sepcsv_data( char *, struct st_sepStr * );
int pvctl_cmn_get_process_tim( struct timeval *, struct tm * );
int pvctl_cmn_set_process_tim( void );
time_t func_cmn_mktime( int year, int month, int day, int hour, int min, int sec);
int	pvctl_cmn_sysDataTm_Set( struct st_sysData *, int , int , int  );
int pvctl_rmtctl_file_read_check( void );
int	pvctl_rmtctl_choice_file( const struct dirent * );
int	pvctl_rmtctl_file_read( char * , struct st_RTCTLINF *);
int	pvctl_confupdate_file_read_check( void );
int	pvctl_confupdate_choice_file( const struct dirent * );
/* [20250807] Chg St */
int	pvctl_confupdate_file_read( char * , struct st_CEMSPINF *, struct st_DEMNDINF *, struct st_RCVPWINF *, struct st_OVRCHINF *, struct st_PWPLNINF *, struct st_BATTEINF *, struct st_FCRSTINF *, struct st_FCRRPINF *, int *, int * );
/* [20250807] Chg Ed */
int	pvctl_confupdate_file_info_check( FILE *, int *, int *, char *  );
int	func_tmmoni_update( struct st_sysData  * );
int	func_tmagg_update( struct st_sysData  * );
int	func_svagg_update( struct st_sysData  * );
long long func_pow10( int );
void func_tolower( char * );	/* [20250206] Add */
int func_avlData_init( struct st_avlData *ps_avlData, int stockNum );
int func_avlData_Set( struct st_avlData *ps_avlData, int tmFlg, int tmVal );
int func_bttPowSet( int *p_setPow, int trgtPow );	/* [20230301] Chg */
int func_pvPowSet( int *p_setPow, int trgtPow );	/* [20230301] Chg */
int func_mode_act( int nowMode );
void func_mode_sv_update( int nowMode );
int func_s1_check( int *nextMode );
int func_mode_check( int nowMode );
int	func_mode_S4( int  );
int	func_mode_S4_StlSoC( int  );
/* [20250807] Add St */
int	func_mode_S4_fcr( void );
/* [20250807] Add Ed */
int	func_mode_S7( void );
int	func_mode_S3( void );
int func_bt_ctl_check( int nowMode );
int func_pv_ctl_check( int nowMode );
int func_rmt_ctl_main( int nowMode );
int func_demand( void );
int	func_dmd_start( void );
int	func_dmd_moni_check( void );
int	func_dmd_moni_end( void );
int	func_dmd_moni( void );
/* [20230529] Add St */
int	emctl_aws_rcv_init( void );
int	emctl_aws_rcv_quit( void );
static void emctl_subscribe_callback_handler_rcv(AWS_IoT_Client *, char *, uint16_t , IoT_Publish_Message_Params *, void *);
void emctl_disconnect_handler(AWS_IoT_Client *, void *);
int emctl_subscribe_order_info_der( struct st_pvJsonParse *, char *, struct st_ORDERPARA  *);
int emctl_subscribe( struct st_pvJsonParse *, int, char * );
int emctl_order_check( void );
int emctl_order_check_sub( struct st_ORDERPARA *, struct st_setdata *, struct st_evtdata * );	/* [20240701] Add */
int emctl_order_update( void );
int emctl_order_update_sub( struct st_ORDERPARA *, struct st_setdata *, struct st_evtdata * );	/* [20240701] Add */
void *emctl_iot_rcv_thread( void *args );
int	emctl_cmn_publisResCheck( char *, int  );
int	emctl_cmn_publishNotify( void );
int	emctl_cmn_setNotify( struct st_resinfo * );
/* [20230529] Add Ed */
void emctl_order_info_set( struct st_ORDERPARA * );/* [20240207] Add Start */
/* [20250807] Add St */
int func_fcr_report_acum_judge( struct timeval, int);	/* [20260114] Chg */
bool func_fcrfreqcheck_in_normal( int * );
void fucc_set_fcrctlprm_tmsv( void );/* [20251215] Chg */
void func_fcr_freq_sim_ctl( struct timeval );
int func_fcr_freq_sim_judge( struct timeval );
/* [20250807] Add Ed */
int func_get_extraParam( char *, time_t *, int *, int * );	/* [20260514] Add */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム制御処理
 *
 * 機能：
 *		外部ファイル・設定ファイルから、制御に関する情報を取得する
 *		３０秒周期に下記処理を行う
 *		(１)スケジュール制御指示ファイルから、当日のスケジュールのみを読み込み、
 *		   現在時刻と一致したスケジュールの制御を行う。
 */
int main(int argc, char *argv[])
{
	int						procNo;						/* プログラム番号 */
	int						retCode;					/* 関数戻り値格納用 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						stopFlag = 0;				/* 停止通知受信済みフラグ */
	struct stat				statBuff;					/* ファイル状態 */
	struct timeval			nowTime;					/* 現在時刻 */
	int						minDataMax;					/* 移動平均データ最大数 */
	int						cyclems=100;				/* 動作周期 */
	int						sysMaxCnt;					/* 処理対象仮想数 */
	pthread_t				threadID;					/* スレッドＩＤ */
	struct tm				timeData;					/* 時刻情報 */
	int						virMaxCnt;					/* 処理対象仮想数 */
	int						tmFlgBt;					/* 計測ステータス */
	int						tmFlgPv;					/* 計測ステータス */
	int						tmFlgBg;					/* 計測ステータス */
	int						svStat = 0;
	int						forcePowPlan = EMCTL_FLG_ON;	/* 需給計画情報強制更新 */
	int						fileCnt = 0;
	struct st_fileIndex		s_fileIndex2[PV_MAX_MKIND];
	pthread_t				rcv_threadID=0;				/* スレッドＩＤ *//* [20230529] Add */
	char					filePath[128];				/* 外部データパス *//* [20230907] Add */
/* [20240910] Add St */
	time_t					fileTime=0;					/* ファイル更新時刻 */
	char					extdataPath[128];			/* 外部データパス */
	int						retFileCode;				/* 関数戻り値格納用 */
/* [20240910] Add Ed */
/* [20250807] Add St */
	struct timeval 			s_fcrErrStTime;				/* 異常動作開始時刻 */
/* [20250807] Add Ed */
/* [20260202] Add St */
	int						preDmdPos = 0;				/* 前処理コマ */
/* [20260202] Add St */
	int						cmExamPri = 0;				/* 容量市場優先度 */
	int						freqAveCnt = 10;			/* 周波数平均100msカウント(初期値1秒) */
	int						svStRepoDataAcum;			/* レポートデータ蓄積 */

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
	sprintf( pv_own_name, "%s", PV_OWN_NAME, procNo );

	/* ファイルをオープンしてＭＭＡＰ化する */
	if( (retCode = commfunc_file_open( s_fileIndex )) < 0 )
	{
		printf("%s : File Open Error\n", PV_OWN_NAME);
		return( -1 );
	}

	/* ＭＭＡＰ化するファイルを特定する（通信対象装置の実装状態に併せてファイルを設定する） */
	memset( s_fileIndex2, 0, sizeof(s_fileIndex2) );
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_BATTE].machineCnt !=  0 )
	{
		/* バッテリが存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_BATTEINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_BATTEINF;
		fileCnt++;
	}
	if( ps_g_PVSYSINF->s_machineInfo[PV_MKIND_POWCN].machineCnt !=  0 )
	{
		/* パワーコンディショナが存在する場合 */
		s_fileIndex2[fileCnt].fileNumb = PV_FILENO_POWCNINF;
		s_fileIndex2[fileCnt].point    = (void **)&ps_g_POWCNINF;
		fileCnt++;
	}
	/* ストッパーを設定 */
	s_fileIndex2[fileCnt].fileNumb = 0;
	s_fileIndex2[fileCnt].point    = NULL;

	/* 使用ファイルをオープンしてＭＭＡＰ化 */
	retCode = commfunc_file_open( s_fileIndex2 );
	if(retCode < 0)
	{
		/* 使用ファイルをクローズ */
		commfunc_file_close( s_fileIndex );
		printf("%s : File Open Error2\n", PV_OWN_NAME);
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_SET );

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_RESET );

	/* データ格納領域を初期化 */
	memset( &s_sysDataRef, 0, sizeof(s_sysDataRef));
	memset( &s_sysDataAv, 0, sizeof(s_sysDataAv));
/* [20250807] Add St */
	memset( &s_fcrErrStTime, 0, sizeof(s_fcrErrStTime));
/* [20250807] Add Ed */

	/* データ格納領域に欠測を設定 */
	for( loopCnt1 = 0; loopCnt1 < PV_MAX_PVSYS_TMPOS; loopCnt1++ )
	{
		s_sysDataRef.tmFlgData[loopCnt1] = PV_TMFLG_DROPTM;
		s_sysDataAv.tmFlgData[loopCnt1] = PV_TMFLG_DROPTM;
	}

	/* 1分間の蓄積データ数 */
	minDataMax = 60 * 1000 / cyclems;

	/* 装置情報を取得 */
	sysMaxCnt = commfunc_devInfo_get( PV_MKIND_PVSYS, 0, NULL, &s_sysInfo, NULL, NULL, NULL, NULL );

	/* 移動平均用データの初期化 */
	commfunc_devAvlData_init( PV_MKIND_PVSYS, minDataMax, &s_sysAvlData );

	/* 移動平均用データから積算型計測値を復元する */
	retCode = commfunc_tmstsfil_get( PV_MKIND_PVSYS, PV_MAX_PVSYS, PV_MAX_PVSYS_TMPOS, &s_sysInfo, &s_sysDataAv, &s_sysAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );

	/* 移動平均情報を初期化 */
	retCode = func_avlData_init( &s_rcvPowAve, (ps_g_RCVPWINF->rcvPowAveTime / 100) );
	retCode = func_avlData_init( &s_loadPowAve, (ps_g_RCVPWINF->loadPowAveTime / 100) );

	/* 初期化処理 */
	pvctl_cmn_init();

/* [20230529] Add_St */
	/* IoT設定情報が有効の場合 */
	if( ps_g_IOTCNINF->statFlag == 1 )
	{
		/* IOT受信用スレッドを生成 */
		if( pthread_create( &rcv_threadID, NULL, emctl_iot_rcv_thread, NULL ) != 0 )
		{

#ifdef PV_DEBUG
printf("%s : Thread EXECUTE Error\n", pv_own_name );
#endif
			sprintf( errMsg, "Thread Execute Error" );
			commfunc_syslog_msgset( pv_own_name, errMsg );

			/* プロセス異常終了処理 */
			pvctl_cmn_abnormal_exit( procNo );
			return( -1 );
		}
	}
/* [20230529] Add_Ed */

	/* 外部データを取得 */
	sprintf(filePath, "%s%s", PV_PATH_CSV, PV_AWS_SUBSCRIBE_SKIP_FILENAME );	/* [20230907] Add */

/* [20240910] Add St */
	/* 外部データファイルを設定 */
	sprintf(extdataPath, "%s%s", PV_PATH_CSV, EMS_CTLCONF_NAME );

/* [20260514] Chg St */
	/* 設定ファイルのチェック*/
	retFileCode = func_get_extraParam( extdataPath, &fileTime, &cmExamPri, &freqAveCnt );
	if( retFileCode != COM_PARAM_GET_OK )
	{
		sprintf( errMsg, "commfunc_file_info_get RET=%d. (%s)", retFileCode, extdataPath );
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}
	else
	{
		sprintf( errMsg, "extra file read. (%s)", extdataPath );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* 容量市場優先を設定 */
		ps_g_EMCTLFIL->cmExamPri = cmExamPri;

		sprintf( errMsg, " %s=%d", EMS_CTLCONF_PARAM3, ps_g_EMCTLFIL->cmExamPri );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		sprintf( errMsg, " %s=%d", EMS_CTLCONF_PARAM4, freqAveCnt );
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}
	/* 移動平均情報を初期化 */
	retCode = func_avlData_init( &s_freqAve, freqAveCnt);
/* [20260514] Chg Ed */
/* [20240910] Add Ed */

	while( 1 )
	{
/* [20230907] Add St */
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
/* [20230907] Add Ed */
/* [20240910] Add St */
/* [20260514] Chg St */
		/* 設定ファイルのチェック*/
		retFileCode = func_get_extraParam( extdataPath, &fileTime, &cmExamPri, &freqAveCnt );
		if( retFileCode == COM_PARAM_GET_OK )
		{
			/* 容量市場優先を設定 */
			ps_g_EMCTLFIL->cmExamPri = cmExamPri;
			sprintf( errMsg, "Updated Capacity Market Priority.");
			commfunc_syslog_msgset( pv_own_name, errMsg );
			sprintf( errMsg, " %s=%d", EMS_CTLCONF_PARAM3, ps_g_EMCTLFIL->cmExamPri );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			/* 周波数平均100msカウントが更新された場合 */
			if( s_freqAve.stocMax != freqAveCnt )
			{
				sprintf( errMsg, "Updated frequency average time.");
				commfunc_syslog_msgset( pv_own_name, errMsg );
				/* 移動平均情報を初期化 */
				retCode = func_avlData_init( &s_freqAve, freqAveCnt);
				sprintf( errMsg, " %s=%d", EMS_CTLCONF_PARAM4, freqAveCnt );
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
/* [20260514] Chg Ed */
		}
/* [20240910] Add Ed */


		/* 停止通知受信済みフラグリセット */
		stopFlag = 0;

		/* 指定周期になるまで待機する */
		if( commfunc_mswait( cyclems, procNo ) < 0 )
		{
			/* 停止通知受信済みフラグ設定 */
			stopFlag = 1;
		}

		/* 設定情報ファイル更新チェック */
		retCode = pvctl_confupdate_file_read_check();

		/* 現在時刻を取得 */
		pvctl_cmn_set_process_tim();

		/* 時刻情報を取得 */
		pvctl_cmn_get_process_tim(&nowTime, &timeData);

/* [20250807] Add St */
		/* 模擬周波数操作 */
		func_fcr_freq_sim_ctl( nowTime );
/* [20250807] Add Ed */

		/* 計測監視情報 */
		func_tmmoni_update(&s_sysDataAv);

		/* 集約計測情報 */
		func_tmagg_update(&s_sysDataAv);

		/* 集約状態情報 */
		func_svagg_update(&s_sysDataAv);

/* [20250807] Add St */
		/* 一次調整力パラメータ設定TM反映 */
		fucc_set_fcrctlprm_tmsv();/* [20251215] Chg */
/* [20250807] Add Ed */
/* [20230529] Add St */
		/* 指示情報更新 */
		emctl_order_update();

		/* IoT制御チェック */
		emctl_order_check();
/* [20230529] Add Ed */

		/* 設定変更情報に伴うファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

/* [20250807] Chg St */
		/* 需給計画更新 */
		func_check_powerPlan(forcePowPlan, &s_fcrErrStTime, &preDmdPos);	/* [20260202] Chg */
/* [20250807] Chg Ed */

		forcePowPlan = EMCTL_FLG_OFF;

		/* 設定変更情報のミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

/* [20230830] Del */

/* [20210313] Add St */
		/* BT充放電制御が無効の場合 */
		if( ps_g_CEMSPINF->btControl == 0 )
		{
			/* 最大充電電力を0に設定 */
			ps_g_EMCTLFIL->avlChgPow = 0;
			/* 最大放電電力を0に設定 */
			ps_g_EMCTLFIL->avlDischgPow = 0;
		}
/* [20210313] Add Ed */

		/* マニュアルモードチェック */
		if( (ps_g_PVSYSINF->pvManualMode == EMCTL_FLG_ON) || 
			(ps_g_PVSYSINF->btManualMode == EMCTL_FLG_ON) )
		{
			pvctl_ManualMode();
		}
		else
		{
			/* 制御開始カウントダウン */
			if( ps_g_EMCTLFIL->actCntdown > 0 )
			{
				ps_g_EMCTLFIL->actCntdown --;
			}
			else
			{
				func_mode_ctl_main(stopFlag);
			}
		}

		/* 計測種別毎の保持 */
		retCode = commfunc_devTmData_set( PV_MKIND_PVSYS, 1, PV_MAX_PVSYS_TMPOS, &s_sysDataAv, &s_sysAvlData, ps_g_TMDATINF );

		/* 計測情報を計測値格納ファイルに保存 */
		retCode = commfunc_tmstsfil_devset( PV_MKIND_PVSYS, 1, PV_MAX_PVSYS_TMPOS, &s_sysDataAv, &s_sysAvlData, ps_g_TMDATINF, ps_g_TMSTSFIL );

		/* 状態蓄積処理 */
		commfunc_svrec_set( ps_g_SVRECINF, ps_g_SVSTSFIL);
		/* 計測蓄積処理 */
		commfunc_tmrec_set( ps_g_TMRECINF, ps_g_TMSTSFIL);
/* [20260515] Add St */
		/* レポートデータ蓄積を取得 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_REPORT_DATA_ACUM, &svStRepoDataAcum, ps_g_SVSTSFIL);

		/* 1秒周期で動作する */
		if( (ps_g_FCRRPINF->readFlg == 1 ) &&
			( svStRepoDataAcum == 1 ) &&
		    (( nowTime.tv_usec / 100000 )== 0 ) )
		{
			/* 実績データ蓄積処理 */
			commfunc_rprec_set( ps_g_FCRRPINF, ps_g_TMSTSFIL);
/* [20260515] Chg St */
		}
/* [20260515] Add Ed */

		/* 停止通知受信済フラグが設定されている場合 */
		if( stopFlag )
		{
			PVCTL_MSG_HIG("PVCTLMNG(PVCTL_MON) : PROCESS STOP COMMAND RECV (PROC:%d)\n", procNo);
			/* 処理を抜ける */
			break;
		}
	}

/* [20230529] Add_St */
	/* スレッドを起動した場合 */
	if( rcv_threadID != 0 )
	{
		/* スレッドの終了を待つ */
		threadStop = 1;
		pthread_join( rcv_threadID, NULL );
	}
/* [20230529] Add_Ed */

	/* 制御終了処理 */
	commfunc_pvctl_stop(PV_CTL_TYPE_CLOUD, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	commfunc_pvctl_stop(PV_CTL_TYPE_MODESW, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );

	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );
	commfunc_file_close( s_fileIndex2 );

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * マニュアルモード処理
 *
 * 機能：
 * 		マニュアルモードの処理を行う。
 * 戻り値：
 */
void pvctl_ManualMode( void )
{
	int						tmFlgBt;					/* 計測ステータス */
	int						tmFlgPv;					/* 計測ステータス */

	/* PVマニュアルモードチェック */
	if( ps_g_PVSYSINF->pvManualMode == EMCTL_FLG_ON )
	{
		/* 計測ステータスを通常に設定 */
		tmFlgPv = PV_TMFLG_NORMAL;
	}
	else
	{
		/* 計測ステータスを欠測に設定 */
		tmFlgPv = PV_TMFLG_DROPTM;
	}

	/* BTマニュアルモードチェック */
	if( ps_g_PVSYSINF->btManualMode == EMCTL_FLG_ON )
	{
		/* 計測ステータスを通常に設定 */
		tmFlgBt = PV_TMFLG_NORMAL;
	}
	else
	{
		/* 計測ステータスを欠測に設定 */
		tmFlgBt = PV_TMFLG_DROPTM;
	}

	/* BT制御値、PV制御値を算出する */
	func_bttPowSet( &ps_g_EMCTLFIL->bttSetPow, ps_g_EMCTLFIL->bttReqPow );		/* [20230301] Chg */

	/* PV発電量を出力制御目標値(0.01%換算)に変換 */
	if( ps_g_EMCTLFIL->pvRatedPow == 0 )
	{
		/* 定格が0wの場合は、0%で設定 */
		ps_g_EMCTLFIL->pvSetPer = 0;
	}
	else
	{
		func_pvPowSet( &ps_g_EMCTLFIL->pvSetPow, ps_g_EMCTLFIL->pvReqPow );		/* [20230301] Chg */
		ps_g_EMCTLFIL->pvSetPer = (int)(( (unsigned long long )ps_g_EMCTLFIL->pvSetPow * 100 * 100 ) / ps_g_EMCTLFIL->pvRatedPow);
	}

	/* 蓄電池制御指示を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_BT_SET_POW, tmFlgBt, ps_g_EMCTLFIL->bttSetPow );
	/* PV出力抑制指示を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PV_SET_PER, tmFlgPv, ps_g_EMCTLFIL->pvSetPer );

	/* 蓄電池制御目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_BT_REQ_POW, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->bttReqPow );
	/* PV発電電力目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PV_REQ_POW, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->pvReqPow );
	/* PV出力抑制目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PV_REQ_PER, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->pvSetPow );

	return;
}
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセス初期化処理
 *
 * 機能：
 * 		プロセスの初期化処理を行う。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int	pvctl_cmn_init( void )
{
	struct timeval	nowTime;
	int				loopCnt1;
	int				retCode = 0;				/* 戻り値 */

	/* 現在時刻を設定 */
	pvctl_cmn_set_process_tim();

	/* 設定変更保存ファイルを初期化 */
	memset( s_updateConfFile, 0, sizeof(s_updateConfFile));

/* [20230919] Del */

	/* 設定情報ファイル更新チェック */
	retCode = pvctl_confupdate_file_read_check();

	/* デバイスIDが異なる場合、情報を初期化する */
	if( strncmp( ps_g_EMCTLFIL->deviceId, ps_g_IOTCNINF->deviceId, strlen(ps_g_EMCTLFIL->deviceId)+1 ) != 0 )
	{
/* [20231211] Add St */
		sprintf( errMsg, "deviceId unmatch.(ps_g_EMCTLFIL:%s != ps_g_IOTCNINF:%s)", ps_g_EMCTLFIL->deviceId, ps_g_IOTCNINF->deviceId );
		commfunc_syslog_msgset( pv_own_name, errMsg );
/* [20231211] Add Ed */

		/* EM制御管理ファイルを初期化 */
		memset( ps_g_EMCTLFIL->s_rr_para, 0, sizeof(ps_g_EMCTLFIL->s_rr_para));	/* [20240701] Chg */
/* [20240701] Add St */
		memset( ps_g_EMCTLFIL->s_fcr_para, 0, sizeof(ps_g_EMCTLFIL->s_fcr_para));
		memset( ps_g_EMCTLFIL->s_frr_para, 0, sizeof(ps_g_EMCTLFIL->s_frr_para));
		memset( ps_g_EMCTLFIL->s_cm_para, 0, sizeof(ps_g_EMCTLFIL->s_cm_para));
		memset( ps_g_EMCTLFIL->s_cm_exam_para, 0, sizeof(ps_g_EMCTLFIL->s_cm_exam_para));
/* [20240701] Add Ed */

		memset( ps_g_EMCTLFIL->s_resinfo, 0, sizeof(ps_g_EMCTLFIL->s_resinfo));

		/* デバイスIDを更新する */
		memcpy( ps_g_EMCTLFIL->deviceId, ps_g_IOTCNINF->deviceId, sizeof(ps_g_EMCTLFIL->deviceId));

		/* 送信順序を設定する（初期時は、添え字順） */
		for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
		{
			ps_g_EMCTLFIL->resOrder[loopCnt1] = loopCnt1;
			ps_g_EMCTLFIL->s_resinfo[loopCnt1].resId = loopCnt1 + 1;
		}

/* [20231211] Add St */
		/* 運用モードを待機に初期化 */
		ps_g_EMCTLFIL->s_setdata_rr.actMode = AWS_MODE_IDLE;
/* [20231211] Add Ed */
/* [20240701] Add St */
		ps_g_EMCTLFIL->s_setdata_fcr.actMode = AWS_MODE_IDLE;
		ps_g_EMCTLFIL->s_setdata_frr.actMode = AWS_MODE_IDLE;
		ps_g_EMCTLFIL->s_setdata_cm.actMode = AWS_MODE_IDLE;
		ps_g_EMCTLFIL->s_setdata_cm_exam.actMode = AWS_MODE_IDLE;
/* [20240701] Add St */

		/* 遠隔制御情報を初期化 */
		memset( ps_g_RTCTLINF, 0, sizeof( struct st_RTCTLINF ));
	}

	/*------------------------------------------*/
	/* デマンド制御の状態情報の初期化           */
	/*------------------------------------------*/
	/* 現在デマンド電力、予測デマンド電力を欠測に更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_CUR_POW, PV_TMFLG_DROPTM, 0 );
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_EST_POW, PV_TMFLG_DROPTM, 0 );

	/* デマンド制御情報を初期化 */
	memset( ps_g_DEMNDFIL, 0, sizeof( struct st_DEMNDFIL));

	/* デマンド閾値情報分ループ */
	for( loopCnt1 = 0; loopCnt1 < PV_MAX_DMD_THAD_LV; loopCnt1++ )
	{
		/* 現在デマンド電力超過を解除 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_CUR_LV1+loopCnt1, PVCTL_FLG_OFF, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		ps_g_DEMNDFIL->curLvOverFlg[loopCnt1] = PVCTL_FLG_OFF;

		/* 予測デマンド電力超過を解除 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_EST_LV1+loopCnt1, PVCTL_FLG_OFF, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		ps_g_DEMNDFIL->estLvOverFlg[loopCnt1] = PVCTL_FLG_OFF;
	}

	/* デマンド開始状態の状変を設定 */
	ps_g_DEMNDFIL->moniFlg = PVCTL_FLG_OFF;
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_MONI, ps_g_DEMNDFIL->moniFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );


	/* CEMS設定を更新 */
	/* BT充放電制御の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_BATTE_CTL, ps_g_CEMSPINF->btControl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	/* PV抑制制御の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_CTL, ps_g_CEMSPINF->pvControl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
/* [20230217] Del St */
	///* 受電電力一定制御の有効/無効を更新 */
	//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_RCVPWR_CTL, ps_g_CEMSPINF->rcvPowConst, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	///* 過充電回避制御の有効/無効を更新 */
	//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OVRCHG_CTL, ps_g_CEMSPINF->ovrChgAvoid, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	///* デマンド制御の有効/無効を更新 */
	//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DEMAND_CTL, ps_g_CEMSPINF->dmdControl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
/* [20230217] Del Ed */

	/* PV0%出力制御 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_ZERO_PER, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* UPSシャットダウンを解除 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_UPS_SHUTDOWN_EXE, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* パワコンの定格容量の合計を取得する */
	ps_g_EMCTLFIL->pvRatedPow = 0;
	for( loopCnt1 = 0; loopCnt1 < ps_gpt_PVSYSINF->s_machineInfo[ PV_MKIND_POWCN ].machineCnt; loopCnt1++ )
	{
		if( ps_g_POWCNINF[loopCnt1].statFlag == 1 )
			ps_g_EMCTLFIL->pvRatedPow += ps_g_POWCNINF[loopCnt1].ratedCap;
	}

/* [20230919] Del */

/* [20230823] Add St */
	/* 前回モードが停止だった場合 */
	if( ps_g_EMCTLFIL->cemsMode == PV_CTL_STOP )
	{
		/* 待機に設定 */
		ps_g_EMCTLFIL->cemsMode = PV_CTL_IDLE;
	}
/* [20230823] Add St */

	ps_g_EMCTLFIL->s1_timer_sec = 0;
	ps_g_EMCTLFIL->preTmFlg = PV_TMFLG_DROPTM;

	/* 制御開始カウントダウン */
	ps_g_EMCTLFIL->actCntdown = ps_g_CEMSPINF->ctlCountDown*10;

	/* 過充電回避制御PV発電停止フラグをOFF */
	ps_g_EMCTLFIL->ovrChgPvStop = PVCTL_FLG_OFF;

	/* 連系中放電停止フラグをOFF */
	ps_g_EMCTLFIL->rcvpDisStopFlg = PVCTL_FLG_OFF;

	/* 連系中充電停止フラグをOFF */
	ps_g_EMCTLFIL->rcvpChgStopFlg = PVCTL_FLG_OFF;

/* [20230919] Add St */
	/* 再計算フラグをOFF */
	ps_g_EMCTLFIL->reCalcFlg = PVCTL_FLG_OFF;

	/* PV出力制御(w)要求(目標)値を初期化 */
	ps_g_EMCTLFIL->pvReqPow=0;

	/* PV出力制御(0.01%)要求(目標)値を初期化 */
	ps_g_EMCTLFIL->pvSetPow=0;

	/* PV出力制御(0.01%)指示値(制御演算からの指示値)を初期化 */
	ps_g_EMCTLFIL->pvSetPer=0;

	/* バッテリ充放電電力要求(目標)値を初期化 */
	ps_g_EMCTLFIL->bttReqPow=0;

	/* バッテリ充放電電力指示値(制御演算からの指示値)を初期化 */
	ps_g_EMCTLFIL->bttSetPow=0;

	/* 蓄電池 放電可能最大値 を初期化*/ 
	ps_g_EMCTLFIL->avlDischgPow=0;

	/* 蓄電池 充電可能最大値 を初期化*/ 
	ps_g_EMCTLFIL->avlChgPow=0;
/* [20230919] Add Ed */

	/* 受電電力目標値、変化量を欠測に更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_RCV_POW_TARGET, PV_TMFLG_DROPTM, 0 );

	return(0);
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 制御メイン処理
 *
 * 機能：
 * 		モード判定とモードによる処理を行う
 * 戻り値：
 */
int func_mode_ctl_main( int stopFlag )
{
	int						nextMode;					/* 次回制御モード */
	int						retCode;					/* 関数戻り値格納用 */
	int						tmFlgBt;					/* 計測ステータス */
	int						tmFlgPv;					/* 計測ステータス */
	int						tmFlgBg;					/* 計測ステータス */
	int						svStat = 0;

	/* モード判定 */
	nextMode = func_mode_check( ps_g_EMCTLFIL->cemsMode );

	/* S1:停止チェック */
	func_s1_check( &nextMode );

	/* モード処理 */
	func_mode_act( nextMode );

	/* BT制御条件チェック */
	retCode = func_bt_ctl_check( nextMode );
	if( retCode < 0 )
	{
		/* 計測ステータスを欠測に設定 */
		tmFlgBt = PV_TMFLG_DROPTM;
	}
	else
	{
		
		/* 計測ステータスを通常に設定 */
		tmFlgBt = PV_TMFLG_NORMAL;
	}

	/* PV制御条件チェック */
	retCode = func_pv_ctl_check( nextMode );
	if( retCode < 0 )
	{
		/* 計測ステータスを欠測に設定 */
		tmFlgPv = PV_TMFLG_DROPTM;
	}
	else
	{
		/* 計測ステータスを通常に設定 */
		tmFlgPv = PV_TMFLG_NORMAL;
	}

	/* PV発電停止を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_ZERO_PER, &svStat, ps_g_SVSTSFIL);
	if( svStat == 1 )
	{
		ps_g_EMCTLFIL->pvReqPow = 0;
	}

	/* 停止指示を受けた場合、0kW,0%の即時指示を行う */
	if( stopFlag == 1  )
	{
		/* BT制御値を算出する */
		func_bttPowSet( &ps_g_EMCTLFIL->bttSetPow, 0 );		/* [20230301] Chg */
		/* PV制御値を算出する */
		func_pvPowSet( &ps_g_EMCTLFIL->pvSetPow, 0 );		/* [20230301] Chg */
		ps_g_EMCTLFIL->pvSetPer = (int)(( (unsigned long long )ps_g_EMCTLFIL->pvSetPow * 100 * 100 ) / ps_g_EMCTLFIL->pvRatedPow);
	}
	else
	{
		/* BT制御値を算出する */
		func_bttPowSet( &ps_g_EMCTLFIL->bttSetPow, ps_g_EMCTLFIL->bttReqPow );				/* [20230301] Chg */

		/* PV制御値を算出する */
		/* PV発電量を出力制御目標値(0.01%換算)に変換 */
		if( ps_g_EMCTLFIL->pvRatedPow == 0 )
		{
			/* 定格が0wの場合は、0%で設定 */
			ps_g_EMCTLFIL->pvSetPer = 0;
		}
		else
		{
			func_pvPowSet( &ps_g_EMCTLFIL->pvSetPow, ps_g_EMCTLFIL->pvReqPow );				/* [20230301] Chg */
			ps_g_EMCTLFIL->pvSetPer = (int)(( (unsigned long long )ps_g_EMCTLFIL->pvSetPow * 100 * 100 ) / ps_g_EMCTLFIL->pvRatedPow);
		}
	}

	/* 蓄電池制御指示を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_BT_SET_POW, tmFlgBt, ps_g_EMCTLFIL->bttSetPow );
	/* PV出力抑制指示を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PV_SET_PER, tmFlgPv, ps_g_EMCTLFIL->pvSetPer );

	/* 蓄電池制御目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_BT_REQ_POW, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->bttReqPow );
	/* PV発電電力目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PV_REQ_POW, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->pvReqPow );
	/* PV出力抑制目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PV_REQ_PER, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->pvSetPow );


	/* 遠隔制御、状態遷移時 */
	func_rmt_ctl_main( nextMode );

	/* 状態を更新 */
	ps_g_EMCTLFIL->cemsMode = nextMode;

	/* CEMS モードを更新 */
	func_mode_sv_update( nextMode );

#ifdef DEBUG_MODE_CMN
	sprintf( errMsg, "func_mode_ctl_main  : btt:Set=%d(Req=%d), pv:Set=%d/%d(Req=%d)", 
	ps_g_EMCTLFIL->bttSetPow, ps_g_EMCTLFIL->bttReqPow,
	ps_g_EMCTLFIL->pvSetPow, ps_g_EMCTLFIL->pvSetPer, ps_g_EMCTLFIL->pvReqPow );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_CMN

}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * マニュアルモード（遠隔制御）処理
 *
 * 機能：
 * 		遠隔制御の処理を行う。
 * 戻り値：
 */
int func_rmt_ctl_main( int nowMode )
{
	int						retCode;
	int						svStat = 0;
	int						svNumb = 0;
	char					deleteName[256];			/* 削除ファイル名 */
	struct st_ctlSvInf		ctlSvInf;					/* 制御管理情報 */
	int						retCheck;					/* 制御チェック結果 */
	char					execCmd[256];


	/* 遠隔制御が終了/強制終了した場合、且つ、ファイルを削除していない場合 */
	if( ( ( ps_g_PVCTLFIL[PV_CTL_TYPE_CLOUD].ctlState == PV_CTL_STATE_FIN ) ||
		  ( ps_g_PVCTLFIL[PV_CTL_TYPE_CLOUD].ctlState == PV_CTL_STATE_FORCE ) ) &&
		( ps_g_RTCTLINF->filename[0] != 0 ) )
	{
		/* 運用中のファイルパスを取得 */
		memset( deleteName, 0x00, sizeof(deleteName));
		sprintf( deleteName, "%s%s", PV_PATH_DWL, ps_g_RTCTLINF->filename);

		/* ファイルが存在しているか確認 */
		if( access( deleteName, R_OK | W_OK ) == 0 )
		{
			/* ファイルを削除する */
			unlink( deleteName );
		}
		/* 遠隔制御情報を初期化 */
		memset( ps_g_RTCTLINF, 0, sizeof( struct st_RTCTLINF ));
	}

	/* 制御指示ファイル読み込みチェック */
	retCode = pvctl_rmtctl_file_read_check();

	/* 遠隔制御ファイルが設定済み、且つ、制御状態が実行中以外の場合 */
	if( ( retCode == 1 ) &&
		( ps_g_PVCTLFIL[PV_CTL_TYPE_CLOUD].ctlState != PV_CTL_STATE_EXE ))
	{
		
		svStat = 0;
		svNumb = 0;
#ifdef DEBUG_RMT
		printf("func_rmt_ctl_main : ctlMode[%d] start\n", ps_g_RTCTLINF->ctlMode );
#endif //DEBUG_RMT

		/* モード別の切り替え条件チェックを実施 */
		switch( ps_g_RTCTLINF->ctlMode )
		{
/* モード切替は非サポート */
#if 0
//			/* 自立運転切替 */
//			case PV_CTL_SW_OFFGRID :
//				svNumb = PV_SYS_SV_MODE_ON_TO_OFF;
//				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_SW_ON_TO_OFF, &svStat, ps_g_SVSTSFIL);
//				break;
//
//			/* 連系運転切替ステップ1 */
//			case PV_CTL_SW_ONGRID_1 :
//				svNumb = PV_SYS_SV_MODE_OFF_TO_ON_1;
//				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_SW_OFF_TO_ON_1, &svStat, ps_g_SVSTSFIL);
//				break;
//
//			/* 連系運転切替ステップ2 */
//			case PV_CTL_SW_ONGRID_2 :
//				svNumb = PV_SYS_SV_MODE_OFF_TO_ON_2;
//				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_SW_OFF_TO_ON_2, &svStat, ps_g_SVSTSFIL);
//				break;
//
//			/* 連系運転切替ステップ3 */
//			case PV_CTL_SW_ONGRID_3 :
//				svNumb = PV_SYS_SV_MODE_OFF_TO_ON_3;
//				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_SW_OFF_TO_ON_3, &svStat, ps_g_SVSTSFIL);
//				break;
#endif
			/* 遠隔制御 */
			case PV_CTL_SW_RMT :
				svNumb = PV_SYS_SV_MODE_REMOTE;
				svStat = 1;
				break;
			default :
				break;
		}

		/* モード切替の場合 */
		if( svNumb != 0 )
		{
			/* モード切替の状変を行う */
			commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_SET, PV_MKIND_PVSYS, 1, svNumb, 1, ps_g_RTCTLINF->userNo, svNumb-1 );
		}
		
		/* モード切替条件を満たしている場合 */
		if( svStat == 1 )
		{
#ifdef DEBUG_RMT
		printf("func_rmt_ctl_main : commfunc_pvctl_start call\n" );
#endif //DEBUG_RMT
			/* 制御の開始を行う */
			commfunc_pvctl_start( PV_CTL_TYPE_CLOUD, ps_g_RTCTLINF->ctlMode, ps_g_RTCTLINF->userNo, ps_g_RTCTLINF->ctlNum, ps_g_RTCTLINF->s_ctlinfo, ps_g_RTCTLINF->filename );
/* [20221011] Add St */
			/* 制御通し番号を設定する */
			memcpy(ps_g_CTLJHFIL->rmtCtlNumb, ps_g_RTCTLINF->rmtCtlNumb, sizeof(ps_g_CTLJHFIL->rmtCtlNumb));
/* [20221011] Add Ed */
		}
		else
		{
			/* モード切替の場合 */
			if( svNumb != 0 )
			{
				/* 切替条件が不可の場合、操作失敗の状変を行う */
				commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, PV_MKIND_PVSYS, 1, svNumb, 0, ps_g_RTCTLINF->userNo, svNumb-1 );
			}

			/* 運用中のファイルパスを取得 */
			memset( deleteName, 0x00, sizeof(deleteName));
			sprintf( deleteName, "%s%s", PV_PATH_DWL, ps_g_RTCTLINF->filename);

			/* ファイルが存在しているか確認 */
			if( (ps_g_RTCTLINF->filename[0] != 0 ) &&
				(access( deleteName, R_OK | W_OK ) == 0 ) )
			{
				/* ファイルを削除する */
				unlink( deleteName );
			}
			/* 遠隔制御情報を初期化 */
			memset( ps_g_RTCTLINF, 0, sizeof( struct st_RTCTLINF ));
		}
	}

#ifdef DEBUG_MODESW
	printf("nowMode=%d, nextMode=%d\n", ps_g_EMCTLFIL->cemsMode, nowMode );
#endif
	/* 遠隔制御、モード遷移制御処理 */
	if( ps_g_EMCTLFIL->cemsMode != nowMode )
	{
		/* 切替時の制御が設定されている場合 */
		if( ps_g_MDCTLINF[nowMode].statFlag == 1 )
		{
			/* 制御の開始を行う */
			commfunc_pvctl_start( PV_CTL_TYPE_MODESW, nowMode, 0, ps_g_MDCTLINF[nowMode].ctlNum, ps_g_MDCTLINF[nowMode].s_ctlinfo, NULL );
		}
	}


	/* 制御動作チェック */
	commfunc_pvctl_actCheck( ps_g_SVSTSFIL );

	/* 制御チェック */
	retCheck = commfunc_pvctl_ctlCheck( PV_MKIND_PVSYS, 1, &ctlSvInf );
	if( retCheck == 1 )
	{
		/* シャットダウン制御、UPSシャットダウン、運転開始/停止の制御、PV発電0kW出力の場合は、SVのみ変更する */
		if( ( ctlSvInf.svNo == PV_SYS_SV_SHUTDOWN_EXE ) ||
			( ctlSvInf.svNo == PV_SYS_SV_UPS_SHUTDOWN_EXE ) ||
			( ctlSvInf.svNo == PV_SYS_SV_PV_RUN_START ) ||
			( ctlSvInf.svNo == PV_SYS_SV_PV_RUN_STOP ) ||
			( ctlSvInf.svNo == PV_SYS_SV_PV_ZERO_PER ) )
		{
#ifdef DEBUG_RMT
			printf("call commfunc_pvctl_resSet( 0 ) svno=%d\n", ctlSvInf.svNo);
#endif //DEBUG_RMT
			/* シャットダウンの場合は、状変をせずにシャットダウンコマンドを実行 */
			if( ctlSvInf.svNo == PV_SYS_SV_SHUTDOWN_EXE)
			{
				sprintf(execCmd, "/usr/bin/touch %s%s", PV_PATH_DAT, PV_FNAME_PV_SHUTDOWN );
				system( execCmd );
			}
			else
			{
				/* 状変を更新 */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, ctlSvInf.svNo, ctlSvInf.svValue, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
			}
			/* 処理結果を正常で通知 */
			commfunc_pvctl_resSet( 0, &ctlSvInf );
		}
		else
		{
#ifdef DEBUG_RMT
			printf("call commfunc_pvctl_resSet( -1) svno=%d\n", ctlSvInf.svNo);
#endif //DEBUG_RMT

			/* 処理結果を異常で通知 */
			commfunc_pvctl_resSet( -1, &ctlSvInf );
		}
	}
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * モード処理
 *
 * 機能：
 * 		デマンド監視の開始/終了と、モードの処理を行う。
 * 戻り値：
 */
int func_mode_act( int nowMode )
{
	int				svStat;
	int				svStat2;
	int				svStat3;
/* [20250807] Add St */
	int				svStatFcrCtl;
/* [20250807] Add Ed */
	int				reCalcFlg = PVCTL_FLG_OFF;
	int				preSpr;

	/* 充電禁止解除SoCを算出 */

	/* S3:自立運転中、S6:自立運転中(障害)以外の場合 */
	if( ( nowMode != PV_CTL_OFFGRID ) &&
		( nowMode != PV_CTL_ERR_OFFGRID ))
	{
		/* PV発電禁止(自立中)をOFFに設定 */
		ps_g_EMCTLFIL->ovrChgPvStop = PVCTL_FLG_OFF;
		
		/* デマンド監視 */
		func_demand();
	}
	else
	{
		/* デマンド時限終了処理 */
		func_dmd_moni_end();

		/* デマンド監視設定を「無し」に設定 */
		ps_g_DEMNDFIL->moniFlg = PVCTL_FLG_OFF;

		/* デマンド開始状態の状変を設定 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_MONI, ps_g_DEMNDFIL->moniFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		
	}

	/* S4:連系運転中、S7:連系運転中(障害)以外の場合 */
	if( ( nowMode != PV_CTL_ONGRID ) &&
		( nowMode != PV_CTL_ERR_ONGRID ))
	{
		/* PV発電/充電禁止(連系中)をOFFに設定 */
		ps_g_EMCTLFIL->rcvpChgStopFlg = PVCTL_FLG_OFF;

		/* 放電禁止(連系中)をOFFに設定 */
		ps_g_EMCTLFIL->rcvpDisStopFlg = PVCTL_FLG_OFF;

	}

/* [20240221] Chg Start */
	/* 受電電力一定制御の有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_RCVPWR_CTL, &svStat, ps_g_SVSTSFIL);

	/* SoC制御の有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_SOC_CTL, &svStat2, ps_g_SVSTSFIL);
/* [20240221] Chg End */

/* [20250205] Add St */
	/* 受電電力を設定 */
	func_avlData_Set( &s_rcvPowAve, s_sysDataAv.tmFlgData[PV_SYS_TM_RCV_POW-1], s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW-1] );

	/* 負荷電力を設定 */
	func_avlData_Set( &s_loadPowAve, s_sysDataAv.tmFlgData[PV_SYS_TM_LOAD_POW-1], s_sysDataAv.tmValData[PV_SYS_TM_LOAD_POW-1] );
/* [20250205] Add Ed */
/* [20250807] Add St */
	/* 一次調整力制御を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL, &svStatFcrCtl, ps_g_SVSTSFIL);
/* [20250807] Add Ed */

/* [20250828] Chg St */
	/* 蓄電池が設置されている場合 */
	if( ps_g_BATTEINF != NULL )
	{
		/* S4:連系運転中の場合 */
		if( nowMode == PV_CTL_ONGRID )
		{
			/* SoC制御の場合 */
			if( svStat2 == PVCTL_FLG_ON )
			{
				/* SoC制御 */
				func_mode_S4_StlSoC(reCalcFlg);
			}
/* [20250807] Add St */
			/* 一次調整力制御の場合 */
			else if( svStatFcrCtl == PVCTL_FLG_ON )
			{
				func_mode_S4_fcr();
			}
/* [20250807] Add Ed */
/* [20240221] Chg Start */
			/* 受電電力一定制御の場合 */
			else if( svStat == PVCTL_FLG_ON )
			{
				/* 連系中制御 */
				func_mode_S4(reCalcFlg);
			}
/* [20240221] Chg End */
/* [20240305] Add Start */
			else
			{
				/* BT制御目標値を更新 */
				ps_g_EMCTLFIL->bttReqPow = 0;
			}
/* [20240305] Add End */
		}
		else
		{
/* [20250205] Del */
			/* S3:自立運転中の場合 */
			if( nowMode == PV_CTL_OFFGRID )
			{
				/* 過充電回避制御 */
				func_mode_S3();
			}
			/* S7:連系中（障害発生）の場合 */
			else if( nowMode == PV_CTL_ERR_ONGRID )
			{
				/* PV発電電力追従 */
				func_mode_S7();
			}
			else
			{
				/* BT制御目標値を更新 */
				ps_g_EMCTLFIL->bttReqPow = 0;
			}
		}
	}
/* [20250828] Chg Ed */

	/* 受電電力平均値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_AVE_RCV_POW, s_rcvPowAve.tmAvlFlg, s_rcvPowAve.tmAvlVal );
	/* 負荷電力平均値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_AVE_LOAD_POW, s_loadPowAve.tmAvlFlg, s_loadPowAve.tmAvlVal );
	/* 周波数平均値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_AVE_GRID_FREQ, s_freqAve.tmAvlFlg, s_freqAve.tmAvlVal );	/* [20260514] Add */
//commfunc_syslog_msgset( pv_own_name, errMsg );

	/* PV発電禁止(自立中)を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OFFGRID_CHG_BAN, ps_g_EMCTLFIL->ovrChgPvStop, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* PV発電/充電禁止(連系中)を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_CHG_BAN, ps_g_EMCTLFIL->rcvpChgStopFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	/* 放電禁止(連系中)を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_DISCHG_BAN, ps_g_EMCTLFIL->rcvpDisStopFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 運転モードSV更新処理
 *
 * 機能：
 * 		運転モードSVの更新を行う。
 * 戻り値：
 */
void func_mode_sv_update( int nowMode )
{
	int			sv_numb;			/* SV番号 */
	int			sv_stat=1;			/* SV値 */
	int			loopCnt1;

	sv_numb = nowMode + PV_SYS_SV_MODE_STOP;

	/* 運転モード数分ループ */
	for( loopCnt1 = 0; loopCnt1 < PV_CTL_MAX; loopCnt1++ )
	{
		/* 更新するSV番号を設定 */
		sv_numb = PV_SYS_SV_MODE_STOP + ((nowMode + loopCnt1) % PV_CTL_MAX);

		/* 状変を実行 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, sv_numb, sv_stat, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

		/* 初回以降は、SV値は0に設定 */
		sv_stat = 0;
	}

	return;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 停止モード遷移監視
 *
 * 機能：
 * 		停止への遷移チェックと実施を行う
 * 戻り値：
 */
int func_s1_check( int *nextMode )
{
	int	 					svStat;
	struct timeval			nowTime;

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, NULL);

/* [20220726] Chg St */
	/* 状態取得：UPSシャットダウン条件 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_UPS_LOWBATT, &svStat, ps_g_SVSTSFIL);
	/* UPSシャットダウン条件がONになった場合 */
	if( svStat == 1 )
	{
		/* S1:停止 */
		*nextMode = PV_CTL_STOP;
	}
	else if( ( *nextMode == PV_CTL_OFFGRID) ||		/* S3:自立運転中 */
			 ( *nextMode == PV_CTL_ERR_OFFGRID ))	/* S6:自立運転中(障害) */
	{
		/* タイマ開始時刻初期化 */
		ps_g_EMCTLFIL->s1_timer_sec = 0;
	}
	else
	{
		/* 状態取得：シャットダウンタイマ開始 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_SHUTDOWN_TIMER_START, &svStat, ps_g_SVSTSFIL);		/* [20230302] Chg */

		/* 停電を検出した場合 */
		if( svStat == 1 )
		{
			/* S1:停止タイマが未設定の場合 */
			if( ps_g_EMCTLFIL->s1_timer_sec == 0 )
			{
				/* 停止タイマの開始時刻を設定 */
				ps_g_EMCTLFIL->s1_timer_sec = nowTime.tv_sec;
			}
			else
			{
				/* シャットダウン待機時間を超えた場合 */
				if( ( nowTime.tv_sec - ps_g_EMCTLFIL->s1_timer_sec ) > (ps_g_CEMSPINF->shutdownWait*60) )
				{
					/* S1:停止 */
					*nextMode = PV_CTL_STOP;
				}
			}
		}
		else
		{
			/* タイマ開始時刻初期化 */
			ps_g_EMCTLFIL->s1_timer_sec = 0;
		}
	}
/* [20220726] Chg Ed */

	return 0;
}
#if 1
int func_mode_check( int nowMode )
{
	int	 		svStat1;
	int	 		svStat2;
	int	 		svStat3;
	int			nextMode;
	int	 		svStatOngrid;
	int	 		svStatOffgrid;

/* [20250828] Chg St */
	/* 現在のモードを設定 */
	nextMode = nowMode;

	/* 蓄電池が設置されている場合 */
	if( ps_g_BATTEINF != NULL )
	{
		/* 需要家蓄電池（連系自立切り替え自動） */
		if( ps_g_BATTEINF->batteType == BT_TYPE_CONSUMER_AUTO_OFFGRID )
		{
			/* 状態取得：自立状態取得 */
			commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_OFFGRID_STATE, &svStat1, ps_g_SVSTSFIL);

			/* 状態取得：重故障(共通) */
			commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_CMMON, &svStat2, ps_g_SVSTSFIL);


			/* 自立状態判定 */
			if( svStat1 == 1 )
			{
				/* 状態取得：重故障(自立) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_OFFGRID, &svStat3, ps_g_SVSTSFIL);

				/* 重故障(共通) 、重故障(自立)、判定 */
				if( ( svStat2 == 1 ) ||
				    ( svStat3 == 1 ) )
				{
					/* S6:自立運転中(障害) */
					nextMode = PV_CTL_ERR_OFFGRID;
				}
				else
				{
					/* S3:自立運転中 */
					nextMode = PV_CTL_OFFGRID;
				}
			}
			else
			{
				/* 状態取得：重故障(連系) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_ONGRID, &svStat3, ps_g_SVSTSFIL);

				/* 重故障(共通) 、重故障(連系)、判定 */
				if( ( svStat2 == 1 ) ||
				    ( svStat3 == 1 ) )
				{
					/* S7:連系運転中(障害) */
					nextMode = PV_CTL_ERR_ONGRID;
				}
				else
				{
					/* S4:連系運転中 */
					nextMode = PV_CTL_ONGRID;
				}
			}
		}
		/* 蓄電所（自立なし） */
		else if( ps_g_BATTEINF->batteType == BT_TYPE_STORAGE_NO_OFFGRID )
		{
			nextMode = nowMode;

			/* 状態取得：重故障(共通) */
			commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_CMMON, &svStat1, ps_g_SVSTSFIL);
			if( svStat1 == EMCTL_FLG_ON )
			{
				/* 前回のCEMSモードによって、遷移先を設定 */
				switch( nowMode )
				{
					case PV_CTL_STOP :			/* S1:停止 */
						nextMode = PV_CTL_IDLE;
						break;
					
					case PV_CTL_IDLE :			/* S2:待機 */
					case PV_CTL_ERR_IDLE :		/* S5:待機(障害) */
						nextMode = PV_CTL_ERR_IDLE;
						break;

					case PV_CTL_OFFGRID :		/* S3:自立運転中 */
					case PV_CTL_ERR_OFFGRID :	/* S6:自立運転中(障害) */

						nextMode = PV_CTL_ERR_OFFGRID;

						/* 状態取得：蓄電システム通信異常 */
						commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_CON_ERR, &svStat1, ps_g_SVSTSFIL);

						/* 状態取得：BT待機設定 */
						commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_IDLE_STATE, &svStat2, ps_g_SVSTSFIL);

						/* 通信異常ではない、且つ、IDLEの場合 */
						if( ( svStat1 == EMCTL_FLG_OFF ) &&
							( svStat2 == EMCTL_FLG_ON ))
						{
							/* S2:待機 に遷移 */
							nextMode = PV_CTL_IDLE;	/* [20230928] Chg */
						}
						break;

					case PV_CTL_ONGRID :		/* S4:連系運転中 */
					case PV_CTL_ERR_ONGRID :	/* S7:連系運転中(障害) */
						nextMode = PV_CTL_ERR_ONGRID;	

						/* 状態取得：52R取得機器通信異常 */
						commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_52R_CON_ERR, &svStat1, ps_g_SVSTSFIL);

						/* 状態取得：52R(切) */
						commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_52R_OFF_STATE, &svStat2, ps_g_SVSTSFIL);

/* [20230926] Chg St */
						/* 通信異常ではない、且つ、52RがOFFの場合 */
						if( ( svStat1 == EMCTL_FLG_OFF ) &&
							( svStat2 == EMCTL_FLG_ON ) )
/* [20230926] Chg Ed */
						{
							/* S2:待機 に遷移 */
							nextMode = PV_CTL_IDLE;	/* [20230928] Chg */
						}
						break;
				}
			}
			else
			{
/* [20230926] Chg St */
/* [20230922] Chg St */
/* [20230814] Chg St */
				/* 状態取得：52R(入) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_52R_ON_STATE, &svStat1, ps_g_SVSTSFIL);
				/* 状態取得：52R(切) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_52R_OFF_STATE, &svStat2, ps_g_SVSTSFIL);

				/* 52R(入)、52R(切)がどちらも発生していない場合 */
				if( ( svStat1 == EMCTL_FLG_OFF ) &&
				    ( svStat2 == EMCTL_FLG_OFF ) )
				{
					/* 前回モードのまま */
				}
				/* 52RがONの場合 */
				else if( svStat1 == EMCTL_FLG_ON )
				{
					/* BTが連系設定 */
					/* 状態取得：重故障(S4:連系運転) */
					commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_ONGRID, &svStat1, ps_g_SVSTSFIL);
					if( svStat1 == EMCTL_FLG_ON )
					{
						/* S7:連系運転中(障害) */
						nextMode = PV_CTL_ERR_ONGRID;
					}
					else
					{
						/* S4:連系運転中 */
						nextMode = PV_CTL_ONGRID;
					}
				}
				/* 52RがOFFの場合 */
				else //if( svStat2 == EMCTL_FLG_ON )
				{
					/* 状態取得：重故障(S2:待機) */
					commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_IDLE, &svStat1, ps_g_SVSTSFIL);
					if( svStat1 == EMCTL_FLG_ON )
					{
						/* S5:待機(障害) */
						nextMode = PV_CTL_ERR_IDLE;
					}
					else
					{
						/* S2:待機 */
						nextMode = PV_CTL_IDLE;
					}
				}
/* [20230814] Chg Ed */
/* [20230922] Chg Ed */
/* [20230926] Chg Ed */
			}
		}
	}
/* [20250828] Chg Ed */

	return nextMode;
}
#else
int func_mode_check( int nowMode )
{
	int			nextMode;
	int	 		svStat;
	int	 		svStat2;
	int	 		svStat3;		/* [20220809] Add */
	int	 		svStatOngrid;
	int	 		svStatOffgrid;

	nextMode = nowMode;

	/* 状態取得：重故障(共通) */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_CMMON, &svStat, ps_g_SVSTSFIL);
	if( svStat == EMCTL_FLG_ON )
	{
		/* 前回のCEMSモードによって、遷移先を設定 */
		switch( nowMode )
		{
			case PV_CTL_STOP :			/* S1:停止 */
				nextMode = PV_CTL_IDLE;
				break;
			
			case PV_CTL_IDLE :			/* S2:待機 */
			case PV_CTL_ERR_IDLE :		/* S5:待機(障害) */
				nextMode = PV_CTL_ERR_IDLE;
				break;

			case PV_CTL_OFFGRID :		/* S3:自立運転中 */
			case PV_CTL_ERR_OFFGRID :	/* S6:自立運転中(障害) */

				nextMode = PV_CTL_ERR_OFFGRID;

				/* 状態取得：蓄電システム通信異常 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_CON_ERR, &svStat, ps_g_SVSTSFIL);

				/* 状態取得：BT待機設定 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_IDLE_STATE, &svStat2, ps_g_SVSTSFIL);

				/* 通信異常ではない、且つ、IDLEの場合 */
				if( ( svStat == EMCTL_FLG_OFF ) &&
					( svStat2 == EMCTL_FLG_ON ))
				{
					/* S2:待機 に遷移 */
					nextMode = PV_CTL_ERR_IDLE;
				}

/* [20220809] Add St */
				/* 状態取得：UVR検出 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_UVR, &svStat, ps_g_SVSTSFIL);

				/* 状態取得：BT通信異常 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_CON_ERR, &svStat2, ps_g_SVSTSFIL);

				/* UVR検出、且つ、BT通信が正常の場合 */
				if( ( svStat == EMCTL_FLG_ON) &&
					( svStat2 == EMCTL_FLG_OFF ))
				{
/* [20220810] Chg St */
					/* S3:自立運転中 だった場合 */
					if( nowMode == PV_CTL_OFFGRID )
					{
						/* S2:待機 に遷移 */
						nextMode = PV_CTL_IDLE;
					}
					else
					{
						/* S5:待機(障害) に遷移 */
						nextMode = PV_CTL_ERR_IDLE;
					}
/* [20220810] Chg Ed */
				}
/* [20220809] Add Ed */
				break;

			case PV_CTL_ONGRID :		/* S4:連系運転中 */
			case PV_CTL_ERR_ONGRID :	/* S7:連系運転中(障害) */
				nextMode = PV_CTL_ERR_ONGRID;	

/* [20220809] Chg St */
				/* 状態取得：UVR検出 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_UVR, &svStat, ps_g_SVSTSFIL);
/* [20220809] Chg Ed */

				/* 状態取得：BT通信異常 */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_CON_ERR, &svStat2, ps_g_SVSTSFIL);

				/* UVR検出、且つ、BT通信が正常の場合 */
				if( ( svStat == EMCTL_FLG_ON) &&
					( svStat2 == EMCTL_FLG_OFF ))
				{
/* [20220810] Chg St */
					/* S4:連系運転中 だった場合 */
					if( nowMode == PV_CTL_ONGRID )
					{
						/* S2:待機 に遷移 */
						nextMode = PV_CTL_IDLE;
					}
					else
					{
						/* S5:待機(障害) に遷移 */
						nextMode = PV_CTL_ERR_IDLE;
					}
/* [20220810] Chg Ed */
				}
				break;
		}
	}
	else
	{
		/* 状態取得：BT連系設定 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_ONGRID_STATE, &svStatOngrid, ps_g_SVSTSFIL);

		/* 状態取得：BT自立設定 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_OFFGRID_STATE, &svStatOffgrid, ps_g_SVSTSFIL);

/* [20220809] Chg St */
		/* 状態取得：UVR検出 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_UVR, &svStat, ps_g_SVSTSFIL);
/* [20220809] Chg Ed */

		/* 状態取得：BT通信異常 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BT_CON_ERR, &svStat2, ps_g_SVSTSFIL);

		/* 連系・自立の場合、UVR検出、且つ、BT通信が正常の場合 */
		if( (( svStatOngrid == EMCTL_FLG_ON ) ||
			 ( svStatOffgrid == EMCTL_FLG_ON ) ) &&
			( svStat == EMCTL_FLG_ON) &&
			( svStat2 == EMCTL_FLG_OFF ))
		{
			/* S2:待機 に遷移 */
			nextMode = PV_CTL_IDLE;		/* [20220810] Chg */
		}
		else
		{
			/***********/
			/* ON-Grid */
			/***********/
			/* BT連系設定 */
			if( svStatOngrid == EMCTL_FLG_ON )
			{
				/* BTが連系設定 */
				/* 状態取得：重故障(S4:連系運転) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_ONGRID, &svStat, ps_g_SVSTSFIL);
				if( svStat == EMCTL_FLG_ON )
				{
					/* S7:連系運転中(障害) */
					nextMode = PV_CTL_ERR_ONGRID;
				}
				else
				{
					/* S4:連系運転中 */
					nextMode = PV_CTL_ONGRID;
				}
			}
			/************/
			/* OFF-Grid */
			/************/
			/* BT自立設定 */
			else if( svStatOffgrid == EMCTL_FLG_ON )
			{
				/* BTが自立設定 */
				/* 状態取得：重故障(S3:自立運転) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_OFFGRID, &svStat, ps_g_SVSTSFIL);
				if( svStat == EMCTL_FLG_ON )
				{
					/* S6:自立運転中(障害) */
					nextMode = PV_CTL_ERR_OFFGRID;
				}
				else
				{
					/* S3:自立運転中 */
					nextMode = PV_CTL_OFFGRID;
				}
			}
			else
			{
				/* 状態取得：重故障(S2:待機) */
				commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ERR_IDLE, &svStat, ps_g_SVSTSFIL);
				if( svStat == EMCTL_FLG_ON )
				{
					/* S5:待機(障害) */
					nextMode = PV_CTL_ERR_IDLE;
				}
				else
				{
					/* S2:待機 */
					nextMode = PV_CTL_IDLE;
				}
			}
		}
	}

#ifdef DEBUG_MODE_CMN
	sprintf( errMsg, "func_mode_check  : nowMode=%d, nextMode=%d", nowMode, nextMode );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_CMN

	return nextMode;
}
#endif

int	func_tmmoni_update( struct st_sysData  *ps_sysData )
{
	long long				avltmLVal;					/* 計測値演算用（移動平均値用）*/
	int						loopCnt1;					/* ループカウンタ */
	int						retCode = 0;				/* 関数戻り値 */
	int						svStat;						/* 状態情報 */

	/* 計測情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_TMSTSFIL );


	/* TM監視情報数分処理を繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_PVSYS_MONSVPOS ; loopCnt1++ )
	{
		/* TM監視情報が未実装の場合 */
		if( ps_g_TMMONINF[loopCnt1].statFlag == 0 )
			continue;

		/* 現在状態を取得 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, ps_g_TMMONINF[loopCnt1].svNumb, &svStat, ps_g_SVSTSFIL);

		/* 正常以外の場合は、更新しない */
		if( ps_g_TMSTSFIL[ps_g_TMMONINF[loopCnt1].tmIfLink].avltmFlag != PV_TMFLG_NORMAL )
		{
			continue;
		}
		/* 現在計測値を取得 */
		avltmLVal = ps_g_TMSTSFIL[ps_g_TMMONINF[loopCnt1].tmIfLink].avltmVal;

		/* 監視種別が以上の場合 */
		if( ps_g_TMMONINF[loopCnt1].moniType == PV_MONI_TYPE_ABOVE )
		{
			/* SV値がOFFの場合 */
			if( svStat == 0 )
			{
				/* 基準値以上の場合(境界含む) */
				if( ps_g_TMMONINF[loopCnt1].baseVal <= avltmLVal )
				{
					svStat = 1;
				}
			}
			/* SV値がONの場合 */
			else
			{
				/* 基準値－不感帯より小さい場合 */
				if( avltmLVal < ( ps_g_TMMONINF[loopCnt1].baseVal - ps_g_TMMONINF[loopCnt1].deadband ))
				{
					svStat = 0;
				}
				
			}
		}
		/* 監視種別が以下の場合 */
		else if( ps_g_TMMONINF[loopCnt1].moniType == PV_MONI_TYPE_BELOW )
		{
			/* SV値がOFFの場合 */
			if( svStat == 0 )
			{
				/* 基準値以下の場合(境界含む) */
				if( avltmLVal <= ps_g_TMMONINF[loopCnt1].baseVal )
				{
					svStat = 1;
				}
			}
			/* SV値がONの場合 */
			else
			{
				/* 基準値＋不感帯より大きい場合 */
				if( avltmLVal > ( ps_g_TMMONINF[loopCnt1].baseVal + ps_g_TMMONINF[loopCnt1].deadband ))
				{
					svStat = 0;
				}
				
			}
		}
		/* 監視種別が範囲の場合 */
		else //if( ps_g_TMMONINF[loopCnt1].moniType == PV_MONI_TYPE_RANGE )
		{
			/* SV値がOFFの場合 */
			if( svStat == 0 )
			{
				/* 基準値±誤差の場合(境界含む) */
				if( (( ps_g_TMMONINF[loopCnt1].baseVal - ps_g_TMMONINF[loopCnt1].gosa ) <= avltmLVal ) &&
				    ( avltmLVal <= ( ps_g_TMMONINF[loopCnt1].baseVal + ps_g_TMMONINF[loopCnt1].gosa )))
				{
					svStat = 1;
				}
			}
			/* SV値がONの場合 */
			else
			{
				/* 基準値±誤差±不感帯の外の場合(境界含まない) */
				if( ( avltmLVal < ( ps_g_TMMONINF[loopCnt1].baseVal - ps_g_TMMONINF[loopCnt1].gosa - ps_g_TMMONINF[loopCnt1].deadband )) ||
				    ( ( ps_g_TMMONINF[loopCnt1].baseVal + ps_g_TMMONINF[loopCnt1].gosa + ps_g_TMMONINF[loopCnt1].deadband ) < avltmLVal))
				{
					svStat = 0;
				}
				
			}
		}


		commfunc_johen_pos( PV_MKIND_PVSYS, 1, ps_g_TMMONINF[loopCnt1].svNumb, svStat, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	/* 計測情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_TMSTSFIL );

	return( 0 );
}

int	func_tmagg_update( struct st_sysData  *ps_sysData )
{
	int						avltmFlag;					/* 計測フラグ */
	long long				avltmLVal;					/* 計測値演算用（移動平均値用）*/
	long long				workLVal;					/* 演算用ワーク                */
	int						avlnormData;				/* 正常データ有りフラグ */
	int						tmIfLink;					/* リンク情報 */
	int						aggtmIfLink;				/* リンク情報 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						retCode = 0;				/* 関数戻り値 */
/* [20250807] Add St */
	int						svStFreqSimTest = 0;		/* 模擬周波数 */
/* [20250807] Add Ed */
	int						workDigit;					/* 乗率(作業用) *//* [20251211] Add */

	/* 計測情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_TMSTSFIL );

	/* 集約計測情報数分処理を繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_PVSYS_AGGTMPOS ; loopCnt1++ )
	{
		/* 演算計測情報が未実装の場合 */
		if( ps_g_TMAGGINF[loopCnt1].statFlag == 0 )
			continue;

		/* 計測値と計測フラグを初期化 */
		avltmFlag = 0;
		avlnormData = 0;
		avltmLVal = 0L;

		/* リンク情報を設定 */
		tmIfLink = ps_g_TMAGGINF[loopCnt1].tmIfLink;

#ifdef PV_DEBUG_TMAGG
printf("%s : TMAGG-CALC DATA:%02d  CALCTM:%04d , LINK:%04d\n", pv_own_name, loopCnt1+1, ps_g_TMAGGINF[loopCnt1].tmNumb, tmIfLink );
#endif

		/* 合計演算の演算対象計測数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < ps_g_TMAGGINF[loopCnt1].aggTmNum ; loopCnt2++ )
		{
			/* リンク情報を設定 */
			aggtmIfLink = ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].tmIfLink;

#ifdef PV_DEBUG_TMAGG
printf("%s : TMAGG-CALC   SUMTM  LINK:%04d  AVL[%02X,%10ld]\n", pv_own_name, aggtmIfLink,
ps_g_TMSTSFIL[aggtmIfLink].avltmFlag,
ps_g_TMSTSFIL[aggtmIfLink].avltmVal);
#endif

			/* 積算値は過去の正常値を使用する為、平均値の情報を使用する */

			/* 計測ステータスが正常の場合 */
			if( ps_g_TMSTSFIL[aggtmIfLink].avltmFlag == PV_TMFLG_NORMAL )
			{
				/* 正常データをカウントする */
				avlnormData++;
			}

			/* 計測値が正常、および、計測種別が積算型の場合は、集約演算を行う */
			if( ( ps_g_TMSTSFIL[aggtmIfLink].avltmFlag == PV_TMFLG_NORMAL ) ||
				( ps_g_TMDATINF[aggtmIfLink].tmKind == PV_TM_KIND_COUNT ) )
			{
				/* 計測値の取得 */
				workLVal = (long long)ps_g_TMSTSFIL[aggtmIfLink].avltmVal;
				/* 変換値の反映 */
				workLVal *= ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiConv;
				/* ベース値の反映 */
				workLVal += ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiBase;
				/* 単位調整値の反映 */
				workLVal *= ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiUnit;
				/* 乗率調整値の反映 */
				workLVal *= ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiExp;

				/* 演算種別が＋の場合 */
				if( ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].plusminus == PV_DEF_PLUS )
				{
					/* 演算結果に加算 */
					avltmLVal += workLVal;
				}
				else
				{
					/* 演算結果から減算 */
					avltmLVal -= workLVal;
				}

			}
#ifdef PV_DEBUG_TMAGG
printf("%s : SUM-CALC   SUMTM  SUMINFO:%04d, CO:%lld, BA:%lld, UN:%lld, EX%lld, VAL:%lld, SUM:%lld\n", 
pv_own_name, 
loopCnt2+1,
ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiConv,
ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiBase,
ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiUnit,
ps_g_TMAGGINF[loopCnt1].s_tmaggdata[loopCnt2].choseiExp,
workLVal, 
avltmLVal);
#endif
		}

		/* 全ての集約対象の計測値が正常ではない場合は、欠測とする */
		if( avlnormData == ps_g_TMAGGINF[loopCnt1].aggTmNum )
		{
			avltmFlag = PV_TMFLG_NORMAL;
		}
		else
		{
			avltmFlag = PV_TMFLG_DROPTM;
		}


#ifdef PV_DEBUG_TMAGG
printf("%s : SUM-KEKKA         LINK:%04d  AVL[%02X,%10lld]\n",
	pv_own_name, tmIfLink, avltmFlag, avltmLVal);
#endif

		/* 合計演算結果に対して乗率を調整する */
		avltmLVal /= ps_g_TMAGGINF[loopCnt1].choseiExp;
		/* 合計演算結果に対して単位を反映する */
		avltmLVal /= ps_g_TMAGGINF[loopCnt1].choseiUnit;
		/* 合計演算結果に対してベース値を反映する */
		avltmLVal -= ps_g_TMAGGINF[loopCnt1].choseiBase;
		/* 合計演算結果に対して変換値を反映する */
		avltmLVal /= ps_g_TMAGGINF[loopCnt1].choseiConv;


		ps_sysData->tmValData[ps_g_TMAGGINF[loopCnt1].tmNumb-1] = (int)avltmLVal;
		ps_sysData->tmFlgData[ps_g_TMAGGINF[loopCnt1].tmNumb-1] = avltmFlag;
	}

	/* 計測情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_TMSTSFIL );

	return( 0 );
}

int	func_svagg_update( struct st_sysData  *ps_sysData )
{
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						retCode = 0;				/* 関数戻り値 */
	int						checkSec;					/* チェック時間 */
	struct st_svaggdata		*ps_svaggdata;				/* 集約SV情報ポインタ */
	unsigned char			stBit;						/* 状変ビット位置 */
	unsigned char			stByte;						/* 装置状態（バイト） */
	int						svStat;						/* SV状態 */
	struct st_svaggstat		*ps_svaggstat;				/* 集約SV保存ポインタ */
	struct timeval 			nowTime;					/* 現在時刻（積算秒） */
	struct timeval 			wrkTime;					/* 判定用 */
	int						matchCnt;					/* 一致件数 */



	/* 集約計測情報数分処理を繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < PV_MAX_PVSYS_AGGSVPOS ; loopCnt1++ )
	{
		matchCnt = 0;
		
		/* 演算計測情報が未実装の場合 */
		if( ps_g_SVAGGINF[loopCnt1].statFlag == 0 )
			continue;

		/* 集約状態の対象状態数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < ps_g_SVAGGINF[loopCnt1].aggSvNum ; loopCnt2++ )
		{
			ps_svaggdata = &ps_g_SVAGGINF[loopCnt1].s_svaggdata[loopCnt2];
			ps_svaggstat  = &ps_g_SVAGGFIL[loopCnt1].s_svaggstat[loopCnt2];

			/* 装置状態情報のビット位置を算出 */
			stBit = ps_svaggdata->svstMask;

			/* SV状態情報 */
			if( ps_svaggdata->svstKind == PV_DEF_SV )
			{
				/* 装置状態の取得と表示 */
				stByte = (ps_g_SVSTSFIL + ps_svaggdata->stPoint )->svStat;

			}
			/* ST状態情報 */
			else //if( ps_svaggdata->svstKind == PV_DEF_ST )
			{
				/* 通信ステータス状態の取得と表示 */
				stByte = (ps_g_STSTSFIL + ps_svaggdata->stPoint )->stStat;
			}

			/* 装置状態取得 */
			if( ( stByte & stBit ) != 0 )
				svStat = 1;
			else
				svStat = 0;

			/* 現在状態と異なる場合、または、 */
			/* 確定時刻が未設定の場合(起動時) */
			if( ( ps_svaggstat->svStat != svStat ) ||
				( ps_svaggstat->fixTim.tv_sec == 0 ) )
			{
				/* 検出時間が未設定(0)の場合 */
				if( ps_svaggstat->detectionTim.tv_sec == 0 )
				{
					/* 積算秒を取得 */
					gettimeofday( &ps_svaggstat->detectionTim, NULL );
				}

				/* 判定時間を取得 */
				if( svStat == 1 )
					/* 現在状態がONの場合は、ON時間を使用 */
					checkSec = ps_svaggdata->onCheckSec;
				else
					/* 現在状態がOFFの場合は、OFF時間を使用 */
					checkSec = ps_svaggdata->offCheckSec;

				/* 現在時刻を取得 */
				gettimeofday(&nowTime, NULL);
				wrkTime = ps_svaggstat->detectionTim;
				wrkTime.tv_sec += checkSec;

				/* (現在時刻>=(検出時刻+判定時間)の場合 */
				if( (nowTime.tv_sec > wrkTime.tv_sec ) ||
				    ((nowTime.tv_sec == wrkTime.tv_sec ) &&
				     (nowTime.tv_usec >= wrkTime.tv_usec ) ))
				{
					/* 現在状態を反映 */
					ps_svaggstat->svStat = svStat;
					/* 確定時刻を更新 */
					ps_svaggstat->fixTim = nowTime;
					/* 検出時刻を初期化 */
					memset(&ps_svaggstat->detectionTim, 0, sizeof(ps_svaggstat->detectionTim));
				}
			}
			else
			{
				/* 現在状態と一致している場合は、検出時刻を初期化 */
				memset(&ps_svaggstat->detectionTim, 0, sizeof(ps_svaggstat->detectionTim));
			}

			/* 現在状態が集約SV値と一致しているかチェック */
			if( ps_svaggstat->svStat == ps_svaggdata->svVal )
			{
				/* インクリメント */
				matchCnt++;
			}
		}

		/* 集約論理演算がANDの場合、且つ、集約SVが全て一致している。または */
		/* 集約論理演算がORの場合、且つ、集約SVが1つでも一致している。 */
		
		if( (( ps_g_SVAGGINF[loopCnt1].logicalOpe == PV_DEF_AND ) &&
			 ( ps_g_SVAGGINF[loopCnt1].aggSvNum == matchCnt )) ||
			(( ps_g_SVAGGINF[loopCnt1].logicalOpe == PV_DEF_OR ) &&
			 ( matchCnt > 0 )) )
		{
			ps_g_SVAGGFIL[loopCnt1].svStat = 1;
		}
		else
		{
			ps_g_SVAGGFIL[loopCnt1].svStat = 0;
		}

		commfunc_johen_pos( PV_MKIND_PVSYS, 1, ps_g_SVAGGINF[loopCnt1].svNumb, ps_g_SVAGGFIL[loopCnt1].svStat, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	return( 0 );
}

int func_avlData_init( struct st_avlData *ps_avlData, int stockNum )
{

	int						loopCnt1;				/* ループカウンタ */

	/* 移動平均情報を初期化 */
	memset( ps_avlData, 0, sizeof( struct st_avlData ));

	/* 最大ストック数を設定 */
	ps_avlData->stocMax = stockNum;

	/* ストック位置を初期化 */
	ps_avlData->stocPtr = 0;

	/* 移動平均計測フラグを欠測に設定初期化 */
	ps_avlData->tmAvlFlg = PV_TMFLG_DROPTM;

	/* 移動平均ストック領域分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < stockNum ; loopCnt1++ )
	{
		/* 移動平均ストックの計測フラグに欠測を設定 */
		ps_avlData->tmStocFlg[loopCnt1] = PV_TMFLG_DROPTM;
	}

	return 0;
}

int func_avlData_Set( struct st_avlData *ps_avlData, int tmFlg, int tmVal )
{
	long long		avlWork;						/* 平均値演算用ワーク */
	int				workFlgData;					/* 最新計測値（フラグ） */
	int				workValData;					/* 最新計測値（計測値） */
	int				flgWork;						/* 平均値演算用ワーク */
	int				stocCnt;						/* 移動平均有効ストック数 */
	int				loopCnt1;						/* ループカウンタ */

	/* ストック位置に計測情報を格納 */
	ps_avlData->tmStocFlg[ps_avlData->stocPtr] = tmFlg;
	ps_avlData->tmStocVal[ps_avlData->stocPtr] = tmVal;

	/* 移動平均値演算用変数の初期化 */
	stocCnt = 0;
	avlWork = 0;
	flgWork = 0;

	/* 移動平均ストック最大数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < ps_avlData->stocMax ; loopCnt1++ )
	{
		workFlgData = ps_avlData->tmStocFlg[loopCnt1];
		workValData = ps_avlData->tmStocVal[loopCnt1];

		/* 計測情報が正常の場合 */ 	
		if( workFlgData == PV_TMFLG_NORMAL )
		{
			/* 移動平均値の計算対象データとして加算 */
			avlWork += workValData;
			/* 移動平均対象データ数加算 */
			stocCnt += 1;
		}
	}

	/* 有効なストックが存在した場合 */
	if( stocCnt > 0 )
	{
		/* 移動平均を演算する（四捨五入）するため演算元を１０倍する */
		avlWork *= 10;
		/* 有効ストック数で除算 */
		avlWork /= stocCnt;
		if( avlWork >= 0 )
		{
			/* 四捨五入処理 */
			if( (avlWork % 10 ) > 4 )
			{
				avlWork += 10;
			}
		}
		else
		{
			/* 四捨五入処理 */
			if( (avlWork % 10 )  < -4 )
			{
				avlWork -= 10;
			}
		}
		/* 移動平均結果 */
		avlWork /= 10;
		/* 有効ストック数がストック最大数以外の場合 */
		if( stocCnt != ps_avlData->stocMax )
		{
			/* 計測フラグに正常（一部異常）を設定 */
			flgWork = PV_TMFLG_ERRNOR;
		}
		else
		{
			/* 計測フラグに正常を設定 */
			flgWork = PV_TMFLG_NORMAL;
		}
	}
	else
	{
		/* 計測フラグに欠測を設定 */
		flgWork = PV_TMFLG_DROPTM;
	}

	/* 移動平均処理結果を保管する */
	ps_avlData->tmAvlFlg = flgWork;
	ps_avlData->tmAvlVal = avlWork;

	/* 次回処理時のストックポインタを設定 */
	ps_avlData->stocPtr++;
	if(ps_avlData->stocPtr >= ps_avlData->stocMax)
	{
		ps_avlData->stocPtr = 0;
	}

	return( 0 );

}

int	func_mode_S4( int reCalcFlg )
{
	struct tm			timeData;				/* 時刻情報 */
	int					rcvTargetPow=0;
	int					rcvPowRangeL;				/* 目標値範囲下限 */
	int					rcvPowRangeH;				/* 目標値範囲上限 */
	int					bttReqPow = 0;				/* 充放電電力値 */
	int					pvReqPow = 0;				/* PV発電量 */
	int					preTmFlg;					/* 前回判定結果 */
	int					addPvPow;					/* パワコン発電可能電力 */
	int					rcvPowVal;					/* 受電電力 */
	int					loadPowVal;					/* 負荷電力 */
	int					tmFlag;						/* 計測ステータス */
	int					pvAddPow=0;					/* PV発電量加算値 */
	int					loopCnt;					/* ループカウンタ */
	int					pvAddIdx;					/* PV発電加算テーブル該当位置 */
	int					preDisStopFlg;				/* 放電停止フラグ */
	int					preChgStopFlg;				/* 充電停止フラグ */
	int					preBcpUse;					/* 連系中BCP使用有無フラグ */
	int					svPvStat = 0;				/* PV発電停止状態 *//* [20210118] Add */
	int					rcvPowPos;					/* 受電電力計画時刻コマ *//* [20210128] Add */
	int					svPlnStat;					/* 需給計画有無 */
	int					dischgRestartSoc;			/* 放電可能SoC */
	int					socLowLimit;				/* SoC下限値 */
	int					svBcpStat;					/* SV状態 */
/* [20241204] Del */
	int					peakCutFlg;					/* デマンド制御フラグ */
/* [20250212] Add St */
	int					svCmExamStat;				/* 容量市場(実効性テスト)動作中 */
	int					svCmStat;					/* 容量市場(実需給)動作中 */

	/* 容量市場(実効性テスト)動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET_EXAM, &svCmExamStat, ps_g_SVSTSFIL);

	/* 容量市場(実需給)動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET, &svCmStat, ps_g_SVSTSFIL);
/* [20250212] Add Ed */

	/* 前回判定結果を保持 */
	preTmFlg = ps_g_EMCTLFIL->preTmFlg;
	preDisStopFlg = ps_g_EMCTLFIL->rcvpDisStopFlg;
	preChgStopFlg = ps_g_EMCTLFIL->rcvpChgStopFlg;
	/* 設定変更による以下の変更には対応 */
	/* ・rcvPowMaxDemand：ピークカット閾値 ‥‥受電電力目標値が変わるので対応 */
	/* ・rcvPowMinDemand：最低受電電力     ‥‥受電電力目標値が変わるので対応 */
	/* ・BCPSoc：BCP用SOC                  ‥‥対応できていない */
	/* ・dischgRestartSoC：放電再開SOC     ‥‥対応できていない */
	/* ・chgRestartSoC：充電再開SOC        ‥‥対応できていない */

	/* 周期時刻を取得 */
	pvctl_cmn_get_process_tim(NULL, &timeData);

	/* 需給計画の有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_PLAN, &svPlnStat, ps_g_SVSTSFIL);

	/* BCP使用有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BCP_USE, &svBcpStat, ps_g_SVSTSFIL);

/* [20230217] Add St */

/* [20241204] Del */

	/* デマンド制御の有効/無効を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_DEMAND_CTL, &peakCutFlg, ps_g_SVSTSFIL);
/* [20230217] Add Ed */

	/* 受電電力一定制御で必要な情報がそろっているかチェック */
	/*    蓄電池Soc */
	/*    受電電力 */
	/*    蓄電池有効電力 */
	if( ( s_sysDataAv.tmFlgData[PV_SYS_TM_BATTE_SOC-1] != PV_TMFLG_NORMAL ) ||
		( s_sysDataAv.tmFlgData[PV_SYS_TM_RCV_POW-1] != PV_TMFLG_NORMAL ) ||
		( s_sysDataAv.tmFlgData[PV_SYS_TM_BATTE_POW-1] != PV_TMFLG_NORMAL ))
	{
		/* 計測情報が欠測の場合は、前回と同じ制御値を使用する */
		tmFlag = PV_TMFLG_DROPTM;
	}
	else
	{
		tmFlag = PV_TMFLG_NORMAL;
	}
	/* 判定結果を保持 */
	ps_g_EMCTLFIL->preTmFlg = tmFlag;

	/* 連系中以外から遷移した場合は、指示情報を初期化する  */
	if( ( ps_g_EMCTLFIL->cemsMode != PV_CTL_ONGRID ) &&		/* S4:連系運転中 */
		( ps_g_EMCTLFIL->cemsMode != PV_CTL_ERR_ONGRID ))	/* S7:連系運転中(障害) */
	{
		ps_g_EMCTLFIL->bttReqPow = 0;
		ps_g_EMCTLFIL->pvReqPow = 0;
		ps_g_EMCTLFIL->preTmFlg = PV_TMFLG_DROPTM;
		ps_g_EMCTLFIL->rcvpDisStopFlg = PVCTL_FLG_OFF;
		ps_g_EMCTLFIL->rcvpChgStopFlg = PVCTL_FLG_OFF;
	}

/* [20250205] Del */

	/* PV発電状態を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_STATE, &svPvStat, ps_g_SVSTSFIL);


#ifdef DEBUG_MODE_S4
	sprintf( errMsg, "func_mode_S4 : reCalc=%d, svPlnStat=%d, peakCut=%d, svBcpStat=%d, rcvPow(Flg,Val)=(%d,%d), SoC=%d", ps_g_EMCTLFIL->reCalcFlg, svPlnStat, peakCutFlg, svBcpStat, s_rcvPowAve.tmAvlFlg, s_rcvPowAve.tmAvlVal, s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC-1] );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif//DEBUG_MODE_S4

	/* 計画がある場合 */
	if( svPlnStat == PVCTL_FLG_ON )
	{
		/* 受電電力目標値を取得 */
		rcvTargetPow = s_sysDataAv.tmValData[PV_SYS_TM_PLAN_RCV_POW -1];

/* [20230302] Add St */
/* [20241204] Chg St */
		/* デマンド制御が有効、且つ、且つ、受電電力目標値がピークカット閾値を超える場合 */
		if( ( peakCutFlg == PVCTL_FLG_ON ) && 
		    ( rcvTargetPow > ps_g_RCVPWINF->rcvPowMaxDemand ))
/* [20241204] Chg Ed */
		{
			/* 受電電力目標値を補正する */
			rcvTargetPow = ps_g_RCVPWINF->rcvPowMaxDemand;
		}
/* [20230302] Add Ed */

		/* 起動時以外は、変化量が設定されている場合は、変化量に従い更新する */
		if( rcvTargetPow > s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW_TARGET-1] )
		{
			/* 変化量を加算しても目標値に届かない場合は、目標値を補正する */
			if( rcvTargetPow > ( s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW_TARGET-1] + ps_g_RCVPWINF->rcvPowChangeAmt ) )
			{
				rcvTargetPow = s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW_TARGET-1] + ps_g_RCVPWINF->rcvPowChangeAmt;
			}
		}
		else if( rcvTargetPow < s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW_TARGET-1] )
		{
			/* 変化量を減算しても目標値に届かない場合は、目標値を補正する */
			if( rcvTargetPow < ( s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW_TARGET-1] - ps_g_RCVPWINF->rcvPowChangeAmt ) )
			{
				rcvTargetPow = s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW_TARGET-1] - ps_g_RCVPWINF->rcvPowChangeAmt;
			}
		}

/* [20230302] Add St */
		/* 受電電力目標値が最低受電電力より低い場合 */
		if( rcvTargetPow < ps_g_RCVPWINF->rcvPowMinDemand )
		{
			/* 受電電力目標値を補正する */
			rcvTargetPow = ps_g_RCVPWINF->rcvPowMinDemand;
		}
/* [20230302] Add Ed */

		/* 受電電力目標値を更新 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_RCV_POW_TARGET, s_sysDataAv.tmFlgData[PV_SYS_TM_PLAN_RCV_POW -1], rcvTargetPow );		/* [20230831] Chg */
	}

/* [20241204] Del */
	/* 計測情報(SoC,bttPow,rcvPow)が欠測の場合 */
	if( tmFlag  == PV_TMFLG_DROPTM )
	{
		/* 前回と同じ制御値を使用する */
		bttReqPow = ps_g_EMCTLFIL->bttReqPow;
		pvReqPow = ps_g_EMCTLFIL->pvReqPow;

#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4 : DROPTM bttReqPow=%d, pvReqPow=%d", bttReqPow, pvReqPow );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
	}
	else
	{
		/* BCP+解除オフセット>=SoC上限 などのパラメータ補正は実施しない */

		/* 充電停止チェック */
		if( ( preChgStopFlg == PVCTL_FLG_OFF ) &&
			( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] >= ps_g_BATTEINF->socUpper ) )
		{
			ps_g_EMCTLFIL->rcvpChgStopFlg = PVCTL_FLG_ON;
			
		}
		/* 充電停止解除チェック */
		else if( ( preChgStopFlg == PVCTL_FLG_ON ) &&
				 ( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] < ( ps_g_BATTEINF->socUpper - ps_g_RCVPWINF->chgRestartSoC )) )	/* [20230224] Chg */
		{
			ps_g_EMCTLFIL->rcvpChgStopFlg = PVCTL_FLG_OFF;
		}

		/* BCP使用有無 */
		if( svBcpStat == 1  )
		{
			/* 使用可能な場合は、SoC下限を使用 */
			socLowLimit = ps_g_BATTEINF->socLower;
		}
		else
		{
			/* 使用不可能な場合は、BCPを使用 */
			socLowLimit = ps_g_RCVPWINF->BCPSoc;
/* [20230216] Add St */
			/* SoC下限値を下回る場合は補正する */
			if( socLowLimit < ps_g_BATTEINF->socLower )
			{
				socLowLimit = ps_g_BATTEINF->socLower;
			}
/* [20230216] Add Ed */
		}

		/* 放電停止チェック */
		if( ( preDisStopFlg == PVCTL_FLG_OFF ) &&
			 ( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] < socLowLimit ) )	/* [20230224] Chg */
		{
			ps_g_EMCTLFIL->rcvpDisStopFlg = PVCTL_FLG_ON;
		}
		/* 放電停止解除チェック */
		else if( ( preDisStopFlg == PVCTL_FLG_ON ) &&
				 ( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] >= ( socLowLimit + ps_g_RCVPWINF->dischgRestartSoC )) )
		{
			ps_g_EMCTLFIL->rcvpDisStopFlg = PVCTL_FLG_OFF;
		}

/* [20230222] Chg St */
/* [20230217] Chg St */
/* [20241204] Chg St */
		/* 目標下限値 */
		rcvPowRangeL = ( rcvTargetPow - ps_g_RCVPWINF->deadZone);
		/* 目標上限値 */
		rcvPowRangeH = ( rcvTargetPow + ps_g_RCVPWINF->deadZone);
/* [20230302] Add St */
/* [20241204] Chg St */

/* [20250212] Add St */
		/* 容量市場が動作中の場合 */
		if( ( svCmStat == PVCTL_FLG_ON ) ||
			( svCmExamStat == PVCTL_FLG_ON ) )
		{
			/* 容量市場は制御目標値以上が成功判定なので、不感帯の上限(充電方向)は適用しない */
			rcvPowRangeH = rcvTargetPow;
		}
/* [20250212] Add Ed */

		/* ピークカットが有効の場合 */
		if( peakCutFlg == PVCTL_FLG_ON )
/* [20241204] Chg Ed */
		{
			/* 不感帯の上限がピークカット閾値を超える場合は補正する */
			if( rcvPowRangeH > ps_g_RCVPWINF->rcvPowMaxDemand )
			{
				rcvPowRangeH = ps_g_RCVPWINF->rcvPowMaxDemand;
			}
		}
/* [20230302] Add Ed */
/* [20241204] Chg Ed */

		/* 最低受電電力を下回る場合は、補正する */
		if( rcvPowRangeL < ps_g_RCVPWINF->rcvPowMinDemand )
		{
			rcvPowRangeL = ps_g_RCVPWINF->rcvPowMinDemand;
		}
/* [20230302] Add St */
		/* 不感帯を補正した可能性があるので、目標値を再度補正する(基本的には変わらない) */
		rcvTargetPow = (rcvPowRangeL + rcvPowRangeH ) / 2;
/* [20230302] Add Ed */
		
/* [20230302] Add Ed */

/* [20230217] Chg Ed */
/* [20230222] Chg Ed */

#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4 : rcvTargetPow=%d, socLow=%d, ChgStopFlg=%d, DisStopFlg=%d, rcvPowRangeL=%d, rcvPowRangeH=%d", rcvTargetPow, socLowLimit, ps_g_EMCTLFIL->rcvpChgStopFlg, ps_g_EMCTLFIL->rcvpDisStopFlg, rcvPowRangeL, rcvPowRangeH );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4

/* [20251203] Chg St */
		/* 前回の制御が正常の場合、且つ、 */
		/* BCP使用有無、受電電力目標値の切り替わりではない場合、且つ、 */
		/* 出力スケジュール更新がない場合 */
		/* 受電電力が目標値の不感帯以内の場合 */
		if( ( preTmFlg == PV_TMFLG_NORMAL ) &&
/* [20241204] Chg St */
			( s_sysDataAv.tmValData[PV_SYS_TM_PLAN_RCV_POW -1] == rcvTargetPow ) &&	/* [20230220] Chg */
/* [20241204] Chg Ed */
			( ps_g_EMCTLFIL->reCalcFlg == PVCTL_FLG_OFF ) && 						/* [20230220] Add */
			( ps_g_EMCTLFIL->preSchUpdataFlg == PVCTL_FLG_OFF ) &&					/* [20231009] Add */
			( ( s_rcvPowAve.tmAvlVal >= rcvPowRangeL ) &&
			  ( s_rcvPowAve.tmAvlVal <= rcvPowRangeH ) ))
		{
			/* 同じ制御値を使用する */
			bttReqPow = ps_g_EMCTLFIL->bttReqPow;
			pvReqPow = ps_g_EMCTLFIL->pvReqPow;

#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4 : rcvPowAve=%d:%d, rcvPow=%d, btPow=%d, SoC=%d, bttReqPow=%d, pvReqPow=%d, rcvPowRangeL=%d, rcvPowRangeH=%d, CONTINUE", 
			 		s_rcvPowAve.tmAvlFlg, s_rcvPowAve.tmAvlVal, s_sysDataAv.tmValData[PV_SYS_TM_RCV_POW-1], 
			 		s_sysDataAv.tmValData[PV_SYS_TM_BATTE_POW-1], s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1], bttReqPow, pvReqPow, rcvPowRangeL, rcvPowRangeH );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
/* [20251203] Chg Ed */
		}
		else
		{
/* [20230220] Add St */
			/* 再計算フラグをOFFへ */
			ps_g_EMCTLFIL->reCalcFlg = PVCTL_FLG_OFF;
/* [20230220] Add Ed */

			/* BT指令値計算
			/*  ①ΔkW = 受電電力(MM1)の平均値 - 受電電力目標値 */
			/*  ②BT指令値 = ① + BTの現在出力値(BTから取得)  */
			bttReqPow = s_rcvPowAve.tmAvlVal - rcvTargetPow + s_sysDataAv.tmValData[PV_SYS_TM_BATTE_POW-1];

/* [20251203] Del */

/* [20251202] Chg St */
/* [20251203] Del */
		}

		/**********************/
		/* 充放電電力補正 */
		/**********************/

		/* 放電の場合(発電が低下(夜間など)、需要家電力が発電より高くなった場合) */
		if( bttReqPow > 0 )
		{
/* [20251203] Chg St */
			/* SoC下限チェックのチェックを削除
			 * IPoCCの充放電計画ではSoCの下限を加味した計画の為、SoCの上限のチェックは行わず、
			 * 残電力量が需給計画(残電力量)の到達したかだけをチェックする
			*/

			/* 残電力量が需給計画(残電力量)に到達 */
			if( s_sysDataAv.tmValData[PV_SYS_TM_BT_REMAIN_WH-1] <= s_sysDataAv.tmValData[PV_SYS_TM_PLAN_REMAIN -1] )
			{
				/* 放電不可 */
				bttReqPow = 0;
			}
/* [20251203] Chg Ed */
/* [20240910] Add Ed */
			else
			{
				/* 放電可能電力チェック */
				if( ps_g_EMCTLFIL->avlDischgPow < bttReqPow )
				{
					/* 放電電力補正 */
					bttReqPow = ps_g_EMCTLFIL->avlDischgPow;
				}
			}
		}
		/* 充電の場合 */
		else
		{
/* [20251203] Chg St */
/* [20230224] Chg St */
			/* 充電禁止の場合 */
			/* ※残電力量が需給計画(残電力量)の到達は放電側しかないので、充電側はSoCをチェックする */
			if( ps_g_EMCTLFIL->rcvpChgStopFlg == PVCTL_FLG_ON )
/* [20230224] Chg Ed */
			{

				/* 充電停止 */
				bttReqPow = 0;
			}
/* [20251203] Chg Ed */
			else
			{
				/* 充電可能電力チェック */
				if( ps_g_EMCTLFIL->avlChgPow < (-1*bttReqPow))
				{
					/* 現在の充電電力を充電発電可能電力にする */
					bttReqPow = (-1*ps_g_EMCTLFIL->avlChgPow);
				}
			}
		}
#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4 : bttPow=%d, SoC=%d, bttReqPow=%d, pvReqPow=%d", 
		 		s_sysDataAv.tmValData[PV_SYS_TM_BATTE_POW-1], s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1], bttReqPow, pvReqPow );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
/* [20251202] Chg Ed */
	}

	/* BT制御目標値を更新 */
	ps_g_EMCTLFIL->bttReqPow = bttReqPow;

	/* PV制御目標値を更新 */
	if( pvReqPow < 0 )
	{
		pvReqPow = 0;
	}
	ps_g_EMCTLFIL->pvReqPow = pvReqPow;

	return 0;
}

int	func_mode_S4_StlSoC( int reCalcFlg )
{
	int					targetRemain=0;			/* 目標残電力量 */
	int					remainRangeH;			/* 目標値範囲上限 */
	int					bttReqPow = 0;			/* 充放電電力値 */
	int					pvReqPow = 0;			/* PV発電量 *//* 未使用 */
	int					tmFlag;					/* 計測ステータス */
	int					svPlnStat;				/* 需給計画有無 */
	int					remainWh=0;				/* 蓄電池残電力量 */
	int					peakCutFlg=0;			/* ピークカット(デマンド制御) */
	int					loadPowAsmption;		/* 想定負荷電力 */
	int					svStBcpUse;				/* BCP使用可否 */
	int					svStSpryDmndSocRecChrgFlg;	/* 需給調整市場SoC回復充電 *//* [20250217] Add */
	int					svStCapMktSocRecChrgFlg;	/* 容量市場SoC回復充電 *//* [20250217] Add */


	/* SoC制御ではSoCを目標値に向かって充放電する為、無効にする */
	ps_g_EMCTLFIL->rcvpChgStopFlg = PVCTL_FLG_OFF;
	ps_g_EMCTLFIL->rcvpDisStopFlg = PVCTL_FLG_OFF;

	/* 受電電力計画値を0に設定 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PLAN_RCV_POW, PV_TMFLG_DROPTM, 0 );
	/* 受電電力目標値を0に設定 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_RCV_POW_TARGET, PV_TMFLG_DROPTM, 0 );

	/* 需給計画の有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_PLAN, &svPlnStat, ps_g_SVSTSFIL);

	/* デマンド制御の有効/無効(ピークカット)を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_DEMAND_CTL, &peakCutFlg, ps_g_SVSTSFIL);

	/* BCP使用有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BCP_USE, &svStBcpUse, ps_g_SVSTSFIL);

/* [20250217] Add St */
	/* 需給調整市場SoC回復充電を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_SUPPLY_DEMAND_SOC, &svStSpryDmndSocRecChrgFlg, ps_g_SVSTSFIL);

	/* 容量市場SoC回復充電を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET_SOC, &svStCapMktSocRecChrgFlg, ps_g_SVSTSFIL);
/* [20250217] Add Ed */

/* [20250205] Del */

	/* SoC制御で必要な情報がそろっているかチェック */
	/*    残電力量 */
	if( ( s_sysDataAv.tmFlgData[PV_SYS_TM_BT_REMAIN_WH-1] != PV_TMFLG_NORMAL ) ||
		( s_rcvPowAve.tmAvlFlg == PV_TMFLG_DROPTM ) ||
		( s_sysDataAv.tmFlgData[PV_SYS_TM_BATTE_POW-1] != PV_TMFLG_NORMAL ))
	{
		/* 計測情報が欠測の場合は、充放電を停止する */
		tmFlag = PV_TMFLG_DROPTM;
	}
	else
	{
		remainWh = s_sysDataAv.tmValData[PV_SYS_TM_BT_REMAIN_WH-1];
		tmFlag = PV_TMFLG_NORMAL;
	}


#ifdef DEBUG_MODE_S4
	sprintf( errMsg, "func_mode_S4_StlSoC : reCalc=%d, svPlnStat=%d, rcvPow(Flg,Val)=(%d,%d), Remain(Flg,Val)=(%d,%d)", ps_g_EMCTLFIL->reCalcFlg, svPlnStat, s_rcvPowAve.tmAvlFlg, s_rcvPowAve.tmAvlVal, s_sysDataAv.tmFlgData[PV_SYS_TM_BT_REMAIN_WH-1] , s_sysDataAv.tmValData[PV_SYS_TM_BT_REMAIN_WH-1] );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif//DEBUG_MODE_S4

	/* 計測情報が欠測の場合 */
	if( tmFlag  == PV_TMFLG_DROPTM )
	{
		/* 充放電を停止する */
		bttReqPow = 0;

#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4_StlSoC : func_mode_S4_StlSoC bttReqPow=%d", bttReqPow );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
	}
	/* 計画がある場合 */
	else if( svPlnStat == PVCTL_FLG_ON )
	{
		/* 想定負荷電力 = 受電電力 ＋ 蓄電池有効電力 */
		loadPowAsmption = s_rcvPowAve.tmAvlVal + s_sysDataAv.tmValData[PV_SYS_TM_BATTE_POW-1];

		/* 残電力量目標値を取得 */
		targetRemain = s_sysDataAv.tmValData[PV_SYS_TM_PLAN_REMAIN -1];

		/* 目標上限値 */
		remainRangeH = ( targetRemain + ps_g_RCVPWINF->deadZoneSoC);

#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4_StlSoC : remainRangeH=%d, targetRemain=%d, remainWh=%d, loadPowAsmption=%d", remainRangeH, targetRemain, remainWh, loadPowAsmption );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4

/* [20250217] Add St */
		/* 需給調整市場、容量市場の回復コマの場合、且つ、残電力量が必要電力量の場合は充放電しない */
		/* ※バックエンドの充放電計画で残電力量が必要電力以上だった場合は回復コマにしない為、本現象は基本的に発生しない */
		if( (( svStSpryDmndSocRecChrgFlg == PVCTL_FLG_ON) ||
			 ( svStCapMktSocRecChrgFlg == PVCTL_FLG_ON )) && 
			( remainWh >= targetRemain ))
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : remainWh=%d >= targetRemain=%d, SpryDmndSocRecChrg=%d, CapMktSocRecChrg=%d", remainWh, targetRemain, svStSpryDmndSocRecChrgFlg, svStCapMktSocRecChrgFlg );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			/* 充放電を停止する */
			bttReqPow = 0;
		}
/* [20250217] Add Ed */
		/*（目標残電力量 ＋ 不感帯）＞ 残電力量 ＞＝ 目標残電力量 */
		else if( ( remainRangeH > remainWh) &&
			( remainWh >= targetRemain ))
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : remainRangeH=%d > remainWh=%d >= targetRemain=%d", remainRangeH, remainWh, targetRemain );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			/* 充放電を停止する */
			bttReqPow = 0;
		}
		else
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : not remainRangeH=%d > remainWh=%d >= targetRemain=%d", remainRangeH, remainWh, targetRemain );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			/* 残電力量が目標残電力量より高い場合 */
			if( remainWh > targetRemain )
			{
				/* 放電 */
#ifdef DEBUG_MODE_S4
				sprintf( errMsg, "func_mode_S4_StlSoC : DIS loadPowAsmption=%d rcvPowMinDemand=%d", loadPowAsmption, ps_g_RCVPWINF->rcvPowMinDemand );
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4

				/* 想定負荷電力が最低受電電力より大きい場合 */
				if( loadPowAsmption > ps_g_RCVPWINF->rcvPowMinDemand )
				{
					/* 充放電指令値 ＝ 想定負荷電力 － 最低受電電力 */
					bttReqPow = loadPowAsmption - ps_g_RCVPWINF->rcvPowMinDemand;
				}
			}
			else
			{
				/* 充電 */
#ifdef DEBUG_MODE_S4
				sprintf( errMsg, "func_mode_S4_StlSoC : CHG bttReqPow=%d avlChgPow=%d", bttReqPow, ps_g_EMCTLFIL->avlChgPow );
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
				/* 充放電指令値 ＝ 充電可能電力 */
				bttReqPow = -1*ps_g_EMCTLFIL->avlChgPow;
			}
		}
#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4_StlSoC : bttReqPow=%d, peakCutFlg=%d, rcvPowMaxDemand=%d", bttReqPow, peakCutFlg, ps_g_RCVPWINF->rcvPowMaxDemand );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4

		/* ピークカットが有効な場合 */
		if( peakCutFlg == PVCTL_FLG_ON )
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : PEEKCUT peakCutFlg=%d", peakCutFlg );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			if( ( loadPowAsmption - bttReqPow ) > ps_g_RCVPWINF->rcvPowMaxDemand )
			{
#ifdef DEBUG_MODE_S4
				sprintf( errMsg, "func_mode_S4_StlSoC : PEEKCUT loadPowAsmption=%d - rcvPowMaxDemand=%d", loadPowAsmption, ps_g_RCVPWINF->rcvPowMaxDemand);
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
				/* 充放電指令値 ＝ (想定負荷電力 － 充放電指令値) － デマンド閾値 (例：想定負荷電力2000 - (充放電指令値-500) - デマンド閾値2400 )*/
				bttReqPow += ( loadPowAsmption - bttReqPow ) - ps_g_RCVPWINF->rcvPowMaxDemand;
			}
		}
	}

	/* 充放電指示値が充電 */
	if( bttReqPow < 0 )
	{
#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4_StlSoC : CHG bttReqPow=%d, avlChgPow=%d, SoC=%d, SoCUpper=%d", bttReqPow, ps_g_EMCTLFIL->avlChgPow, s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1], ps_g_BATTEINF->socUpper);
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
		/* 指令値 ＞ 充電可能電力 */
		if( (-1*bttReqPow) > ps_g_EMCTLFIL->avlChgPow )
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : CHG bttReqPow=%d > avlChgPow=%d", bttReqPow, ps_g_EMCTLFIL->avlChgPow );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			/* 充放電指令値 ＝ 充電可能電力 */
			bttReqPow = (-1 * ps_g_EMCTLFIL->avlChgPow);
		}

/* [20251203] Del */
		/* SoC上限のチェックを削除 
		 * IPoCCの充放電計画のSoC(定格容量から)と現地のSoC(フル充電エネルギーから)の算出が一致しない為、
		 * 計画残電力量を通りに充電できないケースがある為、SoC上限チェックの処理を削除する。
		 * これによる影響は、需要家蓄電池にて受電電力が最低受電電力を下回った場合に充電動作となり、
		 * 計画残電力量を超えて充電する可能性があるが、一時的な動作であること、蓄電池の充電は↑の充電可能電力で抑制されることから
		 * 判定処理を削除しても問題ない。
		*/
	}

	/* 充放電指示値が放電 */
	if( bttReqPow > 0 )
	{
#ifdef DEBUG_MODE_S4
		sprintf( errMsg, "func_mode_S4_StlSoC : DIS bttReqPow=%d, avlDischgPow=%d, svStBcpUse=%d", bttReqPow, ps_g_EMCTLFIL->avlDischgPow, svStBcpUse );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
		/* 指令値 ＞ 放電可能電力 */
		if( bttReqPow > ps_g_EMCTLFIL->avlDischgPow )
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : DIS bttReqPow=%d > avlDischgPow=%d", bttReqPow, ps_g_EMCTLFIL->avlDischgPow );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			/* 充放電指令値 ＝ 放電可能電力 */
			bttReqPow = ps_g_EMCTLFIL->avlDischgPow;
		}

		/* BCP超過 ＝ OFF */
		if( svStBcpUse == PVCTL_FLG_OFF )
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : DIS svStBcpUse=%d, BCPSoc=%d, SoC=%d", svStBcpUse, ps_g_RCVPWINF->BCPSoc, s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
			/* BCP使用可否：OFF */
			/* BCP使用不可のため、放電して下回らせないチェック */
			if( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] >= ps_g_RCVPWINF->BCPSoc )
			{
#ifdef DEBUG_MODE_S4
				sprintf( errMsg, "func_mode_S4_StlSoC : DIS PV_SYS_TM_BATTE_SOC=%d >= BCPSoc=%d", s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1], ps_g_RCVPWINF->BCPSoc );
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
				;/* 放電可能 */
			}
			else
			{
#ifdef DEBUG_MODE_S4
				sprintf( errMsg, "func_mode_S4_StlSoC : DIS PV_SYS_TM_BATTE_SOC=%d < BCPSoc=%d", s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1], ps_g_RCVPWINF->BCPSoc );
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
				/* 充放電指令値 ＝ 0 */
				bttReqPow = 0;
			}
		}
		else
		{
#ifdef DEBUG_MODE_S4
			sprintf( errMsg, "func_mode_S4_StlSoC : DIS svStBcpUse=%d, socLower=%d, SoC=%d", svStBcpUse, ps_g_BATTEINF->socLower, s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4
/* [20251203] Del */
			/* SoC下限のチェックを削除
			 * IPoCCの充放電計画のSoC(定格容量から)と現地のSoC(フル充電エネルギーから)の算出が一致しない為、
			 * 計画残電力量を通りに放電できないケースがある為、SoC下限チェックの処理を削除する。
			 * これによる影響は需要家蓄電池のピークカット動作でSoC下限値を下回っても放電することになるが、
			 * ピークカットの方が優先度は高い為、問題ない。また、蓄電池の放電の制限は↑の放電可能電力で抑制されることから判定処理を削除。
			*/
		}
	}

	/* BT制御目標値を更新 */
	ps_g_EMCTLFIL->bttReqPow = bttReqPow;

	/* PV制御目標値を更新 */
	if( pvReqPow < 0 )
	{
		pvReqPow = 0;
	}
	ps_g_EMCTLFIL->pvReqPow = pvReqPow;

	return 0;
}
/* [20250807] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力制御(連系)
 *
 * 機能：
 * 		一次調整力中の制御を行う
 */
int	func_mode_S4_fcr( void )
{
	int					svStFcrFreqErr				=	0;	/* 一次調整力異常動作中 */
	long long			fcrDltKw					=	0;	/* 一次調整力約定△kW */
	long long			baseFreq;							/* 基準周波数 */
	long long			variationRate;						/* 調定率 */
	long long			deadBand;							/* 不感帯 */
	long long			deadBandLower;						/* 不感帯下限 */
	long long			deadBandUpper;						/* 不感帯上限 */
	long long			deadBandEnd;						/* 不感帯端 */
	long long			freqDevtThrsldLower;				/* 異常判定周波数(下限) */
	long long			freqDevtThrsldUpper;				/* 異常判定周波数(上限) */
	long long			ratedPower;							/* 定格出力 */
	long long			deadBandResType;					/* 不感帯応動種別 */
	long long			freqTm;								/* 周波数 */
	long long			maxPower;							/* 最大出力値 */
	int					bttReqPow					=	0;	/* 充放電電力値 */
	int					pvReqPow					=	0;	/* PV発電量 *//* 未使用 */
	int					svChgResEna;						/* SV充電応動有無 *//* [20251215] 充電応動なしの制御対応 Add */

	/* 一次調整力異常動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL_FREQ_ERR, &svStFcrFreqErr, ps_g_SVSTSFIL);
/* [20251215] 充電応動無効の制御対応 Add St */
	/* 一次調整力異常動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CHG_RES_ENABLED, &svChgResEna, ps_g_SVSTSFIL);
/* [20251215] 充電応動無効の制御対応 Add Ed */

	/* 約定△kW */
	fcrDltKw = ps_g_TMSTSFIL[PV_SYS_TM_FCRCTL_CONTRACT_DLT_POW-1].avltmVal;

	bttReqPow = ps_g_EMCTLFIL->bttReqPow;

	/* 基準周波数 */
	baseFreq = ps_g_FCRSTINF->baseFreq;
	/* 調定率 */
	variationRate = ps_g_FCRSTINF->variationRate;
	/* 不感帯 */
	deadBand = ps_g_FCRSTINF->deadBand;
	/* 不感帯(下限)を設定 */
	deadBandLower = ps_g_FCRSTINF->deadBandLower;
	/* 不感帯(上限)を設定 */
	deadBandUpper = ps_g_FCRSTINF->deadBandUpper;
	/* 異常判定周波数 */
	freqDevtThrsldLower = ps_g_FCRSTINF->freqDevtThrsldLower;
	freqDevtThrsldUpper = ps_g_FCRSTINF->freqDevtThrsldUpper;
	/* 定格出力 */
	ratedPower = ps_g_FCRSTINF->ratedPower;
	/* 放電時出力変化量 */
	/* 充電時出力変化量 */
	/* 不感帯応動種別 */
	deadBandResType = ps_g_FCRSTINF->deadBandResType;
	/* 周波数 */
	freqTm = s_freqAve.tmAvlVal;	/* [20260130] Chg *//* [20260514] Chg */

/* [20251215] 充電応動無効の制御対応 Chg St */
/* [20260514] Chg St */
	/* 周波数が欠測の場合 */
	if( s_freqAve.tmAvlFlg == PV_TMFLG_DROPTM )
	{
		/*前回の制御目標値を使用する */
		bttReqPow = ps_g_EMCTLFIL->bttReqPow;
	
	}
	/* 充電応動有無が無効、かつ、周波数が基準周波数より高い(充電方向の制御)場合 */
	else if( svChgResEna == PV_FCRST_CHG_RESPONSIVE_DISABLED && freqTm > ps_g_TMSTSFIL[PV_SYS_TM_FCRCTL_BASE_FREQ - 1].avltmVal)
/* [20260514] Chg Ed */
	{
		/* 応動しない(出力値を0kWにする) */
		bttReqPow = 0;
	}
	else
	{
		/* 応動する(出力値を演算する) */

		/* 周波数異常時動作中以外 */
		if( svStFcrFreqErr == PVCTL_FLG_OFF )
		{
			if( ps_g_EMCTLFIL->fcrCalcDataType == PV_FCR_CALC_DATA_TYPE_DLTPOW )
			{
				/* 最大出力として約定△kWを出力算出に使用 */
				maxPower = fcrDltKw;
			}
			else
			{
				/* 最大出力として定格出力を出力算出に使用 */
				maxPower = ratedPower;
			}
			/* 周波数から出力を算出する */
			/* 現周波数から出力値計算 */
			if( ( deadBandResType != PV_FCRST_DEADBAND_TYPE_NONE) && 
				( ( freqTm >= deadBandLower ) && ( freqTm <= deadBandUpper ) ) )
			{
				/* 不感帯なし以外で周波数が不感帯域内の場合、出力を0に設定 */
				bttReqPow = 0;
			}
			else
			{
				/* 不感帯ありパターン2以外 */
				if( deadBandResType != PV_FCRST_DEADBAND_TYPE_TWO ) 
				{
					/* 不感帯なし、不感帯ありパターン1で、周波数が不感帯域外 */
					/* 出力計算式 ((基準周波数-計測周波数)×定格出力)÷( 基準周波数 × 調定率 ) */
					bttReqPow = (int)( ( ( baseFreq - freqTm ) * 1000 * 100 * maxPower ) / ( baseFreq * variationRate ) );
#ifdef DEBUG_FCR_CTL_CALC
					printf("%s func_mode_S4_fcr type0,1 bttReqPow[%d] = ( ( baseFreq[%d] - freqTm[%d] ) * maxPower[%d] ) / ( baseFreq[%d] * variationRate[%d] ) ## fcrCalcDataType[%d] \n",
						pv_own_name, bttReqPow, baseFreq, freqTm, maxPower, baseFreq, variationRate, ps_g_EMCTLFIL->fcrCalcDataType );
#endif /* DEBUG_FCR_CTL_CALC */
				}
				else
				{
					/* 不感帯ありパターン2 */
					if( freqTm < baseFreq )
					{
						/* 周波数が下振れ */
						deadBandEnd = deadBandLower;
					}
					else
					{
						/* 周波数が上振れ */
						deadBandEnd = deadBandUpper;
					}
					/* 出力計算式 ((不感帯端-計測周波数)×定格出力)÷( 基準周波数 × 調定率 ) */
					bttReqPow = (int)( ( ( deadBandEnd - freqTm ) * 1000 * 100 * maxPower ) / ( baseFreq * variationRate ) );
#ifdef DEBUG_FCR_CTL_CALC
					printf("%s func_mode_S4_fcr type2 bttReqPow[%d] = ( ( deadBandEnd[%d] - freqTm[%d] ) * maxPower[%d] ) / ( baseFreq[%d] * variationRate[%d] ) ## fcrCalcDataType[%d] \n",
						pv_own_name, bttReqPow, deadBandEnd, freqTm, maxPower, baseFreq, variationRate, ps_g_EMCTLFIL->fcrCalcDataType );
#endif /* DEBUG_FCR_CTL_CALC */
				}
/* [20251212] 出力計算後の約定△kW上限 Add St */
				/* 周波数異常時出力 約定△kWを出力 */
				if( ( baseFreq - freqTm ) > 0 && ( bttReqPow > maxPower ) )
				{
					bttReqPow = maxPower;
				}
				else if( ( baseFreq - freqTm ) < 0 && ( bttReqPow < ( maxPower * -1 ) ) )
				{
					bttReqPow = maxPower * -1;
				}
/* [20251212] 出力計算後の約定△kW上限 Add Ed */
			}
		}
		else
		{
/* [20260513] Chg St */
			/* 周波数異常時出力 約定△kWを出力 */
			bttReqPow = fcrDltKw;
/* [20260513] Chg Ed */
		}
	}
/* [20251212] 充電応動無効の制御対応 Chg Ed */
/* [20260202] 出力抑制の対応 Add St */
	/* 放電の場合 */
	if( bttReqPow > 0 )
	{
#ifdef DEBUG_SCH_DISCHG
		printf("func_mode_S4_fcr ps_g_EMCTLFIL->avlDischgPow=[%d] bttReqPow[%d]\n", ps_g_EMCTLFIL->avlDischgPow, bttReqPow);
#endif /* DEBUG_SCH_DISCHG */
		/* 放電可能電力チェック */
		if( ps_g_EMCTLFIL->avlDischgPow < bttReqPow )
		{
			/* 放電電力補正 */
			bttReqPow = ps_g_EMCTLFIL->avlDischgPow;
		}
	}
/* [20260513] Add St */
	/* 充電の場合 */
	else if( bttReqPow < 0 )
	{
#ifdef DEBUG_SCH_DISCHG
		printf("func_mode_S4_fcr ps_g_EMCTLFIL->avlChgPow=[%d] bttReqPow[%d]\n", ps_g_EMCTLFIL->avlChgPow, bttReqPow);
#endif /* DEBUG_SCH_DISCHG */
		/* 放電可能電力チェック */
		if( (-1*bttReqPow) > ps_g_EMCTLFIL->avlChgPow )
		{
			/* 充放電指令値 ＝ 充電可能電力 */
			bttReqPow = (-1 * ps_g_EMCTLFIL->avlChgPow);
		}
	}
/* [20260513] Add Ed */
/* [20260202] 出力抑制の対応 Add Ed */
	/* BT制御目標値を更新 */
	ps_g_EMCTLFIL->bttReqPow = bttReqPow;

	/* PV制御目標値を更新 */
	if( pvReqPow < 0 )
	{
		pvReqPow = 0;
	}
	ps_g_EMCTLFIL->pvReqPow = pvReqPow;

	return 0;
}
/* [20250807] Add Ed */


/* 連系中（障害発生）時の制御 */
/* PVと発電を停止する方向で動作している */
/* ※モード切替時の処理で、BG・PCS発電停止のSVを立てている為 */
/* 発電電力分を充電する */
int	func_mode_S7( void )
{
	int					bttReqPow = 0;				/* 充放電電力値 */

	/* PV発電電力が実装されている場合 */
	if( ps_g_TMDATINF[ PV_SYS_TM_PV_POW -1 ].statFlag == 1 )
	{
		/* PV発電電力が正常の場合 */
		if( s_sysDataAv.tmFlgData[PV_SYS_TM_PV_POW-1] == PV_TMFLG_NORMAL )
		{
			/* 発電電力分、充電する */
			bttReqPow = -1 * s_sysDataAv.tmValData[PV_SYS_TM_PV_POW-1];
		}
		else
		{
			/* 前回と同じ制御値を使用する */
			bttReqPow = ps_g_EMCTLFIL->pvReqPow;
		}
		/* 蓄電池SoC上限値以上の場合 */
		if( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] >= ps_g_BATTEINF->socUpper )
		{
			/* 充電不可 */
			bttReqPow = 0;
		}
		/* 充電可能電力チェック */
		else if( ps_g_EMCTLFIL->avlChgPow < (-1*bttReqPow))
		{
			/* 充電電力補正 */
			bttReqPow = (-1*ps_g_EMCTLFIL->avlChgPow);
		}
	}

	ps_g_EMCTLFIL->bttReqPow = bttReqPow;

#ifdef DEBUG_MODE_S4
	sprintf( errMsg, "func_mode_S7  : pv(%d,%d), pvReqPow=%d, SoC=%d, bttReqPow=%d", 
	 		s_sysDataAv.tmFlgData[PV_SYS_TM_PV_POW-1],s_sysDataAv.tmValData[PV_SYS_TM_PV_POW-1],
	 		ps_g_EMCTLFIL->pvReqPow,
	 		s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1],
	 		bttReqPow );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif //DEBUG_MODE_S4

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 過充電回避制御
 *
 * 機能：
 * 		自立運転中に蓄電システムが過充電とならないようにPV出力を抑制する。
 */
int	func_mode_S3( void )
{
	int					consumFlg;				/* 需要電力フラグ */
	int					consumVal;				/* 需要電力計測値 */
	int					pvReqPow = 0;			/* PV発電量 */
	int	 				svStat;

	/* 計測ステータスをチェック */
	if( ( s_sysDataAv.tmFlgData[PV_SYS_TM_BATTE_SOC-1] != PV_TMFLG_NORMAL ) ||
		( s_sysDataAv.tmFlgData[PV_SYS_TM_BATTE_POW-1] != PV_TMFLG_NORMAL ) ||
		( s_sysDataAv.tmFlgData[PV_SYS_TM_PV_POW-1] != PV_TMFLG_NORMAL ) )
	{
		consumFlg = PV_TMFLG_DROPTM;
	}
	else
	{
		consumFlg = PV_TMFLG_NORMAL;
	}

	/* 自立中以外から遷移した場合は、指示情報を初期化する  */
	if( ( ps_g_EMCTLFIL->cemsMode != PV_CTL_OFFGRID ) &&		/* S3:自立運転中 */
		( ps_g_EMCTLFIL->cemsMode != PV_CTL_ERR_OFFGRID ))	/* S6:自立運転中(障害) */
	{
		ps_g_EMCTLFIL->pvReqPow = 0;
		ps_g_EMCTLFIL->ovrChgPvStop = PVCTL_FLG_OFF;
	}

	/* 過充電回避制御が無効の場合 */
	if( ps_g_CEMSPINF->ovrChgAvoid == 0 )
	{
		/* PV発電停止 */
		pvReqPow = 0;
	}
	/* 需要電力が正常の場合 */
	else if( consumFlg == PV_TMFLG_NORMAL )
	{
		/* 蓄電池SoC上限値以上の場合 */
		if( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] >= ps_g_BATTEINF->socUpper )
		{
			/* PV発電停止 */
			ps_g_EMCTLFIL->ovrChgPvStop = PVCTL_FLG_ON;
		}
		else
		{
			/* PV発電停止フラグがONの場合、且つ、PV発電再開SoC以下の場合 */
			if( ( ps_g_EMCTLFIL->ovrChgPvStop == PVCTL_FLG_ON ) &&
				( s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1] <= ps_g_OVRCHINF->pvRestartSoc ) )
			{
				/* PV発電停止フラグをOFF */
				ps_g_EMCTLFIL->ovrChgPvStop = PVCTL_FLG_OFF;
			}

#ifdef DEBUG_OVR_POW
			sprintf( errMsg, "func_mode_overchg  : SoC=%d, avlChgPow=%d, pvReqPow=%d", 
			 		s_sysDataAv.tmValData[PV_SYS_TM_BATTE_SOC -1], ps_g_EMCTLFIL->avlChgPow, pvReqPow );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
		}

		/* PV発電停止フラグがONの場合 */
		if( ps_g_EMCTLFIL->ovrChgPvStop == PVCTL_FLG_ON )
		{
			/* PV発電を停止 */
			pvReqPow = 0;
		}
		else
		{
			/* PV発電電力 =  充電可能電力 *//* [20200822] Chg */
			pvReqPow = ps_g_EMCTLFIL->avlChgPow;
		}

	}
	else
	{
		/* 前回と同じ制御値を使用する */
		pvReqPow = ps_g_EMCTLFIL->pvReqPow;
	}

	/* PV制御目標値を更新 */
	ps_g_EMCTLFIL->pvReqPow = pvReqPow;

	return 0;
}

int func_bt_ctl_check( int nowMode )
{
	int			retCode = 0;
	int			svStat;

	/* BT充放電制御の有効/無効をチェック  */
	if( ps_g_CEMSPINF->btControl == 0 )
	{
		retCode = -1;
	}
	/* S4:連系運転中 */
	else if( nowMode == PV_CTL_ONGRID )
	{
		/* 連系運転時BT制御条件を取得 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_BT_CHECK, &svStat, ps_g_SVSTSFIL);

		if( svStat != 1 )
		{
			retCode = -1;
		}
	}

	return retCode;
}

int func_pv_ctl_check( int nowMode )
{
	int			retCode = 0;
	int			svStat;

	/* PV抑制制御の有効/無効をチェック  */
	if( ps_g_CEMSPINF->pvControl == 0 )
	{
		retCode = -1;
	}
	/* S3:自立運転中 */
	else if( nowMode == PV_CTL_OFFGRID )
	{
		/* 自立運転時PV制御条件を取得 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OFFGRID_PV_CHECK, &svStat, ps_g_SVSTSFIL);

		if( svStat != 1 )
		{
			retCode = -1;
		}
	}
	/* S4:連系運転中 */
	else if( nowMode == PV_CTL_ONGRID )
	{
		/* 連系運転時PV制御条件を取得 */
		commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_PV_CHECK, &svStat, ps_g_SVSTSFIL);

		if( svStat != 1 )
		{
			retCode = -1;
		}
	}

	return retCode;
}

/* [20240701] Chg St */
/* [20250807] Chg St */
int func_check_powerPlan( int nowMode, struct timeval *ps_fcrErrStTime, int *p_preDmdPos )	/* [20260202] Chg */
/* [20250807] Chg Ed */
{
	int					svStPeakCutFlg				=	0;	/* ピークカット(デマンド制御) */
	int					svStPlnFlg					=	0;	/* 需給計画有無 */
	int					svStBcpFlg					=	0;	/* BCP使用 */
	int					svStRcvPowFlg				=	0;	/* 受電電力一定制御 */
	int					svStSpryDmndUseFlg			=	0;	/* 需給調整市場使用 */
	int					svStSpryDmndSocRecChrgFlg	=	0;	/* 需給調整市場SoC回復充電 */
	int					svStCapMktUseFlg			=	0;	/* 容量市場使用 */
	int					svStCapMktSocRecChrgFlg		=	0;	/* 容量市場SoC回復充電 */
	int					svStCtlSoc					=	0;	/* SoC制御 */
	int					svStSocRec					=	0;	/* SoC回復制御 */
	int					svStCategoryRR				=	0;	/* 需給調整市場動作中 */
	int					svStCategoryCm				=	0;	/* 容量市場(実需給)動作中 */
	int					svStCategoryCmExam			=	0;	/* 容量市場(実効性テスト)動作中 */
	int					planRcv						=	0;	/* 受電電力計画値 */
	int					bttSoC						=	0;	/* TM需給計画（残電力量） */
/* [20250807] Add St */
	int					svStFcrCtlSocAjt			=	0;	/* 一次調整力SoC調整 */	/* [20251204] 一次調整力SoC調整 Add */
	int					svStFcrCtl					=	0;	/* 一次調整力制御 */
	int					svStFcrFreqErr				=	0;	/* 一次調整力異常動作中 */
	int					svStFcrRepoAcum				=	0;	/* レポートデータ蓄積 */
	int					svStFreqSimTest				=	0;	/* 模擬周波数 */
/* [20250807] Add Ed */

	int					preSvStPeakCutFlg;					/* 前周期 ピークカット(デマンド制御) */
	int					preSvStPlnFlg;						/* 前周期 需給計画有無 */
	int					preSvStBcpFlg;						/* 前周期 BCP使用有無 */
	int					preSvStRcvPowFlg;					/* 前周期 受電電力一定制御 */
	int					preSvStCategoryRR;					/* 前周期 需給調整市場動作中 */
	int					preSvStCategoryCm;					/* 前周期 容量市場(実需給)動作中 */
	int					preSvStCategoryCmExam;				/* 前周期 容量市場(実効性テスト)動作中 */
/* [20250807] Add St */
	int					preSvStFcrCtlSocAjt;				/* 前周期一次調整力SoC調整 */	/* [20251204] 一次調整力SoC調整 Add */
	int					preSvStFcrCtl;						/* 前周期 一次調整力制御 */
	int					preSvStFcrCtlFreqErr;				/* 前周期 一次調整力異常動作中 */
	int					preSvStRepoDataAcum;				/* 前周期 レポートデータ蓄積 */
	int					preSvStFreqSimTest;					/* 前周期 模擬周波数 */
/* [20250807] Add Ed */
	struct tm			timeData;							/* 時刻情報 */
	int					nowDay;								/* 現在日 */
	int					dmdPos;								/* 制御コマ */
	int					loopCnt1;							/* 需給計画ループカウンタ */
	struct st_PWPLNINF *ps_pwplninf;						/* [18]需給計画値情報ファイル */
	int					acSoC; 								/* 調整力必要電力量 */
	int					avlChgPow=0;						/* 充電電力上限 */
	int					avlDischgPow=0;						/* 放電電力上限 */
	struct st_schdata	schData;							/* スケジュール情報 */
	int					schChgTime;							/* 出力変化時間 */

	int					planPow				=	0;			/* 充放電計画 *//* フローに無い */
	struct st_evtdata 	*ps_evtdata;						/* イベント情報 */
/* [20250807] Add St */
	struct timeval		nowTime;							/* 時刻情報 */
	struct timeval		nextStTime;							/* 次コマ開始時刻情報 */
	int					fcrDltKw					=	0;	/* 現コマ一次調整力約定△kW */
	int					fcrDltKwNext				=	0;	/* 次コマ一次調整力約定△kW */
	int					ajustMSec					=	0;	/* 秒数補正値 */
	int					fcrFreqSimJudge				=	0;	/* 模擬周波数判定 */
	int					targetFreq 					=	0;	/* 制御判定周波数 */
	int					errDuration					=	0;	/* 異常継続時間 */
	int					fcrUse						=	0;	/* 一次使用有無 *//* [20250909] Add */
/* [20250807] Add Ed */

	/* 需給計画の有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_PLAN, &preSvStPlnFlg, ps_g_SVSTSFIL);

	/* BCP使用有無を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_BCP_USE, &preSvStBcpFlg, ps_g_SVSTSFIL);

	/* 受電電力一定制御の有効/無効を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_RCVPWR_CTL, &preSvStRcvPowFlg, ps_g_SVSTSFIL);

	/* デマンド制御の有効/無効を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_DEMAND_CTL, &preSvStPeakCutFlg, ps_g_SVSTSFIL);

	/* 需給調整市場動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_SUPPLY_DEMAND, &preSvStCategoryRR, ps_g_SVSTSFIL);

	/* 容量市場(実効性テスト)動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET_EXAM, &preSvStCategoryCmExam, ps_g_SVSTSFIL);

	/* 容量市場(実需給)動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET, &preSvStCategoryCm, ps_g_SVSTSFIL);

/* [20250807] Add St */
	/* 一次調整力SoC調整を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL_SOC_AJT, &preSvStFcrCtlSocAjt, ps_g_SVSTSFIL);

	/* 一次調整力制御を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL, &preSvStFcrCtl, ps_g_SVSTSFIL);

	/* 一次調整力異常動作中を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL_FREQ_ERR, &preSvStFcrCtlFreqErr, ps_g_SVSTSFIL);

	/* レポートデータ蓄積を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_REPORT_DATA_ACUM, &preSvStRepoDataAcum, ps_g_SVSTSFIL);

	/* 模擬周波数を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FREQ_SIM_TEST, &preSvStFreqSimTest, ps_g_SVSTSFIL);
/* [20250807] Add Ed */

/* [20260514] Add St */
	/* 模擬周波数動作中の場合 */
	if( preSvStFreqSimTest == PVCTL_FLG_ON )
	{
		/* 模擬周波数を設定 */
		s_freqAve.tmAvlFlg = PV_TMFLG_NORMAL;
		s_freqAve.tmAvlVal = ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[ps_g_EMCTLFIL->s_fcr_freq_sim_data.targetNumb - 1].freqValue;
	}
	else
	{
		/* 周波数を設定 */
		func_avlData_Set( &s_freqAve, s_sysDataAv.tmFlgData[PV_SYS_TM_GRID_FREQ-1], s_sysDataAv.tmValData[PV_SYS_TM_GRID_FREQ-1] );
//commfunc_syslog_msgset( pv_own_name, errMsg );
	}
/* [20260514] Add Ed */

	/* 最大充電電力更新 */
	if( ps_g_BATTEINF != NULL )
	{
		avlChgPow = ps_g_BATTEINF->maxChargePwr;
		ps_g_EMCTLFIL->avlChgPow = ps_g_BATTEINF->maxChargePwr;
		/* 蓄電池 放電可能最大値が実装済み、且つ、計測フラグが正常の場合、且つ、現在の最大値より小さい場合 */
		if( ( ps_g_TMDATINF[ PV_SYS_TM_BT_AVL_CHG -1 ].statFlag == 1 ) &&
			( s_sysDataAv.tmFlgData[PV_SYS_TM_BT_AVL_CHG - 1] == PV_TMFLG_NORMAL ) &&
			( s_sysDataAv.tmValData[PV_SYS_TM_BT_AVL_CHG - 1] < ps_g_EMCTLFIL->avlChgPow ))	/* [20230830] Add */

		{
			/* 最大充電電力更新 */
			ps_g_EMCTLFIL->avlChgPow = s_sysDataAv.tmValData[PV_SYS_TM_BT_AVL_CHG - 1];
		}

		/* 最大放電電力更新 */
		avlDischgPow = ps_g_BATTEINF->maxDisChgPwr;
		ps_g_EMCTLFIL->avlDischgPow = ps_g_BATTEINF->maxDisChgPwr;
		/* 蓄電池 放電可能最大値が実装済み、且つ、計測フラグが正常の場合、且つ、現在の最大値より小さい場合 */
		if( ( ps_g_TMDATINF[ PV_SYS_TM_BT_AVL_DIS -1 ].statFlag == 1 ) &&
			( s_sysDataAv.tmFlgData[PV_SYS_TM_BT_AVL_DIS - 1] == PV_TMFLG_NORMAL ) &&
			( s_sysDataAv.tmValData[PV_SYS_TM_BT_AVL_DIS - 1] < ps_g_EMCTLFIL->avlDischgPow ))	/* [20230830] Add */
		{
			/* 最大放電電力更新 */
			ps_g_EMCTLFIL->avlDischgPow = s_sysDataAv.tmValData[PV_SYS_TM_BT_AVL_DIS - 1];
		}
	}

/* [20250807] Add St */
	/* 周期時刻を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);
/* [20250807] Add Ed */

	/* 現在日を取得 */
	nowDay = ( timeData.tm_year * 10000) + ( timeData.tm_mon * 100 ) + timeData.tm_mday;
	/* デマンドコマを取得 */
	dmdPos = ( timeData.tm_hour * 2 ) + (timeData.tm_min / 30 );

	ps_pwplninf = &ps_g_PWPLNFIL->powplndata[PV_POWPLAN_IDX_ONGRID];
	/* 格納日数分ループ */
	for( loopCnt1 = 0; loopCnt1 < ps_pwplninf->plnDayNum; loopCnt1 ++ )
	{
		/* 当日のデータの場合 */
		if( ps_pwplninf->s_powDayPln[loopCnt1].plnDay ==  nowDay )
		{
			/* 現在のコマの情報を取得 */
			/* 計画有無 */
			svStPlnFlg = ps_pwplninf->s_powDayPln[loopCnt1].plnStat[dmdPos];

			/* 受電電力目標値を取得 */
			planRcv = ps_pwplninf->s_powDayPln[loopCnt1].rcvPow[dmdPos];

			/* BCP使用有無を取得 */
			svStBcpFlg = ps_pwplninf->s_powDayPln[loopCnt1].useBcp[dmdPos];

			/* 充放電計画値を取得 */
			planPow = ps_pwplninf->s_powDayPln[loopCnt1].bttPow[dmdPos];

			/* 計画残電力量を取得 */
			bttSoC = ps_pwplninf->s_powDayPln[loopCnt1].bttSoC[dmdPos];

			/* 需給調整市場使用を取得 */
			svStSpryDmndUseFlg = ps_pwplninf->s_powDayPln[loopCnt1].acUse[dmdPos];

			/* 調整力必要電力量を取得 */
			acSoC = ps_pwplninf->s_powDayPln[loopCnt1].acSoC[dmdPos];

			/* 充電電力上限を取得 */
			avlChgPow = ps_pwplninf->s_powDayPln[loopCnt1].avlChgPow[dmdPos];

			/* 放電電力上限を取得 */
			avlDischgPow = ps_pwplninf->s_powDayPln[loopCnt1].avlDischgPow[dmdPos];

			/* ピークカット有無を取得 */
			svStPeakCutFlg = ps_pwplninf->s_powDayPln[loopCnt1].peakCutStat[dmdPos];

			/* 容量市場使用を取得 */
			svStCapMktUseFlg = ps_pwplninf->s_powDayPln[loopCnt1].capMktUse[dmdPos];
/* [20250807] Add St */
			/* 一次調整力約定△kWを取得(現コマ) */
			fcrDltKw = ps_pwplninf->s_powDayPln[loopCnt1].fcrDltKw[dmdPos];
/* [20260114] Del */
			/* 一次使用有無を取得 */
			fcrUse = ps_pwplninf->s_powDayPln[loopCnt1].fcrUse[dmdPos];/* [20250909] Add */
/* [20250807] Add Ed */
			break;
		}
	}

/* [20260202] Add St */
	/* 前処理コマと現コマで、コマ切替を判定 */
	if( *p_preDmdPos != dmdPos )
	{
		/* 異常動作開始時刻の初期化 */
		memset( ps_fcrErrStTime, 0, sizeof( struct timeval ) );

		/* 一次調整力異常動作中を更新(OFF) */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL_FREQ_ERR, svStFcrFreqErr, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		preSvStFcrCtlFreqErr = svStFcrFreqErr;
	}
	/* 前処理コマを更新 */
	*p_preDmdPos = dmdPos;
/* [20260202] Add Ed */

/* [20250807] Chg St */
	/* 模擬周波数制御の場合 */
	if( preSvStFreqSimTest == PVCTL_FLG_ON )
	{
		/* 一次調整力制御 */
		svStFcrCtl = PVCTL_FLG_ON;
		/* レポートデータ蓄積 */
		svStFcrRepoAcum = PVCTL_FLG_ON;
		/* 約定△kW */
		fcrDltKw = ps_g_EMCTLFIL->s_fcr_freq_sim_data.fcrDltKw;
		/* 模擬周波数の場合は、判定タイミングは意識しない */
		/* 周波数正常範囲チェック */
		if( func_fcrfreqcheck_in_normal( &targetFreq ) == true )
		{
			/* 異常動作開始時刻の初期化 */
			memset( ps_fcrErrStTime, 0, sizeof( struct timeval ) );
			/* 一次調整力異常動作中：復帰 */
			svStFcrFreqErr = PVCTL_FLG_OFF;
		}
		else
		{
			/* 異常動作開始時刻が初期化状態以外 */
			if( ps_fcrErrStTime->tv_sec != 0 )
			{
				/* 異常動作開始時刻のミリ秒以下差分で経過時間を補正 */
				if( ( nowTime.tv_usec - ps_fcrErrStTime->tv_usec ) < 0 )
				{
					/* 現在時刻のtv_usec < 異常動作開始時刻のtv_usecの場合は、秒数差分が経過時間ではないので補正1秒を秒数差分からマイナスする */
					ajustMSec = 1;
				}

				/* 異常継続時間 */
				errDuration = nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec;

				/* 異常時出力継続時間(秒)の経過をチェック */
				if( ps_g_FCRSTINF->faultDuration >= errDuration )	/* [20260515] Chg */
				{
					/* 周波数偏移異常範囲かつ異常時出力継続時間(秒)経過前の場合 */
					/* 一次調整力異常動作中：発生 */
					svStFcrFreqErr = PVCTL_FLG_ON;
#ifdef DEBUG_FCR_FREQ_SIM
					printf( "%s func_check_powerPlan ★1 svStFcrFreqErr[%d] [%ld] <= [%d] (%ld.%06ld - %ld.%06ld)\n",
						PV_OWN_NAME, svStFcrFreqErr, ps_g_FCRSTINF->faultDuration, ( nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec ), nowTime.tv_sec, nowTime.tv_usec, ps_fcrErrStTime->tv_sec, ps_fcrErrStTime->tv_usec );
#endif /* DEBUG_FCR_FREQ_SIM */
				}
				else
				{
					/* 周波数偏移異常範囲かつ異常時出力継続時間(秒)経過後の場合 */
					/* 一次調整力異常動作中：復帰 */
					svStFcrFreqErr = PVCTL_FLG_OFF;
#ifdef DEBUG_FCR_FREQ_SIM
					printf( "%s func_check_powerPlan ★2 svStFcrFreqErr[%d] [%ld] > [%d] (%ld.%06ld - %ld.%06ld)\n",
						PV_OWN_NAME, svStFcrFreqErr, ps_g_FCRSTINF->faultDuration, ( nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec ), nowTime.tv_sec, nowTime.tv_usec, ps_fcrErrStTime->tv_sec, ps_fcrErrStTime->tv_usec );
#endif /* DEBUG_FCR_FREQ_SIM */
				}
			}
			else
			{
				/* 一次調整力異常動作中：発生 */
				svStFcrFreqErr = PVCTL_FLG_ON;
				/* 異常動作開始時刻の設定 */
				memcpy( ps_fcrErrStTime, &nowTime, sizeof( struct timeval ) );
#ifdef DEBUG_FCR_FREQ_SIM
				printf( "%s func_check_powerPlan FreqErrStart svStFcrFreqErr[%d]\n", PV_OWN_NAME, svStFcrFreqErr );
#endif /* DEBUG_FCR_FREQ_SIM */
			}
		}
	}
/* [20260114] Chg St */
/* [20250807] Chg Ed */
	else
	{
/* [20250807] Add St */
		/* 一次調整力レポートデータ蓄積の判定 */
		svStFcrRepoAcum = func_fcr_report_acum_judge( nowTime, fcrDltKw);
#ifdef DEBUG_FCR_FREQ_CTL
//		printf( "%s func_check_powerPlan svStSpryDmndUseFlg[%d] svStCapMktUseFlg[%d] ps_g_EMCTLFIL->s_setdata_fcr.actMode[%d]\n",
//			PV_OWN_NAME, svStSpryDmndUseFlg, svStCapMktUseFlg, ps_g_EMCTLFIL->s_setdata_fcr.actMode );
#endif /* DEBUG_FCR_FREQ_CTL */
/* [20250807] Add Ed */

		/* 充放電計画がある場合 */
		if( svStPlnFlg == PVCTL_FLG_ON )
		{
			/* 需給調整市場使用＝ON または、容量市場使用＝ONの場合 */
			if( ( svStSpryDmndUseFlg == PVCTL_FLG_ON ) || ( svStCapMktUseFlg == PVCTL_FLG_ON ) )
			{
/* [20240918] Chg St */
				/* 計画充充放電電力が放電(0含む)場合 */
				if( planPow >= 0 )
				{
					/* TM需給計画（残電力量）に必要電力量を設定 */
					bttSoC = acSoC;
/* [20240918] Chg Ed */
					/* 需給調整市場、容量市場の機能優先度は需給計画に従う為、統合コントローラでは意識しない */
					/* 需給調整市場使用＝ON、且つ、三次調整力が運用中の場合 */
					if( ( svStSpryDmndUseFlg == PVCTL_FLG_ON ) &&
						( ps_g_EMCTLFIL->s_setdata_rr.actMode == AWS_MODE_OPE ) )
					{
						/* 需給調整市場動作中を設定 */
						svStCategoryRR = PVCTL_FLG_ON;

						/* 受電電力一定制御をON(1)に設定 */
						svStRcvPowFlg = PVCTL_FLG_ON;

						ps_evtdata = &ps_g_EMCTLFIL->s_evtdata_rr;
					}
					/* 容量市場の場合は、発動指令と優先度に従って判断する */
					/* 容量市場使用＝ON、且つ、容量市場(実需給or実効性テスト)が運用中の場合 */
					else if( ( svStCapMktUseFlg == PVCTL_FLG_ON ) &&
							 (( ps_g_EMCTLFIL->s_setdata_cm.actMode == AWS_MODE_OPE ) ||
							  ( ps_g_EMCTLFIL->s_setdata_cm_exam.actMode == AWS_MODE_OPE )) )
					{
						/* 受電電力一定制御をON(1)に設定 */
						svStRcvPowFlg = PVCTL_FLG_ON;

						/* 実需給と実効性テストが運用中の場合 */
						if(( ps_g_EMCTLFIL->s_setdata_cm.actMode == AWS_MODE_OPE ) &&		/* [20241205] Chg */
						   ( ps_g_EMCTLFIL->s_setdata_cm_exam.actMode == AWS_MODE_OPE ))
						{
							/* 実効性テストが優先の場合 */
							if( ps_g_EMCTLFIL->cmExamPri == PVCTL_FLG_ON )
							{
								/* 容量市場(実効性テスト)動作中を設定 */
								svStCategoryCmExam = PVCTL_FLG_ON;

								ps_evtdata = &ps_g_EMCTLFIL->s_evtdata_cm_exam;
							}
							else
							{
								/* 容量市場(実需給)動作中を設定 */
								svStCategoryCm = PVCTL_FLG_ON;

								ps_evtdata = &ps_g_EMCTLFIL->s_evtdata_cm;
							}
						}
						/* 実需給が運用中の場合 */
						else if( ps_g_EMCTLFIL->s_setdata_cm.actMode == AWS_MODE_OPE )
						{
							/* 容量市場(実需給)動作中を設定 */
							svStCategoryCm = PVCTL_FLG_ON;
							ps_evtdata = &ps_g_EMCTLFIL->s_evtdata_cm;
						}
						/* 実効性テストが運用中の場合 */
						else if( ps_g_EMCTLFIL->s_setdata_cm_exam.actMode == AWS_MODE_OPE )
						{
							/* 容量市場(実効性テスト)動作中を設定 */
							svStCategoryCmExam = PVCTL_FLG_ON;
							ps_evtdata = &ps_g_EMCTLFIL->s_evtdata_cm_exam;
						}
					}
					
					/* 受電電力一定制御をON(1)に設定 */
					if( svStRcvPowFlg == PVCTL_FLG_ON )
					{
						/* リソース種別により分ける */
						/* 蓄電池(発電)リソース */
						if( ps_g_IOTCNINF->resorceType == AWS_RESORCE_STORAGE_BATTERY )
						{
							/* 蓄電所 受電電力＝(計画発電電力(base)＋発電追加量(value))）* -1 */
							planRcv = -1*( ps_evtdata->targetBase + ps_evtdata->targetPow );
						}
						/* 需要リソース */
						else
						{
							/* 需要家 受電電力＝需要予測（base）－ 需要を下げる量(value) */
							planRcv = ( ps_evtdata->targetBase - ps_evtdata->targetPow );
						}

					}
					else
					{
						;/* 処理なし(制御しないための設定があれば行う) */
					}
				}
				else
				{
					/* 必要電力量が0ではない場合 */

/* [20240603] Chg St */
					/* 計画充充放電電力が0の場合 */
					if( planPow == 0 )
					{
						/* TM需給計画（残電力量）＝計画電力量を設定 */
						;
					}
					else
					{
						/* TM需給計画（残電力量）＝必要電力量に設定 */
						bttSoC = acSoC;
					}
/* [20240603] Chg Ed */

					/* 需給調整市場使用 */
					if( svStSpryDmndUseFlg == PVCTL_FLG_ON )
					{
						/* 需給調整市場SoC回復充電をON(1)に設定 */
						svStSpryDmndSocRecChrgFlg = PVCTL_FLG_ON;
					}
					if( svStCapMktUseFlg == PVCTL_FLG_ON )	/* [20250131] Chg */
					{
						/* 容量市場SoC回復充電をON(1)に設定 */
						svStCapMktSocRecChrgFlg = PVCTL_FLG_ON;
					}

					/* 制御＝SoC制御をON(1)に設定 */
					svStCtlSoc = PVCTL_FLG_ON;
				}
			}
/* [20250807] Add St */
			/* 一次使用有無がON、かつ約定△kWが0以外の場合 */
			else if( ( fcrUse == PVCTL_FLG_ON ) && ( fcrDltKw != 0 ) )/* [20250909] Chg */
			{
#ifdef DEBUG_FCR_FREQ_CTL
//			printf( "%s func_check_powerPlan FCRCTL IN ", PV_OWN_NAME );
#endif /* DEBUG_FCR_FREQ_CTL */
				ps_evtdata = &ps_g_EMCTLFIL->s_evtdata_rr;

				/* 一次調整力制御 */
				svStFcrCtl = PVCTL_FLG_ON;

/* [20260515] Add St */
				/* 周波数判定は000msのタイミングでのみ実施する */
				if( ( nowTime.tv_usec / 100000 )== 0 )
				{
/* [20260515] Add Ed */
					/* 周波数正常範囲チェック */
					if( func_fcrfreqcheck_in_normal( &targetFreq ) == true )
					{
/* [20260512] Chg St */
						/* 異常動作中の場合 */
						if(  ps_fcrErrStTime->tv_sec != 0 )
						{
							/* 異常継続時間(判定が000msのタイミングなので、msの差分は見ない *//* [20260515] Chg */
							errDuration = nowTime.tv_sec - ps_fcrErrStTime->tv_sec;

							/* 異常時出力継続時間(秒)の経過をチェック */
							if( ps_g_FCRSTINF->faultDuration >= errDuration )	/* [20260515] Chg */
							{
								/* 周波数偏移異常範囲かつ異常時出力継続時間(秒)経過前の場合 */
								/* 一次調整力異常動作中：発生 */
								svStFcrFreqErr = PVCTL_FLG_ON;
#ifdef DEBUG_FCR_FREQ_SIM
								sprintf( errMsg, "func_check_powerPlan fcr root-1 svStFcrFreqErr[%d] [%ld] <= [%d] (%ld.%06ld - %ld.%06ld)",
									svStFcrFreqErr, ps_g_FCRSTINF->faultDuration, ( nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec ), nowTime.tv_sec, nowTime.tv_usec, ps_fcrErrStTime->tv_sec, ps_fcrErrStTime->tv_usec );
								commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* DEBUG_FCR_FREQ_SIM */
							}
							else
							{
								/* 異常動作開始時刻の初期化 */
								memset( ps_fcrErrStTime, 0, sizeof( struct timeval ) );
								/* 一次調整力異常動作中：復帰 */
								svStFcrFreqErr = PVCTL_FLG_OFF;
#ifdef DEBUG_FCR_FREQ_SIM
								sprintf( errMsg, "func_check_powerPlan fcr root-2 svStFcrFreqErr[%d] [%ld] <= [%d] (%ld.%06ld - %ld.%06ld)",
									svStFcrFreqErr, ps_g_FCRSTINF->faultDuration, ( nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec ), nowTime.tv_sec, nowTime.tv_usec, ps_fcrErrStTime->tv_sec, ps_fcrErrStTime->tv_usec );
								commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* DEBUG_FCR_FREQ_SIM */
							}
						}
						else
						{
							/* 異常動作開始時刻の初期化 */
							memset( ps_fcrErrStTime, 0, sizeof( struct timeval ) );
							/* 一次調整力異常動作中：復帰 */
							svStFcrFreqErr = PVCTL_FLG_OFF;
#ifdef DEBUG_FCR_FREQ_SIM
							sprintf( errMsg, "func_check_powerPlan fcr root-3 svStFcrFreqErr[%d]", svStFcrFreqErr);
							commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* DEBUG_FCR_FREQ_SIM */
						}
					}
					else
					{
#ifdef DEBUG_FCR_FREQ_CTL
//					printf( "FREQ ERROR \n" );
#endif /* DEBUG_FCR_FREQ_CTL */
						/* 異常動作開始時刻が初期化状態以外 */
						if( ps_fcrErrStTime->tv_sec != 0 )
						{

							/* 異常動作開始時刻のミリ秒以下差分で経過時間を補正 */
							if( ( nowTime.tv_usec - ps_fcrErrStTime->tv_usec ) < 0 )
							{
								/* 現在時刻のtv_usec < 異常動作開始時刻のtv_usecの場合は、秒数差分が経過時間ではないので補正1秒を秒数差分からマイナスする */
								ajustMSec = 1;
							}

							/* 異常継続時間 */
							errDuration = nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec;
							/* 異常時出力継続時間(秒)の経過をチェック */
							if( ps_g_FCRSTINF->faultDuration > errDuration )
							{
								/* 周波数偏移異常範囲かつ異常時出力継続時間(秒)経過前の場合 */
								/* 一次調整力異常動作中：発生 */
								svStFcrFreqErr = PVCTL_FLG_ON;
#ifdef DEBUG_FCR_FREQ_CTL
								printf( "%s func_check_powerPlan svStFcrFreqErr[%d] [%ld] <= [%d] (%ld.%06ld - %ld.%06ld)\n",
									PV_OWN_NAME, svStFcrFreqErr, ps_g_FCRSTINF->faultDuration, ( nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec ), nowTime.tv_sec, nowTime.tv_usec, ps_fcrErrStTime->tv_sec, ps_fcrErrStTime->tv_usec );
#endif /* DEBUG_FCR_FREQ_CTL */
							}
							else
							{
								/* 周波数偏移異常範囲かつ異常時出力継続時間(秒)経過後の場合 */
								/* 一次調整力異常動作中：復帰 */
								svStFcrFreqErr = PVCTL_FLG_OFF;
#ifdef DEBUG_FCR_FREQ_CTL
								printf( "%s func_check_powerPlan svStFcrFreqErr[%d] [%ld] > [%d] (%ld.%06ld - %ld.%06ld)\n",
									PV_OWN_NAME, svStFcrFreqErr, ps_g_FCRSTINF->faultDuration, ( nowTime.tv_sec - ps_fcrErrStTime->tv_sec - ajustMSec ), nowTime.tv_sec, nowTime.tv_usec, ps_fcrErrStTime->tv_sec, ps_fcrErrStTime->tv_usec );
#endif /* DEBUG_FCR_FREQ_CTL */
							}
						}
						else
						{
							/* 一次調整力異常動作中：発生 */
							svStFcrFreqErr = PVCTL_FLG_ON;
							/* 異常動作開始時刻の設定 */
							memcpy( ps_fcrErrStTime, &nowTime, sizeof( struct timeval ) );
#ifdef DEBUG_FCR_FREQ_CTL
							printf( "%s func_check_powerPlan FreqErrStart svStFcrFreqErr[%d]\n", PV_OWN_NAME, svStFcrFreqErr );
#endif /* DEBUG_FCR_FREQ_CTL */
						}
					}
/* [20260515] Add St */
				}
				else
				{
					/* 000ms以外の場合は、前周期の情報を使用する */
					svStFcrFreqErr = preSvStFcrCtlFreqErr;

				}
/* [20260515] Add St */
			}
/* [20250909] Add St */
			/* 一次使用有無がON、かつ約定△kWが0の場合 */
			else if( ( fcrUse == PVCTL_FLG_ON ) && ( fcrDltKw == 0 ) )
			{
				/* 一次調整力の調整コマ 目標残電力量に向かってSoC制御を行う */
/* [20250924] Add St */
				/* TM需給計画（残電力量）＝必要電力量に設定 */
				bttSoC = acSoC;
/* [20250924] Add Ed */
/* [20251204] Add St */
				/* 一次調整力SoC調整をON(1)に設定 */
				svStFcrCtlSocAjt = PVCTL_FLG_ON;
/* [20251204] Add Ed */
				/* 制御＝SoC制御をON(1)に設定 */
				svStCtlSoc = PVCTL_FLG_ON;
			}
/* [20250909] Add Ed */
/* [20250807] Add Ed */
			else
			{
				/* TM需給計画（残電力量）＝計画電力量を設定 */
				;

				/* 制御＝SoC制御をON(1)に設定 */
				svStCtlSoc = PVCTL_FLG_ON;
			}
		}
	}
/* [20260114] Chg Ed */

/* [20250807] Add St */
	/* 一次調整力コマではない場合、かつ、模擬周波数制御ではない場合、かつ、異常動作開始時刻が初期化されていない場合 */
	if( svStFcrCtl == PVCTL_FLG_OFF && preSvStFreqSimTest == PVCTL_FLG_OFF && ps_fcrErrStTime->tv_sec != 0 )
	{
		/* 異常動作開始時刻の初期化 */
		memset( ps_fcrErrStTime, 0, sizeof( struct timeval ) );
	}
/* [20250807] Add Ed */
	/* 出力制御情報が有効の場合 */
	if( ps_g_PGCONINF[PV_SCH_TYPE_BATTERY].statFlag == 1 )
	{
		/* 出力変化時間が異常の場合 */
		if( commfunc_pvsch_GetChgTime( PV_SCH_TYPE_BATTERY, &schChgTime ) < 0 )
		{
			/* 出力変化時間を最短に設定 */
			schChgTime = 5;
		}

		/* スケジュール情報のチェック */
		if( commfunc_pvsch_ErrCheck( PV_SCH_TYPE_BATTERY ) < 0 )
		{
			/* 時刻異常や未登録の場合は、0% */
			schData.data = 0;
			/* 出力変化時間を最短に設定 */
			schChgTime = 5;
		}
		else
		{
			/* 出力制御情報を取得し、データがある場合 */
			if( commfunc_pvsch_GetData(NULL, PV_SCH_TYPE_BATTERY, &schData ) == PV_SCH_TYPE_NONE )
			{
				/* スケジュールがない場合の設定値を取得 */
				schData.data = ps_g_SCHCTLFIL->schNonPer;
				/* 出力変化時間を最短に設定 */
				schChgTime = 5;
			}
		}

		/* スケジュール値が100%の場合、ランプ制御は不要の為、出力変化時間を0に設定 */
		if( schData.data == 100 )
		{
			schChgTime = 0;
		}

		/* 出力制御スケジュール値を更新 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_SCH_CHG_TIME, PV_TMFLG_NORMAL, schChgTime );
		/* 出力変化時間を更新 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_SCH_PER, PV_TMFLG_NORMAL, schData.data );

/* [20250828] Chg St */
		/* 出力制御の方が低い場合は、更新する */
		if( ps_g_BATTEINF != NULL )
		{
			if( ( ps_g_BATTEINF->maxDisChgPwr * schData.data / 100 ) < avlDischgPow )
			{
				avlDischgPow = ( ps_g_BATTEINF->maxDisChgPwr * schData.data / 100 );
			}
		}
/* [20250828] Chg Ed */
	}
/* [20251202] Add St */
	else
	{
		/* 出力制御スケジュール値が実装の場合 */
		if(ps_g_TMDATINF[ PV_SYS_TM_SCH_PER -1 ].statFlag == 1 )
		{
			/* 出力制御情報を取得し、データがある場合 */
			if( commfunc_pvsch_GetData(NULL, PV_SCH_TYPE_BATTERY, &schData ) == PV_SCH_TYPE_NONE )
			{
				/* スケジュールがない場合の設定値を取得 */
				schData.data = ps_g_SCHCTLFIL->schNonPer;
			}

			/* 出力制御スケジュール値を更新 */
			pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_SCH_PER, PV_TMFLG_NORMAL, schData.data );
#ifdef DEBUG_SCH_TM_PER
			sprintf( errMsg, "func_check_powerPlan : schData.data(%d)", schData.data );
			commfunc_syslog_msgset( pv_own_name, errMsg );
#endif /* DEBUG_SCH_TM_PER */
		}
	}
/* [20251202] Add Ed */

	/* 前回のスケジュール値と比較 */
	if( s_schDataPre != schData.data )
	{
		ps_g_EMCTLFIL->preSchUpdataFlg = PVCTL_FLG_ON;
		s_schDataPre = schData.data;
	}
	else
	{
		ps_g_EMCTLFIL->preSchUpdataFlg = PVCTL_FLG_OFF;
	}

	/* 需給計画の充電可能電力の方が小さい場合、補正する */
	if( avlChgPow < ps_g_EMCTLFIL->avlChgPow )
	{
		ps_g_EMCTLFIL->avlChgPow = avlChgPow;
	}

	/* 需給計画・出力制御の放電可能電力の方が小さい場合、補正する */
	if( avlDischgPow < ps_g_EMCTLFIL->avlDischgPow )
	{
		ps_g_EMCTLFIL->avlDischgPow = avlDischgPow;
	}
#ifdef DEBUG_SCH_DISCHG
	printf("func_check_powerPlan ps_g_EMCTLFIL->avlDischgPow=[%d] avlDischgPow[%d]\n", ps_g_EMCTLFIL->avlDischgPow, avlDischgPow);
#endif /* DEBUG_SCH_DISCHG */

	/* 充電電力上限 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_CHG_LIMIT, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->avlChgPow );

	/* 放電電力上限 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DISCHG_LIMIT, PV_TMFLG_NORMAL, ps_g_EMCTLFIL->avlDischgPow );

	/* 需給計画有無を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_ONGRID_PLAN, svStPlnFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* BCP使用有無を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_BCP_USE, svStBcpFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 受電電力一定制御の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_RCVPWR_CTL, svStRcvPowFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* デマンド制御の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DEMAND_CTL, svStPeakCutFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* SoC制御の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_SOC_CTL, svStCtlSoc, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 需給計画(残電力量) */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PLAN_REMAIN, PV_TMFLG_NORMAL, bttSoC );

	/* 需給計画(受電電力)一定制御目標値を更新 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_PLAN_RCV_POW, PV_TMFLG_NORMAL, planRcv );

	/* 需給調整市場SoC回復の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_SUPPLY_DEMAND_SOC, svStSpryDmndSocRecChrgFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 容量市場SoC回復の有効/無効を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET_SOC, svStCapMktSocRecChrgFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 需給調整市場動作中を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_SUPPLY_DEMAND, svStCategoryRR, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 容量市場(実効性テスト)動作中を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET_EXAM, svStCategoryCmExam, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 容量市場(実需給)動作中を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OPE_CAPACITY_MARKET, svStCategoryCm, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

/* [20250807] Add St */
/* [20251204] Add St */
	/* 一次調整力SoC調整を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL_SOC_AJT, svStFcrCtlSocAjt, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
/* [20251204] Add Ed */
	/* 一次調整力制御を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL, svStFcrCtl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 一次調整力異常動作中を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL_FREQ_ERR, svStFcrFreqErr, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	/* 一次調整力レポートデータ蓄積を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_REPORT_DATA_ACUM, svStFcrRepoAcum, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	if( svStFcrCtl == PVCTL_FLG_ON )
	{
		/* 約定△kWを更新 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_CONTRACT_DLT_POW, PV_TMFLG_NORMAL, fcrDltKw );
/* [20260130] Del */
	}
	else
	{
		/* 約定△kWを更新 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_CONTRACT_DLT_POW, PV_TMFLG_DROPTM, 0 );
/* [20260130] Del */
	}

/* [20250807] Add Ed */

	/* 再計算チェック */
	if( ( preSvStRcvPowFlg != svStRcvPowFlg )   || 			/* 受電電力一定制御フラグ */
		( preSvStPeakCutFlg != svStPeakCutFlg ) ||			/* デマンド制御フラグ */
		( preSvStBcpFlg != svStBcpFlg )	||					/* BCP使用有無 */
		( preSvStCategoryRR != svStCategoryRR ) ||			/* 需給調整市場動作中 */
		( preSvStCategoryCm != svStCategoryCm ) ||			/* 容量市場(実需給)動作中 */
		( preSvStCategoryCmExam != svStCategoryCmExam ) ||	/* 容量市場(実効性テスト)動作中 */
		( preSvStPlnFlg != svStPlnFlg ) )					/* 需給計画有無 */
	{
		/* 再計算フラグをONに設定 */
		ps_g_EMCTLFIL->reCalcFlg = PVCTL_FLG_ON;
	}

	return 0;
}
/* [20240701] Chg Ed */


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込みチェック処理（運転制御用）
 *
 * 機能：
 * 		pvctl_run_ctl()関数内で使用する 制御指示ファイルの読み込み関数として
 * 		機能する。
 * 		制御対象のファイルがあるかを結果として返却する。
 * 戻り値：
 * 		0：正常処理（制御対象外）
 * 		1：正常処理（制御対象）
 * 	   -1：異常終了（制御対象外）
 */
int pvctl_rmtctl_file_read_check( void )
{
	struct dirent			**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int						retCode;					/* 関数戻り値 */
	int						loopCnt1;					/* ループカウンタ */
	char					deleteName[256];			/* 削除ファイル名 */
	int						filenum;					/* ファイル数 */
	char					readFlg;					/* 遠隔制御ファイル読み込み完了フラグ */
	struct st_RTCTLINF		rmtctlinf;					/* 遠隔制御情報 */
	struct timeval			nowTime;
	struct tm				timeData;				/* 時刻情報 */

	PVCTL_MSG_LOW("START\n");

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, NULL);

	/* 読み込み完了フラグOFF */
	readFlg = 0;

	/* 遠隔制御の設定がない場合、読み込む */
	if( ps_g_RTCTLINF->filename[0] == 0 )
	{
		/* 運転制御ファイルのリストを取得 */
		/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
		/* （一覧作成時に func_choice_dirname フィルタを使用し、アルファベット順にソートする） */
		retCode = scandir( PV_PATH_DWL, &fileList, pvctl_rmtctl_choice_file, alphasort );
		if( retCode < 0 )
		{
			PVCTL_MSG_HIG("retCode=%d\n", retCode);
			return( -1 );
		}
		/* ファイル数を設定 */
		filenum = retCode;

		/* 現在運用中以外のファイル数が存在する場合 */
		if( filenum > 0 )
		{
			/* ファイルリストを降順で繰り返す */
			for( loopCnt1 = 0; loopCnt1 < filenum; loopCnt1++ )
			{
				/* 読み込み完了フラグがOFFの場合 */
				if( readFlg == 0 )
				{
					/* ファイルを解析する */
					memset( deleteName, 0x00, sizeof(deleteName));
					sprintf( deleteName, "%s%s", PV_PATH_DWL, fileList[loopCnt1]->d_name);

					/* ファイルの内容を0クリア */
					memset( &rmtctlinf, 0, sizeof(struct st_RTCTLINF) );

					/* ファイルの解析を行う */
					retCode = pvctl_rmtctl_file_read( fileList[loopCnt1]->d_name, &rmtctlinf );
#ifdef DEBUG_RMT
					sprintf(threadMsg, "pvctl_rmtctl_file_read_check : fileList(%2d/%2d) fileName[%s] retCode=%d", (filenum-loopCnt1), filenum, fileList[loopCnt1]->d_name, retCode);
					commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
#endif //DEBUG_RMT
					/* 解析結果を判定 */
					if( retCode < 0 )
					{
						/* ファイルのフォーマットが異常の場合は、ログ表示のみ */
						/* ファイルを削除する */
						unlink( deleteName );
						sprintf( threadMsg, "remote control file read error. [%s]", fileList[loopCnt1]->d_name );
						commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
						PVCTL_MSG_ERR("read error [%s] retCode=%d\n", fileList[loopCnt1]->d_name, retCode);
						PVCTL_MSG_LOW("unlink[%s]\n", deleteName);
						/* 最新ファイル以外は読まずに破棄する */
					}
					else
					{
						/* 現在時刻が期限を過ぎている場合 */
						if( rmtctlinf.limitSec < nowTime.tv_sec )
						{
							unlink( deleteName );

							/* 時刻情報に変換 */
							localtime_r( &rmtctlinf.limitSec, &timeData );
							timeData.tm_year += 1900;
							timeData.tm_mon+= 1;
							sprintf( threadMsg, "remote control file has expired(%04d/%02d/%02d %02d:%02d:%02d). [%s]", 
								timeData.tm_year,timeData.tm_mon,timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, fileList[loopCnt1]->d_name );
							commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
						}
						else
						{
							/* 読み込み完了フラグをONに設定 */
							readFlg = 1;
							/* ファイル名を保持 */
							memcpy(rmtctlinf.filename, fileList[loopCnt1]->d_name, sizeof(rmtctlinf.filename));
						}
					}
				}
				/* リソースを開放 */
				free( fileList[loopCnt1] );
			}
		}
		/* リソースを開放 */
		free( fileList );
	}

	/* ファイルの読み込みが完了した場合 */
	if( readFlg == 1 )
	{
		/* 遠隔制御情報をコピーする */
		memcpy(ps_g_RTCTLINF, &rmtctlinf, sizeof(struct st_RTCTLINF));

		sprintf( threadMsg, "remote control file read. [%s]", ps_g_RTCTLINF->filename );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
	}
	else if( ps_g_RTCTLINF->filename[0] != 0 )
	{
		/* 読み込み完了フラグをONに設定 */
		readFlg = 1;
	}

	PVCTL_MSG_LOW("END\n");
	return( readFlg );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（運転制御用）
 *
 * 機能：
 * 		pvctl_rmtctl()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名から遠隔制御指示ファイルの
 * 		有無を判断し、ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	pvctl_rmtctl_choice_file( const struct dirent *dir )
{
	char		checkName[128];		/* 確認用ファイル名 */
	char		deleteName[256];	/* 削除ファイル名 */

	memset(checkName, 0x00, sizeof(checkName));
	sprintf( checkName, "%s%s", PV_FNAME_REMOTECTL, ".csv");

	/* 確認ファイル名が３３文字以外の場合は処理対象外のため除外	*/
	if( strnlen( dir->d_name, 34 ) != 33 )
	{
#ifdef DEBUG
		printf("PVCTL_MON : FILE NAME ERROR_1 : ( %s )\n", dir->d_name );
#endif
		//PVCTL_MSG_LOW("filename length error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 送信ファイル名の先頭８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
	if( strncmp( dir->d_name, ps_g_PVSYSINF->siteIdName, 8 ) != 0 )
	{
#ifdef DEBUG
		printf("PVCTL_MON : SITE ID ERROR_2 : ( %s )\n", dir->d_name );
#endif
		//PVCTL_MSG_LOW("siteIdName error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 確認ファイル名の２１バイト目から１３文字が RTCTLINF.csv 以外は処理対象外 */
	if( strncmp( &dir->d_name[20], checkName, 13 ) != 0 )
	{
#ifdef DEBUG
		printf("PVCTL_MON : FILE NAME ERROR_3 : ( %s )\n", dir->d_name );
#endif
		//PVCTL_MSG_LOW("filename error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 現在運用中のファイルと同じ場合は処理対象外 */
	if( strncmp( dir->d_name, ps_g_RTCTLINF->filename, 33 ) == 0 )
	{
#ifdef DEBUG
		printf("PVCTL_MON : FILE NAME Execute : ( %s )\n", dir->d_name );
#endif
		return( 0 );
	}

	/* 現在運用中のファイルより過去のファイルは削除 */
	if( ( ps_g_RTCTLINF->filename[0] != 0 ) &&
	    ( strncmp( dir->d_name, ps_g_RTCTLINF->filename, 33 ) < 0 ) )
	{
#ifdef DEBUG
		printf(" FILE [ %s ] < [ %s ]\n", dir->d_name, ps_g_RTCTLINF->filename );
#endif
		memset( deleteName, 0x00, sizeof(deleteName));
		sprintf( deleteName, "%s%s", PV_PATH_DWL, dir->d_name);
		unlink( deleteName );
		return( 0 );
	}

#ifdef DEBUG
	printf("target file [%s]\n", dir->d_name);
#endif

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込み処理（遠隔制御用）
 *
 * 制御設備情報ファイル生成処理
 *
 * 機能：
 * 		RTCTLINF.csv ファイルから遠隔制御情報ファイル（RTCTLINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	pvctl_rmtctl_file_read( char * filename, struct st_RTCTLINF *ps_rmtctlinf)
{
	FILE				*fp;						/* ファイルポインタ */
	char				readBuff[4096];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt1;					/* ループカウンタ */
	int					workData;					/* 数値変換用 */
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int					syoriCount = 0;				/* CSV処理カウンタ */
	int					sepCount;					/* 分割データカウンタ */
	int					rmtctlCnt = 0;				/* 制御カウンタ */
	int					arrayNumb;					/* 配列添え字 */
	int					userNo;						/* ユーザ番号 */
	int					ctlMode;					/* 制御指示種別 */
	int					rmtYear;					/* 制御（年） */
	int					rmtMon;						/* 制御（月） */
	int					rmtDay;						/* 制御（日） */
	int					rmtHour;					/* 制御（時） */
	int					rmtMin;						/* 制御（分） */
	int					rmtSec;						/* 制御期限（秒） */
	short				force;						/* 強制制御 */
	short				soKind;						/* 所属装置種別 */
	short				soNumb;						/* 所属装置番号 */
	short				runState;					/* 変化率 */
	short				setPerVal;					/* 出力制御容量 */
	short				mKind;						/* 装置種別 */
	short				mNumb;						/* 装置番号 */
	short				svNumb;						/* 状態番号 */
	short				svVal;						/* 状態 */
	int					svIfLink;					/* 状態情報リンク */
	short				oneShot;					/* ワンショット維持時間 */
	short				statFlagRes;				/* 応動監視実装/未実装 */
	short				soKindRes;					/* 応動所属装置種別 */
	short				soNumbRes;					/* 応動所属装置番号 */
	short				svNumbRes;					/* 応動装置内計測番号 */
	int					svIfLinkRes;				/* 状態情報リンク */
	short				svValRes;					/* 応動方向 */
	short				timeOutRes;					/* 応動時間 */
	short				statFlagNotCtl;				/* 制御不可SV条件 監視実装/未実装 */
	short				soKindNotCtl;				/* 制御不可SV条件 所属装置種別 */
	short				soNumbNotCtl;				/* 制御不可SV条件 所属装置番号 */
	short				svNumbNotCtl;				/* 制御不可SV条件 装置内計測番号 */
	int					svIfLinkNotCtl;				/* 状態情報リンク */
	short				svValNotCtl;				/* 制御不可SV条件 方向 */
    struct tm  			mktm;						/* 時刻変換 */

/* [20230905] Add St */
	/* ログ出力 */
	sprintf( errMsg, "remote control file read. [%s]", filename );
	commfunc_remotecontrol_logout(errMsg);
/* [20230905] Add Ed */
	
	PVCTL_MSG_LOW("START\n");
	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_DWL );
	strcat(csvFilePath, filename );

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 4096, fp) != NULL )
		{
/* [20230905] Add St */
			/* ログ出力 */
			readBuff[strlen(readBuff)-1] = 0;
			commfunc_remotecontrol_logout(readBuff);
/* [20230905] Add Ed */


			/* 分割データカウンタを初期化 */
			sepCount = 0;
			/* CSV処理カウンタを＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（9文字以下）はスキップ */
			if( strlen( readBuff ) < 10) continue;
			/* CSVデータの分割 */
			csvCount = pvctl_cmn_sepcsv_data( readBuff, &s_sepStr );

/* [20221011] Chg St */
			/* CSVデータ数が8,9以外、且つ、17以外の場合 */
			if( ( csvCount != 8 ) &&
			    ( csvCount != 9 ) &&
				( csvCount != 17 ) )
/* [20221011] Chg Ed */
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 8 or 11). File[%s] line[%d] DATACNT[%d]", filename, syoriCount, csvCount );
				commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
				retCode = -1;
				PVCTL_MSG_ERR("%s\n", errMsg);
				break;
			}

/* [20221011] Chg St */
			/* 制御指示情報 */
			if( ( csvCount == 8 ) ||
				( csvCount == 9 ) )
/* [20221011] Chg Ed */
			{
				/* ユーザ番号の取得 */
				userNo = atoi( s_sepStr.data[sepCount] );
				sepCount++;
/* [20230216] Del St */
				/* ミツウロコではユーザ番号は使わないので、チェック処理を削除 */
				//if( userNo <= 0 )
				//{
				//	sprintf( errMsg, "CSV-Format Error (userNo). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
				//	commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
				//	PVCTL_MSG_ERR("%s\n", errMsg);
				//	break;
				//}
/* [20230216] Del Ed */

				/* 制御指示種別の取得 */
				ctlMode = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( ( ctlMode < PV_CTL_SW_RMT ) || ( ctlMode > PV_CTL_SW_ONGRID_3 ))
				{
					sprintf( errMsg, "CSV-Format Error (ctlMode). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}

				/* 制御期限（年）の取得 */
				rmtYear = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( rmtYear < 2000 )
				{
					sprintf( errMsg, "CSV-Format Error (year). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}

				/* 制御期限（月）の取得 */
				rmtMon = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (rmtMon < 1 ) || ( 12 < rmtMon ) )
				{
					sprintf( errMsg, "CSV-Format Error (month). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}

				/* 制御期限（日）の取得 */
				rmtDay = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (rmtDay < 1 ) || ( 31 < rmtDay ) )
				{
					sprintf( errMsg, "CSV-Format Error (day). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}
			
				/* 制御期限（時）の取得 */
				rmtHour = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (rmtHour < 0 ) || ( 23 < rmtHour ) )
				{
					sprintf( errMsg, "CSV-Format Error (hour). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}

				/* 制御期限（分）の取得 */
				rmtMin = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (rmtMin < 0 ) || ( 59 < rmtMin ) )
				{
					sprintf( errMsg, "CSV-Format Error (min). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}

				/* 制御期限（秒）の取得 */
				rmtSec = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (rmtSec < 0 ) || (59 < rmtSec ) )
				{
					sprintf( errMsg, "CSV-Format Error (sec). File[%s] LINE[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					PVCTL_MSG_ERR("%s\n", errMsg);
					break;
				}

/* [20221011] Add St */
				if( csvCount == 9 )
				{
					/* 遠隔制御通し番号の取得 */
					sprintf(ps_rmtctlinf->rmtCtlNumb, "%s", s_sepStr.data[sepCount] );
					sepCount++;
				}
/* [20221011] Add Ed */

				/* ファイル名を保持 */
				memcpy( ps_rmtctlinf->filename, filename, sizeof(ps_rmtctlinf->filename));

	            /* 日時から経過時間に変換 */
	            ps_rmtctlinf->limitSec = func_cmn_mktime(rmtYear, rmtMon, rmtDay, rmtHour, rmtMin, rmtSec);

				/* 制御モード */
	            ps_rmtctlinf->ctlMode = ctlMode;

				/* ユーザ番号を設定 */
	            ps_rmtctlinf->userNo = userNo;

#ifdef DEBUG_RMT
				printf("userNo[%d], ctlMode[%d], Limit[%04d/%02d/%02d %02d:%02d:%02d], fileName[%s]\n", userNo, ctlMode, rmtYear, rmtMon, rmtDay, rmtHour, rmtMin, rmtSec, ps_rmtctlinf->filename );
#endif //DEBUG_RMT
			}
			else
			{
				/* 制御不可SV条件 実装／未実装の取得 */
				statFlagNotCtl = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( statFlagNotCtl < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (not cntrol statFlag). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 制御不可SV条件 実装の場合 */
				if( statFlagNotCtl == 1 )
				{
					/* 制御不可SV条件 の装置種別の取得 */
					soKindNotCtl = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( ( soKindNotCtl < PV_MKIND_PVSYS ) || ( soKindNotCtl > PV_MKIND_LAST ) )
					{
						sprintf( errMsg, "CSV-Format Error (not cntrol Machine Kind). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 制御不可SV条件 装置番号の取得 */
					soNumbNotCtl = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( (soNumbNotCtl < 1) || (soNumbNotCtl > ps_g_PVSYSINF->s_machineInfo[soKindNotCtl].maxMachine) )
					{
						sprintf( errMsg, "CSV-Format Error (not cntrol Machine Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 制御不可SV条件 状態番号の取得 */
					svNumbNotCtl = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( (svNumbNotCtl < 1) || (svNumbNotCtl > ps_g_PVSYSINF->s_machineInfo[soKindNotCtl].maxSv) )
					{
						sprintf( errMsg, "CSV-Format Error (not cntrol SV Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 制御対象の状態情報のリンクを取得 */
					svIfLinkNotCtl = ps_g_PVSYSINF->s_machineInfo[soKindNotCtl].svIfLink + ps_g_PVSYSINF->s_machineInfo[soKindNotCtl].maxSv * (soNumbNotCtl - 1) + svNumbNotCtl - 1;
	
					/* 制御不可SV条件 対象の状態情報が未実装の場合 */
					if( ps_g_SVDATINF[svIfLinkNotCtl].statFlag == 0 )
					{
						sprintf( errMsg, "CSV-Format Error (Non-Exist not cntrol SV Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 制御不可SV条件 操作方向の取得 */
					svValNotCtl = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( (svValNotCtl != 1) && (svValNotCtl != 0 ) )
					{
						sprintf( errMsg, "CSV-Format Error (not cntrol SV Value). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}
				}
				else
				{
					sepCount++;
					sepCount++;
					sepCount++;
					sepCount++;
				}

				/* 強制制御の取得 */
				force = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (force != 0 ) && (force != 1 ) )
				{
					sprintf( errMsg, "CSV-Format Error (forceCtl). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 装置種別の取得 */
				mKind = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( ( mKind < PV_MKIND_PVSYS ) || ( mKind > PV_MKIND_LAST ) )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Kind). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 装置番号の取得 */
				mNumb = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (mNumb < 1) || (mNumb > ps_g_PVSYSINF->s_machineInfo[mKind].maxMachine) )
				{
					sprintf( errMsg, "CSV-Format Error (Machine Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 状態番号の取得 */
				svNumb = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (svNumb < 1) || (svNumb > ps_g_PVSYSINF->s_machineInfo[mKind].maxSv) )
				{
					sprintf( errMsg, "CSV-Format Error (SV Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 制御対象の状態情報のリンクを取得 */
				svIfLink = ps_g_PVSYSINF->s_machineInfo[mKind].svIfLink + ps_g_PVSYSINF->s_machineInfo[mKind].maxSv * (mNumb - 1) + svNumb - 1;

				/* 制御対象の状態情報が未実装の場合 */
				if( ps_g_SVDATINF[svIfLink].statFlag == 0 )
				{
					sprintf( errMsg, "CSV-Format Error (Non-Exist SV Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 操作方向の取得 */
				svVal = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( (svVal != 1) && (svVal != 0 ) )
				{
					sprintf( errMsg, "CSV-Format Error (SV Value). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* ワンショット維持時間の取得 */
				oneShot = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( oneShot < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (one shot). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 応動実装／未実装の取得 */
				statFlagRes = atoi( s_sepStr.data[sepCount] );
				sepCount++;
				if( statFlagRes < 0 )
				{
					sprintf( errMsg, "CSV-Format Error (response statFlag). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
					commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
					retCode = -1;
					break;
				}

				/* 応動実装の場合 */
				if( statFlagRes == 1 )
				{
					/* 応動の装置種別の取得 */
					soKindRes = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( ( soKindRes < PV_MKIND_PVSYS ) || ( soKindRes > PV_MKIND_LAST ) )
					{
						sprintf( errMsg, "CSV-Format Error (response Machine Kind). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 応動装置番号の取得 */
					soNumbRes = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( (soNumbRes < 1) || (soNumbRes > ps_g_PVSYSINF->s_machineInfo[soKindRes].maxMachine) )
					{
						sprintf( errMsg, "CSV-Format Error (response Machine Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 応動状態番号の取得 */
					svNumbRes = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( (svNumbRes < 1) || (svNumbRes > ps_g_PVSYSINF->s_machineInfo[soKindRes].maxSv) )
					{
						sprintf( errMsg, "CSV-Format Error (response SV Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 制御対象の状態情報のリンクを取得 */
					svIfLinkRes = ps_g_PVSYSINF->s_machineInfo[soKindRes].svIfLink + ps_g_PVSYSINF->s_machineInfo[soKindRes].maxSv * (soNumbRes - 1) + svNumbRes - 1;
	
					/* 応動対象の状態情報が未実装の場合 */
					if( ps_g_SVDATINF[svIfLinkRes].statFlag == 0 )
					{
						sprintf( errMsg, "CSV-Format Error (Non-Exist response SV Number). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 応動操作方向の取得 */
					svValRes = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( (svValRes != 1) && (svValRes != 0 ) )
					{
						sprintf( errMsg, "CSV-Format Error (response SV Value). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}

					/* 応動時間の取得 */
					timeOutRes = atoi( s_sepStr.data[sepCount] );
					sepCount++;
					if( timeOutRes < 0 )
					{
						sprintf( errMsg, "CSV-Format Error (response Time Out). File[%s] Line[%d] DAT[%d]", filename, syoriCount, sepCount );
						commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
						retCode = -1;
						break;
					}
				}
				/********** 制御不可SV条件 情報の設定 **********/
				/* 実装／未実装の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].statFlagNotCtl = statFlagNotCtl;
				if( statFlagNotCtl == 1 )
				{
					/* 制御不可SV条件 装置種別の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soKindNotCtl = soKindNotCtl;
					/* 制御不可SV条件 装置番号の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soNumbNotCtl = soNumbNotCtl;
					/* 制御不可SV条件 状態番号の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svNumbNotCtl = svNumbNotCtl;
					/* 状態情報リンクの設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svIfLinkNotCtl = svIfLinkNotCtl;
					/* 制御不可SV条件 操作方向の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svValNotCtl = svValNotCtl;
				}

				/********** 制御情報の設定 **********/
				/* 実装／未実装の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].statFlag = 1;
				/* 強制制御の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].forceCtl = force;
				/* 装置種別の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soKindCtl = mKind;
				/* 装置番号の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soNumbCtl = mNumb;
				/* 状態番号の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svNumbCtl = svNumb;
				/* 状態情報リンクの設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svIfLinkCtl = svIfLink;
				/* 操作方向の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svValCtl = svVal;
				/* リンク情報の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].oneShot = oneShot;


				/********** 応動情報の設定 **********/
				/* 実装／未実装の設定 */
				ps_rmtctlinf->s_ctlinfo[rmtctlCnt].statFlagRes = statFlagRes;
				if( statFlagRes == 1 )
				{
					/* 応動装置種別の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soKindRes = soKindRes;
					/* 応動装置番号の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soNumbRes = soNumbRes;
					/* 応動状態番号の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svNumbRes = svNumbRes;
					/* 状態情報リンクの設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svIfLinkRes = svIfLinkRes;
					/* 応動操作方向の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svValRes = svValRes;
					/* 応動時間の設定 */
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].timeOutRes = timeOutRes;
				}


#ifdef DEBUG_RMT
				printf("[%d]: FlagNotCtl=%d, not control soKind,Numb,Sv,Val=[%d,%d,%d,%d],svIfLink=%d, force%d, set soKind,Numb,Sv,Val=[%d,%d,%d,%d],svIfLink=%d, One=%d, FlagRes=%d, res soKind,Numb,Sv,Val=[%d,%d,%d,%d],svIfLink=%d, timeOut=%d\n",
					rmtctlCnt,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].statFlagNotCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soKindNotCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soNumbNotCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svNumbNotCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svValNotCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svIfLinkNotCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].forceCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soKindCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soNumbCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svNumbCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svValCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svIfLinkCtl,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].oneShot,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].statFlagRes,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soKindRes,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].soNumbRes,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svNumbRes,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svValRes,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].svIfLinkRes,
					ps_rmtctlinf->s_ctlinfo[rmtctlCnt].timeOutRes
				);
#endif //DEBUG_RMT
				rmtctlCnt++;
			}
		}

		/* 制御数を設定 */
	    ps_rmtctlinf->ctlNum = rmtctlCnt;

		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "File[%s] Open error", filename );
		commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
		retCode = -1;
		PVCTL_MSG_ERR("%s\n", errMsg);
	}

	/* 終了 */
	return( retCode );	
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込みチェック処理（運転制御用）
 *
 * 機能：
 * 		pvctl_run_ctl()関数内で使用する 制御指示ファイルの読み込み関数として
 * 		機能する。
 * 		制御対象のファイルがあるかを結果として返却する。
 * 戻り値：
 * 		0：正常処理（制御対象外）
 * 		1：正常処理（制御対象）
 * 	   -1：異常終了（制御対象外）
 */
int pvctl_confupdate_file_read_check( void )
{
	struct dirent			**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int						retCode;					/* 関数戻り値 */
	int						loopCnt1;					/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	char					deleteName[256];			/* 削除ファイル名 */
	int						filenum;					/* ファイル数 */
	char					readFlg;					/* 遠隔制御ファイル読み込み完了フラグ */
	struct st_CEMSPINF		s_cemspinf;					/* CEMS制御情報 */
	struct st_DEMNDINF		s_demndinf;					/* デマンド監視情報 */
	struct st_RCVPWINF		s_rcvpwinf;					/* 受電電力一定制御情報 */
	struct st_OVRCHINF		s_ovrchinf;					/* 過充電回避制御情報 */
	struct st_BATTEINF		s_batteinf;					/* バッテリ情報 *//* [20230830] Add */
	struct st_PWPLNINF		s_pwplninf[PV_POWPLAN_IDX_MAX];/* 受電電力計画値情報 */
/* [20250807] Add St */
	struct st_FCRSTINF		s_fcrstinf;					/* 一次調整力パラメータ設定情報 */
	struct st_FCRRPINF		s_fcrrpinf;					/* 一次調整力レポート蓄積項目情報 */
	int						loopCnt4;					/* ループカウンタ */
/* [20250807] Add Ed */
	char					readFileName[128];			/* 読み込みファイル名 */
	int						userNo=0;					/* ユーザ番号 */
	int						updateType=-1;				/* 設定変更種別 */
#ifdef DEBUG_POWER_PLAN
	int			planIdx;
	int			loopCnt3;					/* ループカウンタ */
	char		temp[512];
#endif //DEBUG_POWER_PLAN

	PVCTL_MSG_LOW("START\n");

	/* 読み込みファイル名を初期化 */
	memset(readFileName, 0, sizeof(readFileName));

	/* 運転制御ファイルのリストを取得 */
	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	/* （一覧作成時に pvctl_confupdate_choice_file フィルタを使用し、アルファベット順にソートする） */
	retCode = scandir( PV_PATH_DWL, &fileList, pvctl_confupdate_choice_file, alphasort );
	if( retCode < 0 )
	{
		PVCTL_MSG_HIG("retCode=%d\n", retCode);
		return( -1 );
	}
	/* ファイル数を設定 */
	filenum = retCode;

	/* 読み込み完了フラグOFF */
	readFlg = 0;

	/* 現在運用中以外のファイル数が存在する場合 */
	if( filenum > 0 )
	{
		/* ファイルリストを昇順で繰り返す */
		for( loopCnt1 = 0; loopCnt1 < filenum; loopCnt1++ )
		{
			/* ファイルを解析する */
			memset( deleteName, 0x00, sizeof(deleteName));
			sprintf( deleteName, "%s%s", PV_PATH_DWL, fileList[loopCnt1]->d_name);

			memset( &s_cemspinf, 0x00, sizeof(s_cemspinf));
			memset( &s_demndinf, 0x00, sizeof(s_demndinf));
			memset( &s_rcvpwinf, 0x00, sizeof(s_rcvpwinf));
			memset( &s_ovrchinf, 0x00, sizeof(s_ovrchinf));
			memset( s_pwplninf, 0x00, sizeof(s_pwplninf));
			memset( &s_batteinf, 0x00, sizeof(s_batteinf));	/* [20230830] Add */
/* [20250807] Add St */
			memset( &s_fcrstinf, 0x00, sizeof(s_fcrstinf));
			memset( &s_fcrrpinf, 0x00, sizeof(s_fcrrpinf));
/* [20250807] Add Ed */
/* [20250807] Chg St */
			/* ファイルの解析を行う */
			retCode = pvctl_confupdate_file_read( fileList[loopCnt1]->d_name, &s_cemspinf, &s_demndinf, &s_rcvpwinf, &s_ovrchinf, s_pwplninf, &s_batteinf, &s_fcrstinf, &s_fcrrpinf, &userNo, &updateType);	/* [20230830] Chg */
			PVCTL_MSG_MID("fileList(%2d/%2d) fileName[%s] retCode=%d\n", (filenum-loopCnt1), filenum, fileList[loopCnt1]->d_name, retCode);
/* [20250807] Chg Ed */
			/* 解析結果を判定 */
			if( retCode < 0 )
			{
				/* ファイルを削除する */
				unlink( deleteName );
				sprintf( threadMsg, "update config file read error. [%s]", fileList[loopCnt1]->d_name );
				commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
				PVCTL_MSG_ERR("read error [%s] retCode=%d\n", fileList[loopCnt1]->d_name, retCode);
				PVCTL_MSG_LOW("unlink[%s]\n", deleteName);


				/* 設定変更、ユーザ番号が登録されている場合 */
/* [20230125] Chg St */
				if( ( userNo > 0 ) &&
					( ( updateType == PV_UPDATECONF_TYPE_PARA ) ||
					  ( updateType == PV_UPDATECONF_TYPE_PLAN )))
/* [20230125] Chg Ed */
				{
					/* 設定異常を登録する */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
				}
			}
			else
			{
/* [20230125] Chg St */
				if( ( updateType == PV_UPDATECONF_TYPE_PARA ) ||
					( updateType == PV_UPDATECONF_TYPE_PLAN ) )
/* [20230125] Chg Ed */
				{
					/* 設定異常を解除する */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

					/* 起動後の設定ファイルと不一致の場合 */
					if(  strncmp( fileList[loopCnt1]->d_name, s_updateConfFile, 33 ) != 0 )
					{
						/* 起動後の再読み込み、且つ、前回起動中の最終ファイルと一致する場合 */
						if( ( s_updateConfFile[0] == 0 ) &&
							( strncmp( fileList[loopCnt1]->d_name, ps_g_PVSYSINF->updateConfFile, 33 ) == 0 ))
						{
							/* 状変は登録しない */
						}
						else
						{
/* [20230125] Chg St */
							if( updateType == PV_UPDATECONF_TYPE_PARA )
							{
								/* 状変に登録する */
								commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_SET, PV_MKIND_PVSYS, 1, PV_SYS_SV_CEMSPARAMSET, 1, userNo, PV_SYS_SV_CEMSPARAMSET-1 );
							}
							else//if( updateType == PV_UPDATECONF_TYPE_PLAN )
							{
								/* 状変に登録する */
								commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_SET, PV_MKIND_PVSYS, 1, PV_SYS_SV_PLANPARAMSET, 1, userNo, PV_SYS_SV_PLANPARAMSET-1 );
							}
/* [20230125] Chg Ed */
						}

						/* すでに読み込みファイルがあった場合 */
						if( readFileName[0] != 0 )
						{
							/* そのファイルは古い為、削除する */
							memset( deleteName, 0x00, sizeof(deleteName));
							sprintf( deleteName, "%s%s", PV_PATH_DWL, readFileName);
							/* ファイルを削除する */
							unlink( deleteName );
							sprintf( threadMsg, "update config file found. delete. [%s]", readFileName );
							commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
						}
						/* ファイル名を保持 */
						memcpy(readFileName, fileList[loopCnt1]->d_name, sizeof(readFileName));
						/* 読み込み完了フラグをONに設定 */
						readFlg = 1;
					}
				}
			}
			/* リソースを開放 */
			free( fileList[loopCnt1] );
		}
	}

	/* ファイルの読み込みが完了した場合 */
	if( readFlg == 1 )
	{
		/* 運用中のファイルパスを取得 */
		memset( deleteName, 0x00, sizeof(deleteName));
		sprintf( deleteName, "%s%s", PV_PATH_DWL, ps_g_PVSYSINF->updateConfFile);

		/* ファイルが存在している、現在運用中のファイルと不一致の場合 */
		if( (ps_g_PVSYSINF->updateConfFile[0] != 0 ) &&
		    ( strncmp( readFileName, ps_g_PVSYSINF->updateConfFile, 33 ) != 0 ) &&
			(access( deleteName, R_OK | W_OK ) == 0 ) )
		{
			/* ファイルを削除する */
			unlink( deleteName );
			sprintf( threadMsg, "update config file exchange. now config file delete. [%s]", readFileName );
			commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
		}

		/* 設定変更情報に伴うファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

		/* 設定ファイル名をコピーする */
		memcpy(s_updateConfFile, readFileName, sizeof(s_updateConfFile));
		memcpy(ps_g_PVSYSINF->updateConfFile, readFileName, sizeof(ps_g_PVSYSINF->updateConfFile));

		sprintf( threadMsg, "update config file read. [%s]", ps_g_PVSYSINF->updateConfFile );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		/* 受電電力一定制御情報を更新を更新 */
		/* 受電電力平均時間が変更となる場合 */
		if( ps_g_RCVPWINF->rcvPowAveTime != s_rcvpwinf.rcvPowAveTime )
		{
			/* 移動平均情報を初期化 */
			func_avlData_init( &s_rcvPowAve, (s_rcvpwinf.rcvPowAveTime / 100) );
		}
		/* 負荷電力平均時間が変更となる場合 */
		if( ps_g_RCVPWINF->loadPowAveTime != s_rcvpwinf.loadPowAveTime )
		{
			/* 移動平均情報を初期化 */
			func_avlData_init( &s_loadPowAve, (s_rcvpwinf.loadPowAveTime / 100) );
		}

		/* 再計算チェック */
		/* 制御不感帯、デマンド閾値、最低受電電力電力、放電再開SOC、充電再開SOCは、毎周期チェックしている */
		/* BCPの変更も放電禁止/解除チェックで毎周期行っている */

		memcpy(ps_g_CEMSPINF, &s_cemspinf, sizeof(struct st_CEMSPINF));
		/* BT充放電制御の有効/無効を更新 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_BATTE_CTL, ps_g_CEMSPINF->btControl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

/* [20230217] Del St */
		///* PV抑制制御の有効/無効を更新 */
		//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_CTL, ps_g_CEMSPINF->pvControl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		///* 受電電力一定制御の有効/無効を更新 */
		//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_RCVPWR_CTL, ps_g_CEMSPINF->rcvPowConst, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		///* 過充電回避制御の有効/無効を更新 */
		//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_OVRCHG_CTL, ps_g_CEMSPINF->ovrChgAvoid, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		///* デマンド制御の有効/無効を更新 */
		//commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DEMAND_CTL, ps_g_CEMSPINF->dmdControl, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
/* [20230217] Del Ed */

		/* デマンド制御情報を更新 */
		memcpy( &ps_g_DEMNDFIL->s_updatedmd, &s_demndinf, sizeof(struct st_DEMNDINF));

		/* 受電電力一定制御情報を更新 */
		memcpy(ps_g_RCVPWINF, &s_rcvpwinf, sizeof(struct st_RCVPWINF));

		/* 過充電回避制御情報を更新 */
		memcpy(ps_g_OVRCHINF, &s_ovrchinf, sizeof(struct st_OVRCHINF));

#ifdef DEBUG_POWER_PLAN
	commfunc_syslog_msgset( PV_OWN_NAME, "s_pwplninf" );

	/* 登録日数分ループ */
	for( loopCnt2 = 0; loopCnt2 < s_pwplninf[0].plnDayNum; loopCnt2++ )
	{
		sprintf(temp, "%d,%d,0", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].plnStat[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,1", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].rcvPow[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,2", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].bttPow[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,3", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].bttSoC[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,4", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].useBcp[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

/* [20230529] Add St */
		sprintf(temp, "%d,%d,5", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].acUse[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,6", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].acSoC[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,7", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].avlDischgPow[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );

		sprintf(temp, "%d,%d,8", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].avlChgPow[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
/* [20230529] Add Ed */
/* [20240426] Add St */
		sprintf(temp, "%d,%d,9", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].peakCutStat[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
		sprintf(temp, "%d,%d,10", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].capMktUse[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
/* [20240426] Add Ed */
/* [20250807] Add St */
/* [20251209] Chg St */
		sprintf(temp, "%d,%d,11", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].fcrUse[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
		sprintf(temp, "%d,%d,12", 0,s_pwplninf[0].s_powDayPln[loopCnt2].plnDay);
		for(loopCnt3=0; loopCnt3<48; loopCnt3++)
			sprintf(&temp[strlen(temp)], ",%d", s_pwplninf[0].s_powDayPln[loopCnt2].fcrDltKw[loopCnt3]);
		sprintf( threadMsg, "s_pwplninf [%s]", temp );
		commfunc_syslog_msgset( PV_OWN_NAME, threadMsg );
/* [20251209] Chg Ed */
/* [20250807] Add Ed */
	}
#endif //DEBUG_POWER_PLAN


		/* 受電電力計画値情報を更新 */
		memcpy(ps_g_PWPLNINF, &s_pwplninf, sizeof(struct st_PWPLNINF));

		/* 受電電力計画値情報を更新する */
		memcpy(ps_g_PWPLNFIL->powplndata, s_pwplninf, sizeof(s_pwplninf));

/* [20230830] Add St */
		if( ( ps_g_BATTEINF != NULL ) &&
			( s_batteinf.statFlag == 1 ))
		{
			/* バッテリ用途 */
			ps_g_BATTEINF->batteType = s_batteinf.batteType;
			/* バッテリ容量(wh) */
			ps_g_BATTEINF->batteCap = s_batteinf.batteCap;
			/* 最大定格電力(w) */
			ps_g_BATTEINF->maxRatedPwr = s_batteinf.maxRatedPwr;
			/* 最大充電電力(w) */
			ps_g_BATTEINF->maxChargePwr = s_batteinf.maxChargePwr;
			/* 最大放電電力(w) */
			ps_g_BATTEINF->maxDisChgPwr = s_batteinf.maxDisChgPwr;
			/* SoC閾値上限 */
			ps_g_BATTEINF->socUpper = s_batteinf.socUpper;
			/* SoC閾値下限 */
			ps_g_BATTEINF->socLower = s_batteinf.socLower;
		}
/* [20230830] Add Ed */

/* [20250807] Add St */
		/* 一次調整力パラメータ設定情報を更新する */
		s_fcrstinf.readFlg = readFlg;
		memcpy(ps_g_FCRSTINF, &s_fcrstinf, sizeof(struct st_FCRSTINF));
#ifdef DEBUG_FCR_CTL_PRM
		printf("(%s) ps_g_FCRSTINF baseFreq=%d variationRate=%d deadBand=%d freqDevtThrsld=%d freqDevtThrsldLower=%d freqDevtThrsldUpper=%d faultDuration=%d ratedPower=%d dischgPowerChgAmount=%d chgPowerChgAmount=%d deadBandResType=%d chgResponsiveEnabled=%d\n",
			PV_OWN_NAME, ps_g_FCRSTINF->baseFreq, ps_g_FCRSTINF->variationRate, ps_g_FCRSTINF->deadBand, ps_g_FCRSTINF->freqDevtThrsld,
			ps_g_FCRSTINF->freqDevtThrsldLower, ps_g_FCRSTINF->freqDevtThrsldUpper, ps_g_FCRSTINF->faultDuration, ps_g_FCRSTINF->ratedPower,
			ps_g_FCRSTINF->dischgPowerChgAmount, ps_g_FCRSTINF->chgPowerChgAmount, ps_g_FCRSTINF->deadBandResType, ps_g_FCRSTINF->chgResponsiveEnabled );/* [20251215] 充電応動有無の対応 Chg */
#endif /* DEBUG_FCR_CTL_PRM */
		/* 一次調整力レポート蓄積項目情報を更新する */
		s_fcrrpinf.readFlg = readFlg;
		memcpy(ps_g_FCRRPINF, &s_fcrrpinf, sizeof(struct st_FCRRPINF));
#ifdef DEBUG_FCR_CTL_PRM
		for(int loopCnt4 = 0; loopCnt4 < PV_FCRRP_DATA_MAX; loopCnt4++)
		{
			printf("(%s) ps_g_FCRRPINF dataMax=%d loopCnt=%d dataNumb=%d mKind=%d mNumb=%d tmNumb=%d\n", 
				PV_OWN_NAME, ps_g_FCRRPINF->dataMax, loopCnt4, ps_g_FCRRPINF->s_fcrrpData[loopCnt4].dataNumb, ps_g_FCRRPINF->s_fcrrpData[loopCnt4].mKind,
				ps_g_FCRRPINF->s_fcrrpData[loopCnt4].mNumb, ps_g_FCRRPINF->s_fcrrpData[loopCnt4].tmNumb);
		}
#endif /* DEBUG_FCR_CTL_PRM */
/* [20250807] Add Ed */

		/* 設定変更情報のミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	}
	else if( ps_g_PVSYSINF->updateConfFile[0] != 0 )
	{
		/* 読み込み完了フラグをONに設定 */
		readFlg = 1;
	}
	/* リソースを開放 */
	free( fileList );

	PVCTL_MSG_LOW("END\n");
	return( readFlg );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（設定変更ファイル用）
 *
 * 機能：
 * 		pvctl_confupdate()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュール制御指示ファイルの
 * 		有無を判断し、ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	pvctl_confupdate_choice_file( const struct dirent *dir )
{
	char		checkName[128];		/* 確認用ファイル名 */
	char		deleteName[256];	/* 削除ファイル名 */

	memset(checkName, 0x00, sizeof(checkName));
	sprintf( checkName, "%s%s", PV_FNAME_UPDATECNF, ".csv");

	/* 確認ファイル名が３３文字以外の場合は処理対象外のため除外	*/
	if( strnlen( dir->d_name, 34 ) != 33 )
	{
#ifdef PV_DEBUG
		printf("PVCTL_MON : FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
		//PVCTL_MSG_LOW("filename length error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 送信ファイル名の先頭８文字が収集装置ＩＤと一致しないファイルは処理対象外 */
	if( strncmp( dir->d_name, ps_g_PVSYSINF->siteIdName, 8 ) != 0 )
	{
#ifdef PV_DEBUG
		printf("PVCTL_MON : SITE ID ERROR : ( %s )\n", dir->d_name );
#endif
		//PVCTL_MSG_LOW("siteIdName error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 確認ファイル名の２１バイト目から１３文字が UPDATECNF.csv 以外は処理対象外 */
	if( strncmp( &dir->d_name[20], checkName, 13 ) != 0 )
	{
#ifdef PV_DEBUG
		printf("PVCTL_MON : FILE NAME ERROR : ( %s )\n", dir->d_name );
#endif
		//PVCTL_MSG_LOW("filename error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 起動後の運用中のファイルと同じ場合は処理対象外 */
	if( strncmp( dir->d_name, s_updateConfFile, 33 ) == 0 )
	{
		//PVCTL_MSG_LOW("siteIdName error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* 現在運用中のファイルより過去のファイルは削除 */
	if( ( ps_g_PVSYSINF->updateConfFile[0] != 0 ) &&
	    ( strncmp( dir->d_name, ps_g_PVSYSINF->updateConfFile, 33 ) < 0 ) )
	{
#ifdef PV_DEBUG
		printf("NKFTPSND : STOCK OUT FILE [ %s ] < [ %s ]\n", dir->d_name, ps_g_PVSYSINF->updateConfFile );
#endif
		memset( deleteName, 0x00, sizeof(deleteName));
		sprintf( deleteName, "%s%s", PV_PATH_DWL, dir->d_name);
		unlink( deleteName );
		//PVCTL_MSG_LOW("siteIdName error [%s]\n", dir->d_name);
		return( 0 );
	}

	PVCTL_MSG_MID("target file [%s]\n", dir->d_name);

	/* 処理対象 */
	return( 1 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込み処理（スケジュール制御用）
 *
 * 制御設備情報ファイル生成処理
 *
 * 機能：
 * 		RTCTLINF.csv ファイルからスケジュール制御情報ファイル（RTCTLINF.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
/* [20250807] Chg St */
int	pvctl_confupdate_file_read( char * filename, struct st_CEMSPINF *ps_cemspinf, struct st_DEMNDINF *ps_demndinf, struct st_RCVPWINF *ps_rcvpwinf, struct st_OVRCHINF *ps_ovrchinf, struct st_PWPLNINF *ps_pwplninf, struct st_BATTEINF *ps_batteinf, struct st_FCRSTINF *ps_fcrstinf, struct st_FCRRPINF *ps_fcrrpinf, int *userNo, int *updateType )	/* [20230830] Chg */
/* [20250807] Chg Ed */
{
	FILE				*fp;						/* ファイルポインタ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	int					retCode = 0;				/* 戻り値 */
	char				readBuff[2048];				/* CSVファイル読み込みバッファ *//* [20230905] Add */
	
	PVCTL_MSG_LOW("START\n");
	
	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_DWL );
	strcat(csvFilePath, filename );

/* [20230905] Add St */
	/* ログ出力 */
	sprintf( errMsg, "update config file read. [%s]", filename );
	commfunc_updateconf_logout(errMsg);
/* [20230905] Add Ed */
	
	while(1)
	{
		/* CSVファイルオープン */
		if( (fp = fopen(csvFilePath, "r")) != NULL)
		{
/* [20230905] Add St */
			/* ログ出力用 */
			while( fgets( readBuff, sizeof(readBuff), fp) != NULL )
			{
				/* ログ出力 */
				readBuff[strlen(readBuff)-1] = 0;
				commfunc_updateconf_logout(readBuff);
			}
/* [20230905] Add Ed */

			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_CONFUPDATE_Check( fp, userNo, updateType, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}

			/* 設定変更種別が制御設定以外の場合は、以降の読み込みは不要 */
/* [20230125] Chg St */
			if( ( *updateType != PV_UPDATECONF_TYPE_PARA ) &&
			    ( *updateType != PV_UPDATECONF_TYPE_PLAN ))
/* [20230125] Chg Ed */
			{
				break;
			}

			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_CEMSPINF_Check( fp, PV_CSVTYPE_UPD, ps_cemspinf, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}

			///* ファイルの先頭へ移動 */
			//fseek(fp, 0, SEEK_SET);
			//retCode = func_DEMNDINF_Check( fp, PV_CSVTYPE_UPD, ps_demndinf, csvFilePath, ps_g_PVSYSINF, ps_g_TMDATINF );
			//if( retCode < 0 )
			//{
			//	break;
			//}

			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_RCVPWINF_Check( fp, PV_CSVTYPE_UPD, ps_rcvpwinf, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}

			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_OVRCHINF_Check( fp, PV_CSVTYPE_UPD, ps_ovrchinf, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}

			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_PWPLNINF_Check( fp, PV_CSVTYPE_UPD, ps_pwplninf, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}
/* [20230830] Add St */
			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_BATTEINF_Check( fp, PV_CSVTYPE_UPD, ps_batteinf, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}
/* [20230830] Add Ed */
/* [20250807] Add St */
			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_FCRSTINF_Check( fp, PV_CSVTYPE_UPD, ps_fcrstinf, csvFilePath );
			if( retCode < 0 )
			{
				break;
			}
			/* ファイルの先頭へ移動 */
			fseek(fp, 0, SEEK_SET);
			retCode = func_FCRRPINF_Check( fp, PV_CSVTYPE_UPD, ps_fcrrpinf, csvFilePath, ps_g_PVSYSINF, ps_g_TMDATINF );
			if( retCode < 0 )
			{
				break;
			}
/* [20250807] Add Ed */
			retCode = 0;
			/* CSVファイルをクローズ */
			fclose( fp );
		}
		else
		{
			sprintf( errMsg, "File[%s] Open error", filename );
			commfunc_syslog_msgset( PV_OWN_NAME, errMsg );
			retCode = -1;
			PVCTL_MSG_ERR("%s\n", errMsg);
		}
		break;
	}

	/* 終了 */
	return( retCode );	
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
int	pvctl_cmn_abnormal_exit( int procNo )
{
	/* プロセス異常中フラグを設定 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_SET );
	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );
	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	return( 0 );
}



int	pvctl_cmn_sysDataTm_Set( struct st_sysData *ps_sysData, int tmNumb, int tmFlg, int tmVal )
{
	ps_sysData->tmFlgData[tmNumb -1] = tmFlg;
	ps_sysData->tmValData[tmNumb -1] = tmVal;

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * UNIXタイム返却
 *
 * 機能：
 * 		年月日からUNIXタイムを返却する
 * 戻り値：
 * 		UNIXタイム
 */
time_t func_cmn_mktime( int year, int month, int day, int hour, int min, int sec)
{
    time_t r_time;
    struct tm mk_time;

    memset(&mk_time, 0, sizeof(mk_time));

    mk_time.tm_year = year - 1900;  /* 西暦年 - 1900 */
    mk_time.tm_mon = month - 1;     /* 月 ( 1月＝0 ) */
    mk_time.tm_mday = day;      	/* 日 */
    mk_time.tm_hour = hour;     	/* 時 */
    mk_time.tm_min = min;       	/* 分 */
    mk_time.tm_sec = sec;      		/* 秒 */
    mk_time.tm_isdst = -1;    		/* サマータイムフラグ */

    r_time = mktime(&mk_time);

    return r_time;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶ分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列をカンマで区切り、ps_sepStr に格納する。
 * 戻り値：
 * 		カンマで区切られたデー多数を返す。
 */
int pvctl_cmn_sepcsv_data( char *pc_data, struct st_sepStr *ps_sepStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_sepStr, 0, sizeof(struct st_sepStr) );

	pt = pc_data;
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		if( *pt == ',' )
		{
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
 * プロセス動作時刻設定処理
 *
 * 機能：
 * 		プロセス動作時刻を設定する。
 * 戻り値：
 * 		正常終了
 */
int pvctl_cmn_set_process_tim( void )
{
	
	/* 積算秒を取得 */
	gettimeofday( &s_nowTime, NULL );
	/* 時刻情報に変換 */
	localtime_r( &s_nowTime.tv_sec, &s_timeData );

	s_timeData.tm_year += 1900;
	s_timeData.tm_mon+= 1;

	PVCTL_MSG_MID("now[%d%02d%02d %02d:%02d %02d]\n",
			s_timeData.tm_year,s_timeData.tm_mon,s_timeData.tm_mday,
			s_timeData.tm_hour, s_timeData.tm_min, s_timeData.tm_sec);

	return(0);
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
	if( p_nowTime != NULL )
	{
		/* 積算秒の引数がNULL以外の場合、設定する */
		*p_nowTime = s_nowTime;
	}
	if( p_timData != NULL )
	{
		/* 時刻情報の引数がNULL以外の場合、設定する */
		*p_timData = s_timeData;
	}

	return(0);
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * 蓄電池 出力変化量制御設定
 *
 * 機能：
 *		引数 trgtPow を出力制御の目標値(w)に向けて制御を行う設定をする
 *		
 * 戻り値：
 * 		 0：正常処理
 *		-1：異常処理
 */
/* [20230301] Chg St */
int func_bttPowSet( int *p_setPow, int trgtPow )
{
	struct timeval			nowtime;					/* 現在時刻 */

/* [20250807] Add St */
	int						svStFcrCtl;
/* [20250807] Add Ed */
	
/* [20250807] Chg St */
	/* 一次調整力制御を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CTL, &svStFcrCtl, ps_g_SVSTSFIL);

	if( svStFcrCtl == PVCTL_FLG_OFF )
	{
		/* 一次調整力制御中以外*/
		/* 現在の充放電電力指示値が目標電力より大きい場合 */
		if( *p_setPow > trgtPow )
		{
			/* 出力変化量を減算すると目標値を下回る場合 */
			if( ( *p_setPow - ps_g_CEMSPINF->btChangeAmtChg ) < trgtPow )
			{
				/* 目標値に到達 */
				*p_setPow = trgtPow;
			}
			else
			{
				/* 出力変化量を減算する */
				*p_setPow -= ps_g_CEMSPINF->btChangeAmtChg;
			}
		}
		/* 現在の充放電電力指示値が目標電力より小さい場合 */
		else if( *p_setPow < trgtPow )
		{
			/* 変化量を加算すると目標値以上となる場合 */
			if( ( *p_setPow + ps_g_CEMSPINF->btChangeAmtDischg ) >= trgtPow )
			{
				/* 目標値に到達 */
				*p_setPow = trgtPow;
			}
			else
			{
				/* 出力変化量を加算 */
				*p_setPow += ps_g_CEMSPINF->btChangeAmtDischg;
			}
		}
	}
	else
	{
#ifdef DEBUG_FCR_FREQ_CTL
//		printf( "%s func_bttPowSet *p_setPow[%d] trgtPow[%d]\n", PV_OWN_NAME, *p_setPow, trgtPow );
#endif /* DEBUG_FCR_FREQ_CTL */
		/* 一次調整力制御中*/
		/* 現在の充放電電力指示値が目標電力より大きい場合 */
		if( *p_setPow > trgtPow )
		{
			/* 出力変化量を減算すると目標値を下回る場合 */
			if( ( *p_setPow - ps_g_FCRSTINF->chgPowerChgAmount ) < trgtPow )
			{
				/* 目標値に到達 */
				*p_setPow = trgtPow;
			}
			else
			{
				/* 出力変化量を減算する */
				*p_setPow -= ps_g_FCRSTINF->chgPowerChgAmount;
			}
		}
		/* 現在の充放電電力指示値が目標電力より小さい場合 */
		else if( *p_setPow < trgtPow )
		{
			/* 変化量を加算すると目標値以上となる場合 */
			if( ( *p_setPow + ps_g_FCRSTINF->dischgPowerChgAmount ) >= trgtPow )
			{
				/* 目標値に到達 */
				*p_setPow = trgtPow;
			}
			else
			{
				/* 出力変化量を加算 */
				*p_setPow += ps_g_FCRSTINF->dischgPowerChgAmount;
			}
		}
	}
/* [20250807] Chg Ed */

#ifdef DEBUG_BT_VAL
	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowtime, NULL);
	sprintf( errMsg, "func_bttPowSet : chgPow:%d, dischgPow:%d, targetPow:%d, setPow:%d", ps_g_CEMSPINF->btChangeAmtChg, ps_g_CEMSPINF->btChangeAmtDischg, trgtPow, *p_setPow );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * パワコン 出力変化量制御設定
 *
 * 機能：
 *		引数 trgtPow を出力制御の目標値(%)に向けて制御を行う設定をする
 *		
 * 戻り値：
 * 		 0：正常処理
 *		-1：異常処理
 */
int func_pvPowSet( int *p_setPow, int trgtPow )
{
	struct timeval			nowtime;					/* 現在時刻 */
	int						setPow;
	int						svStat1;

	/* PV発電停止を取得 */
	commfunc_sv_stat_get( PV_MKIND_PVSYS, 1, PV_SYS_SV_PV_ZERO_PER, &svStat1, ps_g_SVSTSFIL);

	setPow = *p_setPow;

	/* PV発電禁止がON場合、且つ、 */
	/* 設定値より目標値が小さい場合 */
	if( (( svStat1 == 1 ) ||
		 ( ps_g_EMCTLFIL->rcvpChgStopFlg == PVCTL_FLG_ON )) &&
	     ( setPow >= trgtPow) )
	{
		/* 出力変化量を減算すると目標値を下回る場合 */
		if( trgtPow <= ( setPow - ps_g_CEMSPINF->pvChangeAmtDown ) )
		{
			/* 出力変化量を減算する */
			setPow -= ps_g_CEMSPINF->pvChangeAmtDown;
			
		}
		else
		{
			/* 目標値に到達 */
			setPow = trgtPow;
		}
	}
	else
	{
		/* PV発電電力が正常の場合、且つ、目標値が現在発電電力より小さい場合 */
		if( ( s_sysDataAv.tmFlgData[PV_SYS_TM_PV_POW-1] == PV_TMFLG_NORMAL ) &&
			( trgtPow < s_sysDataAv.tmValData[PV_SYS_TM_PV_POW-1] ))
		{
			/* 発電を下げる場合は、即時設定にする */
			setPow = trgtPow;
		}
		else
		{
			/* 欠測の場合は、前回値を設定する */
			if( s_sysDataAv.tmFlgData[PV_SYS_TM_PV_POW-1] != PV_TMFLG_NORMAL )
			{
				;
			}
			else
			{
				/* 変化量を加算すると目標値以上となる場合 */
				if( ( s_sysDataAv.tmValData[PV_SYS_TM_PV_POW-1] + ps_g_CEMSPINF->pvChangeAmtUp ) >= trgtPow )
				{
					/* 目標値に到達 */
					setPow = trgtPow;
				}
				else
				{
					/* 現在の発電電力に出力変化量を加算 */
					setPow = s_sysDataAv.tmValData[PV_SYS_TM_PV_POW-1] + ps_g_CEMSPINF->pvChangeAmtUp;
				}
			}
		}
	}

#ifdef DEBUG_PER_VAL
	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowtime, NULL);
	sprintf( errMsg, "func_pvPowSet : nowPow:%d, targetPer:%d, setPer:%d", *p_setPow, trgtPow, setPow );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

	*p_setPow = setPow;

	return 0;
}
/* [20230301] Chg Ed */

int func_demand( void )
{
	struct tm			timeData;				/* 時刻情報 */
	struct timeval		nowTime;				/* 現在時刻 */

	/* 周期時刻を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);


	/* デマンド時限の切替時刻(毎0分、毎30分)、且つ、開始時刻が過去の場合 */
	if( ( ((char)timeData.tm_min % PVCTL_DMD_TIMED ) == 0 ) &&
		( ( (long long unsigned int)ps_g_DEMNDFIL->startTime.tv_sec / PVCTL_DMD_TIMED_SEC ) < ( nowTime.tv_sec / PVCTL_DMD_TIMED_SEC ) ) )
	{
		/* デマンド時限終了処理 */
		func_dmd_moni_end();

		/* デマンド開始時刻を更新 */
		ps_g_DEMNDFIL->startTime = nowTime;

		/* デマンド時限開始処理 */
		func_dmd_start();
	}
	/* デマンド時限の切替時刻(毎0分、毎30分)以外の場合 */
	else
	{
		/* デマンド監視処理 */
		func_dmd_moni();
	}

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * デマンド時限開始処理
 *
 * 機能：
 * 		デマンド時限の開始処理を行う。
 * 戻り値：
 * 		 0：正常処理
 */
int	func_dmd_start( void )
{
	int						tmFlg;						/* 計測ステータス */
	int						tmVal;						/* 計測値 */

	PVCTL_MSG_LOW("START\n");

	/* 更新情報の実装フラグがONの場合 */
	if( ps_g_DEMNDFIL->s_updatedmd.statFlag == PVCTL_FLG_ON )
	{
		/* デマンド情報を更新する */
		memcpy( ps_g_DEMNDINF, &ps_g_DEMNDFIL->s_updatedmd, sizeof( struct st_DEMNDINF ));
		/* 更新後は、更新情報を初期化する */
		memset( &ps_g_DEMNDFIL->s_updatedmd, 0, sizeof( struct st_DEMNDINF ));
	}

#ifdef DMD_DEBUG_DATA_SET
	ps_g_TMSTSFIL[ps_g_DEMNDINF->tmIfLink].tmFlag = PV_TMFLG_NORMAL;
	ps_g_TMSTSFIL[ps_g_DEMNDINF->tmIfLink].tmVal = debugDataVal;
#else
#endif //DMD_DEBUG_DATA_SET

	/* デマンド監視情報が未実装、または、デマンド制御が無効の場合 */
	if( ( ps_g_DEMNDINF->statFlag == PVCTL_FLG_OFF ) ||
		( ps_g_CEMSPINF->dmdControl == PVCTL_FLG_OFF ))
	{
		ps_g_DEMNDFIL->moniFlg   = PVCTL_FLG_OFF;			/* デマンド監視設定を「無し」に設定 */
		ps_g_DEMNDFIL->tmAbnFlag = PVCTL_FLG_OFF;			/* 計測異常検出を「無し」に設定 */
		tmFlg = PV_TMFLG_DROPTM;							/* 計測ステータスを欠測に設定 */
		tmVal = 0;
	}
	else
	{
		/* 監視対象の計測情報を取得する */
		tmFlg = ps_g_TMSTSFIL[ps_g_DEMNDINF->tmIfLink].tmFlag;
		tmVal = ps_g_TMSTSFIL[ps_g_DEMNDINF->tmIfLink].tmVal;

		/* 監視対象計測値の計測ステータスを判定 */
		if( tmFlg != PV_TMFLG_NORMAL )
		{
			ps_g_DEMNDFIL->moniFlg   = PVCTL_FLG_OFF;			/* デマンド監視設定を「無し」に設定 */
			ps_g_DEMNDFIL->tmAbnFlag = PVCTL_FLG_ON;			/* 計測異常検出を「有り」に設定 */

			/* デマンド時限計測値を格納 */
			ps_g_DEMNDFIL->tmValPow0 = 0;
			PVCTL_MSG_HIG("tmValPow0=%d\n", ps_g_DEMNDFIL->tmValPow0);
		}
		else
		{
			ps_g_DEMNDFIL->moniFlg   = PVCTL_FLG_ON;			/* デマンド監視設定を「有り」に設定 */
			ps_g_DEMNDFIL->tmAbnFlag = PVCTL_FLG_OFF;			/* 計測異常検出を設定 */

			/* デマンド時限計測値を格納 */
			ps_g_DEMNDFIL->tmValPow0 = tmVal;
			PVCTL_MSG_HIG("tmValPow0=%d\n", ps_g_DEMNDFIL->tmValPow0);

/* [20210603] Add St */
			/* デマンド時限計測値(前回値)を格納 */
			ps_g_DEMNDFIL->tmValPowLast = ps_g_DEMNDFIL->tmValPow0;
			ps_g_DEMNDFIL->tmValOffset = 0;
/* [20210603] Add Ed */
		}

	}
	/* デマンド計測情報を初期化 */
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_CUR_POW, tmFlg, 0 );
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_EST_POW, tmFlg, 0 );
	pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_EST_POW_MAX, tmFlg, 0 );


	/* デマンド開始状態の状変を設定 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_MONI, ps_g_DEMNDFIL->moniFlg, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

	PVCTL_MSG_LOW("END\n");

	return(0);
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * デマンド閾値超過判定
 *
 * 機能：
 * 		デマンド閾値の超過判定を行い、超過している場合はデマンド制御を行う。
 * 戻り値：
 * 		 0：正常処理
 */
int	func_dmd_moni_check( void )
{
	int						dmdLv;						/* ループカウンタ */
	int						loopCnt2;					/* ループカウンタ */
	int						checkTmVal;					/* 対象電力値 */
	int						convThresholdVal;			/* 閾値 */

	PVCTL_MSG_LOW("START\n");
	/* 現在デマンド電力 */
	/* 閾値情報分ループする */
	for( dmdLv = 0; dmdLv < PV_MAX_DMD_THAD_LV; dmdLv++ )
	{
		/* 未実装の場合は、対象外 */
		if( ps_g_DEMNDINF->s_thresholdInf[dmdLv].statFlag == PVCTL_FLG_OFF )
		{
			continue;
		}

		/* 既に閾値超過している場合は対象外 */
		if( ps_g_DEMNDFIL->curLvOverFlg[dmdLv] == PVCTL_FLG_ON )
		{
			PVCTL_MSG_MID("dmdLv[%d] allready Current threshold Over\n", dmdLv+1)
			continue;
		}

		/* 現在デマンド電力を取得 */
		checkTmVal = s_sysDataAv.tmValData[PV_SYS_TM_DMD_CUR_POW-1];

		/* 監視対象電力が閾値を超えている場合 */
		if( checkTmVal >= ps_g_DEMNDINF->s_thresholdInf[dmdLv].thresholdVal)
		{
			/* 閾値超過フラグをONに設定し、閾値超過状態を更新 */
			ps_g_DEMNDFIL->curLvOverFlg[dmdLv] = PVCTL_FLG_ON;
			/* デマンド開始状態の状変を設定 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_CUR_LV1+dmdLv, PVCTL_FLG_ON, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
	}

	/* 予測デマンド電力 */
	/* 閾値情報分ループする */
	for( dmdLv = 0; dmdLv < PV_MAX_DMD_THAD_LV; dmdLv++ )
	{
		/* 未実装の場合は、対象外 */
		if( ps_g_DEMNDINF->s_thresholdInf[dmdLv].statFlag == PVCTL_FLG_OFF )
		{
			continue;
		}

		/* 既に閾値超過している場合は対象外 */
		if( ps_g_DEMNDFIL->estLvOverFlg[dmdLv] == PVCTL_FLG_ON )
		{
			PVCTL_MSG_MID("dmdLv[%d] allready estimation threshold Over\n", dmdLv+1)
			continue;
		}

		/* 予測デマンド電力を取得 */
		checkTmVal = s_sysDataAv.tmValData[PV_SYS_TM_DMD_EST_POW-1];

		/* 監視対象電力が閾値を超えている場合 */
		if( checkTmVal >= ps_g_DEMNDINF->s_thresholdInf[dmdLv].thresholdVal)
		{
			/* 閾値超過フラグをONに設定し、閾値超過状態を更新 */
			ps_g_DEMNDFIL->estLvOverFlg[dmdLv] = PVCTL_FLG_ON;
			/* デマンド開始状態の状変を設定 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_EST_LV1+dmdLv, PVCTL_FLG_ON, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
	}

	PVCTL_MSG_LOW("END\n");

	return(0);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * デマンド時限終了処理
 *
 * 機能：
 * 		DEMNDFIL.csv ファイルからデマンド制御情報ファイル（DEMNDFIL.dat）を生成する。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int	func_dmd_moni_end( void )
{
	int						dmdLv;					/* ループカウンタ */

	PVCTL_MSG_LOW("START\n");


	/* 現在デマンド電力 */
	/* 閾値情報分ループする */
	for( dmdLv = 0; dmdLv <= PV_MAX_DMD_THAD_LV; dmdLv++ )
	{
		/* 未実装の場合は、対象外 */
		if( ps_g_DEMNDINF->s_thresholdInf[dmdLv].statFlag == PVCTL_FLG_OFF )
		{
			continue;
		}

		/* 閾値超過していない場合は処理しない */
		if( ps_g_DEMNDFIL->curLvOverFlg[dmdLv] == PVCTL_FLG_OFF )
		{
			PVCTL_MSG_MID("dmdLv[%d] Current threshold Not Over\n", dmdLv+1);
			continue;
		}

		/* 閾値超過フラグをOFFに設定し、閾値超過状態を更新 */
		ps_g_DEMNDFIL->curLvOverFlg[dmdLv] = PVCTL_FLG_OFF;
		/* デマンド開始状態の状変を設定 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_CUR_LV1+dmdLv, PVCTL_FLG_OFF, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	/* 予測デマンド電力 */
	/* 閾値情報分ループする */
	for( dmdLv = 0; dmdLv <= PV_MAX_DMD_THAD_LV; dmdLv++ )
	{
		/* 未実装の場合は、対象外 */
		if( ps_g_DEMNDINF->s_thresholdInf[dmdLv].statFlag == PVCTL_FLG_OFF )
		{
			continue;
		}

		/* 閾値超過していない場合は処理しない */
		if( ps_g_DEMNDFIL->estLvOverFlg[dmdLv] == PVCTL_FLG_OFF )
		{
			PVCTL_MSG_MID("dmdLv[%d] estimation threshold not Over\n", dmdLv+1);
			continue;
		}

		/* 閾値超過フラグをOFFに設定し、閾値超過状態を更新 */
		ps_g_DEMNDFIL->estLvOverFlg[dmdLv] = PVCTL_FLG_OFF;
		/* デマンド開始状態の状変を設定 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_EST_LV1+dmdLv, PVCTL_FLG_OFF, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	}

	PVCTL_MSG_LOW("END\n");

	return(0);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * デマンド監視対象計測情報取得
 *
 * 機能：
 * 		デマンド監視対象の計測ステータス、計測値を取得する。
 * 戻り値：
 * 		 0：正常処理
 */
int	func_dmd_moni( void )
{
	long long 				tmpCurPow;					/* 現在デマンド電力演算 */
	long long				tmpEstPow;					/* 予測デマンド電力演算 */
	int						tmCurPow;					/* 現在デマンド電力 */
	int						tmClcPow;					/* 予測デマンド電力 */
	struct tm				timeData;					/* 時刻情報 */
	struct timeval			nowTime;					/* 現在時刻 */
	int						timePtSec;					/* デマンド時限からの時刻(秒)  */
	int						tmClcMax;					/* 最大予測デマンド電力 */
	int						workVal;					/* 演算用 */
	int						tmFlg;						/* 計測ステータス */
	int						tmVal;						/* 計測値 */
	long long 				workLVal;

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);

	/* 読み込みフラグがONの場合 */
	if( ps_g_DEMNDINF->statFlag == PVCTL_FLG_ON )
	{
		/* 計測ステータス、計測値を取得する */
		tmFlg = ps_g_TMSTSFIL[ps_g_DEMNDINF->tmIfLink].tmFlag;
		tmVal = ps_g_TMSTSFIL[ps_g_DEMNDINF->tmIfLink].tmVal;
	}
	else
	{
		tmFlg = PV_TMFLG_DROPTM;		/* 計測ステータスを「欠測」に設定 */
		tmVal = 0;						/* 計測値を０に設定 */
	}

	PVCTL_MSG_HIG("statFlag=%d, [tmFlg,tmVal]=[%02x,%d]\n", ps_g_DEMNDINF->statFlag, tmFlg, tmVal);

	/* デマンド制御がOFFになった場合、即時監視設定をOFFにする */
	if( ps_g_CEMSPINF->dmdControl == PVCTL_FLG_OFF )
	{
		ps_g_DEMNDFIL->moniFlg = PVCTL_FLG_OFF;
	}

	/* デマンド監視設定がONの場合 */
	if( ps_g_DEMNDFIL->moniFlg == PVCTL_FLG_ON )
	{
		/* 監視対象計測ステータスが正常の場合 */
		if( tmFlg == PV_TMFLG_NORMAL )
		{
/* [20210603] Add St */
			/* 計測値が前回の値より小さい場合 */
			if( tmVal < ps_g_DEMNDFIL->tmValPowLast )
			{
				/* カウンタが1周回った or リセットしたとみなし、前回値をオフセットとして加算する */
				ps_g_DEMNDFIL->tmValOffset += ps_g_DEMNDFIL->tmValPowLast;
			}
			ps_g_DEMNDFIL->tmValPowLast = tmVal;
/* [20210603] Add St */

/* [20210603] Chg St */
			/* 現在デマンド電力算出 */
			workLVal  = (long long)((( tmVal + ps_g_DEMNDFIL->tmValOffset ) - ps_g_DEMNDFIL->tmValPow0 ) * 2);	
/* [20210603] Chg Ed */
			workLVal *= ps_g_DEMNDINF->choseiConv;		/* 変換値の反映 */
			workLVal += ps_g_DEMNDINF->choseiBase;		/* ベース値の反映 */
			workLVal *= ps_g_DEMNDINF->choseiUnit;		/* 単位調整値の反映 */
			workLVal *= ps_g_DEMNDINF->choseiExp;		/* 乗率調整値の反映 */
			tmpCurPow = (int)workLVal;

			/* デマンド開始時限からの経過秒数(100ms換算) */
			timePtSec = (nowTime.tv_sec % PVCTL_DMD_TIMED_SEC)*10 + (nowTime.tv_usec/100000);

			/* 予測デマンド電力算出 */
			tmpEstPow = tmpCurPow * 1800 * 10 / timePtSec;

			/* デマンド演算の単位、桁に合わせて変換 */
			workLVal = (long long)tmpCurPow;
			workLVal += ((ps_g_DEMNDINF->dmdpowExp * ps_g_DEMNDINF->dmdpowdUnit) -1);	/* 切り上げを行う */
			workLVal /= ps_g_DEMNDINF->dmdpowExp;		/* デマンド演算結果に対して乗率を調整する */
			workLVal /= ps_g_DEMNDINF->dmdpowdUnit;	/* デマンド演算結果に対して単位を反映する */
			workLVal -= 0;								/* デマンド演算結果に対してベース値(固定値0)を反映する */
			workLVal /= 1;								/* デマンド演算結果に対して変換値(固定値1)を反映する */
			tmCurPow = (int)workLVal;

			workLVal = (long long)tmpEstPow;
			workLVal += ((ps_g_DEMNDINF->dmdpowExp * ps_g_DEMNDINF->dmdpowdUnit) -1);	/* 切り上げを行う */
			workLVal /= ps_g_DEMNDINF->dmdpowExp;		/* デマンド演算結果に対して乗率を調整する */
			workLVal /= ps_g_DEMNDINF->dmdpowdUnit;	/* デマンド演算結果に対して単位を反映する */
			workLVal -= 0;								/* デマンド演算結果に対してベース値(固定値0)を反映する */
			workLVal /= 1;								/* デマンド演算結果に対して変換値(固定値1)を反映する */
			tmClcPow = (int)workLVal;

			PVCTL_MSG_LOW("tmVal=%d, tmValPow0=%d, timePtSec=%d\n", tmVal, ps_g_DEMNDFIL->tmValPow0, timePtSec);
			PVCTL_MSG_LOW("tmpCurPow=%lld, tmpEstPow=%lld\n", tmpCurPow, tmpEstPow);
			PVCTL_MSG_LOW("tmCurPow=%d, tmClcPow=%d\n", tmCurPow, tmClcPow);


			/* 計測情報を更新 */
			pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_CUR_POW, tmFlg, tmCurPow );
			pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_EST_POW, tmFlg, tmClcPow );


			PVCTL_MSG_MID("tmClcPow[%d], tmClcMax[%d]\n", tmClcPow, tmClcMax);

			/* 計測異常検出を「無し」に設定 */
			ps_g_DEMNDFIL->tmAbnFlag = PVCTL_FLG_OFF;
			/* デマンド開始状態の状変を設定 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_MONI, PVCTL_FLG_ON, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

			/* 監視開始時刻を超えた場合 */
			if( (timeData.tm_min % PVCTL_DMD_TIMED) >= ps_g_DEMNDINF->startUpTime )
			{
				/* 過去の最大予測デマンド電力を超えた */
				if( tmClcPow > s_sysDataAv.tmValData[PV_SYS_TM_DMD_EST_POW_MAX-1] )
				{
					/* 最大予測デマンド電力値を更新 */
					pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_EST_POW_MAX, tmFlg, tmClcPow );
				}

				/* 閾値超過チェック処理 */
				func_dmd_moni_check();
			}

		}
		else
		{
			/* 現在デマンド電力、予測デマンド電力を欠測に更新 */
			pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_CUR_POW, PV_TMFLG_DROPTM, 0 );
			pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_DMD_EST_POW, PV_TMFLG_DROPTM, 0 );
			/* 計測異常検出を「有り」に設定 */
			ps_g_DEMNDFIL->tmAbnFlag = PVCTL_FLG_ON;
			/* デマンド開始状態の状変を設定 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_DMD_MONI, PVCTL_FLG_OFF, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

		}
	}

	return(0);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
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


/* [20250206] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * -----------------------------------------------------------------------------
 * 文字列小文字変換
 *
 * 機能：
 *		文字列を小文字に変換する
 */
void func_tolower( char *pcStr )
{
	int				loopCnt;

	/* 文字数分ループ */
	for( loopCnt = 0 ; loopCnt < strlen(pcStr) ; loopCnt++ )
	{
		/* 1文字ずつ小文字に変換 */
		/* tolower:変換に成功すれば変換後の小文字を返し、変換に失敗すれば変換前の文字を返す */
		pcStr[loopCnt] = tolower(pcStr[loopCnt]);
	}

	return;
}
/* [20250206] Add Ed */


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IOT 受信スレッド
 *
 * 機能：
 *		制御指示命令のトピックをsubscribeする
 *		
 */
void *emctl_iot_rcv_thread( void *args )
{
	int							aws_init=-1;
	IoT_Error_t 				rc = FAILURE;
	IoT_Publish_Message_Params 	paramsQOS;
	struct tm					timeData;					/* 時刻情報 */
	struct timeval				nowTime;					/* 現在時刻(積算秒) */
	char 						cPayload[512];

	/* スレッド停止フラグがＯＦＦの間は処理を続ける */
	while( threadStop == 0 )
	{
		/* スレッドの動作周期を1秒に変更 */
		commfunc_mswait2(1000);

		if( aws_init != 0 )
		{
			/* AWS 初期化処理 */
			aws_init = emctl_aws_rcv_init();
			if( aws_init == 0 )
			{
				/* 装置状態状変：IOT通信異常(受信)：復帰 */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
			}
			else
			{
				/* 装置状態状変：IOT通信異常(受信)：発生 */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
				continue;
			}
		}

		if( aws_init == 0 )
		{
			rc = aws_iot_mqtt_yield(&s_rcv_client, 500);

			/* 切断された場合 */
			if( ( NETWORK_ATTEMPTING_RECONNECT == rc) ||
			    ( NETWORK_MANUALLY_DISCONNECTED == rc ) )
			{
				/* 装置状態状変：IOT通信異常(受信)：発生 */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
				continue;
			}
			else if ( rc >= SUCCESS)
			{
				/* 正常の場合 */
				if( rc == SUCCESS )
				{
					/* 装置状態状変：IOT通信異常(受信)：復帰 */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
				}
			}
			else
			{

				PVCTL_MSG_ERR("emctl_iot_rcv_thread ; aws_iot_mqtt_yield is Error(%d)\n", rc);
				/* 装置状態状変：IOT通信異常(受信)：発生 */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

				/* 再接続以外の以上の場合は、一度終了する */
				emctl_aws_rcv_quit();
				aws_init=-1;
				continue;
			}

			/* 通知情報を送信 */
			aws_init = emctl_cmn_publishNotify();
		}

	}

	/* AWS 終了処理 */
	if( aws_init == 0 )
	{
		emctl_aws_rcv_quit();
	}

#ifdef PV_DEBUG
	printf("%s : Thread END\n", pv_own_name);
#endif

	return( NULL );
}
/* [20190621] Add_Ed */


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
int emctl_aws_rcv_init( void )
{
	char			rootCA[PATH_MAX + 1];
	char			clientCRT[PATH_MAX + 1];
	char			clientKey[PATH_MAX + 1];
	char			rcv_clientid[PATH_MAX + 1];

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
	mqttInitParams.disconnectHandler = emctl_disconnect_handler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&s_rcv_client, &mqttInitParams);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_init returned error : %d ", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		return -1;
	}

	connectParams.keepAliveIntervalInSec = 60;		/* [20250110] Chg */
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	sprintf(rcv_clientid, "%s-rcv", ps_g_IOTCNINF->deviceId);
	connectParams.pClientID = rcv_clientid;
	connectParams.clientIDLen = (uint16_t) strlen(rcv_clientid);
	connectParams.isWillMsgPresent = false;

	rc = aws_iot_mqtt_connect(&s_rcv_client, &connectParams);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_connect : Error(%d)", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		rc = aws_iot_mqtt_free( &s_rcv_client );
		PVCTL_MSG_ERR("%s\n", errMsg);
		return -1;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&s_rcv_client, true);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_autoreconnect_set_status : Unable to set Auto Reconnect to true - %d", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		rc = aws_iot_mqtt_disconnect( &s_rcv_client );
		rc = aws_iot_mqtt_free( &s_rcv_client );
		PVCTL_MSG_ERR("%s\n", errMsg);
		return -1;
	}

	sprintf(s_rcvTopicName, "%s", ps_g_IOTCNINF->rcvTopicName);
	rc = aws_iot_mqtt_subscribe(&s_rcv_client, s_rcvTopicName, strlen(s_rcvTopicName), QOS0, emctl_subscribe_callback_handler_rcv, NULL);
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_subscribe : topic[%s] Error subscribing : %d", s_rcvTopicName, rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		rc = aws_iot_mqtt_disconnect( &s_rcv_client );
		rc = aws_iot_mqtt_free( &s_rcv_client );
		PVCTL_MSG_ERR("%s\n", errMsg);
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
int emctl_aws_rcv_quit( void )
{
	IoT_Error_t rc = FAILURE;

	commfunc_json_free( &s_rcv_pvJsonparse );

	rc = aws_iot_mqtt_disconnect( &s_rcv_client );
	if( ( SUCCESS != rc) && ( NETWORK_DISCONNECTED_ERROR != rc) ) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_disconnect : Unable to set Auto Reconnect to true - %d", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVCTL_MSG_ERR("%s\n", errMsg);
	}

	rc = aws_iot_mqtt_free( &s_rcv_client );
	if(SUCCESS != rc) {
		sprintf(errMsg, "rcv : aws_iot_mqtt_free : Unable to set Auto Reconnect to true - %d", rc);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVCTL_MSG_ERR("%s\n", errMsg);
		return -1;
	}

	return 0;
}

static void emctl_subscribe_callback_handler_rcv(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen, IoT_Publish_Message_Params *params, void *pData)
{
	int			retCode;
	char		errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	char		logFile[128];				/* AWS受信ログファイル名 */
	char		logOldFile[128];			/* AWS受信ログファイル名 */
	FILE		*fp;						/* AWS受信ログファイルポインタ */
	time_t		nowTime;					/* 現在時刻 */
	struct tm	dayTime;					/* 日付情報 */
	int			fileSize;					/* ファイルサイズ */
	char		*p_payload;					/* ペイロードポインタ */

	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	//printf("Subscribe callback\n");
	//printf("%.*s\t%.*s\n", topicNameLen, topicName, (int) params->payloadLen, (char *) params->payload);

	/* 終端を設定 */
	p_payload = (char *)params->payload;
	p_payload[params->payloadLen] = 0;

	/* 受信ログ出力 */
	commfunc_json_logout(PV_FNAME_AWSRCV_LOG, (char *)params->payload);

	/* 受信データ解析 */
	retCode = commfunc_json_parse(&s_rcv_pvJsonparse, 0, (int)params->payloadLen, (char *) params->payload, errmsg );

	/* 処理結果 */
	if( retCode != PV_JSON_RET_OK )
	{
		commfunc_json_free( &s_rcv_pvJsonparse );
		return;
	}

	/* 受信データ取得 */
	retCode = emctl_subscribe( &s_rcv_pvJsonparse, retCode, errmsg );


	commfunc_json_free( &s_rcv_pvJsonparse );

	return;
}

void emctl_disconnect_handler(AWS_IoT_Client *pClient, void *pData)
{
/* [20250305] Chg St */
	char	errmsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

	sprintf(errmsg, "disconnect_handler Snd clientState=[%d] isPingOutstanding=[%d] isAutoReconnectEnabled=[%d]",
		pClient->clientStatus.clientState, pClient->clientStatus.isPingOutstanding, pClient->clientStatus.isAutoReconnectEnabled);
	commfunc_syslog_msgset( pv_own_name, errmsg );

/* [20250305] Chg Ed */
	return;
}

int emctl_subscribe_order_info_der( struct st_pvJsonParse *ps_jsonParse, char *pc_errMsg, struct st_ORDERPARA  *ps_orderpara)
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
		commfunc_syslog_msgset( pv_own_name, pc_errMsg );
		retCode = PV_JSON_ERR_FORMAT;
	}
	else
	{
		/* デバイスIDが不一致の場合 */
		if( strcmp( ps_g_IOTCNINF->deviceId, jsonParseRsult.value ) != 0 )
		{
			sprintf(pc_errMsg, "[%s] does not match (%s!=%s).", tag, ps_g_IOTCNINF->deviceId, jsonParseRsult.value);
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
			retCode = PV_JSON_ERR_FORMAT;
		}
		else
		{
/* [20240701] Chg St */
			/* 商品区分のチェック */
			if( ( strcmp( jsonParseRsult.value, AWS_CATEGORY_REPLACEMENT_RESERVE_FOR_FIT ) == 0 ) ||
				( strcmp( jsonParseRsult.value, AWS_CATEGORY_REPLACEMENT_RESERVE ) == 0 ))
			{
				/* 需給調整市場(三次調整力①②) */
				ps_orderpara->category = AWS_CATEGORY_RR;
			}
			else if( strcmp( jsonParseRsult.value, AWS_CATEGORY_CAPACITYMARKET ) == 0 )
			{
				/* 容量市場 */
				ps_orderpara->category = AWS_CATEGORY_CM;
			}
			//else if(( strcmp( jsonParseRsult.value, AWS_CATEGORY_FREQUENCY_CONTAINMENT ) == 0 ) ||
			//		( strcmp( jsonParseRsult.value, AWS_CATEGORY_SYNC_FREQUENCY_RESTORATION ) == 0 ))
			//{
			//	/* 需給調整市場(二次調整力①②) */
			//	ps_orderpara->category = AWS_CATEGORY_FRR;
			//}
/* [20250807] Chg St */
			else if( strcmp( jsonParseRsult.value, AWS_CATEGORY_FREQUENCY_RESTORATION ) == 0 ) 
			{
				/* 需給調整市場(一次調整力) */
				ps_orderpara->category = AWS_CATEGORY_FCR;
			}
/* [20250807] Chg Ed */
			else
			{
				sprintf(pc_errMsg, "[%s] is not supported(%s).", tag, jsonParseRsult.value);
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				retCode = PV_JSON_ERR_UNSUPPORTED;
			}
/* [20240701] Chg Ed */
			memcpy( ps_orderpara->categoryStr, jsonParseRsult.value, sizeof( ps_orderpara->categoryStr ));
		}

		/* 正常、且つ、商品区分が容量市場の場合 */
		if( ( retCode == PV_JSON_RET_OK ) &&
			( ps_orderpara->category == AWS_CATEGORY_CM ) )
		{
			/* 実効性テスト取得 */
			sprintf(tag, "cm_exam");
			retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
			if( retCode != PV_JSON_RET_OK )
			{
				sprintf(pc_errMsg, "[%s] is not found.", tag);
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );

				/* 見つからない場合は、実需給指令とする */
				ps_orderpara->cm_exam = PVCTL_FLG_OFF;
				retCode = PV_JSON_RET_OK;
			}
			else
			{
/* [20250206] Add St */
				/* 文字列を小文字に変換 */
				func_tolower(jsonParseRsult.value);
/* [20250206] Add Ed */

				/* 値チェック */
				if( strcmp( jsonParseRsult.value, AWS_STR_TRUE ) == 0 )
				{
					/* 実効性テスト指令 */
					ps_orderpara->cm_exam = PVCTL_FLG_ON;
				}
				else if( strcmp( jsonParseRsult.value, AWS_STR_FALSE ) == 0 )
				{
					/* 実需給指令 */
					ps_orderpara->cm_exam = PVCTL_FLG_OFF;
				}
				else
				{
					sprintf(pc_errMsg, "[%s] is param Error(%s).", tag, jsonParseRsult.value);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
				}
			}
		}

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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				retCode = PV_JSON_ERR_PARAM;
			}
			else
			{
			retCode = func_json_conv_strTotimeval( ps_orderpara->eventTimeStr, &ps_orderpara->eventStTimeSec );

				if( retCode != PV_JSON_RET_OK )
				{
					sprintf(pc_errMsg, "date[%s] is format Error.", ps_orderpara->eventTimeStr);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
/* [20201224] Add St */
				else if( ps_orderpara->s_interval[loopCnt1].duration.tv_sec > (1440 * 60 / ps_orderpara->timeConv) )	/* [20211207] Chg */
				{
					sprintf(pc_errMsg, "[%s] is more than %d(%s).", tag, (1440 * 60 / ps_orderpara->timeConv), jsonParseRsult.value );	/* [20211207] Chg */
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
/* [20201224] Add Ed */

				retCode = commfunc_json_getValue2( ps_jsonParse, (jsonParseRsult.dataPos+1), 0, tag, &jsonParseRsult );
				/* 該当するタグが複数あった場合 */
				if( retCode == PV_JSON_RET_OK )
				{
					sprintf(pc_errMsg, "[%s] is already implemented.", tag );
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}
				/* 三次調整力 以外の場合は、負の数はNG */
				if(( ps_orderpara->orderType != AWS_ORDER_TYPE_AC))
				{
					if( atoi(jsonParseRsult.value) < 0 )
					{
						sprintf(pc_errMsg, "[%s] is less than 0(%s).", tag, jsonParseRsult.value );
						commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					retCode = PV_JSON_ERR_PARAM;
					break;
				}

				/* 三次調整力 以外の場合は、負の数はNG */
				if(( ps_orderpara->orderType != AWS_ORDER_TYPE_AC))
				{
					if( atoi(jsonParseRsult.value) < 0 )
					{
						sprintf(pc_errMsg, "[%s] is less than 0(%s).", tag, jsonParseRsult.value );
						commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
						commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				}
				
				/* valueがない場合 */
				if( false == valueFound )
				{
					sprintf(tag, "interval.interval%d.value", loopCnt1 );
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				}

				/* baseがない場合 */
				if( false == baseFound )
				{
					sprintf(tag, "interval.interval%d.base", loopCnt1 );
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				}

				retCode = PV_JSON_ERR_FORMAT;
				break;
			}

			/* Interval情報は任意なので、どちらも見つからなくても正常として扱う */
			retCode = PV_JSON_RET_OK;
		}
	}


	/* エネルギーマネジメント管理ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
			retCode = PV_JSON_ERR_PARAM;
		}
		else
		{
			/* Intervalの実行時間がない場合 */
			if( ( intervalStart.tv_sec == ps_orderpara->eventStTimeSec.tv_sec ) &&
				( intervalStart.tv_usec == ps_orderpara->eventStTimeSec.tv_usec ) )
			{
				sprintf(pc_errMsg, "all interval duration is 0.");
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
			}

			/* イベント終了時刻を設定 */
			ps_orderpara->eventEdTimeSec = intervalStart;

/* [20240202] Chg Start */
			/* 指示情報を管理情報へコピー */
			emctl_order_info_set( ps_orderpara );
/* [20240202] Chg End */
		}
	}

	/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	return retCode;
}

int emctl_subscribe( struct st_pvJsonParse *ps_jsonParse, int parseRet, char *pc_errMsg )
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
		commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
			commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				retCode = PV_JSON_ERR_FORMAT;
			}
			else
			{
				/* デバイスIDが不一致の場合 */
				if( strcmp( ps_g_IOTCNINF->deviceId, jsonParseRsult.value ) != 0 )
				{
					sprintf(pc_errMsg, "[%s] does not match (%s!=%s).", tag, ps_g_IOTCNINF->deviceId, jsonParseRsult.value);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				retCode = PV_JSON_ERR_FORMAT;
			}
			else
			{
				memcpy( orderpara.orderStr, jsonParseRsult.value, sizeof( orderpara.orderStr ));

				/* 指令種別のチェック */
				if( strcmp( jsonParseRsult.value, AWS_ORDER_TYPE_AC_STR ) == 0 )
				{
					orderpara.orderType = AWS_ORDER_TYPE_AC;
					retCode = emctl_subscribe_order_info_der( ps_jsonParse, pc_errMsg, &orderpara);
				}
				else
				{
					sprintf(pc_errMsg, "[%s] is not supported.", tag);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					retCode = PV_JSON_ERR_UNSUPPORTED;
				}
			}
		}

		/* エネルギーマネジメント管理ファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

		/* メッセージ種別を設定 */
		resInfo.messageKind = AWS_NOTIFY_RES;
		/* リトライ回数を設定 */
		resInfo.retryCnt = ps_g_IOTCNINF->sndRetry;
		/* 応答結果、エラーメッセージを設定 */
		resInfo.resResut = retCode;
		memcpy( resInfo.errMsg, pc_errMsg, sizeof(resInfo.errMsg));
		/* 応答情報登録 */
		emctl_cmn_setNotify( &resInfo );

		/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	}
	else if( ( strcmp( "Response", jsonParseRsult.value ) == 0 ) ||
			 ( strcmp( "Notify", jsonParseRsult.value ) == 0 ) )
	{
/* [20230907] Add St */
		if( s_SubscribeSkip == 1 )
		{
			/* Subscribeによる送信チェックをスキップする */
		}
		else
		{
/* [20230907] Add Ed */
			/* 通知の場合 */
			if( strcmp( "Notify", jsonParseRsult.value ) == 0 )
			{
			 	/* イベント状態を取得 */
				sprintf(tag, "event_status");
				retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
				if( retCode != PV_JSON_RET_OK )
				{
					sprintf(pc_errMsg, "[%s] is not found.", tag);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
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
				commfunc_syslog_msgset( pv_own_name, pc_errMsg );
			}
			else
			{
				/* デバイスIDが不一致の場合 */
				if( strcmp( ps_g_IOTCNINF->deviceId, jsonParseRsult.value ) != 0 )
				{
					sprintf(pc_errMsg, "[%s] is not supported.", tag);
					commfunc_syslog_msgset( pv_own_name, pc_errMsg );
				}
				else
				{
					/* イベントID取得 */
					sprintf(tag, "event_id");
					retCode = commfunc_json_getValue2( ps_jsonParse, 0, 0, tag, &jsonParseRsult );
					if( retCode != PV_JSON_RET_OK )
					{
						sprintf(pc_errMsg, "[%s] is not found.", tag);
						commfunc_syslog_msgset( pv_own_name, pc_errMsg );
					}
					else
					{
						/* メッセージ送信完了 */
						emctl_cmn_publisResCheck( jsonParseRsult.value, messageKind );
					}
				}
			}
/* [20230907] Add St */
		}
/* [20230907] Add Ed */
	}
	else
	{
		sprintf(pc_errMsg, "%s[%s] is not supported.", tag, jsonParseRsult.value);
		commfunc_syslog_msgset( pv_own_name, pc_errMsg );
		retCode = -1;
	}

	return retCode;
}

/* [20240701] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IoT 指令切り替えチェック
 *
 * 機能：
 * 		IoT 指令の切り替えを行う
 * 戻り値：
 * 		-1：制御なし
 * 		 0：制御正常
 */
int emctl_order_update_sub( struct st_ORDERPARA *ps_orderpara, struct st_setdata *ps_setdata, struct st_evtdata *ps_evtdata )
{
	struct timeval			nowTime;
	struct tm				timeData;
	struct st_ORDERPARA		*ps_orderpara_now;
	struct st_ORDERPARA		*ps_orderpara_next;
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
	commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

	/* 現在のモード・指令種別を基準に設定*/
	actMode = ps_setdata->actMode;
	orderType = ps_setdata->orderType;
	
	ps_orderpara_now = &ps_orderpara[AWS_ORDER_NOW];
	ps_orderpara_next = &ps_orderpara[AWS_ORDER_NEXT];
	/* 次回の開始時刻が有効な場合 */
	if( ps_orderpara_next->eventStTimeSec.tv_sec != 0 )
	{

		/* 開始時刻を過ぎている場合、指示情報を更新する */
		if( (ps_orderpara_next->eventStTimeSec.tv_sec < nowTime.tv_sec ) ||
			((ps_orderpara_next->eventStTimeSec.tv_sec == nowTime.tv_sec ) &&
			 (ps_orderpara_next->eventStTimeSec.tv_usec <= nowTime.tv_usec )))
		{
			/* 現在運用中の場合 */
			if( actMode == AWS_MODE_OPE )
			{
				/* 現在運用中のイベントの終了通知を行う */
				resInfo.messageKind = AWS_NOTIFY_NOTIFY_E;
				memcpy( resInfo.eventId, ps_orderpara_now->eventId, sizeof(resInfo.eventId));
				/* 応答情報登録 */
				emctl_cmn_setNotify( &resInfo );
			}

			/* 指示情報を更新 */
			memmove( ps_orderpara_now, ps_orderpara_next, sizeof(struct st_ORDERPARA ) * (AWS_ORDER_STOCK - 1) );/* [20240202] Chg */
			memset( &ps_orderpara[AWS_ORDER_STOCK - 1], 0, sizeof(struct st_ORDERPARA ));/* [20240202] Chg */

			orderType = ps_orderpara_now->orderType;

			sprintf(errMsg, "order update : %04d-%02d-%02d %02d:%02d:%02d.%03d", 
					timeData.tm_year,timeData.tm_mon,timeData.tm_mday,
					timeData.tm_hour, timeData.tm_min, timeData.tm_sec, nowTime.tv_usec/1000);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			ps_evtdata->intervalPos = 0;

			/* 開始時刻と終了時刻が同じ場合は、イベントの開始/終了通知を行わない */
			if( ( ps_orderpara_now->eventStTimeSec.tv_sec == ps_orderpara_now->eventEdTimeSec.tv_sec ) &&
				( ps_orderpara_now->eventStTimeSec.tv_usec == ps_orderpara_now->eventEdTimeSec.tv_usec ))
			{
				actMode = AWS_MODE_IDLE;
			}
			else
			{
				actMode = AWS_MODE_OPE;

				/* 次のイベントの開始通知を行う */
				resInfo.messageKind = AWS_NOTIFY_NOTIFY_S;
				memcpy( resInfo.eventId, ps_orderpara_now->eventId, sizeof(resInfo.eventId));
				/* 応答情報登録 */
				emctl_cmn_setNotify( &resInfo );
			}
		}
	}

	ps_orderpara_now = &ps_orderpara[AWS_ORDER_NOW];
	/* 現在の終了時刻が有効な場合 */
	if( ps_orderpara_now->eventEdTimeSec.tv_sec != 0 )
	{
		/* 終了時刻を過ぎている場合、指示情報を更新する */
		if( (ps_orderpara_now->eventEdTimeSec.tv_sec < nowTime.tv_sec ) ||
			((ps_orderpara_now->eventEdTimeSec.tv_sec == nowTime.tv_sec ) &&
			 (ps_orderpara_now->eventEdTimeSec.tv_usec <= nowTime.tv_usec )))
		{
			/* イベントIDの終了通知を行う */
			if( actMode == AWS_MODE_OPE )
			{
				/* イベントの終了通知を行う */
				resInfo.messageKind = AWS_NOTIFY_NOTIFY_E;
				memcpy( resInfo.eventId, ps_orderpara_now->eventId, sizeof(resInfo.eventId));
				/* 応答情報登録 */
				emctl_cmn_setNotify( &resInfo );
			}

			actMode = AWS_MODE_IDLE;

			/* 運用中の場合のみログ出力 */
			if( ps_setdata->actMode == AWS_MODE_OPE )
			{
				sprintf(errMsg, "order end : %04d-%02d-%02d %02d:%02d:%02d.%03d", 
						timeData.tm_year,timeData.tm_mon,timeData.tm_mday,
						timeData.tm_hour, timeData.tm_min, timeData.tm_sec, nowTime.tv_usec/1000);
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
		}
	}

	/* 動作モード・指令種別を更新する */
	ps_setdata->actMode = actMode;
	ps_setdata->orderType = orderType;
	
	/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

	return 0;
}
/* [20240701] Add Ed */


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IoT 指令切り替えチェック
 *
 * 機能：
 * 		IoT 指令の切り替えを行う
 * 戻り値：
 * 		-1：制御なし
 * 		 0：制御正常
 */
int emctl_order_update( void )
{
/* [20240701] Chg St */
	/********************/
	/* 各指令の更新する */
	/********************/

	/* 需給調整市場(三次調整力①②) */
	emctl_order_update_sub(ps_g_EMCTLFIL->s_rr_para, &ps_g_EMCTLFIL->s_setdata_rr, &ps_g_EMCTLFIL->s_evtdata_rr);

	/* 需給調整市場(二次調整力①②) */
	emctl_order_update_sub(ps_g_EMCTLFIL->s_frr_para, &ps_g_EMCTLFIL->s_setdata_frr, &ps_g_EMCTLFIL->s_evtdata_frr);

	/* 需給調整市場((一次調整力) */
	emctl_order_update_sub(ps_g_EMCTLFIL->s_fcr_para, &ps_g_EMCTLFIL->s_setdata_fcr, &ps_g_EMCTLFIL->s_evtdata_fcr);
	
	/* 容量市場(実需給) */
	emctl_order_update_sub(ps_g_EMCTLFIL->s_cm_para, &ps_g_EMCTLFIL->s_setdata_cm, &ps_g_EMCTLFIL->s_evtdata_cm);

	/* 容量市場(実効性テスト) */
	emctl_order_update_sub(ps_g_EMCTLFIL->s_cm_exam_para, &ps_g_EMCTLFIL->s_setdata_cm_exam, &ps_g_EMCTLFIL->s_evtdata_cm_exam);

	return 0;
/* [20240701] Chg Ed */
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
int emctl_order_check( void )
{
/* [20240701] Chg St */
	/********************/
	/* 各指令の更新する */
	/********************/

	/* 需給調整市場(三次調整力①②) */
	emctl_order_check_sub(ps_g_EMCTLFIL->s_rr_para, &ps_g_EMCTLFIL->s_setdata_rr, &ps_g_EMCTLFIL->s_evtdata_rr);

	/* 需給調整市場(二次調整力①②) */
	emctl_order_check_sub(ps_g_EMCTLFIL->s_frr_para, &ps_g_EMCTLFIL->s_setdata_frr, &ps_g_EMCTLFIL->s_evtdata_frr);

	/* 需給調整市場((一次調整力) */
	emctl_order_check_sub(ps_g_EMCTLFIL->s_fcr_para, &ps_g_EMCTLFIL->s_setdata_fcr, &ps_g_EMCTLFIL->s_evtdata_fcr);
	
	/* 容量市場(実需給) */
	emctl_order_check_sub(ps_g_EMCTLFIL->s_cm_para, &ps_g_EMCTLFIL->s_setdata_cm, &ps_g_EMCTLFIL->s_evtdata_cm);

	/* 容量市場(実効性テスト) */
	emctl_order_check_sub(ps_g_EMCTLFIL->s_cm_exam_para, &ps_g_EMCTLFIL->s_setdata_cm_exam, &ps_g_EMCTLFIL->s_evtdata_cm_exam);


	return 0;
/* [20240701] Chg Ed */
}

/* [20240701] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IoT 制御チェック(sub)
 *
 * 機能：
 * 		IoT 制御のチェックと実行を行う
 * 戻り値：
 * 		-1：制御なし
 * 		 0：制御正常
 */
int emctl_order_check_sub( struct st_ORDERPARA *ps_orderpara, struct st_setdata *ps_setdata, struct st_evtdata *ps_evtdata )
{
	int						loopCnt1;					/* ループカウンタ */
	struct timeval			nowTime;
	struct tm				timeData;
	int						retCode = 0;
	int						targetPow = 0;
	int						targetBase = 0;
	int						valueConv = 0;
	int						tmFlag = PV_TMFLG_DROPTM;
	struct st_ORDERPARA		*ps_orderpara_now;

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);

	if( ps_setdata->actMode == AWS_MODE_OPE )
	{
		ps_orderpara_now = &ps_orderpara[AWS_ORDER_NOW];

		/* 期間数分ループ */
		for( loopCnt1 = ps_evtdata->intervalPos; loopCnt1 < AWS_INTERVAL_MAX; loopCnt1++ )
		{
			/* 実装済みチェック */
			if( ps_orderpara_now->s_interval[loopCnt1].statFlag == 1 )
			{
				/* 現在時刻が期間内の場合 */
				if( ( (ps_orderpara_now->s_interval[loopCnt1].startSec.tv_sec < nowTime.tv_sec ) ||
					  ((ps_orderpara_now->s_interval[loopCnt1].startSec.tv_sec == nowTime.tv_sec ) &&
					   (ps_orderpara_now->s_interval[loopCnt1].startSec.tv_usec <= nowTime.tv_usec ))) &&
					( (nowTime.tv_sec < ps_orderpara_now->s_interval[loopCnt1].endSec.tv_sec) ||
					  ((nowTime.tv_sec == ps_orderpara_now->s_interval[loopCnt1].endSec.tv_sec ) &&
					   (nowTime.tv_usec <= ps_orderpara_now->s_interval[loopCnt1].endSec.tv_usec ))) )
				{
					/* 切り替わり時はログを出力 */
					if( ps_orderpara_now->s_interval[loopCnt1].setFlag == 0 )
					{
						sprintf(errMsg, "IoT info[%d] : %04d-%02d-%02d %02d:%02d:%02d.%03d -> base[%d], value[%d].", 
								loopCnt1,
								timeData.tm_year,timeData.tm_mon,timeData.tm_mday,
								timeData.tm_hour, timeData.tm_min, timeData.tm_sec, nowTime.tv_usec/1000,
								ps_orderpara_now->s_interval[loopCnt1].base,
								ps_orderpara_now->s_interval[loopCnt1].value);
						commfunc_syslog_msgset( pv_own_name, errMsg );
					}
					
					/* 実行済みに設定 */
					ps_orderpara_now->s_interval[loopCnt1].setFlag = 1;
					ps_evtdata->intervalPos = loopCnt1;

					targetPow = ps_orderpara_now->s_interval[loopCnt1].value;
					targetBase= ps_orderpara_now->s_interval[loopCnt1].base;
					valueConv = ps_orderpara_now->valueConv;
					break;
				}
			}
		}
	}

	/* 制御値(kw)を設定 */
	ps_evtdata->targetPow = targetPow * valueConv;
	ps_evtdata->targetBase = targetBase * valueConv;

	PVCTL_MSG_LOW("END\n");

	return retCode;
}
/* [20240701] Add Ed */

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
int	emctl_cmn_setNotify( struct st_resinfo *ps_resInfo )
{
	struct timeval			nowTime;
	int						retCode = 0;				/* 戻り値 */
	struct st_resinfo		*ps_SetInfo;
	int						loopCnt1;
	int						tempCnt = AWS_NOTIFY_STOCK;
	int						tempRes;
#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "emctl_cmn_setNotify : START, eventId=%s, messageKind=%d", ps_resInfo->eventId, ps_resInfo->messageKind );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

	/* フェールセーフ（基本的にはありえない） */
	for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
	{
		ps_SetInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]];

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
					ps_g_EMCTLFIL->resOrder[loopCnt1],
					ps_SetInfo->eventId,
					ps_SetInfo->eventStTimeSec.tv_sec,/* [20240207] Add */
					ps_SetInfo->messageKind,
					ps_SetInfo->errMsg);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			break;
		}
	}

	/* 未設定のキューがない場合、未設定の応答情報を検索する */
	if( loopCnt1 == AWS_NOTIFY_STOCK )
	{
		for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
		{
			ps_SetInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]];

			/* 送信情報が未設定の場合 */
			if( ps_SetInfo->sndstatus == AWS_NOTIFY_ST_NON )
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "emctl_cmn_setNotify : Set [%d] resId[%d]", loopCnt1+1, ps_SetInfo->resId );
				commfunc_syslog_msgset( pv_own_name, errMsg );
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
				sprintf(errMsg, "emctl_cmn_setNotify : NotWait [%d] resId[%d]", loopCnt1+1, ps_SetInfo->resId );
				commfunc_syslog_msgset( pv_own_name, errMsg );
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
				ps_SetInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[0]];
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "emctl_cmn_setNotify : AWS_NOTIFY_ST_NON resInfo is not found.");
				commfunc_syslog_msgset( pv_own_name, errMsg );
				/* SYSログに出力 */
				sprintf(errMsg, "emctl_cmn_setNotify : delete [1] resId[%d] eventId[%s], eventTimeSec[%d], messageKind[%d], Msg[%s]", /* [20240207] Chg */
						ps_SetInfo->resId,
						ps_SetInfo->eventId,
						ps_SetInfo->eventStTimeSec.tv_sec,/* [20240207] Add */
						ps_SetInfo->messageKind,
						ps_SetInfo->errMsg);
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
			}
		}
	}

	/* 応答情報に追加する */
	if( tempCnt == AWS_NOTIFY_STOCK )
	{
		/* 一番古い応答情報に上書きする */
		tempRes = ps_g_EMCTLFIL->resOrder[0];
		for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK-1; loopCnt1++ )
		{
			ps_g_EMCTLFIL->resOrder[loopCnt1] = ps_g_EMCTLFIL->resOrder[loopCnt1+1];
		}
		ps_g_EMCTLFIL->resOrder[loopCnt1] = tempRes;
	}
	else
	{
		/* 設定可能な応答情報を更新し、送信待ちの最後に設定する */
		tempRes = ps_g_EMCTLFIL->resOrder[tempCnt];
		for( loopCnt1 = tempCnt-1; loopCnt1 >= 0; loopCnt1-- )
		{
			if( ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]].sndstatus == AWS_NOTIFY_ST_WAIT )
			{
				break;
			}
			ps_g_EMCTLFIL->resOrder[loopCnt1+1] = ps_g_EMCTLFIL->resOrder[loopCnt1];
			tempCnt--;
		}
		ps_g_EMCTLFIL->resOrder[tempCnt] = tempRes;
	}
	ps_SetInfo = &ps_g_EMCTLFIL->s_resinfo[tempRes];

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

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "emctl_cmn_setNotify : END");
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

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
int	emctl_cmn_publisResCheck( char *event_id, int messageKind )
{
	int						retCode=-1;
	struct st_resinfo		*ps_SetInfo;
	struct st_resinfo		*ps_preInfo;
	int						hitCnt=0;
	int						loopCnt1;
	int						tempCnt;

	/* エネルギーマネジメント管理ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "emctl_cmn_publisResCheck : START, eventId=%s, messageKind=%d", event_id, messageKind );
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

	/* 設定する応答情報を検索する */
	for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
	{
		ps_SetInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]];

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
				sprintf(errMsg, "emctl_cmn_publisResCheck : [%d] resId[%d] -> complete", loopCnt1+1, ps_SetInfo->resId);
				commfunc_syslog_msgset( pv_own_name, errMsg );
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
		for( tempCnt = ps_g_EMCTLFIL->resOrder[loopCnt1]; loopCnt1 < AWS_NOTIFY_STOCK-1; loopCnt1++ )
		{
			ps_g_EMCTLFIL->resOrder[loopCnt1] = ps_g_EMCTLFIL->resOrder[loopCnt1+1];
		}
		ps_g_EMCTLFIL->resOrder[loopCnt1] = tempCnt;
	}

	/* 一致する情報が見つからなかった場合 */
	if( hitCnt == 0 )
	{
		sprintf(errMsg, "Notify info is not found eventId[%s], messageKind[%d]", event_id, messageKind );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		
	}

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "emctl_cmn_publisResCheck : END");
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
	/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

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
int	emctl_cmn_publishNotify( void )
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
/* [20230907] Chg St */
	int							messageKind;					/* メッセージ種別(応答/開始/終了) */
	char						eventId[PV_JSON_DATALEN_MAX];	/* イベントID */

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "emctl_cmn_publishNotify : START");
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

	for( loopCnt1 = 0; loopCnt1 < AWS_NOTIFY_STOCK; loopCnt1++ )
	{
		/* 積算秒を取得 */
		gettimeofday( &nowTime, NULL );
		/* 時刻情報に変換 */
		localtime_r( &nowTime.tv_sec, &timeData );

		/* エネルギーマネジメント管理ファイルのミューテックスをロック */
		commfunc_mutex_lock( PV_FILENO_EMCTLFIL );

		ps_SetInfo = &ps_g_EMCTLFIL->s_resinfo[ps_g_EMCTLFIL->resOrder[loopCnt1]];

#ifdef PV_DEBUG_QUEUE
		sprintf(errMsg, "emctl_cmn_publishNotify : [%d] resId[%d] sndstate[%d]", loopCnt1+1, ps_SetInfo->resId, ps_SetInfo->sndstatus );
		commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
		/* 送信情報が送信待ちの場合 */
		if( ps_SetInfo->sndstatus == AWS_NOTIFY_ST_WAIT )
		{
			/* リトライ回数があり、且つ、送信回数がリトライ回数に到達した場合  */
			if( ( ps_SetInfo->retryCnt != 0 ) &&
				( ps_SetInfo->resSndCnt >= ps_SetInfo->retryCnt ) )
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "emctl_cmn_publishNotify : [%d] resId[%d] -> failde", loopCnt1+1, ps_SetInfo->resId);
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif
				/* 送信状態を更新しする */
				ps_SetInfo->sndstatus = AWS_NOTIFY_ST_FAIL;

				/* 応答送信順序を更新する（最後に移動） */
				tempCnt = ps_g_EMCTLFIL->resOrder[loopCnt1];
				for( loopCnt2 = loopCnt1; loopCnt2 < AWS_NOTIFY_STOCK-1; loopCnt2++ )
				{
					ps_g_EMCTLFIL->resOrder[loopCnt2] = ps_g_EMCTLFIL->resOrder[loopCnt2+1];
				}
				ps_g_EMCTLFIL->resOrder[loopCnt2] = tempCnt;
				/* 応答情報送信順序を前にずらしたため、次の情報(同じ番号)からやり直し */
				loopCnt1--;
				/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
				commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );
				continue;
			}
			else
			{
#ifdef PV_DEBUG_QUEUE
				sprintf(errMsg, "emctl_cmn_publishNotify : [%d] resId[%d] publish, eventId[%s], messageKind[%d]", loopCnt1+1, ps_SetInfo->resId, ps_SetInfo->eventId, ps_SetInfo->messageKind );
				commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

				/* 送信回数をインクリメントする */
				ps_SetInfo->resSndCnt++;

				/* メッセージ種別毎に分ける */
				switch( ps_SetInfo->messageKind )
				{
					case AWS_NOTIFY_RES :
						/* 送信データ生成 */
						sprintf(cPayload, "{\n\"message_type\":\"Response\",\n\"event_id\": \"%s\",\n\"device_id\": \"%s\",\n\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\",\n\"response_code\":%d,\n\"response_message\":\"%s\"\n}", 
								ps_SetInfo->eventId, ps_g_IOTCNINF->deviceId,
								timeData.tm_year+1900,timeData.tm_mon+1,timeData.tm_mday,timeData.tm_hour, timeData.tm_min, timeData.tm_sec,(nowTime.tv_usec/1000),ps_SetInfo->resResut, ps_SetInfo->errMsg);
						break;
					case AWS_NOTIFY_NOTIFY_S :
						localtime_r( &ps_SetInfo->eventTimeSec.tv_sec, &evTimeData );

						/* 送信データ生成 */
/* [20230711] Chg St */
						sprintf(cPayload, "{\n\"message_type\":\"Notify\",\n\"event_id\": \"%s\",\n\"device_id\": \"%s\",\n\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\",\n\"date\":\"%04d-%02d-%02d %02d:%02d:%02d.%d00\",\n\"event_status\":\"start\"\n}", 
								ps_SetInfo->eventId, ps_g_IOTCNINF->deviceId,
								timeData.tm_year+1900,timeData.tm_mon+1,timeData.tm_mday,timeData.tm_hour, timeData.tm_min, timeData.tm_sec,(nowTime.tv_usec/1000),
								evTimeData.tm_year+1900,evTimeData.tm_mon+1,evTimeData.tm_mday,evTimeData.tm_hour, evTimeData.tm_min, evTimeData.tm_sec, ps_SetInfo->eventTimeSec.tv_usec/100000);
/* [20230711] Chg Ed */
						break;
					case AWS_NOTIFY_NOTIFY_E :
						localtime_r( &ps_SetInfo->eventTimeSec.tv_sec, &evTimeData );

						/* 送信データ生成 */
						sprintf(cPayload, "{\n\"message_type\":\"Notify\",\n\"event_id\": \"%s\",\n\"device_id\": \"%s\",\n\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d.%03d\",\n\"date\":\"%04d-%02d-%02d %02d:%02d:%02d.%d00\",\n\"event_status\":\"stop\"\n}", 
								ps_SetInfo->eventId, ps_g_IOTCNINF->deviceId,
								timeData.tm_year+1900,timeData.tm_mon+1,timeData.tm_mday,timeData.tm_hour, timeData.tm_min, timeData.tm_sec,(nowTime.tv_usec/1000),
								evTimeData.tm_year+1900,evTimeData.tm_mon+1,evTimeData.tm_mday,evTimeData.tm_hour, evTimeData.tm_min, evTimeData.tm_sec, ps_SetInfo->eventTimeSec.tv_usec/100000);
						break;
				}
				messageKind = ps_SetInfo->messageKind;		/* メッセージ種別保持*/
				sprintf(eventId, ps_SetInfo->eventId);		/* イベントID保持 */
				
				/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
				commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );

				paramsQOS.qos = QOS0;
				paramsQOS.payload = (void *) cPayload;
				paramsQOS.payloadLen = strlen(cPayload);
				paramsQOS.isRetained = 0;
				rc = aws_iot_mqtt_publish(&s_rcv_client, s_rcvTopicName, strlen(s_rcvTopicName), &paramsQOS);

				if(SUCCESS != rc)
				{
					sprintf(errMsg, "emctl_iot_rcv_thread : aws_iot_mqtt_publish is Error(%d). eventId=%s, resSndCnt=%d", rc, eventId, ps_SetInfo->resSndCnt);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					PVCTL_MSG_ERR("%s\n", errMsg);
					/* 装置状態状変：IOT通信異常(受信)：発生 */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

					if( rc < SUCCESS)
					{
						/* aws再起動 */
						emctl_aws_rcv_quit();
						retCode=-1;
						break;
					}
				}
				else
				{
					PVCTL_MSG_HIG("aws_iot_mqtt_publish is SUCCESS (messageKind,event_id)=(%d,%s)", messageKind, eventId);

					/* 装置状態状変：IOT通信異常(受信)：復帰 */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_IOTRCVERR, 0, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

/* [20230907] Add St */
					/* Subscribeによる送信チェックをスキップする */
					if( s_SubscribeSkip == 1 )
					{
						/* 送信完了チェック */
						emctl_cmn_publisResCheck( eventId, messageKind );
						/* 応答情報送信順序を前にずらしたため、次の情報(同じ番号)からやり直し *//* [20240208] Add */
						loopCnt1--;
					}
/* [20230907] Add Ed */
				}


				/* 送信データがAWSライブラリの受信バッファを超える場合 */
				if( paramsQOS.payloadLen > AWS_IOT_MQTT_RX_BUF_LEN )
				{
					sprintf(errMsg, "aws_iot_mqtt_publish data size over (%d > %d).", paramsQOS.payloadLen, AWS_IOT_MQTT_RX_BUF_LEN);
					commfunc_syslog_msgset( pv_own_name, errMsg );

					/* 装置状態状変：設定異常：発生 */
					commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, 1, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
				}
			}
		}
		else
		{
			/* エネルギーマネジメント管理ファイルのミューテックスをアンロック */
			commfunc_mutex_unlock( PV_FILENO_EMCTLFIL );
		}
	}

#ifdef PV_DEBUG_QUEUE
	sprintf(errMsg, "emctl_cmn_publishNotify : END");
	commfunc_syslog_msgset( pv_own_name, errMsg );
#endif

/* [20230907] Chg Ed */

	return retCode;
}
/* [20230529] Add Ed */

/* [20240701] Chg St */
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
void emctl_order_info_set( struct st_ORDERPARA *ps_addOrderpara )
{
	char						errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */
	bool						stIsMutch = false;			/* 開始時刻一致 */
	bool						idIsMutch = false;			/* イベントID一致 */
	bool						writePtCnf = false;			/* 書込み位置確定 */
	int							wpCnt;						/* 書込み位置ループカウンタ */
	int							cntInterval;				/* インターバルループカウンタ */
	bool						rcvStMin = false;			/* 受信イベント開始時刻＜保持イベント開始時刻 */
	struct timeval				tmpEventStTimeSec;			/* イベント開始時刻(sec)(ログ出力判断に使用」) */
/* [20240701] Add St */
	struct st_ORDERPARA 		*ps_orderpara;				/* イベントパラメータ */

	/* 商品区分が需給調整市場(三次調整力①②)の場合 */
	if( ps_addOrderpara->category == AWS_CATEGORY_RR )
	{
		ps_orderpara = ps_g_EMCTLFIL->s_rr_para;
	}
	/* 商品区分が需給調整市場(二次調整力①②)の場合 */
	else if( ps_addOrderpara->category == AWS_CATEGORY_FRR )
	{
		ps_orderpara = ps_g_EMCTLFIL->s_frr_para;
	}
	/* 商品区分が需給調整市場(一次調整力)の場合 */
	else if( ps_addOrderpara->category == AWS_CATEGORY_FCR )
	{
		ps_orderpara = ps_g_EMCTLFIL->s_fcr_para;
	}
	/* 商品区分が容量市場、且つ、実効性テストの場合 */
	else if( ( ps_addOrderpara->category == AWS_CATEGORY_CM ) &&
			 ( ps_addOrderpara->cm_exam == 1 ))
	{
		ps_orderpara = ps_g_EMCTLFIL->s_cm_exam_para;
	}
	/* 商品区分が容量市場、且つ、実需給の場合 */
	else if( ( ps_addOrderpara->category == AWS_CATEGORY_CM ) &&
			 ( ps_addOrderpara->cm_exam == 0 ))
	{
		ps_orderpara = ps_g_EMCTLFIL->s_cm_para;
	}
/* [20240701] Add Ed */


	/* ログ出力判定用にストック端-2(未来ストック4)の情報を保持 */
	memcpy( &tmpEventStTimeSec, &ps_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec, sizeof( tmpEventStTimeSec ) );
	/* 未来1～5ストックへの格納場所を検索 */
	/* イベントID、イベント開始時刻の一致は、同一指令と判断する */
	/* 受信指令のイベント開始時刻がストックのイベント開始時刻より小さい場合、そのストックに受信指令を差し込む */
	/* 空きストック＞イベント開始時刻が早い＞同一指令の優先順位で判定する */
	for ( wpCnt = AWS_ORDER_NEXT; wpCnt < AWS_ORDER_STOCK; wpCnt++ ) {
		/* フラグ初期化 */
		idIsMutch = false;
		stIsMutch = false;
		/* イベントIDが設定されているか */
		if ( strlen( ps_orderpara[wpCnt].eventId ) == 0 )
		{
			/* 空きストック(イベントIDなし) */
			/* 書込み位置確定 */
			writePtCnf = true;
			break;
		}
		/* イベントIDが一致するか */
		if ( strcmp( ps_orderpara[wpCnt].eventId, ps_addOrderpara->eventId ) == 0 )
		{
			/* イベントID一致 */
			idIsMutch = true;
		}

		/* イベント開始時刻が一致するか */
		if ( ( ps_orderpara[wpCnt].eventStTimeSec.tv_sec == ps_addOrderpara->eventStTimeSec.tv_sec ) &&
			 ( ps_orderpara[wpCnt].eventStTimeSec.tv_usec == ps_addOrderpara->eventStTimeSec.tv_usec ) )
		{
			/* イベント開始時刻が一致の場合 */
			stIsMutch = true;
		}
		/* イベント開始時刻がストックより早いか */
		else if ( ( ps_orderpara[wpCnt].eventStTimeSec.tv_sec > ps_addOrderpara->eventStTimeSec.tv_sec ) ||
				( ( ps_orderpara[wpCnt].eventStTimeSec.tv_sec == ps_addOrderpara->eventStTimeSec.tv_sec ) &&
				  ( ps_orderpara[wpCnt].eventStTimeSec.tv_usec > ps_addOrderpara->eventStTimeSec.tv_usec ) ) )
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
			memmove( &ps_orderpara[wpCnt + 1], &ps_orderpara[wpCnt], sizeof(struct st_ORDERPARA) * ( AWS_ORDER_STOCK - 1 - wpCnt ) );
		}
	}
	else
	{
		/* 同一指令、空きが見つからなかった場合、最後尾の指令を上書き */
		wpCnt = AWS_ORDER_STOCK -1;
	}

	/* 受信指令をストックに保存する */
	memcpy(&ps_orderpara[wpCnt], ps_addOrderpara, sizeof(struct st_ORDERPARA));

	/* ログ出力対象の動作か(未来4に上書き、または、未来4の内容が変わる差し込み) */
	if ( ( wpCnt == ( AWS_ORDER_STOCK - 2 ) ) || ( tmpEventStTimeSec.tv_sec != ps_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec.tv_sec ) ||
		( ( tmpEventStTimeSec.tv_sec == ps_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec.tv_sec ) && ( tmpEventStTimeSec.tv_usec != ps_orderpara[AWS_ORDER_STOCK - 2].eventStTimeSec.tv_usec ) ) )
	{
		/* 最大使用想定の未来ストック4に書込みをした場合、または差し込みで未来ストック4の値が変更された場合、未来ストックの中身をログ出力 */
		/* 応答に乗せない様ローカル変数を使用 */
		sprintf(errMsg, "orderpara nextslot No4 used");
		commfunc_syslog_msgset( pv_own_name, errMsg );
		/* 指令ストックを現在を含め全てログ出力 */
		for ( wpCnt = AWS_ORDER_NOW; wpCnt < AWS_ORDER_STOCK; wpCnt++ )
		{
			/* ストックの出力 */
			sprintf(errMsg, "stock No=%d eventId=%s orderType=%s eventTimeStr=%s", wpCnt, ps_orderpara[wpCnt].eventId,
				ps_orderpara[wpCnt].orderStr, ps_orderpara[wpCnt].eventTimeStr );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			/* インターバルの内容を最大10個出力 */
			for ( cntInterval = 0; cntInterval < 10; cntInterval++ )
			{
				/* インターバルが設定されているか */
				if ( ps_orderpara[wpCnt].s_interval[cntInterval].duration.tv_sec == 0 &&
					 ps_orderpara[wpCnt].s_interval[cntInterval].duration.tv_usec == 0 )
				{
					/* スキップ */
					continue;
				}
				/* インターバルのログ出力 */
				sprintf(errMsg, "interval No=%ld intervaltm=%d base=%d value=%4d", cntInterval,
					(ps_orderpara[wpCnt].s_interval[cntInterval].endSec.tv_sec - ps_orderpara[wpCnt].s_interval[cntInterval].startSec.tv_sec) / 60,
					ps_orderpara[wpCnt].s_interval[cntInterval].base, ps_orderpara[wpCnt].s_interval[cntInterval].value );
				commfunc_syslog_msgset( pv_own_name, errMsg );
			}
		}
	}
}
/* [20240207] Add End */
/* [20240701] Chg Ed */
/* [20250807] Add St */
/* [20260114] Chg St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力レポートデータ蓄積実施判断
 *
 * 機能：
 * 		一維持調整力レポートデータ蓄積実施を判断する
 * 		1.現コマの約定△kWが0以外の場合ON
 * 		2.現コマの約定△kWが0&次コマの約定△kWが0以外&次コマ開始まで蓄積開始時間以下の場合ON
 * 		2.現コマの約定△kWが0&前コマの約定△kWが0以外&前コマ終了から蓄積終了時間以下の場合ON
 * 		3.上記以外OFF
 * 引数：
 * 	  現在時刻(struct timeval)
 * 	  現コマの配列位置
 * 戻り値：
 * 		 1：レポート蓄積ON
 * 		 0：レポート蓄積OFF
 */
int func_fcr_report_acum_judge( struct timeval nowTime, int nowDkw)
{
	struct timeval		tmpTime;							/* 時刻(算出用) */
	struct tm	 		timeData;							/* 日付情報(算出用) */
	int					targetDay;							/* 対象日 */
	int					dmdPos;								/* 制御コマ */
	struct st_PWPLNINF *ps_pwplninf;						/* [18]需給計画値情報ファイル */
	int					loopCnt1;							/* ループカウント */
	int					fcrDltKw;							/* 一次調整力約定△kW */
	int 				result = PVCTL_FLG_OFF;				/* レポートデータ蓄積有無 */
	
	ps_pwplninf = &ps_g_PWPLNFIL->powplndata[PV_POWPLAN_IDX_ONGRID];

	/* 現在コマが一次の場合 */
	if( nowDkw != 0 )
	{
		/* レポートデータ蓄積ON */
		result = PVCTL_FLG_ON;
	}
	else
	{
		/********************/
		/* 前コマのチェック */
		/********************/
		/** 前コマの開始時刻を算出 **/
		/* 現在コマの開始時刻を算出 */
		tmpTime.tv_usec = 0;
		tmpTime.tv_sec = nowTime.tv_sec - (nowTime.tv_sec % 1800);
		/* 前コマの時刻(30分前)を算出 */
		tmpTime.tv_sec -= 1800;

		/* 前コマの日付、デマンドコマを算出 */
		localtime_r( &tmpTime.tv_sec, &timeData );
		/* 対象日を取得 */
		targetDay = ( (timeData.tm_year+1900) * 10000) + ( (timeData.tm_mon+1) * 100 ) + timeData.tm_mday;
		/* デマンドコマを取得 */
		dmdPos = ( timeData.tm_hour * 2 ) + (timeData.tm_min / 30 );

		fcrDltKw = 0;
		/* 格納日数分ループ */
		for( loopCnt1 = 0; loopCnt1 < ps_pwplninf->plnDayNum; loopCnt1 ++ )
		{
			/* 対象のデータの場合 */
			if( ps_pwplninf->s_powDayPln[loopCnt1].plnDay ==  targetDay )
			{
				/* 一次調整力約定△kWを取得 */
				fcrDltKw = ps_pwplninf->s_powDayPln[loopCnt1].fcrDltKw[dmdPos];
				break;
			}
		}
		/* 前コマが一次の場合 */
		if( fcrDltKw != 0 )
		{
			/* 前コマの終了時刻=(現在コマの開始時刻)を算出 */
			tmpTime.tv_sec = nowTime.tv_sec - (nowTime.tv_sec % 1800);

			/* 現在時刻が前コマ終了時刻の経過秒数が、レポート蓄積終了時間未満である場合 */
			if( (nowTime.tv_sec - tmpTime.tv_sec ) < PV_REPORTDATA_ACUM_END_TIME )
			{
				/* レポートデータ蓄積ON */
				result = PVCTL_FLG_ON;
			}
		}

		/********************/
		/* 次コマのチェック */
		/********************/
		/** 次コマの開始時刻を算出 **/
		/* 現在コマの開始時刻を算出 */
		tmpTime.tv_usec = 0;
		tmpTime.tv_sec = nowTime.tv_sec - (nowTime.tv_sec % 1800);
		/* 次コマの時刻(30分後)を算出 */
		tmpTime.tv_sec += 1800;

		/* 次コマの日付、デマンドコマを算出 */
		localtime_r( &tmpTime.tv_sec, &timeData );
		/* 対象日を取得 */
		targetDay = ( (timeData.tm_year+1900) * 10000) + ( (timeData.tm_mon+1) * 100 ) + timeData.tm_mday;
		/* デマンドコマを取得 */
		dmdPos = ( timeData.tm_hour * 2 ) + (timeData.tm_min / 30 );

		fcrDltKw = 0;
		/* 格納日数分ループ */
		for( loopCnt1 = 0; loopCnt1 < ps_pwplninf->plnDayNum; loopCnt1 ++ )
		{
			/* 対象のデータの場合 */
			if( ps_pwplninf->s_powDayPln[loopCnt1].plnDay ==  targetDay )
			{
				/* 一次調整力約定△kWを取得 */
				fcrDltKw = ps_pwplninf->s_powDayPln[loopCnt1].fcrDltKw[dmdPos];
				break;
			}
		}
		/* 次コマが一次の場合 */
		if( fcrDltKw != 0 )
		{
			/* 次コマの開始時刻を算出 */
			tmpTime.tv_sec = nowTime.tv_sec + (1800 - nowTime.tv_sec % 1800);

			/* 現在時刻から次コマ開始時刻の残り秒数が、レポート蓄積開始時間以下である場合 */
			if( (tmpTime.tv_sec - nowTime.tv_sec) <= PV_REPORTDATA_ACUM_START_TIME )
			{
				/* レポートデータ蓄積ON */
				result = PVCTL_FLG_ON;
			}
		}
	}


	return result;
}
/* [20260114] Chg Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 周波数正常範囲チェック
 *
 * 機能：
 * 		周波数が一次調整力パラメータ設定の正常範囲にあるかチェックする
 * 		※正常範囲＝基本周波数(Hz)±異常判定周波数(Hz)
 * 戻り値：
 * 	   true：正常
 * 	  false：異常
 */
bool func_fcrfreqcheck_in_normal( int *freq )
{
	bool result = true;
	long targetFreq;
	long lowerFreq;
	long upperFreq;
	struct timeval			nowTime;					/* 現在時刻 */
	struct tm				timeData;					/* 時刻情報 */

	/* 時刻情報を取得 */
	pvctl_cmn_get_process_tim(&nowTime, &timeData);

	/* 欠測以外の場合 */
	if( s_freqAve.tmAvlFlg != PV_TMFLG_DROPTM )	/* [20260514] Chg */
	{
		targetFreq = s_freqAve.tmAvlVal;	/* [20260514] Chg */
		*freq = targetFreq;
		lowerFreq = ps_g_TMSTSFIL[PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD_LOWER-1].avltmVal;
		upperFreq = ps_g_TMSTSFIL[PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD_UPPER-1].avltmVal;

		/* 異常周波数に到達していない場合 */
/* [20251031] Chg St */
//		if( ( lowerFreq < targetFreq) && 
//			( upperFreq > targetFreq ) )
		if( lowerFreq < targetFreq )
/* [20251031] Chg Ed */
		{
			result = true;
		}
		/* 異常周波数に到達した場合 */
		else
		{
			result = false;
		}

#ifdef DEBUG_FCRCTL_FREQ_ERR
		printf( "func_freqcheck_in_normal targetFreq[%ld] lowerFreq[%ld] upperFreq[%ld] result=%s\n",
			targetFreq, lowerFreq, upperFreq, result == true ? "true":"false");
#endif /* DEBUG_FCRCTL_FREQ_ERR */

	}

	return result;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 一次調整力パラメータ設定TMSV反映
 *
 * 機能：
 * 		一次調整力パラメータ設定をTMに反映する
 */
void fucc_set_fcrctlprm_tmsv( void )/* [20251215] Chg */
{
	int tmFlg = PV_TMFLG_NORMAL;

	if( ps_g_FCRSTINF->readFlg == 1 )
	{
#ifdef DEBUG_FCR_CTL_PRM
		printf("(%s) baseFreq=%d variationRate=%d deadBand=%d freqDevtThrsld=%d freqDevtThrsldLower=%d freqDevtThrsldUpper=%d faultDuration=%d ratedPower=%d dischgPowerChgAmount=%d chgPowerChgAmount=%d deadBandResType=%d chgResponsiveEnabled=%d\n",
			PV_OWN_NAME, ps_g_FCRSTINF->baseFreq, ps_g_FCRSTINF->variationRate, ps_g_FCRSTINF->deadBand, ps_g_FCRSTINF->freqDevtThrsld,
			ps_g_FCRSTINF->freqDevtThrsldLower, ps_g_FCRSTINF->freqDevtThrsldUpper, ps_g_FCRSTINF->faultDuration, ps_g_FCRSTINF->ratedPower,
			ps_g_FCRSTINF->dischgPowerChgAmount, ps_g_FCRSTINF->chgPowerChgAmount, ps_g_FCRSTINF->deadBandResType, ps_g_FCRSTINF->chgResponsiveEnabled );/* [20251215] 充電応動有無の対応 Chg */
#endif /* DEBUG_FCR_CTL_PRM */
		if( ps_g_FCRSTINF->baseFreq == 0 )
		{
			/* 基準周波数がない場合、ファイルなしと見なし欠測とする */
			tmFlg = PV_TMFLG_DROPTM;
		}
		/* TM値への反映 */
		/* 一次調整力パラメータ設定：基準周波数 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_BASE_FREQ, tmFlg, ps_g_FCRSTINF->baseFreq );
		/* 一次調整力パラメータ設定：調定率 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_FREQ_VARIATION_RATE, tmFlg, ps_g_FCRSTINF->variationRate );
		/* 一次調整力パラメータ設定：不感帯 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_DEAD_BAND, tmFlg, ps_g_FCRSTINF->deadBand );
		/* 一次調整力パラメータ設定：不感帯(下限) */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_DEAD_BAND_LOWER, tmFlg, ps_g_FCRSTINF->deadBandLower );
		/* 一次調整力パラメータ設定：不感帯(上限) */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_DEAD_BAND_UPPER, tmFlg, ps_g_FCRSTINF->deadBandUpper );
		/* 一次調整力パラメータ設定：異常判定周波数 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD, tmFlg, ps_g_FCRSTINF->freqDevtThrsld );
		/* 一次調整力パラメータ設定：異常判定周波数(下限) */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD_LOWER, tmFlg, ps_g_FCRSTINF->freqDevtThrsldLower );
		/* 一次調整力パラメータ設定：異常判定周波数(上限) */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD_UPPER, tmFlg, ps_g_FCRSTINF->freqDevtThrsldUpper );
		/* 一次調整力パラメータ設定：異常時出力継続時間 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_ERR_FAULT_DURATION, tmFlg, ps_g_FCRSTINF->faultDuration );
		/* 一次調整力パラメータ設定：定格出力 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_RATED_POWER, tmFlg, ps_g_FCRSTINF->ratedPower );
		/* 一次調整力パラメータ設定：不感帯応動種別 */
		pvctl_cmn_sysDataTm_Set( &s_sysDataAv, PV_SYS_TM_FCRCTL_DEAD_BAND_RES_TYPE, tmFlg, ps_g_FCRSTINF->deadBandResType );
/* [20251215] 充電応動有無の対応 Add St */
		/* 一次調整力パラメータ設定：充電応動有無 */
		commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FCR_CHG_RES_ENABLED, ps_g_FCRSTINF->chgResponsiveEnabled, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
/* [20251215] 充電応動有無の対応 Add Ed */

		/* 設定終わりでフラグ変更 */
		ps_g_FCRSTINF->readFlg = 2;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 模擬周波数操作
 *
 * 機能：
 * 		一次調整力の模擬周波数のSVと模擬周波数データを操作する
 * 		
 * 引数：
 * 	  現在時刻(struct timeval)
 * 戻り値：
 * 		 0：制御無し
 * 		 1～100：模擬周波数配列番号
 * 		-1：模擬周波数が過去(削除対象)
 */
void func_fcr_freq_sim_ctl( struct timeval nowTime )
{
	int					fcrFreqSimJudge				= 	0;	/* 模擬周波数判定 */
	int					svStFreqSimTest				=	0;	/* 模擬周波数 */

	/* 模擬周波数制御判断 */
	fcrFreqSimJudge = func_fcr_freq_sim_judge( nowTime );
	if( fcrFreqSimJudge > 0 )
	{
		ps_g_EMCTLFIL->s_fcr_freq_sim_data.targetNumb = fcrFreqSimJudge;
		svStFreqSimTest = PVCTL_FLG_ON;
	}
	else
	{
		svStFreqSimTest = PVCTL_FLG_OFF;
		/* 模擬周波数終了の場合 */
	 	if( fcrFreqSimJudge == PV_FCR_FREQ_SIM_END )
		{
			/* 模擬周波数データの削除 */
			memset( &ps_g_EMCTLFIL->s_fcr_freq_sim_data, 0, sizeof( ps_g_EMCTLFIL->s_fcr_freq_sim_data ) );
		}
	}
	/* 模擬周波数を更新 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FREQ_SIM_TEST, svStFreqSimTest, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 模擬周波数制御判断
 *
 * 機能：
 * 		一次調整力の模擬周波数制御実施を判断する
 * 		
 * 引数：
 * 	  現在時刻(struct timeval)
 * 戻り値：
 * 		 0：制御無し
 * 		 1～100：模擬周波数配列番号
 * 		-1：模擬周波数が過去(削除対象)
 */
int func_fcr_freq_sim_judge( struct timeval nowTime )
{
	int result = 0;
	int loopCnt1;
	int dataCnt = ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount;
	int fcrDltKw = ps_g_EMCTLFIL->s_fcr_freq_sim_data.fcrDltKw;

	/* 模擬周波数が設定されている場合 */
	if( fcrDltKw != 0 && dataCnt != 0 )
	{
		/* 現在時刻が模擬周波数開始時刻より前(同秒でマイクロ秒が早い) */
		if( ( nowTime.tv_sec < ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_stTime_timeval.tv_sec ) ||
			( ( nowTime.tv_sec == ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_stTime_timeval.tv_sec ) &&
			( nowTime.tv_usec < ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_stTime_timeval.tv_usec ) ) )
		{
			/* 模擬周波数制御時間前 */
			result = 0;
		}
		/*  現在時刻が模擬周波数の最終制御時間の終了を過ぎている */
		else if( ( nowTime.tv_sec > ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[dataCnt - 1].s_stTime_timeval.tv_sec + ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[dataCnt - 1].duration ) ||
			( ( nowTime.tv_sec == ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[dataCnt - 1].s_stTime_timeval.tv_sec + ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[dataCnt - 1].duration ) &&
			( nowTime.tv_usec > ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[dataCnt - 1].s_stTime_timeval.tv_usec ) ) )
		{
			/* 模擬周波数制御時間を終了 */
			result = -1;
		}
		/* 模擬周波数制御時間 */
		else
		{
			/* 制御情報の位置を検索 */
			for( loopCnt1 = 0; loopCnt1 < ps_g_EMCTLFIL->s_fcr_freq_sim_data.simDataCount; loopCnt1++ )
			{
				/* 現在時刻が開始時間以上、かつ開始時間＋継続時間より小さい場合 */
				if( ( nowTime.tv_sec >= ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].s_stTime_timeval.tv_sec ) &&
					( nowTime.tv_sec < ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].s_stTime_timeval.tv_sec +
						ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].duration ) )
				{
					/* 周波数が設定されている */
					if( ps_g_EMCTLFIL->s_fcr_freq_sim_data.s_fcr_freq_sim_list[loopCnt1].freqValue != 0 )
					{
						/* 模擬周波数制御情報位置 */
						result = loopCnt1 + 1;
						break;
					}
				}
			}
		}
	}

	return result;
}

/* [20250807] Add Ed */
/* [20260514] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 外部設定値取得
 *
 * 機能：
 * 		外部ファイルから設定値を取得する
 */
int func_get_extraParam( char *pc_filePath, time_t *pt_time, int *p_cmExamPri, int *p_freqAveCnt )
{
	int				retCode;					/* 戻り値 */
	char			getStr[128];				/* 取得文字列 */

	/* 設定ファイルのチェック */
	retCode = commfunc_file_info_get( pc_filePath, pt_time, EMS_CTLCONF_PARAM3, getStr );
	if( retCode == COM_PARAM_GET_OK )
	{
		/* 容量市場優先度を取得 */
		*p_cmExamPri = atoi(getStr);

		retCode = commfunc_file_info_get( pc_filePath, NULL, EMS_CTLCONF_PARAM4, getStr );
		if( retCode == COM_PARAM_GET_OK )
		{
			/* 周波数平均100msカウントを取得 */
			*p_freqAveCnt = atoi(getStr);
		}
	}

	return retCode;
}
/* [20260514] Add Ed */
