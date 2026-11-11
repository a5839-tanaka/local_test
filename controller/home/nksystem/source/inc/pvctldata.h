/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * デバイス処理用データ関連定義
 *
 */


/* 制御管理情報 */
struct st_ctlSvInf {
	struct timeval			startTime;			/* 処理開始時刻 */
	int						mode;
	int						ctlIdx;
	int						ctlProc;
	int						svNo;
	int						svValue;
	int						onshoTim;
};

/* 関数定義(comm_pvctl.c) */
int commfunc_pvctl_start( int ctlType, int mode, int userNo, int ctlNum, struct st_CTLINF *ps_ctlinf, char * fileName );
int commfunc_pvctl_stop( int ctlType, struct st_SVSTSFIL *ps_SVSTSFIL, struct st_SVMSKINF *ps_SVMSKINF );
int commfunc_pvctl_actCheck( struct st_SVSTSFIL *ps_SVSTSFIL );
int commfunc_pvctl_resSet( int retCode, struct st_ctlSvInf *ps_ctlSvInf );
int commfunc_pvctl_ctlCheck( int soKind, int soNumb, struct st_ctlSvInf *ps_ctlSvInf );
int commfunc_pvctl_bttPowGet( int *p_setPow, int *p_chgTime, struct st_TMSTSFIL *ps_TMSTSFIL );	/* [20230823] Chg */
int commfunc_pvctl_pvPerGet( int *p_setPer, struct st_TMSTSFIL *ps_TMSTSFIL );
int commfunc_pvctl_bioPowGet( int *p_setPow, struct st_TMSTSFIL *ps_TMSTSFIL );
