/*
   TAB STOP SIZE = 4
*/
/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
#define EMBASE_PROCNAME_EMSYSTEM	"emsystem"					/* 装置監視プロセス */
																/* 装置監視プロセス動作設定ファイル */
#define	EMBASE_CONFIG_FILE			"/home/nksystem/sbin/emsystem.conf"

#define EMBASE_HOSTNAME_C			"nksystem"					/* 共通ホスト名称（デフォルト値） */
#define EMBASE_HOSTNAME_1			"nksystem1"					/* １系装置ホスト名称（デフォルト値） */
#define EMBASE_HOSTNAME_2			"nksystem2"					/* ２系装置ホスト名称（デフォルト値） */
#define EMBASE_HOSTNAME_1S			"nksystem1s"				/* １系装置系列間ホスト名称（デフォルト値） */
#define EMBASE_HOSTNAME_2S			"nksystem2s"				/* ２系装置系列間ホスト名称（デフォルト値） */

#define EMBASE_NETDEV_NAME_S		"enp2s0"					/* ネットワークＩ／Ｆ名（サービス用：デフォルト値） */
#define EMBASE_NETDEV_NAME_I		"enp3s0"					/* ネットワークＩ／Ｆ名（系列間用：デフォルト値） */

#define EMBASE_DRBD_RESOURCE		"r0"						/* ＤＲＢＤリソース名 */
#define EMBASE_DRBD_DEVICE			"/dev/drbd0"				/* ＤＲＢＤデバイス名 */
#define	EMBASE_DRBD_CHECK_TIME		5							/* ＤＲＢＤサービス動作確認周期（分）（デフォルト値） */

#define EMBASE_LIFCHK_SND_PORT		9650						/* 死活監視ＵＤＰ送信ポート番号（デフォルト値） */
#define EMBASE_LIFCHK_RCV_PORT		9660						/* 死活監視ＵＤＰ受信ポート番号（デフォルト値） */
#define EMBASE_LIFCHK_SND_TIME		1							/* 死活監視情報送信周期（秒）（デフォルト値） */
#define EMBASE_LIFCHK_RCV_TIME		3							/* 死活監視情報受信タイムアウト（秒）（デフォルト値） */
#define EMBASE_LIFCHK_DAT_KIND		5050						/* 死活監視情報種別 */

#define	EMBASE_FPATH_SBIN			"/home/nksystem/sbin/"		/* 実行モジュールパス */
#define	EMBASE_FPATH_SONLINE		"/home/nksystem/sONLDATA/"	/* ファイルパス */
#define	EMBASE_FNAME_EMSYSTEM		"EMSYSTEM.dat"				/* ２系列状態管理ファイル名 */

#define EMBASE_FPATH_LOG			"/home/nksystem/slog/"		/* ログファイルPATH */
#define EMBASE_FNAME_EMSYSLOG		"emsyslog"					/* ログファイル */
#define EMBASE_FNEXT_EMSYSLOG		"log"						/* ログファイル拡張子 */
#define EMBASE_LOGMSG_SIZE			1024						/* ログメッセージ長 */

#define	EMBASE_FNAME_STOP_FLG		"stop_flg"					/* 停止指示ファイル */

#define EMBASE_MSGQUE_EMSYSTEM		"/emsystem"					/* メッセージキュー名称：emsystemiプロセス受信用 */

#define	EMBASE_MSG_TIMEOUT			5							/* メッセージ有効時間（秒） */
#define	EMBASE_MSG_MOD_WAIT			1101						/* モード遷移：準備 */
#define	EMBASE_MSG_MOD_STOP			1102						/* モード遷移：停止 */
#define	EMBASE_MSG_MOD_STANDB		1103						/* モード遷移：待機 */
#define	EMBASE_MSG_MOD_OPERAT		1104						/* モード遷移：運用 */
#define	EMBASE_MSG_MOD_PAR_WAIT		1201						/* モード遷移：他系準備 */
#define	EMBASE_MSG_MOD_PAR_STOP		1202						/* モード遷移：他系停止 */
#define	EMBASE_MSG_MOD_PAR_STANDB	1203						/* モード遷移：他系待機 */
#define	EMBASE_MSG_MOD_PAR_OPERAT	1204						/* モード遷移：他系運用 */
#define EMBASE_MSG_OS_SHUTDOWN		9100						/* 装置制御：シャットダウン */
#define EMBASE_MSG_OS_PAR_SHUTDOWN	9200						/* 装置制御：他系シャットダウン */

#define	EMBASE_SYSNUMBER_C			0							/* 共通 */
#define	EMBASE_SYSNUMBER_1			1							/* １系装置 */
#define	EMBASE_SYSNUMBER_2			2							/* ２系装置 */

#define EMBASE_NETDEV_SERVICE		0							/* ネットワークＩ／Ｆ（サービス用） */
#define EMBASE_NETDEV_INTERNAL		1							/* ネットワークＩ／Ｆ（系列間用） */

#define	EMBASE_SYSMOD_UNKNOW		0							/* ２系列運転モード：不明 */
#define	EMBASE_SYSMOD_WAIT			1							/* ２系列運転モード：準備 */
#define	EMBASE_SYSMOD_STOP			2							/* ２系列運転モード：停止 */
#define	EMBASE_SYSMOD_STANDB		3							/* ２系列運転モード：待機 */
#define	EMBASE_SYSMOD_OPERAT		4							/* ２系列運転モード：運用 */

#define	EMBASE_NETDEV_OK			0							/* ネットワークＩ／Ｆ状態：正常 */
#define	EMBASE_NETDEV_NG			1							/* ネットワークＩ／Ｆ状態：異常 */
#define	EMBASE_NETDEV_UN			2							/* ネットワークＩ／Ｆ状態：不明（相手系のみ） */

#define	EMBASE_LIFCHK_OK			0							/* 死活監視状態：正常 */
#define	EMBASE_LIFCHK_NG			1							/* 死活監視状態：異常 o*/

/* ================= */
/* ＤＲＢＤ状態定義  */
/* ================= */
/* ロール状態 */
#define EMBASE_DRBD_ROLE_PRIMARY			0					/* プライマリロール */
#define EMBASE_DRBD_ROLE_SECONDARY			1					/* セカンダリロール */
#define EMBASE_DRBD_ROLE_UNKNOWN			2					/* 未接続 */
#define EMBASE_DRBD_ROLE_OTHER				EMBASE_DRBD_ROLE_UNKNOWN+1			/* 上記以外 */
/* ディスク状態 */
#define EMBASE_DRBD_DSTATE_UPTODATE			0					/* 同期中（正常） */
#define EMBASE_DRBD_DSTATE_DUNKNOWN			1					/* 非同期（接続していないため確認できない） */
#define EMBASE_DRBD_DSTATE_INCONSISTENT		2					/* 非同期（データ不一致） */
#define EMBASE_DRBD_DSTATE_CONSISTENT		3					/* 非同期（データ一致） */
#define EMBASE_DRBD_DSTATE_DISKLESS			4					/* 非同期（未接続） */
#define EMBASE_DRBD_DSTATE_OUTDATED			5					/* 非同期（リソース一致） */
#define EMBASE_DRBD_DSTATE_ATTACHING		6					/* メタデータ読み取り中 */
#define EMBASE_DRBD_DSTATE_FAILED			7					/* ＤＩＳＫ障害（Ｉ／Ｏ障害発生中） */
#define EMBASE_DRBD_DSTATE_NEGOTIATING		8					/* 接続処理中 */
#define EMBASE_DRBD_DSTATE_OTHER			EMBASE_DRBD_DSTATE_NEGOTIATING+1	/* 上記以外 */
/* 接続状態 */
#define EMBASE_DRBD_CSTATE_CONNECTED		0					/* 同期中 */
#define EMBASE_DRBD_CSTATE_STANDALONE		1					/* 非同期（単独動作） */
#define EMBASE_DRBD_CSTATE_DISCONNECTING	2					/* 非同期（切断中、後にStandAlone） */
#define EMBASE_DRBD_CSTATE_WFCONNECTION		3					/* 非同期（対向ノードがネットワーク上で可視するまで待ち */
#define EMBASE_DRBD_CSTATE_UNCONNECTED		4					/* 非同期（接続試行前状態、次の状態はWFConnection） */
#define EMBASE_DRBD_CSTATE_TIMEOUT			5					/* 非同期（対向ノードと通信タイムアウト、次の状態はUnconnected）*/
#define EMBASE_DRBD_CSTATE_BROKENPIPE		6					/* 非同期（対向ノードと接続断、次の状態はUnconnected） */
#define EMBASE_DRBD_CSTATE_NETWORKFAILURE	7					/* 非同期（対向ノードと接続断、次の状態はUnconnected） */
#define EMBASE_DRBD_CSTATE_PROTOCOLERROR	8					/* 非同期（対向ノードと接続断、次の状態はUnconnected） */
#define EMBASE_DRBD_CSTATE_TEARDOWN			9					/* 非同期（対向が接続を閉じている、次の状態はUnconnected） */
#define EMBASE_DRBD_CSTATE_WFREPORTPARAMS	10					/* 非同期（接続待ち） */
#define EMBASE_DRBD_CSTATE_STARTINGSYNCS	11					/* 非同期（手動同期準備中：同期元、次の状態はSyncSourceなど） */
#define EMBASE_DRBD_CSTATE_STARTINGSYNCT	12					/* 非同期（手動同期準備中：同期先、次の状態はSyncTargetなど） */
#define EMBASE_DRBD_CSTATE_WFBITMAPS		13					/* 非同期（部分同期準備中：同期元、次の状態はSyncSourceなど） */
#define EMBASE_DRBD_CSTATE_WFBITMAPT		14					/* 非同期（部分同期準備中：同期先、次の状態はSyncTargetなど） */
#define EMBASE_DRBD_CSTATE_WFSYNCUUID		15					/* 非同期（同期準備中、次の状態はSyncSourceなど） */
#define EMBASE_DRBD_CSTATE_SYNCSOURCE		16					/* 非同期（同期中：同期元） */
#define EMBASE_DRBD_CSTATE_SYNCTARGET		17					/* 非同期（同期中：同期先） */
#define EMBASE_DRBD_CSTATE_PAUSEDSYNCS		18					/* 非同期（同期一時停止中：同期元） */
#define EMBASE_DRBD_CSTATE_PAUSEDSYNCT		19					/* 非同期（同期一時停止中：同期先） */
#define EMBASE_DRBD_CSTATE_VERIFYS			20					/* 非同期（照合中：同期元） */
#define EMBASE_DRBD_CSTATE_VERIFYT			21					/* 非同期（照合中：同期先） */
#define EMBASE_DRBD_CSTATE_OTHER			EMBASE_DRBD_CSTATE_VERIFYT+1		/* 上記以外 */
/* ストッパ */
#define EMBASE_DRBD_STOPPER					-1					/* ストッパー */

struct st_EMSYSTEM {											/* ２系列状態管理ファイル構造体 */
	int				sysNumber;									/* 自系装置番号（0:１系装置／1:２系装置） */
	int				lifChkSts[2];								/* 死活監視状態（0:サービス用経由/1:系列間用経由） */
	struct st_kei 	{
		int			sysMode;									/* 運転モード */
		int			netDevSts[2];								/* ネットワークI/F状態（0:サービス用/1:系列間用） */
	} s_kei[2];
	int				drbdRole[2];								/* ＤＲＢＤロール状態（0:自系/1:他系） */
	int				drbdDstate[2];								/* ＤＲＢＤディスク状態（0:自系/1:他系） */
	int				drbdCstate;									/* ＤＲＢＤ接続状態 */
	int				newSysMode;									/* 遷移先モード */
	time_t			modeTranTime;								/* 運用モード変更確認時刻 */
	time_t			operateLastTime[2];							/* 運用モード最終稼働時刻（0:自系/1:他系） */
	time_t			operateTotalTime[2];						/* 運用モード正常稼働時間（0:自系/1:他系） */
	time_t			recvTime;									/* 死活監視情報受信時刻 */
};

struct st_drbdRoleTable {										/* ＤＲＢＤロール状態 */
	int				Numb;										/* 状態番号 */
	char			Name[64];									/* 状態名 */
};
struct st_drbdDstateTable {										/* ＤＲＢＤディスク状態 */
	int				Numb;										/* 状態番号 */
	char			Name[64];									/* 状態名 */
};
struct st_drbdCstateTable {										/* ＤＲＢＤ接続状態 */
	int				Numb;										/* 状態番号 */
	char			Name[64];									/* 状態名 */
};
static struct st_drbdRoleTable		s_drbdRoleTable[] = {		/* ＤＲＢＤロール状態 */
	{ EMBASE_DRBD_ROLE_PRIMARY,			"Primary"		},		/* プライマリロール */
	{ EMBASE_DRBD_ROLE_SECONDARY,		"Secondary"		},		/* セカンダリロール */
	{ EMBASE_DRBD_ROLE_UNKNOWN,			"Unknown"		},		/* 未接続 */
	{ EMBASE_DRBD_ROLE_OTHER,			"-unknown-"		},		/* 上記以外 */
	{ EMBASE_DRBD_STOPPER,				"---"			}		/* ストッパー */
};
static struct st_drbdDstateTable	s_drbdDstateTable[] = {		/* ＤＲＢＤディスク状態 */
	{ EMBASE_DRBD_DSTATE_UPTODATE,		"UpToDate"		},		/* 同期中（正常） */
	{ EMBASE_DRBD_DSTATE_DUNKNOWN,		"DUnknown"		},		/* 非同期（接続していないため確認できない） */
	{ EMBASE_DRBD_DSTATE_INCONSISTENT,	"Inconsistent"	},		/* 非同期（データ不一致） */
	{ EMBASE_DRBD_DSTATE_CONSISTENT,	"Consistent"	},		/* 非同期（データ一致） */
	{ EMBASE_DRBD_DSTATE_DISKLESS,		"Diskless"		},		/* 非同期（未接続） */
	{ EMBASE_DRBD_DSTATE_OUTDATED,		"Outdated"		},		/* 非同期（リソース一致） */
	{ EMBASE_DRBD_DSTATE_ATTACHING,		"Attaching"		},		/* メタデータ読み取り中 */
	{ EMBASE_DRBD_DSTATE_FAILED,		"Failed"		},		/* ＤＩＳＫ障害（Ｉ／Ｏ障害発生中） */
	{ EMBASE_DRBD_DSTATE_NEGOTIATING,	"Negotiating"	},		/* 接続処理中 */
	{ EMBASE_DRBD_DSTATE_OTHER,			"-unknown-"		},		/* 上記以外 */
	{ EMBASE_DRBD_STOPPER,				"---"			}		/* ストッパー */
};
static struct st_drbdCstateTable	s_drbdCstateTable[] = {		/* ＤＲＢＤ接続状態 */
	{ EMBASE_DRBD_CSTATE_CONNECTED,		"Connected"		},		/* 同期中 */
	{ EMBASE_DRBD_CSTATE_STANDALONE,	"StandAlone"	},		/* 非同期（単独動作） */
	{ EMBASE_DRBD_CSTATE_DISCONNECTING,	"Disconnecting"	},		/* 非同期（切断中、後にStandAlone） */
	{ EMBASE_DRBD_CSTATE_WFCONNECTION,	"WFConnection"	},		/* 非同期（対向ノードがネットワーク上で可視するまで待ち */
	{ EMBASE_DRBD_CSTATE_UNCONNECTED,	"Unconnected"	},		/* 非同期（接続試行前状態、次の状態はWFConnection） */
	{ EMBASE_DRBD_CSTATE_TIMEOUT,		"Timeout"		},		/* 非同期（対向ノードと通信タイムアウト、次の状態はUnconnected） */
	{ EMBASE_DRBD_CSTATE_BROKENPIPE,	"BrokenPipe"	},		/* 非同期（対向ノードと接続断、次の状態はUnconnected） */
	{ EMBASE_DRBD_CSTATE_NETWORKFAILURE,"NetworkFailure"},		/* 非同期（対向ノードと接続断、次の状態はUnconnected） */
	{ EMBASE_DRBD_CSTATE_PROTOCOLERROR,	"ProtocolError"	},		/* 非同期（対向ノードと接続断、次の状態はUnconnected） */
	{ EMBASE_DRBD_CSTATE_TEARDOWN,		"TearDown"		},		/* 非同期（対向が接続を閉じている、次の状態はUnconnected） */
	{ EMBASE_DRBD_CSTATE_WFREPORTPARAMS,"WFReportParams"},		/* 非同期（接続待ち） */
	{ EMBASE_DRBD_CSTATE_STARTINGSYNCS,	"StartingSyncS"	},		/* 非同期（手動同期準備中：同期元、次の状態はSyncSourceなど） */
	{ EMBASE_DRBD_CSTATE_STARTINGSYNCT,	"StartingSyncT"	},		/* 非同期（手動同期準備中：同期先、次の状態はSyncTargetなど） */
	{ EMBASE_DRBD_CSTATE_WFBITMAPS,		"WFBitMapS"		},		/* 非同期（部分同期準備中：同期元、次の状態はSyncSourceなど） */
	{ EMBASE_DRBD_CSTATE_WFBITMAPT,		"WFBitMapT"		},		/* 非同期（部分同期準備中：同期先、次の状態はSyncTargetなど） */
	{ EMBASE_DRBD_CSTATE_WFSYNCUUID,	"WFSyncUUID"	},		/* 非同期（同期準備中、次の状態はSyncSourceなど） */
	{ EMBASE_DRBD_CSTATE_SYNCSOURCE,	"SyncSource"	},		/* 非同期（同期中：同期元） */
	{ EMBASE_DRBD_CSTATE_SYNCTARGET,	"SyncTarget"	},		/* 非同期（同期中：同期先） */
	{ EMBASE_DRBD_CSTATE_PAUSEDSYNCS,	"PausedSyncS"	},		/* 非同期（同期一時停止中：同期元） */
	{ EMBASE_DRBD_CSTATE_PAUSEDSYNCT,	"PausedSyncT"	},		/* 非同期（同期一時停止中：同期先） */
	{ EMBASE_DRBD_CSTATE_VERIFYS,		"VerifyS"		},		/* 非同期（照合中：同期元） */
	{ EMBASE_DRBD_CSTATE_VERIFYT,		"VerifyT"		},		/* 非同期（照合中：同期先） */
	{ EMBASE_DRBD_CSTATE_OTHER,			"-unknown-"		},		/* 上記以外 */
	{ EMBASE_DRBD_STOPPER,				"---"			}		/* ストッパー */
};
struct st_msgFrame {											/* メッセージフレーム */
	time_t		sendTime;										/* メッセージ送信時刻 */
	int			msgKind;										/* メッセージ種別 */
};


extern int embase_func_startup_adm(void **);
extern int embase_func_startup( int );
extern int embase_func_finish();
extern int embase_func_msgq_send( int );
extern int embase_func_msgq_receive();

extern int embase_func_get_kei_info();
extern int embase_func_get_lifchk_service();
extern int embase_func_get_lifchk_internal();
extern int embase_func_get_1kei_mode();
extern int embase_func_get_2kei_mode();
extern int embase_func_get_my_mode();
extern int embase_func_get_par_mode();
extern int embase_func_get_1kei_net_service();
extern int embase_func_get_2kei_net_service();
extern int embase_func_get_my_net_service();
extern int embase_func_get_par_net_service();
extern int embase_func_get_1kei_net_internal();
extern int embase_func_get_2kei_net_internal();
extern int embase_func_get_my_net_internal();
extern int embase_func_get_par_net_internal();
extern time_t embase_func_get_lastrecv_time();
extern int embase_func_get_drbd_my_role();
extern int embase_func_get_drbd_par_role();
extern int embase_func_get_drbd_my_dstate();
extern int embase_func_get_drbd_par_dstate();
extern int embase_func_get_drbd_my_cstate();
extern time_t embase_func_get_mode_tran_time();
extern time_t embase_func_get_my_operate_total_time();
extern time_t embase_func_get_par_operate_total_time();
extern time_t embase_func_get_my_operate_last_time();
extern time_t embase_func_get_par_operate_last_time();
extern int embase_func_get_newSysMode();

extern int embase_func_set_kei_info( int );
extern int embase_func_set_lifchk_service( int );
extern int embase_func_set_lifchk_internal( int );

/*--+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+*/
