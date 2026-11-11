/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20230823] 2023.08.23
 * ・出力制御オンライン対応
 * 
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 共通関数：制御状態保存関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・制御状態保存ファイルの情報を更新する
 */
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvctldata.h"

#if 0
#define PVCTL_DEBUG
#define BTCTL_DEBUG_PER
#define BTCTL_DEBUG_PID
#define DEBUG_RMT_CTL
#endif



int commfunc_pvctl_start( int ctlType, int mode, int userNo, int ctlNum, struct st_CTLINF *ps_ctlinf, char *fileName )
{
	int						retCode = 0;
	struct st_PVCTLFIL		*ps_pvctlfil;

	if( ps_gpt_PVCTLFIL == NULL )
	{
		printf("commfunc_pvctl_start(): Error PVCTLFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	ps_pvctlfil = &ps_gpt_PVCTLFIL[ctlType];

	/* 制御状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_PVCTLFIL);

	/* 処理中の場合 */
	if( ps_pvctlfil->ctlState == PV_CTL_STATE_EXE )
	{
		/* 処理中に更新したことをログに出力する */
		retCode = -1;
	}

	/* 制御保存情報を初期化 */
	memset( ps_pvctlfil, 0, sizeof( struct st_PVCTLFIL ));

	/* 制御数を設定 */
	ps_pvctlfil->ctlNum = ctlNum;

	/* ユーザ番号を設定 */
	ps_pvctlfil->userNo = userNo;

	/* モードを設定 */
	ps_pvctlfil->ctlMode = mode;

	/* 制御情報をコピー */
	memcpy( ps_pvctlfil->s_ctlinfo, ps_ctlinf, sizeof( struct st_CTLINF ) * PV_MAX_CTL_NUM );

	/* 制御管理情報の初期化 */
	memset( ps_pvctlfil->s_ctlmng, 0, sizeof( ps_pvctlfil->s_ctlmng ));

	/* ファイル名を保持 */
	if( fileName != NULL )
	{
		memcpy(ps_pvctlfil->filename, fileName, sizeof( ps_pvctlfil->filename ));
	}

	/* 処理Indexを初期化 */
	ps_pvctlfil->ctlIdx = 0;

	if( ctlNum > 0 )
	{
		/* 現在時刻をセット */
		gettimeofday( &ps_pvctlfil->startTime, NULL );

		/* 処理中に設定 */
		ps_pvctlfil->ctlState = PV_CTL_STATE_EXE;
	}

#ifdef DEBUG_RMT_CTL
	printf("commfunc_pvctl_start : [%d] ctlNum[%d], userNo[%d], time[%ld]\n", ctlType, ctlNum, userNo, ps_pvctlfil->startTime.tv_sec );
#endif

	/* 制御状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_PVCTLFIL);

	return retCode;
}

int commfunc_pvctl_stop( int ctlType, struct st_SVSTSFIL *ps_SVSTSFIL, struct st_SVMSKINF *ps_SVMSKINF )
{
	int 					loopCnt1;
	int 					loopCnt2;
	int						svStat;
	struct st_CTLINF		*ps_ctlinf;
	struct st_CTLMNG		*ps_ctmng;
	struct st_PVCTLFIL		*ps_pvctlfil;
	int						retCode;
	struct timeval			nowTime;
	struct timeval			wrkTime;
	char					deleteName[256];			/* 削除ファイル名 */
	int						ctlSkip;


	if( ps_gpt_PVCTLFIL == NULL )
	{
		printf("commfunc_pvctl_stop(): Error PVCTLFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 制御状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_PVCTLFIL);

	ps_pvctlfil = &ps_gpt_PVCTLFIL[ctlType];

	/* 実行中以外の場合は、処理しない */
	if( ps_pvctlfil->ctlState == PV_CTL_STATE_EXE )
	{
		ps_ctlinf = &ps_pvctlfil->s_ctlinfo[ps_pvctlfil->ctlIdx];
		ps_ctmng  = &ps_pvctlfil->s_ctlmng[ps_pvctlfil->ctlIdx];

#ifdef DEBUG_RMT_CTL
		printf("commfunc_pvctl_stop : ps_ctmng->ctlProc=%d\n", ps_ctmng->ctlProc );
#endif

		/* 制御状態が待機の場合 */
		if( ps_ctmng->ctlProc == PV_CTL_PROC_WAIT )
		{
			/* 処理終了とする */
			ps_pvctlfil->ctlState = PV_CTL_STATE_FORCE;
		}
		/* 制御状態が設定の場合 */
		else if( ps_ctmng->ctlProc == PV_CTL_PROC_SET )
		{
#ifdef DEBUG_RMT_CTL
			printf("commfunc_pvctl_stop : idx[%d] proc==%d, retTime=%d.%06d, retCode=%d, oneShot=%d, statFlagRes=%d\n", loopCnt2, ps_ctmng->ctlProc, 
				ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime.tv_sec, ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime.tv_usec,
				ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retCode,  ps_ctlinf->oneShot,  ps_ctlinf->statFlagRes);
#endif

			/* 終了時刻が設定されている場合 */
			/* 処理結果が異常の場合 */
			/* ワンショット制御待ちの場合 */
			/* 制御対象が収集装置の場合 */
			if( ( ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime.tv_sec > 0 ) &&
				( ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retCode == 0 ) &&
				( ps_ctlinf->oneShot > 0 ) &&
				( ps_ctlinf->soKindCtl == PV_MKIND_PVSYS ))
			{
				/* 制御の状変を更新 */
				commfunc_johen_pos( PV_MKIND_PVSYS, 1, ps_ctlinf->svNumbCtl, (0x01 & ~ps_ctlinf->svValCtl ), 0, 0, ps_SVSTSFIL, ps_SVMSKINF );
			}
			else
			{
				/* 操作失敗 */
				commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );
			}
			/* 処理終了とする */
			ps_pvctlfil->ctlState = PV_CTL_STATE_FORCE;
		}
		/* 制御状態がワンショットの場合 */
		else if( ps_ctmng->ctlProc == PV_CTL_PROC_ONE )
		{
#ifdef DEBUG_RMT_CTL
			printf("commfunc_pvctl_stop : idx[%d] proc==%d, retTime=%d.%06d, retCode=%d, statFlagRes=%d\n", loopCnt2, ps_ctmng->ctlProc, 
				ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retTime.tv_sec, ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retTime.tv_usec,
				ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retCode,  ps_ctlinf->statFlagRes);
#endif
			/* 操作失敗 */
			commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );
			/* 処理終了とする */
			ps_pvctlfil->ctlState = PV_CTL_STATE_FORCE;
		}
		/* 制御状態が応答の場合 */
		else if( ps_ctmng->ctlProc == PV_CTL_PROC_RES )
		{
#ifdef DEBUG_RMT_CTL
			/* 現在時刻を取得 */
		 	gettimeofday( &nowTime, NULL );
			printf("commfunc_pvctl_stop : idx[%d] proc==%d, retTime=%ld.%06d, retCode=%d, timeOut=%d, nowTime=%ld.%06d\n", loopCnt2, ps_ctmng->ctlProc, 
				ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime.tv_sec, ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime.tv_usec,
				ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retCode,  ps_ctlinf->timeOutRes, nowTime.tv_sec, nowTime.tv_usec );
#endif
			/* 処理終了とする */
			ps_pvctlfil->ctlState = PV_CTL_STATE_FORCE;
		}
	}

	/* 制御状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_PVCTLFIL);

	return 0;
}



int commfunc_pvctl_actCheck( struct st_SVSTSFIL *ps_SVSTSFIL )
{
	int 					loopCnt1;
	int 					loopCnt2;
	int						svStat;
	int						jhnKind;
	struct st_CTLINF		*ps_ctlinf;
	struct st_CTLMNG		*ps_ctmng;
	struct st_PVCTLFIL		*ps_pvctlfil;
	int						svNumb;
	int						retCode;
	struct timeval			nowTime;
	struct timeval			wrkTime;
	char					deleteName[256];			/* 削除ファイル名 */
	int						ctlSkip;


	if( ps_gpt_PVCTLFIL == NULL )
	{
		printf("commfunc_pvctl_start(): Error PVCTLFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 制御状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_PVCTLFIL);

	/* 遠隔制御から先にチェックする */
	for( loopCnt1 = PV_CTL_TYPE_MAX-1; loopCnt1 >= 0; loopCnt1-- )
	{
		/* 処理結果を初期化 */
		retCode = 0;
		ps_pvctlfil = &ps_gpt_PVCTLFIL[loopCnt1];

		if( ps_pvctlfil->ctlState != PV_CTL_STATE_EXE )
			continue;

#ifdef DEBUG_RMT_CTL
		printf("commfunc_pvctl_actCheck : [%d] ctlState=%d\n", loopCnt1, ps_pvctlfil->ctlState );
#endif
		/* 残りの制御数分ループする */
		for( loopCnt2 = ps_pvctlfil->ctlIdx; loopCnt2 < ps_pvctlfil->ctlNum; loopCnt2++ )
		{
			ps_ctlinf = &ps_pvctlfil->s_ctlinfo[loopCnt2];
			ps_ctmng  = &ps_pvctlfil->s_ctlmng[loopCnt2];
#ifdef DEBUG_RMT_CTL
		printf("commfunc_pvctl_actCheck : ps_ctmng->ctlProc=%d\n", ps_ctmng->ctlProc );
#endif
			ctlSkip = 0;
			/* これまでの制御結果が異常の場合、且つ、*/
			/* 現在の制御対象が強制でない場合 */
			if( ( 0 != ps_pvctlfil->retCode) &&
				( 0 == ps_ctlinf->forceCtl ))
			{
				/* 制御を飛ばす */
				continue;
			}

			/* 制御状態が待機の場合 */
			if( ps_ctmng->ctlProc == PV_CTL_PROC_WAIT )
			{
				/* 未実装の場合は、応動チェックせずに操作を行う。 */
				if( ps_ctlinf->statFlagRes == 0 )
				{
					/* 制御の状変は、操作のSVを登録 */
					ps_ctmng->soKindJhn	= ps_ctlinf->soKindCtl;
					ps_ctmng->soNumbJhn	= ps_ctlinf->soNumbCtl;
					ps_ctmng->svNoJhn	= ps_ctlinf->svNumbCtl;
					ps_ctmng->svValJhn	= ps_ctlinf->svValCtl;
					ps_ctmng->svIfLinkJhn = ps_ctlinf->svIfLinkCtl;
				}
				else
				{
					/* 制御対象の応動SVをチェック */
					commfunc_sv_stat_get( ps_ctlinf->soKindRes, ps_ctlinf->soNumbRes, ps_ctlinf->svNumbRes, &svStat, ps_SVSTSFIL);

					/* 制御の状変を保持する */
					ps_ctmng->soKindJhn	= ps_ctlinf->soKindRes;
					ps_ctmng->soNumbJhn	= ps_ctlinf->soNumbRes;
					ps_ctmng->svNoJhn	= ps_ctlinf->svNumbRes;
					ps_ctmng->svValJhn	= ps_ctlinf->svValRes;
					ps_ctmng->svIfLinkJhn = ps_ctlinf->svIfLinkRes;

					/* 強制制御がOFF、且つ、応動SVが一致の場合(=制御対象外) */
					if( ( 0 == ps_ctlinf->forceCtl ) &&
					    ( svStat == ps_ctlinf->svValRes ))
					{
						ctlSkip = 1;
					}
				}

				/* 制御対象の場合 */
				if( ctlSkip == 0 )
				{
					/* 設定に変更 */
					ps_ctmng->ctlProc = PV_CTL_PROC_SET;

#ifdef DEBUG_RMT_CTL
					printf("commfunc_pvctl_actCheck : idx[%d] proc=%d->%d wait, soKind,Numb=[%d,%d], svIfLink=%d\n", loopCnt2, PV_CTL_PROC_WAIT, ps_ctmng->ctlProc, ps_ctlinf->soKindCtl, ps_ctlinf->soNumbCtl, ps_ctmng->svIfLinkJhn);
#endif
					/* 操作開始 */
					commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_SET, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );

					/* 制御不可SV条件が実装の場合は、制御不可SVのチェックを行う */
					if( ps_ctlinf->statFlagNotCtl != 0 )
					{
						/* 制御対象の制御不可SV条件 SVをチェック */
						commfunc_sv_stat_get( ps_ctlinf->soKindNotCtl, ps_ctlinf->soNumbNotCtl, ps_ctlinf->svNumbNotCtl, &svStat, ps_SVSTSFIL);

						/* 制御不可SV値が一致している場合は、制御失敗として処理を終了する */
						if( ps_ctlinf->svValNotCtl == svStat )
						{
							/* 管理情報に現在時刻をセット */
							gettimeofday( &ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime, NULL );
							/* 処理結果を異常に設定 */
							ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retCode = -1;

							/* 操作失敗 */
							commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );

							/* 制御状態を完了 */
							ps_ctmng->ctlProc = PV_CTL_PROC_FIN;

							/* 制御結果に異常を設定 */
							ps_pvctlfil->retCode = -1;
							continue;
#ifdef DEBUG_RMT
					printf("commfunc_pvctl_actCheck : idx[%d] proc=%d FIN, NotCtl(soKind,soNumb,svNunb,Val)=[%d,%d,%d,%d]\n", loopCnt2, ps_ctmng->ctlProc, ps_ctlinf->soKindNotCtl, ps_ctlinf->soNumbNotCtl, ps_ctlinf->svNumbNotCtl, ps_ctlinf->svValNotCtl);
#endif //DEBUG_RMT
						}
					}
					break;
				}
				else
				{
#ifdef DEBUG_RMT_CTL
					printf("commfunc_pvctl_actCheck : idx[%d] proc=%d  already res soKind,Numb,Sv,Val[%d,%d,%d,%d]\n", loopCnt2, PV_CTL_PROC_WAIT, 
						ps_ctlinf->soKindRes, ps_ctlinf->soNumbRes, ps_ctlinf->svNumbRes, ps_ctlinf->svValRes );
#endif

					/* 管理情報に現在時刻をセット */
					gettimeofday( &ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime, NULL );
					/* 処理結果を正常に設定 */
					ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retCode = 0;
					/* 制御状態を完了 */
					ps_ctmng->ctlProc = PV_CTL_PROC_FIN;
				}
			}
			/* 制御状態が設定の場合 */
			else if( ps_ctmng->ctlProc == PV_CTL_PROC_SET )
			{
#ifdef DEBUG_RMT_CTL
				printf("commfunc_pvctl_actCheck : idx[%d] proc==%d, retTime=%d.%06d, retCode=%d, oneShot=%d, statFlagRes=%d\n", loopCnt2, ps_ctmng->ctlProc, 
					ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime.tv_sec, ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime.tv_usec,
					ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retCode,  ps_ctlinf->oneShot,  ps_ctlinf->statFlagRes);
#endif
				/* 終了時刻が設定されている場合 */
				if( ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime.tv_sec > 0 )
				{
					/* 処理結果が異常の場合 */
					if( ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retCode < 0 )
					{
						retCode = -1;
						/* 操作失敗 */
						commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );

						/* 制御状態を完了 */
						ps_ctmng->ctlProc = PV_CTL_PROC_FIN;

						/* 制御結果に異常を設定 */
						ps_pvctlfil->retCode = -1;
						continue;
					}
					/* 処理結果が正常の場合 */
					else
					{
						/* ワンショット時間が0の場合は、応動確認待ち */
						if( ps_ctlinf->oneShot == 0 )
						{
							/* 応動が実装の場合 */
							if( ps_ctlinf->statFlagRes != 0 )
							{
								/* 応動に変更 */
								ps_ctmng->ctlProc = PV_CTL_PROC_RES;

								/* 管理情報に現在時刻をセット */
								gettimeofday( &ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].setTime, NULL );

#ifdef DEBUG_RMT_CTL
								printf("commfunc_pvctl_actCheck : idx[%d] proc=%d->%d\n", PV_CTL_PROC_SET, ps_ctmng->ctlProc ); 
#endif
								break;
							}
							else
							{
								/* 応動が未実装の場合は、制御状態を完了 */
								ps_ctmng->ctlProc = PV_CTL_PROC_FIN;
							}
						}
						else
						{
							/* 現在時刻を取得 */
						 	gettimeofday( &nowTime, NULL );
							wrkTime = ps_ctmng->s_ctlrec[PV_CTL_PROC_SET-1].retTime;
							wrkTime.tv_sec += ps_ctlinf->oneShot;

							/* (現在時刻>=(操作終了時刻+維持時間)の場合 */
							if( (nowTime.tv_sec > wrkTime.tv_sec ) ||
							    ((nowTime.tv_sec == wrkTime.tv_sec ) &&
							     (nowTime.tv_usec >= wrkTime.tv_usec ) ))
							{
								/* ワンショットに変更 */
								ps_ctmng->ctlProc = PV_CTL_PROC_ONE;
#ifdef DEBUG_RMT_CTL
								printf("commfunc_pvctl_actCheck : idx[%d] proc=%d->%d\n", PV_CTL_PROC_SET, ps_ctmng->ctlProc ); 
#endif
								break;
							}
							else
							{
								/* 判定時間待ち */
								break;
							}
						}
					}
				}
				else
				{
					/* 操作待ち状態 */
					break;
				}
			}
			/* 制御状態がワンショットの場合 */
			else if( ps_ctmng->ctlProc == PV_CTL_PROC_ONE )
			{
#ifdef DEBUG_RMT_CTL
				printf("commfunc_pvctl_actCheck : idx[%d] proc==%d, retTime=%d.%06d, retCode=%d, statFlagRes=%d\n", loopCnt2, ps_ctmng->ctlProc, 
					ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retTime.tv_sec, ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retTime.tv_usec,
					ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retCode,  ps_ctlinf->statFlagRes);
#endif
				/* 終了時刻が設定されている場合 */
				if( ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retTime.tv_sec > 0 )
				{
					/* 処理結果が以上の場合 */
					if( ps_ctmng->s_ctlrec[PV_CTL_PROC_ONE-1].retCode < 0 )
					{
						retCode = -1;
						/* 操作失敗 */
						commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );

						/* 制御状態を完了 */
						ps_ctmng->ctlProc = PV_CTL_PROC_FIN;

						/* 制御結果に異常を設定 */
						ps_pvctlfil->retCode = -1;
						continue;
					}
					/* 処理結果が正常の場合 */
					else
					{
						/* 応動が実装の場合 */
						if( ps_ctlinf->statFlagRes != 0 )
						{
							/* 応動に変更 */
							ps_ctmng->ctlProc = PV_CTL_PROC_RES;

							/* 管理情報に現在時刻をセット */
							gettimeofday( &ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].setTime, NULL );

#ifdef DEBUG_RMT_CTL
							printf("commfunc_pvctl_actCheck : idx[%d] proc=%d->%d\n", PV_CTL_PROC_ONE, ps_ctmng->ctlProc ); 
#endif
							break;
						}
						else
						{
							/* 制御状態を完了 */
							ps_ctmng->ctlProc = PV_CTL_PROC_FIN;
						}
					}
				}
				else
				{
					/* 操作待ち状態 */
					break;
				}
			}
			/* 制御状態が応答の場合 */
			else if( ps_ctmng->ctlProc == PV_CTL_PROC_RES )
			{
#ifdef DEBUG_RMT_CTL
				/* 現在時刻を取得 */
			 	gettimeofday( &nowTime, NULL );
				printf("commfunc_pvctl_actCheck : idx[%d] proc==%d, retTime=%ld.%06d, retCode=%d, timeOut=%d, nowTime=%ld.%06d\n", loopCnt2, ps_ctmng->ctlProc, 
					ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime.tv_sec, ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime.tv_usec,
					ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retCode,  ps_ctlinf->timeOutRes, nowTime.tv_sec, nowTime.tv_usec );
#endif
				/* 応動時間が設定されている場合 */
				if( ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].setTime.tv_sec > 0 )
				{
					/* 現在時刻を取得 */
				 	gettimeofday( &nowTime, NULL );

					wrkTime = ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].setTime;
					wrkTime.tv_sec += ps_ctlinf->timeOutRes;

					/* 制御対象の応動SVをチェック */
					commfunc_sv_stat_get( ps_ctlinf->soKindRes, ps_ctlinf->soNumbRes, ps_ctlinf->svNumbRes, &svStat, ps_SVSTSFIL);

					/* 応動SVが不一致の場合 */
					if( svStat != ps_ctlinf->svValRes )
					{
						/* (現在時刻>=(応答開始時刻+判定時間)の場合 */
						if( (nowTime.tv_sec > wrkTime.tv_sec ) ||
						    ((nowTime.tv_sec == wrkTime.tv_sec ) &&
						     (nowTime.tv_usec >= wrkTime.tv_usec ) ))
						{
							retCode = -1;

							/* 管理情報に現在時刻をセット */
							gettimeofday( &ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime, NULL );
							/* 処理結果を正常に設定 */
							ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retCode = retCode;
							/* 制御状態を完了 */
							ps_ctmng->ctlProc = PV_CTL_PROC_FIN;

							/* 判定時間を超えても応動SVが一致しない為、異常終了 */
							commfunc_ctl_johen_pos( PV_CTL_JOHEN_KIND_ERR, ps_ctmng->soKindJhn, ps_ctmng->soNumbJhn, ps_ctmng->svNoJhn, ps_ctmng->svValJhn, ps_pvctlfil->userNo, ps_ctmng->svIfLinkJhn );

							/* 制御結果に異常を設定 */
							ps_pvctlfil->retCode = -1;
							continue;
						}
						else
						{
							/* 判定時間待ち */
							break;
						}
					}
					else
					{
						/* 応動SVが一致した場合 */
						/* 管理情報に現在時刻をセット */
						gettimeofday( &ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retTime, NULL );
						/* 処理結果を正常に設定 */
						ps_ctmng->s_ctlrec[PV_CTL_PROC_RES-1].retCode = 0;
						/* 制御状態を完了 */
						ps_ctmng->ctlProc = PV_CTL_PROC_FIN;
						/* 次のIdxに移行 */
					}
				}
				else
				{
					/* 応答時間が設定されていない場合は、不具合 */
				}
			}
		}
#ifdef DEBUG_RMT_CTL
		printf("commfunc_pvctl_actCheck : LoopEnd idx[%d] ctlNum[%d], retCode=%d\n", loopCnt2, ps_pvctlfil->ctlNum, retCode );
#endif

		/* 制御Idxを更新  */
		ps_pvctlfil->ctlIdx = loopCnt2;

		/* 制御Idxが終端に到達した場合 */
		if( ps_pvctlfil->ctlIdx == ps_pvctlfil->ctlNum )
		{
			/* 制御の種別を設定 */
			if( ps_pvctlfil->retCode == 0 )
			{
				jhnKind = PV_CTL_JOHEN_KIND_SET;
			}
			else
			{
				jhnKind = PV_CTL_JOHEN_KIND_ERR;
			}
			
			/* 制御を終了する */
			ps_pvctlfil->ctlState = PV_CTL_STATE_FIN;
			/* クラウド制御の場合 */
			if( loopCnt1 == PV_CTL_TYPE_CLOUD)
			{
				/* モードの制御結果の状変を行う */
				switch( ps_pvctlfil->ctlMode )
				{
#if 0
					/* 自立運転切替 */
					case PV_CTL_SW_OFFGRID :
						svNumb = PV_SYS_SV_MODE_ON_TO_OFF;
						break;

					/* 連系運転切替ステップ1 */
					case PV_CTL_SW_ONGRID_1 :
						svNumb = PV_SYS_SV_MODE_OFF_TO_ON_1;
						break;

					/* 連系運転切替ステップ2 */
					case PV_CTL_SW_ONGRID_2 :
						svNumb = PV_SYS_SV_MODE_OFF_TO_ON_2;
						break;

					/* 連系運転切替ステップ3 */
					case PV_CTL_SW_ONGRID_3 :
						svNumb = PV_SYS_SV_MODE_OFF_TO_ON_3;
						break;
#endif
					/* 遠隔制御 */
					case PV_CTL_SW_RMT :
					default :
						svNumb = PV_SYS_SV_MODE_REMOTE;
						break;
				}
				/* モード切替の状変を行う */
				commfunc_ctl_johen_pos( jhnKind, PV_MKIND_PVSYS, 1, svNumb, 0, ps_pvctlfil->userNo, svNumb-1 );
			}
#ifdef DEBUG_RMT_CTL
			printf("commfunc_pvctl_actCheck : idx[%d] ctlNum[%d], retCode=%d proc=%d\n", ps_pvctlfil->ctlIdx, ps_pvctlfil->ctlNum, retCode, ps_pvctlfil->ctlState );
#endif
		}
	}

	/* 制御状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_PVCTLFIL);

	return 0;
}




/* 処理結果を保存する */
/* 処理結果が異常の場合、制御状態を応動に設定し、処理結果と現在時刻を応答結果に設定する */
/* 処理結果が正常の場合、ワンショットが実装済みの場合、制御状態をワンショットに設定する。 */
/*                       ワンショットが未実装の場合、制御状態を応動に設定する。 */
/* 状態現在時刻を応答結果に設定する */
int commfunc_pvctl_resSet( int retCode, struct st_ctlSvInf *ps_ctlSvInf )
{
	int 					ctlKind;
	int						ctlIdx;
	struct st_CTLINF		*ps_ctlinf;
	struct st_CTLMNG		*ps_ctmng;
	struct st_PVCTLFIL		*ps_pvctlfil;

	if( ps_gpt_PVCTLFIL == NULL )
	{
		printf("commfunc_pvctl_resSet(): Error PVCTLFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	if( ps_ctlSvInf == NULL )
	{
		printf("commfunc_pvctl_resSet(): param is NULL\n");
		return -1;
	}

	ps_pvctlfil = &ps_gpt_PVCTLFIL[ps_ctlSvInf->mode];

	if( ps_pvctlfil->ctlState == PV_CTL_STATE_EXE )
	{
#ifdef DEBUG_RMT_CTL
	printf("commfunc_pvctl_resSet : call\n");
	printf("commfunc_pvctl_resSet : call [%d] idx[%d] retCode=%d\n", ps_ctlSvInf->mode, ps_ctlSvInf->ctlIdx, retCode);
	printf("commfunc_pvctl_resSet : ps_ctlSvInf->startTime.tv_sec=%ld\n", ps_ctlSvInf->startTime.tv_sec);
	printf("commfunc_pvctl_resSet : ps_gpt_PVCTLFIL[ctlKind].ctlIdx=%d\n", ps_pvctlfil->ctlIdx);
	printf("commfunc_pvctl_resSet : ps_ctlSvInf->ctlProc=%d\n", ps_ctlSvInf->ctlProc);
#endif

	/* 制御状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_PVCTLFIL);
	ctlKind = ps_ctlSvInf->mode;
	ctlIdx = ps_ctlSvInf->ctlIdx;


	/* 制御の情報が正しいかチェック */
	if( ( ps_ctlSvInf->startTime.tv_sec == ps_gpt_PVCTLFIL[ctlKind].startTime.tv_sec ) &&
		( ps_gpt_PVCTLFIL[ctlKind].ctlIdx == ctlIdx) )
	{
		ps_ctmng = &ps_gpt_PVCTLFIL[ctlKind].s_ctlmng[ctlIdx];
		ps_ctlinf = &ps_gpt_PVCTLFIL[ctlKind].s_ctlinfo[ctlIdx];

		/* 処理結果を保存 */
		ps_ctmng->s_ctlrec[ps_ctlSvInf->ctlProc-1].retCode = retCode;
		/* 処理結果時刻に現在時刻を設定 */
		gettimeofday( &ps_ctmng->s_ctlrec[ps_ctmng->ctlProc-1].retTime, NULL );
#ifdef DEBUG_RMT_CTL
		printf("commfunc_pvctl_resSet : [%d] idx[%d] retCode=%d, retTime=%ld\n", ctlKind, ctlIdx, retCode, ps_ctmng->s_ctlrec[ps_ctmng->ctlProc-1].retTime);
#endif
	}
#ifdef DEBUG_RMT_CTL
	else
	{
		printf("commfunc_pvctl_resSet : [%d] idx[%d] != %d\n", ctlKind, ctlIdx, ps_gpt_PVCTLFIL[ctlKind].ctlIdx);
	}
#endif
	}
	/* 制御状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_PVCTLFIL);

	return 0;
}

	
/* 通信プロセス側がコールする、もしくは PVSYS側でコールする */
int commfunc_pvctl_ctlCheck( int soKind, int soNumb, struct st_ctlSvInf *ps_ctlSvInf )
{
	int 					loopCnt1;
	int						ctlIdx;
	int						retCode = 0;
	struct st_CTLINF		*ps_ctlinf;
	struct st_CTLMNG		*ps_ctmng;

	if( ps_gpt_PVCTLFIL == NULL )
	{
		printf("commfunc_pvctl_ctlCheck(): Error PVCTLFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 制御状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_PVCTLFIL);


	/* 遠隔制御から先にチェックする */
	for( loopCnt1 = PV_CTL_TYPE_MAX-1; loopCnt1 >= 0; loopCnt1-- )
	{
		/* 処理中以外の場合は、skip */
		if( ps_gpt_PVCTLFIL[loopCnt1].ctlState != PV_CTL_STATE_EXE )
		{
			continue;
		}

		ctlIdx = ps_gpt_PVCTLFIL[loopCnt1].ctlIdx;
		ps_ctlinf = &ps_gpt_PVCTLFIL[loopCnt1].s_ctlinfo[ctlIdx];
		ps_ctmng = &ps_gpt_PVCTLFIL[loopCnt1].s_ctlmng[ctlIdx];

		/* 制御状態が設定の場合、または */
		/* 制御状態がワンショットの場合 */
		if( ( ps_ctmng->ctlProc == PV_CTL_PROC_SET ) ||
		    ( ps_ctmng->ctlProc == PV_CTL_PROC_ONE ) )
		{
#ifdef DEBUG_RMT_CTL
			printf("commfunc_pvctl_ctlCheck : [%d] idx[%d] ctlProc=%d, soKind,Numb=[%d,%d], setTime.tv_sec=%ld\n", loopCnt1, ctlIdx, ps_ctmng->ctlProc, soKind, soNumb, ps_ctmng->s_ctlrec[ps_ctmng->ctlProc-1].setTime.tv_sec);
#endif
			/* 装置番号、装置種別が一致している場合、且つ */
			/* 開始時刻が設定されていない場合 */
			if( ( ps_ctlinf->soKindCtl == soKind ) &&
				( ps_ctlinf->soNumbCtl == soNumb ) && 
				( ps_ctmng->s_ctlrec[ps_ctmng->ctlProc-1].setTime.tv_sec == 0 ))
			{
				memset(ps_ctlSvInf, 0, sizeof( struct st_ctlSvInf ));
				ps_ctlSvInf->startTime = ps_gpt_PVCTLFIL[loopCnt1].startTime;
				ps_ctlSvInf->mode = loopCnt1;
				ps_ctlSvInf->ctlProc = ps_ctmng->ctlProc;
				ps_ctlSvInf->ctlIdx = ctlIdx;
				ps_ctlSvInf->svNo = ps_ctlinf->svNumbCtl;
				if( ps_ctmng->ctlProc == PV_CTL_PROC_SET )
				{
					/* 操作の場合は、SV値をそのまま設定 */
					ps_ctlSvInf->svValue = ps_ctlinf->svValCtl;
				}
				else
				{
					/* ワンショットの場合は、SV値を反転させる */
					ps_ctlSvInf->svValue = (0x01 & ~ps_ctlinf->svValCtl );
				}

				/* 管理情報に現在時刻をセット */
				gettimeofday( &ps_ctmng->s_ctlrec[ps_ctmng->ctlProc-1].setTime, NULL );

#ifdef DEBUG_RMT_CTL
				printf("commfunc_pvctl_ctlCheck : soKind,Numb,svVal[%d,%d,%d], time=%ld\n", ps_ctlinf->soKindCtl, ps_ctlinf->soNumbCtl, ps_ctlSvInf->svValue, ps_gpt_PVCTLFIL[loopCnt1].startTime.tv_sec);
#endif

				retCode = 1;
				break;
			}
		}
	}

	/* 制御状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_PVCTLFIL);

	return retCode;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * BT出力抑制 制御値取得
 *
 * 機能：
 * 		引数 result の前回周期の処理結果から、次回の制御値を算出する
 *		
 * 戻り値：
 * 		 0：正常処理
 *		-1：異常処理
 */
int commfunc_pvctl_bttPowGet( int *p_setPow, int *p_chgTime, struct st_TMSTSFIL *ps_TMSTSFIL )	/* [20230823] Chg */
{
	int			retCode = 0;

	/* 計測値保存ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_TMSTSFIL);

	/* 欠測の場合は、0w  */
	if( ps_TMSTSFIL[PV_SYS_TM_BT_SET_POW-1].tmFlag == PV_TMFLG_NORMAL )
	{
		*p_setPow = ps_TMSTSFIL[PV_SYS_TM_BT_SET_POW-1].tmVal;
	}
	else
	{
		*p_setPow = 0;
	}

/* [20230823] Add St */
	/* 欠測の場合は、0分(即時)  */
	if( ps_TMSTSFIL[PV_SYS_TM_SCH_CHG_TIME-1].tmFlag == PV_TMFLG_NORMAL )
	{
		*p_chgTime = ps_TMSTSFIL[PV_SYS_TM_SCH_CHG_TIME-1].tmVal;
	}
	else
	{
		*p_chgTime = 0;
	}
/* [20230823] Add Ed */

	/* 計測値保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_TMSTSFIL);

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * PV出力抑制 制御値取得
 *
 * 機能：
 * 		引数 result の前回周期の処理結果から、次回の制御値を算出する
 *		
 * 戻り値：
 * 		 0：正常処理
 *		-1：異常処理
 */
int commfunc_pvctl_pvPerGet( int *p_setPer, struct st_TMSTSFIL *ps_TMSTSFIL )
{
	int			retCode = 0;

	/* 計測値保存ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_TMSTSFIL);


	/* 欠測の場合は、0%  */
	if( ps_TMSTSFIL[PV_SYS_TM_PV_SET_PER-1].tmFlag == PV_TMFLG_NORMAL )
	{
		*p_setPer = ps_TMSTSFIL[PV_SYS_TM_PV_SET_PER-1].tmVal;
	}
	else
	{
		*p_setPer = 0;
	}

	/* 計測値保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_TMSTSFIL);

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * バイオガス発電電力 制御値取得
 *
 * 機能：
 * 		引数 result の前回周期の処理結果から、次回の制御値を算出する
 *		
 * 戻り値：
 * 		 0：正常処理
 *		-1：異常処理
 */
int commfunc_pvctl_bioPowGet( int *p_setPow, struct st_TMSTSFIL *ps_TMSTSFIL )
{
	int			retCode = 0;

	/* 計測値保存ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_TMSTSFIL);

	/* 欠測の場合は、0w  */
	if( ps_TMSTSFIL[PV_SYS_TM_BIO_SET_POW-1].tmFlag == PV_TMFLG_NORMAL )
	{
		*p_setPow = ps_TMSTSFIL[PV_SYS_TM_BIO_SET_POW-1].tmVal;
	}
	else
	{
		*p_setPow = 0;
	}

	/* 計測値保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_TMSTSFIL);

	return retCode;
}

