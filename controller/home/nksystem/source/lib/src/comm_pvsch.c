/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20251120] 2025.11.20 Ph5品証指摘No.37対応
 * ・スケジュールなし(-1)がスケジュール管理情報に更新されない問題の対応
 * [20251027] 2025.10.27
 * ・外部機器から取得したスケジュール値を取り込むように修正
 * [20170411] 2017.04.11
 * ・スケジュールデータの取得において、時刻指定に対応した
 * [20181221] 2018.12.21
 * ・ID登録結果が登録済み以外は異常としていが、未登録のみ異常とするように修正
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 太陽光モニタリングシステム
 * -----------------------------------------------------------------------------
 * 共通関数：スケジュール管理情報関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・スケジュール管理譲歩う保存ファイルの情報を取得する
 */
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"

#if 0
#define	PV_DEBUG_CDT								/* デバッグフラグ：CDTスケジュール */
#endif


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * スケジュール制御のエラーチェック
 *
 * 機能：
 * 		スケジュール制御にエラー情報を返却する。
 *		出力抑制に非対応の場合は、非対応(0)を返却する。
 * 戻り値：
 * 		 1以上：スケジュール情報正常
 * 		 0    ：スケジュール制御非対応
 *		 0未満：スケジュール情報異常
 */
int commfunc_pvsch_ErrCheck( int outCtlType )
{
	int					retCode;			/* 処理結果 */
	time_t 				u_time;
	struct tm 			timeData;

	/* 発電所IDが登録されていない場合は、出力抑制非対応 */
	if( ps_gpt_SCHCTLFIL[outCtlType].plantId[0] == 0 )
	{
		/* 出力抑制無効 */
		retCode = 0;
	}
	else
	{
		/* 積算秒を取得 */
		u_time = time( NULL );
		/* 時刻情報に変換 */
		localtime_r( &u_time, &timeData );

		/* 時刻(年)が2000年以下の場合 */
		if(( timeData.tm_year + 1900 ) <= 2000)
		{
			retCode = -1;
		}
/* [20181221] Chg */
		/* ID登録結果が未登録の場合 */
		else if( ps_gpt_SCHCTLFIL[outCtlType].idRegist == 0 )
		{
			retCode = -1;
		}
		else
		{
			retCode = 1;
		}
	}

	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * スケジュール情報取得
 *
 * 機能：
 * 		指定された時刻（UNIXタイム）のスケジュール情報を返却する
 *		時刻が０の場合は、現在時刻のスケジュール情報を返却する。
 * 戻り値：
 * 		PV_SCH_TYPE_NONE		(0)	：スケジュール種別（なし）
 *		PV_SCH_TYPE_FIXED		(1)	：スケジュール種別（固定）
 *		PV_SCH_TYPE_UPDATE		(2)	：スケジュール種別（更新）
 */
int commfunc_pvsch_GetData( time_t *reftime, int outCtlType, struct st_schdata * pSchData  )	/* [20170411] Chg */
{
	struct st_schdata	*schData = NULL;			/* スケジュールデータポインタ */
	time_t 				newYearTime;				/* 1/1 0:00の積算秒 */
	struct tm 			timeData;					/* 時刻情報 */
    int					diff_time;					/* 1/1 0:00からの差分(秒) */
    int					data_pos;					/* データ位置 */
	int					buffSelect;					/* バッファ使用面 */
	int					loopCnt;					/* ループカウンタ */
	char				schType = PV_SCH_TYPE_NONE;	/* スケジュール種別（0:なし、1:固定、2:更新）  */
	char				dataVal = 0;				/* 出力制御量（％） */

	/* スケジュール管理情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_SCHCTLFIL );
	/* 時刻指定がない場合 */
	if( reftime == NULL )
	{
		/* 現在時刻のスケジュールを参照する */
		schData = &ps_gpt_SCHCTLFIL[outCtlType].s_schinfo[ps_gpt_SCHCTLFIL[outCtlType].buffSelect].s_schdata[ps_gpt_SCHCTLFIL[outCtlType].dataPos];
	}
	else
	{
		/* 時刻情報に変換 */
		localtime_r( reftime, &timeData );

		/* バッファ面を検索 */
		for( loopCnt = 0; loopCnt < PV_SCH_BUFF_NUM; loopCnt++ )
		{
			if( ps_gpt_SCHCTLFIL[outCtlType].s_schinfo[loopCnt].schYear == (timeData.tm_year+1900) )
			{
				break;
			}
		}

		/* 指定時刻のバッファ面がある場合 */
		if( loopCnt != PV_SCH_BUFF_NUM )
		{
		    timeData.tm_mon = 0;     /* 月 ( 1月＝0 ) */
		    timeData.tm_mday = 1;    /* 日 */
		    timeData.tm_hour = 0;    /* 時 */
		    timeData.tm_min = 0;     /* 分 */
		    timeData.tm_sec = 0;     /* 秒 */

			/* 1/1 0:00の積算秒を取得 */
		    newYearTime = mktime(&timeData);

			/* 現在時刻との差分を算出 */
			diff_time = *reftime - newYearTime;

			/* スケジュールデータ位置を算出 */
			data_pos = (int)(diff_time/PV_SCH_TI_SEC);

			/* 指定時刻のスケジュールを参照する */
			schData = &ps_gpt_SCHCTLFIL[outCtlType].s_schinfo[loopCnt].s_schdata[data_pos];
		}
	}

	/* スケジュールデータがある場合 */
	if( schData != NULL )
	{
		/* スケジュール種別を更新 */
		schType = schData->schType;
		memcpy(pSchData, schData, sizeof(struct st_schdata));
	}

	/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );

	/* スケジュール種別を返却 */
	return schType;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/* 
 * スケジュール情報取得
 *
 * 機能：
 * 		発電所の出力変化時間を返却する
 * 戻り値：
 * 		0    ：正常
 *		0以外：異常
 * 		time ：5～10
 */
int commfunc_pvsch_GetChgTime( int outCtlType, int * time )
{
	/* 出力変化時間が0より大きい場合 */
	if( ps_gpt_SCHCTLFIL[outCtlType].chgTime > 0 )
	{
		/* 出力変化時間を設定 */
		*time = ps_gpt_SCHCTLFIL[outCtlType].chgTime;

		/* 正常 */
		return 0;
	}
	else
	{
		/* 異常 */
		return -1;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログ用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_ctl_johen_msggetlog(
 * 			char (*johenMsgLog)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgLog で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgLog
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_pvsch_msggettcp( char (*schMsgLog)[PV_MSG_SIZE_SCH], int outCtlType )
{
	struct st_schdata	*pSchData = NULL;			/* スケジュールデータポインタ */
	time_t 				newYearTime;				/* 1/1 0:00の積算秒 */
	struct tm 			timeData;					/* 時刻情報 */
    int					diff_time;					/* 1/1 0:00からの差分(秒) */
    int					data_pos;					/* データ位置 */
	int					buffSelect;					/* バッファ使用面 */
	struct tm			buffTime;					/* 時刻情報 */
	int					loopCnt1;					/* ループカウンタ */
	int					loopCnt2;					/* ループカウンタ */
	int					updateCnt;					/* 更新データ数 */
	int					dataCnt=0;					/* スケジュール格納日数 */
	char				cSchData[256];				/* 1日分のデータ */
	time_t 				u_nowTime;					/* 現在時刻 */
	time_t 				u_dayTime;					/* スケジュール取得時刻 */

	/* スケジュール管理情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_SCHCTLFIL );

	/* 積算秒を取得 */
	u_nowTime = time( NULL );

	for( loopCnt1 = 0; loopCnt1 < 7; loopCnt1++ )
	{
		/* 日付を更新 */
		u_dayTime = u_nowTime + (loopCnt1 * (24 * 60 * 60 ));

		/* 時刻情報に変換 */
		localtime_r( &u_dayTime, &timeData );

		/* バッファ面を検索 */
		for( buffSelect = 0; buffSelect < PV_SCH_BUFF_NUM; buffSelect++ )
		{
			if( ps_gpt_SCHCTLFIL[outCtlType].s_schinfo[buffSelect].schYear == (timeData.tm_year+1900) )
			{
				break;
			}
		}

		/* 指定時刻のバッファ面がある場合 */
		if( buffSelect != PV_SCH_BUFF_NUM )
		{
			buffTime = timeData;
		    buffTime.tm_mon = 0;     /* 月 ( 1月＝0 ) */
		    buffTime.tm_mday = 1;    /* 日 */
		    buffTime.tm_hour = 0;    /* 時 */
		    buffTime.tm_min = 0;     /* 分 */
		    buffTime.tm_sec = 0;     /* 秒 */

			/* 1/1 0:00の積算秒を取得 */
		    newYearTime = mktime(&buffTime);

			/* 現在時刻との差分を算出 */
			diff_time = u_dayTime - newYearTime;

			/* 差分から00:00:00となるように端数を切り捨てる */
			diff_time -= (diff_time % (24 * 60 * 60 ));

			/* スケジュールデータ位置を算出 */
			data_pos = (int)(diff_time/PV_SCH_TI_SEC);


			sprintf(cSchData, "SCH,%04d%02d%02d,%d",
				timeData.tm_year + 1900,
				timeData.tm_mon + 1,
				timeData.tm_mday,
				outCtlType);

			/* 更新データ数を初期化 */
			updateCnt = 0;

			/* 48コマ分ループ */
			for( loopCnt2 = 0; loopCnt2 < PV_DAY_SCH_NUM; loopCnt2++ )
			{
				/* 指定時刻のスケジュールを参照する */
				pSchData = &ps_gpt_SCHCTLFIL[outCtlType].s_schinfo[buffSelect].s_schdata[data_pos];

				/* 更新データがある場合 */
				if( pSchData->update == PV_FLG_STS_SET )
				{
					/* 更新データをカウント */
					updateCnt++;
					pSchData->update = PV_FLG_STS_RESET;
				}

				/* データがない場合 */
				if( pSchData->schType == PV_SCH_TYPE_NONE )
				{
					sprintf(&cSchData[strlen(cSchData)], ",%d", ps_gpt_SCHCTLFIL[outCtlType].schNonPer);		/* [20251120] Chg */
				}
				else
				{
					/* データを格納 */
					sprintf(&cSchData[strlen(cSchData)], ",%d", pSchData->data);
				}
				data_pos++;
			}

			/* 更新データが1つでもある場合 */
			if( updateCnt > 0 )
			{
				/* データを格納 */
				sprintf(schMsgLog[dataCnt], "%s", cSchData );
				dataCnt++;
			}
		}
	}

	/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );

	
	return( dataCnt );
}

/* [20251027] Add St */
/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * スケジュールデータを反映する
 *
 * 機能：
 * 		スケジュールデータを管理情報に反映する
 * 戻り値：
 */
int commfunc_set_sch_data( int outCtlType, time_t setDate, int *p_schData )
{
	int						buffSelect;
	int						setDataNum;
	int						dataPos;
	int						retCode = 0;
    struct tm				s_timeData;
    struct tm				e_timeData;
    time_t 					s_time;
    time_t 					e_time;
    int						target_year;
	struct st_schdata		*ptr_schData;
	int						loopCnt;					/* ループカウンタ */
	struct st_SCHCTLFIL     *ps_SCHCTLFIL;
	struct st_schdata		schData;

	/* スケジュール管理情報保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_SCHCTLFIL );

	/* 引数の時刻から対象日の00:00の時刻を設定 */
	localtime_r( &setDate, &e_timeData );
    e_timeData.tm_hour = 0;
    e_timeData.tm_min  = 0;
    e_timeData.tm_sec  = 0;

	/* 引数の時刻の年の1/1 0:00の時刻を設定 */
	memcpy(&s_timeData, &e_timeData, sizeof(e_timeData));
	s_timeData.tm_mon = 0;
	s_timeData.tm_mday = 1;

	/* それぞれの時刻を秒に変換 */
    s_time = mktime(&s_timeData);
    e_time = mktime(&e_timeData);

	/* 秒の差分を30分コマ換算し、1/1 00:00からのコマ数を算出 */
	dataPos = (int)((e_time - s_time)/PV_SCH_TI_SEC);

	/* 現在年を設定 */
	target_year = e_timeData.tm_year + 1900;

	ps_SCHCTLFIL = &ps_gpt_SCHCTLFIL[outCtlType];

	/* 書き込みバッファ面取得 */
	/* スケジュールデータが今年の場合 */
	if( ps_SCHCTLFIL->s_schinfo[ps_SCHCTLFIL->buffSelect].schYear == target_year )
	{
		buffSelect = ps_SCHCTLFIL->buffSelect;
	}
	/* スケジュールデータが翌年の場合 */
	else if( (ps_SCHCTLFIL->s_schinfo[ps_SCHCTLFIL->buffSelect].schYear + 1) == target_year )
	{
		/* バッファを切り換える */
		buffSelect = (ps_SCHCTLFIL->buffSelect+1) % PV_SCH_BUFF_NUM;

		/* バッファの開始年がスケジュール年と異なる場合 */
		if( ps_SCHCTLFIL->s_schinfo[buffSelect].schYear != target_year)
		{
			/* バッファを初期化する */
			memset( &ps_SCHCTLFIL->s_schinfo[buffSelect], 0x00, sizeof(struct st_schinfo));
			ps_SCHCTLFIL->s_schinfo[buffSelect].schYear = target_year;
		}
	}
	else
	{
		/* 現在のバッファを初期化して使用する */
		buffSelect = ps_SCHCTLFIL->buffSelect;
		memset( &ps_SCHCTLFIL->s_schinfo[buffSelect], 0x00, sizeof(struct st_schinfo));
		ps_SCHCTLFIL->s_schinfo[buffSelect].schYear = target_year;
	}

	/* 書き込み開始ポインタを設定 */
	ptr_schData = &ps_SCHCTLFIL->s_schinfo[buffSelect].s_schdata[dataPos];

	/* データ数分繰り返す */
	for( loopCnt = 0; loopCnt < 48; loopCnt++ )
	{
		/* 更新前の情報を保持 */
		memcpy(&schData, ptr_schData, sizeof(schData));


		/* 出力制御率の範囲内の場合のみ更新する */
		if( ( *p_schData >= 0 ) &&
			( *p_schData <= 100 ))
		{
			/* 該当スケジュールを更新 */
			ptr_schData->schType = PV_SCH_TYPE_UPDATE;
			ptr_schData->data = *p_schData;
		}
/* [20251120] Add St */
		/* 出力制御率が範囲外の場合は、スケジュールなしとする */
		else
		{
			/* スケジュール種別をなしに設定 */
			ptr_schData->schType = PV_SCH_TYPE_NONE;
		}
/* [20251120] Add Ed */

		/* データが更新されていた場合はセットする */
		if( ( schData.schType != ptr_schData->schType ) ||
			( schData.data != ptr_schData->data ))
		{
			ptr_schData->update = PV_FLG_STS_SET;
		}

		p_schData++;
		ptr_schData++;
	}

	/* スケジュール管理情報保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_SCHCTLFIL );

	return retCode;

}
/* [20251027] Add Ed */
