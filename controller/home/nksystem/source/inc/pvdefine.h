/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * [20260514] 2026.05.14
 * ・一次調整力の系統周波数を平均値を算出し使用するように修正
 * [20260130] 2026.01.30
 * ・TM制御判定周波数、異常継続時間の削除
 * [20260109] 2026.01.09
 * ・一次調整力レポート蓄積終了時間を追加
 * [20251218] 2025.12.18 機能修正
 * ・SV定義を移動
 * [20251215] 2025.12.15 機能変更
 * ・履歴蓄積情報最大数を100に変更
 * [20251215] 2025.12.15 機能追加
 * ・充電応動有無、充電応動無効、有効の定義値を追加
 * [20251204] 2025.12.04
 * ・一次調整力SoC調整を追加
 * [20251027] 2025.10.27
 * ・出力制御ユニット用ＰＬＣ（ModbusTCP）に対応
 * [20250807] 2025.08.07
 * ・一次調整力（オフライン）対応
 * [20250421] 2025.04.21
 * ・Tesla社製蓄電池 Controls-V2に対応
 * [20240701] 2024.07.01 Ph4.2対応
 * ・需給調整市場(三次①、②）、容量市場(実需給)、容量市場(実効性テスト)で
 *   個々のストックを管理するように修正
 * [20240216] 2024.02.16 Ph4対応
 * ・SVに容量市場使用、SoC回復充電を追加
 * [20240201] 2024.02.01 Ph4対応
 * ・指令のストック数を2→6(現在、未来1～5)に変更
 * [20230824] 2023.08.24
 * ・ACシステムからのorder_typeを「DER3」から「AC」に変更
 * ・出力制御オンライン対応
 * [20230302] 2023.03.02
 * ・停電のSV名称をシャットダウンタイマ開始に変更
 * [20230125] 2023.01.25
 * ・需給計画設定のSVを追加
 * [20210128] 2020.01.28
 * ・受電電力計画値機能追加
 * [20210113] 2020.01.13
 * ・PV発電禁止(連系中)のPV抑制を変化量に従って抑制するように修正
 * [20210112] 2020.01.12
 * ・受電電力目標値の切り替え時にを変化量に従って移行するように修正
 * [20180814] 2018.08.14 <#131> [仕様変更]
 * ・収集装置のSV番号、TM番号の変更
 * [20180712] 2018.07.12 <ST_BUG005> [仕様変更]
 * ・ファイル保持時間の上限を60日(1440h)に変更
 *   ※現地のSIMが使用できなかった場合に削除せずに保持しておきたい為。
 *     <参考>空き容量50GB
 *           2分に2ファイル=約700KB
 *           1ｈ = 30 * 約700KB = 約21000KB
 *           52428800KB / 21000KB = 2496ｈ
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 共通デファイン
 *
 * 注意：
 * ファイルに関するファイル番号およびファイル名称の定義情報は統一性を保つ必要か
 * ら下記のファイルでまとめて定義しています
 * [ pvfile.h ]
 *
 * シリアルポートに関する定義情報は統一性を保つ必要から下記のファイルでまとめて
 * 定義しています
 * [ pvserial.h ]
 */

/*
 * 管理プロセス名
 */
#define	PV_PROCNAME_NKMANAGE	"nkmanage"		/* 管理プロセス */

/*
 * 通信プロセス名
 * （通信回線情報ファイルに内容によりnkmanageプロセスに起動されるプロセス名） 
 */
#define	PV_PROCNAME_SMA4			"pcssma4"		/* ＳＭＡ製パワーコンディショナ（Data Manager M） ModbusTCP通信  */
#define	PV_PROCNAME_NKIOPLC1		"nkioplc1"		/* 収集装置外部Ｉ／Ｏ用ＰＬＣ（三菱電気製MELSEC iQ-F）通信プロセス１ */
#define	PV_PROCNAME_TESLA1   		"btttesl1"		/* Tesla製リチウムイオン電池 通信プロセス１（ModbusTCP通信） */
#define	PV_PROCNAME_TESLA2   		"btttesl2"		/* Tesla製リチウムイオン電池 Controls-v2 通信プロセス２（ModbusTCP通信） *//* [20250421] Add */
#define	PV_PROCNAME_CEC1			"remiocec1"		/* 中央電子社製 接点伝送装置プロトコル（SMNP通信） */
#define PV_PROCNAME_TAKEMULTIM2		"takemlm2"		/* タケモトデンキ製マルチメータ通信プロセス２（ModbusRTU） */
#define	PV_PROCNAME_HUAWEI2			"pcshwei2"		/* ＨＵＡＷＥＩ製パワーコンディショナ（SmartLogger経由）通信プロセス２ [20170619] Add */
#define	PV_PROCNAME_SANKEN2			"pcshwei2"		/* サンケン電気製パワーコンディショナ（SmartLogger経由）通信プロセス２ [20170619] Add */
#define PV_PROCNAME_MITSUMULTIM1	"mitsumlm1"		/* 三菱電機製EcoMonitor通信プロセス１（ModbusRTU） */
#define	PV_PROCNAME_COMPOWAYF1		"omronkm1"		/* スマート計測監視機器通信プロセス */
#define	PV_PROCNAME_SCHNEIDER1		"mlmschn1"		/* シュナイダー製マルチメータ通信プロセス１（ModbusTCP通信） *//* [20230529] Add */
#define	PV_PROCNAME_SCHPLC1		    "schplc1"		/* 出力制御ユニット用ＰＬＣ通信プロセス（ModbusTCP通信） *//* [20251027] Add */
#define	PV_PROCNAME_PPMSMA1		    "ppmsma1"		/* SMA製PPM（ModbusTCP通信） *//* [20251101] Add */

/*
 * 業務プロセス名
 * （管理ファイルの内容によりnkmanageプロセスにより起動される業務プロセス名）
 */
#define	PV_PROCNAME_BUSINESS01	"repoedit"		/* MicroGridシステムサーバ送信編集プロセス */
#define	PV_PROCNAME_BUSINESS02	"nkftpsnd"		/* MicroGridシステムサーバ送信プロセス */
#define	PV_PROCNAME_BUSINESS03	"emctlmng"		/* エネルギーマネジメント制御管理プロセス */
#define	PV_PROCNAME_BUSINESS04	"pvschmng"		/* 出力制御情報管理プロセス *//* [20230529] Add */
#define	PV_PROCNAME_BUSINESS05	"nkiotsnd"		/* IoT-Coreデータ送信プロセス *//* [20230529] Add */
#define	PV_PROCNAME_BUSINESS06	"yobi"			/* （予備） */
#define	PV_PROCNAME_BUSINESS07	"yobi"			/* （予備） */
#define	PV_PROCNAME_BUSINESS08	"yobi"			/* （予備） */
#define	PV_PROCNAME_BUSINESS09	"yobi"			/* （予備） */
#define	PV_PROCNAME_BUSINESS10	"yobi"			/* （予備） */

/*
 * サポートする通信プロトコル名
 */
#define	PV_COMPRO_SMA4			"SMA4"			/* ＳＭＡ製パワーコンディショナ（Data Manager M） ModbusTCP通信 [20190507] Add */
#define PV_COMPRO_NKIOPLC1		"NKIOPLC1"		/* 収集装置外部Ｉ／Ｏ用ＰＬＣ（SLMP:Ethernet） [20180510] Add */
#define	PV_COMPRO_TESLA1	    "TESLA1"	    /* Tesla製リチウムイオン電池（ModbusTCP） */
#define	PV_COMPRO_TESLA2	    "TESLA2"	    /* Tesla製リチウムイオン電池 Controls-v2（ModbusTCP） *//* [20250421] Add */
#define	PV_COMPRO_CEC1		    "CEC1"			/* 中央電子製 接点伝送装置（SNMP通信） */
#define PV_COMPRO_TAKEMULTIM2	"TAKEMULTIM2"	/* タケモトデンキ製マルチメータ（ModbusRTU:EIA485） */
#define	PV_COMPRO_HUAWEI2		"HUAWEI2"		/* ＨＵＡＷＥＩ製パワーコンディショナ（SmartLogger経由）ModbusTCP通信 [20170619] Add */
#define	PV_COMPRO_SANKEN2		"SANKEN2"		/* サンケン電気製パワーコンディショナ（SmartLogger経由）ModbusTCP通信 [20170619] Add */
#define PV_COMPRO_MITSUMULTIM1	"MITSUMULTIM1"	/* 三菱電機製EcoMonitor（ModbusRTU:EIA485） */
#define	PV_COMPRO_COMPOWAYF1	"COMPOWAYF1"	/* COMPOWAY/F（スマート計測監視機器KM1シリーズ用）＊KE1シリーズを除く */
#define	PV_COMPRO_SCHNEIDER1	"SCHNEIDER1"	/* シュナイダー製マルチメータ（ModbusTCP） *//* [20230529] Add */
#define	PV_COMPRO_SCHPLC1	    "SCHPLC1"		/* 出力制御ユニット用ＰＬＣ（ModbusTCP） *//* [20251027] Add */
#define	PV_COMPRO_PPMSMA1	    "PPMSMA1"		/* SMA製PPM（ModbusTCP） *//* [20251101] Add */

/*
 * 運転モード関連
 */
#define	PV_MODE_STOP			0				/* 運転モード：停止 */
#define	PV_MODE_RUN				1				/* 運転モード：運転 */
#define	PV_MODE_STP_TO_RUN		5				/* 運転モード：運転遷移 */
#define	PV_MODE_RUN_TO_STP		6				/* 運転モード：停止遷移 */

/*
 * 制御モード状態
 */
#define	PV_CTL_STOP				0				/* 制御モード：S1:停止 */
#define	PV_CTL_IDLE				1				/* 制御モード：S2:待機 */
#define	PV_CTL_OFFGRID			2				/* 制御モード：S3:自立運転中 */
#define	PV_CTL_ONGRID			3				/* 制御モード：S4:連系運転中 */
#define	PV_CTL_ERR_IDLE			4				/* 制御モード：S5:待機(障害) */
#define	PV_CTL_ERR_OFFGRID		5				/* 制御モード：S6:自立運転中(障害) */
#define	PV_CTL_ERR_ONGRID		6				/* 制御モード：S7:連系運転中(障害) */
#define	PV_CTL_MAX				7				/* 制御モード数 */



/*
 * 動作環境関連情報
 */
#define	PV_MTYPE_OTHER			0				/* 一般環境 */
#define	PV_MTYPE_OPSA_DEB6		1				/* OpenBlocks(Debian 6)環境 */
#define	PV_MTYPE_OPSA_DEB7		2				/* OpenBlocks(Debian 7)環境 */

/*
 * プロセスの起動情報
 */
#define	PV_PROC_NEWSTART		0				/* 新規起動 */
#define	PV_PROC_RESTART			1				/* 再起動 */

/*
 * ウォッチドッグ関連情報
 */
#define	PV_WDOG_CYCLE			5				/* ウォッチドッグ周期（秒） */
#define	PV_WDOG_LMTCNT			120				/* ウォッチドッグカウント（実際の時間=ウォッチドッグ周期xカウント） */
#define	PV_WDOG_RES_RESET		3600			/* プロセスリスタート回数リセット時間（秒） */

/*
 * 収集装置の対応機種に関する定義
 */
#define	PV_MAX_MKIND			20				/* 装置種別最大数 */
#define	PV_MKIND_PVSYS			0				/* 装置種別：収集装置本体 */
#define	PV_MKIND_BATTE			1				/* 装置種別：バッテリー */
#define	PV_MKIND_POWCN			2				/* 装置種別：パワーコンディショナ */
#define	PV_MKIND_MLTMT			3				/* 装置種別：マルチメータ */
#define	PV_MKIND_REMIO			4				/* 装置種別：汎用リモートＩＯ装置 */
#define	PV_MKIND_PLCFA			5				/* 装置種別：ＰＬＣ装置 */
#define	PV_MKIND_LAST			PV_MKIND_PLCFA	/* 装置種別：終端 */

//#define PV_MKIND_VIRTL			1				/* 装置種別：仮想ＴＭ／ＳＶ */

/*
 * 装置番号の最大数
 */
#define PV_MAX_DEVICE			256				/* 装置番号の最大数 */
#define	PV_MAX_PVSYS			1				/* 収集装置最大数 */
#define	PV_MAX_BATTE			1				/* バッテリー最大数 */
#define	PV_MAX_POWCN			90				/* パワーコンディショナ最大数 */
#define	PV_MAX_MLTMT			10				/* マルチメータ最大数 */
#define	PV_MAX_REMIO			4				/* 汎用リモートＩＯ装置最大数 */
#define	PV_MAX_PLCFA			4				/* ＰＬＣ装置最大数 */

//#define	PV_MAX_VIRTL			1				/* 仮想ＴＭ／ＳＶ装置最大数 */

/*
 * 機器毎のＴＭ・ＳＶ・ＳＴ情報数の定義
 */
#define	PV_MAX_STPOS			32				/* 通信ステータス最大状態数 */
#define	PV_MAX_PVSYS_STPOS		0				/* 収集装置本体関連の最大通信ステータス数 */
#define	PV_MAX_PVSYS_CTLTMPOS	100				/* 制御ＴＭ数 */
#define	PV_MAX_PVSYS_AGGTMPOS	100				/* 集約ＴＭ数 */
#define	PV_MAX_AGG_TMCNT		50				/* 集約ＴＭの上限数 */
/* 収集装置本体関連の最大ＴＭ数 */
#define	PV_MAX_PVSYS_TMPOS		( PV_MAX_PVSYS_CTLTMPOS + PV_MAX_PVSYS_AGGTMPOS )

#define	PV_MAX_PVSYS_CTLSVPOS	80				/* 収集装置本体関連の制御状態数 */
#define	PV_MAX_PVSYS_MONSVPOS	20				/* 収集装置本体関連のTM監視状態数 */
#define	PV_MAX_PVSYS_AGGSVPOS	156				/* 収集装置本体関連の集約状態数 */
#define	PV_MAX_AGG_SVCNT		90				/* 集約ＳＶの上限数 */
/* 収集装置本体関連の最大危機状態数 */
#define	PV_MAX_PVSYS_SVPOS		( PV_MAX_PVSYS_CTLSVPOS + PV_MAX_PVSYS_MONSVPOS + PV_MAX_PVSYS_AGGSVPOS )


#define	PV_MAX_BATTE_TMPOS		150				/* バッテリー（全社共通）１台あたりの最大ＴＭ数 */
#define	PV_MAX_BATTE_SVPOS		256				/* バッテリー（全社共通）１台あたりの最大機器状態数 */
#define	PV_MAX_POWCN_TMPOS		60				/* パワーコンディショナ（全社共通）１台あたりの最大ＴＭ数 */
#define	PV_MAX_POWCN_SVPOS		512				/* パワーコンディショナ（全社共通）１台あたりの最大機器状態数 */
#define	PV_MAX_MLTMT_TMPOS		110				/* マルチメータ１台あたりの最大ＴＭ数 */
#define	PV_MAX_MLTMT_SVPOS		256				/* マルチメータ１台あたりの最大機器状態数 */
#define	PV_MAX_REMIO_TMPOS		64				/* 汎用リモートＩＯ装置１台あたりの最大ＴＭ数 [20130412] Add */
#define	PV_MAX_REMIO_SVPOS		256				/* 汎用リモートＩＯ装置１台あたりの最大機器状態数 [20130412] Add */
#define	PV_MAX_PLCFA_TMPOS		200				/* PLC装置１台あたりの最大ＴＭ数 [20150106] Add */
#define	PV_MAX_PLCFA_SVPOS		256				/* PLC装置１台あたりの最大機器状態数 [20150106] Add */


/* 計測値保持最大数 */
#define	PV_MAX_TMDATA			( (PV_MAX_PVSYS_TMPOS * PV_MAX_PVSYS) + \
								  (PV_MAX_BATTE_TMPOS * PV_MAX_BATTE) + \
								  (PV_MAX_POWCN_TMPOS * PV_MAX_POWCN) + \
								  (PV_MAX_MLTMT_TMPOS * PV_MAX_MLTMT) + \
								  (PV_MAX_REMIO_TMPOS * PV_MAX_REMIO) + \
								  (PV_MAX_PLCFA_TMPOS * PV_MAX_PLCFA) \
								)
/* 装置状態保持最大数 */
#define	PV_MAX_SVDATA			( (PV_MAX_PVSYS_SVPOS * PV_MAX_PVSYS) + \
								  (PV_MAX_BATTE_SVPOS * PV_MAX_BATTE) + \
								  (PV_MAX_POWCN_SVPOS * PV_MAX_POWCN) + \
								  (PV_MAX_MLTMT_SVPOS * PV_MAX_MLTMT) + \
								  (PV_MAX_REMIO_SVPOS * PV_MAX_REMIO) + \
								  (PV_MAX_PLCFA_SVPOS * PV_MAX_PLCFA) \
								)
/* 通信ステータス状態保持最大数 */
#define	PV_MAX_STDATA			( (PV_MAX_PVSYS + \
								   PV_MAX_BATTE + \
								   PV_MAX_POWCN + \
								   PV_MAX_MLTMT + \
								   PV_MAX_REMIO + \
								   PV_MAX_PLCFA ) * PV_MAX_STPOS )


/*
 * 履歴蓄積情報最大数
 */
#define	PV_MAX_TM_HISTORY		100		/* 計測履歴最大計測数 *//* [20251215] Chg */
#define	PV_MAX_SV_HISTORY		100		/* 計測履歴最大状態数 *//* [20251215] Chg */

#define	PV_FILE_TMREC_TIME		70		/* 計測情報蓄積秒数（秒）:収集データファイル周期(60秒)+予備(10秒) */
#define	PV_FILE_SVREC_TIME		70		/* 状態情報蓄積秒数（秒）:収集データファイル周期(60秒)+予備(10秒) */
#define	PV_TMREC_DATA_NUM		(PV_FILE_TMREC_TIME * 10 )	/* 計測情報蓄積内部データ数 */
#define	PV_SVREC_DATA_NUM		(PV_FILE_SVREC_TIME * 10 )	/* 状態情報蓄積内部データ数 */
#define	PV_RPREC_DATA_NUM		120		/* 実績データ蓄積秒数（秒）:収集データファイル周期(60秒)+再起動のタイミングが59秒となった場合を考慮 *//* [20260515] Add */


/*
 * 制御状態を管理可能な最大装置数
 */
#define	PV_MAX_CTRL_INFO			2				/* 制御管理情報数 */

/* 
 * 最大数の定義（回線数・機器数・情報数） 
 */
#define	PV_MAX_PROC				20				/* プロセス数（管理プロセスを除く） */
#define	PV_MAX_COMLINE			10				/* 通信回線最大数 */
#define	PV_MAX_BUSINESS			10				/* 業務プロセス最大数 */

#define	PV_MAX_RESTART			5				/* プロセスリスタート回数 */
#define	PV_MAX_STOCK			5				/* 計測情報蓄積回数（５回＝５分） */
#define	PV_MAX_SVR				2				/* モニタリングサーバ数 */

/*
 * 収集装置における状態・計測情報の最大数定義
 */
#define	PV_MAX_JOHEN			3000			/* 状変情報最大保持数 */
#define	PV_MAX_LOG				3000			/* システムログ情報最大保持数 */
#define	PV_MAX_CTLINF			3000			/* 制御情報最大保持数 */

/*
 * サーバ向け送信情報蓄積時間
 */
#define PV_MAX_FILE_STOCK_HOUR	1440				/* 24時間 x 60日 = 1440時間 */

/*
 * 表示端末装置の表示桁最大数
 */
#define	PV_MAX_PVDSP_COLS		20				/* 最大２０桁 */

/* [20250807] Add St */
/* レポートデータ */
#define	PV_MAX_REPO_DATA_NO		10		/* レポートデータNo 対応している最大数 */
/* [20250807] Add Ed */

/*
 * 収集装置本体の状態情報に関する定義
 */
#define	PV_SYS_SV_SYSRUN					1			/* 収集装置異常：装置運転 */
#define	PV_SYS_SV_SYSERR					2			/* 収集装置異常：装置異常 */
#define	PV_SYS_SV_OPE_NUMB_1				3			/* 収集装置：1系運転 */
#define	PV_SYS_SV_OPE_NUMB_2				4			/* 収集装置：2系運転 */

#define PV_SYS_SV_MANUALSTOP				9			/* 収集装置異常：手動停止 */
#define	PV_SYS_SV_SHUKUTAI					10			/* 収集装置異常：縮退運転 */
#define PV_SYS_SV_PROC_FILE_FOUND			11			/* プログラムアップデートファイル検出 */
#define PV_SYS_SV_DATA_FILE_FOUND			12			/* データアップデートファイル検出 */
#define PV_SYS_SV_REBOOT_ST					13			/* 再起動開始 */
#define PV_SYS_SV_SHUTDOWN_RSV				14			/* シャットダウン予約開始 */
/* [20230529] Add St */
#define PV_SYS_SV_NO_SCH					15			/* 収集装置異常：運用スケジュールなし */
#define PV_SYS_SV_FIXED_SCH					16			/* 収集装置異常：固定スケジュール運用 */
#define PV_SYS_SV_UPDATE_SCH				17			/* 収集装置異常：更新スケジュール運用 */
#define PV_SYS_SV_CLOCK_ERR					18			/* 収集装置異常：時刻同期エラー */
#define	PV_SYS_SV_IOTRCVERR					19			/* 収集装置異常：IOT通信異常(受信) */
#define	PV_SYS_SV_IOTSNDERR					20			/* 収集装置異常：IOT通信異常(送信) */
#define PV_SYS_SV_COMMERR					21			/* 収集装置異常：通信異常 */
/* [20230529] Add Ed */
#define	PV_SYS_SV_CONFIGERR					22			/* 収集装置異常：設定異常 */
#define	PV_SYS_SV_CEMSPARAMSET				23			/* 収集装置異常：CEMS制御設定 */
#define	PV_SYS_SV_PLANPARAMSET				24			/* 収集装置異常：需給計画設定 *//* [20230125] Add */

#define	PV_SYS_SV_MODE_REMOTE				27			/* 遠隔制御 *//* [20251218] Add */
#define PV_SYS_SV_UPS_SHUTDOWN_EXE			28			/* UPSシャットダウン指示 */
#define PV_SYS_SV_SHUTDOWN_EXE				29			/* シャットダウン指示 */
#define PV_SYS_SV_PV_RUN_START				30			/* 未使用 *//* PV運転開始制御 */
#define PV_SYS_SV_PV_RUN_STOP				31			/* 未使用 *//* PV運転停止制御 */
#define PV_SYS_SV_PV_ZERO_PER				32			/* 未使用 *//* PV発電停止 */

#define PV_SYS_SV_ONGRID_PLAN				36			/* 需給計画(連系)なし */
#define PV_SYS_SV_BCP_USE					37			/* BCP使用可否 */
#define PV_SYS_SV_OFFGRID_CHG_BAN			38			/* 未使用 *//* PV発電禁止(自立中) */
#define PV_SYS_SV_ONGRID_DISCHG_BAN			39			/* BT放電禁止(連系中) */
#define PV_SYS_SV_ONGRID_CHG_BAN			40			/* BT充電/PV発電禁止(連系中) */
#define PV_SYS_SV_MODE_STOP					41			/* S1:停止中 */
#define PV_SYS_SV_MODE_IDLE					42			/* S2:待機中 */
#define PV_SYS_SV_MODE_OFFGRID				43			/* S3:自立中 */
#define PV_SYS_SV_MODE_ONGRID				44			/* S4:連系中 */
#define PV_SYS_SV_MODE_ERR_IDLE				45			/* S5:待機中（障害発生） */
#define PV_SYS_SV_MODE_ERR_OFFGRI			46			/* S6:自立中（障害発生） */
#define PV_SYS_SV_MODE_ERR_ONGRID			47			/* S7:連系中（障害発生） */

#define PV_SYS_SV_BATTE_CTL					52			/* BT充放電制御 */
#define PV_SYS_SV_PV_CTL					53			/* 未使用 *//* PV抑制制御 */
#define PV_SYS_SV_RCVPWR_CTL				54			/* 受電電力一定制御 */
#define PV_SYS_SV_OVRCHG_CTL				55			/* 未使用 *//* 過充電回避制御 */
#define PV_SYS_SV_DEMAND_CTL				56			/* デマンド制御 */
/* [20230529] Add St */
#define PV_SYS_SV_OPE_SOC_CTL				57			/* SoC制御 */
/* [20240221] Chg Start */
#define PV_SYS_SV_OPE_SUPPLY_DEMAND			59			/* 需給調整市場動作中 */
#define PV_SYS_SV_OPE_SUPPLY_DEMAND_SOC		60			/* 需給調整市場SoC */
#define PV_SYS_SV_OPE_CAPACITY_MARKET_EXAM	61			/* 容量市場(実効性テスト)動作中 *//* [20240701] Chg */
#define PV_SYS_SV_OPE_CAPACITY_MARKET_SOC	62			/* 容量市場SoC */
#define PV_SYS_SV_OPE_CAPACITY_MARKET		63			/* 容量市場動作中 *//* [20240701] Add */
//#define									63			/* 調整力公募動作中 */
//#define									64			/* 調整力公募SoC */
/* [20240221] Chg End */
/* [20230529] Add Ed */


//#define	PV_SYS_SV_MODE_OFF_TO_ON_1			61			/* 未使用 *//* 連系切替ステップ1 */
//#define	PV_SYS_SV_MODE_OFF_TO_ON_2			62			/* 未使用 *//* 連系切替ステップ2 */
//#define	PV_SYS_SV_MODE_OFF_TO_ON_3			63			/* 未使用 *//* 連系切替ステップ3 */
//#define	PV_SYS_SV_MODE_ON_TO_OFF			64			/* 未使用 *//* 自立運転切替 */
/* [20251218] Del */
/* [20251218] Chg St */
/* [20250807] Add St */
#define	PV_SYS_SV_FCR_CTL_SOC_AJT			64			/* 一次調整力SoC調整 */	/* [20251204] 一次調整力SoC調整 Add */
#define	PV_SYS_SV_FCR_CTL					65			/* 一次調整力制御 */
#define	PV_SYS_SV_FCR_CTL_FREQ_ERR			66			/* 一次調整力異常動作中 */
#define	PV_SYS_SV_REPORT_DATA_ACUM			67			/* 一次調整力レポートデータ蓄積 */
#define	PV_SYS_SV_FREQ_SIM_TEST				68			/* 模擬周波数 */
#define PV_SYS_SV_FCR_CHG_RES_ENABLED		69			/* 充電応動有無 *//* [20251215] Add */
/* [20250807] Add Ed */
/* [20251218] Chg Ed */

#define PV_SYS_SV_DMD_MONI					70			/* 未使用 *//* デマンド監視(開始/停止) */
#define PV_SYS_SV_DMD_CUR_LV1				71			/* 未使用 *//* 現在デマンド電力Lv1超過 */
#define PV_SYS_SV_DMD_CUR_LV2				72			/* 未使用 *//* 現在デマンド電力Lv2超過 */
#define PV_SYS_SV_DMD_CUR_LV3				73			/* 未使用 *//* 現在デマンド電力Lv3超過 */
#define PV_SYS_SV_DMD_CUR_LV4				74			/* 未使用 *//* 現在デマンド電力Lv4超過 */

#define PV_SYS_SV_DMD_EST_LV1				76			/* 未使用 *//* 予測デマンド電力Lv1超過 */
#define PV_SYS_SV_DMD_EST_LV2				77			/* 未使用 *//* 予測デマンド電力Lv2超過 */
#define PV_SYS_SV_DMD_EST_LV3				78			/* 未使用 *//* 予測デマンド電力Lv3超過 */
#define PV_SYS_SV_DMD_EST_LV4				79			/* 未使用 *//* 予測デマンド電力Lv4超過 */

#define PV_SYS_SV_PV_STATE					81			/* 未使用 *//* PV発電状態 */

#define	PV_SYS_SV_ERR_OFFGRID_SV			198			/* 重故障SV(S3:自立運転) */
#define	PV_SYS_SV_ERR_ONGRID_SV				199			/* 重故障SV(S4:連系運転) */
#define	PV_SYS_SV_ERR_CMMON					200			/* 重故障(共通) */

#define	PV_SYS_SV_ERR_IDLE					202			/* 重故障(S2:待機) */
#define	PV_SYS_SV_ERR_OFFGRID				203			/* 重故障(S3:自立運転) */
#define	PV_SYS_SV_ERR_ONGRID				204			/* 重故障(S4:連系運転) */

#define	PV_SYS_SV_NOT_CONDITON_OFF_GRID		211			/* 自立切替不可 */
#define	PV_SYS_SV_NOT_CONDITON_ON_GRID		214			/* 連系切替不可 */
#define	PV_SYS_SV_ONGRID_BIO_CHECK			213			/* 未使用 *//* 連系運転時BG制御条件 */
#define	PV_SYS_SV_OFFGRID_BIO_CHECK			214			/* 未使用 *//* 自立運転時BG制御条件 */
#define	PV_SYS_SV_ONGRID_BT_CHECK			215			/* 連系運転時BT制御条件 */
#define	PV_SYS_SV_ONGRID_PV_CHECK			216			/* 未使用 *//* 連系運転時PV制御条件 */
#define	PV_SYS_SV_OFFGRID_BT_CHECK			217			/* 自立運転時BT制御条件 */
#define	PV_SYS_SV_OFFGRID_PV_CHECK			218			/* 未使用 *//* 自立運転時PV制御条件 */

#define	PV_SYS_SV_SHUTDOWN_TIMER_START		219			/* シャットダウンタイマ開始 *//* [20230302] Chg */
#define	PV_SYS_SV_BT_ONGRID_STATE			220			/* BT連系設定 */
#define	PV_SYS_SV_BT_OFFGRID_STATE			221			/* BT自立設定 */
#define	PV_SYS_SV_BT_OPERATION				222			/* バッテリ操作権限(1:可、0:不可) */
#define	PV_SYS_SV_BT_IDLE_STATE				223			/* BT待機設定 */
#define	PV_SYS_SV_BT_CON_ERR				224			/* 蓄電システム通信異常 */
#define	PV_SYS_SV_52R_CON_ERR				225			/* 52R取得機器通信異常 *//* [20230529] Add */
#define	PV_SYS_SV_52R_ABNORMAL				226			/* 52R取得機器故障 *//* [20230529] Add */
#define	PV_SYS_SV_52R_ON_STATE				227			/* 52R ON状態 *//* [20230529] Add *//* [20230814] Chg */
#define	PV_SYS_SV_52R_OFF_STATE				228			/* 52R OFF状態 *//* [20230529] Add *//* [20230814] Chg */



#define	PV_SYS_SV_SYSTEM_ERR				231			/* システム状態 */
#define	PV_SYS_SV_PV_ALLSTOP				232			/* PV運転停止 *//* pcshwei2で参照しているが、実質未使用 */
#define	PV_SYS_SV_PV_ALLSART				233			/* PV運転開始 *//* pcshwei2で参照しているが、実質未使用 */
#define	PV_SYS_SV_SYSTEM_CLEAN				234			/* システム状態(正常) */
#define	PV_SYS_SV_UPS_LOWBATT				235			/* UPSシャットダウン条件 */
#define	PV_SYS_SV_MEGAPACK_COMM				236			/* MEGAPACK通信異常状態 *//* [20231024] Add */

#define PV_SYS_SV_PROTECTIVE_BLOCK_1		240			/* 保護ブロック1 */
#define PV_SYS_SV_PROTECTIVE_BLOCK_2		241			/* 保護ブロック2 */


/*
 * 収集装置本体の計測情報に関する定義
 */

#define PV_SYS_TM_AVE_RCV_POW				1		/* 受電電力平均値 */
#define PV_SYS_TM_AVE_LOAD_POW				2		/* 負荷電力平均値 */
#define PV_SYS_TM_BT_SET_POW				3		/* 蓄電池制御指示 */
#define PV_SYS_TM_AVE_GRID_FREQ				4		/* 周波数平均値 *//* [20260514] Add */
#define PV_SYS_TM_BIO_SET_POW				4		/* バイオ発電指令値 *//* 未使用 */
#define PV_SYS_TM_PV_SET_PER				5		/* PV出力抑制指示 *//* 未使用 */
#define PV_SYS_TM_PLAN_RCV_POW			    6		/* 需給計画(受電電力) */
#define PV_SYS_TM_PLAN_REMAIN		    	7		/* 需給計画(残電力量) *//* [20230529] Add */
#define PV_SYS_TM_CHG_LIMIT			    	8		/* 充電電力上限 *//* [20230529] Add */
#define PV_SYS_TM_DISCHG_LIMIT		    	9		/* 放電電力上限 *//* [20230529] Add */

#define PV_SYS_TM_RCV_POW_TARGET		   11		/* 受電電力現在目標値 */
#define PV_SYS_TM_BT_REQ_POW			   12		/* 蓄電池制御目標値 */
#define PV_SYS_TM_BIO_REQ_POW			   13		/* バイオ発電目標値 *//* 未使用 */
#define PV_SYS_TM_PV_REQ_POW			   14		/* PV発電電力目標値 *//* 未使用 */
#define PV_SYS_TM_PV_REQ_PER			   15		/* PV出力抑制目標値 *//* 未使用 */
#define PV_SYS_TM_SCH_PER				   16		/* 出力制御スケジュール値 *//* [20230824] */
#define PV_SYS_TM_SCH_CHG_TIME			   17		/* 出力制御出力変化時間 *//* [20230824] */



#define PV_SYS_TM_DMD_CUR_POW			   21		/* 現在デマンド電力 */
#define PV_SYS_TM_DMD_EST_POW			   22		/* 予測デマンド電力 */
#define PV_SYS_TM_DMD_EST_POW_MAX		   23		/* 予測デマンド電力最大値 */
/* [20250807] Add St */
#define PV_SYS_TM_FCRCTL_BASE_FREQ				   24		/* 基準周波数 */
#define PV_SYS_TM_FCRCTL_FREQ_VARIATION_RATE	   25		/* 調定率 */
#define PV_SYS_TM_FCRCTL_DEAD_BAND				   26		/* 不感帯 */
#define PV_SYS_TM_FCRCTL_DEAD_BAND_LOWER		   27		/* 不感帯(下限) */
#define PV_SYS_TM_FCRCTL_DEAD_BAND_UPPER		   28		/* 不感帯(上限) */
#define PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD		   29		/* 異常判定周波数 */
#define PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD_LOWER	   30		/* 異常判定周波数(下限) */
#define PV_SYS_TM_FCRCTL_FREQ_DEVT_THRSLD_UPPER	   31		/* 異常判定周波数(上限) */
#define PV_SYS_TM_FCRCTL_ERR_FAULT_DURATION		   32		/* 異常時出力継続時間 */
#define PV_SYS_TM_FCRCTL_RATED_POWER			   33		/* 定格出力 */
#define PV_SYS_TM_FCRCTL_DEAD_BAND_RES_TYPE		   34		/* 不感帯応動種別 */
#define PV_SYS_TM_FCRCTL_CONTRACT_DLT_POW		   35		/* 約定△kW */
/* [20260130] Del */
/* [20250807] Add Ed */




#define PV_SYS_TM_RCV_POW				  101		/* 受電電力 */
#define PV_SYS_TM_RCV_POW_WH			  102		/* 受電電力量 */
#define PV_SYS_TM_BATTE_POW				  103		/* 蓄電池有効電力 */
#define PV_SYS_TM_BATTE_SOC				  104		/* 蓄電池SoC */
#define PV_SYS_TM_BIO_POW				  105		/* バイオガス発電電力合計 *//* 未使用 */
/* [20250807] Add St */
#define PV_SYS_TM_GRID_FREQ				  105		/* 系統周波数 */
/* [20250807] Add Ed */
#define PV_SYS_TM_PV_POW				  106		/* PV発電電力合計 *//* 未使用 */
#define PV_SYS_TM_LOAD_POW_OFF			  107		/* 負荷電力合計(自立) *//* 未使用 */
#define PV_SYS_TM_LOAD_POW				  108		/* 負荷電力合計 */
#define PV_SYS_TM_LOAD_POW_WH			  109		/* 負荷電力量 */
#define PV_SYS_TM_BATTE_POW_WH			  110		/* 蓄電池充放電電力量 */
#define PV_SYS_TM_BT_AVL_CHG			  111		/* 蓄電池最大充電電力 */
#define PV_SYS_TM_BT_AVL_DIS			  112		/* 蓄電池最大放電電力 */
#define PV_SYS_TM_BT_REMAIN_WH			  113		/* 蓄電池残電力量 */


/*
 * 通信ステータスの状態情報に関する定義
 */
#define	PV_ST01_NONRESPONSE		1				/* 通信ステータス01：通信無応答異常 */
#define	PV_ST02_UPBCCERR		2				/* 通信ステータス02：上りＢＣＣ／ＣＲＣ異常 */
#define	PV_ST03_UPCOMMANDERR	3				/* 通信ステータス03：上りコマンド異常 */
#define	PV_ST04_UPDATAERR		4				/* 通信ステータス04：上りデータ異常 */
#define	PV_ST05_YOBI			5				/* 通信ステータス05：（予備） */
#define	PV_ST06_YOBI			6				/* 通信ステータス06：（予備） */
#define	PV_ST07_YOBI			7				/* 通信ステータス07：（予備） */
#define	PV_ST08_YOBI			8				/* 通信ステータス08：（予備） */
#define	PV_ST09_YOBI			9				/* 通信ステータス09：（予備） */
#define	PV_ST10_YOBI			10				/* 通信ステータス10：（予備） */
#define	PV_ST11_YOBI			11				/* 通信ステータス11：（予備） */
#define	PV_ST12_YOBI			12				/* 通信ステータス12：（予備） */
#define	PV_ST13_YOBI			13				/* 通信ステータス13：（予備） */
#define	PV_ST14_LANERR			14				/* 通信ステータス14：ＬＡＮ接続異常 */
#define	PV_ST15_DEVICEBUSY		15				/* 通信ステータス15：デバイスＢＵＳＹ */
#define	PV_ST16_DEVICEERROR		16				/* 通信ステータス16：デバイス異常 */
#define	PV_ST17_NOTSENDERR		17				/* 通信ステータス17：下り送信不可異常 */
#define	PV_ST18_DWBCCERR		18				/* 通信ステータス18：下りＢＣＣ／ＣＲＣ異常 */
#define	PV_ST19_DWCOMMANDERR	19				/* 通信ステータス19：下りコマンド異常 */
#define	PV_ST20_DWFORMATERR		20				/* 通信ステータス20：下りフォーマット異常 */
#define	PV_ST21_DWFRAMEERR		21				/* 通信ステータス21：下りフレーム異常 */
#define	PV_ST22_DWPARAMERR		22				/* 通信ステータス22：下りパラメータ異常 */
#define	PV_ST23_DWDATAERR		23				/* 通信ステータス23：下りデータ異常 */
#define	PV_ST24_DWPARITYERR		24				/* 通信ステータス24：下りパリティ異常 */
#define	PV_ST25_DWSUBADDERR		25				/* 通信ステータス25：下りサブアドレス異常 */
#define	PV_ST26_DWBUFFERERR		26				/* 通信ステータス26：下り受信バッファフル異常 */
#define	PV_ST27_DWCONTROLERR	27				/* 通信ステータス27：下り制御異常 */
#define	PV_ST28_DWNORESERR		28				/* 通信ステータス28：下り無通信異常 */
#define	PV_ST29_YOBI			29				/* 通信ステータス29：（予備） */
#define	PV_ST30_YOBI			30				/* 通信ステータス30：（予備） */
#define	PV_ST31_YOBI			31				/* 通信ステータス31：（予備） */
#define	PV_ST32_YOBI			32				/* 通信ステータス32：（予備） */

/*
 * 計測値に関する定義
 */
#define	PV_TMFLG_NORMAL			0				/* 計測ステータス：正常 */
#define	PV_TMFLG_ERRNOR			1				/* 計測ステータス：正常（一部異常） */
#define	PV_TMFLG_DROPTM			2				/* 計測ステータス：欠測 */
#define	PV_TMFLG_TMERR1			4				/* 計測ステータス：計測異常１（収集対象装置判定） */
#define	PV_TMFLG_TMERR2			8				/* 計測ステータス：計測異常２（収集装置判定） */

#define	PV_TM_KIND_VALUE		1				/* 計測種別：瞬時型 */
#define	PV_TM_KIND_COUNT		2				/* 計測種別：積算型 */
#define	PV_TM_KIND_AVERG		3				/* 計測種別：平均型 *//* 非サポート */

#define	PV_TM_UKIND_WH			1				/* Wh */
#define	PV_TM_UKIND_KWH			2				/* kWh */
#define	PV_TM_UKIND_VARH		3				/* Varh */
#define	PV_TM_UKIND_KVARH		4				/* kvarh */
#define	PV_TM_UKIND_W			5				/* W */
#define	PV_TM_UKIND_KW			6				/* kW */
#define	PV_TM_UKIND_A			7				/* A */
#define	PV_TM_UKIND_KA			8				/* kA */
#define	PV_TM_UKIND_V			9				/* V */
#define	PV_TM_UKIND_KV			10				/* kV */
#define	PV_TM_UKIND_HZ			11				/* Hz */
#define	PV_TM_UKIND_C			12				/* ℃ */
#define	PV_TM_UKIND_KW_M2		13				/* kW/㎡ */
#define	PV_TM_UKIND_S			14				/* 秒 */
#define	PV_TM_UKIND_KAI			15				/* 回 */
#define	PV_TM_UKIND_KWH_KAI		16				/* kWh/回 */
#define	PV_TM_UKIND_PERCENT		17				/* ％ */
#define	PV_TM_UKIND_MA			18				/* mA */
#define	PV_TM_UKIND_VRMS		19				/* Vrms */
#define	PV_TM_UKIND_ARMS		20				/* Arms */
#define	PV_TM_UKIND_KVAR		21				/* kVar */
#define	PV_TM_UKIND_KVA			22				/* kVA */
#define	PV_TM_UKIND_HOUR		23				/* 時 */
#define	PV_TM_UKIND_MIN			24				/* 分 */
#define	PV_TM_UKIND_WM2			25				/* W/㎡ */
#define	PV_TM_UKIND_MWH			26				/* MWh */
#define	PV_TM_UKIND_MW			27				/* MW */
#define	PV_TM_UKIND_MAH			28				/* mAh */
#define	PV_TM_UKIND_AH			29				/* Ah */
#define	PV_TM_UKIND_KHZ			30				/* kHz */
#define	PV_TM_UKIND_MIN_1		31				/* min-1 */
#define	PV_TM_UKIND_M3_S		32				/* m3/s */
#define	PV_TM_UKIND_CM			33				/* cm */
#define	PV_TM_UKIND_M			34				/* m */
#define	PV_TM_UKIND_MPA			35				/* Mpa */
#define	PV_TM_UKIND_MVAR		36				/* MVar */
#define	PV_TM_UKIND_MM			37				/* mm */
#define	PV_TM_UKIND_VAR			38				/* Var */
#define	PV_TM_UKIND_VA			39				/* VA */
#define	PV_TM_UKIND_OHM			40				/* Ω */
#define	PV_TM_UKIND_NUM			41				/* 台 */
#define	PV_TM_UKIND_WS			42				/* W/s *//* [20250421] Add */
#define	PV_TM_UKIND_MV			43				/* mV *//* [20251103] Add */
#define	PV_TM_UKIND_LAST		PV_TM_UKIND_MV	/* */

/* [20230529] Add St */
/*
 * 蓄電池の用途種別に関する定義
 */
#define	BT_TYPE_STORAGE_NO_OFFGRID			1				/* 蓄電所（自立なし） */
#define	BT_TYPE_STORAGE						2				/* 蓄電所（自立あり） */
#define	BT_TYPE_CONSUMER_AUTO_OFFGRID		3				/* 需要家蓄電池（連系/自立切り替え自動） */
#define	BT_TYPE_CONSUMER_MAN_OFFGRID		4				/* 需要家蓄電池（連系/自立切り替え手動） */
#define	BT_TYPE_MAX							BT_TYPE_CONSUMER_MAN_OFFGRID
/* [20230529] Add Ed */


/*
 * 加減算種別に関する定義
 */
#define	PV_DEF_PLUS				0				/* 加算 */
#define	PV_DEF_MINUS			1				/* 減算 */

/*
 * 論理演算種別に関する定義
 */
#define	PV_DEF_OR				0				/* ＯＲ */
#define	PV_DEF_AND				1				/* ＡＮＤ */

/*
 * 論理演算種別に関する定義
 */
#define	PV_DEF_SV				0				/* 装置状態情報 */
#define	PV_DEF_ST				1				/* 通信ステータス情報 */

/*
 * 期間演算に関する定義
 */
#define	PV_DIFTYPE_YEAR			1				/* 期間演算（年） */
#define	PV_DIFTYPE_MONTH		2				/* 期間演算（月） */
#define	PV_DIFTYPE_DAY			3				/* 期間演算（日） */
#define	PV_DIFTYPE_HOUR			4				/* 期間演算（時） */

/*
 * 状態情報に関する定義
 */
#define	PV_SV_SVST_ST			1				/* 状変蓄積種別：通信ステータス情報 */
#define	PV_SV_SVST_SV			2				/* 状変蓄積種別：装置状態情報 */
#define	PV_SV_KIND_NORMAL		0				/* 状変種別：通常状変対象 */
#define	PV_SV_KIND_JOMASK		1				/* 状変種別：状変マスク対象 */

/*
 * フラグなどのステータス状態に関する定義
 */
#define	PV_FLG_STS_SET			1				/* フラグセット */
#define	PV_FLG_STS_RESET		0				/* フラグリセット */

/*
 * メッセージに関する定義
 */
#define	PV_MSG_SIZE_JHD			128				/* サーバ送信用状変メッセージ最大長 */
#define	PV_MSG_SIZE_LOG			512				/* システムログ用メッセージ最大長 */
#define	PV_MSG_SIZE_TMH			1024			/* サーバ送信用計測履歴メッセージ最大長 */
#define	PV_MSG_SIZE_SVH			512				/* サーバ送信用計測履歴メッセージ最大長 */
#define	PV_MSG_SIZE_CTL			128				/* サーバ送信用制御状変メッセージ最大長 */
#define	PV_MSG_SIZE_SCH			256				/* サーバ送信用出力抑制状変メッセージ最大長 *//* [20230529] Add */
#define	PV_MSG_SIZE_REPO		256				/* サーバ送信用レポートデータメッセージ最大長 *//* [20250807] Add */


/*
 * 共有メモリ等に関する定義
 */
#define	PV_SHMKEY_MUTEX			3000			/* 共有メモリキー（ミューテックス用） */

												/* 下記サイズの設定は以下の点に注意すること */
												/* ・システムのページサイズの倍数であること */
												/* ・pthread_mutex_t のサイズ x ファイル最	*/
												/*   大数 の値よりも大きいこと              */
												/*   CentOs7.8,AlmaLinux 8.10のページサイズは(4096)       */
												/*   pthread_mutex_t のサイズは 40バイト    */
#define	PV_SHMSIZ_MUTEX			4096			/* 共有メモリサイズ（ミューテックス用）     */


// デマンド閾値数 上限
#define	PV_MAX_DMD_THAD_LV						5

#define PV_MAX_CTL_NUM							20

/* 制御状態蓄積情報 */
#define PV_CTL_JOHEN_KIND_SET							0
#define PV_CTL_JOHEN_KIND_ERR							-1

/* 設定変更種別 */
#define PV_UPDATECONF_TYPE_PARA							1
#define PV_UPDATECONF_TYPE_PLAN							2


#define EMCTL_FLG_ON			1
#define EMCTL_FLG_OFF			0

#define PV_CTL_SW_RMT			1			/* 遠隔制御 */
#define PV_CTL_SW_OFFGRID		2			/* 自立運転切替 */
#define PV_CTL_SW_ONGRID_1		3			/* 連系運転切替1 */
#define PV_CTL_SW_ONGRID_2		4			/* 連系運転切替2 */
#define PV_CTL_SW_ONGRID_3		5			/* 連系運転切替3 */

#define PV_CTL_STATE_OFF		0			/* 未設定 */
#define PV_CTL_STATE_EXE		1			/* 処理中 */
#define PV_CTL_STATE_FIN		2			/* 処理終了 */
#define PV_CTL_STATE_FORCE		3			/* 強制終了 */


#define PV_CTL_TYPE_MODESW		0			/* モード遷移時制御 */
#define PV_CTL_TYPE_CLOUD		1			/* 画面からの制御 */
#define PV_CTL_TYPE_MAX			2			/* 制御管理上限 */

#define PV_CTL_PROC_WAIT		0			/* 待機 */
#define PV_CTL_PROC_SET			1			/* 操作設定 */
#define PV_CTL_PROC_ONE			2			/* ワンショット設定 */
#define PV_CTL_PROC_RES			3			/* 応動監視 */
#define PV_CTL_PROC_FIN			4			/* 完了 */

#define PV_RCVPW_THR_NUM		10

/*
 * SNMP通信に関する定義
 */
#define SNMP_EXPANSION_MIB						".1.3.6.1.4.1"			/* 拡張MIB */
#define SNMP_VAL_TYPE_INTEGER					"INTEGER"				/* 整数 */
#define SNMP_VAL_TYPE_STRING					"STRING"				/* 文字列 */
#define SNMP_VAL_SUBTYPE_BIT					0						/* BIT情報(ex:"01000000" ※左からCH1) */
#define SNMP_DATA_TYPE_TM						1						/* 計測情報 */
#define SNMP_DATA_TYPE_SV						2						/* 状態情報 */

/* [20210112] Add St */
/*
 * ファイル設定情報に関する定義
 */
#define COM_PARAM_GET_OK		1				/* 正常終了(取得) */
#define COM_PARAM_GET_NOUPDATE	0				/* 更新なし */
#define COM_PARAM_GET_NOFILE	-1				/* ファイルなし */
#define COM_PARAM_GET_IOERR		-2				/* FILEIOエラー */
#define COM_PARAM_GET_NOTFOUND	-3				/* パラメータが見つからない */
/* [20210112] Add Ed */

/* [20210128] Add St */
/*
 * 受電電力計画値に関する定義
 */
#define PV_POWPLN_MAX_DAY					14					/* 受電電力計画値最大設定日数 */
#define PV_POWPLN_DAY_PERIOD				48					/* 受電電力計画値1日のコマ数 */

/* [20210128] Add Ed */


#define PV_BIO_SUPPRESS_LV_MAX				5					/* バイオガス発電抑制LV最大 */

#define PV_POWPLAN_IDX_ONGRID				0					/* 需給計画連系 */
#define PV_POWPLAN_IDX_OFFGRID				1					/* 需給計画自立 */
#define PV_POWPLAN_IDX_MAX					2					/* 需給計画数 */

#define PV_POWPLAN_TYPE_RCV					0					/* 計画種別(受電) */
#define PV_POWPLAN_TYPE_BIO					1					/* 計画種別(バイオ) */
#define PV_POWPLAN_TYPE_PV					2					/* 計画種別(PV) */

#define PV_POWPLAN_TYPE_STAT				0					/* 計画有無 */
#define PV_POWPLAN_TYPE_RCVPOW				1					/* 計画種別(受電電力) */
#define PV_POWPLAN_TYPE_BTTPOW				2					/* 計画種別(充放電電力) */
#define PV_POWPLAN_TYPE_SOC					3					/* 計画種別(SoC) */
#define PV_POWPLAN_TYPE_USE_BCP				4					/* 計画種別(BCP) */
#define PV_POWPLAN_TYPE_USE_AC				5					/* 計画種別(需給調整市場使用) *//* [20230529] Add */
#define PV_POWPLAN_TYPE_REC_SOC				6					/* 計画種別(需給調整市場必要電力量) *//* [20230529] Add */
#define PV_POWPLAN_TYPE_CHG_LIMIT			7					/* 計画種別(充電電力上限) *//* [20230529] Add */
#define PV_POWPLAN_TYPE_DIS_LIMIT			8					/* 計画種別(放電電力上限) *//* [20230529] Add */
#define PV_POWPLAN_TYPE_OVER_PEAKCUT		9					/* 計画種別(ピークカット使用有無) *//* [20240220] Add */
#define PV_POWPLAN_TYPE_USE_CM				10					/* 計画種別(容量市場使用) *//* [20240220] Add */
/* [20250807] Add St */
#define PV_POWPLAN_TYPE_FCR_USE				11					/* 計画種別(一次調整力使用) *//* [20250909] Add */
#define PV_POWPLAN_TYPE_FCR_DLTKW			12					/* 計画種別(一次調整力約定ΔkW) *//* [20250807] Add */
#define PV_POWPLAN_TYPE_NUM					13					/* 計画種別数 *//* [20230529] Chg *//* [20240220] Chg *//* [20250807] Chg *//* [20250909] Chg */
/* [20250807] Add Ed */


#define PV_MONI_TYPE_RANGE					0					/* 範囲 */
#define PV_MONI_TYPE_ABOVE					1					/* 以上 */
#define PV_MONI_TYPE_BELOW					2					/* 以下 */

/* [20230529] Add St */
/*
 * jsonデータ関連情報
 */
#define	PV_JSON_TREE_MAX		10				/* jsonの階層の最大値 */
#define	PV_JSON_DATALEN_MAX		64				/* タグ名、値のデータ長の上限 */
#define	PV_JSON_DATA_MAX		1024				/* jsonの値の上限数 */

#define	PV_JSON_SEARCH_TYPE_TAG		1			/* 検索種別：タグ */
#define	PV_JSON_SEARCH_TYPE_VAL		2			/* 検索種別：値 */

/*
 * AWSに関する定義
 */
#define	AWS_INTERVAL_MAX				1800			/* 制御可能な最大期間数 */

#define AWS_ORDER_TYPE_AC				1				/* ACシステム */

#define AWS_ORDER_TYPE_AC_STR			"AC"			/* ACシステム */

/* リソース種別 */
#define AWS_RESORCE_THERMAL_POWER			1     /* 01：火力発電        */
#define AWS_RESORCE_HYDRO_POWER				2     /* 02：水力発電        */
#define AWS_RESORCE_PUMPED_STORAGE			3     /* 03：揚水発電        */
#define AWS_RESORCE_STORAGE_BATTERY			4     /* 04：蓄電池          */
#define AWS_RESORCE_VPP_GENERATION			5     /* 05：VPP(発電)       */
#define AWS_RESORCE_VPP_GENERATION_DEMAND	6     /* 06：VPP(発電+需要)  */
#define AWS_RESORCE_VPP_DEMAND				7     /* 07：VPP(需要)       */
#define AWS_RESORCE_MAX						AWS_RESORCE_VPP_DEMAND

#define AWS_ORDER_NOW			0		/* 現在 */
#define AWS_ORDER_NEXT			1		/* 未来 */
#define AWS_ORDER_STOCK			6		/* ストック数 *//* [20240201] Chg 2→6 */

#define AWS_NOTIFY_STOCK		20		/* 通知ストック数 *//* [20240701] Chg */
#define AWS_NOTIFY_ST_NON		0		/* 通知状態：未設定 */
#define AWS_NOTIFY_ST_WAIT		1		/* 通知状態：送信待ち */
#define AWS_NOTIFY_ST_COMP		2		/* 通知状態：送信完了 */
#define AWS_NOTIFY_ST_FAIL		3		/* 通知状態：送信失敗 */

#define AWS_NOTIFY_RES			0		/* 応答 */
#define AWS_NOTIFY_NOTIFY_S		1		/* 通知(開始) */
#define AWS_NOTIFY_NOTIFY_E		2		/* 通知(終了) */

#define AWS_MODE_IDLE			0		/* 待機 */
#define AWS_MODE_OPE			1		/* 運用中 */

#define	PV_LOG_OFF				0				/* ログ出力なし */
#define	PV_LOG_ON				1				/* ログ強制出力 */

/* [20240701] Chg St */
/* [20240221] Add St */
/* 商品区分 */
#define AWS_CATEGORY_REPLACEMENT_RESERVE_FOR_FIT		"3200"		/* 三次調整力(2) */
#define AWS_CATEGORY_REPLACEMENT_RESERVE				"3100"		/* 三次調整力(1) */
#define AWS_CATEGORY_FREQUENCY_CONTAINMENT				"2200"		/* 二次調整力(2)(未対応) */
#define AWS_CATEGORY_SYNC_FREQUENCY_RESTORATION			"2100"		/* 二次調整力(1)(未対応) */
#define AWS_CATEGORY_FREQUENCY_RESTORATION				"1000"		/* 一次調整力(未対応) */
//#define AWS_CATEGORY_									"0000"		/* 調整力公募(非使用) */
#define AWS_CATEGORY_CAPACITYMARKET						"0010"		/* 容量市場 */

#define AWS_CATEGORY_RR									1			/* 需給調整市場(三次調整力①②) */
#define AWS_CATEGORY_CM 								2			/* 容量市場 */
#define AWS_CATEGORY_FRR								3			/* 需給調整市場(二次調整力①②) */
#define AWS_CATEGORY_FCR								4			/* 需給調整市場(一次調整力) */

#define AWS_STR_TRUE									"true"		/* True */
#define AWS_STR_FALSE									"false"		/* False */
/* [20240221] Add Ed */
/* [20240701] Chg Ed */

/* [20260514] Add St */
/* 制御用定義 */
#define AVE_CNT_MAX										600			/* 100ms移動平均上限 */
/* [20260514] Add Ed */

/*
 * スケジュール制御に関する定義
 */
#define	PV_DAY_SCH_NUM			24*2							/* 1日のスケジュール数 */
#define	PV_MAX_RUNSCH			PV_DAY_SCH_NUM * PV_MAX_POWCN	/* 設定可能な1日のスケジュール数 */
#define PV_SCH_TI_MIN			30								/* 制御スケジュール間隔（分） */
#define PV_SCH_TI_SEC			(PV_SCH_TI_MIN*60)				/* 制御スケジュール間隔（秒） */
#define PV_SCH_DAY_TI_NUM		(24*(60/PV_SCH_TI_MIN))			/* 1日のスケジュール数 */
#define PV_SCH_TYPE_NONE		0								/* スケジュール種別（なし） */
#define PV_SCH_TYPE_FIXED		1								/* スケジュール種別（固定） */
#define PV_SCH_TYPE_UPDATE		2								/* スケジュール種別（更新） */
#define PV_SCH_BUFF_NUM			2								/* バッファ面数 */

#define	PV_SCH_TYPE_BATTERY		0								/* 蓄電池 */
#define	PV_SCH_TYPE_PV			1								/* 太陽光 */
#define	PV_SCH_TYPE_MAX			2								/* スケジュール種別上限 */

#define PV_SCH_POWERGRID_HEPCO		1		/* 北海道電力 */
#define PV_SCH_POWERGRID_WIND1		2		/* 東北電力 */
#define PV_SCH_POWERGRID_TEPCO		3		/* 東京電力 */
#define PV_SCH_POWERGRID_CHUDEN		4		/* 中部電力 */
#define PV_SCH_POWERGRID_RIKUDEN	5		/* 北陸電力 */
#define PV_SCH_POWERGRID_KANSAI		6		/* 関西電力 *//* [20211124] Add */
#define PV_SCH_POWERGRID_ENERGIA	7		/* 中国電力 */
#define PV_SCH_POWERGRID_YONDEN		8		/* 四国電力 */
#define PV_SCH_POWERGRID_KYUDEN		9		/* 九州電力 */
											/* 沖縄電力 */
#define PV_SCH_POWERGRID_MIN		PV_SCH_POWERGRID_HEPCO
#define PV_SCH_POWERGRID_MAX		10
/* [20230529] Add Ed */

/* [20250807] Add St */
#define PV_FCRST_FREQ_50					50000		/* 基準周波数50Hz(50(乗率0.001)） */
#define PV_FCRST_FREQ_60					60000		/* 基準周波数60Hz(60(乗率0.001)） */
#define PV_FCRST_VRRATE_LOWER				1			/* 調定率下限(0.001(乗率0.001)) */
#define PV_FCRST_VRRATE_UPPER				5000		/* 調定率上限(5.000(乗率0.001)) */
#define PV_FCRST_DEADBAND_LOWER				0			/* 不感帯下限(0(乗率0.001)) */
#define PV_FCRST_DEADBAND_50_UPPER			10			/* 不感帯上限_基準周波数50(0.010(乗率0.001)) */
#define PV_FCRST_DEADBAND_60_UPPER			12			/* 不感帯上限_基準周波数60(0.012(乗率0.001)) */
#define PV_FCRST_FREQDEVTSHRSLD_NOMAL		200			/* 異常判定周波数_通常(0.2(乗率0.001)) */
#define PV_FCRST_FREQDEVTSHRSLD_HOKKAIDO	300			/* 異常判定周波数_北海道(0.3(乗率0.001)) */
#define PV_FCRST_FAULTDURATION_LOWER		1			/* 異常時出力継続時間下限(1秒(試験で待たないように下限を下げる)) */
#define PV_FCRST_FAULTDURATION_UPPER		1800		/* 異常時出力継続時間上限(1800秒(1コマ分)) */
#define PV_FCRST_RATEDPOWER_LOWER			1000000		/* 定格出力下限(一次調整力最低約定量1000kW) */
#define PV_FCRST_RATEDPOWER_UPPER			10000000	/* 定格出力上限(10000kW) */
#define PV_FCRST_DISCHGPOWERCHGAMT_LOWER	1000		/* 放電時出力変化量下限(1kW) */
#define PV_FCRST_DISCHGPOWERCHGAMT_UPPER	10000000	/* 放電時出力変化量上限(10000kW) */
#define PV_FCRST_CHGPOWERCHGAMT_LOWER		1000		/* 充電時出力変化量下限(1kW) */
#define PV_FCRST_CHGPOWERCHGAMT_UPPER		10000000	/* 充電時出力変化量上限(10000kW) */
#define PV_FCRST_DEADBAND_TYPE_NONE			0			/* 不感帯応動種別：不感帯なし */
#define PV_FCRST_DEADBAND_TYPE_ONE			1			/* 不感帯応動種別：不感帯ありパターン1 */
#define PV_FCRST_DEADBAND_TYPE_TWO			2			/* 不感帯応動種別：不感帯ありパターン2 */
#define PV_FCRST_CHG_RESPONSIVE_DISABLED	0			/* 充電応動有無：無効 *//* [20251215] Add */
#define PV_FCRST_CHG_RESPONSIVE_ENABLED		1			/* 充電応動有無：有効 *//* [20251215] Add */
#define PV_FCRRP_DATANUMB_LOWRE				1			/* データ番号下限 */
#define PV_FCRRP_DATANUMB_UPPER				10			/* データ番号上限 */

#define PV_FCRRP_DATA_MAX					10			/* 一次調整力レポート蓄積項目最大数 */

#define PV_REPORTDATA_ACUM_START_TIME		300			/* 一次調整力レポート蓄積開始時間 */
#define PV_REPORTDATA_ACUM_END_TIME			5			/* 一次調整力レポート蓄積終了時間 *//* [20260109] Add */

#define PV_FCR_FREQ_SIM_LIST_MAX			100			/* 模擬周波数最大 */
#define PV_FCR_FREQ_SIM_END					-1			/* 模擬周波数終了 */

#define PV_FCR_CALC_DATA_TYPE_DLTPOW		0			/* 算出データType：約定△kW */
#define PV_FCR_CALC_DATA_TYPE_RATEDPOW		1			/* 算出データType：定格出力 */
/* [20250807] Add Ed */
