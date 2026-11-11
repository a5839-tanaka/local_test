/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251027] 2025.10.27
 * ・外部機器から取得したスケジュール値を取り込むように修正
 * [20250717] 2025.07.17
 * ・時刻同期サービスを停止する処理を追加
 * [20231120] 2023.11.20
 * ・日付が変わった際に、ID登録結果が未登録の場合、再度取得するように修正
 * [20230905] 2023.09.05
 * ・20230529のファイル名誤り
 * [20230529] 2023.05.29
 * ・ダウンロードしたスケジュールファイルは削除する
 * ・Ph2対応
 * [20190827] 2019.08.27
 * ・SSL通信データをファイルに出力する機能をONにした際のファイルの出力先と
 *   ファイル名を変更
 * [20190611] 2019.06.11
 * ・ＩＤ登録結果が未登録時の場合はスケジュールのダウンロードしないように修正
 * [20190116] 2019.01.16
 * ・スケジュール配信サーバとのSSL通信データをファイルに出力する機能を追加
 * [20181114] 2018.11.14
 * ・SSL_readにて異常となった場合、処理を継続しないように修正
 * ・現在のスケジュール情報のバッファ面の年が現在時刻と一致しない場合、
 *   バッファ面を検索するように修正
 * [20180621] 2018.06.21
 * ・電力サーバへのダウンロードチェックを秒単位に変更
 * [20180604] 2018.06.04
 * ・SSLの通信データをファイルに出力するデバッグ機能を追加
 * [20180601] 2018.06.01
 * ・ＩＤ登録結果は起動時にのみ行い、結果(通信異常含む)に関わらずスケジュールの
 *    ダウンロード処理に移行する。
 * [20180309] 2018.03.09
 * ・ダウンロードしたＩＤ登録結果ファイルをPV_PATH_DWLに保持するように変更
 * [20170329] 2017.03.29
 * ・ダウンロードしたファイルが正常な場合、継続してダウンロードするように変更
 * ・更新スケジュール未取得時に固定スケジュール(月間)を取得するように処理追加
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * スケジュール管理プロセス（pvschmng）
 *
 * 機能：
 *		太陽光モニタリングシステム専用のスケジュール管理プロセス
 * 		以下の機能を提供する
 *		プロセス動作周期（１０秒）に下記処理を行う
 *		  
 *		(1)ＮＴＰサーバとの時刻同期
 *		  発電所ＩＤのチェックサムに対する実施時間帯にスケジュール情報配信システ
 *		  ムと発電所の時刻同期を行う。
 *		  時刻同期がエラーとなった場合は、１分間隔でリトライ（最大５回）を行う。
 *		  さらにエラーとなった場合は、３０分後に再リトライを継続する。
 *		    ┏━━━━━━━┳━━━━━━━━━━━┓
 *		    ┃ チェックサム ┃      実施時間帯      ┃
 *		    ┣━━━━━━━╋━━━━━━━━━━━┫
 *		    ┃		0		┃ 21:00:00 ～ 21:09:59 ┃
 *		    ┃		1		┃ 21:30:00 ～ 21:39:59 ┃
 *		    ┃		2		┃ 22:00:00 ～ 22:09:59 ┃
 *		    ┃		3		┃ 22:30:00 ～ 22:39:59 ┃
 *		    ┃		4		┃ 23:00:00 ～ 23:09:59 ┃
 *		    ┃		5		┃ 23:30:00 ～ 23:39:59 ┃
 *		    ┃		6		┃ 00:00:00 ～ 00:09:59 ┃
 *		    ┃		7		┃ 00:30:00 ～ 00:39:59 ┃
 *		    ┃		8		┃ 01:00:00 ～ 01:09:59 ┃
 *		    ┃		9		┃ 01:30:00 ～ 01:39:59 ┃
 *		    ┗━━━━━━━┻━━━━━━━━━━━┛
 *		  
 *		(2)出力変化時間の取得
 *		  出力変化時間が定義されたＣＳＶファイルが更新されていた場合、再読み込み
 *		  を行う。
 *		  
 *		(3)現在時刻のスケジュールの取得
 *		  現在時刻のスケジュール情報を取得する。
 *		  また、スケジュール情報に従い、以下の状態を更新する。
 *		  ┌──────┬────────┐
 *		  ｜スケジュール｜  収集装置状態  ｜
 *		  ｜    種別    ｜１５｜１６｜１７｜
 *		  ├──────┼──┼──┼──┤
 *		  ｜    固定    ｜ ０ ｜ １ ｜ ０ ｜
 *		  ｜    更新    ｜ ０ ｜ ０ ｜ １ ｜
 *		  ｜    なし    ｜ １ ｜ ０ ｜ ０ ｜
 *		  └──────┴──┴──┴──┘
 *		  
 *		(3)スケジュール情報の取得
 *		  現在時刻が毎正時にスケジュールの取得を行う。
 *		  スケジュールを取得は以下の順に行う。
 *		     ①発電所ＩＤの登録結果の取得
 *		     ②更新スケジュールの取得
 *		     ③固定スケジュール（年間）の取得
 *		     ④更新スケジュールの取得         ┐繰り返し
 *		    (⑤)固定スケジュール（年間）の取得┘⑤は更新フラグが変化した場合のみ
 *		    (⑥)固定スケジュール（月間）の取得
 *		  
 *		  [1]発電所ＩＤの登録結果
 *		     登録結果が「未登録」の場合は、収集装置状態[３２]をＯＮにし、以降の
 *		     スケジュール情報配信サーバへの接続は行わない。
 *		     ※収集装置を再起動した場合は、再度接続を試みる。
 *		     登録結果の取得に失敗した場合は、１分間隔にリトライを継続する。
 *		     
 *		  [2]更新スケジュールの取得
 *		     取得に成功した場合は、ファイル内のチェックサムが正しい場合、スケ
 *		     ジュールデータを反映し、次回アクセス日時を保持する。
 *		     現在時刻が次回アクセス日時を超えた場合、新たに更新スケジュールの取
 *		     得を行う。
 *		     更新スケジュールファイルにて定義されている更新フラグが前回の値と異
 *		     なる場合、固定スケジュール（年間）を取得する。
 *		     スケジュールファイルはファイル内の整合性の結果に関わらず、読み込み
 *		     後にモニタリングサーバへ送信する為、ファイル名に収集装置ＩＤを付与
 *		     し、FTPDATAディレクトリに移動する。
 *		     取得に失敗した場合は、３０分後（初期起動時は５分）にリトライを継続
 *		     する。
 *		     
 *		  [3]固定スケジュール（年間）の取得
 *		     更新フラグに対応した固定スケジュール（年間）を取得する。
 *		     初期起動時は更新スケジュール取得後の毎正分に固定スケジュール取得を
 *		     行う。失敗した場合は１分後にリトライを継続する。
 *		     初期起動以降は発電所ＩＤのチェックサムに対する実施時間帯に取得を行
 *		     う。失敗した場合は、５分後にリトライ（最大５回）を行う。さらにエラ
 *		     ーとなった場合は、翌日の実施時間帯に再度リトライを行う。
 *		     スケジュールの優先度は更新＞固定の為、固定スケジュールを反映する際
 *		     、既に更新スケジュールが設定されていた場合は反映しない。
 *		     ただし、各月に定義されているチェックサムが正しい月のみ反映する。
 *		     スケジュールファイルはファイル内の整合性の結果に関わらず、読み込み
 *		     後にモニタリングサーバへ送信する為、ファイル名に収集装置ＩＤを付与
 *		     し、FTPDATAディレクトリに移動する。
 *		        ┏━━━━━━━┳━━━━━━━━━━━┓
 *		        ┃ チェックサム ┃      実施時間帯      ┃
 *		        ┣━━━━━━━╋━━━━━━━━━━━┫
 *		        ┃		0		┃ 21:10:00 ～ 21:29:59 ┃
 *		        ┃		1		┃ 21:40:00 ～ 21:59:59 ┃
 *		        ┃		2		┃ 22:10:00 ～ 22:29:59 ┃
 *		        ┃		3		┃ 22:40:00 ～ 22:59:59 ┃
 *		        ┃		4		┃ 23:10:00 ～ 23:29:59 ┃
 *		        ┃		5		┃ 23:40:00 ～ 23:59:59 ┃
 *		        ┃		6		┃ 00:10:00 ～ 00:29:59 ┃
 *		        ┃		7		┃ 00:40:00 ～ 00:59:59 ┃
 *		        ┃		8		┃ 01:10:00 ～ 01:29:59 ┃
 *		        ┃		9		┃ 01:40:00 ～ 01:59:59 ┃
 *		        ┗━━━━━━━┻━━━━━━━━━━━┛
 *		     
 *		  [4]固定スケジュール（月間）の取得
 *		     初期起動後、更新スケジュール、固定スケジュール（月間）を１度も取得
 *		     していない場合は、固定スケジュール（月間）を取得する。
 *		     初期起動以降は現在時刻のスケジュールがない場合、又は、月末日の場合
 *		     に、発電所ＩＤのチェックサムに対する実施時間帯に取得を行う。
 *		     失敗した場合は、５分後にリトライ（最大５回）を行う。さらにエラーと
 *		     なった場合は、翌日の実施時間帯に再度リトライを行う。
 *		     スケジュールの優先度は更新＞固定の為、固定スケジュールを反映する際
 *		     、既に更新スケジュールが設定されていた場合は反映しない。
 *		     スケジュールファイルはファイル内の整合性の結果に関わらず、読み込み
 *		     後にモニタリングサーバへ送信する為、ファイル名に収集装置ＩＤを付与
 *		     し、FTPDATAディレクトリに移動する。
 *		     
 *		  [*]通信エラー、エラーファイルによるリトライについて
 *		     スケジュール配信サーバーに接続した際のリトライ要否は以下の通り。
 *		     ・ＳＳＬ接続に失敗した場合は、要リトライ。
 *		     ・ＳＳＬ接続に成功し、ＨＴＴＰステータスコード
 *		       ┌────┬─────────┬────┐
 *		       ｜ステース｜レスポンスの意味  ｜リトライ｜
 *		       ｜ コード ｜                  ｜  要否  ｜
 *		       ├────┼─────────┼────┤
 *		       ｜ １ｘｘ ｜情報              ｜   要   ｜
 *		       ｜ ２ｘｘ ｜成功              ｜  不要  ｜
 *		       ｜ ３ｘｘ ｜リダイレクション  ｜   要   ｜
 *		       ｜ ４ｘｘ ｜クライアントエラー｜  不要  ｜
 *		       ｜ ５ｘｘ ｜サーバエラー      ｜   要   ｜
 *		       ｜上記以外｜上記以外          ｜   要   ｜
 *		       └────┴─────────┴────┘
 *		     
 *		     取得したファイルがエラーファイルだった場合のリトライ要否は以下の通り
 *		       ┌───┬────┐
 *		       ｜エラー｜リトライ｜
 *		       ｜コード｜  要否  ｜
 *		       ├───┼────┤
 *		       ｜E1001 ｜  不要  ｜
 *		       ｜E1002 ｜  不要  ｜
 *		       ｜E1003 ｜  不要  ｜
 *		       ｜E1006 ｜  不要  ｜
 *		       ｜E1007 ｜  不要  ｜
 *		       ｜E1008 ｜  不要  ｜
 *		       ｜E1009 ｜  不要  ｜
 *		       ｜E1010 ｜  不要  ｜
 *		       ｜E0001 ｜   要   ｜
 *		       ｜E0002 ｜   要   ｜
 *		       ｜E0003 ｜   要   ｜
 *		       └───┴────┘
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

#include "embase.h"
#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvcomm.h"
#include "pvdevdata.h"
#include "pvschmng.h"

#define	PV_OWN_NAME		"PVSCHMNG"					/* 自プログラム名 */

#if 0
#define	PV_DEBUG									/* デバッグフラグ：通常処理確認 */
#define	PV_DEBUG_THRED								/* デバッグフラグ：ＳＦＴＰ設定情報表示 */
#define	PV_DEBUG_CDT								/* デバッグフラグ： */
#define PV_DEBUG_TCP
#endif


struct st_PVSYSINF		*ps_g_PVSYSINF;					/* 管理ファイルポインタ */
struct st_TMDATINF		*ps_g_TMDATINF;					/* 計測情報ファイルポインタ */
struct st_SVDATINF		*ps_g_SVDATINF;					/* 装置状態情報ファイルポインタ */
struct st_SVMSKINF		*ps_g_SVMSKINF;					/* 状変マスクファイルポインタ */
struct st_TMSTSFIL		*ps_g_TMSTSFIL;					/* 計測値ファイルポインタ */
struct st_STSTSFIL		*ps_g_STSTSFIL;					/* 通信ステータス状態ファイルポインタ */
struct st_SVSTSFIL		*ps_g_SVSTSFIL;					/* 装置状態ファイルポインタ */
struct st_JOHENFIL		*ps_g_JOHENFIL;					/* 状変蓄積ファイルポインタ */
struct st_PVSYSLOG		*ps_g_PVSYSLOG;					/* システムログファイルポインタ */
struct st_TMSTOFIL		*ps_g_TMSTOFIL;					/* 蓄積ファイルポインタ */
struct st_PVCTLFIL		*ps_g_PVCTLFIL;					/* 制御状態保存ファイルポインタ */
struct st_PGCONINF		*ps_g_PGCONINF;					/* 電力会社向け情報ファイルポインタ */
struct st_SCHCTLFIL		*ps_g_SCHCTLFIL;				/* スケジュール管理情報ファイルポインタ */

													/* 使用ファイルインデックス */
struct st_fileIndex		s_fileIndex[] = 
{
	{	PV_FILENO_PVSYSINF, (void **)&ps_g_PVSYSINF, 0, 0, 0 },
	{	PV_FILENO_TMDATINF, (void **)&ps_g_TMDATINF, 0, 0, 0 },
	{	PV_FILENO_SVDATINF, (void **)&ps_g_SVDATINF, 0, 0, 0 },
	{	PV_FILENO_SVMSKINF, (void **)&ps_g_SVMSKINF, 0, 0, 0 },
	{	PV_FILENO_TMSTSFIL, (void **)&ps_g_TMSTSFIL, 0, 0, 0 },
	{	PV_FILENO_STSTSFIL, (void **)&ps_g_STSTSFIL, 0, 0, 0 },
	{	PV_FILENO_SVSTSFIL, (void **)&ps_g_SVSTSFIL, 0, 0, 0 },
	{	PV_FILENO_JOHENFIL, (void **)&ps_g_JOHENFIL, 0, 0, 0 },
	{	PV_FILENO_PVSYSLOG, (void **)&ps_g_PVSYSLOG, 0, 0, 0 },
	{	PV_FILENO_TMSTOFIL, (void **)&ps_g_TMSTOFIL, 0, 0, 0 },
	{	PV_FILENO_PVCTLFIL, (void **)&ps_g_PVCTLFIL, 0, 0, 0 },
	{	PV_FILENO_PGCONINF, (void **)&ps_g_PGCONINF, 0, 0, 0 },
	{	PV_FILENO_SCHCTLFIL, (void **)&ps_g_SCHCTLFIL, 0, 0, 0 },
	{	0,	                NULL,                    0, 0, 0 }
};

static char					pv_own_name[128];							/* 自プロセス情報 */
static char					errMsg[PV_MSG_SIZE_LOG];					/* エラーメッセージ */
struct tm					s_timeData;									/* 時刻情報 */
time_t						s_Time;										/* 現在積算秒 */
time_t						s_lastTime;									/* 前回積算秒 */
static int					s_schYear;									/* 現在時刻（年） */
static time_t				s_lastUpdate;								/* 最終更新時刻 */
static time_t				s_ntpdateFlg;								/* ntpdate最終実行時刻 */
static time_t				s_fixSchReqFlg;								/* 固定スケジュール要求フラグ */
static time_t				s_fixSchReqTime;							/* 固定スケジュール要求時刻 */
static char					s_fixSchRetryCnt;							/* 固定スケジュールリトライ回数 */
static char					s_sslCertKeyPath[128];						/* SSL 秘密鍵ファイルパス */
static int					schAcsTime[10] = {76200,78000,79800,81600,83400,85200,600,2400,4200,6000};
static int					ntpAcsTime[10] = {75600,77400,79200,81000,82800,84600,0,1800,3600,5400};
static char  				sslBuff[PV_SSL_READ_SIZE];
static char  				*p_sslBuff;
static int   				sslReadSize;
static SSL   				*ssl;
static FILE					*sslfp=NULL;				/* ファイルディスクリプタ */
static char					s_sslDataFlag; 				/* SSL通信データ出力フラグ [20190116] Add */
static char					s_appTimeSyncName[64] = "";				/* 待機モード時の時刻同期アプリサービス名 *//* [20250717] Add */

/* 内部関数定義 */
int	func_init( void );
void	func_csvupdate_check( void );
int func_pvsch_pgconinf_read( void );
void	func_buff_select( void );
int	func_sch_main( void );
int	func_schfile_request( int reqType );
int	func_schfile_download_request( int reqType );
int	func_schfile_read( int reqType );
int func_schfile_read_err( void );
int	func_schfile_read_id( void );
int	func_ssl_request(char *kubun);
int	func_choice_file_month( const struct dirent *dir );
int	func_choice_file_year( const struct dirent *dir );
int	func_choice_file_update( const struct dirent *dir );
int	func_choice_file_err( const struct dirent *dir );
int	func_choice_file_id( const struct dirent *dir );
int func_ssl_connect( char *kbn );
int func_ssl_read( char **p_restBuff );
int func_ssl_fgets( char **buff );
int func_ssl_getStatus( void );
int func_ssl_getResHeader( void );
int func_ssl_getResBody( void );
int func_ssl_filedownload( void );
int func_cmn_get_data_pos( short year, char month, char day, char hour, char min);
int func_cmn_choice_filename( const char *FileName );
int func_cmn_time_check( struct tm *timData );
int	func_cmn_string_check( char *strPoint, char strlength );
int func_cmn_schfile_read( void *func_ptr, int fmtNo );
int func_cmn_FileFormatCheck( const char *FilePath, int type);
int func_cmn_bin2Num(char *strData, int byteNum);
int func_cmn_set_process_tim( void );
int func_cmn_get_process_tim( time_t *p_nowTime, struct tm *p_timData );
int func_cmn_sepcsv_data( char *, struct st_sepStr * );
int	func_ntpdate_start( int syncMode );
int	func_ntpdate_stop( void );
int	func_abnormal_exit( int procNo );
int func_schfile_data_check(struct tm *timeData, int dataNum, unsigned char *ptr_data, int fmtNo );
int func_schfile_data_set(int buffSelect, int dataPos, int dataNum, unsigned char *ptr_data, int fmtNo );
time_t func_cmn_mktime( short year, char month, char day, char hour, char min, char sec);
int func_ssl_data_file_write( char *preFix, char *p_buff, int length );
int func_ssl_data_file_close( void );
int func_cmn_num2byte(int , int , char *);
int func_cmn_str2byte(char *, char *, char );
void func_cmn_set_data_str( struct tm *, char *, char  );
/* [20250717] Add St */
int func_get_config( void );										/* 設定ファイル読込処理 */
int func_sep_data( char *, char, int, struct st_sepStr * );	/* データセパレート処理 */
int func_time_sync_stop( void );
/* [20250717] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム スケジュール管理処理
 *
 * 機能：
 *		スケジュール情報配信サーバからスケジュールファイルをダウンロードし、
 *		ファイルに定義されている出力抑制値をSCHCTLFIL.DATに保持する。
 *		(１)スケジュール制御指示ファイルから、当日のスケジュールのみを読み込み、
 *		   現在時刻と一致したスケジュールの制御を行う。
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
	int						today=0;					/* 現在日 *//* [20231120] Add */
	int						nowdate;					/* 現在時刻 *//* [20231120] Add */


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
		printf("PVSCHMNG(PVSCH_MON) : File Open Error\n");
		return( -1 );
	}

	/* プロセス動作中フラグを設定 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_SET );

	/* プロセス異常中フラグを解除 */
	commfunc_manage_abnFlagSet( procNo, PV_FLG_STS_RESET );

	/* プロセス初期化処理 */
	func_init();

	while( 1 )
	{
		/* 停止通知受信済みフラグリセット */
		stopFlag = 0;


		/* 指定周期になるまで待機する */
/* [20180621] Chg St */
		//if( commfunc_wait( 10, procNo ) < 0 )
		if( commfunc_wait( 1, procNo ) < 0 )
/* [20180621] Chg Ed */
		{
			/* 停止通知受信済みフラグ設定 */
			stopFlag = 1;
		}

		/* SSL通信データ出力フラグをローカルに保持 */
		s_sslDataFlag = ps_g_PVSYSINF->sslDataFlag;	/* [20190116] Add */

		/* 現在時刻を取得 */
		func_cmn_set_process_tim();
		 
		func_cmn_get_process_tim(&u_time, &timeData);

/* [20231120] Add St */
		/* 現在時刻から何月日を取得 */
		nowdate = timeData.tm_year;
		nowdate *= 100;
		nowdate += timeData.tm_mon;
		nowdate *= 100;
		nowdate += timeData.tm_mday;
		/* 日付が変わっている場合 */
		if( today != nowdate )
		{
			/* 通信エラーコード、設定値異常を解除 */
			ps_g_SCHCTLFIL->configErr = 0;
			ps_g_SCHCTLFIL->s_dwlinfo.comErrCode = 0;

			/* ID登録結果が登録以外の場合 */
			if( ps_g_SCHCTLFIL->idRegist != 1 )
			{
				/* ID登録結果を初期化 */
				ps_g_SCHCTLFIL->idRegist = 0xFF;
			}

			/* 日付を更新 */
			today = nowdate;
		}
/* [20231120] Add Ed */

/* [20251027] Add St */
		/* スケジュール管理情報が設定されている場合 */
		if( ps_g_SCHCTLFIL->statFlag == 1 )
		{
/* [20251027] Add Ed */
/* [CENTOS] Chg St */
			/* 実行ユーザチェック */
			if( getuid() != 0 )
			{
				/* root権限以外は、systemctlを実行できないので、ログを出力し、終了 */
				if( ntpFlg == 0 )
				{
					commfunc_syslog_msgset( PV_OWN_NAME, "#### Failed to ntpdate: Access denied #### " ); 
					ntpFlg = -1;
				}
			}
			else
			{
	/* [20250717] Add St */
				/* 時刻同期サービス終了 */
				func_time_sync_stop();
	/* [20250717] Add Ed */

				/* NTPサーバ同期処理 */
				retCode = func_ntpdate_start( ntpFlg );
				if( retCode == 0 )
				{
					ntpFlg = 1;
				}
			}
/* [CENTOS] Chg Ed */
/* [20251027] Add St */
		}
/* [20251027] Add Ed */

		/* 年が2000年未満の場合は、動作しない */
		if( timeData.tm_year < 2000 )
		{
			/* 装置状態状変：時刻同期エラー */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CLOCK_ERR, 1, 0, 0,
				ps_g_SVSTSFIL, ps_g_SVMSKINF );
		}
		else
		{
			/* 装置状態状変：時刻同期エラー解除 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CLOCK_ERR, 0, 0, 0,
				ps_g_SVSTSFIL, ps_g_SVMSKINF );

/* [20180621] Chg St */
			/* 毎10秒に更新チェックを行う */
			if( ( timeData.tm_sec % 10 ) == 0 )
			{
				/* CSVデータ更新の判定 */
				func_csvupdate_check();
			}
/* [20180621] Chg Ed */

			/* スケジュールデータの位置を更新 */
			func_buff_select();

/* [20251027] Add St */
			/* スケジュール管理情報が設定されている場合 */
			if( ps_g_SCHCTLFIL->statFlag == 1 )
			{
/* [20251027] Add Ed */
/* [20180621] Chg St */
				/* スケジュール情報配信サーバからダウンロード */
				retCode = func_sch_main();
/* [20180621] Chg Ed */
/* [20251027] Add St */
			}
/* [20251027] Add Ed */

/* [20231120] Dell St */
//			/* 装置状態状変：設定値異常 */
//			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_CONFIGERR, ps_g_SCHCTLFIL->configErr, 0, 0, 
//						ps_g_SVSTSFIL, ps_g_SVMSKINF );
/* [20231120] Dell Ed */

			/* 装置状態状変：通信異常 */
			commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_COMMERR, ps_g_SCHCTLFIL->getSchErr, 0, 0, 
						ps_g_SVSTSFIL, ps_g_SVMSKINF );

		}

		if( stopFlag )
		{
			PVSCH_MSG_HIG("PVSCHMNG(PVSCH_MON) : PROCESS STOP COMMAND RECV (PROC:%d)\n", procNo);
			/* 処理を抜ける */
			break;
		}
	}

	/* プロセス動作中フラグを解除 */
	commfunc_manage_runFlagSet( procNo, PV_FLG_STS_RESET );

	/* ファイルをクローズする */
	commfunc_file_close( s_fileIndex );

	/* ntpdateを停止する */
	func_ntpdate_stop();

	return( 0 );
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
int	func_init( void )
{
	int						loopCnt1;					/* ループカウンタ */
	int						retCode;					/* 関数戻り値格納用 */
	char					filePath[1024];				/* ファイルパス */
	short					svNumb;						/* 装置状態番号 */
	unsigned char			svBitMask;					/* 状態情報ビットマスク */
	short					svOnOff;					/* 状態情報 */
	char					csvFilePath[1024];			/* CSVファイルパス */
    struct passwd 			*pwd;

	memset(s_sslCertKeyPath, 0, sizeof(s_sslCertKeyPath));
	/* 実ユーザIDからユーザ名を取得 */
    pwd = getpwuid( getuid() );

	/* 環境設定ファイルから設定値を取り込む */
	func_get_config();						/* [20250717] Add */

	/* SSL認証秘密鍵ファイルのパスを設定 */
	if( strncmp( pwd->pw_name, "root", 4 ) == 0 )
	{
		sprintf(s_sslCertKeyPath, "/root/.ssh/%s", PV_SSL_CERT_KEY);
	}
	else
	{
		sprintf(s_sslCertKeyPath, "/home/%s/.ssh/%s", pwd->pw_name, PV_SSL_CERT_KEY);
	}

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcat(csvFilePath, PV_FNAME_PGCONINF );
	strcat(csvFilePath, ".csv");

	/* ファイルが存在しているか確認 */
	if( access( csvFilePath, R_OK | W_OK ) == 0 )
	{
		/* 発電所ＩＤが異なる場合、ＩＤ登録結果を初期化 */
		if( strncmp( ps_g_SCHCTLFIL->plantId, ps_g_PGCONINF->plantId, 26 ) != 0 )
		{
			/* 初期化 */
			memset(ps_g_SCHCTLFIL, 0, sizeof(struct st_SCHCTLFIL));
			ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = 0xFF;
			ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = 0xFF;
			ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = PV_UP_SCH_RTY_MIN_1ST;

			/* 発電所ＩＤからチェックサムを取得 */
			ps_g_SCHCTLFIL->chkSumNumb = ps_g_PGCONINF->plantId[25] - '0';

			/* 発電所ＩＤを更新 */
			memcpy( ps_g_SCHCTLFIL->plantId, ps_g_PGCONINF->plantId, sizeof(ps_g_SCHCTLFIL->plantId));
			PVSCH_MSG_HIG("update plantId[%s]\n", ps_g_SCHCTLFIL->plantId );

			/* ID登録結果を初期化 */
			ps_g_SCHCTLFIL->idRegist = 0xFF;
		}

		/* 電力会社識別子 */
		ps_g_SCHCTLFIL->powerGrid = ps_g_PGCONINF->powerGrid;

		/* スケジュール区分を初期化 */
		memset( ps_g_SCHCTLFIL->s_dwlinfo.kubun, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.kubun));
		/* エラー情報を初期化 */
		ps_g_SCHCTLFIL->s_dwlinfo.comErrCode = 0;	/* 通信エラーコード */
		/* エラーファイルのエラーコードを初期化 */
		memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode));
		/* エラーファイルのエラーメッセージを初期化 */
		memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg));
		/* ダウンロードファイル名を初期化 */
		memset(ps_g_SCHCTLFIL->s_dwlinfo.fileName, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.fileName));


		/* 出力変化時間を更新 */
		ps_g_SCHCTLFIL->chgTime = ps_g_PGCONINF->chgTime;
		/* スケジュール未登録時の出力制御値(%)を更新 */
		ps_g_SCHCTLFIL->schNonPer = ps_g_PGCONINF->schNonPer;

		/* ID登録結果が登録以外の場合 */
/* [20180601] Chg St */
		if( ps_g_SCHCTLFIL->idRegist != 1 )
/* [20180601] Chg Ed */
		{
			/* ID登録結果を初期化 */
			ps_g_SCHCTLFIL->idRegist = 0xFF;
		}

		/* 装置状態状変：設定値異常 解除 */
		ps_g_SCHCTLFIL->configErr=0;				/* 設定値異常 */

		/* 装置状態状変：通信異常 解除*/
		ps_g_SCHCTLFIL->getSchErr=0;				/* スケジュール取得異常 */

		/* 実装フラグをON */
		ps_g_SCHCTLFIL->statFlag=1;

	}
	else
	{
/* [20251027] Cht St */
		/* 実装フラグをOFFに設定 */
		ps_g_SCHCTLFIL->statFlag = 0;
/* [20251027] Cht Ed */
	}

	/* スケジュール関連初期化 */
	s_schYear = 0;
	s_lastUpdate = 0;
	s_ntpdateFlg = 0;
	s_fixSchReqFlg = 0;
	s_fixSchReqTime = 0;
	s_fixSchRetryCnt = 0;

	/* 積算秒を取得 */
	s_lastTime = time( NULL );

	return(0);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＣＳＶデータ更新チェック
 *
 * 機能：
 * 		ＣＳＶデータの更新時刻をチェックし、再読み込みを行う。
 * 		※ただし、出力変化時間のみ更新する。
 * 戻り値：
 */
void	func_csvupdate_check( void )
{

	char					csvFilePath[1024];			/* CSVファイルパス */
	struct stat				statBuff;					/* ファイル状態 */

	//PVSCH_MSG_LOW("START\n");

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcat(csvFilePath, PV_FNAME_PGCONINF );
	strcat(csvFilePath, ".csv");

	/* ファイルが存在しているか確認 */
	if( access( csvFilePath, R_OK | W_OK ) == 0 )
	{
		/* ファイル状態の取得に失敗 */
		if( stat( csvFilePath, &statBuff ) != 0 )
		{
			sprintf( errMsg, "PGCONINF.csv File Access Error!!" );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
		}
		else
		{
			/* ファイルの最終更新時刻の確認 */
			if( s_lastUpdate != 0 )
			{
				/* ファイルが変更されている場合 */
				if( s_lastUpdate != statBuff.st_mtime )
				{
					/* 電力会社向け情報取得 */
					func_pvsch_pgconinf_read();
					
					/* 最終更新時刻を保存 */
					s_lastUpdate = statBuff.st_mtime;
				}
			}
			else
			{
				/* 初回(起動時)は、最終更新時刻を保存 */
				s_lastUpdate = statBuff.st_mtime;
			}
		}
	}

	//PVSCH_MSG_LOW("END\n");


}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 電力会社向け情報ファイル読み込み
 *
 * 機能：
 * 		電力会社向け情報の出力変化時間のみ更新する。
 * 戻り値：
 */
int func_pvsch_pgconinf_read( void )
{
	FILE				*fp;						/* ファイルポインタ */
	char				readBuff[1024];				/* CSVファイル読み込みバッファ */
	char				csvFilePath[1024];			/* CSVファイルパス */
	int					csvCount;					/* CSVデータ数（デリミタ分割数） */
	int					retCode = 0;				/* 戻り値 */
	struct st_sepStr	s_sepStr;					/* CSVデータ分割格納 */
	int					workData;					/* 数値変換用 */
	int					syoriCount = 0;				/* CSV処理カウンタ */

	/* ファイルパスの展開 */
	strcpy(csvFilePath, PV_PATH_CSV );
	strcat(csvFilePath, PV_FNAME_PGCONINF );
	strcat(csvFilePath, ".csv");

	/* CSVファイルオープン */
	if( (fp = fopen(csvFilePath, "r")) != NULL)
	{
		/* CSVファイルの終端まで繰り返す */
		while( fgets( readBuff, 1024, fp) != NULL )
		{
			/* CSV処理カウンタ＋１ */
			syoriCount++;
			/* コメント行はスキップ */
			if( readBuff[0] == '#' ) continue;
			/* 未入力行（1文字以下）はスキップ */
			if( strlen( readBuff ) < 1) continue;
			/* CSVデータの分割 */
			csvCount = func_cmn_sepcsv_data( readBuff, &s_sepStr );

			/* CSVデータ数が不正の場合 */
			if( csvCount != 9 )
			{
				sprintf( errMsg, "CSV-Format Error (Data is not 9). File[%s] Line[%d] DATACNT[%d]", csvFilePath, syoriCount, csvCount );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				retCode = -1;
				break;
			}

			/* 出力変化時間の取得 */
			workData = atol( s_sepStr.data[7] );
			if( (workData < 5 ) || ( workData > 10 ) )
			{
				sprintf( errMsg, "CSV-Format Error (Outoput Change Time). File[%s] Line[%d] DAT[7]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				retCode = -1;
				break;
			}
			PVSCH_MSG_HIG("Output Change Time [%d] -> [%d]\n", ps_g_SCHCTLFIL->chgTime, workData );
			ps_g_SCHCTLFIL->chgTime = workData;

			/* スケジュール未登録時の出力制御値の取得 */
			workData = atol( s_sepStr.data[8] );
			if( (workData < 0 ) || ( workData > 100 ) )
			{
				sprintf( errMsg, "CSV-Format Error (schdule none percent). File[%s] Line[%d] DAT[8]", csvFilePath, syoriCount );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				retCode = -1;
				break;
			}
			ps_g_SCHCTLFIL->schNonPer = workData;
		}
		/* CSVファイルをクローズ */
		fclose( fp );
	}
	else
	{
		sprintf( errMsg, "File[%s] Open error", csvFilePath );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		retCode = -1;
	}

	/* 終了 */
	return( retCode );	
	
}
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 現在時刻のスケジュール情報の設定
 *
 * 機能：
 * 		現在スケジュールのバッファ面、データ位置の設定
 * 戻り値：
 */
void	func_buff_select( void )
{
	int				loopCnt;
	time_t 			u_time;
	struct tm		timeData;					/* 時刻情報 */
	char			svFixed;
	char			svUpdate;
	char			svNoSch;

	func_cmn_get_process_tim(&u_time, &timeData);

	/* 現在時刻が前回時刻より過去となり、且つ、差分がPV_SCH_SYNC_ERR_MIN以上の場合 */
	if( ( u_time < s_lastTime ) &&
	    ((s_lastTime - u_time) >= (PV_SCH_SYNC_ERR_MIN*60)) )
	{
		/* ダウンロード情報を初期化する。 */
		memset(&ps_g_SCHCTLFIL->s_dwlinfo, 0, sizeof(struct st_dwlinfo));
		ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = 0xFF;
		ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = 0xFF;
		ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = PV_UP_SCH_RTY_MIN_1ST;
		/* 設定値異常を解除する。 */
		ps_g_SCHCTLFIL->configErr = 0;
	}

/* [20181114] Chg St */
	/* 電源ON時又は、年切り替わり時、または、現在時刻の年とバッファ年が不一致の場合 */
	if( ( s_schYear != timeData.tm_year ) || ( ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear != timeData.tm_year ))
/* [20181114] Chg Ed */
	{
		s_schYear = timeData.tm_year;

		/* 使用面を検索する */
		for( loopCnt = 0; loopCnt < PV_SCH_BUFF_NUM; loopCnt++ )
		{
			if( ps_g_SCHCTLFIL->s_schinfo[(ps_g_SCHCTLFIL->buffSelect+loopCnt) % PV_SCH_BUFF_NUM].schYear == s_schYear )
			{
				break;
			}
		}

		/* 使用面がない場合 */
		if( loopCnt == PV_SCH_BUFF_NUM )
		{
			/* ダウンロード情報を初期化する。 */
			memset(&ps_g_SCHCTLFIL->s_dwlinfo, 0, sizeof(struct st_dwlinfo));
			ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = 0xFF;
			ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = 0xFF;
			ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = PV_UP_SCH_RTY_MIN_1ST;
			/* バッファを初期化し、0面を使用面にする。 */
			memset( &ps_g_SCHCTLFIL->s_schinfo, 0x00, sizeof(struct st_schinfo)*PV_SCH_BUFF_NUM);
			ps_g_SCHCTLFIL->buffSelect = 0;
			ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear = s_schYear;
			/* 設定値異常を解除する。 */
			ps_g_SCHCTLFIL->configErr = 0;
		}
		/* 使用面があった場合 */
		else
		{
			/* 使用面を設定する */
			ps_g_SCHCTLFIL->buffSelect = loopCnt;

			/* 次面の年が設定され、且つ、翌年でない場合は初期化する */
			if( ( ps_g_SCHCTLFIL->s_schinfo[(ps_g_SCHCTLFIL->buffSelect + 1) % PV_SCH_BUFF_NUM].schYear != 0 ) &&
				( ps_g_SCHCTLFIL->s_schinfo[(ps_g_SCHCTLFIL->buffSelect + 1) % PV_SCH_BUFF_NUM].schYear != s_schYear + 1) )
			{
				memset( &ps_g_SCHCTLFIL->s_schinfo[(ps_g_SCHCTLFIL->buffSelect+1) % PV_SCH_BUFF_NUM], 0x00, sizeof(struct st_schinfo));
			}
		}
	}

	/* 前回時刻として保持 */
	s_lastTime = u_time;


	/* 現在時刻のデータ位置を算出 */
	ps_g_SCHCTLFIL->dataPos = func_cmn_get_data_pos(timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min);

	/* 現在時刻のスケジュール情報を状態として設定する */
	switch( ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].s_schdata[ps_g_SCHCTLFIL->dataPos].schType )
	{
		case PV_SCH_TYPE_FIXED :
			svFixed = 1;
			svUpdate = 0;
			svNoSch = 0;
			break;
		case PV_SCH_TYPE_UPDATE :
			svFixed = 0;
			svUpdate = 1;
			svNoSch = 0;
			break;
		case PV_SCH_TYPE_NONE :
		default :
			svFixed = 0;
			svUpdate = 0;
			svNoSch = 1;
			break;
	}

	/* 装置状態状変：固定スケジュール運用 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_FIXED_SCH, svFixed, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	/* 装置状態状変：更新スケジュール運用 */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_UPDATE_SCH, svUpdate, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );
	/* 装置状態状変：運用スケジュールなし */
	commfunc_johen_pos( PV_MKIND_PVSYS, 1, PV_SYS_SV_NO_SCH, svNoSch, 0, 0, ps_g_SVSTSFIL, ps_g_SVMSKINF );


	PVSCH_MSG_MID("buffSelect=%d, year=%d, dataPos=[%d], (type,data)=[%d,%d]\n", ps_g_SCHCTLFIL->buffSelect, 
			ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear, ps_g_SCHCTLFIL->dataPos, 
			ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].s_schdata[ps_g_SCHCTLFIL->dataPos].schType,
			ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].s_schdata[ps_g_SCHCTLFIL->dataPos].data);

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュール取得処理
 *
 * 機能：
 * 		スケジュールファイルの要求を行う必要があるか以下の順にチェックし、必要な
 * 		場合、ダウンロードを行う。
 *		  ①発電所ＩＤが未登録の場合        ：ＩＤ登録確認
 *		  ②次回アクセス時刻を過ぎた場合    ：更新スケジュール
 *		  ③初期起動後に更新フラグが不一致  ：固定スケジュール(年間)
 *		  ④初期起動後に更新スケジュール、固定スケジュール(月間)
 *		    を取得していない場合            ：固定スケジュール(月間)
 *		  ⑤更新フラグが不一致、且つ、現在時刻が発電所ＩＤの末尾のチェックサムに
 *		    応じた実施時間帯の場合          ：固定スケジュール(年間)
 *		  ⑥現在時刻のスケジュールが未設定の場合、又は、月末日の場合、
 *		    且つ、現在時刻が発電所ＩＤの末尾のチェックサムに応じた実施時間帯の場合
 *		                                  ：固定スケジュール(月間)
 * 			┏━━━━━━━┳━━━━━━━━━━━┓
 * 			┃ チェックサム ┃      実施時間帯      ┃
 * 			┣━━━━━━━╋━━━━━━━━━━━┫
 * 			┃		0		┃ 21:10:00 ～ 21:29:59 ┃
 * 			┃		1		┃ 21:40:00 ～ 21:59:59 ┃
 * 			┃		2		┃ 22:10:00 ～ 22:29:59 ┃
 * 			┃		3		┃ 22:40:00 ～ 22:59:59 ┃
 * 			┃		4		┃ 23:10:00 ～ 23:29:59 ┃
 * 			┃		5		┃ 23:40:00 ～ 23:59:59 ┃
 * 			┃		6		┃ 00:10:00 ～ 00:29:59 ┃
 * 			┃		7		┃ 00:40:00 ～ 00:59:59 ┃
 * 			┃		8		┃ 01:10:00 ～ 01:29:59 ┃
 * 			┃		9		┃ 01:40:00 ～ 01:59:59 ┃
 * 			┗━━━━━━━┻━━━━━━━━━━━┛
 * 戻り値：
 * 		PV_REQ_SCH_TYPE_NONE		：スケジュール種別（なし）
 * 		PV_REQ_SCH_TYPE_YEAR		：スケジュール種別（年間）
 * 		PV_REQ_SCH_TYPE_MONTH		：スケジュール種別（月間）
 * 		PV_REQ_SCH_TYPE_UPDATE		：スケジュール種別（更新）
 * 		PV_REQ_SCH_TYPE_IDREG		：スケジュール種別（ID登録確認）
 */
int	func_sch_main( void )
{
	time_t 			u_time;
	struct tm 		timeData;
	int 			todaySec;
	int				retCode = 0;
	int				reqType = PV_REQ_SCH_TYPE_NONE;
	int				fixType;
	time_t 			u_nextMon;
	struct tm		nextMonData;

	PVSCH_MSG_LOW("START\n");

	func_cmn_get_process_tim(&u_time, &timeData);

/* [20170329] Chg St */
	while( 1 )
	{
		/* 設定値異常が発生 */
		if( ps_g_SCHCTLFIL->configErr != 0 )
		{
			/* リクエストをしない */
			PVSCH_MSG_HIG("Schedule No Request. configErr=%d\n", ps_g_SCHCTLFIL->configErr );
			break;
		}

		/* ＩＤ登録結果を取得していない場合 */
		if( ps_g_SCHCTLFIL->idRegist == 0xFF )
		{
			/* ID登録確認結果を取得する */
			retCode = func_schfile_request(PV_REQ_SCH_TYPE_IDREG);
		}

/* [20190611] Add St */
		/* ＩＤ登録結果が取得済み(ID未登録)の場合 */
		if( ps_g_SCHCTLFIL->idRegist == 0 )
		{
			/* リクエストをしない */
			break;
		}
/* [20190611] Add Ed */
/* [20180601] Del

		/* 現在時刻が次回アクセス日時(初期起動時は0)を超えた場合 */
		if( ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime <= u_time )
		{
			/* 更新スケジュールを取得する */
			retCode = func_schfile_request(PV_REQ_SCH_TYPE_UPDATE);

			/* 更新スケジュール取得にて、設定値異常が発生 */
			if( ps_g_SCHCTLFIL->configErr != 0 )
			{
				/* 固定スケジュールの取得は行わない */
				PVSCH_MSG_HIG("Schedule No Request. configErr=%d\n", ps_g_SCHCTLFIL->configErr);
				break;
			}
		}

		/* 更新フラグが不一致の場合、且つ */
		/* 固定スケジュール(年間)を1度もダウンロードしていない場合 */
		if( ( ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr != ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext ) &&
		    ( ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr == 0xFF ) )
		{
			/* 固定スケジュール(年)を取得する */
			reqType = PV_REQ_SCH_TYPE_YEAR;
		}
		/* 更新スケジュール、又は、固定スケジュール(月間)を1度もダウンロードしていない場合 */
		else if(( ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext == 0xFF ) &&
				( ps_g_SCHCTLFIL->s_dwlinfo.getMon == 0 ))
		{
			/* 固定スケジュール(月間)を取得する */
			reqType = PV_REQ_SCH_TYPE_MONTH;
		}
		else
		{
			/* 固定スケジュール種別を初期化 */
			fixType = PV_REQ_SCH_TYPE_NONE;

			/* 翌月の月を取得 */
			u_nextMon = u_time + ( PV_FIX_SCH_GET_INTER_DAY * 60 * 60 * 24 );
			localtime_r( &u_nextMon, &nextMonData );
			nextMonData.tm_mon += 1;

			/* 更新フラグが不一致の場合 */
			if( ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr != ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext )
			{
				fixType = PV_REQ_SCH_TYPE_YEAR;
			}
			/* 更新スケジュールを1度もダウンロードしていない場合、且つ */
			/* 現在時刻のスケジュール、又は、翌月のスケジュールがない場合 */
			else if( ( ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext == 0xFF ) &&
					 (( ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].s_schdata[ps_g_SCHCTLFIL->dataPos].schType == PV_SCH_TYPE_NONE ) ||
					  ( ps_g_SCHCTLFIL->s_dwlinfo.getMon != (((nextMonData.tm_year * 100) + nextMonData.tm_mon ) % 10000)) ) )
			{
				fixType = PV_REQ_SCH_TYPE_MONTH;
			}

			
			if( fixType != PV_REQ_SCH_TYPE_NONE )
			{
				/* 当日の積算秒を取得 */
				todaySec = PV_GET_TODAY_SEC(timeData.tm_hour, timeData.tm_min);

				/* チェックサムからの要求期間内の場合 */
			    if( (schAcsTime[ps_g_SCHCTLFIL->chkSumNumb] <= todaySec) && (todaySec <= (schAcsTime[ps_g_SCHCTLFIL->chkSumNumb]+PV_SCH_FIXED_LIMIT_SEC)) )
			    {
					/* 固定スケジュール要求していない場合 */
					if( s_fixSchReqFlg == 0 )
					{
						/* 固定スケジュール要求をセットする */
						s_fixSchReqFlg = 1;

						/* リトライ回数を初期化 */
						s_fixSchRetryCnt = 0;

						/* 要求時刻を保持 */
						s_fixSchReqTime = u_time;
					}
				}
				else
				{
					/* 固定スケジュール要求をリセット */
					s_fixSchReqFlg = 0;
				}

				/* 現在時刻が要求時刻の範囲内の場合 */
				if( ( s_fixSchReqTime > 0 ) && 
				    ( s_fixSchReqTime <= u_time) && ( u_time <= (s_fixSchReqTime + (PV_FIX_SCH_RTY_NUM * PV_FIX_SCH_RTY_MIN * 60))))
				{
					/* リトライ時刻を過ぎている場合 */
					if( ( s_fixSchReqTime + (s_fixSchRetryCnt * PV_FIX_SCH_RTY_MIN * 60)) <= u_time )
					{
						/* 固定スケジュールを取得する */
						reqType = fixType;

						/* リトライ回数をインクリメント */
						s_fixSchRetryCnt++;
					}
				}
				else
				{
					/* 要求時刻をクリア */
					s_fixSchReqTime = 0;
				}
			}
			else
			{
				/* 固定スケジュール要求をリセット */
				s_fixSchReqFlg = 0;
			}
		}

		/* 要求スケジュールがある場合 */
		if( reqType != PV_REQ_SCH_TYPE_NONE )
		{
			retCode = func_schfile_request(reqType);
		}

		break;
	}
/* [20170329] Chg Ed */

	PVSCH_MSG_MID("retCode=%d\n", retCode);

	PVSCH_MSG_LOW("END\n");

	return retCode;

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイル取得＆解析処理
 *
 * 機能：
 * 		スケジュールファイルのダウンロードと読み込みを行う。
 * 引数：
 * 		PV_REQ_SCH_TYPE_YEAR		：スケジュール種別（年間）
 * 		PV_REQ_SCH_TYPE_MONTH		：スケジュール種別（月間）
 * 		PV_REQ_SCH_TYPE_UPDATE		：スケジュール種別（更新）
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int	func_schfile_request( int reqType )
{
	int				retCode;
	time_t 			u_time;

	PVSCH_MSG_LOW("START\n");

	func_cmn_get_process_tim(&u_time, NULL);

	/* エラーファイルのエラー情報を初期化 */
	memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode));
	memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg));

	/* スケジュール情報配信サーバからダウンロード */
	retCode = func_schfile_download_request( reqType );

	/* 処理結果が正常な場合 */
	if( retCode == 0 )
	{
		/* スケジュールファイル読み込み */
		retCode = func_schfile_read( reqType );
	}

	/* 正常な場合は、状態を復帰する */
	if( retCode == 0 )
	{
		/* 設定値異常 復帰 */
		ps_g_SCHCTLFIL->configErr = 0;
	}
	/* 異常な場合 */
	else
	{
		/* 更新スケジュールの場合 */
		if( reqType == PV_REQ_SCH_TYPE_UPDATE )
		{
			/* 次回アクセス時刻を更新する */
			ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime = u_time + (ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin * 60);
		}

/* [20180601] Add St */
		/* ＩＤ登録結果の場合 */
		if( reqType == PV_REQ_SCH_TYPE_IDREG )
		{
			/* ID登録結果を通信異常（2）に設定 */
			ps_g_SCHCTLFIL->idRegist = 2;
		}
/* [20180601] Add Ed */
	}

	PVSCH_MSG_MID("reqType=%d, retCode=%d\n", reqType, retCode);

	PVSCH_MSG_LOW("END\n");
	return retCode;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイルダウンロード要求
 *
 * 機能：
 * 		スケジュールファイルのダウンロードを要求する
 * 引数：
 * 		PV_REQ_SCH_TYPE_NONE		：スケジュール種別（なし）
 * 		PV_REQ_SCH_TYPE_YEAR		：スケジュール種別（年間）
 * 		PV_REQ_SCH_TYPE_MONTH		：スケジュール種別（月間）
 * 		PV_REQ_SCH_TYPE_UPDATE		：スケジュール種別（更新）
 */
int	func_schfile_download_request( int reqType )
{
	struct tm 		timeData;
	int				retCode = 0;		/* 関数戻り値格納用 */
	char			kbn[5];
	time_t 			u_time;

	PVSCH_MSG_LOW("START\n");

	/* 区分領域初期化 */
	memset(kbn, 0x00, sizeof(kbn));

	/* 要求スケジュール種別 */
	switch( reqType )
	{
		/* 固定スケジュール（月間） */
		case PV_REQ_SCH_TYPE_MONTH :
/* [20170329] Add St */
			/* 現在時刻のスケジュールがない場合 */
			if( ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].s_schdata[ps_g_SCHCTLFIL->dataPos].schType == PV_SCH_TYPE_NONE )
			{
				/* 当月を指定 */
				func_cmn_get_process_tim(NULL, &timeData);
				sprintf(kbn, "%02d%02d", (timeData.tm_year%100), timeData.tm_mon );
			}
			else
			{
				/* 次月を指定 */
				func_cmn_get_process_tim(&u_time, NULL);
				u_time += ( PV_FIX_SCH_GET_INTER_DAY * 60 * 60 * 24 );
				localtime_r( &u_time, &timeData );
				timeData.tm_mon += 1;
			}
/* [20170329] Add Ed */
			sprintf(kbn, "%02d%02d", (timeData.tm_year%100), timeData.tm_mon );
			break;
		/* 固定スケジュール（年間） */
		case PV_REQ_SCH_TYPE_YEAR :
			sprintf(kbn, "999%d", ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext );
			break;
		/* 更新スケジュール */
		case PV_REQ_SCH_TYPE_UPDATE :
			sprintf(kbn, "0000");
			break;
		/* ID登録確認 */
		case PV_REQ_SCH_TYPE_IDREG :
			sprintf(kbn, "8888");
			break;
		default :
			sprintf( errMsg, "reqType is not support[%d]", reqType );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			retCode = -1;
			return retCode;
	}
	/* スケジュール区分を保持 */
	memcpy( ps_g_SCHCTLFIL->s_dwlinfo.kubun, kbn, sizeof(kbn));
	PVSCH_MSG_MID("kbn=[%s]\n", kbn);

	/* 結果情報を初期化 */
	ps_g_SCHCTLFIL->s_dwlinfo.comErrCode = 0;
	memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode));
	memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg));
	memset(ps_g_SCHCTLFIL->s_dwlinfo.fileName, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.fileName));

	/* SSL接続 */
	retCode = func_ssl_connect(kbn);

	/* 処理結果が異常の場合 */
	if( retCode < 0 )
	{
		/* 通信異常 発生 */
		ps_g_SCHCTLFIL->getSchErr = 1;
	}
	else
	{
		/* 通信異常 復帰 */
		ps_g_SCHCTLFIL->getSchErr = 0;
		
	}

	PVSCH_MSG_LOW("END\n");
	return retCode;

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイル読み込み処理
 *
 * 機能：
 * 		ダウンロードしたスケジュールファイルの読み込みを行う。
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常処理
 */
int	func_schfile_read( int reqType )
{

	int						retCode;					/* 関数戻り値格納用 */

	PVSCH_MSG_LOW("START\n");

	/* エラーファイルチェック */
	retCode = func_schfile_read_err();
	if( retCode != 0 )
	{
		/* 発電所側のエラーが発生している場合 */
		if( strncmp( &ps_g_SCHCTLFIL->s_dwlinfo.appErrCode[0], "E1", 2 ) == 0 )
		{
			/* 設定値異常とする */
			ps_g_SCHCTLFIL->configErr = 1;
		}
/* [20170329] Add St */
		/* 配信する更新スケジュールがない場合 */
		else if( ( strncmp( &ps_g_SCHCTLFIL->s_dwlinfo.appErrCode[0], "E0003", 5 ) == 0 ) &&
		         ( reqType == PV_REQ_SCH_TYPE_UPDATE ) )
		{
			/* 更新スケジュールリトライ時間を通常時に変更 */
			ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = PV_UP_SCH_RTY_MIN;
		}
/* [20170329] Add Ed */
	}
	else
	{
		/* 要求スケジュール種別 */
		switch( reqType )
		{
			/* 固定スケジュール（月間） */
			case PV_REQ_SCH_TYPE_MONTH :
				retCode = func_cmn_schfile_read(func_choice_file_month, PV_SCH_FMT_NO_MONTH);
/* [20170329] Add St */
				/* 正常終了の場合 */
				if( retCode == 0 )
				{
					ps_g_SCHCTLFIL->s_dwlinfo.getMon = atoi(ps_g_SCHCTLFIL->s_dwlinfo.kubun);
				}
/* [20170329] Add Ed */
				break;
			/* 固定スケジュール（年間） */
			case PV_REQ_SCH_TYPE_YEAR :
				retCode = func_cmn_schfile_read(func_choice_file_year, PV_SCH_FMT_NO_YEAR);
				/* 正常終了の場合(1は一部正常) */
				if( retCode == 0 )
				{
					/* 次回更新フラグを現在に反映する */
					ps_g_SCHCTLFIL->s_dwlinfo.upFlgCurr = ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext;
				}
				break;
			/* 更新スケジュール */
			case PV_REQ_SCH_TYPE_UPDATE :
				retCode = func_cmn_schfile_read(func_choice_file_update, PV_SCH_FMT_NO_UPDATE);
				/* 正常終了の場合 */
				if( retCode == 0 )
				{
					/* 更新スケジュールリトライ時間を通常時に変更 */
					ps_g_SCHCTLFIL->s_dwlinfo.upSchRtyMin = PV_UP_SCH_RTY_MIN;
				}
				break;
			/* ID登録確認 */
			case PV_REQ_SCH_TYPE_IDREG :
				retCode = func_schfile_read_id();
				break;
			default :
				/* 要求スケジュール種別に該当しないためエラーとする */
				sprintf( errMsg, "Request Schedule Type Error %d", reqType );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				PVSCH_MSG_ERR("%s\n", errMsg);
				retCode = -1;
				return retCode;
		}
	}

	PVSCH_MSG_LOW("retCode=%d\n", retCode);

	PVSCH_MSG_LOW("END\n");

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込みチェック処理（スケジュールファイル共通）
 *
 * 機能：
 * 		更新スケジュール、固定スケジュールファイルの読み込みを行う。
 * 戻り値：
 * 		0：正常終了
 * 	   -1：再取得
 */
int func_cmn_schfile_read( void *func_ptr, int fmtNo )
{
	struct dirent	**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int				retCode;					/* 関数戻り値 */
	int				retCode2;					/* 関数戻り値 */
	int				loopCnt1;					/* ループカウンタ */
	char			checkFile[256];				/* チェックファイル名 */
	char			movePath[256];				/* 変更後ファイルパス */
	int				filenum;					/* ファイル数 */
	struct tm 		timeData;

	PVSCH_MSG_LOW("START\n");

	/* スケジュール制御ファイルのリストを取得 */
	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	retCode = scandir( PV_PATH_DWL, &fileList, func_ptr, alphasort );
	if( retCode < 0 )
	{
		sprintf( errMsg, "Schedule File List Get Error" );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	/* ファイル数を設定 */
	filenum = retCode;

	/* ファイル数が0より大きい場合 */
	if( filenum > 0 )
	{
		/* ファイルリストを昇順で繰り返す */
		for( loopCnt1 = 0; loopCnt1 < filenum; loopCnt1++ )
		{
			/* ファイルを解析する */
			memset( checkFile, 0x00, sizeof(checkFile));
			sprintf( checkFile, "%s%s", PV_PATH_DWL, fileList[loopCnt1]->d_name);

			/* ファイルのフォーマットチェック(発電所ID、チェックサム) */
			retCode2 = func_cmn_FileFormatCheck(checkFile, fmtNo);
			if( retCode2 < 0 )
			{
				retCode = retCode2;
			}
			else
			{
				retCode = 0;
			}

/* [20190827] Chg St */
			/* SSL通信データ出力フラグがOFFの場合は、モニタリングサーバへの送信対象とする */
			/* （リネーム＆フォルダ移動する） */
			if( s_sslDataFlag == PV_SERIAL_WRITE_OFF )
			{
				/* 変更後のファイルパスを生成 */
				memset(movePath, 0x00, sizeof(movePath));
				sprintf( movePath, "%s%s_%s", PV_PATH_FTP, ps_g_PVSYSINF->siteIdName, fileList[loopCnt1]->d_name);
			}
			else
			{
				/* SSL通信データ出力フラグがOFFの場合は、モニタリングサーバへの送信対象としない */
				/* （ダウンロード時間を付与する） */
				func_cmn_get_process_tim(NULL, &timeData);
				sprintf(movePath, "%s%04d%02d%02d%02d%02d%02d_%s", 
					PV_PATH_DWL, timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, fileList[loopCnt1]->d_name);
			}
/* [20190827] Chg Ed */

/* [20230529] Chg St */
			///* ファイル名を変更する */
			//rename( checkFile, movePath );
			//PVSCH_MSG_MID("rename[%s]->[%s]\n", checkFile, movePath);
			/* ミツウロコではファイルは削除する */
			unlink(checkFile);	/* [20230905] Chg */
/* [20230529] Chg Ed */

			/* リソースを開放 */
			free( fileList[loopCnt1] );
		}
	}
	else
	{
		sprintf( errMsg, "Schedule File is not found [fmtNo=%03d]", fmtNo );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		retCode = -1;
	}
	/* リソースを開放 */
	free( fileList );

	PVSCH_MSG_LOW("retCode=%d\n", retCode);

	PVSCH_MSG_LOW("END\n");
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込みチェック処理（エラーファイル）
 *
 * 機能：
 * 		エラーファイルの読み込みを行う。
 * 戻り値：
 * 		0：正常処理（制御対象外）
 * 		1：正常処理（制御対象）
 * 	   -1：異常終了（制御対象外）
 */
int	func_schfile_read_err( void )
{
	struct dirent	**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int				retCode=-1;					/* 関数戻り値 */
	int				loopCnt1;					/* ループカウンタ */
	char			checkFile[256];				/* チェックファイル名 */
	int				filenum;					/* ファイル数 */
	char			buff[256];
	FILE			*fp;						/* ファイルディスクリプタ */
	int				fileSize;

	PVSCH_MSG_LOW("START\n");

	/* スケジュール制御ファイルのリストを取得 */
	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	retCode = scandir( PV_PATH_DWL, &fileList, func_choice_file_err, alphasort );
	if( retCode < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "Schedule File List Get Error" );
		return( -1 );
	}
	/* ファイル数を設定 */
	filenum = retCode;

	/* ファイル数が0より大きい場合 */
	if( filenum > 0 )
	{
		retCode = -1;
		
		/* ファイルリストを昇順で繰り返す */
		for( loopCnt1 = 0; loopCnt1 < filenum; loopCnt1++ )
		{
			/* ファイルを解析する */
			memset( checkFile, 0x00, sizeof(checkFile));
			sprintf( checkFile, "%s%s", PV_PATH_DWL, fileList[loopCnt1]->d_name);

			/* 対象スケジュールファイルオープン */
			if( (fp = fopen(checkFile, "rb")) == NULL)
			{
				sprintf( errMsg, "File[%s] Open error", checkFile );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				PVSCH_MSG_ERR("%s\n", errMsg);
			}
			else
			{
				/* ファイル読み込み */
				memset(buff, 0, sizeof(buff));
				fileSize = fread( buff, sizeof( unsigned char ), sizeof(buff), fp );

				/* 最低限のファイルサイズの確認 */
				if( fileSize < 5 )
				{
					sprintf( errMsg, "fileSize ERR[%d].[%s]", fileSize, fileList[loopCnt1]->d_name);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					PVSCH_MSG_ERR("%s\n", errMsg);
				}
				else
				{
					/* エラーコードとメッセージを区切る */
					buff[5]=0;

					/* エラーコードを設定 */
					memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode));
					sprintf(ps_g_SCHCTLFIL->s_dwlinfo.appErrCode, "%s", &buff[0]);

					/* エラーメッセージが格納されている場合 */
					if( fileSize > 6 )
					{
						/* エラーメッセージを保持 */
						memset(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg, 0, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg));
						sprintf(ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg, "%s", &buff[6]);
					}

					sprintf( errMsg, "ErrCode[%s], ErrMsg[%s]", ps_g_SCHCTLFIL->s_dwlinfo.appErrCode, ps_g_SCHCTLFIL->s_dwlinfo.appErrMsg );
					commfunc_syslog_msgset( pv_own_name, errMsg );
					PVSCH_MSG_ERR("%s\n", errMsg);
				}
				fclose(fp);
			}

			/* ファイルを削除する */
			unlink( checkFile );

			/* リソースを開放 */
			free( fileList[loopCnt1] );
		}
	}
	else
	{
		retCode = 0;
	}
	/* リソースを開放 */
	free( fileList );

	PVSCH_MSG_LOW("retCode=%d\n", retCode);

	PVSCH_MSG_LOW("END\n");
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイル読み込みチェック処理（スケジュール制御用）
 *
 * 機能：
 * 		ID登録結果ファイルの読み込みを行う。
 * 戻り値：
 * 		0：正常処理（制御対象外）
 * 		1：正常処理（制御対象）
 * 	   -1：異常終了（制御対象外）
 */
int	func_schfile_read_id( void )
{
	struct dirent	**fileList;					/* ディレクトリ内ファイルリスト用ポインタ */
	int				retCode=-1;					/* 関数戻り値 */
	int				loopCnt1;					/* ループカウンタ */
	int				loopCnt2;
	char			checkFile[256];				/* チェックファイル名 */
	int				filenum;					/* ファイル数 */
	char			buff[256];
	FILE			*fp;						/* ファイルディスクリプタ */
	int				fileSize;
	char			plantId[27];
	char			movePath[256];				/* 変更後ファイルパス *//* [20180309] Add St */

	PVSCH_MSG_LOW("START\n");

	/* スケジュール制御ファイルのリストを取得 */
	/* 送信フォルダの中から送信対象ファイルが格納されているディレクトリ一覧を取得する */
	retCode = scandir( PV_PATH_DWL, &fileList, func_choice_file_id, alphasort );
	if( retCode < 0 )
	{
		commfunc_syslog_msgset( pv_own_name, "Schedule File List Get Error" );
		return( -1 );
	}
	/* ファイル数を設定 */
	filenum = retCode;

	/* ファイル数が0より大きい場合 */
	if( filenum > 0 )
	{
		/* ファイルリストを昇順で繰り返す */
		for( loopCnt1 = 0; loopCnt1 < filenum; loopCnt1++ )
		{
			/* ファイルを解析する */
			memset( checkFile, 0x00, sizeof(checkFile));
			sprintf( checkFile, "%s%s", PV_PATH_DWL, fileList[loopCnt1]->d_name);

/* [20180309] Add St */
			/* 変更後のファイルパスを生成 */
			memset(movePath, 0x00, sizeof(movePath));
			sprintf( movePath, "%s%s_%s", PV_PATH_DWL, ps_g_PVSYSINF->siteIdName, fileList[loopCnt1]->d_name);
/* [20180309] Add Ed */

			/* 対象スケジュールファイルオープン */
			if( (fp = fopen(checkFile, "rb")) == NULL)
			{
				sprintf( errMsg, "File[%s] Open error", checkFile );
				commfunc_syslog_msgset( pv_own_name, errMsg );
				PVSCH_MSG_ERR("%s\n", errMsg);
				retCode = -1;
			}
			else
			{
				/* ファイル読み込み */
				fileSize = fread( buff, sizeof( unsigned char ), sizeof(buff), fp );

				/* ファイルサイズの確認 */
				/* ヘッダ部(6)+データ部(発電所ID(26)+登録結果(1)) */
				if( fileSize < 33 )
				{
					sprintf( errMsg, "fileSize ERR[%d].[%s]", fileSize, fileList[loopCnt1]->d_name);
					commfunc_syslog_msgset( pv_own_name, errMsg );
					PVSCH_MSG_ERR("%s\n", errMsg);
					retCode = -1;
				}
				else
				{
					/* 発電所IDチェック */
					for( loopCnt2 = 0; loopCnt2 < 26; loopCnt2++ )
					{
						sprintf(&plantId[loopCnt2], "%d", buff[6+loopCnt2]);
					}

					/* 発電所IDのチェック */
					if( strncmp( &plantId[0], ps_g_PGCONINF->plantId, 26 ) != 0 )
					{
						sprintf( errMsg, "plantId different=[%s]", plantId);
						commfunc_syslog_msgset( pv_own_name, errMsg );
						PVSCH_MSG_ERR("%s\n", errMsg);
						retCode = -1;
					}
					else
					{
						/* 登録結果の確認 */
						if( buff[32] == 0 )
						{
							/* ID登録済みに変更する */
							ps_g_SCHCTLFIL->idRegist = 1;
							retCode = 0;
						}
						else
						{
							/* ID未登録に変更する */
							ps_g_SCHCTLFIL->idRegist = 0;
							retCode = 0;
							sprintf( errMsg, "plantId is no regist");
							commfunc_syslog_msgset( pv_own_name, errMsg );
							PVSCH_MSG_HIG("%s\n", errMsg);
						}
					}
				}
				fclose(fp);
			}

/* [20230529] Chg St */
/* [20180309] Chg St */
//			/* ファイルを削除する */
//			unlink( checkFile );
//			/* ファイル名を変更する */
//			rename( checkFile, movePath );
//			PVSCH_MSG_MID("rename[%s]->[%s]\n", checkFile, movePath);
/* [20180309] Chg Ed */
			/* ファイルを削除する */
			unlink( checkFile );
/* [20230529] Chg Ed */

			/* リソースを開放 */
			free( fileList[loopCnt1] );
		}
	}
	else
	{
		retCode = -1;
		sprintf( errMsg, "Id regist file is not Found");
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
	}
	/* リソースを開放 */
	free( fileList );

	PVSCH_MSG_LOW("retCode=%d\n", retCode);

	PVSCH_MSG_LOW("END\n");
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（固定スケジュール(月間)）
 *
 * 機能：
 * 		func_cmn_schfile_read()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュールファイルの有無を判断し、
 * 		ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_file_month( const struct dirent *dir )
{
	char		delFile[256];			/* 削除ファイル名 */

	/* ファイルパスを生成 */
	sprintf( delFile, "%s%s", PV_PATH_DWL, dir->d_name );

	/* 確認ファイル名の共通チェック */
	if( func_cmn_choice_filename(dir->d_name) < 0 )
	{
		return(0);
	}


	/* 確認ファイル名のフォーマットNoのチェック */
	if( strncmp( &dir->d_name[0], "202_", 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "filename format number different. unlink[%s]", dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	/* スケジュール区分のチェック */
	if( strncmp( &dir->d_name[4], ps_g_SCHCTLFIL->s_dwlinfo.kubun, 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "download filename kubun is different[%s]. unlink [%s]", ps_g_SCHCTLFIL->s_dwlinfo.kubun, dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	PVSCH_MSG_MID("target file [%s]\n", dir->d_name);

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（固定スケジュール(年間)）
 *
 * 機能：
 * 		func_cmn_schfile_read()関数内で使用する scandir() 関数のマスク関数として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュールファイルの有無を判断し、
 * 		ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_file_year( const struct dirent *dir )
{
	int			workData;					/* 数値変換用 */
	char		delFile[256];			/* 削除ファイル名 */

	/* ファイルパスを生成 */
	sprintf( delFile, "%s%s", PV_PATH_DWL, dir->d_name );

	/* 確認ファイル名の共通チェック */
	if( func_cmn_choice_filename(dir->d_name) < 0 )
	{
		return(0);
	}

	/* 確認ファイル名のフォーマットNoのチェック */
	if( strncmp( &dir->d_name[0], "201_", 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "filename format number different. unlink[%s]", dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	/* 確認ファイル名のスケジュール区分のチェック */
	if( strncmp( &dir->d_name[4], "999", 3 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "download filename kubun is different[999X]. kubun error. unlink [%s]", dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}
	/* 確認ファイル名の更新フラグのチェック */
	workData = dir->d_name[7] - '0';

	/* 更新フラグが異なる場合は、破棄する */
	if( workData != ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "download file update flag is different[%d] != [%d]. unlink %s", ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext, workData, dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return(0);
	}

	PVSCH_MSG_MID("target file [%s]\n", dir->d_name);

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（更新スケジュール）
 *
 * 機能：
 * 		func_cmn_schfile_read()関数内で使用するscandir()関数のマスク関数内として
 * 		機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュールファイルの有無を判断し、
 * 		ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_file_update( const struct dirent *dir )
{
	int			workData;					/* 数値変換用 */
	char		delFile[256];			/* 削除ファイル名 */

	/* ファイルパスを生成 */
	sprintf( delFile, "%s%s", PV_PATH_DWL, dir->d_name );

	/* 確認ファイル名の共通チェック */
	if( func_cmn_choice_filename(dir->d_name) < 0 )
	{
		return(0);
	}

	/* 確認ファイル名のフォーマットNoのチェック */
	if( strncmp( &dir->d_name[0], "203_", 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "filename format number different. unlink[%s]", dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	/* 確認ファイル名のスケジュール区分のチェック */
	if( strncmp( &dir->d_name[4], "0000", 4 ) != 0 )
	{
		unlink( delFile );
		sprintf( errMsg, "download filename kubun error. unlink [%s]", dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	PVSCH_MSG_MID("target file [%s]\n", dir->d_name);

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（エラーファイル）
 *
 * 機能：
 * 		scandir() 関数のマスク関数内で共通のフィルタ処理として機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュールファイル(共通)の
 * 		有無を判断し、ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_file_err( const struct dirent *dir )
{
	char		delFile[256];			/* 削除ファイル名 */

	/* ファイルパスを生成 */
	sprintf( delFile, "%s%s", PV_PATH_DWL, dir->d_name );

	/* 確認ファイル名の共通チェック */
	if( func_cmn_choice_filename(dir->d_name) < 0 )
	{
		return(0);
	}

	/* 確認ファイル名のフォーマットNoのチェック */
	if( strncmp( &dir->d_name[0], "ERR_", 4 ) != 0 )
	{
//		PVSCH_MSG_LOW("filename format number error [%s]\n", dir->d_name);
		return( 0 );
	}

	/* スケジュール区分のチェック */
	if( strncmp( &dir->d_name[4], ps_g_SCHCTLFIL->s_dwlinfo.kubun, 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "download filename kubun is different[%s]. unlink[%s]", ps_g_SCHCTLFIL->s_dwlinfo.kubun, dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	PVSCH_MSG_MID("target file [%s]\n", dir->d_name);

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（ID登録結果）
 *
 * 機能：
 * 		scandir() 関数のマスク関数内で共通のフィルタ処理として機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュールファイル(共通)の
 * 		有無を判断し、ファイルがあるものだけを処理する。
 * 戻り値：
 * 		0：正常処理（処理対象外）
 * 		1：正常処理（処理対象）
 */
int	func_choice_file_id( const struct dirent *dir )
{
	char		delFile[256];			/* 削除ファイル名 */

	/* ファイルパスを生成 */
	sprintf( delFile, "%s%s", PV_PATH_DWL, dir->d_name );

	/* 確認ファイル名の共通チェック */
	if( func_cmn_choice_filename(dir->d_name) < 0 )
	{
		return(0);
	}

	/* 確認ファイル名のフォーマットNoのチェック */
	if( strncmp( &dir->d_name[0], "301_", 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "filename format number different. unlink[%s]", dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	/* 確認ファイル名のスケジュール区分のチェック */
	if( strncmp( &dir->d_name[4], "8888", 4 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "download filename kubun is different[%s]. unlink[%s]", ps_g_SCHCTLFIL->s_dwlinfo.kubun, dir->d_name);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( 0 );
	}

	PVSCH_MSG_MID("target file [%s]\n", dir->d_name);

	/* 処理対象 */
	return( 1 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフィルタ処理（スケジュールファイル共通）
 *
 * 機能：
 * 		scandir() 関数のマスク関数内で共通のフィルタ処理として機能する。
 * 		scandir()関数で列挙されるファイル名からスケジュールファイル(共通)の
 * 		有無を判断し、ファイルがあるものだけを処理する。
 * 戻り値：
 * 	   -1：正常処理（処理対象外）
 * 		0：正常処理（処理対象）
 */
int func_cmn_choice_filename( const char *FileName )
{
	char 		tmpName[56];
	char		delFile[256];			/* 削除ファイル名 */
	struct tm	check_time;				/* 時刻構造体 */

	/* ファイルパスを生成 */
	sprintf( delFile, "%s%s", PV_PATH_DWL, FileName );

	/* 確認ファイル名が55文字以外の場合は処理対象外のため除外	*/
	if( strnlen( FileName, 56 ) != 55 )
	{
		//PVSCH_MSG_LOW("filename length error [%s]\n", FileName);
		return( -1 );
	}

	memset(tmpName, 0x00, sizeof(tmpName));
	memcpy(tmpName, FileName, sizeof(tmpName));

	/* 確認ファイル名の拡張子のチェック */
	if( strncmp( &tmpName[50], ".data", 5 ) != 0 )
	{
		//sprintf( errMsg, "download filename kakutyoushi error [%s]", FileName);
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	tmpName[50]=0;

	/* ファイル名フォーマットチェック */
	if( ( tmpName[3] != '_' ) ||
		( tmpName[8] != '_' ) ||
		( tmpName[35] != '_' ) )
	{
		//sprintf( errMsg, "download filename format error [%s]", FileName);
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	tmpName[3] = 0;
	tmpName[8] = 0;
	tmpName[35]= 0;

	/* 生成時刻の文字列をチェック */
	if( func_cmn_string_check( &tmpName[36], 14) != 0x0001 )
	{
		//sprintf( errMsg, "download filename CreateTime error [%s]", FileName);
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}

	/* 生成日時を時刻構造体に格納 */
	check_time.tm_sec = atoi( &tmpName[48] );	tmpName[48]=0;
	check_time.tm_min = atoi( &tmpName[46] );	tmpName[46]=0;
	check_time.tm_hour = atoi( &tmpName[44] );	tmpName[44]=0;
	check_time.tm_mday = atoi( &tmpName[42] );	tmpName[42]=0;
	check_time.tm_mon = atoi( &tmpName[40] );	tmpName[40]=0;
	check_time.tm_year = atoi( &tmpName[36] );	tmpName[36]=0;

	/* 確認ファイル名の生成日時のチェック */
	if( func_cmn_time_check(&check_time) < 0 )
	{
		sprintf( errMsg, "download filename time error [%s]", FileName);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
	}


	/* 発電所IDの文字列をチェック */
	if( func_cmn_string_check( &tmpName[9], 26) != 0x0001 )
	{
		//sprintf( errMsg, "download filename plantId error [%s]", FileName);
		//commfunc_syslog_msgset( pv_own_name, errMsg );
		//PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}

	/* 確認ファイル名の発電所IDのチェック */
	if( strncmp( &tmpName[9], ps_g_PGCONINF->plantId, 26 ) != 0 )
	{
		/* ファイルを削除 */
		unlink( delFile );
		sprintf( errMsg, "download filename plantId is different. unlink[%s]", FileName);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}

	return ( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻情報チェック
 *
 * 機能：
 * 		引数 timData で指定された時刻情報の整合性をチェックする
 * 戻り値：
 *		 0：正常
 *		-1：異常
 */
int func_cmn_time_check( struct tm *timData )
{
	/* 年のチェック */
	if( timData->tm_year <= 2000 )
	{
		return( -1 );
	}

	/* 月のチェック */
	if( (timData->tm_mon < 1 ) || (timData->tm_mon > 12 ) )
	{
		return( -1 );
	}

	/* 日のチェック */
	if( (timData->tm_mday < 1 ) || (timData->tm_mday > 31 ) )
	{
		return( -1 );
	}

	/* 時のチェック */
	if( (timData->tm_hour < 0 ) || (timData->tm_hour > 23 ) )
	{
		return( -1 );
	}

	/* 分のチェック */
	if( (timData->tm_min < 0 ) || (timData->tm_min > 59 ) )
	{
		return( -1 );
	}

	/* 秒のチェック */
	if( (timData->tm_sec < 0 ) || (timData->tm_sec > 59 ) )
	{
		return( -1 );
	}
	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 文字列判定処理
 *
 * 機能：
 * 		引数 strPointer で指定されたアドレスに格納されている文字列を確認し、内
 *		容毎に以下の値を返す。
 * 戻り値：
 *		 0x0001：数字が含まれている場合
 *		 0x0002：ピリオドが含まれている場合
 *		 0x0004：ハイフン又は、アンダースコアが含まれている場合
 *		 0x0008：小文字アルファベットが含まれている場合
 *		 0x0010：大文字アルファベットが含まれている場合
 *		 0x0100：アルファベットが16進数（A-F,a-f）以外が含まれている場合
 *		-1：上記以外の文字（空白、記号、タブ、全角など）が含まれている場合
 *		    または文字列長が引数（strlenMax）による指定文字以上の場合
 */
int	func_cmn_string_check( char *strPoint, char strlength )
{
	int					retCode = 0;				/* 戻り値 */
	int					loopCnt;					/* ループカウンタ */

	/* 文字列長がstrlength以外の場合は異常	*/
	if( strnlen( strPoint, strlength+1 ) < strlength )
		return( -1 );

	/* 文字列の最後まで確認する */
	for( loopCnt = 0 ; loopCnt < strlength ; loopCnt++ )
	{
		/* ハイフンまたはアンダースコアの場合 */
		if( (*(strPoint + loopCnt) == '-') || (*(strPoint + loopCnt) == '_') )
		{
			retCode |= 0x0004;
		}
		/* アルファベット（小文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'a') && (*(strPoint + loopCnt) <= 'z') )
		{
			retCode |= 0x0008;
			/* 16進数の文字以外の場合 */
			if(*(strPoint + loopCnt) > 'f')
			{
				retCode |= 0x1000;
			}
		}
		/* アルファベット（大文字）の場合 */
		else if( (*(strPoint + loopCnt) >= 'A') && (*(strPoint + loopCnt) <= 'Z') )
		{
			retCode |= 0x0010;
			/* 16進数の文字以外の場合 */
			if(*(strPoint + loopCnt) > 'F')
			{
				retCode |= 0x1000;
			}
		}
		/* 数値の場合 */
		else if( (*(strPoint + loopCnt) >= '0') && (*(strPoint + loopCnt) <= '9') )
		{
			retCode |= 0x0001;
		}
		/* ピリオドの場合 */
		else if((*(strPoint + loopCnt) == '.'))
		{
			retCode |= 0x0002;
		}
		else
		{
			return( -1 );
		}
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイルのデータを反映する
 *
 * 機能：
 * 		スケジュールファイルのデータを管理情報に反映する
 * 戻り値：
 * 		-1：全て範囲外
 * 		 0：全て範囲内
 * 		 1：一部範囲外
 */
int func_schfile_data_check(struct tm *timeData, int dataNum, unsigned char *ptr_data, int fmtNo )
{
	int						buffSelect;
	int						setDataNum;
	int						dataPos;
	int						retCode = 0;

	PVSCH_MSG_LOW("START\n");

	/* スケジュール管理情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_SCHCTLFIL );

	/* データをコピーする */
	/* 書き込みバッファ面取得 */
	/* スケジュールデータが今年の場合 */
	if( ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear == timeData->tm_year )
	{
		buffSelect = ps_g_SCHCTLFIL->buffSelect;
	}
	/* スケジュールデータが翌年の場合 */
	else if( (ps_g_SCHCTLFIL->s_schinfo[ps_g_SCHCTLFIL->buffSelect].schYear + 1) == timeData->tm_year )
	{
		/* バッファを切り換える */
		buffSelect = (ps_g_SCHCTLFIL->buffSelect+1) % PV_SCH_BUFF_NUM;

		/* バッファの開始年がスケジュール年と異なる場合 */
		if( ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear != timeData->tm_year)
		{
			/* バッファを初期化する */
			memset( &ps_g_SCHCTLFIL->s_schinfo[buffSelect], 0x00, sizeof(struct st_schinfo));
			ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear = timeData->tm_year;
		}
	}
	else
	{
		/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
		commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );
		PVSCH_MSG_LOW("set schedule year[%d] not found. \n", timeData->tm_year);
		return( -1 );
	}

	/* バッファ面に対する書き込み位置を取得 */
	dataPos = func_cmn_get_data_pos(timeData->tm_year, timeData->tm_mon, timeData->tm_mday, timeData->tm_hour, timeData->tm_min);

	// 2016/12/31 23:50 => 17567
	// 2016 データ数    => 17568(↑は0はじまりだから）
	/* バッファ面への書き込み可能数を算出 */
	if( (dataPos + dataNum) > (PV_GET_YEAR_DAYS(timeData->tm_year) * PV_SCH_DAY_TI_NUM))
	{
		setDataNum = (PV_GET_YEAR_DAYS(timeData->tm_year) * PV_SCH_DAY_TI_NUM) - dataPos;
	}
	else
	{
		setDataNum = dataNum;
	}

	/* データ書き込み共通関数 */
	func_schfile_data_set(buffSelect, dataPos, setDataNum, ptr_data, fmtNo);
	ptr_data += setDataNum;


	/* 書き込みデータが残っている場合 */
	if( dataNum != setDataNum )
	{
		
		/* 書き込み面がSelect面と同じ場合 */
		if( buffSelect == ps_g_SCHCTLFIL->buffSelect )
		{
			/* 2面目のバッファ面に切り換えて設定 */
			buffSelect = (buffSelect+1) % PV_SCH_BUFF_NUM;

			/* バッファの開始年がスケジュール年と異なる場合 */
			if( ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear != timeData->tm_year+1)
			{
				/* バッファを初期化する */
				memset( &ps_g_SCHCTLFIL->s_schinfo[buffSelect], 0x00, sizeof(struct st_schinfo));
				/* バッファ面の年を設定する */
				ps_g_SCHCTLFIL->s_schinfo[buffSelect].schYear = timeData->tm_year+1;
			}

			/* 残データの書き込みデータ数を補正(2面を超えることがある為） */
			if( (dataNum - setDataNum) > (PV_GET_YEAR_DAYS(timeData->tm_year+1) * PV_SCH_DAY_TI_NUM) )
			{
				/* 1年分のデータを設定 */
				setDataNum = PV_GET_YEAR_DAYS(timeData->tm_year+1) * PV_SCH_DAY_TI_NUM;
				retCode = 1;
			}
			else
			{
				/* 全てのデータを設定 */
				setDataNum = dataNum - setDataNum;
			}

			/* データ書き込み共通関数 */
			func_schfile_data_set(buffSelect, 0, setDataNum, ptr_data, fmtNo);
		}
		else
		{
			retCode = 1;
		}
	}
	/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );

	PVSCH_MSG_LOW("END\n");
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイルのデータを反映する
 *
 * 機能：
 * 		スケジュールファイルのデータを管理情報に反映する
 * 戻り値：
 */
int func_schfile_data_set(int buffSelect, int dataPos, int dataNum, unsigned char *ptr_data, int fmtNo )
{
	int        			schTypeBorder;
	int        			setSchType;
	int					loopCnt;
	struct st_schdata	*ptr_schData;
	signed char			*p_data;

	PVSCH_MSG_LOW("START\n");

	PVSCH_MSG_MID("(buffSelect,dataPos,dataNum,fmtNo)=[%d,%d,%d,%d]\n", buffSelect, dataPos, dataNum, fmtNo);

	p_data = (signed char *)ptr_data;
	
	/* フォーマットNoによる上書き境界種別と更新種別を設定 */
	switch( fmtNo )
	{
		case PV_SCH_FMT_NO_UPDATE :
			schTypeBorder = PV_SCH_TYPE_UPDATE;
			setSchType = PV_SCH_TYPE_UPDATE;
			break;
		case PV_SCH_FMT_NO_YEAR :
		case PV_SCH_FMT_NO_MONTH :
		default :
			schTypeBorder = PV_SCH_TYPE_FIXED;
			setSchType = PV_SCH_TYPE_FIXED;
			break;
	}

	/* 書き込み開始ポインタを設定 */
	ptr_schData = &ps_g_SCHCTLFIL->s_schinfo[buffSelect].s_schdata[dataPos];

/* [20251027] Del *//* 不要な更新処理を削除 */

	/* データ数分繰り返す */
	for( loopCnt = 0; loopCnt < dataNum; loopCnt++ )
	{
		/* 該当スケジュールの更新判定 */
		if( ptr_schData->schType <= schTypeBorder )
		{
			/* 該当スケジュールを更新 */
			ptr_schData->schType = setSchType;

			/* 出力制御率の範囲チェック */
			if( *p_data < 0 )
			{
				/* 0より小さい場合は、0に丸めこむ */
				ptr_schData->data = 0;
			}
			else if( *p_data > 100 )
			{
				/* 100より大きい場合は、100に丸めこむ */
				ptr_schData->data = 100;
			}
			else
			{
				ptr_schData->data = *p_data;
			}

			/* データ更新 *//* [20230529] Add */
			ptr_schData->update = PV_FLG_STS_SET;

		}
		p_data++;
		ptr_schData++;
	}

	PVSCH_MSG_LOW("END\n");

	return 0;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ファイルフォーマットチェック
 *
 * 機能：
 * 		スケジュールファイルのフォーマットをチェックする
 * 戻り値：
 * 		 0：正常
 * 		-1：異常(システム異常)
 * 		-2：異常(ファイルサイズ異常)
 * 		-3：異常(発電所ＩＤ不正、チェックサム異常)
 */
int func_cmn_FileFormatCheck( const char *FilePath, int fmtNo)
{
	FILE					*fp;						/* ファイルディスクリプタ */
	int						dataBlock;
	int						fileSize;
	int						checkSize=0;
	unsigned  char			buff[20065+1];
	int						loopCnt;
	int						loopCnt2;
	unsigned  char			*p_buff;
	char					plantId[27];
	int						dataNum;
	int						checksum;
	int						updateFlg;
	int						checksumNum;
	int						retCode = 0;
	int						blockErr;
	unsigned  char			*ptr_data;
	struct tm				timeData;		/* 時刻情報 */
	struct tm				nextTimeData;		/* 時刻情報 */
	time_t					upSchReqTime;

	PVSCH_MSG_LOW("START\n");

	PVSCH_MSG_LOW("FilePath=[%s]\n", FilePath);

	/* 対象スケジュールファイルオープン */
	if( (fp = fopen(FilePath, "rb")) == NULL)
	{
		sprintf( errMsg, "File[%s] Open error", FilePath );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}

	/* ファイル読み込み */
	fileSize = fread( buff, sizeof( unsigned char ), sizeof(buff), fp );

	/* 最低限のファイルサイズの確認 */
	/* データ部のデータ数: 6 byte */
	if( fileSize <= 6 )
	{
		sprintf( errMsg, "file is not enough size[%s:%d]", FilePath, fileSize);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		fclose(fp);
		return( -2 );
	}


	p_buff = &buff[0];

	/* データ部のデータ数を取得 */
	dataBlock = func_cmn_bin2Num(p_buff, 6);
	p_buff+=6;
	checkSize +=6;
	PVSCH_MSG_MID("dataBlock=[%d]\n", dataBlock);

	for( loopCnt=0; loopCnt < dataBlock; loopCnt++)
	{
		/* ブロック毎のエラー情報を初期化 */
		blockErr = 0;

		/* 固定オフセット（53 byte） */
		/* ┣スケジュールID    :10 byte */
		/* ┣発電所ID          :26 byte */
		/* ┣制御年月日        :12 byte */
		/* ┗出力制御率データ数: 5 byte */
		checkSize += 53;
		if( fileSize < checkSize )
		{
			sprintf( errMsg, "DataBlock[%d] is not enough1 data size.[%d]<[%d]", loopCnt+1, fileSize, checkSize);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			retCode = -2;
			break;
		}

		/* スケジュールＩＤは意識しない */
		p_buff+=10;		/* スケジュールID */

		/* 発電所IDチェック */
		memset(plantId, 0, sizeof(plantId));
		for( loopCnt2=0; loopCnt2 < 26; loopCnt2++ )
		{
			sprintf(&plantId[loopCnt2], "%d", *p_buff);
			p_buff++;
		}
		/* 発電所IDのチェック */
		if( strncmp( &plantId[0], ps_g_PGCONINF->plantId, 26 ) != 0 )
		{
			sprintf( errMsg, "DataBlock[%d] is plantId different=[%s]", loopCnt+1, plantId);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			blockErr = -1;
		}

		/* 制御年月日を取得 */
		timeData.tm_year = func_cmn_bin2Num(p_buff, 4);		p_buff+=4;
		timeData.tm_mon  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
		timeData.tm_mday = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
		timeData.tm_hour = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
		timeData.tm_min  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
		timeData.tm_sec  = 0;

		/* 確認ファイル名の生成日時のチェック */
		if( func_cmn_time_check(&timeData) < 0 )
		{
			sprintf( errMsg, "schedule date error [%04d/%02d/%02d %02d:%02d %02d]",
						timeData.tm_year, timeData.tm_mon, timeData.tm_mday,
						timeData.tm_hour, timeData.tm_min, timeData.tm_sec);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			blockErr = -2;
		}


		/* 制御データ数を取得 */
		dataNum = func_cmn_bin2Num(p_buff, 5);
		p_buff+=5;

		/* 出力データ以降のファイルサイズチェック */
		/* ┣データ部          :dataNum byte */
		/* ┣チェックサム      : 2 byte */
		/* ┣更新フラグ      ※: 1 byte */
		/* ┗次回アクセス日時※:14 byte */
		/*  ※更新スケジュールのみ*/
		checkSize += dataNum + 2;
		if( fmtNo == PV_SCH_FMT_NO_UPDATE )
		{
			checkSize += 15;
		}
		if( fileSize < checkSize )
		{
			sprintf( errMsg, "DataBlock[%d] is not enough2 data size.[%d]<[%d]", loopCnt+1, fileSize, checkSize);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			retCode = -2;
			break;
		}

		PVSCH_MSG_MID("DataStartTime=[%04d%02d%02d %02d:%02d], DataNum=[%d]\n", 
			timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, dataNum);

		/* 出力データのチェックサムを算出 */
		ptr_data = p_buff;
		checksum = 0;
		for( loopCnt2=0; loopCnt2 < dataNum; loopCnt2++ )
		{
			checksum += *p_buff;
			p_buff++;
		}

		/* 更新スケジュールの場合は、更新フラグを取得 */
		if( fmtNo == PV_SCH_FMT_NO_UPDATE )
		{
			updateFlg = *p_buff;
			p_buff++;
		}

		/* チェックサム取得 */
		checksumNum = func_cmn_bin2Num(p_buff, 2);
		p_buff+=2;

		/* 出力データのチェックサムを判定 */
		if( ( checksum % (timeData.tm_mon + timeData.tm_mday)) != checksumNum )
		{
			sprintf( errMsg, "check sum different [%d] != [%d], date[%04d/%02d/%02d %02d:%02d]", 
				checksumNum,  (checksum % (timeData.tm_mon + timeData.tm_mday)), 
				timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			blockErr = -3;
		}

		/* 更新スケジュールの場合は、次回アクセス日時を取得 */
		if( fmtNo == PV_SCH_FMT_NO_UPDATE )
		{
			/* 次回アクセス日時を取得 */
			nextTimeData.tm_year = func_cmn_bin2Num(p_buff, 4);		p_buff+=4;
			nextTimeData.tm_mon  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			nextTimeData.tm_mday = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			nextTimeData.tm_hour = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			nextTimeData.tm_min  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;
			nextTimeData.tm_sec  = func_cmn_bin2Num(p_buff, 2);		p_buff+=2;

			/* 確認ファイル名の生成日時のチェック */
			if( func_cmn_time_check(&nextTimeData) < 0 )
			{
				sprintf( errMsg, "update time error [%04d/%02d/%02d %02d:%02d %02d]",
							nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday,
							nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
				commfunc_syslog_msgset( pv_own_name, errMsg );
				PVSCH_MSG_ERR("%s\n", errMsg);
				blockErr = -2;
			}
			else
			{
				/* 時刻取得共通関数 */
				upSchReqTime = func_cmn_mktime(nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday, 
											nextTimeData.tm_hour, nextTimeData.tm_min, nextTimeData.tm_sec);
			
			}
		}

		/* ブロックのフォーマットチェック  */
		if( blockErr != 0 )
		{
			/* 戻り値にエラーを設定 */
			retCode = -3;
		}
		else
		{
			/* 正常の場合は、スケジュールデータを反映する */
			func_schfile_data_check(&timeData, dataNum, ptr_data, fmtNo);
		}
	}

	/* 更新スケジュールは、次回アクセス日時と更新フラグを反映する */
	if( ( retCode == 0 ) && ( fmtNo == PV_SCH_FMT_NO_UPDATE ) )
	{
		PVSCH_MSG_HIG("updateFlg=[%d]\n", updateFlg);
		PVSCH_MSG_HIG("NextAccessTime=[%04d%02d%02d %02d:%02d], DataNum=[%d]\n", 
			nextTimeData.tm_year, nextTimeData.tm_mon, nextTimeData.tm_mday, nextTimeData.tm_hour, nextTimeData.tm_min, dataNum);
		ps_g_SCHCTLFIL->s_dwlinfo.upSchReqTime = upSchReqTime;
		ps_g_SCHCTLFIL->s_dwlinfo.upFlgNext = updateFlg;
	}

	fclose(fp);

	PVSCH_MSG_LOW("END\n");
	
	return retCode;
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
int func_cmn_bin2Num(char *strData, int byteNum)
{
	int						loopCnt;
	int						dataNum;
	int						powNum = 1;

	dataNum=0;
	for( loopCnt = byteNum-1; loopCnt >= 0; loopCnt-- )
	{
		dataNum += (strData[loopCnt] * powNum);
		powNum *= 10;
	}
	return dataNum;
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
	/* ntpdateを停止する */
	func_ntpdate_stop();

	return( 0 );
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
int func_cmn_set_process_tim( void )
{
	
	/* 積算秒を取得 */
	s_Time = time( NULL );
	/* 時刻情報に変換 */
	localtime_r( &s_Time, &s_timeData );

	s_timeData.tm_year += 1900;
	s_timeData.tm_mon+= 1;

//	PVSCH_MSG_MID("now[%04d/%02d/%02d %02d:%02d %02d]\n",
//			s_timeData.tm_year,s_timeData.tm_mon,s_timeData.tm_mday,
//			s_timeData.tm_hour, s_timeData.tm_min, s_timeData.tm_sec);

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
int func_cmn_get_process_tim( time_t *p_nowTime, struct tm *p_timData )
{
	if( p_nowTime != NULL )
	{
		/* 積算秒の引数がNULL以外の場合、設定する */
		*p_nowTime = s_Time;
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
 * UNIXタイム返却
 *
 * 機能：
 * 		年月日からUNIXタイムを返却する
 * 戻り値：
 * 		UNIXタイム
 */
time_t func_cmn_mktime( short year, char month, char day, char hour, char min, char sec)
{
    time_t r_time;
    struct tm mk_time;

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
 * 指定時刻のデータ格納位置を返却
 *
 * 機能：
 * 		指定した年に対して、月日時分に対する格納位置を返却する
 * 戻り値：
 * 		データ位置
 */
int func_cmn_get_data_pos( short year, char month, char day, char hour, char min)
{
    time_t 	s_time;
    time_t 	e_time;
    int	   	diff_time;
    int		data_num;

	s_time = func_cmn_mktime(year, 1, 1, 0, 0, 0);
	e_time = func_cmn_mktime(year, month, day, hour, min, 0);

	diff_time = e_time - s_time;
	data_num = (int)(diff_time/PV_SCH_TI_SEC);

    return data_num;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ｎｔｐ時刻同期処理
 *
 * 機能：
 *		ｎｔｐサーバとの時刻同期を行う
 * 		時刻同期は、発電所ＩＤの末尾のチェックサムに応じ、
 *		ntpdateを実行するshllスクリプトをバックグランドで起動する。
 * 		shellが起動中の場合は、何もしない。
 * 			┏━━━━━━━┳━━━━━━━━━━━┓
 * 			┃ チェックサム ┃      実施時間帯      ┃
 * 			┣━━━━━━━╋━━━━━━━━━━━┫
 * 			┃		0		┃ 21:00:00 ～ 21:09:59 ┃
 * 			┃		1		┃ 21:30:00 ～ 21:39:59 ┃
 * 			┃		2		┃ 22:00:00 ～ 22:09:59 ┃
 * 			┃		3		┃ 22:30:00 ～ 22:39:59 ┃
 * 			┃		4		┃ 23:00:00 ～ 23:09:59 ┃
 * 			┃		5		┃ 23:30:00 ～ 23:39:59 ┃
 * 			┃		6		┃ 00:00:00 ～ 00:09:59 ┃
 * 			┃		7		┃ 00:30:00 ～ 00:39:59 ┃
 * 			┃		8		┃ 01:00:00 ～ 01:09:59 ┃
 * 			┃		9		┃ 01:30:00 ～ 01:39:59 ┃
 * 			┗━━━━━━━┻━━━━━━━━━━━┛
 * 		ただし、引数 syncTypeが1の場合は、上記の実施時間帯に関わらず実施する。
 * 
 * 共通：
 *		時刻同期がエラーとなった場合は、1分間隔で最大5回リトライを行い、
 *		さらにエラーとなった場合は、30分後に再リトライを継続する。
 * 
 */
int	func_ntpdate_start( int syncType )
{
	int				retCode;					/* 関数戻り値 */
	struct tm		timeData;					/* 時刻情報 */
	time_t			checktime;					/* 積算秒 */
	char			ntpCommand[512];			/* SSHコマンド */
	int 			todaySec;					/* 当日の積算秒 */
	char			checkCmd[512];
	char			checkPid[512];
	FILE			*fp;

	/* 実行コマンド生成 */
	sprintf( checkCmd, "pgrep -o %s", PV_SCH_NTP_SHELL );
	/* コマンドを実行してntpdデーモンのプロセスIDをパイプから取得 */
	if( (fp = popen( checkCmd, "r" )) == NULL )
	{
		/* 確認コマンド実行失敗 */
		sprintf( errMsg, "%s CHECK FILE OPEN Error!", PV_SCH_NTP_SHELL ); 
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		retCode = -1;
	}
	else
	{
		/* 文字列を初期化 */
		memset( checkPid, 0, sizeof(checkPid) );
		/* プロセスＩＤを取得 */
		fgets( checkPid, 512, fp );
		/* パイプを閉じる */
		pclose( fp );

		/* 時刻取得 */
		func_cmn_get_process_tim(&checktime, &timeData);

		/* 当日の積算秒を取得 */
		todaySec = PV_GET_TODAY_SEC(timeData.tm_hour, timeData.tm_min);

		/* 強制起動、又は、 */
		/* チェックサムからの要求期間内の場合、且つ前回の実行からPV_SCH_NTP_LIMIT_SEC秒経過している場合 */
		if( (syncType == 0 ) ||
		    ((ntpAcsTime[ps_g_SCHCTLFIL->chkSumNumb] <= todaySec) && 
	          (todaySec < (ntpAcsTime[ps_g_SCHCTLFIL->chkSumNumb]+PV_SCH_NTP_LIMIT_SEC)) ))
		{
			/* 要求時間内に実行していない場合 */
			if( s_ntpdateFlg == 0 )
			{
				/* PIDが格納されていない場合(=停止) */
				if( atol( checkPid ) == 0 )
				{
					/* ＮＴＰサーバＩＰアドレス、ポート番号が設定されている場合 */
					if( ( ps_g_PGCONINF->ntpIpAddr[0] != 0 ) &&
						( ps_g_PGCONINF->ntpPortNumb > 0 ) )
					{
						/* 時刻同期Shellを起動する */
						sprintf(ntpCommand, "%s%s %s %d &", PV_PATH_EXE, PV_SCH_NTP_SHELL, ps_g_PGCONINF->ntpIpAddr, ps_g_PGCONINF->ntpPortNumb );		/* [20250710] Chg */
						PVSCH_MSG_HIG("system(%s)\n", ntpCommand );
						if( 0 == system( ntpCommand ) )
						{
							/* 実行フラグセット */
							s_ntpdateFlg = 1;
							/* ログを出力 */
							sprintf(errMsg, "func_ntpdate_start(): system( %s ) 実行", ntpCommand );
							commfunc_syslog_msgset( pv_own_name, errMsg );
						}
						else
						{
							/* ログを出力 */
							sprintf(errMsg, "func_ntpdate_start(): system( %s ) 失敗", ntpCommand );
							commfunc_syslog_msgset( pv_own_name, errMsg );
						}
					}
				}
			}
		}
		else
		{
			/* 実行フラグリセット */
			s_ntpdateFlg = 0;
		}

		retCode = 0;
	}

	return(retCode);
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期Shell停止
 *
 * 機能：
 *		時刻同期Shellを停止する
 * 
 * 共通：
 * 
 */
int	func_ntpdate_stop( void )
{
	int				retCode;					/* 関数戻り値 */
	char			checkCmd[512];
	char			checkPid[512];
	pid_t			pidNumb;
	FILE			*fp;

	/* 実行コマンド生成 */
	sprintf( checkCmd, "pgrep -o %s", PV_SCH_NTP_SHELL );

	while(1)
	{
		/* コマンドを実行してntpdデーモンのプロセスＩＤをパイプから取得 */
		if( (fp = popen( checkCmd, "r" )) == NULL )
		{
			/* 確認コマンド実行失敗 */
			sprintf( errMsg, "%s CHECK FILE OPEN Error!", PV_SCH_NTP_SHELL ); 
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			return( -1 );
		}

		pidNumb = 0;
		/* プロセスＩＤがなくなるまで繰り返す */
		while( fgets( checkPid, 512, fp ) != NULL )
		{
			/* プロセスＩＤの取得 */
			pidNumb = (pid_t)atoi( checkPid );
			PVSCH_MSG_HIG("kill Pid[%d]\n", pidNumb);

			/* ntpdate Shellに SIGKILL を送信 */
			retCode = kill( pidNumb, SIGKILL );
		}

		/* パイプを閉じる */
		pclose( fp );

		/* PIDがない場合 */
		if( pidNumb == 0 )
		{
			break;
		}
	}

	return(0);
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
int func_cmn_sepcsv_data( char *pc_data, struct st_sepStr *ps_sepStr )
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
 * ＳＳＬ接続
 *
 * 機能：
 * 		引数 kbn で指定された文字列を要求区分としてスケジュール情報配信サーバに
 * 		リクエストを行う。
 * 戻り値：
 * 		 0：正常終了
 * 		-1：異常終了
 */
int func_ssl_connect( char *kbn )
{
    int                 sockfd;
    char                *pt;
    char                msg[1024];
    char                host[1024];
    char                path[1024];
    struct sockaddr_in  s_sndAddr;
    struct hostent      *servhost;
    SSL_CTX             *sslCtx;
    int					sslret;
    long				sslVeryErr;
	X509 				*server_cert;
	char 				*certStr;
	char				verifyFlg=0;
	char				prefix[32];

	PVSCH_MSG_LOW("START\n");

    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));

	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "Start    : func_ssl_connect" );

    /* ＵＲＬからホスト名・パス取得 */
    /* ホスト名を取得 */
    sscanf(ps_g_PGCONINF->ipAddr, "https://%s", host);

    /* パスを取得 */
    if((pt = strchr(host, '/')) != NULL)
    {
        strcpy(path, pt);
        *pt = 0;
    }

    /* ＩＰアドレス取得 */
    if( (servhost = gethostbyname(host)) == NULL )
    {
		sprintf( errMsg, "ERROR gethostbyname(%s) Errno(%d),  h_errno(%d) ", host, errno,  h_errno );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}

    memset(&s_sndAddr, 0, sizeof(s_sndAddr));
    memcpy(&s_sndAddr.sin_addr, servhost->h_addr, servhost->h_length);
    s_sndAddr.sin_family = AF_INET;

    /* HTTPSのポート番号設定 */
    s_sndAddr.sin_port = htons( ps_g_PGCONINF->portNumb );
    /* ソケット生成 */
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 

    if( sockfd < 0 )
    {
		sprintf( errMsg, "ERROR TCP Socket Error (%s) Errno(%d)", host, errno );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : socket" );

    /* コネクト */
    if( connect(sockfd, (struct sockaddr *)&s_sndAddr, sizeof(s_sndAddr)) == -1 )
	{
		/* ソケットをクローズ */
		close( sockfd );
		sprintf( errMsg, "ERROR TCP Device CONNECT Error (%s:%d) Errno(%d)",
			host, s_sndAddr.sin_port, errno );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : connect" );
 
    /* エラー文字列の解放 */
    SSL_load_error_strings();

	/* ライブラリを初期化 */
    (void)SSL_library_init();
    /* プロトコル指定(SSLv2、SSLv3、TLSv1) */
    sslCtx = SSL_CTX_new(SSLv23_client_method());

    if( sslCtx == NULL )
    {
		close( sockfd );
		sprintf( errMsg, "ERROR SSL_CTX_new Error(null)");
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : SSL_CTX_new" );

	/* 証明書の設定 */
	sslret = SSL_CTX_load_verify_locations(sslCtx, s_sslCertKeyPath, NULL);
    if( sslret != 1 )
    {
		sprintf( errMsg, "ERROR SSL_CTX_load_verify_locations (%s)", ERR_reason_error_string(sslret) );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_HIG("%s\n", errMsg);
		/* 認証キーがなくても継続する */
		verifyFlg = 0;
	}
	else
	{
		verifyFlg = 1;
	}

	/* SSLオブジェクトの生成 */
    ssl = SSL_new(sslCtx);
    if( ssl == NULL )
    {
		sprintf( errMsg, "ERROR SSL_new Error(null)");
		commfunc_syslog_msgset( pv_own_name, errMsg );
	    SSL_CTX_free(sslCtx);
		close( sockfd );
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
		
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : SSL_new" );
	
	/* SSLオブジェクトとファイルディスクリプタを接続 */
    sslret = SSL_set_fd(ssl, sockfd);
    if( sslret != 1 )
    {
		sprintf( errMsg, "ERROR SSL_set_fd (%s)", ERR_reason_error_string(sslret) );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		close( sockfd );
	    SSL_CTX_free(sslCtx);
	    SSL_free(ssl); 
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : SSL_set_fd" );

	/* SSLの接続 */
    sslret = SSL_connect(ssl);
    if( sslret != 1 )
    {
		sprintf( errMsg, "ERROR SSL_connect ret(%d), errno(%d)", sslret, SSL_get_error(ssl, sslret) );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		close( sockfd );
	    SSL_CTX_free(sslCtx);

	    SSL_free(ssl); 
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : SSL_connect" );

/* [20190116] Add St */
	/* どの暗号化方式を使用するかを表示 (DES-CBC3-MD5 など) */
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		const char *cipher;
		cipher = SSL_get_cipher(ssl);
		sprintf(errMsg, "使用する暗号化方式は %s です。", cipher);
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}


	/* どの SSL プロトコルを使用するかを表示 (SSLv2, SSLv3/TLSv1 など) */
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		const char *version;
		version = SSL_get_cipher_version(ssl);
		sprintf(errMsg, "使用するプロトコルは %s です。", version);
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}
/* [20190116] Add Ed */

	/* 受信したサーバ証明書情報を取得 */
	server_cert = SSL_get_peer_certificate(ssl);
	if( server_cert != NULL )
	{
		/* サーバ証明書の所有者情報の取得 */
		certStr = X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0);
		sprintf(errMsg, "subject name:%s", certStr);
		PVSCH_MSG_LOW("%s", errMsg);
/* [20190116] Add St */
		if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		{
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
/* [20190116] Add Ed */

		/* サーバ証明書の発行者情報の取得 */
		certStr = X509_NAME_oneline(X509_get_issuer_name(server_cert), 0, 0);
		sprintf(errMsg, "issuer name:%s", certStr);
		PVSCH_MSG_LOW("%s", errMsg);
/* [20190116] Add St */
		if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		{
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
/* [20190116] Add Ed */

		/* X509の開放 */
		X509_free(server_cert);
	}

/* [20190116] Add St */
#if 0
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		BIO *STDout = BIO_new_fp(stdout, BIO_NOCLOSE);

		ASN1_TIME *notBefore = X509_get_notBefore(server_cert);
		ASN1_TIME *notAfter  = X509_get_notAfter(server_cert);

		BIO_printf(STDout, "   有効期限開始年月日: ");
		ASN1_TIME_print(STDout, notBefore);
		BIO_printf(STDout, "\n");
		BIO_printf(STDout, "   有効期限終了年月日: ");
		ASN1_TIME_print(STDout, notAfter);
		BIO_printf(STDout, "\n");
		BIO_free(STDout);
	}
#endif
/* [20190116] Add Ed */

	/* 秘密鍵が設定できている場合 */
	if( verifyFlg == 1 )
	{
		/* 認証結果のステータスを取得 */
		sslVeryErr = SSL_get_verify_result(ssl);
		if( sslVeryErr != X509_V_OK )
		{
			sprintf( errMsg, "ERROR SSL_get_verify_result ret(%lu), err(%s)", sslVeryErr, X509_verify_cert_error_string(sslVeryErr) );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_HIG("%s\n", errMsg);
			/* 認証結果が正常以外（信頼できないサーバ）でもファイルの取得は継続する */
		}
	}

    /* HTTPリクエストメッセージ生成 */
    sprintf(msg, "POST %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Length: 84\r\ncontent-type: application/x-www-form-urlencoded\r\n\r\npower_plant_id=%s&mac_address=%s&schedule_kbn=%s\r\n", 
    		path, host, ps_g_PGCONINF->plantId, ps_g_PGCONINF->macAddr, kbn );

/* [20190116] Add St */
	/* リクエストデータ出力 */
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		sprintf(prefix, "req_%s", kbn);
		func_ssl_data_file_write( prefix, msg, strlen(msg));
		func_ssl_data_file_close( );
	}
/* [20190116] Add Ed */

    /* リクエスト送信 */
    sslret = SSL_write(ssl, msg, strlen(msg));
    if( sslret < 0 )
    {
		sprintf( errMsg, "ERROR SSL_write ret(%d), errno(%d)", sslret, SSL_get_error(ssl, sslret) );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		close( sockfd );
	    SSL_CTX_free(sslCtx);
	    SSL_free(ssl); 
	    SSL_shutdown(ssl); 
		PVSCH_MSG_ERR("%s\n", errMsg);
		return( -1 );
	}
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
		commfunc_syslog_msgset( pv_own_name, "call out : SSL_write" );

/* [20190116] Add St */
	/* レスポンスデータ出力 */
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		sprintf(prefix, "res_%s", kbn);
		func_ssl_data_file_write( prefix, NULL, 0);
	}

/* [20190116] Add Ed */
	/* スケジュールファイルダウンロード */
	sslret = func_ssl_filedownload();

/* [20190116] Add St */
	/* レスポンスデータ出力 */
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		func_ssl_data_file_close( );
	}
/* [20190116] Add Ed */

    /* SSLの終了 */
    SSL_shutdown(ssl); 
    SSL_free(ssl); 
    SSL_CTX_free(sslCtx);

    ERR_free_strings();

	/* ソケットのクローズ */
    close(sockfd);

	PVSCH_MSG_LOW("END\n");

    return(sslret);
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールファイルダウンロード
 *
 * 機能：
 * 		
 * 戻り値：
 * 		 0 : 正常終了
 * 	 	-1 : 異常終了
 */
int func_ssl_filedownload( void )
{
    int retCode;

	PVSCH_MSG_LOW("START\n");

	/* SSL関連初期化 */
	p_sslBuff = NULL;
	sslReadSize = 0;

	/* レスポンスステータス取得 */
	retCode = func_ssl_getStatus();
	if( retCode < 0 )
	{
		return retCode;
	}
	
	/* レスポンスヘッダ取得 */
	retCode = func_ssl_getResHeader();
	if( retCode < 0 )
	{
		return retCode;
	}

	/* レスポンスボディ取得 */
	retCode = func_ssl_getResBody();
	if( retCode < 0 )
	{
		return retCode;
	}

	PVSCH_MSG_LOW("END\n");

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 指定時刻のデータ格納位置を返却
 *
 * 機能：
 * 		指定した年に対して、月日時分に対する格納位置を返却する
 * 戻り値：
 * 		  0 : 正常終了
 * 	 正の数 : 検索失敗（最後に検索した行の先頭位置）
 * 	 負の数 : 
 */
int func_ssl_read( char **p_restBuff )
{
	int reqSize;
	int readPos;
	int readSize;

	if( *p_restBuff != NULL )
	{
		readPos = sslReadSize - (*p_restBuff - sslBuff);
		reqSize = (PV_SSL_READ_SIZE-1) - readPos;
		memcpy(sslBuff, *p_restBuff, readPos);
	}
	else
	{
		reqSize = PV_SSL_READ_SIZE -1;
		readPos = 0;

	}

	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		sprintf( errMsg, "call in  : SSL_read(&sslBuff[%d], reqSize=%d)", readPos, reqSize );
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}

	/* SSLのデータを取得 */
	readSize = SSL_read(ssl, &sslBuff[readPos], reqSize);

	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		sprintf( errMsg, "call out : SSL_read readSize=%d", readSize );
		commfunc_syslog_msgset( pv_own_name, errMsg );
	}

	if( readSize < 0)
	{
		sprintf( errMsg, "SSL read Error(%d)(%d)", readSize, SSL_get_error(ssl, readSize));
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);

/* [20181114] Add St */
		/* 異常の場合は処理を終了する */
		return -1;
/* [20181114] Add Ed */
	}

/* [20190116] Add St */
	/* レスポンスデータ出力 */
	if( s_sslDataFlag == PV_SERIAL_WRITE_ON )
	{
		func_ssl_data_file_write( NULL, &sslBuff[readPos], readSize);
	}
/* [20190116] Add Ed */
	sslReadSize = readPos+readSize;
	sslBuff[sslReadSize] = 0;
	*p_restBuff = sslBuff;

	return sslReadSize;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 指定時刻のデータ格納位置を返却
 *
 * 機能：
 * 		指定した年に対して、月日時分に対する格納位置を返却する
 * 戻り値：
 * 		  0 : 正常終了
 * 	 正の数 : 検索失敗（最後に検索した行の先頭位置）
 * 	 負の数 : 
 */
int func_ssl_fgets( char **buff )
{
	int		readSize;
	int 	readFlg = 0;
	int		retCode;
	char	*tmp;

	while( 1 )
	{
		/* 読み込みバッファポインタがNULL以外の場合 */
		if( p_sslBuff != NULL )
		{
			/* 改行コード（CR+LF(\r\n)）を検索 */
			if((tmp = strstr(p_sslBuff, "\r\n")) != NULL)
			{
				/* 見つかった場合、\r\nを0に置換し、行の先頭アドレスと、データサイズ(\r\n含む)を返却 */
				*tmp = 0;
				*tmp++;
				*tmp = 0;
				*tmp++;
				*buff = p_sslBuff;
				retCode = (tmp - p_sslBuff);
				p_sslBuff = tmp;
				break;
			}
			/* 改行コード（LF(\n)）を検索 */
			else if((tmp = strstr(p_sslBuff, "\n")) != NULL)
			{
				/* 見つかった場合、\nを0に置換し、行の先頭アドレスと、データサイズ(\n含む)を返却 */
				*tmp = 0;
				*tmp++;
				*buff = p_sslBuff;
				retCode = (tmp - p_sslBuff);
				p_sslBuff = tmp;
				break;
			}
			else
			{
				/* 先頭から検索して見つからなかった場合 */
				if( p_sslBuff == sslBuff )
				{
					PVSCH_MSG_HIG("SSL_read 1 Line data size Over");
					retCode = -1;
					break;
				}
			}
			
		}

		/* データを読み込む */
		readSize = func_ssl_read(&p_sslBuff);

		/* ファイル終端、又は、異常の場合 */
		if( readSize <= 0 )
		{
			/* 終了する */
			retCode = readSize;
			p_sslBuff = NULL;
			break;
		}
		else
		{
			/* 先頭位置を設定する */
			p_sslBuff = sslBuff;
		}
	}

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＨＴＴＰステータスコード取得
 *
 * 機能：
 * 		スケジュール情報配信サーバからのレスポンスを解析し、ステータスコードを
 * 		取得する。
 * 戻り値：
 * 		 0 : 正常終了
 * 	 	-1 : 異常終了
 */
int func_ssl_getStatus( void )
{
	char	*p_buff;
	char	*p_tmp1;
	char	*p_tmp2;
	int		retCode;
	int		lineLen;
	int		stsCode=-1;

	PVSCH_MSG_LOW("START\n");

	/* 1行データを取得する */
	lineLen = func_ssl_fgets(&p_buff);
	if( lineLen <= 0 )
	{
		retCode = -1;
		return retCode;
	}

	if(strncmp(p_buff, "HTTP/", 5) != 0)
	{
		sprintf( errMsg, "HTTP Status Line Error1 [%s]", p_buff);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		retCode = -1;
		return retCode;
	}
	
	/* 空白を検索 */
	if((p_tmp1 = strstr(p_buff, " ")) == NULL)
	{
		sprintf( errMsg, "HTTP Status Line Error2 [%s]", p_buff);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		retCode = -1;
		return retCode;
	}

	p_tmp1++;
	/* 次の空白を検索 */
	if((p_tmp2 = strstr(p_tmp1, " ")) == NULL)
	{
		sprintf( errMsg, "HTTP Status Line Error3 [%s]", p_buff);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		retCode = -1;
		return retCode;
	}

	/* ステータスコードを取得 */
	*p_tmp2 = 0;
	stsCode = atoi(p_tmp1);
	*p_tmp2++;
	memset(ps_g_SCHCTLFIL->s_dwlinfo.comErrMsg,0,sizeof(ps_g_SCHCTLFIL->s_dwlinfo.comErrMsg));
	strncpy(ps_g_SCHCTLFIL->s_dwlinfo.comErrMsg, p_tmp2, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.comErrMsg)-1);

	/* 通信エラーコードを設定 */
	ps_g_SCHCTLFIL->s_dwlinfo.comErrCode = stsCode;

	/* エラーコード判定*/
	switch( (int)(stsCode/100) )
	{
		case 2:
			retCode = 0;
			break;
		case 4:
			/* 設定値異常 */
			ps_g_SCHCTLFIL->configErr = 1;
		case 1:
		case 3:
		case 5:
		default :
			sprintf( errMsg, "HTTP Responce Error StatusCode(%3d), Msg(%s)", stsCode, ps_g_SCHCTLFIL->s_dwlinfo.comErrMsg);
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			retCode = -1;
			break;
	}

	PVSCH_MSG_LOW("END\n");
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＨＴＴＰレスポンスヘッダ取得
 *
 * 機能：
 * 		スケジュール情報配信サーバからのレスポンスを解析し、ヘッダ情報を取得する。
 * 		※取得するヘッダ情報はない為、空行のみ検索する
 * 戻り値：
 * 		 0 : 正常終了
 * 	 	-1 : 異常終了
 */
int func_ssl_getResHeader( void )
{
	char	*p_buff;
	int		retCode;
	int		lineLen;

	PVSCH_MSG_LOW("START\n");

	while( 1 )
	{
		/* 1行データを取得する */
		lineLen = func_ssl_fgets(&p_buff);
		if( lineLen <= 0 )
		{
			retCode = -1;
			break;
		}
		/* 先頭が改行の場合 */
		if( p_buff[0] == 0 )
		{
			/* 終了する */
			retCode = 0;
			break;
		}
	}

	PVSCH_MSG_LOW("END\n");
	return retCode;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＨＴＴＰレスポンスボディ取得
 *
 * 機能：
 * 		スケジュール情報配信サーバからのレスポンスを解析し、ダウンロードデータを
 * 		ファイルに保存する。
 * 戻り値：
 * 		 0 : 正常終了
 * 	 	-1 : 異常終了
 */
int func_ssl_getResBody( void )
{
	int 	retCode = 0;
	int 	fileSize = 0;
	char 	*p_buff;
	char 	fileName[64];
	char 	filePath[128];
	char	*p_tmp1;
	char	*p_tmp2;
	int		readSize;
	int		totalSize=0;
	FILE	*fp;						/* ファイルディスクリプタ */

	PVSCH_MSG_LOW("START\n");

	memset(fileName, 0, sizeof(fileName));

	while( 1 )
	{
		/* 1行データを取得する */
		readSize = func_ssl_fgets(&p_buff);
		if( readSize <= 0 )
		{
			retCode = -1;
			break;
		}

		/* ファイル名を検索 */
		if((p_tmp1 = strstr(p_buff, "Content-Disposition: ")) != NULL)
		{
			if((p_tmp1 = strstr(p_buff, "filename=")) != NULL)
			{
				p_tmp1+=9;

				if( strlen(p_tmp1) >= sizeof(fileName))
				{
					sprintf( errMsg, "fileName Length Error(%d)", strlen(p_tmp1));
					commfunc_syslog_msgset( pv_own_name, errMsg );
					PVSCH_MSG_ERR("%s\n", errMsg);
					retCode = -1;
					break;
				}
				else
				{
					/* ファイル名を取得 */
					strcpy(fileName, p_tmp1);
				}
			}
		}

		/* ファイルサイズを検索 */
		if((p_tmp1 = strstr(p_buff, "Content-Length: ")) != NULL)
		{
			p_tmp1+=16;
			fileSize = atoi(p_tmp1);
		}

		/* 先頭が改行の場合 */
		if( p_buff[0] == 0 )
		{
			p_buff += 2;
			break;
		}
	}

/* [20181114] Chg St */
	/* ファイル名、ファイルサイズが取得できた場合、且つ、異常が発生していない場合 */
	if(( fileSize > 0 ) && ( fileName[0] != 0 ) && ( retCode == 0 ))
/* [20181114] Chg Ed */
	{
		/* 保存ファイルパスを生成 */
		sprintf( filePath, "%s%s", PV_PATH_DWL, fileName );

		/* 保存ファイルをオープン */
		if( (fp = fopen(filePath, "wb")) == NULL)
		{
			sprintf( errMsg, "File[%s] Open error", filePath );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			retCode = -1;
		}
		else
		{
			while( 1 )
			{
				/* バッファポインタが設定されている場合 */
				if( p_buff != NULL )
				{
					/* 残りのデータを読み込む */
					readSize = sslReadSize - (p_buff - sslBuff);
				}
				else
				{
					/* データを読み込む */
					readSize = func_ssl_read(&p_buff);
					if( readSize <= 0 )
					{
						retCode = -1;
						break;
					}
				}	

				/* ファイルサイズまでのデータを書き込み */
				if( (totalSize + readSize) > fileSize )
				{
					/* ファイルサイズを超える場合、補正して書き込む */
					fwrite( p_buff, sizeof( unsigned char ), (fileSize-totalSize), fp );
					/* 書き込みサイズを更新 */
					totalSize = fileSize;
					break;
				}
				else
				{
					/* ファイルサイズを超えない場合、全て書き込む */
					fwrite( p_buff, sizeof( unsigned char ), readSize, fp );
					/* 書き込みサイズを更新 */
					totalSize += readSize;
				}
				p_buff = NULL;
			}
			/* ファイルクローズ */
			fclose(fp);

/* [20181114] Chg St */
			/* 書き込みデータサイズ判定、且つ、異常が発生しない場合 */
			if( ( totalSize == fileSize ) && ( retCode == 0 ))
/* [20181114] Chg Ed */
			{
				/* 正常終了 */
				retCode = 0;
				/* ダウンロードファイル名を保持 */
				memcpy(ps_g_SCHCTLFIL->s_dwlinfo.fileName, fileName, sizeof(ps_g_SCHCTLFIL->s_dwlinfo.fileName));
				sprintf( errMsg, "schedule file download[%s] fileSize(%d)", fileName, fileSize);
				commfunc_syslog_msgset( pv_own_name, errMsg );

			}
			else
			{
				/* 異常終了 */
				unlink(filePath);
				sprintf( errMsg, "%s fileSize Error (%d) != (%d)", fileName, fileSize, totalSize);
				commfunc_syslog_msgset( pv_own_name, errMsg );
				PVSCH_MSG_ERR("%s\n", errMsg);
				retCode = -1;
			}
		}
	}
	else
	{
		sprintf( errMsg, "fileSize(%d) or fileName(%s) Error", fileSize, fileName);
		commfunc_syslog_msgset( pv_own_name, errMsg );
		PVSCH_MSG_ERR("%s\n", errMsg);
		/* 異常終了 */
		retCode = -1;
	}

	PVSCH_MSG_LOW("END\n");
	return retCode;
}

/* [20190116] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * SSL通信データファイル書き込み
 *
 * 機能：
 * 		SSLの通信データをファイルに書き込む
 * 戻り値：
 * 		  0 : 正常終了
 * 	 負の数 : 異常終了
 */
int func_ssl_data_file_write( char *preFix, char *p_buff, int length )
{
	struct tm				timeData;					/* 時刻情報 */
	struct timeval			timeVal;					/* 現在積算秒 */
	char					filePath[512];				/* 出力ファイルパス */
	int						retCode = 0;				/* 終了コード */

	/* ファイルディスクリプタがNULLの場合(Openしていない場合) */
	if( sslfp == NULL )
	{
		/* 積算秒を取得 */
		gettimeofday(&timeVal, NULL);
		/* 時刻情報に変換 */
		localtime_r( &timeVal.tv_sec, &timeData );

		timeData.tm_year += 1900;
		timeData.tm_mon+= 1;

		sprintf(filePath, "%s%04d%02d%02d%02d%02d%02d%03d_%s.bin", 
			PV_PATH_DWL, timeData.tm_year, timeData.tm_mon, timeData.tm_mday, timeData.tm_hour, timeData.tm_min, timeData.tm_sec, (timeVal.tv_usec/1000 ), preFix);

		/* 保存ファイルをオープン */
		if( (sslfp = fopen(filePath, "wb")) == NULL)
		{
			sprintf( errMsg, "File[%s] Open error", filePath );
			commfunc_syslog_msgset( pv_own_name, errMsg );
			PVSCH_MSG_ERR("%s\n", errMsg);
			retCode = -1;
		}
		else
		{
			retCode = 0;
			sprintf( errMsg, "File[%s] Open", filePath );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
	}

	/* ファイルを開いている場合、且つ、バッファがNULL以外、且つ、書き込みデータサイズがある場合 */
	if( ( sslfp != NULL ) && ( p_buff != NULL ) && ( length > 0 ))
	{
		/* ファイルに書き込む */
		sprintf( errMsg, "fwrite size=%d", length );
		commfunc_syslog_msgset( pv_own_name, errMsg );
		fwrite( p_buff, sizeof( unsigned char ), length, sslfp );
	}

	return retCode;

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * SSL通信データファイルクローズ
 *
 * 機能：
 * 		SSLの通信データを書き込んだファイルをクローズする
 */
int func_ssl_data_file_close( void )
{
	
	/* ファイルディスクリプタがNULL以外の場合 */
	if( sslfp != NULL )
	{
		/* ファイルクローズ */
		fclose(sslfp);
		sprintf( errMsg, "fclose" );
		commfunc_syslog_msgset( pv_own_name, errMsg );

		/* 初期化 */
		sslfp = NULL;
	}
}
/* [20190116] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 数値->バイトデータ変換
 *
 * 機能：
 * 		引数の数値(num)を引数のバイトサイズ(dataNum)のデータとして出力(strData)する
 * 		<例> num=15, dataNum=3
 * 		    *(strData+2) = 5
 * 		    *(strData+1) = 1
 * 		    *(strData+0) = 0
 */
int func_cmn_num2byte(int num, int dataNum, char *strData)
{
	int						loopCnt;
	int						strLen ;

	strLen = strlen(strData);

	for( loopCnt = dataNum-1; loopCnt >= 0; loopCnt-- )
	{
		strData[loopCnt] = num%10;
		num /= 10;
	}
	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 数値文字->数値変換
 *
 * 機能：
 * 		引数の数値文字列(strData)を引数の文字サイズ(strLen)分、数値として格納する
 * 		<例> strData="150", strLen=3
 * 		    *(byteData+2) = 5
 * 		    *(byteData+1) = 1
 * 		    *(byteData+0) = 0
 */
int func_cmn_str2byte(char *strData, char *byteData, char strLen)
{
	int						loopCnt;
	int						retCode = 0;

	for( loopCnt = 0; loopCnt < strLen; loopCnt++ )
	{
		if( ( strData[loopCnt] >= '0' ) && ( strData[loopCnt] <= '9' ) )
		{
			byteData[loopCnt] = strData[loopCnt] - '0';
		}
		else
		{
			printf("func_cmn_str2byte:[%c]ERROR!!!!\n", strData[loopCnt]);
			retCode = -1;
			break;
		}
	}
	return retCode;
}



/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻情報->文字変換
 *
 * 機能：
 * 		引数の時刻情報(timData)を連続した文字に変換して引数の文字数(setByte)分、
 * 		格納する(setStr)
 */
void func_cmn_set_data_str( struct tm *timData, char *setStr, char setByte )
{
	char 		tmpStr[15];
	int			loopCnt;

	sprintf(tmpStr, "%04d%02d%02d%02d%02d%02d",
			timData->tm_year,timData->tm_mon,timData->tm_mday,
			timData->tm_hour, timData->tm_min, timData->tm_sec);

	for( loopCnt = 0; loopCnt < setByte; loopCnt++)
		setStr[loopCnt] = tmpStr[loopCnt];
}

/* [20250717] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻同期サービス終了
 *
 * 機能：
 * 		出力制御は自動での時刻同期はしない為、時刻同期サービスを停止する。
 * 		格納する(setStr)
 */
int func_time_sync_stop( void )
{
	int		retCode;					/* 戻り値 */
	char	cmdStr[1024];				/* コマンド用ワーク */

	/* 時刻同期サービス名が設定されている場合 */
	if( strlen(s_appTimeSyncName) > 0 )
	{
		/* 時刻同期サービスがchronydの場合 */
		if( 0 == strcmp( s_appTimeSyncName, "chronyd") )
		{
			/* 時刻同期プロセスが動作しているか確認するコマンドを実行 */
			sprintf( cmdStr, "/usr/bin/pgrep -o %s > /dev/null 2>&1", s_appTimeSyncName );
			if( 0 == system( cmdStr ) )
			{
				/* ログを出力 */
				sprintf(errMsg, "func_time_sync_stop(): 時刻同期対象プロセス [ %s ] 動作中", s_appTimeSyncName );
				commfunc_syslog_msgset( pv_own_name, errMsg );

				/* ログを出力 */
				sprintf(errMsg, "func_time_sync_stop(): 時刻同期アプリサービス( %s ) 停止処理開始", s_appTimeSyncName );
				commfunc_syslog_msgset( pv_own_name, errMsg );

				sprintf( cmdStr, "systemctl stop %s.service > /dev/null 2>&1", s_appTimeSyncName );
				retCode = system( cmdStr );
				if( retCode != 0 )
				{
					/* ログを出力 */
					sprintf(errMsg, "func_time_sync_stop(): cmd [ sytemctl stop %s ] 失敗 errno:%d", s_appTimeSyncName, errno );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
				else
				{
					/* ログを出力 */
					sprintf(errMsg, "func_time_sync_stop(): cmd [ systemctl stop %s ] 成功", s_appTimeSyncName );
					commfunc_syslog_msgset( pv_own_name, errMsg );
				}
			}
		}
		else
		{
			sprintf(errMsg, "func_time_sync_stop(): Not supported Service : %s", s_appTimeSyncName );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 設定値ファイル
 *
 * 機能：
 *		EMBASE_CONFIG_FILE で指定された設定ファイルが存在する場合、設定値を取得
 *		する。
 *		・時刻同期サービス名 ............................................ (18)
 *
 * 初期値:
 *		 No | プログラム定義名(定義値)          | 設定ファイル定義名 | 設定範囲
 *		----+-----------------------------------+--------------------+------------
 *		(18)| なし                              | TIMESYNC_SERV_NAME | 64文字以内
 *
 * 戻り値：
 * 		 0: 正常
 *		-1: 異常
 */
int func_get_config( void )
{
	FILE				*fp;									/* ファイルポインタ */
	char				checkData[2048];						/* 読込情報 */
	int					sepCount;								/* 分割数 */
	int					workData;								/* ワークデータ */
	struct st_sepStr	s_sepStr;								/* 分割データ格納領域 */
	
	/* ログを出力 */
	sprintf(errMsg, "func_get_config(): 設定ファイル読込開始");
	commfunc_syslog_msgset( pv_own_name, errMsg );

	/* 設定ファイルをオープン */
	if( (fp = fopen( EMBASE_CONFIG_FILE, "r" )) == NULL )
	{
		/* ログを出力 */
		sprintf(errMsg, "func_get_config(): Configuration File Not Find or Open Error!");
		commfunc_syslog_msgset( pv_own_name, errMsg );

		return( -1 );
	}

	/* ファイルの最後まで繰り返す */
	while( fgets( checkData, 512, fp ) != NULL )
	{
		/*コメント行の除外 */
		if( checkData[0] == '#' ) continue;

		/* 入力を = で区切る *//* 空白、タブ文字はデータとして処理しない */
		sepCount = func_sep_data( checkData, '=', 0x01, &s_sepStr );

		/* データ分割数が2以外の場合は除外 */
		if( sepCount != 2 ) continue;

		/* 分割データ2つ目の文字数が0の場合は除外 */
		if( 0 == strlen( s_sepStr.data[1] ) ) continue;

		/* 分割データ2つ目の文字数が64文字より多い場合は除外 */
		if( 64 <= strlen( s_sepStr.data[1] ) ) continue;

		/* 数値データに変換 */
		workData = atoi( s_sepStr.data[1] );

		if( 0 == strcmp( "TIMESYNC_SERV_NAME", s_sepStr.data[0] ) )
		{
			/* 時刻同期サービス名を設定 */
			sprintf( s_appTimeSyncName, "%s", s_sepStr.data[1] ); 

			/* ログを出力 */
			sprintf(errMsg, "func_get_config(): Time Sync Service Name set [ %s ]", s_appTimeSyncName );
			commfunc_syslog_msgset( pv_own_name, errMsg );
		}
	}
	
	/* ファイルクローズ */
	fclose( fp );

	/* ログを出力 */
	sprintf(errMsg, "func_get_config(): 設定ファイル読込終了");
	commfunc_syslog_msgset( pv_own_name, errMsg );

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
		if( (i_flg & PV_SCH_SEP_FLG_SPCSKP) && (*pt == ' ') && (c_delim != ' ') ) continue;
		if( (i_flg & PV_SCH_SEP_FLG_SPCSKP) && (*pt == '\t') && (c_delim != '\t') ) continue;

		/* 指定されたデリミタの場合 */
		if( *pt == c_delim )
		{
			/* 連続する区切り文字が終わるまで新たなデータとして処理しない場合 */ 
			if( (i_flg & PV_SCH_SEP_FLG_CONTIN) && ( *(pt + 1) == c_delim ) ) continue;
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

/* [20250717] Add Ed */
