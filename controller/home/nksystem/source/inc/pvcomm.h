/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 * [20251027] 2025.10.27
 * ・外部機器から取得したスケジュール値を取り込むように修正
 * [20250807] 2025.08.07 機能追加
 * ・一次調整力（オフライン）対応
 *   関連ファイルを追加
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 共通ライブラリ関連定義
 *
 */

/*
 * グローバルアクセス用変数定義（共有ライブラリ用）
 */

#ifdef PV_LIB_COMM_FILE
/* 定義 */
pthread_mutex_t         *ps_g_MUTEXMASTER = NULL;   /* ミューテックス格納アドレス */
struct st_PVSYSINF      *ps_gpt_PVSYSINF = NULL;    /* システム管理情報ファイル */
struct st_JOHENFIL      *ps_gpt_JOHENFIL = NULL;	/* 状変蓄積ファイル */
struct st_PVSYSLOG      *ps_gpt_PVSYSLOG = NULL;	/* システムログファイル */
struct st_PVCTLFIL      *ps_gpt_PVCTLFIL = NULL;	/* 制御情報保存ファイル */
struct st_CTLJHFIL      *ps_gpt_CTLJHFIL = NULL;	/* 制御状変蓄積ファイル */
struct st_TMRECFIL      *ps_gpt_TMRECFIL = NULL;	/* 計測情報蓄積ファイル */
struct st_SVRECFIL      *ps_gpt_SVRECFIL = NULL;	/* 状態情報蓄積ファイル */
struct st_SCHCTLFIL     *ps_gpt_SCHCTLFIL = NULL;	/* スケジュール管理情報保存ファイル */
struct st_RPRECFIL      *ps_gpt_RPRECFIL = NULL;	/* 一次調整力レポート蓄積ファイル *//* [20260515] Add */
#else
/* 外部宣言 */
extern  pthread_mutex_t     *ps_g_MUTEXMASTER;		/* ミューテックス格納アドレス */
extern struct st_PVSYSINF   *ps_gpt_PVSYSINF; 		/* システム管理情報ファイル */
extern struct st_JOHENFIL   *ps_gpt_JOHENFIL;		/* 状変蓄積ファイル */
extern struct st_PVSYSLOG   *ps_gpt_PVSYSLOG;		/* システムログファイル */
extern struct st_PVCTLFIL   *ps_gpt_PVCTLFIL;		/* 制御情報保存ファイル */
extern struct st_CTLJHFIL   *ps_gpt_CTLJHFIL;		/* 制御状変蓄積ファイル */
extern struct st_TMRECFIL   *ps_gpt_TMRECFIL;		/* 計測情報蓄積ファイル */
extern struct st_SVRECFIL   *ps_gpt_SVRECFIL;		/* 状態情報蓄積ファイル */
extern struct st_SCHCTLFIL  *ps_gpt_SCHCTLFIL;		/* スケジュール管理情報保存ファイル */
extern struct st_RPRECFIL   *ps_gpt_RPRECFIL;		/* 一次調整力レポート蓄積ファイル *//* [20260515] Add */
#endif

/* 関数定義(comm_file.c) */
int commfunc_file_open( struct st_fileIndex * );
int commfunc_file_close( struct st_fileIndex * );
int	commfunc_mutex_create( void );
int	commfunc_mutex_attach( void );
int	commfunc_mutex_detach( void );
int	commfunc_mutex_destroy( void );
int commfunc_file_sync( struct st_fileIndex * );
int commfunc_file_remap( struct st_fileIndex * );
int	commfunc_mutex_lock( int );
int	commfunc_mutex_unlock( int );
/* [20250807] Chg St */
int commfunc_sumInfChk( int , int , int , int *, int *, int );
/* [20250807] Chg Ed */
int commfunc_chkUChosei( int );
int	func_BATTEINF_Check( FILE *, int , struct st_BATTEINF *, char *  );	/* [20230830] Add */
int	func_DEMNDINF_Check( FILE *, int , struct st_DEMNDINF *, char * , struct st_PVSYSINF *, struct st_TMDATINF * );
int	func_CEMSPINF_Check( FILE *, int , struct st_CEMSPINF *, char * );
int	func_RCVPWINF_Check( FILE *, int , struct st_RCVPWINF *, char * );
int	func_OVRCHINF_Check( FILE *, int , struct st_OVRCHINF *, char * );
int	func_CONFUPDATE_Check( FILE *, int *, int *, char * );
int	func_PWPLNINF_Check( FILE *, int , struct st_PWPLNINF *, char * );	/* [20210128] Add */
/* [20250807] Add St */
int	func_FCRSTINF_Check( FILE *, int , struct st_FCRSTINF *, char * );	/* [20250807] Add */
int	func_FCRRPINF_Check( FILE *, int , struct st_FCRRPINF *, char *, struct st_PVSYSINF *, struct st_TMDATINF * );	/* [20250807] Add */
/* [20250807] Add Ed */
int	commfunc_file_info_get( char *, time_t *, char *, char * );	/* [20210112] Add */
int commfunc_updateconf_logout( char * );		/* [20230905] Add */
int commfunc_remotecontrol_logout( char * );		/* [20230905] Add */

/* 関数定義(comm_syslog.c) */
int commfunc_syslog_msgset( char *, char * );
int commfunc_syslog_msgget( char (*)[] );
int commfunc_syslog_msgget_tail( int, char (*)[] );

/* 関数定義(comm_manage.c) */
int commfunc_manage_execFlagSet( int, int );
int	commfunc_manage_allstop( void );
int	commfunc_manage_stopCheck( void );
int	commfunc_manage_allkill( void );
int	commfunc_manage_watchdogSet( void );
int commfunc_manage_reStartCheck( int );
int commfunc_manage_pidSet( int, int );
int commfunc_manage_procNameSet( int, char * );
int commfunc_manage_stopProc( int *, int );
int	commfunc_manage_runFlagSet( int, int );
int	commfunc_manage_watchdogReset( int );
int	commfunc_manage_abnFlagSet( int, int );
int	commfunc_manage_abnFlagCheck( void );
int	commfunc_manage_dtsdLockFlagSet( int );
int	commfunc_manage_dtsdLockFlagGet( void );
int	commfunc_manage_johenLockFlagSet( int );
int	commfunc_manage_johenLockFlagGet( void );
int	commfunc_manage_sftpSndStsSet( int, int );
int	commfunc_manage_sftpSndStsGet( void );
int	commfunc_manage_manualStopSet( int );
int	commfunc_manage_manualStopGet( void );
int	commfunc_manage_routerResetSet( int );
int	commfunc_manage_routerResetGet( void );
int	commfunc_manage_ctlInstSet( int, int );		/* */
int	commfunc_manage_ctlInstGet( int );			/* */

/* 関数定義(comm_sys.c) */
void commfunc_pidtid_get(int *, int *);
int commfunc_get_soft_ver( char *);

/* 関数定義(comm_pow10abs.c) */
long long commfunc_pow10abs( short );

/* 関数定義(comm_johen.c) */
int	commfunc_johen_sv(int, int, int, unsigned char *, int, int, struct st_SVSTSFIL *, struct st_SVMSKINF * );
int	commfunc_johen_sv_wmask(int, int, int, unsigned char *, unsigned char *, int, int, struct st_SVSTSFIL *, struct st_SVMSKINF * );
int	commfunc_johen_pos(int, int, int, int, int, int, struct st_SVSTSFIL *, struct st_SVMSKINF * );
int	commfunc_johen_stat(int, int, int, int, int, struct st_STSTSFIL * );
int	commfunc_johen_pos_mask(int, int, int, int, int, int, struct st_SVSTSFIL *, struct st_SVMSKINF * );
int	commfunc_johen_stat_mask(int, int, int, int, int, struct st_STSTSFIL * );
int commfunc_johen_msgset(int, int, int, int, int, int, int );
int commfunc_johen_msggettcp( char (*)[] );
int commfunc_johen_msggetiot( char (*)[] );
int commfunc_johen_msggetlog( char (*)[] );
int commfunc_johen_msgget_tail( int, struct st_JDATA * );
int	commfunc_st_stat_get(int, int, int, int, struct st_STSTSFIL * );
int	commfunc_sv_get(int, int, int, unsigned char *, struct st_SVSTSFIL * );
int	commfunc_sv_stat_get(int, int, int, int *, struct st_SVSTSFIL * );
int	commfunc_johen_resetstat(int, int, int, struct st_STSTSFIL * );


/* 関数定義(comm_ctl_johen.c) */
int	commfunc_ctl_johen_pos(int, int, int, int, int, int, int );
int commfunc_ctl_johen_msgset(int, int, int, int, int, int, int, int );
int commfunc_ctl_johen_msggettcp( char (*)[] );
int commfunc_ctl_johen_msggetiot( char (*)[] );
int commfunc_ctl_johen_msggetlog( char (*)[] );
int commfunc_ctl_johen_msgget_tail( int, struct st_CTLDATA * );

/* 関数定義(comm_wait.c) */
int	commfunc_wait(int, int );
int	commfunc_mswait(int, int );
int	commfunc_mswait2(int );

/* 関数定義(comm_bitsub.c) */
int	commfunc_bitset(unsigned char *, int, int);
int	commfunc_bitget(unsigned char *, int);

/* 関数定義(comm_pvsch.c) */
int commfunc_pvsch_ErrCheck( int );
int commfunc_pvsch_GetData( time_t *, int, struct st_schdata * );
int commfunc_pvsch_GetChgTime( int, int *);
int commfunc_pvsch_msggettcp( char (*)[], int );
int commfunc_set_sch_data( int, time_t , int * );	/* [20251027] Add */

