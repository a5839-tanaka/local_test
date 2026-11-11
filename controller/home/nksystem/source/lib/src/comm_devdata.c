/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20260515] 2026.05.15
 * ・一次調整力の実績データファイルをdatファイルで運用するように変更
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 共通関数：装置データ処理関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・各装置が取得した計測値を元に移動平均を計算
 */
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvserial.h"
#include "pvdevdata.h"
#include "pvcomm.h"

#if 0
#define PV_DEBUG
#define PV_DEBUG_AVE
#define PV_DEBUG_STOCK
#endif


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置情報取得処理
 *
 * 機能：
 * 		引数 lineNo で指定された通信回線番号に所属する装置を引数 mKind で指定さ
 *		れた装置種別に従い該当する装置の情報ファイル（XXXXXINF.dat）から検索し、
 *		引数 pdevInfoPointer の構造体にセットする。
 * 		セットする装置情報は引数 ps_devTypeName に格納されている型式のみとする。
 * 		該当する装置情報が存在した場合は戻り値に装置情報数がセットされる。
 *		引数 pdevId2Numb に NULL 以外の値が設定されている場合は、装置IDから装置
 *		番号を逆引きできる情報をセットする。
 *		引数 pdevId2Index に NULL 以外の値が設定されている場合は、装置IDから情
 *		報の格納位置を逆引きできる情報をセットする。
 *		装置種別が多回路エネルギーモニタの場合は、所属回の実装未実装情報をセッ
 *		トする
 * 戻り値：
 * 		 0 ：正常処理（該当なし）
 * 		 0<：正常終了（処理対象の装置情報数）
 * 		-1 ：異常終了
 */
int	commfunc_devInfo_get(
int						mKind,				/* 装置種別（入力） */
int						lineNo,				/* 所属回線番号（入力） */
struct st_devTypeName	*ps_devTypeName,	/* 処理対象型式名称テーブル（入力） */
void					*pdevInfoPointer,	/* 装置情報（出力） */
int						*preqCounter,		/* 要求回数（出力） */
int						*pdevId2Numb,		/* 装置ID番号－装置番号検索用（出力） */
int						*pdevId2Index,		/* 装置ID番号－インデックス番号検索用（出力） */
void					*filePointer )		/* 情報ファイルポインタ（入力） */
{
	char					typeName[PV_DEVDATA_TNAME]; /* 型式名称処理用 */
	int						statFlagCheck;	/* 実装/未実装確認 */
	int						lineNumbCheck;	/* 所属回線番号 */
	int						idNumb;			/* 装置ID番号 */
	int						findFlag = 0;	/* 検出フラグ */
	int						findCnt  = 0;	/* 処理対象検出数 */
	int						maxloop;		/* 最大処理数 */
	int						reqWork;		/* 要求回数ワーク */
	int						loopCnt1;		/* ループカウンタ */
	int						loopCnt2;		/* ループカウンタ */
	struct st_stdDevInfo	*ps_stdDevInfo;	/* 装置情報（標準） */
	struct st_BATTEINF		*ps_BATTEINF;	/* バッテリー情報ファイル */
	struct st_POWCNINF		*ps_POWCNINF;	/* パワーコンディショナ情報ファイル */
	struct st_MLTMTINF		*ps_MLTMTINF;	/* マルチメータ情報ファイル */
	struct st_REMIOINF		*ps_REMIOINF;	/* 汎用リモートIO装置情報ファイル */
	struct st_PLCFAINF		*ps_PLCFAINF;	/* ＰＬＣ情報ファイル */

#ifdef PV_DEBUG
printf("commfunc_devInfo_get() : MKIND[%d], LINE[%d]\n", mKind, lineNo );
#endif

	/* 装置種別毎の初期値設定 */
	switch( mKind )
	{
		case PV_MKIND_PVSYS:	/* 装置種別: パワーコンディショナ */
			/* 初期化 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			memset( ps_stdDevInfo, 0, sizeof(struct st_stdDevInfo) * PV_MAX_PVSYS );
			ps_stdDevInfo[0].devNumb  = 1;
			ps_stdDevInfo[0].idNumb   = 1;
			return 1;
			break;
		case PV_MKIND_POWCN:	/* 装置種別: パワーコンディショナ */
			maxloop = PV_MAX_POWCN;
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_POWCNINF = (struct st_POWCNINF *)filePointer;
			break;
		case PV_MKIND_BATTE:	/* 装置種別: バッテリー */
			maxloop = PV_MAX_BATTE;
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_BATTEINF = (struct st_BATTEINF *)filePointer;
			break;
		case PV_MKIND_MLTMT:	/* 装置種別: マルチメータ */
			maxloop = PV_MAX_MLTMT;
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_MLTMTINF = (struct st_MLTMTINF *)filePointer;
			break;
		case PV_MKIND_REMIO:	/* 装置種別: 汎用リモートIO装置 */
			maxloop = PV_MAX_REMIO;
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_REMIOINF = (struct st_REMIOINF *)filePointer;
			break;
		case PV_MKIND_PLCFA:	/* 装置種別: ＰＬＣ装置 */
			maxloop = PV_MAX_PLCFA;
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_PLCFAINF = (struct st_PLCFAINF *)filePointer;
			break;
		default:				/* 装置種別:上記以外の場合 */
			return( -1 );
			break;
	}

	/* 要求回数の初期化 */
	*preqCounter = 0;

	/* 所属回線番号チェック */
	if( (lineNo < 0) || (lineNo > PV_MAX_COMLINE) )
	{
		return( -1 );
	}

	/* 初期化 */
	memset( ps_stdDevInfo, 0, sizeof(struct st_stdDevInfo) * maxloop );

	/* 装置ID－装置番号が必要な場合 */
	if( pdevId2Numb != NULL )
	{
		/* 装置ID-パワコン番号変換テーブルを初期化 */
		memset( pdevId2Numb, 0 , sizeof(int) * PV_MAX_DEVICE );
	}

	/* 装置ID－インデックス番号が必要な場合 */
	if( pdevId2Index != NULL )
	{
		/* 装置ID-インデックス番号変換テーブルを初期化 */
		memset( pdevId2Index, 0 , sizeof(int) * PV_MAX_DEVICE );
	}

	/* 装置数分繰り返す */
	for(loopCnt1 = 0 ; loopCnt1 <  maxloop ; loopCnt1++)
	{
		statFlagCheck = 0;
		lineNumbCheck = 0;
		reqWork       = 0;

		/* 装置種別毎の確認 */
		switch( mKind )
		{
			case PV_MKIND_POWCN:
				/* 実装/未実装情報の取得 */
				statFlagCheck = ps_POWCNINF[loopCnt1].statFlag;
				/* 所属回線番号の取得 */
				lineNumbCheck = ps_POWCNINF[loopCnt1].lineNumb;
				/* 型式名称の取得 */
				strncpy( typeName, ps_POWCNINF[loopCnt1].typeName, 32 );
				/* 装置IDの取得 */
				idNumb = ps_POWCNINF[loopCnt1].idNumb;
				break;
			case PV_MKIND_BATTE:
				/* 実装/未実装情報の取得 */
				statFlagCheck = ps_BATTEINF[loopCnt1].statFlag;
				/* 所属回線番号の取得 */
				lineNumbCheck = ps_BATTEINF[loopCnt1].lineNumb;
				/* 型式名称の取得 */
				strncpy( typeName, ps_BATTEINF[loopCnt1].typeName, 32 );
				/* 装置IDの取得 */
				idNumb = ps_BATTEINF[loopCnt1].idNumb;
				break;
			case PV_MKIND_MLTMT:
				/* 実装/未実装情報の取得 */
				statFlagCheck = ps_MLTMTINF[loopCnt1].statFlag;
				/* 所属回線番号の取得 */
				lineNumbCheck = ps_MLTMTINF[loopCnt1].lineNumb;
				/* 型式名称の取得 */
				strncpy( typeName, ps_MLTMTINF[loopCnt1].typeName, 32 );
				/* 装置IDの取得 */
				idNumb = ps_MLTMTINF[loopCnt1].idNumb;
				break;
			case PV_MKIND_REMIO:
				/* 実装/未実装情報の取得 */
				statFlagCheck = ps_REMIOINF[loopCnt1].statFlag;
				/* 所属回線番号の取得 */
				lineNumbCheck = ps_REMIOINF[loopCnt1].lineNumb;
				/* 型式名称の取得 */
				strncpy( typeName, ps_REMIOINF[loopCnt1].typeName, 16 );
				/* 装置IDの取得 */
				idNumb = ps_REMIOINF[loopCnt1].idNumb;
				break;
			case PV_MKIND_PLCFA:
				/* 実装/未実装情報の取得 */
				statFlagCheck = ps_PLCFAINF[loopCnt1].statFlag;
				/* 所属回線番号の取得 */
				lineNumbCheck = ps_PLCFAINF[loopCnt1].lineNumb;
				/* 型式名称の取得 */
				strncpy( typeName, ps_PLCFAINF[loopCnt1].typeName, 16 );
				/* 装置IDの取得 */
				idNumb = ps_PLCFAINF[loopCnt1].idNumb;
				break;
		}

		/* 未実装の場合 */
		if( statFlagCheck == 0 )
			continue;

		/* 所属回線番号が異なる場合 */
		if( lineNo != lineNumbCheck )
			continue;

		findFlag = 0;

		/* 対応機種テーブルから探す */
		for( loopCnt2 = 0 ; ps_devTypeName[loopCnt2].devType != -1 ; loopCnt2++ )
		{
			/* 型式名称が一致した場合 */
			if( strncmp( ps_devTypeName[loopCnt2].typeName, typeName, 32 ) ==  0 )
			{
				/* 処理タイプを設定 */
				ps_stdDevInfo[findCnt].devType = ps_devTypeName[loopCnt2].devType;

				/* 基本要求回数を設定 */
				ps_stdDevInfo[findCnt].reqCnt = ps_devTypeName[loopCnt2].reqCnt;

				/* 基本要求回数を設定 */
				reqWork = ps_devTypeName[loopCnt2].reqCnt;

				/* 検出フラグON */
				findFlag = 1;

				/* 検索処理を抜ける */
				break;
			}
		}

		/* 対象装置が見検出の場合 */
		if( findFlag == 0 )
			continue;

		/* 要求回数を加算 */
		if( (reqWork > 0 ) && ( reqWork < PV_SERIAL_REQUEST_MAX ) )
			*preqCounter += PV_SERIAL_REQUEST_MAX;
		else
			*preqCounter += reqWork;

		/* 装置番号の設定 */
		ps_stdDevInfo[findCnt].devNumb  = loopCnt1 + 1;
		/* 装置IDの設定 */
		ps_stdDevInfo[findCnt].idNumb   = idNumb;
		/* 処理対象装置検出数を＋１ */

		findCnt++;

		if( pdevId2Numb != NULL )
		{
			/* 装置ID-装置番号変換テーブルに装置番号を設定 */
			pdevId2Numb[ idNumb - 1 ] = loopCnt1 + 1;
		}
		if( pdevId2Index != NULL )
		{
			/* 装置ID-インデックス番号変換テーブルにインデックス値を設定 */
			pdevId2Index[ idNumb - 1 ] = findCnt;
		}

#ifdef PV_DEBUG
printf("REQEST COUNTER ( LineNo:%02d, DevNumb:%02d ) : %d\n", lineNo, loopCnt1 + 1, *preqCounter );
#endif

	}

#ifdef PV_DEBUG
printf("REQEST COUNTER TOTAL : %d\n", *preqCounter );
#endif

	return( findCnt );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 移動平均データの初期化
 *
 * 機能：
 *		引数 mKind で指定された装置種別に合わせ、引数 pdevAvlDataPointer で指定
 *		された移動平均データの格納エリアを初期化（0クリア）する。
 * 戻り値：
 * 		0：正常処理
 */
int commfunc_devAvlData_init(
int		mKind,										/* 装置種別（入力） */
int		stocMax,									/* 平均値算出用保存回数（入力） *//* 未使用 */
void	*pdevAvlDataPointer )						/* 移動平均値データ格納領域（出力） */
{
	int						maxDevCnt;				/* 処理装置数 */
	int						maxTmCnt;				/* 処理TM数 */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	int						loopCnt3;				/* ループカウンタ */
	struct st_sysAvlData	*ps_sysAvlData;			/* 移動平均値データ（パワーコンディショナ） */
	struct st_pcsAvlData	*ps_pcsAvlData;			/* 移動平均値データ（パワーコンディショナ） */
	struct st_bttAvlData	*ps_bttAvlData;			/* 移動平均値データ（バッテリー） */
	struct st_mltAvlData	*ps_mltAvlData;			/* 移動平均値データ（マルチメータ） */
	struct st_remAvlData	*ps_remAvlData;			/* 移動平均値データ（汎用リモートIO装置） */
	struct st_plcAvlData	*ps_plcAvlData;			/* 移動平均値データ（ＰＬＣ装置） */

	/* 移動平均用データの初期化 */
	switch( mKind )
	{
		case PV_MKIND_PVSYS:
			ps_sysAvlData = (struct st_sysAvlData *)pdevAvlDataPointer;
			memset( ps_sysAvlData, 0, sizeof(struct st_sysAvlData) * PV_MAX_PVSYS );
			maxDevCnt = PV_MAX_PVSYS;
			maxTmCnt  = PV_MAX_PVSYS_TMPOS;
			break;
		case PV_MKIND_POWCN:
			ps_pcsAvlData = (struct st_pcsAvlData *)pdevAvlDataPointer;
			memset( ps_pcsAvlData, 0, sizeof(struct st_pcsAvlData) * PV_MAX_POWCN );
			maxDevCnt = PV_MAX_POWCN;
			maxTmCnt  = PV_MAX_POWCN_TMPOS;
			break;
		case PV_MKIND_BATTE:
			ps_bttAvlData = (struct st_bttAvlData *)pdevAvlDataPointer;
			memset( ps_bttAvlData, 0, sizeof(struct st_bttAvlData) * PV_MAX_BATTE );
			maxDevCnt = PV_MAX_BATTE;
			maxTmCnt  = PV_MAX_BATTE_TMPOS;
			break;
		case PV_MKIND_MLTMT:
			ps_mltAvlData = (struct st_mltAvlData *)pdevAvlDataPointer;
			memset( ps_mltAvlData, 0, sizeof(struct st_mltAvlData) * PV_MAX_MLTMT );
			maxDevCnt = PV_MAX_MLTMT;
			maxTmCnt  = PV_MAX_MLTMT_TMPOS;
			break;
		case PV_MKIND_REMIO:
			ps_remAvlData = (struct st_remAvlData *)pdevAvlDataPointer;
			memset( ps_remAvlData, 0, sizeof(struct st_remAvlData) * PV_MAX_REMIO );
			maxDevCnt = PV_MAX_REMIO;
			maxTmCnt  = PV_MAX_REMIO_TMPOS;
			break;
		case PV_MKIND_PLCFA:
			ps_plcAvlData = (struct st_plcAvlData *)pdevAvlDataPointer;
			memset( ps_plcAvlData, 0, sizeof(struct st_plcAvlData) * PV_MAX_PLCFA );
			maxDevCnt = PV_MAX_PLCFA;
			maxTmCnt  = PV_MAX_PLCFA_TMPOS;
			break;
		default:
			return( -1 );
			break;
	}

	/* 装置最大数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < maxDevCnt ; loopCnt1++ )
	{
		/* 装置１台あたりの計測情報数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < maxTmCnt ; loopCnt2++ )
		{
			switch( mKind )
			{
				case PV_MKIND_PVSYS:
					/* 移動平均演算結果の計測フラグに欠測を設定 */
					ps_sysAvlData[loopCnt1].tmDataAvlFlg[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_POWCN:
					/* 移動平均演算結果の計測フラグに欠測を設定 */
					ps_pcsAvlData[loopCnt1].tmDataAvlFlg[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_BATTE:
					/* 移動平均演算結果の計測フラグに欠測を設定 */
					ps_bttAvlData[loopCnt1].tmDataAvlFlg[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_MLTMT:
					/* 移動平均演算結果の計測フラグに欠測を設定 */
					ps_mltAvlData[loopCnt1].tmDataAvlFlg[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_REMIO:
					/* 移動平均演算結果の計測フラグに欠測を設定 */
					ps_remAvlData[loopCnt1].tmDataAvlFlg[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_PLCFA:
					/* 移動平均演算結果の計測フラグに欠測を設定 */
					ps_plcAvlData[loopCnt1].tmDataAvlFlg[loopCnt2] = PV_TMFLG_DROPTM;
					break;
			}
		}
	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * １分間の移動平均演算処理
 *
 * 機能：
 * 		引数 pcsMaxCnt で指定された台数分の計測情報が引数 ps_bttData に示される
 * 		アドレスに格納されているので、これらの計測情報を元に、引数 ps_bttAvlData 
 * 		に示すアドレスに計測情報を格納した後、１分間の移動平均値を演算する。
 * 		移動平均は計測種別が瞬時値のみ対象とし、積算値は除外する。
 * 戻り値：
 * 		0：正常処理
 */
//int commfunc_devAvlData_get(
int commfunc_devTmData_set(
int						mKind,						/* 装置種別（入力） */
int						devNumb,					/* 処理装置番号（入力） */
int						maxTmCnt,					/* 処理最大計測情報数（１台あたりの最大） */
void 					*pdevDataPointer,			/* 取得データ（入力） */
void 					*pdevAvlDataPointer,		/* 移動平均算出用データ（出力） */
struct st_TMDATINF		*ps_TMDATINF )				/* 計測情報ファイルポインタ（入力） */
{
	int				tmKind;							/* 計測種別 */
	int				tmLink;							/* 計測情報ファイルリンク */
	int				stocPtr;						/* 移動平均ストック格納位置 */
	int				loopCnt1;						/* ループカウンタ */

	struct st_sysData		*ps_sysData;			/* 計測値データ（収集装置） */
	struct st_bttData		*ps_bttData;			/* 計測値データ（バッテリー） */
	struct st_pcsData		*ps_pcsData;			/* 計測値データ（パワーコンディショナ） */
	struct st_mltData		*ps_mltData;			/* 計測値データ（マルチメータ） */
	struct st_remData		*ps_remData;			/* 計測値データ（汎用リモートIO装置） */
	struct st_plcData		*ps_plcData;			/* 計測値データ（ＰＬＣ装置） */

	struct st_sysAvlData	*ps_sysAvlData;			/* 移動平均値データ（収集装置） */
	struct st_pcsAvlData	*ps_pcsAvlData;			/* 移動平均値データ（パワーコンディショナ） */
	struct st_bttAvlData	*ps_bttAvlData;			/* 移動平均値データ（バッテリー） */
	struct st_mltAvlData	*ps_mltAvlData;			/* 移動平均値データ（マルチメータ） */
	struct st_remAvlData	*ps_remAvlData;			/* 移動平均値データ（汎用リモートIO装置） */
	struct st_plcAvlData	*ps_plcAvlData;			/* 移動平均値データ（ＰＬＣ装置）  */


	ps_sysData = (struct st_sysData *)pdevDataPointer;
	ps_pcsData = (struct st_pcsData *)pdevDataPointer;
	ps_bttData = (struct st_bttData *)pdevDataPointer;
	ps_mltData = (struct st_mltData *)pdevDataPointer;
	ps_remData = (struct st_remData *)pdevDataPointer;
	ps_plcData = (struct st_plcData *)pdevDataPointer;

	ps_sysAvlData = (struct st_sysAvlData *)pdevAvlDataPointer;
	ps_pcsAvlData = (struct st_pcsAvlData *)pdevAvlDataPointer;
	ps_bttAvlData = (struct st_bttAvlData *)pdevAvlDataPointer;
	ps_mltAvlData = (struct st_mltAvlData *)pdevAvlDataPointer;
	ps_remAvlData = (struct st_remAvlData *)pdevAvlDataPointer;
	ps_plcAvlData = (struct st_plcAvlData *)pdevAvlDataPointer;

	/* 計測情報ファイルのリンク位置の取得 */
	tmLink  = ps_gpt_PVSYSINF->s_machineInfo[mKind].tmIfLink + (devNumb - 1) * ps_gpt_PVSYSINF->s_machineInfo[mKind].maxTm;

	/* 計測情報数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
	{
		/* 計測情報が未実装の場合 */
		if( (ps_TMDATINF + tmLink + loopCnt1)->statFlag == 0 )
			continue;

		/* 計測種別を取得 */
		tmKind = (ps_TMDATINF + tmLink + loopCnt1)->tmKind;

#ifdef PV_DEBUG_AVE
	printf("commfunc_devAvlData_get : DEV,TM[%2d,%3d] TYP:%d ", devNumb, loopCnt1+1, tmKind );
#endif

		/* 装置種別に応じた計測値の処理 */
		switch( mKind )
		{
			case PV_MKIND_PVSYS:
				commfunc_devTmKindData( tmKind,
				                         ps_sysData->tmFlgData[loopCnt1],
				                         ps_sysData->tmValData[loopCnt1],
				                         &ps_sysAvlData->tmDataAvlFlg[loopCnt1],
				                         &ps_sysAvlData->tmDataAvlVal[loopCnt1]);
				break;
			case PV_MKIND_POWCN:
				commfunc_devTmKindData( tmKind,
				                         ps_pcsData->tmFlgData[loopCnt1],
				                         ps_pcsData->tmValData[loopCnt1],
				                         &ps_pcsAvlData->tmDataAvlFlg[loopCnt1],
				                         &ps_pcsAvlData->tmDataAvlVal[loopCnt1] );
				break;
			case PV_MKIND_BATTE:
				commfunc_devTmKindData( tmKind,
				                         ps_bttData->tmFlgData[loopCnt1],
				                         ps_bttData->tmValData[loopCnt1],
				                         &ps_bttAvlData->tmDataAvlFlg[loopCnt1],
				                         &ps_bttAvlData->tmDataAvlVal[loopCnt1] );
				break;
			case PV_MKIND_MLTMT:
				commfunc_devTmKindData( tmKind,
				                         ps_mltData->tmFlgData[loopCnt1],
				                         ps_mltData->tmValData[loopCnt1],
				                         &ps_mltAvlData->tmDataAvlFlg[loopCnt1],
				                         &ps_mltAvlData->tmDataAvlVal[loopCnt1] );
				break;
			case PV_MKIND_REMIO:
				commfunc_devTmKindData( tmKind,
				                         ps_remData->tmFlgData[loopCnt1],
				                         ps_remData->tmValData[loopCnt1],
				                         &ps_remAvlData->tmDataAvlFlg[loopCnt1],
				                         &ps_remAvlData->tmDataAvlVal[loopCnt1] );
				break;
			case PV_MKIND_PLCFA:
				commfunc_devTmKindData( tmKind,
				                         ps_plcData->tmFlgData[loopCnt1],
				                         ps_plcData->tmValData[loopCnt1],
				                         &ps_plcAvlData->tmDataAvlFlg[loopCnt1],
				                         &ps_plcAvlData->tmDataAvlVal[loopCnt1] );
				break;
		}
	}
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測種別毎の演算処理（計測項目単位）
 *
 * 機能：
 *		引数 tmFlg と tmVal の計測情報（計測フラグ、計測値）を stocFlg および
 *		stocVal で指定された領域内の stocPtr で指示された位置に格納する。
 *		計測種別を示す tmKind が積算型の場合は、avlFlg で指定された領域に tmFlg 
 *		の値を格納し、tmFlg が正常の場合は tmVal の値を avlVal の領域に格納する。
 *		tmKind が瞬時値（非平均型）の場合は、tmFlg と tmVal の値を avlFlg と
 *		avlVal で指定された領域に格納する。
 * 戻り値：
 * 		0：正常処理
 */
int commfunc_devTmKindData(
int				tmKind,								/* 計測種別（入力） */
int				tmFlg,								/* 計測値情報：計測フラグ（入力） */
int				tmVal,								/* 計測値情報：計測値（入力） */
int				*avlFlgPtr,							/* 移動平均情報保存位置：計測フラグ（出力） */
int				*avlValPtr	)						/* 移動平均情報保存位置：計測値（出力） */
{
	long long		avlWork;						/* 平均値演算用ワーク */
	int				workFlgData;					/* 最新計測値（フラグ） */
	int				workValData;					/* 最新計測値（計測値） */
	int				flgWork;						/* 平均値演算用ワーク */
	int				stocCnt;						/* 移動平均有効ストック数 */
	int				loopCnt1;						/* ループカウンタ */

	/* 計測種別が積算型の場合 */
	if( tmKind == PV_TM_KIND_COUNT )
	{
		/* 移動平均処理は行わずに瞬時値を保管する */
		*avlFlgPtr = tmFlg;
		/* 積算型の場合は直近の正常取得時の計測値を保持する必要があるため、 */
		/* 計測フラグが正常の場合のみ上書き更新する */
		if( tmFlg == PV_TMFLG_NORMAL )
		{
			*avlValPtr = tmVal;
		}
	}
	/* 計測種別が瞬時値型の場合 */
	else //if( tmKind == PV_TM_KIND_VALUE )
	{
		/* 移動平均処理は行わずに瞬時値を保管する */
		*avlFlgPtr = tmFlg;
		*avlValPtr = tmVal;
	}

#ifdef PV_DEBUG_AVE
//printf(": AVL[%Xh:%9d]\n", *avlFlgPtr, *avlValPtr );
printf("commfunc_devTmKindData: AVL[%Xh:%9d]\n", *avlFlgPtr, *avlValPtr );
#endif

	return( 0 );

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 計測情報保存処理
 *
 * 機能：
 * 		引数 pdevDataPointer に示されるアドレスに格納されている計測情報と、引数
 * 		pdevAvlDataPointer に示すアドレスに格納されている１分間の移動平均値を計
 *		測情報保存ファイル（TMSTSFIL.dat）に保存する。
 *		装置種別毎にデータの構成が異なるため、引数 mKind で指定された装置種別に
 *		従い処理を行う。
 */
int	commfunc_tmstsfil_devset(
int					mKind,							/* 装置種別（入力） */
int					devNumb,						/* 処理対象装置番号（入力） */
int					maxTmCnt,						/* 処理対象TM数（入力） */
void				*pdevDataPointer,				/* 計測値データ（入力） */
void				*pdevAvlDataPointer,			/* 移動平均値データ（入力） */
struct st_TMDATINF	*ps_TMDATINF,					/* 計測値情報ファイルポインタ（入力） */
struct st_TMSTSFIL	*ps_TMSTSFIL ) 					/* 計測値保存ファイルポインタ（出力） */
{

	int						tmLink;					/* 計測情報ファイルリンク */
	int						loopCnt1;				/* ループカウンタ */

	struct st_sysData		*ps_sysData;			/* 計測値データ（収集装置） */
	struct st_pcsData		*ps_pcsData;			/* 計測値データ（パワーコンディショナ） */
	struct st_bttData		*ps_bttData;			/* 計測値データ（バッテリー） */
	struct st_mltData		*ps_mltData;			/* 計測値データ（マルチメータ） */
	struct st_remData		*ps_remData;			/* 計測値データ（汎用リモートIO装置） */
	struct st_plcData		*ps_plcData;			/* 計測値データ（ＰＬＣ装置） */

	struct st_sysAvlData	*ps_sysAvlData;			/* 移動平均値データ（収集装置） */
	struct st_pcsAvlData	*ps_pcsAvlData;			/* 移動平均値データ（パワーコンディショナ） */
	struct st_bttAvlData	*ps_bttAvlData;			/* 移動平均値データ（バッテリー） */
	struct st_mltAvlData	*ps_mltAvlData;			/* 移動平均値データ（マルチメータ） */
	struct st_remAvlData	*ps_remAvlData;			/* 移動平均値データ（汎用リモートIO装置） */
	struct st_plcAvlData	*ps_plcAvlData;			/* 移動平均値データ（ＰＬＣ装置） */

	/* 装置種別毎の処理 */
	switch( mKind )
	{
		case PV_MKIND_PVSYS:
			/* 装置情報の設定 */
			ps_sysData = (struct st_sysData *)pdevDataPointer;
			ps_sysAvlData = (struct st_sysAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_POWCN:
			/* 装置情報の設定 */
			ps_pcsData = (struct st_pcsData *)pdevDataPointer;
			ps_pcsAvlData = (struct st_pcsAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_BATTE:
			/* 装置情報の設定 */
			ps_bttData = (struct st_bttData *)pdevDataPointer;
			ps_bttAvlData = (struct st_bttAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_MLTMT:
			/* 装置情報の設定 */
			ps_mltData = (struct st_mltData *)pdevDataPointer;
			ps_mltAvlData = (struct st_mltAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_REMIO:
			/* 装置情報の設定 */
			ps_remData = (struct st_remData *)pdevDataPointer;
			ps_remAvlData = (struct st_remAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_PLCFA:
			/* 装置情報の設定 */
			ps_plcData = (struct st_plcData *)pdevDataPointer;
			ps_plcAvlData = (struct st_plcAvlData *)pdevAvlDataPointer;
			break;
		default:
			return( -1 );
			break;
	}

	/* 計測状態保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_TMSTSFIL );

	/* 計測情報ファイルのリンク位置の取得 */
	tmLink  = ps_gpt_PVSYSINF->s_machineInfo[mKind].tmIfLink + (devNumb - 1) * ps_gpt_PVSYSINF->s_machineInfo[mKind].maxTm;

	/* 装置種別毎の処理 */
	/* （可能な限りswitch文の処理を行わない方式とする） */
	switch( mKind )
	{
		case PV_MKIND_PVSYS:
			/* 装置の計測情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
			{
				/* 計測情報が未実装の場合 */
				if( ps_TMDATINF[tmLink + loopCnt1].statFlag == 0 )
					continue;

				/* 瞬時値を保存する（リアル値） */
				ps_TMSTSFIL[tmLink + loopCnt1].tmFlag    = ps_sysData->tmFlgData[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].tmVal     = ps_sysData->tmValData[loopCnt1];
				/* 平均値を保存する（計測種別が瞬時型の場合は移動平均、積算型の場合は直近の正常取得値） */
				ps_TMSTSFIL[tmLink + loopCnt1].avltmFlag = ps_sysAvlData->tmDataAvlFlg[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].avltmVal  = ps_sysAvlData->tmDataAvlVal[loopCnt1];
			}
			break;
		case PV_MKIND_POWCN:
			/* 装置の計測情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
			{
				/* 計測情報が未実装の場合 */
				if( ps_TMDATINF[tmLink + loopCnt1].statFlag == 0 )
					continue;

				/* 瞬時値を保存する（リアル値） */
				ps_TMSTSFIL[tmLink + loopCnt1].tmFlag    = ps_pcsData->tmFlgData[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].tmVal     = ps_pcsData->tmValData[loopCnt1];
				/* 平均値を保存する（計測種別が瞬時型の場合は移動平均、積算型の場合は直近の正常取得値） */
				ps_TMSTSFIL[tmLink + loopCnt1].avltmFlag = ps_pcsAvlData->tmDataAvlFlg[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].avltmVal  = ps_pcsAvlData->tmDataAvlVal[loopCnt1];
			}
			break;
		case PV_MKIND_BATTE:
			/* 装置の計測情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
			{
				/* 計測情報が未実装の場合 */
				if( ps_TMDATINF[tmLink + loopCnt1].statFlag == 0 )
					continue;

				/* 瞬時値を保存する（リアル値） */
				ps_TMSTSFIL[tmLink + loopCnt1].tmFlag    = ps_bttData->tmFlgData[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].tmVal     = ps_bttData->tmValData[loopCnt1];
				/* 平均値を保存する（計測種別が積算型の場合は直近の正常取得値） */
				ps_TMSTSFIL[tmLink + loopCnt1].avltmFlag = ps_bttAvlData->tmDataAvlFlg[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].avltmVal  = ps_bttAvlData->tmDataAvlVal[loopCnt1];
			}
			break;
		case PV_MKIND_MLTMT:
			/* 装置の計測情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
			{
				/* 計測情報が未実装の場合 */
				if( ps_TMDATINF[tmLink + loopCnt1].statFlag == 0 )
					continue;

				/* 瞬時値を保存する（リアル値） */
				ps_TMSTSFIL[tmLink + loopCnt1].tmFlag    = ps_mltData->tmFlgData[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].tmVal     = ps_mltData->tmValData[loopCnt1];
				/* 平均値を保存する（計測種別が積算型の場合は直近の正常取得値） */
				ps_TMSTSFIL[tmLink + loopCnt1].avltmFlag = ps_mltAvlData->tmDataAvlFlg[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].avltmVal  = ps_mltAvlData->tmDataAvlVal[loopCnt1];
			}
			break;
		case PV_MKIND_REMIO:
			/* 装置の計測情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
			{
				/* 計測情報が未実装の場合 */
				if( ps_TMDATINF[tmLink + loopCnt1].statFlag == 0 )
					continue;

				/* 瞬時値を保存する（リアル値） */
				ps_TMSTSFIL[tmLink + loopCnt1].tmFlag    = ps_remData->tmFlgData[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].tmVal     = ps_remData->tmValData[loopCnt1];
				/* 平均値を保存する（計測種別が瞬時型の場合は移動平均、積算型の場合は直近の正常取得値） */
				ps_TMSTSFIL[tmLink + loopCnt1].avltmFlag = ps_remAvlData->tmDataAvlFlg[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].avltmVal  = ps_remAvlData->tmDataAvlVal[loopCnt1];
			}
			break;
		case PV_MKIND_PLCFA:
			/* 装置の計測情報数分繰り返す */
			for( loopCnt1 = 0 ; loopCnt1 < maxTmCnt ; loopCnt1++ )
			{
				/* 計測情報が未実装の場合 */
				if( ps_TMDATINF[tmLink + loopCnt1].statFlag == 0 )
					continue;

				/* 瞬時値を保存する（リアル値） */
				ps_TMSTSFIL[tmLink + loopCnt1].tmFlag    = ps_plcData->tmFlgData[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].tmVal     = ps_plcData->tmValData[loopCnt1];
				/* 平均値を保存する（計測種別が瞬時型の場合は移動平均、積算型の場合は直近の正常取得値） */
				ps_TMSTSFIL[tmLink + loopCnt1].avltmFlag = ps_plcAvlData->tmDataAvlFlg[loopCnt1];
				ps_TMSTSFIL[tmLink + loopCnt1].avltmVal  = ps_plcAvlData->tmDataAvlVal[loopCnt1];
			}
			break;
	}

	/* 計測状態保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_TMSTSFIL );

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 積算計測値の復元処理
 *
 * 機能：
 *		収集装置が一時的に運用停止して再起動した場合に、積算型計測値の計測値を
 *		復元するため、１分間の移動平均値を保存している計測情報保存ファイル
 *		（TMSTSFIL.dat）から取得して、引数 ps_devAvlDataPointer で指定たされた
 *		アドレスの領域に保存する。
 */
int	commfunc_tmstsfil_get(
int					mKind,							/* 装置種別（入力） */
int					devMaxCnt,						/* 処理対象装置最大数（入力） */
int					maxTmCnt,						/* 処理対象TM数（入力） */
void				*pdevInfoPointer,				/* 装置情報（入力） */
void				*pdevDataPointer,				/* 計測値データ（出力） */
void				*pdevAvlDataPointer,			/* 移動平均値データ（出力） */
struct st_TMDATINF	*ps_TMDATINF,					/* 計測値情報ファイルポインタ（入力） */
struct st_TMSTSFIL	*ps_TMSTSFIL )					/* 計測値保存ファイルポインタ（入力） */
{
	int						devNumb;				/* 装置番号 */
	int						tmLink;					/* 計測情報ファイルリンク */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */

	struct st_stdDevInfo	*ps_stdDevInfo;			/* 装置情報（標準） */

	struct st_sysData		*ps_sysData;			/* 計測値データ（収集装置） */
	struct st_pcsData		*ps_pcsData;			/* 計測値データ（パワーコンディショナ） */
	struct st_bttData		*ps_bttData;			/* 計測値データ（バッテリー） */
	struct st_mltData		*ps_mltData;			/* 計測値データ（マルチメータ） */
	struct st_remData		*ps_remData;			/* 計測値データ（汎用リモートIO装置） */
	struct st_plcData		*ps_plcData;			/* 計測値データ（ＰＬＣ装置） */

	struct st_sysAvlData	*ps_sysAvlData;			/* 移動平均値データ（収集装置） */
	struct st_pcsAvlData	*ps_pcsAvlData;			/* 移動平均値データ（パワーコンディショナ） */
	struct st_bttAvlData	*ps_bttAvlData;			/* 移動平均値データ（バッテリー） */
	struct st_mltAvlData	*ps_mltAvlData;			/* 移動平均値データ（マルチメータ） */
	struct st_remAvlData	*ps_remAvlData;			/* 移動平均値データ（汎用リモートIO装置） */
	struct st_plcAvlData	*ps_plcAvlData;			/* 移動平均値データ（ＰＬＣ装置） */

	/* 装置種別毎の処理 */
	switch( mKind )
	{
		case PV_MKIND_PVSYS:
			/* 装置情報の設定 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_sysData = (struct st_sysData *)pdevDataPointer;
			ps_sysAvlData = (struct st_sysAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_POWCN:
			/* 装置情報の設定 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_pcsData = (struct st_pcsData *)pdevDataPointer;
			ps_pcsAvlData = (struct st_pcsAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_BATTE:
			/* 装置情報の設定 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_bttData = (struct st_bttData *)pdevDataPointer;
			ps_bttAvlData = (struct st_bttAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_MLTMT:
			/* 装置情報の設定 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_mltData = (struct st_mltData *)pdevDataPointer;
			ps_mltAvlData = (struct st_mltAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_REMIO:
			/* 装置情報の設定 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_remData = (struct st_remData *)pdevDataPointer;
			ps_remAvlData = (struct st_remAvlData *)pdevAvlDataPointer;
			break;
		case PV_MKIND_PLCFA:
			/* 装置情報の設定 */
			ps_stdDevInfo = (struct st_stdDevInfo *)pdevInfoPointer;
			ps_plcData = (struct st_plcData *)pdevDataPointer;
			ps_plcAvlData = (struct st_plcAvlData *)pdevAvlDataPointer;
			break;
		default:
			return( -1 );
			break;
	}

	/* 計測状態保存ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_TMSTSFIL );

	/* 実装バッテリー数分繰り返す */
	for( loopCnt1 = 0 ; loopCnt1 < devMaxCnt ; loopCnt1++ )
	{
		/* 装置番号の取得 */
			devNumb = ps_stdDevInfo[loopCnt1].devNumb;

		/* 計測情報ファイルのリンク位置の取得 */
		tmLink  = ps_gpt_PVSYSINF->s_machineInfo[mKind].tmIfLink + (devNumb - 1) * ps_gpt_PVSYSINF->s_machineInfo[mKind].maxTm;

		/* 計測情報数分繰り返す */
		for( loopCnt2 = 0 ; loopCnt2 < maxTmCnt ; loopCnt2++ )
		{
			/* 計測情報が未実装の場合 */
			if( ps_TMDATINF[tmLink + loopCnt2].statFlag == 0 )
				continue;

			/* 計測フラグを欠測に設定（装置種別毎の処理） */
			switch( mKind )
			{
				case PV_MKIND_PVSYS:
					ps_sysData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_POWCN:
					ps_pcsData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_BATTE:
					ps_bttData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_MLTMT:
					ps_mltData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_REMIO:
					ps_remData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
					break;
				case PV_MKIND_PLCFA:
					ps_plcData[loopCnt1].tmFlgData[loopCnt2] = PV_TMFLG_DROPTM;
					break;
			}

			/* 計測情報が積算型以外の場合 */
			if( ps_TMDATINF[tmLink + loopCnt2].tmKind != PV_TM_KIND_COUNT )
				continue;

			/* １分平均値から計測値(積算データ)を再現する（装置種別毎の処理） */
			switch( mKind )
			{
				case PV_MKIND_PVSYS:
					ps_sysAvlData[loopCnt1].tmDataAvlVal[loopCnt2] = ps_TMSTSFIL[tmLink + loopCnt2].avltmVal;
					break;
				case PV_MKIND_POWCN:
					ps_pcsAvlData[loopCnt1].tmDataAvlVal[loopCnt2] = ps_TMSTSFIL[tmLink + loopCnt2].avltmVal;
					break;
				case PV_MKIND_BATTE:
					ps_bttAvlData[loopCnt1].tmDataAvlVal[loopCnt2] = ps_TMSTSFIL[tmLink + loopCnt2].avltmVal;
					break;
				case PV_MKIND_MLTMT:
					ps_mltAvlData[loopCnt1].tmDataAvlVal[loopCnt2] = ps_TMSTSFIL[tmLink + loopCnt2].avltmVal;
					break;
				case PV_MKIND_REMIO:
					ps_remAvlData[loopCnt1].tmDataAvlVal[loopCnt2] = ps_TMSTSFIL[tmLink + loopCnt2].avltmVal;
					break;
				case PV_MKIND_PLCFA:
					ps_plcAvlData[loopCnt1].tmDataAvlVal[loopCnt2] = ps_TMSTSFIL[tmLink + loopCnt2].avltmVal;
					break;
			}
		}
	}
	/* 計測状態保存ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_TMSTSFIL );

	return( 0 );
}

int commfunc_tmrec_set( struct st_TMRECINF * ps_tmhistinf, struct st_TMSTSFIL * ps_tmstsfil)
{
	struct timeval		nowTime;
	int					loopCnt1;
	struct st_TMREC		*ps_tmhst;
	struct tm			localTime;
	char				errMsg[PV_MSG_SIZE_LOG];

	gettimeofday( &nowTime, NULL );

	/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
	if( ps_gpt_TMRECFIL->curPoint >= (PV_TMREC_DATA_NUM-1) )
		/* ポインタを先頭レコードにセット */
		ps_gpt_TMRECFIL->curPoint = 0;
	else
		/* ポインタを+1 */
		ps_gpt_TMRECFIL->curPoint += 1;
	
	/* 状変蓄積の処理ポインタがサーバ送信用処理ポインタに追いついた場合 */
	if( ps_gpt_TMRECFIL->curPoint == ps_gpt_TMRECFIL->tcpPoint )
	{
		/* 年月日時分秒を取得 */
		localtime_r( &ps_gpt_TMRECFIL->s_tmhst[ps_gpt_TMRECFIL->curPoint].sec, &localTime );

		sprintf( errMsg, "write overtakes reading del data[%02d:%02d:%02d.%03d]", 
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			(ps_gpt_TMRECFIL->s_tmhst[ps_gpt_TMRECFIL->curPoint].uSec/100000));
		commfunc_syslog_msgset( "commfunc_tmrec_set()", errMsg );

		/* サーバ送信用ポインタを+1 */
		ps_gpt_TMRECFIL->tcpPoint += 1;
		/* サーバ送信用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_TMRECFIL->tcpPoint >= PV_TMREC_DATA_NUM )
		{
			/* サーバ送信用ポインタを先頭レコードにセット */
			ps_gpt_TMRECFIL->tcpPoint = 0;
		}
	}

	/* 現在時刻をを記録 */
	ps_tmhst = &ps_gpt_TMRECFIL->s_tmhst[ps_gpt_TMRECFIL->curPoint];
	ps_tmhst->sec  = nowTime.tv_sec;
	ps_tmhst->uSec = nowTime.tv_usec;

	/* 計測履歴を記録 */
	for( loopCnt1 = 0; loopCnt1 < PV_MAX_TM_HISTORY; loopCnt1++ )
	{
		/* 実装済みの場合は、計測値を格納 */
		if( ps_tmhistinf[loopCnt1].statFlag != 0 )
		{
			ps_tmhst->s_tmData[loopCnt1].tmFlag = ps_tmstsfil[ps_tmhistinf[loopCnt1].tmIfLinkPos].avltmFlag;
			ps_tmhst->s_tmData[loopCnt1].tmVal  = ps_tmstsfil[ps_tmhistinf[loopCnt1].tmIfLinkPos].avltmVal;
		}
		/* 未実装みの場合は、欠測を格納 */
		else
		{
			ps_tmhst->s_tmData[loopCnt1].tmFlag = PV_TMFLG_DROPTM;
		}
	}

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * サーバ送信用計測履歴情報取得処理
 *
 * 書式：
 * 		int commfunc_tmrec_msggettcp(
 * 			char (*tmhistMsgTcp)[PV_MSG_SIZE_JHD] )
 * 			int dataMax(
 * 機能：
 * 		tmhistMsgTcp で指定された二次元文字配列に蓄積された計測履歴情報を出力する。
 * 戻り値：
 * 		0～n：正常処理（計測履歴情報数） ／ -1：異常終了
 * 入力：
 * 		tmhistMsgTcp
 * 			計測履歴情報を格納する文字配列（二次元配列）
 */
int commfunc_tmrec_msggettcp( char (*tmhistMsgTcp)[PV_MSG_SIZE_TMH], int dataMax )
{
	int					msgCnt = 0;
	int					loopCnt1;
	struct timeval		tmstoTime;
	struct tm			localTime;
	char				msgStr[PV_MSG_SIZE_SVH];
	struct st_TMREC		*ps_tmhst;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_TMRECFIL == NULL )
	{
		printf("commfunc_tmrec_msggettcp(): Error TMRECFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 計測蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_TMRECFIL );

	/* サーバ送信用ポインタが計測蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_TMRECFIL->curPoint != ps_gpt_TMRECFIL->tcpPoint )
	{
		/* 計測蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_TMRECFIL->tcpPoint >= (PV_TMREC_DATA_NUM-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_TMRECFIL->tcpPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_TMRECFIL->tcpPoint += 1;

		ps_tmhst = &ps_gpt_TMRECFIL->s_tmhst[ps_gpt_TMRECFIL->tcpPoint];
	
		/* 状変時刻を取得 */
		tmstoTime.tv_sec  = ps_gpt_TMRECFIL->s_tmhst[ps_gpt_TMRECFIL->tcpPoint].sec;
		tmstoTime.tv_usec  = ps_gpt_TMRECFIL->s_tmhst[ps_gpt_TMRECFIL->tcpPoint].uSec;
		/* 年月日時分秒を取得 */
		localtime_r( &tmstoTime.tv_sec, &localTime );

		/* 計測履歴情報を生成 */
		sprintf( msgStr, "%s,%04d%02d%02d%02d%02d%02d%d00",
			PV_FNAME_TMREC_PREFIX,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			(tmstoTime.tv_usec/100000));

		for( loopCnt1 = 0; loopCnt1 < PV_MAX_TM_HISTORY; loopCnt1++ )
		{
			if( ps_tmhst->s_tmData[loopCnt1].tmFlag == PV_TMFLG_DROPTM )
			{
				sprintf( &msgStr[strlen(msgStr)], ",");
			}
			else
			{
				sprintf( &msgStr[strlen(msgStr)], ",%d", ps_tmhst->s_tmData[loopCnt1].tmVal);
				
			}
		}
		memcpy( tmhistMsgTcp[msgCnt], msgStr, PV_MSG_SIZE_SVH );

		msgCnt++;
		if( msgCnt >= dataMax )
		{
			break;
		}
	}

	/* 計測蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_TMRECFIL );

#ifdef PV_DEBUG_STOCK
		printf("commfunc_tmrec_msggettcp : tcpPoint[%d], dataCnt[%d]\n", (ps_gpt_TMRECFIL->tcpPoint+(PV_TMREC_DATA_NUM-msgCnt))%PV_TMREC_DATA_NUM, msgCnt );
#endif
	
	return( msgCnt );
}


int commfunc_svrec_set( struct st_SVRECINF * ps_svrecinf, struct st_SVSTSFIL * ps_svstsfil)
{
	struct timeval		nowTime;
	int					loopCnt1;
	struct st_SVREC		*ps_svrec;
	struct tm			localTime;
	char				errMsg[PV_MSG_SIZE_LOG];

	gettimeofday( &nowTime, NULL );

	/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
	if( ps_gpt_SVRECFIL->curPoint >= (PV_SVREC_DATA_NUM-1) )
		/* ポインタを先頭レコードにセット */
		ps_gpt_SVRECFIL->curPoint = 0;
	else
		/* ポインタを+1 */
		ps_gpt_SVRECFIL->curPoint += 1;
	
	/* 状変蓄積の処理ポインタがサーバ送信用処理ポインタに追いついた場合 */
	if( ps_gpt_SVRECFIL->curPoint == ps_gpt_SVRECFIL->tcpPoint )
	{
		/* 年月日時分秒を取得 */
		localtime_r( &ps_gpt_SVRECFIL->s_svrec[ps_gpt_SVRECFIL->curPoint].sec, &localTime );

		sprintf( errMsg, "write overtakes reading del data[%02d:%02d:%02d.%03d]", 
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			(ps_gpt_SVRECFIL->s_svrec[ps_gpt_SVRECFIL->curPoint].uSec/100000));
		commfunc_syslog_msgset( "commfunc_svrec_set()", errMsg );

		/* サーバ送信用ポインタを+1 */
		ps_gpt_SVRECFIL->tcpPoint += 1;
		/* サーバ送信用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_SVRECFIL->tcpPoint >= PV_SVREC_DATA_NUM )
		{
			/* サーバ送信用ポインタを先頭レコードにセット */
			ps_gpt_SVRECFIL->tcpPoint = 0;
		}
	}

	/* 現在時刻をを記録 */
	ps_svrec = &ps_gpt_SVRECFIL->s_svrec[ps_gpt_SVRECFIL->curPoint];
	ps_svrec->sec  = nowTime.tv_sec;
	ps_svrec->uSec = nowTime.tv_usec;

	/* 計測履歴を記録 */
	for( loopCnt1 = 0; loopCnt1 < PV_MAX_SV_HISTORY; loopCnt1++ )
	{
		/* 実装済みの場合は、状態を格納 */
		if( ps_svrecinf[loopCnt1].statFlag != 0 )
		{
			/* SV位置とBitが同じ場合は、ON */
			if( (ps_svstsfil[ps_svrecinf[loopCnt1].svStLinkPos].svStat & (char)ps_svrecinf[loopCnt1].stBit) == (char)ps_svrecinf[loopCnt1].stBit )
			{
				ps_svrec->svStat[loopCnt1] = 1;
			}
			else
			{
				ps_svrec->svStat[loopCnt1] = 0;
			}
		}
		else
		{
			ps_svrec->svStat[loopCnt1] = -1;
		}
	}

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * サーバ送信用計測履歴情報取得処理
 *
 * 書式：
 * 		int commfunc_svsto_msggettcp(
 * 			char (*svrecMsgTcp)[PV_MSG_SIZE_JHD] )
 * 			int dataMax(
 * 機能：
 * 		svrecMsgTcp で指定された二次元文字配列に蓄積された計測履歴情報を出力する。
 * 戻り値：
 * 		0～n：正常処理（計測履歴情報数） ／ -1：異常終了
 * 入力：
 * 		svrecMsgTcp
 * 			計測履歴情報を格納する文字配列（二次元配列）
 */
int commfunc_svrec_msggettcp( char (*svrecMsgTcp)[PV_MSG_SIZE_SVH], int dataMax )
{
	int					msgCnt = 0;
	int					loopCnt1;
	struct timeval		svstoTime;
	struct tm			localTime;
	struct st_SVREC		*ps_svrec;
	char				msgStr[PV_MSG_SIZE_SVH];

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_SVRECFIL == NULL )
	{
		printf("commfunc_svsto_msggettcp(): Error SVRECFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 計測蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_SVRECFIL );

	/* サーバ送信用ポインタが計測蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_SVRECFIL->curPoint != ps_gpt_SVRECFIL->tcpPoint )
	{
		/* 計測蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_SVRECFIL->tcpPoint >= (PV_SVREC_DATA_NUM-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_SVRECFIL->tcpPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_SVRECFIL->tcpPoint += 1;

		ps_svrec = &ps_gpt_SVRECFIL->s_svrec[ps_gpt_SVRECFIL->tcpPoint];
	
		/* 状変時刻を取得 */
		svstoTime.tv_sec  = ps_gpt_SVRECFIL->s_svrec[ps_gpt_SVRECFIL->tcpPoint].sec;
		svstoTime.tv_usec  = ps_gpt_SVRECFIL->s_svrec[ps_gpt_SVRECFIL->tcpPoint].uSec;
		/* 年月日時分秒を取得 */
		localtime_r( &svstoTime.tv_sec, &localTime );

		/* 計測履歴情報を生成 */
		sprintf( msgStr, "%s,%04d%02d%02d%02d%02d%02d%d00",
			PV_FNAME_SVREC_PREFIX,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			(svstoTime.tv_usec/100000));

		for( loopCnt1 = 0; loopCnt1 < PV_MAX_SV_HISTORY; loopCnt1++ )
		{
			/* 状態が未実装の場合 */
			if( ps_svrec->svStat[loopCnt1] < 0 )
			{
				sprintf( &msgStr[strlen(msgStr)], ",");
			}
			else
			{
				sprintf( &msgStr[strlen(msgStr)], ",%d", ps_svrec->svStat[loopCnt1]);
			}
		}

		memcpy( svrecMsgTcp[msgCnt], msgStr, PV_MSG_SIZE_SVH );

		msgCnt++;
		if( msgCnt >= dataMax )
		{
			break;
		}
	}

	/* 計測蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_SVRECFIL );

#ifdef PV_DEBUG_STOCK
		printf("commfunc_svsto_msggettcp : tcpPoint[%d], dataCnt[%d]\n", (ps_gpt_SVRECFIL->tcpPoint+(PV_SVREC_DATA_NUM-msgCnt))%PV_SVREC_DATA_NUM, msgCnt );
#endif
	
	return( msgCnt );
}

/* [20260515] Add St */
int commfunc_rprec_set( struct st_FCRRPINF * ps_fcrrpinf, struct st_TMSTSFIL * ps_tmstsfil)
{
	struct timeval		nowTime;
	int					loopCnt1;
	struct st_REPOREC	*ps_reporec;
	struct tm			localTime;
	char				errMsg[PV_MSG_SIZE_LOG];

	gettimeofday( &nowTime, NULL );

	/* 実績データ蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_RPRECFIL );

	/* 実績データ蓄積ファイルの最終レコードにポインタがある場合 */
	if( ps_gpt_RPRECFIL->curPoint >= (PV_RPREC_DATA_NUM-1) )
		/* ポインタを先頭レコードにセット */
		ps_gpt_RPRECFIL->curPoint = 0;
	else
		/* ポインタを+1 */
		ps_gpt_RPRECFIL->curPoint += 1;
	
	/* 実績データ蓄積の処理ポインタがサーバ送信用処理ポインタに追いついた場合 */
	if( ps_gpt_RPRECFIL->curPoint == ps_gpt_RPRECFIL->tcpPoint )
	{
		/* 年月日時分秒を取得 */
		localtime_r( &ps_gpt_RPRECFIL->s_reporec[ps_gpt_RPRECFIL->curPoint].sec, &localTime );

		sprintf( errMsg, "write overtakes reading del data[%02d:%02d:%02d.%03d]", 
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			(ps_gpt_RPRECFIL->s_reporec[ps_gpt_RPRECFIL->curPoint].uSec/100000));
		commfunc_syslog_msgset( "commfunc_rprec_set()", errMsg );

		/* サーバ送信用ポインタを+1 */
		ps_gpt_RPRECFIL->tcpPoint += 1;
		/* サーバ送信用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_RPRECFIL->tcpPoint >= PV_RPREC_DATA_NUM )
		{
			/* サーバ送信用ポインタを先頭レコードにセット */
			ps_gpt_RPRECFIL->tcpPoint = 0;
		}
	}

	/* 現在時刻をを記録 */
	ps_reporec = &ps_gpt_RPRECFIL->s_reporec[ps_gpt_RPRECFIL->curPoint];
	ps_reporec->sec  = nowTime.tv_sec;
	ps_reporec->uSec = nowTime.tv_usec;
	/* 実績データを記録 */
	for( loopCnt1 = 0; loopCnt1 < PV_FCRRP_DATA_MAX; loopCnt1++ )
	{
		/* 実装済み(データ番号が設定されている)の場合は、計測値を格納 */
		if( ps_fcrrpinf->s_fcrrpData[loopCnt1].dataNumb != 0 )
		{
			ps_reporec->s_rpData[loopCnt1].tmFlag = ps_tmstsfil[ps_fcrrpinf->s_fcrrpData[loopCnt1].tmIfLinkPos].avltmFlag;
			ps_reporec->s_rpData[loopCnt1].tmVal  = ps_tmstsfil[ps_fcrrpinf->s_fcrrpData[loopCnt1].tmIfLinkPos].avltmVal;
		}
		/* 未実装みの場合は、欠測を格納 */
		else
		{
			ps_reporec->s_rpData[loopCnt1].tmFlag = PV_TMFLG_DROPTM;
		}
	}

	/* 実績データ蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_RPRECFIL );

	return 0;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * サーバ送信用実績データ情報取得処理
 *
 * 書式：
 * 		int commfunc_rprec_msggettcp(
 * 			char (*reprecMsgTcp)[PV_MSG_SIZE_REPO] )
 * 			int dataMax(
 * 機能：
 * 		reprecMsgTcp で指定された二次元文字配列に蓄積された実績データ情報を出力する。
 * 戻り値：
 * 		0～n：正常処理（実績データ情報数） ／ -1：異常終了
 * 入力：
 * 		reprecMsgTcp
 * 			実績データ情報を格納する文字配列（二次元配列）
 */
int commfunc_rprec_msggettcp( char (*reprecMsgTcp)[PV_MSG_SIZE_REPO], int dataMax )
{
	int					msgCnt = 0;
	int					loopCnt1;
	struct timeval		tmstoTime;
	struct tm			localTime;
	char				msgStr[PV_MSG_SIZE_SVH];
	struct st_REPOREC	*ps_reporec;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_RPRECFIL == NULL )
	{
		printf("commfunc_rprec_msggettcp(): Error RPRECFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 実績データ蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_RPRECFIL );

	/* サーバ送信用ポインタが実績データ蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_RPRECFIL->curPoint != ps_gpt_RPRECFIL->tcpPoint )
	{
		/* 実績データ蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_RPRECFIL->tcpPoint >= (PV_RPREC_DATA_NUM-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_RPRECFIL->tcpPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_RPRECFIL->tcpPoint += 1;

		ps_reporec = &ps_gpt_RPRECFIL->s_reporec[ps_gpt_RPRECFIL->tcpPoint];
	
		/* 状変時刻を取得 */
		tmstoTime.tv_sec  = ps_gpt_RPRECFIL->s_reporec[ps_gpt_RPRECFIL->tcpPoint].sec;
		tmstoTime.tv_usec  = ps_gpt_RPRECFIL->s_reporec[ps_gpt_RPRECFIL->tcpPoint].uSec;
		/* 年月日時分秒を取得 */
		localtime_r( &tmstoTime.tv_sec, &localTime );

		/* 実績データ情報を生成 */
		sprintf( msgStr, "%s,%04d%02d%02d%02d%02d%02d%d00",
			PV_FNAME_RPREC_PREFIX,
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			(tmstoTime.tv_usec/100000));

		for( loopCnt1 = 0; loopCnt1 < PV_FCRRP_DATA_MAX; loopCnt1++ )
		{
			if( ps_reporec->s_rpData[loopCnt1].tmFlag == PV_TMFLG_DROPTM )
			{
				sprintf( &msgStr[strlen(msgStr)], ",");
			}
			else
			{
				sprintf( &msgStr[strlen(msgStr)], ",%d", ps_reporec->s_rpData[loopCnt1].tmVal);
				
			}
		}
		memcpy( reprecMsgTcp[msgCnt], msgStr, PV_MSG_SIZE_REPO );

		msgCnt++;
		if( msgCnt >= dataMax )
		{
			break;
		}
	}

	/* 実績データ蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_RPRECFIL );

#ifdef PV_DEBUG_STOCK
		printf("commfunc_rprec_msggettcp : tcpPoint[%d], dataCnt[%d]\n", (ps_gpt_RPRECFIL->tcpPoint+(PV_RPREC_DATA_NUM-msgCnt))%PV_RPREC_DATA_NUM, msgCnt );
#endif
	
	return( msgCnt );
}
/* [20260515] Add Ed */
