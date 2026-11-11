/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * [20251218] 2025.12.18 機能改善
 * ・コメント誤記を修正
 * [20251212] 2025.12.12 機能追加
 * ・一次調整力パラメータ設定に充電応動有無を追加
 * [20251211] 2025.12.11 不具合修正
 * ・模擬周波数設定時の乗数合わせ
 * [20250909] 2025.09.09 機能追加
 * ・充放電計画に一次使用有無を追加
 * [20250807] 2025.08.07 機能追加
 * ・一次調整力（オフライン）対応
 *   関連ファイルを追加
 * [20250106] 2025.01.06 機能追加
 * ・計測蓄積、状態蓄積ファイルの作成ディレクトリを変更
 * [20240701] 2024.07.01 Ph4.2対応
 * ・需給調整市場(三次①、②）、容量市場(実需給)、容量市場(実効性テスト)で
 *   個々のストックを管理するように修正
 * [20240220] 2024.02.20 Ph4対応
 * ・RCVPWINFにSoC不感帯を追加
 * ・CEMSPINFに需給調整市場、容量市場の優先度を追加
 * ・PWPLNINFにピークカット使用有無、容量市場使用有無を追加
 * [20240201] 2024.02.01 Ph4対応
 * ・応答情報にイベント開始時刻を追加。
 * [20231009] 2023.10.09
 * ・需給調整中、且つ、不感帯にいる時にスケジュールが変更になった場合、追従しない不具合の対応。
 * [20230902] 2023.09.02
 * ・ログ出力ファイルの保持数をdefine定義に変更
 * [20230301] 2023.03.01 仕様変更
 * ・BT出力変化量制御の有効/無効を廃止(常に有効)し、変化量を充電/放電方向に分ける
 * [20230224] 2023.02.24 機能追加
 * ・計測蓄積、状態蓄積ファイルの作成有無を追加
 * [20230217] 2023.02.17 結合試験
 * ・需給計画に対する制御を見直し
 * [20221011] 2022.10.11 仕様変更
 * ・遠隔制御処理時、画面表示中対応
 * [20210511] 2021.05.11
 * ・デマンド監視中に積算計測値がリセットされた or 1周した場合の対応
 * [20210128] 2020.01.28
 * ・受電電力計画値機能追加
 * 
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * ファイル関連情報定義
 *
 * 注意：構造体は４バイトアライメントに注意してすること
 */


/*
 * ファイルパス
 */
#define	PV_PATH_EXE			"/home/nksystem/bin/"		/* 収集装置実行モジュール格納ディレクトリ */
#define	PV_PATH_CSV			"/home/nksystem/CSVDATA/"	/* 収集装置データ定義ファイル格納ディレクトリ */
#define	PV_PATH_DAT			"/home/nksystem/ONLDATA/"	/* 収集装置実行時オンラインファイル格納ディレクトリ */
#define	PV_PATH_FTP			"/home/nksystem/FTPDATA/"	/* サーバ送信用ファイル格納ディレクトリ */
#define	PV_PATH_DWL			"/home/nksystem/DWLDATA/"	/* ダウンロードファイル格納ディレクトリ */
#define	PV_PATH_EXT			"/home/nksystem/EXTDATA/"	/* 外部データファイル格納ディレクトリ */
#define	PV_PATH_LOG			"/home/nksystem/log/"		/* ログ格納ディレクトリ */
#define	PV_PATH_REC			"/home/nksystem/RECDATA/"	/* 蓄積ファイル格納ディレクトリ *//* [20250106] Add */
/* [20230529] Add St */
#define	PV_PATH_CERT		"/home/nksystem/CERTDATA/"	/* 証明書ファイル格納ディレクトリ */
#define	PV_PATH_IOT			"/home/nksystem/IOTDATA/"	/* IOT送信用ファイル格納ディレクトリ */
#define	PV_PATH_JSON		"/home/nksystem/JSONDATA/"	/* JSONデータ保持ディレクトリ */

/*
 * AWS-IoT ファイル名称
*/
#define PV_AWS_IOT_ROOT_CA_FILENAME       "root-CA.crt"
#define PV_AWS_IOT_CERTIFICATE_FILENAME   "certificate.pem.crt"
#define PV_AWS_IOT_PRIVATE_KEY_FILENAME   "private.pem.key"
#define PV_AWS_IOT_FILE_PREFIX			  "IOT"
#define PV_AWS_IOT_FILE_PREFIX_TMREC	  "TMREC"
#define PV_AWS_IOT_FILE_PREFIX_SVREC	  "SVREC"
/* [20230529] Add Ed */

/*
 * 収集装置のオンラインファイル数
 */
#define	PV_MAX_FILEINDEX	60				/* オンラインファイルの最大数 */

/*
 * 収集装置オンラインファイル番号
 * ------------------------------
 * （ファイルを追加する場合は lib/src/comm_file.c の修正も行うこと）
 */
#define PV_FILENO_PVSYSINF		1					/* 01:システム管理ファイル */
#define	PV_FILENO_COMLIINF		2					/* 02:通信回線情報ファイル */
#define	PV_FILENO_TMDATINF		3					/* 03:計測情報ファイル */
#define	PV_FILENO_SVDATINF		4					/* 04:装置状態情報ファイル */
#define	PV_FILENO_STDATINF		5					/* 05:通信ステータス情報ファイル */
#define	PV_FILENO_SVMSKINF		6					/* 06:装置状態状変マスク情報ファイル */
#define	PV_FILENO_PVSVRINF		7					/* 07:モニタリングサーバ情報ファイル */
#define	PV_FILENO_TMRECINF		8					/* 08:計測履歴情報ファイル */
#define	PV_FILENO_SVRECINF		9					/* 09:状態履歴情報ファイル */
#define	PV_FILENO_TMAGGINF		10					/* 10:集約計測情報ファイル */
#define	PV_FILENO_SVAGGINF		11					/* 11:集約状態情報ファイル */
#define PV_FILENO_TMMONINF		12					/* 12:計測監視情報 */
#define PV_FILENO_CEMSPINF		13					/* 13:ＣＥＭＳ制御 */
#define PV_FILENO_DEMNDINF		14					/* 14:デマンド監視情報 */
#define PV_FILENO_RCVPWINF		15					/* 15:受電電力一定制御情報 */
#define PV_FILENO_OVRCHINF		16					/* 16:過充電回避制御 */
#define PV_FILENO_MDCTLINF		17					/* 17:モード切替時制御 */
#define PV_FILENO_PWPLNINF		18					/* 18:受電電力計画値情報 [20210128] Add */
#define	PV_FILENO_IOTCNINF		19					/* 19:IoT設定情報 [20230529] Add */
#define PV_FILENO_PGCONINF		20					/* 20:電力会社向け情報ファイル [20230529] Add */
#define	PV_FILENO_BATTEINF		21					/* 21:バッテリー情報ファイル */
#define	PV_FILENO_POWCNINF		22					/* 22:パワーコンディショナ情報ファイル */
#define	PV_FILENO_MLTMTINF		23					/* 23:マルチメータ情報ファイル */
#define	PV_FILENO_REMIOINF		24					/* 24:汎用リモートＩＯ装置情報ファイル */
#define	PV_FILENO_PLCFAINF		25					/* 25:ＰＬＣ情報ファイル */
#define	PV_FILENO_RTCTLINF		31					/* 31:遠隔制御情報保存ファイル */
#define	PV_FILENO_DEMNDFIL		32					/* 32:デマンド制御情報保存ファイル */
#define	PV_FILENO_FCRSTINF		33					/* 33:一次調整力パラメータ設定情報ファイル [20250807] Add */
#define	PV_FILENO_FCRRPINF		34					/* 34:一次調整力レポート蓄積項目情報ファイル [20250807] Add */
#define	PV_FILENO_RPRECFIL		35					/* 35:一次調整力レポート蓄積ファイル [20260515] Add */
#define	PV_FILENO_TMSTSFIL		41					/* 41:計測値保存ファイル */
#define	PV_FILENO_SVSTSFIL		42					/* 42:装置状態保存ファイル */
#define	PV_FILENO_STSTSFIL		43					/* 43:通信ステータス状態保存ファイル */
#define	PV_FILENO_JOHENFIL		44					/* 44:状変蓄積ファイル */
#define	PV_FILENO_TMSTOFIL		45					/* 45:全機器情報保存ファイル */
#define	PV_FILENO_TMRECFIL		46					/* 46:計測情報記録ファイル */
#define	PV_FILENO_SVRECFIL		47					/* 47:状態情報記録ファイル */
#define	PV_FILENO_SVAGGFIL		48					/* 48:集約状態保存ファイル */
#define	PV_FILENO_EMCTLFIL		49					/* 49:エネルギーマネジメント管理ファイル */
#define	PV_FILENO_CTLJHFIL		50					/* 50:制御状変蓄積ファイル */
#define	PV_FILENO_PVCTLFIL		51					/* 51:制御状態保存ファイル */
#define	PV_FILENO_PWPLNFIL		52					/* 52:需給計画値保存ファイル [20210128] Add */
#define	PV_FILENO_SCHCTLFIL		53					/* 53:スケジュール管理情報保存ファイル [20230529] Add */

#define	PV_FILENO_PVSYSLOG		60					/* 60:システムログファイル */


/*
 * 収集装置オンラインファイル（一部ＣＳＶを含む）名称
 */
#define	PV_FNAME_PVSYSINF		"PVSYSINF"			/* 01:システム管理ファイル */
#define	PV_FNAME_COMLIINF		"COMLIINF"			/* 02:通信回線情報ファイル */
#define	PV_FNAME_TMDATINF		"TMDATINF"			/* 03:計測情報ファイル */
#define	PV_FNAME_SVDATINF		"SVDATINF"			/* 04:装置状態情報ファイル */
#define	PV_FNAME_STDATINF		"STDATINF"			/* 05:通信ステータス情報ファイル */
#define	PV_FNAME_SVMSKINF		"SVMSKINF"			/* 06:装置状態状変マスク情報ファイル */
#define	PV_FNAME_PVSVRINF		"PVSVRINF"			/* 07:モニタリングサーバ情報ファイル */
#define	PV_FNAME_TMRECINF		"TMRECINF"			/* 08:計測履歴情報ファイル */
#define	PV_FNAME_SVRECINF		"SVRECINF"			/* 09:状態履歴情報ファイル */
#define	PV_FNAME_TMAGGINF		"TMAGGINF"			/* 10:集約計測情報ファイル */
#define	PV_FNAME_SVAGGINF		"SVAGGINF"			/* 11:集約状態情報ファイル */
#define PV_FNAME_TMMONINF		"TMMONINF"			/* 12:計測監視情報 */
#define PV_FNAME_CEMSPINF		"CEMSPINF"			/* 13:ＣＥＭＳ制御 */
#define PV_FNAME_DEMNDINF		"DEMNDINF"			/* 14:デマンド監視情報 */
#define PV_FNAME_RCVPWINF		"RCVPWINF"			/* 15:受電電力一定制御情報 */
#define PV_FNAME_OVRCHINF		"OVRCHINF"			/* 16:過充電回避制御 */
#define PV_FNAME_MDCTLINF		"MDCTLINF"			/* 17:モード切替時制御 */
#define	PV_FNAME_PWPLNINF		"PWPLNINF"			/* 18:需給計画値情報ファイル [20210128] Add */
#define	PV_FNAME_IOTCNINF		"IOTCNINF"			/* 19:IoT設定情報 [20230529] Add */
#define	PV_FNAME_PGCONINF		"PGCONINF"			/* 20:電力会社向け情報ファイル [20230529] Add */
#define	PV_FNAME_POWCNINF		"POWCNINF"			/* 21:パワーコンディショナ情報ファイル */
#define	PV_FNAME_MLTMTINF		"MLTMTINF"			/* 22:マルチメータ情報ファイル */
#define	PV_FNAME_BATTEINF		"BATTEINF"			/* 23:バッテリー情報ファイル */
#define	PV_FNAME_REMIOINF		"REMIOINF"			/* 24:汎用リモートＩＯ装置情報ファイル */
#define	PV_FNAME_PLCFAINF		"PLCFAINF"			/* 28:ＰＬＣ情報ファイル */
#define	PV_FNAME_RTCTLINF		"RTCTLINF"			/* 31:遠隔制御情報保存ファイル */
#define	PV_FNAME_DEMNDFIL		"DEMNDFIL"			/* 32:デマンド制御情報保存ファイル */
#define	PV_FNAME_FCRSTINF		"FCRSTINF"			/* 33:一次調整力パラメータ設定情報ファイル [20250807] Add */
#define	PV_FNAME_FCRRPINF		"FCRRPINF"			/* 34:一次調整力レポート蓄積項目情報ファイル [20250807] Add */
#define	PV_FNAME_RPRECFIL		"RPRECFIL"			/* 35:一次調整力レポート蓄積ファイル [20260515] Add */
#define	PV_FNAME_TMSTSFIL		"TMSTSFIL"			/* 41:計測値保存ファイル */
#define	PV_FNAME_SVSTSFIL		"SVSTSFIL"			/* 42:装置状態保存ファイル */
#define	PV_FNAME_STSTSFIL		"STSTSFIL"			/* 43:通信ステータス情報保存ファイル */
#define	PV_FNAME_JOHENFIL		"JOHENFIL"			/* 44:状変蓄積ファイル */
#define	PV_FNAME_TMSTOFIL		"TMSTOFIL"			/* 45:全機器情報保存ファイル */
#define	PV_FNAME_TMRECFIL		"TMRECFIL"			/* 46:計測情報記録ファイル */
#define	PV_FNAME_SVRECFIL		"SVRECFIL"			/* 47:状態情報記録ファイル */
#define	PV_FNAME_SVAGGFIL		"SVAGGFIL"			/* 48:集約状態保存ファイル */
#define	PV_FNAME_EMCTLFIL		"EMCTLFIL"			/* 49:エネルギーマネジメント制御管理ファイル */
#define	PV_FNAME_CTLJHFIL		"CTLJHFIL"			/* 50:制御状変蓄積ファイル */
#define	PV_FNAME_PVCTLFIL		"PVCTLFIL"			/* 51:制御状態保存ファイル */
#define	PV_FNAME_PWPLNFIL		"PWPLNFIL"			/* 52:需給計画値保存ファイル [20210128] Add */
#define	PV_FNAME_SCHCTLFIL		"SCHCTLFIL"			/* 53:スケジュール管理情報保存ファイル [20230529] Add */

#define	PV_FNAME_PVSYSLOG		"PVSYSLOG"			/* 60:システムログファイル */

/*
 * 特殊ファイル名称
 */
#define	PV_FNAME_PV_STOP		"stop_flg"			/* 収集装置業務アプリケーション停止ファイル */
#define	PV_FNAME_PV_REBOOT		"reboot_flg"		/* 収集装置再起動ファイル */
#define	PV_FNAME_PV_RECOVERY	"recovery_flg"		/* 収集装置リカバリー実行ファイル */
#define	PV_FNAME_PV_SHUTDOWN	"shutdown_flg"		/* 収集装置シャットダウン実行ファイル */
#define	PV_FNAME_PV_DTSDLOCK	"dtsdlock_flg"		/* データ送信ロックファイル */
#define	PV_FNAME_PV_FALLBACK	"fallback_flg"		/* 縮退運転中ファイル */
#define	PV_FNAME_PV_SILENTRB	"silentrb_flg"		/* サイレントリブートファイル */

#define	PV_FNAME_SFTP_BAT		"sftp_batch"		/* ＳＦＴＰバッチファイル */
#define	PV_FNAME_DWLFILE_LIST	"dwlfile_list"		/* ダウンロードファイルリスト */

#define	PV_FNAME_SFTP_LOG		"sftp_log"			/* ＳＦＴＰログファイル */
#define	PV_FNAME_UDPSND_LOG		"udpsnd_log"		/* ＵＤＰ送信ログファイル */
#define	PV_FNAME_UDPRCV_LOG		"udprcv_log"		/* ＵＤＰ受信ログファイル */
#define	PV_FNAME_SYSTEM_LOG		"pvsys_log"			/* 収集装置ログファイル */
#define	PV_FNAME_SERIAL_LOG		"serial-line"		/* シリアル通信ファイル */
#define	PV_FNAME_JOHEN_LOG		"johen_log"			/* 状変ログファイル */
#define	PV_FNAME_AWSRCV_LOG		"aws_rcv_log"		/* ＡＷＳ受信ログファイル [20230529] Add */
#define	PV_FNAME_IOTCMN_LOG		"iot_cmn_log"		/* ＡＷＳ受信ログファイル [20230529] Add */
#define	PV_LOG_STOCK_NUM		5					/* ログファイル保持数 *//* [20230902] Add */

#define PV_FNAME_GPIO_SCRIPT	"pvgpio.sh"			/* GPIO操作用スクリプト */
#define PV_FNAME_RRESET_SCRIPT	"router_reset.sh"	/* モバイルルータ再起動操作用スクリプト */

#define PV_FNAME_UPDATECNF		"UPDATECNF"			/* 設定変更ファイル */
#define PV_FNAME_REMOTECTL		"REMOTECTL"			/* 遠隔制御ファイル */

#define PV_FNAME_TMREC_PREFIX	"TMREC"				/* 計測記録ファイル */
#define PV_FNAME_SVREC_PREFIX	"SVREC"				/* 状態記録ファイル */
#define PV_FNAME_RPREC_PREFIX	"FCRREPO"			/* 実績データ記録ファイル *//* [20260515] Add */

#define PV_CSVTYPE_NEW			0					/* 新規作成 */
#define PV_CSVTYPE_UPD			1					/* 更新 */

#define	PV_FNAME_FCRREPO_PREFIX	"FCRREPO"			/* 一次調整力レポートファイル *//* [20250807] Add */

/*
 * オンラインファイル情報登録構造体
 */
struct st_fileIndex {
	long			fileNumb;						/* ファイル番号 */
	void			**point;						/* ＭＭＡＰアドレス */
	int				fd;								/* ファイルディスクリプタ */
	int				openStat;						/* オープンステータス */
	long			fileSize;						/* ファイルサイズ（自動設定） */
};

/*
 * オンラインファイル構造体定義
 */
struct st_PVSYSINF {								/* [01]システム管理ファイル */
	char			siteIdName[16];					/* 発電所ID */
	char			gyomuProc[PV_MAX_BUSINESS];		/* 業務プロセス起動フラグ */
	char			yobi1[16 - PV_MAX_BUSINESS];	/* （予備） */
	char			lineLogFlag[PV_MAX_COMLINE];	/* 通信回線ログ出力フラグ */
	char			awsRcvLogFlag;					/* AWS受信ログ出力フラグ [20230529] Chg */
	char			iotSndLogFlag;					/* IOTデータ送信ログ出力フラグ [20230529] Chg */
	char			sslDataFlag;					/* SSL通信データ出力フラグ [20230529] Add */
	char			recFileCre;						/* 計測蓄積・状態蓄積ファイル作成フラグ *//* [20230224] Chg */
	char			yobi2[12 - PV_MAX_COMLINE];		/* （予備） */
	char			dtsdLock;						/* データ送信ロック機能状態 */
	char			johenLock;						/* 状変ロック機能状態 */
	char			yobi4[14];						/* （予備） */
	time_t			lastSetTime;					/* 最終変更時刻 */
	time_t			dayTimeSum;						/* 時刻変更累積(1日) */
	char			sftpStsChk;						/* SFTP送信状態 */
	char			manualStop;						/* 手動停止扱い */
	char			routerReset;					/* モバイルルータリセット要求 */
	char			pvManualMode;					/* PV手動制御モード */
	char			btManualMode;					/* BT手動制御モード */
	char			embaseUseFlg;					/* embaselib使用有無 */
	char			yobi5[2];						/* （予備） */
	char			updateConfFile[64];				/* 設定変更ファイル名 */
	char			yobi3[256];						/* （予備） */
	struct st_prcInfo {
		char			execFlag;					/* 起動指示済みフラグ（管理プロセス更新） */
		char			restFlag;					/* 再起動指示済みフラグ（管理プロセス更新） */
		char			stopFlag;					/* 停止指示済みフラグ（管理プロセス更新） */
		char			runFlag;					/* 動作中フラグ（業務プロセス更新） */
		char			abnFlag;					/* 異常中フラグ（業務プロセス更新） */
		char			wdogFlag;					/* ウォッチドッグフラグ（管理プロセス・業務プロセス更新） */
		char			wdogCnt;					/* ウォッチドッグカウンタ（管理プロセス更新） */
		char			reStCnt;					/* 再起動実施カウンタ（管理プロセス更新） */
		char			ctlInst;					/* 制御指示フラグ */
		char			yobi[7];					/* （未使用） */
		time_t			startTime;					/* プロセス起動時刻（積算秒） */
		int				processID;					/* プロセスＩＤ（子プロセスID） */
		int				yobi2;						/* （未使用） */
		char			procName[16];				/* プロセス名称 */
	} s_procInfo[PV_MAX_PROC];
	struct st_machineInfo {
		int				machineCnt;					/* 装置種別毎の実装機器数 */
		int				maxMachine;					/* 装置最大数 */
		int				maxTm;						/* 計測報数（１装置あたり） */
		int				maxSv;						/* 装置状態情報数（１装置あたり） */
		int				tmIfLink;					/* 計測情報リンク情報 */
		int				svIfLink;					/* 装置状態情報リンク情報 */
		int				svStLink;					/* 装置状態リンク情報 */
		int				stStLink;					/* 通信ステータス状態リンク情報 */
		unsigned char	statFlag[32];				/* 実装/未実装フラグ（装置番号順） */
	} s_machineInfo[PV_MAX_MKIND];
};

struct st_COMLIINF {								/* [02]通信回線情報（１レコード分） */
	int				statFlag;						/* 実装/未実装 */
	char			protoName[12];					/* 通信プロトコル名称 */
	int				deviceType;						/* シリアルデバイスタイプ */
	char			targetName[20];					/* デバイス名称 or IPアドレス */
	int				portNumb;						/* ポート番号 */
	int				baudRate;						/* ボーレート（通信速度） */
	int				dataSize;						/* データサイズ */
	int				parity;							/* パリティ */
	int				stopBit;						/* ストップビット */
	int				rcvTime;						/* 受信応答待ちタイマ */
	int				sndTime;						/* 送信待ちタイマ */
	int				cycleTime;						/* 収集周期 */
	int				retryCount;						/* リトライ回数 */
	int				yobi;							/* （予備） */
};

struct st_TMDATINF {								/* [03]計測情報（１レコード分） */
	short			statFlag;						/* 実装／未実装 */
	short			soKind;							/* 所属装置種別 */
	short			soNumb;							/* 所属装置番号 */
	short			tmNumb;							/* 装置内計測番号 */
	short			tmKind;							/* 計測種別 */
	short			expData;						/* 乗率 */
	short			unitKind;						/* 単位種別 */
	short			yobi;							/* （予備） */
	long long		convData;						/* 変換値 */
	long long		baseData;						/* ベース値 */
	char			tmName[72];						/* 計測名称 */
	int				storeNumb;						/* 蓄積番号 */
	int				iotSndFlag;						/* IOT送信有無 *//* [20230529] Add */
	short			yobi2[4];						/* （予備） */
};

struct st_SVDATINF {								/* [04]装置状態情報（１レコード分） */
	short			statFlag;						/* 実装／未実装 */
	short			soKind;							/* 所属装置種別 */
	short			soNumb;							/* 所属装置番号 */
	short			svNumb;							/* 装置内計測番号 */
	short			svKind;							/* 状変マスク情報 */
	short			yobi[3];						/* 予備 */
	char			svName[72];						/* 状態名称 */
	int				storeNumb;						/* 蓄積番号 */
	int				iotSndFlag;						/* IOT送信有無 *//* [20230529] Add */
	short			yobi2[4];						/* （予備） */
};

struct st_STDATINF {								/* [05]通信ステータス情報（１レコード分） */
	char			stName[72];						/* 状態名称 */
};

struct st_SVMSKINF {								/* [06]装置状態状変マスク情報（１レコード分） */
	char			svMsk;							/* マスク情報（ビット列） */
};

struct st_PVSVRINF {								/* [07]モニタリングサーバ情報ファイル */
	int				stockHour;						/* ファイル保存時間 */
	int				yobi[3];						/* （予備） */
	struct st_svrinfo {
		char			ipAddr[32];					/* サーバＩＰアドレス */
		int				portNumb;					/* ＳＳＨポート番号 */
		int				yobiMain[3];				/* （予備） */
		char			loginName[32];				/* ログイン名 */
		char			sendPath[128];				/* ファイル送信用ファイルパス */
		char			procPath[128];				/* プログラムアップデートファイル確認用ファイルパス */
		char			dataPath[128];				/* データアップデートファイル確認用ファイルパス */
	} s_svrinfo[PV_MAX_SVR];
};

struct st_TMRECINF {								/* [08]計測履歴情報（１レコード分） */
	short			statFlag;						/* 実装／未実装 */
	short			soKind;							/* 所属装置種別 */
	short			soNumb;							/* 所属装置番号 */
	short			tmNumb;							/* 装置内計測番号 */
	int				tmIfLinkPos;					/* 計測情報リンク情報（格納位置情報） */
};

struct st_SVRECINF {								/* [09]状態履歴情報（１レコード分） */
	short			statFlag;						/* 実装／未実装 */
	short			soKind;							/* 所属装置種別 */
	short			soNumb;							/* 所属装置番号 */
	short			svNumb;							/* 装置内計測番号 */
	int				svStLinkPos;					/* 装置状態リンク情報（バイト位置） */
	int				stBit;							/* sv値Bit位置 */
};


struct st_TMAGGINF {								/* [10]集約計測情報ファイル（１レコード分） */
	short			statFlag;						/* 実装／未実装 */
	short			soKind;							/* 所属装置種別(0:固定) */
	short			soNumb;							/* 所属装置番号(1:固定) */
	short			tmNumb;							/* 装置内計測番号(CEMS反映先状態番号) */
	int				tmIfLink;						/* 計測情報リンク情報（格納位置情報） */
	short			yobi;							/* （予備） */
	short			aggTmNum;						/* 演算対象計測情報数 */
	long long		choseiExp;						/* 調整用乗率値 */
	long long		choseiUnit;						/* 調整用単位値 */
	long long		choseiConv;						/* 調整用変換値 */
	long long		choseiBase;						/* 調整用ベース値 */
	char			tmName[72];						/* 合計演算計測名称 */
	struct st_tmaggdata {
		int				tmIfLink;					/* 計測情報リンク情報 */
		int				yobi;						/* （予備） */
		short			plusminus;					/* 加減算フラグ */
		short			soKind;						/* 装置種別 */
		short			soNumb;						/* 装置番号 */
		short			tmNumb;						/* 計測番号 */
		long long		choseiExp;					/* 調整用乗率値 */
		long long		choseiUnit;					/* 調整用単位値 */
		long long		choseiConv;					/* 調整用変換値 */
		long long		choseiBase;					/* 調整用ベース値 */
	} s_tmaggdata[PV_MAX_AGG_TMCNT];
};

struct st_SVAGGINF {								/* [11]集約状態情報ファイル（１レコード分） */
	short			statFlag;               		/* 実装／未実装 */
	short			soKind;							/* 所属装置種別(0:固定) */
	short			soNumb;							/* 所属装置番号(1:固定) */
	short			svNumb;                 		/* 状態番号(CEMS反映先状態番号) */
	char			svName[72];						/* 状態名称 */
	short			aggSvNum;						/* 集約SV対象数 */
	short			logicalOpe;						/* 論理演算子 */
	short			yobi[2];						/* 予備 */
	int				svIfLink;						/* 状態情報リンク情報 */
	int				yobi2;							/* 予備 */
	struct st_svaggdata {
		short		soKind;							/* 装置種別 */
		short		soNumb;							/* 装置番号 */
		short		svstKind;						/* 状態種別 */
		short		svstNumb;						/* 状態番号 */
		short		svVal;							/* SV値 */
		short		yobi1[3];						/* 予備 */
		int			svIfLink;						/* 状態情報リンク情報 */
		int			stPoint;						/* 状態情報リンク情報バイト位置 */
		int			svstMask;						/* 状態情報リンク情報ビットデータ */
		int			onCheckSec;						/* ON時判定時間 */
		int			offCheckSec;					/* OFF時判定時間 */
		int			yobi2[3];						/* 予備 */
	}s_svaggdata[PV_MAX_AGG_SVCNT];
};

struct st_TMMONINF {								/* [12]計測監視情報ファイル（１レコード分） */
	short			statFlag;               		/* 実装／未実装 */
	short			soKind;							/* 所属装置種別(0:固定) */
	short			soNumb;							/* 所属装置番号(1:固定) */
	short			svNumb;							/* 応答状態番号 */
	char			svName[72];						/* 状態名称 */
	int				svIfLink;						/* 状態情報リンク情報 */
	short			soKindMoni;						/* 装置種別 */
	short			soNumbMoni;						/* 装置番号 */
	short			tmNumbMoni;						/* 計測番号 */
	short			yobi;							/* 予備 */
	int				tmIfLink;						/* 計測情報リンク情報 */
	int				moniType;						/* 監視種別 */
	int				baseVal;						/* 基準値 */
	int				gosa;							/* 誤差範囲 */
	int				deadband;						/* 不感帯 */
};

struct st_CEMSPINF {								/* [13]ＣＥＭＳ制御情報保存ファイル */
	int				btControl;						/* BT充放電制御 */
	int				pvControl;						/* PV抑制制御 */
	int				rcvPowConst;					/* 受電電力一定制御 */
	int				ovrChgAvoid;					/* 過充電回避制御 */
	int				dmdControl;						/* デマンド制御 */
	int				shutdownWait;					/* シャットダウン待機時間 */
/* [20230301] Chg St */
	int				btChangeAmtChg;					/* BT出力変化量(充電) */
	int				btChangeAmtDischg;				/* BT出力変化量(放電) */
	int				pvChangeAmtDown;				/* PV出力変化量(抑制) */
	int				pvChangeAmtUp;					/* PV出力変化量(発電) */
/* [20230301] Chg Ed */
	int				ctlCountDown;					/* 制御開始時間 */
/* [20230217] Add St */
	int				arbitragePri;					/* 価格連動制御機能優先度 */
	int				peakCutPri;						/* ピークカット機能優先度 */
	int				bcpPri;							/* BCP機能優先度 */
/* [20230217] Add Ed */
/* [20240220] Add St */
	int				balancePri;						/* 需給調整市場優先度 */
	int				capacityPri;					/* 容量市場優先度 */
/* [20240220] Add Ed */
};

struct st_DEMNDINF {								/* [14]デマンド監視情報保存ファイル */
	short			statFlag;						/* 実装／未実装 */
	short			soKind;							/* 所属装置種別 */
	short			soNumb;							/* 所属装置番号 */
	short			tmNumb;							/* 装置内計測番号 */
	short			expData;						/* 乗率 */
	short			unitKind;						/* 単位種別 */
	short			startUpTime;					/* 監視開始時刻 */
	short			yobi1[2];						/* 未使用 */
	int				tmIfLink;						/* 計測情報リンク情報（監視対象計測値） */
	long long		dmdpowExp;						/* 調整用乗率値（デマンド電力） */
	long long		dmdpowdUnit;					/* 調整用単位値（デマンド電力） */
	long long		choseiExp;						/* 調整用乗率値（監視対象計測値） */
	long long		choseiUnit;						/* 調整用単位値（監視対象計測値） */
	long long		choseiConv;						/* 調整用変換値（監視対象計測値） */
	long long		choseiBase;						/* 調整用ベース値（監視対象計測値） */
	int				dmdContractPow;					/* 契約電力 */
	struct st_thresholdInf {
		short			statFlag;					/* 実装／未実装 */
		int				thresholdVal;				/* デマンド閾値 */
	} s_thresholdInf[PV_MAX_DMD_THAD_LV];
};

struct st_RCVPWINF {								/* [15]受電電力一定制御情報保存ファイル */
	int				BCPSoc;							/* BCP用SOC */
/* [20210128] Del */
	int				dropOffLoadPower;				/* 受電電力脱落負荷量 */
	int				deadZone;						/* 制御不感帯 */
	int				dischgRestartSoC;				/* 放電再開SOC */
	int				chgRestartSoC;					/* 充電再開SOC */
	int				rcvPowAveTime;					/* 受電電力平均時間 */
	int				loadPowAveTime;					/* 負荷電力平均時間 */
	int				rcvPowChangeAmt;				/* 受電電力目標値変化量 */
	int				rcvPowMaxDemand;				/* デマンド閾値 */
	int				rcvPowMinDemand;				/* 受電電力最低電力 */
	int				deadZoneSoC;					/* SoC制御不感帯 *//* [20240220] Add */
	int				yobi[1];						/* （予備） */
	int				pvAddPowInfNum;					/* 閾値数 */
	struct st_pvAddPowInf{
		int			loadPowLimit;					/* 負荷電力閾値 */
		int			addPvPower;						/* PV発電加算値 */
	} s_pvAddPowInf[PV_RCVPW_THR_NUM];
};


struct st_OVRCHINF {								/* [16]過充電回避制御情報保存ファイル */
	int				dropOffLoadPower;				/* 脱落負荷量 */
	int				pvRestartSoc;					/* PV発電再開SoC */
};

struct st_CTLINF {									/* 制御情報 */
		short			statFlagNotCtl;				/* 制御不可SV条件 実装/未実装 */
		short			soKindNotCtl;				/* 制御不可SV条件 制御所属装置種別 */
		short			soNumbNotCtl;				/* 制御不可SV条件 制御所属装置番号 */
		short			svNumbNotCtl;				/* 制御不可SV条件 制御装置内計測番号 */
		short			yobi1[3];					/* （予備） */
		int				svIfLinkNotCtl;				/* 制御不可SV条件 状態情報リンク情報 */
		short			svValNotCtl;				/* 制御不可SV条件 操作方向 */
		short			statFlag;					/* 実装／未実装 */
		short			forceCtl;					/* 強制制御 */
		short			soKindCtl;					/* 制御所属装置種別 */
		short			soNumbCtl;					/* 制御所属装置番号 */
		short			svNumbCtl;					/* 制御装置内計測番号 */
		short			yobi2[3];					/* （予備） */
		int				svIfLinkCtl;				/* 状態情報リンク情報 */
		short			svValCtl;					/* 操作方向 */
		short			oneShot;					/* ワンショット維持時間 */
		short			statFlagRes;				/* 応動監視実装/未実装 */
		short			soKindRes;					/* 応動所属装置種別 */
		short			soNumbRes;					/* 応動所属装置番号 */
		short			svNumbRes;					/* 応動装置内計測番号 */
		short			yobi3[2];					/* （予備） */
		int				svIfLinkRes;				/* 状態情報リンク情報 */
		short			svValRes;					/* 応動方向 */
		short			timeOutRes;					/* 応動時間 */
		short			yobi4[2];					/* （予備） */
};

struct st_MDCTLINF {								/* [17]モード切替時制御情報保存ファイル */
	int					statFlag;					/* 実装／未実装 */
	int					ctlMode;					/* 制御モード */
	int					ctlNum;						/* 制御数 */
	int					yobi;						/* 予備 */
	struct st_CTLINF	s_ctlinfo[PV_MAX_CTL_NUM];	/* 制御情報 */
};

/* [20210128] Add St */
struct st_PWPLNINF {								/* [18]需給計画値情報ファイル */
	int			plnDayNum;							/* 需給計画値格納日数 */
	int			yobi2;								/* 予備 */
	struct st_powDayPln {
		int 	plnDay; 							/* 計画日時 */
		int		yobi;								/* 予備 */
		int 	plnStat[PV_POWPLN_DAY_PERIOD]; 		/* 計画有無 */
		int 	rcvPow[PV_POWPLN_DAY_PERIOD]; 		/* 受電電力計画値 */
		int 	bttPow[PV_POWPLN_DAY_PERIOD]; 		/* 蓄電池充放電電力計画値 */
		int 	bttSoC[PV_POWPLN_DAY_PERIOD]; 		/* 蓄電池SoC計画値 */
		int 	useBcp[PV_POWPLN_DAY_PERIOD]; 		/* BCP使用有無計画値 */
		int 	acUse[PV_POWPLN_DAY_PERIOD]; 		/* 需給調整市場使用 *//* [20230529] Add */
		int 	acSoC[PV_POWPLN_DAY_PERIOD]; 		/* 調整力必要電力量 *//* [20230529] Add */
		int		avlChgPow[PV_POWPLN_DAY_PERIOD];	/* 放電電力上限 *//* [20230529] Add */
		int		avlDischgPow[PV_POWPLN_DAY_PERIOD];	/* 充電電力上限 *//* [20230529] Add */
		int 	peakCutStat[PV_POWPLN_DAY_PERIOD];	/* ピークカット超過有無 *//* [20240216] Add */
		int 	capMktUse[PV_POWPLN_DAY_PERIOD];	/* 容量市場使用 *//* [20240216] Add */
/* [20250807] Add St */
		int		fcrUse[PV_POWPLN_DAY_PERIOD];		/* 一次使用有無 *//* [20250909] Add */
		int		fcrDltKw[PV_POWPLN_DAY_PERIOD];		/* 一次調整力約定△kW *//* [20250807] Add */
/* [20250807] Add Ed */
	}s_powDayPln[PV_POWPLN_MAX_DAY];
};
/* [20210128] Add Ed */

/* [20230529] Add St */
struct st_IOTCNINF {								/* [19]IOT設定情報ファイル */
	int				statFlag;						/* 実装／未実装 */
	int				resorceType;					/* リソース種別 */
	char			hostAddress[256];				/* HostAddress */
	int				portNumb;						/* ポート番号*/
	int				yobi1;							/* 予備 */
	char			deviceId[32];					/* デバイスID */
	char			rcvTopicName[128];				/* 受信トピック名 */
	char			sndTopicName[128];				/* 送信トピック名 */
	int				sndRetry;						/* 応答送信リトライ回数 */
	int				sndCycle;						/* 送信周期 */
	int				stockHour;						/* ファイル保存時間 */
	int				recCycle;						/* 蓄積周期 */
	int				yobi2[32];						/* 予備 */
};

struct st_PGCONINF {								/* [20]電力会社向け情報ファイル */
	int				statFlag;						/* 実装／未実装 */
	int				outCtlType;						/* 出力制御種別 */
	int				yobi4[2];						/* （予備） */
	char			plantId[27];					/* 発電所ＩＤ */
	char			yobi1[5];						/* （予備） */
	char			macAddr[13];					/* ＭＡＣアドレス */
	char			yobi2[3];						/* （予備） */
	char			ipAddr[128];					/* サーバＵＲＬ */
	int				portNumb;						/* ポート番号 */
	int				yobi3;							/* （予備） */
	char			ntpIpAddr[128];					/* ＮＴＰサーバＩＰアドレス */
	int				ntpPortNumb;					/* ＮＴＰサーバポート番号 */
	int				chgTime;						/* 出力変化時間 */
	int				schNonPer;						/* スケジュール未登録時の出力制御値 *//* 未使用 */
	int				powerGrid;						/* 電力会社識別子 */
	char			yobi5[28];						/* （予備） */
};
/* [20230529] Add Ed */

struct st_RTCTLINF {								/* [31]遠隔制御情報保存ファイル */
	char				filename[256];				/* ファイル名 */
	int					ctlMode;					/* 制御モード */
	int					userNo;						/* ユーザ番号 */
	int					ctlNum;						/* 制御数 */
	time_t				limitSec;					/* 期限時刻(秒) */
	int					yobi;						/* 予備 */
	struct st_CTLINF	s_ctlinfo[PV_MAX_CTL_NUM];	/* 制御情報 */
	char				rmtCtlNumb[16];				/* 遠隔制御通し番号 *//* [20221011] Chg */
};

struct st_POWCNINF {								/* [21]パワーコンディショナ情報（１レコード分） */
	int				statFlag;						/* 実装/未実装 */
	int				lineNumb;						/* 所属通信回線番号 */
	int				idNumb;							/* 装置ID番号 */
	int				yobi1;							/* 未使用 */
	char			typeName[32];					/* 型式名称 */
	char			powcnName[72];					/* パワーコンディショナ名称 */
	int				yobi2[2];						/* 未使用 */
	int				stStLink;						/* 通信ステータスリンク情報 */
	int				tmIfLink;						/* 計測リンク情報 */
	int				svIfLink;						/* 装置状態リンク情報 */
	int				svStLink;						/* 装置状態リンク情報 */
	int				ratedCap;						/* 定格容量 */
	int				panelCap;						/* パネル容量 */
};

struct st_MLTMTINF {								/* [22]マルチメータ情報（１レコード分） */
	int				statFlag;						/* 実装/未実装 */
	int				lineNumb;						/* 所属通信回線番号 */
	int				idNumb;							/* 装置ID番号 */
	int				yobi1;							/* 未使用 */
	char			typeName[32];					/* 型式名称 */
	char			mltmtName[72];					/* マルチメータ名称 */
	int				yobi2[2];						/* 未使用 */
	int				stStLink;						/* 通信ステータスリンク情報 */
	int				tmIfLink;						/* 計測リンク情報 */
	int				svIfLink;						/* 装置状態リンク情報 */
	int				svStLink;						/* 装置状態リンク情報 */
};

struct st_BATTEINF {								/* [23]バッテリー情報（１レコード分） */
	int				statFlag;						/* 実装/未実装 */
	int				lineNumb;						/* 所属通信回線番号 */
	int				idNumb;							/* 装置ID番号 */
	int				yobi1;							/* 未使用 */
	char			typeName[32];					/* 型式名称 */
	char			batteName[72];					/* バッテリー名称 */
	int				yobi2[2];						/* 未使用 */
	char			batteId[16];					/* バッテリーID */
	int				batteType;						/* バッテリ用途 *//* [20230529] Add */
	int				batteCap;						/* バッテリ容量(wh) */
	int				maxRatedPwr;					/* 最大定格電力(w) */
	int				maxChargePwr;					/* 最大充電電力(w) */
	int				maxDisChgPwr;					/* 最大放電電力(w) */
	int				socUpper;						/* SoC閾値上限 */
	int				socLower;						/* SoC閾値下限 */
	int				stStLink;						/* 通信ステータスリンク情報 */
	int				tmIfLink;						/* 計測リンク情報 */
	int				svIfLink;						/* 装置状態リンク情報 */
	int				svStLink;						/* 装置状態リンク情報 */
	int				yobi3[2];						/* 未使用 */
};

struct st_REMIOINF {								/* [24]汎用リモートＩＯ装置情報（１レコード分） */
	int				statFlag;						/* 実装/未実装 */
	int				lineNumb;						/* 所属通信回線番号 */
	int				idNumb;							/* 装置ID番号 */
	int				yobi1;							/* 未使用 */
	char			typeName[32];					/* 型式名称 */
	char			remioName[72];					/* 汎用リモートＩＯ装置名称 */
	int				yobi2[2];						/* 未使用 */
	int				stStLink;						/* 通信ステータスリンク情報 */
	int				tmIfLink;						/* 計測リンク情報 */
	int				svIfLink;						/* 装置状態リンク情報 */
	int				svStLink;						/* 装置状態リンク情報 */
};

struct st_PLCFAINF {								/* [28]ＰＬＣ情報（１レコード分） */
	int				statFlag;						/* 実装/未実装 */
	int				lineNumb;						/* 所属通信回線番号 */
	int				idNumb;							/* 装置ID番号 */
	int				yobi1;							/* 未使用 */
	char			typeName[16];					/* 型式名称 */
	char			plcfaName[72];					/* 直流計測装置名称 */
	int				yobi2[2];						/* 未使用 */
	int				stStLink;						/* 通信ステータスリンク情報 */
	int				tmIfLink;						/* 計測リンク情報 */
	int				svIfLink;						/* 装置状態リンク情報 */
	int				svStLink;						/* 装置状態リンク情報 */
};

struct st_thresholdChk {
	short			statFlag;					/* 実装／未実装 */
	int				thresholdVal;				/* デマンド閾値 */
} s_thresholdChk[PV_MAX_DMD_THAD_LV];

struct st_DEMNDFIL {										/* [31]デマンド制御情報保存ファイル */
	struct timeval		startTime;							/* デマンド開始時刻 */
	int					moniFlg;							/* デマンド監視設定 */
	int					tmValPow0;							/* デマンド時限計測値 */
	int					curLvOverFlg[PV_MAX_DMD_THAD_LV];	/* 現在デマンドLv超過フラグ */
	int					estLvOverFlg[PV_MAX_DMD_THAD_LV];	/* 予測デマンドLv超過フラグ */
	int					tmAbnFlag;							/* 計測異常検出 */
	struct st_DEMNDINF	s_updatedmd;
	int					tmValPowLast;						/* デマンド時限計測値前回値 *//* [20210511] Add */
	int					tmValOffset;						/* デマンド時限オフセット値 *//* [20210511] Add */
};

/* [20250807] Add St */
struct st_FCRSTINF {								/* [33]一次調整力パラメータ設定情報 */
	int				readFlg;						/* 起動時読込フラグ */
	int				baseFreq;						/* 基準周波数 *//* [20251218] Chg */
	int				variationRate;					/* 調定率 *//* [20251218] Chg */
	int				deadBand;						/* 不感帯 */
	int				deadBandLower;					/* 不感帯(下限) */
	int				deadBandUpper;					/* 不感帯(上限) */
	int				freqDevtThrsld;					/* 異常判定周波数 */
	int				freqDevtThrsldLower;			/* 異常判定周波数(下限) */
	int				freqDevtThrsldUpper;			/* 異常判定周波数(上限) */
	int				faultDuration;					/* 異常時出力継続時間 */
	int				ratedPower;						/* 定格出力 */
	int				dischgPowerChgAmount;			/* 放電時出力変化量 */
	int				chgPowerChgAmount;				/* 充電時出力変化量 */
	int				deadBandResType;				/* 不感帯応動種別 */
	int				chgResponsiveEnabled;			/* 充電応動有無 *//* [20251212] Add */
};

struct st_FCRRPINF {								/* [34]一次調整力レポート蓄積項目情報 */
	int				readFlg;						/* 起動時読込フラグ */
	int				dataMax;						/* データ数 */
	struct st_fcrrpData {
		short		dataNumb;						/* データ番号 */
		short		mKind;							/* 装置種別 */
		short		mNumb;							/* 装置番号 */
		short		tmNumb;							/* 計測番号 */
		int			tmIfLinkPos;					/* 計測情報リンク情報（格納位置情報） *//* [20260515] */
	} s_fcrrpData[PV_FCRRP_DATA_MAX];
};
/* [20250807] Add Ed */

/* [20260515] Add St */
struct st_RPRECFIL {								/* [35]一次調整力レポート蓄積ファイル */
	int					curPoint;					/* 記録位置（現在位置） */
	int					tcpPoint;					/* サーバ送信プロセス最終処理位置 */
	struct st_REPOREC {
		time_t			sec;						/* 計測記録時間（積算秒） */
		suseconds_t		uSec;						/* 計測記録時間（マイクロ秒） */
		struct st_RPDATA{
			int				tmFlag;					/* 計測フラグ */
			int				tmVal;					/* 計測値 */
		}s_rpData[PV_FCRRP_DATA_MAX];
	}s_reporec[PV_RPREC_DATA_NUM];
};
/* [20260515] Add Ed */

struct st_TMSTSFIL {								/* [41]計測状態保存ファイル（１レコード分） */
	int				tmFlag;							/* 現在値：計測フラグ */
	int				tmVal;							/* 現在時：計測値 */
	int				avltmFlag;						/* 平均値：計測フラグ */
	int				avltmVal;						/* 平均時：計測値 */
};

struct st_SVSTSFIL {								/* [42]装置状態保存ファイル（１レコード分） */
	char			svStat;							/* 装置状態（ビット列） */
};

struct st_STSTSFIL {								/* [43]通信ステータス状態保存ファイル（１レコード分） */
	char			stStat;							/* 通信ステータス状態（ビット列） */
};

struct st_JOHENFIL {								/* [44]状変蓄積ファイル */
	int				curPoint;						/* 状変記録位置（現在位置） */
	int				tcpPoint;						/* サーバ送信プロセス最終処理位置 */
	int				iotPoint;						/* IOT送信最終処理位置 *//* [20230529] Chg */
	int				outPoint;						/* ログ出力最終処理位置 */
	struct st_JDATA {
		time_t			sec;						/* 状変発生時間（積算秒） */
		suseconds_t		uSec;						/* 状変発生時間（マイクロ秒） */
		short			mKind;						/* 装置種別 */
		short			mNumb;						/* 装置番号 */
		short			svstKind;					/* 状態種別 */
		short			svstNumb;					/* 状態番号 */
		short			svstStat;					/* 装置状態 */
		short			yobi[3];					/* 予備 */
		int				svstLink;					/* 情報ファイルポインタ */
		int				yobi2;						/* 予備 */
		short			maskFlag;					/* 状変マスク情報（0:マスク無し／1:マスク対象） */
		short			yobi3[3];					/* 予備 */
	} s_jData[PV_MAX_JOHEN];
};

struct st_TMSTOFIL {								/* [45]全情報保存ファイル */
	struct st_STSTSFIL s_ststsfil[(PV_MAX_STDATA-1)/8+1];	/* 通信ステータス処理用領域 */
	struct st_SVSTSFIL s_svstsfil[(PV_MAX_SVDATA-1)/8+1];	/* 装置状態処理用領域 */
	struct st_TMSTSFIL 	s_tmstsfil[PV_MAX_TMDATA];	/* 計測値保存領域 */
};

struct st_TMRECFIL {								/* [46]計測情報記録ファイル */
	int					curPoint;					/* 記録位置（現在位置） */
	int					tcpPoint;					/* サーバ送信プロセス最終処理位置 */
	struct st_TMREC {
		time_t			sec;						/* 計測記録時間（積算秒） */
		suseconds_t		uSec;						/* 計測記録時間（マイクロ秒） */
		struct st_TMDATA{
			int				tmFlag;					/* 計測フラグ */
			int				tmVal;					/* 計測値 */
		}s_tmData[PV_MAX_TM_HISTORY];
	}s_tmhst[PV_TMREC_DATA_NUM];
};

struct st_SVRECFIL {								/* [47]状態情報記録ファイル */
	int					curPoint;					/* 記録位置（現在位置） */
	int					tcpPoint;					/* サーバ送信プロセス最終処理位置 */
	struct st_SVREC {
		time_t			sec;						/* 計測記録時間（積算秒） */
		suseconds_t		uSec;						/* 計測記録時間（マイクロ秒） */
		char			svStat[PV_MAX_SV_HISTORY];	/* 状態情報保持領域 */
	}s_svrec[PV_SVREC_DATA_NUM];
};

struct st_SVAGGFIL {								/* [48]集約状態保存ファイル（１レコード分） */
	int				svStat;							/* 現在SV状態 */
	struct st_svaggstat {
		struct timeval	detectionTim;				/* 検出時間 */
		struct timeval	fixTim;						/* 確定時間 */
		int				svStat;						/* 現在SV状態 */
	}s_svaggstat[PV_MAX_AGG_SVCNT];
};

struct st_EMCTLFIL {								/* [49]エネルギーマネジメント管理ファイル */
	int							actCntdown;			/* 制御開始カウントダウン */
	int							cemsMode;			/* CEMSモード */
	int							preTmFlg;			/* 前回計測情報 */
	int							reCalcFlg;			/* 再計算フラグ *//* [20230220] Add */
	int							yobi1;				/* 未使用 */
	int							rcvpDisStopFlg;		/* 連系中放電停止フラグ */
	int							rcvpChgStopFlg;		/* 連系中充電停止フラグ */
	int							yobi2;				/* 未使用 */
	int							pvReqPow;			/* PV出力制御(w)要求(目標)値 */
	int							pvSetPow;			/* PV出力制御(0.01%)要求(目標)値 */
	int							pvSetPer;			/* PV出力制御(0.01%)指示値(制御演算からの指示値) */
	int							bttReqPow;			/* バッテリ充放電電力要求(目標)値 */
	int							bttSetPow;			/* バッテリ充放電電力指示値(制御演算からの指示値) */
	int							preSchUpdataFlg;	/* スケジュール更新フラグ *//* [20231009] Add */
	int							yobi3[2];			/* 未使用 */
	int							pvSprFlg;			/* 未使用 *//* PV抑制有無 */
	int							avlDischgPow;		/* 蓄電池 放電可能最大値 */ 
	int							avlChgPow;			/* 蓄電池 充電可能最大値 */ 
	int							ovrChgPvStop;		/* 過充電回避制御PV発電停止フラグ */
	int							pvRatedPow;	 	    /* PV発電容量合計 */
	time_t						s1_timer_sec;		/* S1:停止タイマ開始時刻 */
	int							disChgStopFlg;		/* 放電停止フラグ */
	int							chgStopFlg;			/* 充電停止フラグ */
	int							cmExamPri;			/* 容量市場優先フラグ(0：実需給優先、1:実効性テスト優先） *//* [20240701] Add */
/* [20230529] Add St */
	char			deviceId[32];						/* デバイスID */
	struct st_ORDERPARA {								/* Web-APIパラメータ */
		char			eventId[PV_JSON_DATALEN_MAX];		/* イベントID */
		char			orderStr[PV_JSON_DATALEN_MAX];		/* 指令種別 */
		int				orderType;							/* 指令種別 */
		char			categoryStr[PV_JSON_DATALEN_MAX];	/* 商品区分 *//* [20240221] Add */
		int				category;							/* 商品区分 *//* [20240221] Add */
		int				cm_exam;							/* 実効性テストフラグ *//* [20240701] Add */
		char			deviceId[PV_JSON_DATALEN_MAX];		/* デバイスID */
		char			timeStamp[PV_JSON_DATALEN_MAX];		/* タイムスタンプ */
		char			timeUnit[PV_JSON_DATALEN_MAX];		/* 時間単位 */
		char			valueUnit[PV_JSON_DATALEN_MAX];		/* 指令値単位 */
		char			eventTimeStr[PV_JSON_DATALEN_MAX];	/* イベント開始時刻(文字列) */
		struct timeval	eventStTimeSec;						/* イベント開始時刻(sec) */
		struct timeval	eventEdTimeSec;						/* イベント終了時刻(sec) */
		char		duration[PV_JSON_DATALEN_MAX];		/* 継続時間 */
		int			timeConv;							/* 秒への変換値 */
		int			valueConv;							/* Wへの変換値 */
		struct st_interval{								/* 制御期間情報 */
			int				statFlag;							/* 実装/未実装 */
			int				setFlag;							/* 実行フラグ */
			struct timeval	duration;							/* 継続時間 */
			int				value;								/* 充放電電力(kW) */
			int				base;								/* 基準値(kW) */
			int				yobi[7];							/* 予備 */
			struct timeval	startSec;							/* インターバル開始時刻(sec) */
			struct timeval	endSec;								/* インターバル開始時刻(sec) */
		}s_interval[AWS_INTERVAL_MAX];
	}s_rr_para[AWS_ORDER_STOCK];								/* 三次調整力①② *//* [20240701] Chg */
/* [20240701] Add St */
	struct st_ORDERPARA 	s_fcr_para[AWS_ORDER_STOCK];		/* 一次調整力 */
	struct st_ORDERPARA 	s_frr_para[AWS_ORDER_STOCK];		/* 二次調整力①② */
	struct st_ORDERPARA 	s_cm_para[AWS_ORDER_STOCK];			/* 容量市場 */
	struct st_ORDERPARA 	s_cm_exam_para[AWS_ORDER_STOCK];	/* 容量市場(実効性テスト) */
/* [20240701] Add Ed */
	struct st_ORDERPARA 		s_contactorpara;		/* 遮断機制御 */
	struct st_evtdata{									/* 制御データ */
		int			intervalPos;						/* 制御期間情報*/
		int			targetBase;							/* 充放電基準値 */
		int			targetPow;							/* 目標値(上げ：(-)充電、下げ：(+)放電) */
		int			yobi[6];							/* 予備 */
	}s_evtdata_rr;																		/* [20240701] Chg */
/* [20240701] Add St */
	struct st_evtdata		s_evtdata_fcr;				/* 制御データ(一次調整力) */
	struct st_evtdata		s_evtdata_frr;				/* 制御データ(二次調整力①②) */
	struct st_evtdata		s_evtdata_cm;				/* 制御データ(容量市場：実需給) */
	struct st_evtdata		s_evtdata_cm_exam;			/* 制御データ(容量市場：実効性テスト) */
/* [20240701] Add Ed */
	int						resOrder[AWS_NOTIFY_STOCK];	/* 応答送信順序（Queue構造がよかったが、EMSを停止するとキューポインタが壊れてしまい順序が崩れる為、送信順序で対応） */
	struct st_resinfo{									/* 応答情報 */
		int					resId;							/* 応答ID */
		int					sndstatus;						/* 送信状況(未設定/送信待ち/送信完了) */
		int					resResut;						/* 応答結果 */
		int					sndResut;						/* 送信結果 *//* 未使用 */
		int					resSndCnt;						/* 応答送信回数 */
		int					retryCnt;						/* 送信リトライ回数（0：無制限、>0：有限 */
		int					messageKind;					/* メッセージ種別(応答/開始/終了) */
		struct timeval		eventTimeSec;					/* イベント時刻 */
		int					yobi[7];						/* 予備 */
		char				eventId[PV_JSON_DATALEN_MAX];	/* イベントID */
		char				errMsg[PV_MSG_SIZE_LOG];		/* 応答メッセージ */
		struct timeval		eventStTimeSec;					/* イベント開始時刻(sec) *//* [20240207] Add */
	}s_resinfo[AWS_NOTIFY_STOCK];
	struct st_setdata{									/* 制御データ */
		int			actMode;							/* 動作モード */
		int			orderType;							/* 指令種別 */
		int			yobi[32];							/* 予備 */
	}s_setdata_rr;																		/* [20240701] Chg */
/* [20240701] Add St */
	struct st_setdata		s_setdata_fcr;				/* 制御データ(一次調整力) */
	struct st_setdata		s_setdata_frr;				/* 制御データ(二次調整力①②) */
	struct st_setdata		s_setdata_cm;				/* 制御データ(容量市場：実需給) */
	struct st_setdata		s_setdata_cm_exam;			/* 制御データ(容量市場：実効性テスト) */
/* [20240701] Add Ed */
/* [20230529] Add Ed */
/* [20250807] Add St */
	struct st_fcrFreqSimData							/* 模擬周波数データ */
	{
		int					targetNumb;					/* 制御対象 */
		int 				simDataCount;				/* 登録データ数 */
		int					fcrDltKw;					/* 約定△kW */
		int					freqMeasurementNum;			/* 乗数 *//* [20251211] 模擬周波数の乗数合わせ */
		struct timeval		s_stTime_timeval;			/* 開始時刻 */
		struct st_fcrFreqSimList
		{
			int				duration;					/* 継続時間 */
			int				freqValue;					/* 周波数 */
			struct timeval	s_stTime_timeval;			/* 開始時刻(timeval) */
		}s_fcr_freq_sim_list[PV_FCR_FREQ_SIM_LIST_MAX];
	}s_fcr_freq_sim_data;
	int						fcrCalcDataType;			/* 一次調整力算出データType */
/* [20250807] Add Ed */
};

struct st_CTLJHFIL {								/* [50]制御状変蓄積ファイル */
	int				curPoint;						/* 制御情報記録位置（現在位置） */
	int				tcpPoint;						/* サーバ送信プロセス最終処理位置 */
	int				outPoint;						/* ログ出力最終処理位置 */
	int				iotPoint;						/* IOT送信最終処理位置 *//* [20230529] Add */
	struct st_CTLDATA {
		time_t			sec;						/* 状変発生時間（積算秒） */
		suseconds_t		uSec;						/* 状変発生時間（マイクロ秒） */
		int				mKind;						/* 装置種別 */
		int				mNumb;						/* 装置番号 */
		int				ctlKind;					/* 状態種別 */
		int				ctlNumb;					/* 状態番号 */
		int				ctlStat;					/* 装置状態 */
		int				ctlLink;					/* 情報ファイルポインタ */
		int				maskFlag;					/* 状変マスク情報（0:マスク無し／1:マスク対象） */
		int				userNo;						/* ユーザ番号 */
	} s_ctlData[PV_MAX_CTLINF];
	char				rmtCtlNumb[16];				/* 遠隔制御通し番号 *//* [20221011] Add */
};

struct st_PVCTLFIL {								/* [51]制御状態保存ファイル（１レコード分） */
	int					ctlState;					/* 処理状態(0:未処理、1:処理中、2:終了) */
	int					ctlMode;					/* 制御モード */
	struct timeval		startTime;					/* 開始時刻 */
	int					ctlIdx;						/* 処理インデックス */
	int					userNo;						/* ユーザ番号 */
	int					ctlNum;						/* 制御数 */
	int					retCode;					/* 制御結果 */
	char				filename[256];				/* ファイル名 */
	struct st_CTLINF	s_ctlinfo[PV_MAX_CTL_NUM];	/* 制御情報 */
	struct st_CTLMNG {
		int					ctlProc;				/* 制御過程（1:セット、2:ワンショット、3:応答） */
		int					soKindJhn;				/* 制御状変装置種別 */
		int					soNumbJhn;				/* 制御状変装置番号 */
		int					svNoJhn;				/* 制御状変状態番号 */
		int					svValJhn;				/* 制御状変制御値 */
		int					svIfLinkJhn;			/* 状態情報リンク情報 */
		struct st_CTLREC {
			struct timeval	setTime;				/* 処理時刻 */
			struct timeval	retTime;				/* 処理終了時刻 */
			int				retCode;				/* 処理結果 */
		}s_ctlrec[3];								/* 0:セット、1:ワンショット、2:応動 */
	} s_ctlmng[PV_MAX_CTL_NUM];
};

/* [20210128] Add St */
struct st_PWPLNFIL {										/* [52]需給計画値保存ファイル */
	int 				plnDayNow;							/* 受電電力当日 */
	int					yobi1;								/* 予備 */
	struct st_lastDayPln {
		int				statFlag;							/* 保存値有無(0:一度も計画値がない、1:保存値がある） */
		int				yobi;
		int 			lastPowPln[PV_POWPLN_DAY_PERIOD]; 	/* 受電電力計画保存値 */
		int 			lastBioPln[PV_POWPLN_DAY_PERIOD]; 	/* バイオ発電電力 */
		int 			lastPvPln[PV_POWPLN_DAY_PERIOD]; 	/* PV発電電力保存値 */
	}s_lastDayPln[PV_POWPLAN_IDX_MAX];
	struct st_PWPLNINF	powplndata[PV_POWPLAN_IDX_MAX];		/* 受電電力計画情報 */
};
/* [20210128] Add Ed */

/* [20230529] Add St */
struct st_SCHCTLFIL {								/* [47]スケジュール管理情報保存ファイル */
	int						statFlag;				/* 実装／未実装 */
	int						outCtlType;				/* 出力制御種別 */
	int						yobi3[2];				/* （予備） */
	char					plantId[27];			/* 発電所ＩＤ */
	char					yobi[5];				/* 予備 */
	int						powerGrid;				/* 電力会社識別子 */
	int						chgTime;				/* 出力変化時間 */
	int						chkSumNumb;				/* チェックサム番号 */
	int						idRegist;				/* ID登録結果 */
	int						configErr;				/* 設定値異常 */
	int						getSchErr;				/* スケジュール取得異常 */
	int						schNonPer;				/* スケジュール未登録時の出力制御値(%)を設定 *//* 未使用 */
	int						yobi2[32];				/* 予備 */
	struct st_dwlinfo {
		char				kubun[5];				/* スケジュール区分 */
		char				yobi1[3];				/* 予備 */
		int					comErrCode;				/* 通信エラーコード */
		int					yobi2[5];				/* 予備 */
		char				comErrMsg[128];			/* 通信エラーメッセージ */
		char				appErrCode[8];			/* エラーファイルのエラーコード */
		char				appErrMsg[128];			/* エラーファイルのエラーメッセージ */
		char 				fileName[64];			/* ファイル名 */
		int					yobi3[32];				/* 予備 */
		int					getMon;					/* 最新ダウンロード月 */
		int					yobi4;					/* 予備 */
		int					upFlgCurr;				/* 更新フラグ現在（０～９、初期値：0xFF） */
		int					upFlgNext;				/* 次回取得更新フラグ（０～９、初期値：0xFF） */
		int					upSchRtyMin;			/* 更新スケジュールリトライ時間 */
		time_t				upSchReqTime;			/* 更新スケジュール要求時刻(次回アクセスorリトライ時に使用) */
		int					yobi5[32];				/* 予備 */
	}s_dwlinfo;

	int						buffSelect;				/* バッファ使用面 */
	int						dataPos;				/* 現在のデータ位置 */
	struct st_schinfo {
		int					schYear;				/* 格納スケジュール年 */
		int					updateKomaEnd;			/* 最終更新コマ *//* 使用しないかも */
		int					yobi[4];				/* （未使用） */
		struct st_schdata{
			int		update;							/* 更新フラグ *//* 使用しないかも */
			int		schType;						/* スケジュール種別（0:なし、1:固定、2:更新）  */
			int		data;							/* 出力抑制（％） */
//			int		chgLimit;						/* 充電制限（％） */
//			int		disLimit;						/* 放電制限（％） */
//			int		pvLimit;						/* 太陽光制限（％） */
		}s_schdata[17568];							/* スケジュールデータ(366日×48データ) */
	}s_schinfo[PV_SCH_BUFF_NUM];
};
/* [20230529] Add Ed */

struct st_PVSYSLOG {								/* [60]システムログファイル */
	int				curPoint;						/* 記録位置（現在位置） */
	int				outPoint;						/* 最終処理位置 */
	int				yobPoint[2];					/* 予備ポインタ */
	struct st_LOGDATA {
		time_t			sec;						/* 状変発生時間（積算秒） */
		suseconds_t		uSec;						/* 状変発生時間（マイクロ秒） */
		char			logMsg[PV_MSG_SIZE_LOG];	/* ログメッセージ */
	} s_logData[PV_MAX_LOG];
};

