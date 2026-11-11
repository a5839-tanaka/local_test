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
 * 共通関数：状変処理関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・状変蓄積ファイルに状変情報を蓄積する。
 * 		・状変蓄積ファイルから状変情報を取り出す。
 */
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvserial.h"


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変処理（ビットフィールド形式）
 *
 * 書式：
 * 		int	commfunc_johen_sv(
 * 			int soKind,
 * 			int soNumb,
 * 			int maxSv,
 * 			unsigned char *psvStat,
 * 			int svIfLink,
 * 			int svStLink,
 * 			struct st_SVSTSFIL *ps_SVSTSFIL,
 * 			struct st_SVMSKINF *ps_SVMSKINF )
 * 機能：
 * 		引数 psvStat で指定されたアドレスから始まる８ビットのビットフィールド形
 * 		式で格納されている状態情報と状態情報保存ファイル（SVSTSFIL.dat）に格納さ
 * 		れている状態情報を比較し、状態が不一致の場合は状変処理を行う。
 * 		状態情報の不一致確認後は、psvStat に格納されている状態情報でSVSTSFIL.dat
 * 		を更新する。
 * 		比較位置は引数 soKind（装置種別）、soNumb（装置番号）、svStLink
 * 		（SVSTSFIL.dat比較開始位置）を元に行い、比較範囲は maxSv によるフィールド
 * 		数で判定する。
 *		なお、状態不一致の比較を行わない情報が存在するので、除外対象を特定するた
 *		め装置状態状変マスク情報ファイルを参照する。状変マスクに該当する状態情報
 *		は不一致を検出しても状変処理を行わないが、状対更新は行う。
 *		その他の引数は、ps_SVSTSFIL（装置状態格納ファイルのMMAPポインタ情報）、
 *		ps_SVMSKINF （装置状態状変マスク情報ファイルのMMAPポインタ情報）を引数と
 *		して処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		maxSv
 * 			ビットフィールド数
 * 		*psvStat
 * 			装置状態情報（ビットフィールド形式）の開始ポインタ
 * 		svIfLink
 * 			装置状態情報ファイルリンク情報
 * 		svStLink
 * 			装置状態保存ファイルリンク情報
 * 		*ps_SVSTSFIL
 * 			装置状態保存ファイルMMAPポインタ
 * 		*ps_SVMSKFIL
 * 			装置状態状変マスク情報ファイルMMAPポインタ
 */
int	commfunc_johen_sv(
	int soKind,
	int soNumb,
	int maxSv,
	unsigned char *psvStat,
	int svIfLink,
	int	svStLink,
	struct st_SVSTSFIL *ps_SVSTSFIL,
	struct st_SVMSKINF *ps_SVMSKINF )
{
	int					loopPoint;				/* ループカウンタ（ポイント） */
	unsigned char		loopBit;				/* ループカウンタ（ビット） */
	int					maxLoop;				/* 最大処理数（ポイント） */
	int					svCount = 0;			/* 処理機器数カウンタ */
	unsigned char		svMask;					/* 状変マスク */
	unsigned char		svStat;					/* 装置状態（旧状態） */
	unsigned char		svChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_sv(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 最大処理ループ数を算出 */
	maxLoop = (maxSv - 1) / 8 + 1;

	/* 装置状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_SVSTSFIL);

	/* 最大ループカウンタまたは最大処理機器数分繰り返す */
	for(loopPoint = 0 ; (loopPoint < maxLoop) && (svCount < maxSv) ; loopPoint++ )
	{
		/* データ定義による状変マスク値を取得 */
		svMask = (ps_SVMSKINF + svStLink + loopPoint)->svMsk;
		/* 装置状態（旧状態）を取得 */
		svStat = (ps_SVSTSFIL + svStLink + loopPoint)->svStat;

		/* 新状態と旧状態を比較 */
		svChek = *(psvStat + loopPoint) ^ svStat;

#if 0
		printf("commfunc_johen_sv(): STS:%02X, OLD:%02X, MSK:%02X, CHK:%02X\n",
			*(psvStat + loopPoint), svStat, svMask, svChek);
#endif
		if( svChek )
		{
			for(loopBit = (unsigned char)0x80 ; loopBit && (svCount < maxSv) ; loopBit >>= 1 )
			{
				/* 機器処理数を+1 */
				svCount++;

				/* 当該ビットの状態が変化している場合 */
				if( svChek & loopBit )
				{
					/* 状変マスク対象の場合 */
					if( svMask & loopBit )
					{
						/* 状変処理 */
						if( *(psvStat + loopPoint) & loopBit )
						{
							/* 発生状変（マスクあり） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 1, PV_SV_KIND_JOMASK, svIfLink + svCount - 1);
						}
						else
						{
							/* 復帰状変（マスクあり） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 0, PV_SV_KIND_JOMASK, svIfLink + svCount - 1);
						}
					}
					else
					{
						/* 状変処理 */
						if( *(psvStat + loopPoint) & loopBit )
						{
#if 0
							printf("commfunc_johen_sv(): JOHEN(SV)  SKND:%d  SONO:%d  SVNO:%d  STS:1  REC:%d  LNK:%d\n",
								soKind, soNumb, svCount, svStLink + loopPoint, svIfLink + svCount - 1);
#endif

							/* 発生状変（マスク無し） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 1, PV_SV_KIND_NORMAL, svIfLink + svCount - 1);
						}
						else
						{
#if 0
							printf("commfunc_johen_sv(): JOHEN(SV)  SKND:%d  SONO:%d  SVNO:%d  STS:0  REC:%d  LNK:%d\n",
								soKind, soNumb, svCount, svStLink + loopPoint, svIfLink + svCount - 1);
#endif

							/* 復帰状変（マスク無し） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 0, PV_SV_KIND_NORMAL, svIfLink + svCount - 1);
						}
					}
				}
			}
		}
		else
		{
			/* 機器処理数を+8 */
			svCount += 8;
		}
		/* 最新の状態を装置状態ファイルに記録する */
		/* （状変マスクは関係なく現在状態を保存する） */
		(ps_SVSTSFIL + svStLink + loopPoint)->svStat = *(psvStat + loopPoint);
	}

	/* 装置状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_SVSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変処理（ビットフィールド形式）
 *
 * 書式：
 * 		int	commfunc_johen_sv_wmask(
 * 			int soKind,
 * 			int soNumb,
 * 			int maxSv,
 * 			unsigned char *psvStat,
 * 			unsigned char *johMask,
 * 			int svIfLink,
 * 			int svStLink,
 * 			struct st_SVSTSFIL *ps_SVSTSFIL,
 * 			struct st_SVMSKINF *ps_SVMSKINF ) 
 * 機能：
 * 		引数 psvStat で指定されたアドレスから始まる８ビットのビットフィールド形
 * 		式で格納されている状態情報と状態情報保存ファイル（SVSTSFIL.dat）に格納さ
 * 		れている状態情報を比較し、状態が不一致の場合は状変処理を行う。
 * 		状態情報の不一致確認後は、psvStat に格納されている状態情報でSVSTSFIL.dat
 * 		を更新する。
 * 		比較位置は引数 soKind（装置種別）、soNumb（装置番号）、svStLink
 * 		（SVSTSFIL.dat比較開始位置）を元に行い、比較範囲は maxSv によるフィールド
 * 		数で判定する。
 *		なお、状態不一致の比較を行わない情報が存在するので、除外対象を特定するた
 *		め装置状態状変マスク情報ファイルを参照する。状変マスクに該当する状態情報
 *		は不一致を検出しても状変処理を行わないが、状対更新は行う。
 *		その他の引数は、ps_SVSTSFIL（装置状態格納ファイルのMMAPポインタ情報）、
 *		ps_SVMSKINF （装置状態状変マスク情報ファイルのMMAPポインタ情報）を引数と
 *		して処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		maxSv
 * 			ビットフィールド数
 * 		*psvStat
 * 			装置状態情報（ビットフィールド形式）の開始ポインタ
 * 		*johMask
 * 			状変マスク情報（ビットフィールド形式）の開始ポインタ
 * 		svIfLink
 * 			装置状態情報ファイルリンク情報
 * 		svStLink
 * 			装置状態保存ファイルリンク情報
 * 		*ps_SVSTSFIL
 * 			装置状態保存ファイルMMAPポインタ
 * 		*ps_SVMSKFIL
 * 			装置状態状変マスク情報ファイルMMAPポインタ
 */
int	commfunc_johen_sv_wmask(
	int soKind,
	int soNumb,
	int maxSv,
	unsigned char *psvStat,
	unsigned char *johMask,
	int svIfLink,
	int	svStLink,
	struct st_SVSTSFIL *ps_SVSTSFIL,
	struct st_SVMSKINF *ps_SVMSKINF )
{
	int					loopPoint;				/* ループカウンタ（ポイント） */
	unsigned char		loopBit;				/* ループカウンタ（ビット） */
	int					maxLoop;				/* 最大処理数（ポイント） */
	int					svCount = 0;			/* 処理機器数カウンタ */
	unsigned char		svMask;					/* 状変マスク */
	unsigned char		svStat;					/* 装置状態（旧状態） */
	unsigned char		svChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_sv_wmask(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 最大処理ループ数を算出 */
	maxLoop = (maxSv - 1) / 8 + 1;

	/* 装置状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_SVSTSFIL);

	/* 最大ループカウンタまたは最大処理機器数分繰り返す */
	for(loopPoint = 0 ; (loopPoint < maxLoop) && (svCount < maxSv) ; loopPoint++ )
	{
		/* 状変マスク値を取得 */
		svMask = (ps_SVMSKINF + svStLink + loopPoint)->svMsk | *(johMask + loopPoint);
		/* 装置状態（旧状態）を取得 */
		svStat = (ps_SVSTSFIL + svStLink + loopPoint)->svStat;

		/* 新状態と旧状態を比較 */
		svChek = *(psvStat + loopPoint) ^ svStat;

#if 0
		printf("commfunc_johen_sv(): STS:%02X, OLD:%02X, MSK:%02X, CHK:%02X\n",
			*(psvStat + loopPoint), svStat, svMask, svChek);
#endif
		/* 状変がある場合 */
		if( svChek )
		{
			for(loopBit = (unsigned char)0x80 ; loopBit && (svCount < maxSv) ; loopBit >>= 1 )
			{
				/* 機器処理数を+1 */
				svCount++;

				/* 当該ビットの状態が変化している場合 */
				if( svChek & loopBit )
				{

					/* 状変マスク対象の場合 */
					if( svMask & loopBit )
					{
						/* 状変処理 */
						if( *(psvStat + loopPoint) & loopBit )
						{
							/* 発生状変（マスクあり） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 1, PV_SV_KIND_JOMASK, svIfLink + svCount - 1);
						}
						else
						{
							/* 復帰状変（マスクあり） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 0, PV_SV_KIND_JOMASK, svIfLink + svCount - 1);
						}
					}
					else
					{

						/* 状変処理 */
						if( *(psvStat + loopPoint) & loopBit )
						{
#if 0
							printf("commfunc_johen_sv(): JOHEN(SV)  SKND:%d  SONO:%d  SVNO:%d  STS:1  REC:%d  LNK:%d\n",
								soKind, soNumb, svCount, svStLink + loopPoint, svIfLink + svCount - 1);
#endif

							/* 発生状変（マスクなし） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 1, PV_SV_KIND_NORMAL, svIfLink + svCount - 1);
						}
						else
						{
#if 0
							printf("commfunc_johen_sv(): JOHEN(SV)  SKND:%d  SONO:%d  SVNO:%d  STS:0  REC:%d  LNK:%d\n",
								soKind, soNumb, svCount, svStLink + loopPoint, svIfLink + svCount - 1);
#endif

							/* 復帰状変（マスクなし） */
							commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svCount, 0, PV_SV_KIND_NORMAL, svIfLink + svCount - 1);

						}
					}
				}
			}
		}
		else
		{
			/* 機器処理数を+8 */
			svCount += 8;
		}
		/* 最新の状態を装置状態ファイルに記録する */
		/* （状変マスクは関係なく現在状態を保存する） */
		(ps_SVSTSFIL + svStLink + loopPoint)->svStat = *(psvStat + loopPoint);
	}

	/* 装置状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_SVSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態状変処理（単独処理）
 *
 * 書式：
 * 		int	commfunc_johen_pos(
 * 			int soKind,
 * 			int soNumb,
 * 			int svNumb,
 * 			int svStat,
 * 			int svIfLink,
 * 			int svStLink,
 * 			struct st_SVSTSFIL *ps_SVSTSFIL,
 * 			struct st_SVMSKINF *ps_SVMSKINF )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、svNumb（状態番号）、svStat（状態情報）
 * 		の情報を元に、状態情報保存ファイル（SVSTSFIL.dat）に格納されている状態情報を比較し、
 * 		状態が不一致の場合は状変処理を行う。
 * 		状態情報の不一致確認後は、svStat に格納されている状態情報でSVSTSFIL.datを更新する。
 * 		svStLink（比較開始位置）を元に行う。
 *		なお、状態不一致の比較を行わない情報が存在するので、除外対象を特定するため装置状態
 *		状変マスク情報ファイルを参照する。状変マスクに該当する状態情報は不一致を検出しても
 *		状変処理を行わないが、状対更新は行う。
 *		その他の引数は、ps_SVSTSFIL（装置状態格納ファイルのMMAPポインタ情報）、ps_SVMSKINF
 *		（装置状態状変マスク情報ファイルのMMAPポインタ情報）を引数として処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		svNumb
 * 			状態番号
 * 		svStat
 * 			状態情報
 * 		svIfLink
 * 			装置状態情報ファイルリンク情報
 * 		svStLink
 * 			装置状態保存ファイルリンク情報
 * 		*ps_SVSTSFIL
 * 			装置状態保存ファイルMMAPポインタ
 * 		*ps_SVMSKFIL
 * 			装置状態状変マスク情報ファイルMMAPポインタ
 */
int	commfunc_johen_pos(
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int svNumb,									/* 状態番号 */
	int	svStat,									/* 装置状態 */
	int svIfLink,								/* 状態情報ファイルポインタ（当該装置の先頭ポインタ） */
	int svStLink,								/* 状態状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_SVSTSFIL *ps_SVSTSFIL,
	struct st_SVMSKINF *ps_SVMSKINF )
{
	int					svPoint;				/* 状変ポイント位置 */
	unsigned char		svBit;					/* 状変ビット位置 */
	unsigned char		svMask;					/* 状変マスク */
	unsigned char		svOld;					/* 装置状態（旧状態） */
	unsigned char		svNew;					/* 装置状態（新状態） */
	unsigned char		svChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_pos(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状態情報のポイントを算出 */
	svPoint = svStLink + (svNumb - 1) / 8;
	/* 状態情報のビット位置を算出 */
	svBit   = (unsigned char)0x80 >> ((svNumb - 1) % 8);

	/* 装置状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_SVSTSFIL);

	/* 装置状態（旧状態）を取得 */
	svOld = (ps_SVSTSFIL + svPoint)->svStat;
	/* 装置状態（新状態）を作成 */
	if( svStat )
	{
		/* 装置状態をＯＮ設定 */
		svNew = svOld | svBit;
	}
	else
	{
		/* 装置状態をＯＦＦ設定 */
		svNew = svOld & ~svBit;
	}
	/* データで定義された状変マスク値を取得 */
	svMask = (ps_SVMSKINF + svPoint)->svMsk;

	/* 新状態と旧状態を比較 */
	svChek = svNew ^ svOld;

#if 0
	printf("commfunc_johen_pos(): SOKD:%2d, SONO:%2d, SV:%3d, STS:%d, LNK:%02X, BIT:%02X, NEW:%02X, OLD:%02X, MSK:%02X, CHK:%02X\n",
		soKind, soNumb, svNumb, svStat, svPoint, svBit, svNew, svOld, svMask, svChek);
#endif

	/* 装置状態が不一致 */
	if( svChek )
	{

		/* 状変マスク対象の状態の場合 */
		if( ( svChek & ~svMask ) == 0 )
		{
			/* 状変処理 */
			if( svStat )
			{
				/* 発生状変（マスク対象） */
				commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svNumb, 1, PV_SV_KIND_JOMASK, svIfLink + svNumb - 1);
			}
			else
			{
				/* 復帰状変（マスク対象） */
				commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svNumb, 0, PV_SV_KIND_JOMASK, svIfLink + svNumb - 1);
			}
		}
		else
		{

			/* 状変処理 */
			if( svStat )
			{
#if 0
				printf("commfunc_johen_pos(): JOHEN(SV)  SKND:%d  SONO:%d  SVNO:%d  STS:1  REC:%d  LNK:%d\n",
					soKind, soNumb, svNumb, svPoint, svIfLink + svNumb - 1);
#endif

				/* 発生状変（マスク無し） */
				commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svNumb, 1, PV_SV_KIND_NORMAL, svIfLink + svNumb - 1);

			}
			else
			{
#if 0
				printf("commfunc_johen_pos(): JOHEN(SV)  SKND:%d  SONO:%d  SVNO:%d  STS:0  REC:%d  LNK:%d\n",
					soKind, soNumb, svNumb, svPoint, svIfLink + svNumb - 1);
#endif

				/* 復帰状変（マスク無し） */
				commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svNumb, 0, PV_SV_KIND_NORMAL, svIfLink + svNumb - 1);

			}

		}

	}
	/* 最新の状態を装置状態ファイルに記録する */
	/* （状変マスクは関係なく現在状態を保存する） */
	(ps_SVSTSFIL + svPoint)->svStat = svNew;

	/* 装置状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_SVSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態マスク状変処理（単独処理）
 *
 * 書式：
 * 		int	commfunc_johen_pos_mask(
 * 			int soKind,
 * 			int soNumb,
 * 			int svNumb,
 * 			int svStat,
 * 			int svIfLink,
 * 			int svStLink,
 * 			struct st_SVSTSFIL *ps_SVSTSFIL,
 * 			struct st_SVMSKINF *ps_SVMSKINF )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、svNumb（状態番号）、svStat（状態情報）
 * 		の情報を元に、状態情報保存ファイル（SVSTSFIL.dat）に格納されている状態情報を比較し、
 * 		状態が不一致の場合はマスク状変処理を行う。
 * 		状態情報の不一致確認後は、svStat に格納されている状態情報でSVSTSFIL.datを更新する。
 * 		svStLink（比較開始位置）を元に行う。
 *		マスク状変処理とは状変蓄積ファイルに状変を記録するが、モニタリングサーバ向けの送信
 *		情報からは通知が除外される。
 *		その他の引数は、ps_SVSTSFIL（装置状態格納ファイルのMMAPポインタ情報）、ps_SVMSKINF
 *		（装置状態状変マスク情報ファイルのMMAPポインタ情報）を引数として処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		svNumb
 * 			状態番号
 * 		svStat
 * 			状態情報
 * 		svIfLink
 * 			装置状態情報ファイルリンク情報
 * 		svStLink
 * 			装置状態保存ファイルリンク情報
 * 		*ps_SVSTSFIL
 * 			装置状態保存ファイルMMAPポインタ
 * 		*ps_SVMSKFIL
 * 			装置状態状変マスク情報ファイルMMAPポインタ
 */
int	commfunc_johen_pos_mask(
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int svNumb,									/* 状態番号 */
	int	svStat,									/* 装置状態 */
	int svIfLink,								/* 状態情報ファイルポインタ（当該装置の先頭ポインタ） */
	int svStLink,								/* 状態状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_SVSTSFIL *ps_SVSTSFIL,
	struct st_SVMSKINF *ps_SVMSKINF )
{
	int					svPoint;				/* 状変ポイント位置 */
	unsigned char		svBit;					/* 状変ビット位置 */
	unsigned char		svOld;					/* 装置状態（旧状態） */
	unsigned char		svNew;					/* 装置状態（新状態） */
	unsigned char		svChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_pos_mask(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状態情報のポイントを算出 */
	svPoint = svStLink + (svNumb - 1) / 8;
	/* 状態情報のビット位置を算出 */
	svBit   = (unsigned char)0x80 >> ((svNumb - 1) % 8);

	/* 装置状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_SVSTSFIL);

	/* 装置状態（旧状態）を取得 */
	svOld = (ps_SVSTSFIL + svPoint)->svStat;
	/* 装置状態（新状態）を作成 */
	if( svStat )
	{
		/* 装置状態をＯＮ設定 */
		svNew = svOld | svBit;
	}
	else
	{
		/* 装置状態をＯＦＦ設定 */
		svNew = svOld & ~svBit;
	}

	/* 新状態と旧状態を比較 */
	svChek = svNew ^ svOld;

#if 0
	printf("commfunc_johen_pos_mask(): SOKD:%2d, SONO:%2d, SV:%3d, STS:%d, LNK:%02X, BIT:%02X, NEW:%02X, OLD:%02X, MSK:%02X, CHK:%02X\n",
		soKind, soNumb, svNumb, svStat, svPoint, svBit, svNew, svOld, svMask, svChek);
#endif

	/* 装置状態が不一致 */
	if( svChek )
	{
		/* 状変処理 */
		if( svStat )
		{
			/* 発生状変（マスク対象） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svNumb, 1, PV_SV_KIND_JOMASK, svIfLink + svNumb - 1);
		}
		else
		{
			/* 復帰状変（マスク対象） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_SV, svNumb, 0, PV_SV_KIND_JOMASK, svIfLink + svNumb - 1);
		}
	}
	/* 最新の状態を装置状態ファイルに記録する */
	/* （状変マスクは関係なく現在状態を保存する） */
	(ps_SVSTSFIL + svPoint)->svStat = svNew;

	/* 装置状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_SVSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状変処理（単独処理）
 *
 * 書式：
 * 		int	commfunc_johen_stat(
 * 			int soKind,
 * 			int soNumb,
 * 			int stNumb,
 * 			int svStat,
 * 			int ststLink,
 * 			struct st_STSTSFIL *ps_STSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、stNumb（状態番号）、stStat
 * 		（状態情報）の情報を元に、通信ステータス状態情報保存ファイル
 * 		（STSTSFIL.dat）に格納されている状態情報を比較し、状態が不一致の場合は状
 * 		変処理を行う。
 * 		状態情報の不一致確認後は、stStat に格納されている状態情報でSTSTSFIL.dat
 * 		を更新する。更新位置はststLink（比較開始位置）を元に行う。
 *		その他の引数は、ps_STSTSFIL（通信ステータス状態格納ファイルのMMAPポイン
 *		タ情報）を引数として処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		stNumb
 * 			状態番号
 * 		stStat
 * 			状態情報
 * 		ststLink
 * 			通信ステータス状態保存ファイルリンク情報
 * 		*ps_STSTSFIL
 * 			通信ステータス状態保存ファイルMMAPポインタ
 */
int	commfunc_johen_stat(
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int stNumb,									/* 状態番号 */
	int	stStat,									/* 通信ステータス状態 */
	int ststLink,								/* 通信ステータス状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_STSTSFIL *ps_STSTSFIL )
{
	int					stPoint;				/* 状変ポイント位置 */
	unsigned char		stBit;					/* 状変ビット位置 */
	unsigned char		stOld;					/* 通信ステータス状態（旧状態） */
	unsigned char		stNew;					/* 通信ステータス状態（新状態） */
	unsigned char		stChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_stat(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 通信ステータス状態情報のポイントを算出 */
	stPoint = ststLink + (stNumb - 1) / 8;
	/* 通信ステータス状態情報のビット位置を算出 */
	stBit   = (unsigned char)0x80 >> ((stNumb - 1) % 8);

	/* 通信ステータス状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_STSTSFIL);

	/* 通信ステータス状態（旧状態）を取得 */
	stOld = (ps_STSTSFIL + stPoint)->stStat;
	/* 通信ステータス状態（新状態）を作成 */
	if( stStat )
	{
		/* 通信ステータス状態をＯＮ設定 */
		stNew = stOld | stBit;
	}
	else
	{
		/* 通信ステータス状態をＯＦＦ設定 */
		stNew = stOld & ~stBit;
	}

	/* 新状態と旧状態の比較 */
	stChek = (stNew ^ stOld);

#if 0
	printf("commfunc_johen_stat(): STS:%02X, OLD:%02X, CHK:%02X\n",
		stNew, stOld, stChek);
#endif

	/* 通信ステータス状態が不一致 */
	if( stChek )
	{
		/* 状変処理 */
		if( stStat )
		{
#if 0
			printf("commfunc_johen_stat(): JOHEN(ST)  SKND:%d  SONO:%d  STNO:%d  STS:1  REC:%d  LNK:%d\n",
				soKind, soNumb, stNumb, stPoint, ststLink );
#endif
			/* 発生状変（マスク無し） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_ST, stNumb, 1, PV_SV_KIND_NORMAL, stNumb - 1);
		}
		else
		{
#if 0
			printf("commfunc_johen_stat(): JOHEN(ST)  SKND:%d  SONO:%d  STNO:%d  STS:0  REC:%d  LNK:%d\n",
				soKind, soNumb, stNumb, stPoint, ststLink);
#endif
			/* 復帰状変（マスク無し） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_ST, stNumb, 0, PV_SV_KIND_NORMAL, stNumb - 1);
		}
	}
	/* 最新の状態を通信ステータス状態ファイルに記録する */
	/* （状変マスクは関係なく現在状態を保存する） */
	(ps_STSTSFIL + stPoint)->stStat = stNew;

	/* 通信ステータス状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_STSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータスマスク状変処理（単独処理）
 *
 * 書式：
 * 		int	commfunc_johen_stat_mask(
 * 			int soKind,
 * 			int soNumb,
 * 			int stNumb,
 * 			int stStat,
 * 			int ststLink,
 * 			struct st_STSTSFIL *ps_STSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、stNumb（状態番号）、stStat
 * 		（状態情報）の情報を元に、通信ステータス状態情報保存ファイル
 * 		（STSTSFIL.dat）に格納されている状態情報を比較し、状態が不一致の場合は状
 * 		変処理を行う。
 * 		状態情報の不一致確認後は、stStat に格納されている状態情報でSTSTSFIL.dat
 * 		を更新する。更新位置はststLink（比較開始位置）を元に行う。
 *		マスク状変処理とは状変蓄積ファイルに状変を記録するが、モニタリングサーバ
 *		向けの送信情報からは通知が除外される。
 *		その他の引数は、ps_STSTSFIL（通信ステータス状態格納ファイルのMMAPポイン
 *		タ情報）を引数として処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		stNumb
 * 			状態番号
 * 		stStat
 * 			状態情報
 * 		ststLink
 * 			通信ステータス状態保存ファイルリンク情報
 * 		*ps_STSTSFIL
 * 			通信ステータス状態保存ファイルMMAPポインタ
 */
int	commfunc_johen_stat_mask(
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int stNumb,									/* 状態番号 */
	int	stStat,									/* 通信ステータス状態 */
	int ststLink,								/* 通信ステータス状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_STSTSFIL *ps_STSTSFIL )
{
	int					stPoint;				/* 状変ポイント位置 */
	unsigned char		stBit;					/* 状変ビット位置 */
	unsigned char		stOld;					/* 通信ステータス状態（旧状態） */
	unsigned char		stNew;					/* 通信ステータス状態（新状態） */
	unsigned char		stChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_stat_mask(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 通信ステータス状態情報のポイントを算出 */
	stPoint = ststLink + (stNumb - 1) / 8;
	/* 通信ステータス状態情報のビット位置を算出 */
	stBit   = (unsigned char)0x80 >> ((stNumb - 1) % 8);

	/* 通信ステータス状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_STSTSFIL);

	/* 通信ステータス状態（旧状態）を取得 */
	stOld = (ps_STSTSFIL + stPoint)->stStat;
	/* 通信ステータス状態（新状態）を作成 */
	if( stStat )
	{
		/* 通信ステータス状態をＯＮ設定 */
		stNew = stOld | stBit;
	}
	else
	{
		/* 通信ステータス状態をＯＦＦ設定 */
		stNew = stOld & ~stBit;
	}

	/* 新状態と旧状態の比較 */
	stChek = (stNew ^ stOld);

#if 0
	printf("commfunc_johen_stat(): STS:%02X, OLD:%02X, CHK:%02X\n",
		stNew, stOld, stChek);
#endif

	/* 通信ステータス状態が不一致 */
	if( stChek )
	{
		/* 状変処理 */
		if( stStat )
		{
			/* 発生状変（マスクあり） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_ST, stNumb, 1, PV_SV_KIND_JOMASK, stNumb - 1);
		}
		else
		{
			/* 復帰状変（マスクあり） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_ST, stNumb, 0, PV_SV_KIND_JOMASK, stNumb - 1);
		}
	}
	/* 最新の状態を通信ステータス状態ファイルに記録する */
	/* （状変マスクは関係なく現在状態を保存する） */
	(ps_STSTSFIL + stPoint)->stStat = stNew;

	/* 通信ステータス状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_STSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状態取得処理
 *
 * 書式：
 * 		int	commfunc_st_stat_get(
 * 			int soKind,
 * 			int soNumb,
 * 			int stNumb,
 * 			int ststLink,
 * 			struct st_STSTSFIL *ps_STSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、stNumb（状態番号）の情報を
 *		元に、通信ステータス状態情報保存ファイル（STSTSFIL.dat）に格納されている
 *		状態情報を取得し関数戻り値に設定する。
 * 		状態取得位置はststLink（比較開始位置）を元に行う。
 *		その他の引数は、ps_STSTSFIL（通信ステータス状態格納ファイルのMMAPポイン
 *		タ情報）
 * 戻り値：
 * 		0：復帰状態 / 1：発生状態 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		stNumb
 * 			状態番号
 * 		ststLink
 * 			通信ステータス状態保存ファイルリンク情報
 * 		*ps_STSTSFIL
 * 			通信ステータス状態保存ファイルMMAPポインタ
 */
int	commfunc_st_stat_get(
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int stNumb,									/* 状態番号 */
	int ststLink,								/* 通信ステータス状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_STSTSFIL *ps_STSTSFIL)
{
	int					stPoint;				/* 状態ポイント位置 */
	unsigned char		stBit;					/* 状変ビット位置 */
	unsigned char		stOld;					/* 装置状態（旧状態） */

	/* 通信ステータス状態情報のポイントを算出 */
	stPoint = ststLink + (stNumb - 1) / 8;
	/* 通信ステータス状態情報のビット位置を算出 */
	stBit   = (unsigned char)0x80 >> ((stNumb - 1) % 8);

	/* 通信ステータス状態（旧状態）を取得 */
	stOld = (ps_STSTSFIL + stPoint)->stStat;

	/* 通信ステータス状態取得 */
	if( ( stOld & stBit ) != 0 )
		return( 1 );
	else
		return( 0 );

}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 通信ステータス状変処理（全通信ステータス復帰）
 *
 * 書式：
 * 		int	commfunc_johen_resetstat(
 * 			int soKind,
 * 			int soNumb,
 * 			int ststLink,
 * 			struct st_STSTSFIL *ps_STSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）の情報を元に、指定された装置
 * 		の通信ステータスについて状態情報保存ファイル（STSTSFIL.dat）に格納されて
 * 		いる状態情報を確認し、状態が発生中のものは全て復帰状変処理を行う。
 * 		状態後は状態情報でSTSTSFIL.datを更新する。
 * 		更新位置はststLink（比較開始位置）を元に行う。
 *		その他の引数は、ps_STSTSFIL（通信ステータス状態格納ファイルのMMAPポイン
 *		タ情報）を引数として処理する。 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		ststLink
 * 			通信ステータス状態保存ファイルリンク情報
 * 		*ps_STSTSFIL
 * 			通信ステータス状態保存ファイルMMAPポインタ
 */
int	commfunc_johen_resetstat(
	int soKind,									/* 装置種別 */
	int soNumb,									/* 装置番号 */
	int ststLink,								/* 通信ステータス状態格納ファイルポインタ（当該装置の先頭ポインタ） */
	struct st_STSTSFIL *ps_STSTSFIL )
{
	int					stPoint;				/* 状変ポイント位置 */
	int					stNumb;					/* 状態番号 */
	unsigned char		stBit;					/* 状変ビット位置 */
	unsigned char		stOld;					/* 通信ステータス状態（旧状態） */
	unsigned char		stNew;					/* 通信ステータス状態（新状態） */
	unsigned char		stChek;					/* 状変検出 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_resetstat(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 通信ステータス状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_STSTSFIL);

	for( stNumb = 1 ; stNumb <= PV_MAX_STPOS ; stNumb++ )
	{
		/* 通信ステータス状態情報のポイントを算出 */
		stPoint = ststLink + (stNumb - 1) / 8;
		/* 通信ステータス状態情報のビット位置を算出 */
		stBit   = (unsigned char)0x80 >> ((stNumb - 1) % 8);

		/* 通信ステータス状態（旧状態）を取得 */
		stOld = (ps_STSTSFIL + stPoint)->stStat;
		/* 通信ステータス状態（新状態）を作成 */
		/* 通信ステータス状態をＯＦＦ設定 */
		stNew = stOld & ~stBit;
	
		/* 新状態と旧状態の比較 */
		stChek = (stNew ^ stOld);

#if 0
		printf("commfunc_johen_stat(): STS:%02X, OLD:%02X, CHK:%02X\n",
			stNew, stOld, stChek);
#endif

		/* 通信ステータス状態が不一致 */
		if( stChek )
		{
#if 0
			printf("commfunc_johen_resetstat(): JOHEN(ST)  SKND:%d  SONO:%d  STNO:%d  STS:0  REC:%d\n",
				soKind, soNumb, stNumb, stPoint);
#endif

			/* 復帰状変（マスク無し） */
			commfunc_johen_msgset(soKind, soNumb, PV_SV_SVST_ST, stNumb, 0, PV_SV_KIND_NORMAL, stNumb - 1);
		}
		/* 最新の状態を通信ステータス状態ファイルに記録する */
		/* （状変マスクは関係なく現在状態を保存する） */
		(ps_STSTSFIL + stPoint)->stStat = stNew;
	}

	/* 通信ステータス状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_STSTSFIL);

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変蓄積ファイル格納処理
 *
 * 書式：
 * 		int commfunc_johen_msgset(
 * 			int soKind,
 * 			int soNumb,
 * 			int svstKind,
 * 			int svstNumb,
 * 			int svstStat,
 *			int maskFlag,
 * 			int svstIfLink )
 * 機能：
 * 		引数で指定された状態情報を状変情報として状変蓄積ファイル（JOHENFIL.dat）
 *		に保存する。
 * 		装置種別（soKind）、装置番号（soNumb）、状変種別（svstKind）、状態番号
 *		（svstNumb）、状態情報（svstStat）、状態情報リンク情報（svstIfLink）を
 *		状変蓄積ファイルに保存する。
 * 		保存時には保存処理を行った時刻として積算秒の情報を付加して保存する。
 * 戻り値：
 * 		0：正常処理 ／ -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		svstKind
 * 			状変種別（1：通信ステータス状変／2：装置状態状変）
 *		svstNumb
 * 			状態番号
 *		maskFlag
 *			状変マスクフラグ（0：マスクなし／1：マスクあり）
 * 		svstLink
 * 			状態情報リンク情報（状態情報が直接参照可能なリンク情報）
 */
int commfunc_johen_msgset(
	int soKind,
	int soNumb,
	int svstKind,
	int svstNumb,
	int svstStat,
	int	maskFlag,
	int svstIfLink )
{
	struct timeval		nowtime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_msgset(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 処理対象が「収集装置，運転状態」情報の場合 */
	if( ( svstKind == PV_SV_SVST_SV ) && ( soKind == PV_MKIND_PVSYS ) && ( soNumb == 1 ) && ( svstNumb == PV_SYS_SV_SYSRUN ) )
	{
	}
	else
	{
		/* 状変ロック中の場合 */
		if( commfunc_manage_johenLockFlagGet() != 0 )
		{
			/* 状変マスク対象 */
			maskFlag = PV_SV_KIND_JOMASK;
		}
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_JOHENFIL );

	/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
	if( ps_gpt_JOHENFIL->curPoint >= (PV_MAX_JOHEN-1) )
		/* ポインタを先頭レコードにセット */
		ps_gpt_JOHENFIL->curPoint = 0;
	else
		/* ポインタを+1 */
		ps_gpt_JOHENFIL->curPoint += 1;
	
	/* 状変蓄積の処理ポインタがサーバ送信用処理ポインタに追いついた場合 */
	if( ps_gpt_JOHENFIL->curPoint == ps_gpt_JOHENFIL->tcpPoint )
	{
		/* サーバ送信用ポインタを+1 */
		ps_gpt_JOHENFIL->tcpPoint += 1;
		/* サーバ送信用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_JOHENFIL->tcpPoint >= PV_MAX_JOHEN )
		{
			/* サーバ送信用ポインタを先頭レコードにセット */
			ps_gpt_JOHENFIL->tcpPoint = 0;
		}
	}
	/* 状変蓄積の処理ポインタがIOT処理用ポインタに追いついた場合 */
	if( ps_gpt_JOHENFIL->curPoint == ps_gpt_JOHENFIL->iotPoint )
	{
		/* IOT処理用ポインタを+1 */
		ps_gpt_JOHENFIL->iotPoint += 1;
		/* IOT処理用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_JOHENFIL->iotPoint >= PV_MAX_JOHEN )
		{
			/* IOT処理用ポインタを先頭レコードにセット */
			ps_gpt_JOHENFIL->iotPoint = 0;
		}
	}
	/* 状変蓄積の処理ポインタがログ出力用ポインタに追いついた場合 */
	if( ps_gpt_JOHENFIL->curPoint == ps_gpt_JOHENFIL->outPoint )
	{
		/* ログ出力用ポインタを+1 */
		ps_gpt_JOHENFIL->outPoint += 1;
		/* ログ出力用ポインタが最終レコードを超過した場合 */
		if(ps_gpt_JOHENFIL->outPoint >= PV_MAX_JOHEN )
		{
			/* ログ出力用ポインタを先頭レコードにセット */
			ps_gpt_JOHENFIL->outPoint = 0;
		}
	}
	/* 現在時刻を取得 */
	gettimeofday( &nowtime, NULL );

	/* 状変情報を記録 */
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].sec      = nowtime.tv_sec;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].uSec     = nowtime.tv_usec;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].mKind    = soKind;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].mNumb    = soNumb;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].svstNumb = svstNumb;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].svstKind = svstKind;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].svstStat = svstStat;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].svstLink = svstIfLink;
	ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->curPoint].maskFlag = maskFlag;

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_JOHENFIL );
	
	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * サーバ送信用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_johen_msggettcp(
 * 			char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgTcp で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgTcp
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_johen_msggettcp( char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_msggettcp(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_JOHENFIL );

	/* サーバ送信用ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_JOHENFIL->curPoint != ps_gpt_JOHENFIL->tcpPoint )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_JOHENFIL->tcpPoint >= (PV_MAX_JOHEN-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_JOHENFIL->tcpPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_JOHENFIL->tcpPoint += 1;
	
		/* 状変時刻を取得 */
		johenTime.tv_sec  = ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* 状変マスクフラグが通常状変扱いの場合 */
		if( ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].maskFlag == PV_SV_KIND_NORMAL )
		{

		/* 状変メッセージを生成 */
		sprintf( johenMsgTcp[msgCnt], "JHD,%04d%02d%02d%02d%02d%02d%06ld,%s,%d,%d,%d,%d",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].uSec,
			(ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].svstKind == PV_SV_SVST_ST)? "ST" : "SV",
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].mKind,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].mNumb,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].svstNumb,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->tcpPoint].svstStat );

		msgCnt++;

		}

	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_JOHENFIL );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * IOT送信用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_johen_msggetiot(
 * 			char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgTcp で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgTcp
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_johen_msggetiot( char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_msggettcp(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_JOHENFIL );

	/* IOT送信用ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_JOHENFIL->curPoint != ps_gpt_JOHENFIL->iotPoint )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_JOHENFIL->iotPoint >= (PV_MAX_JOHEN-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_JOHENFIL->iotPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_JOHENFIL->iotPoint += 1;
	
		/* 状変時刻を取得 */
		johenTime.tv_sec  = ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].sec;
		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* 状変マスクフラグが通常状変扱いの場合 */
		if( ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].maskFlag == PV_SV_KIND_NORMAL )
		{
	        //<例>
	        //  {"date":"2018-07-01 01:30:00","mkind":1,"mnumb":10,"type":"sv","no":100,"state":false},

			/* 状変メッセージを生成 */
			sprintf( johenMsgTcp[msgCnt], "{\"date\":\"%04d-%02d-%02d %02d:%02d:%02d.%06d\",\"mkind\":%d,\"mnumb\":%d,\"type\":\"%s\",\"no\":%d,\"state\":%s}",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].uSec,
				ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].mKind,
				ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].mNumb,
				(ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].svstKind == PV_SV_SVST_ST)? "st" : "sv",
				ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].svstNumb,
				(ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->iotPoint].svstStat == 1)? "true" : "false");

			msgCnt++;
		}

	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_JOHENFIL );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * システムログ用状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_johen_msggetlog(
 * 			char (*johenMsgLog)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgLog で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 * 		johenMsgLog
 * 			状変メッセージを格納する文字配列（二次元配列）
 */
int commfunc_johen_msggetlog( char (*johenMsgLog)[PV_MSG_SIZE_JHD] )
{
	int					msgCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_msggetlog(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	commfunc_mutex_lock( PV_FILENO_JOHENFIL );

	/* ログ出力用ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	while( ps_gpt_JOHENFIL->curPoint != ps_gpt_JOHENFIL->outPoint )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( ps_gpt_JOHENFIL->outPoint >= (PV_MAX_JOHEN-1) )
			/* ポインタを先頭レコードにセット */
			ps_gpt_JOHENFIL->outPoint = 0;
		else
			/* ポインタを+1 */
			ps_gpt_JOHENFIL->outPoint += 1;
	
		/* 状変時刻を取得 */
		johenTime.tv_sec  = ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].sec;
		johenTime.tv_usec = ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].uSec,

		/* 年月日時分秒を取得 */
		localtime_r( &johenTime.tv_sec, &localTime );

		/* 状変メッセージを生成 */
		sprintf( johenMsgLog[msgCnt], "%04d/%02d/%02d %02d:%02d:%02d.%06ld %s [%02d,%03d,%03d] : %d %s",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			johenTime.tv_usec,
			(ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].svstKind == PV_SV_SVST_ST)? "ST" : "SV",
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].mKind,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].mNumb,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].svstNumb,
			ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].svstStat,
			(ps_gpt_JOHENFIL->s_jData[ps_gpt_JOHENFIL->outPoint].maskFlag == PV_SV_KIND_NORMAL)? "" : "[MASK]" );

		msgCnt++;
	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock( PV_FILENO_JOHENFIL );
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 最終状変メッセージ取得処理
 *
 * 書式：
 * 		int commfunc_johen_msgget_tail(
 *			int tailCnt,
 * 			char (*johenMsgTcp)[PV_MSG_SIZE_JHD] )
 * 機能：
 * 		johenMsgTcp で指定された二次元文字配列に蓄積された状変メッセージを出力する。
 *		一番最後の状変メッセージから遡った tailCnt 分とする。
 * 戻り値：
 * 		0～n：正常処理（状変メッセージ数） ／ -1：異常終了
 * 入力：
 *		tailCnt
 *			最後から遡った状変数
 * 		ps_JDATA
 * 			状変メッセージを格納する構造体ポインタ
 */
int commfunc_johen_msgget_tail( int tailCnt, struct st_JDATA *ps_JDATA )
{
	int					msgCnt = 0;
	int					curPoint;
	int					syoPoint;
	int					syoCnt = 0;
	struct tm			localTime;
	struct timeval		johenTime;

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_JOHENFIL == NULL )
	{
		printf("commfunc_johen_msgget_tail(): Error JOHENFIL file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 状変蓄積ファイルのミューテックスをロック */
	//commfunc_mutex_lock( PV_FILENO_JOHENFIL );	/* [20180117] DEL 呼び出し元がデバッガなので、mutexは使わない

	/* 現在の処理ポインタを取得 */
	curPoint = ps_gpt_JOHENFIL->curPoint;

	/* 処理開始ポインタを最新のポインタから tailCnt 遡った位置に設定する */
	if( curPoint >= tailCnt )
	{
		syoPoint = curPoint - tailCnt;
	}
	else
	{
		syoPoint = PV_MAX_JOHEN - tailCnt + curPoint;
		
	}

	/* 処理ポインタが状変蓄積処理ポインタと同じになるまで処理を繰り返す */
	/* かつ、編集した状変数が指定された数以下の場合に処理を行う */
	while( (( curPoint != syoPoint ) && ( syoCnt < tailCnt )) || (( tailCnt == PV_MAX_JOHEN ) && ( syoCnt == 0 ) && ( curPoint == syoPoint ))  )
	{
		/* 状変蓄積ファイルの最終レコードにポインタがある場合 */
		if( syoPoint >= (PV_MAX_JOHEN-1) )
			/* ポインタを先頭レコードにセット */
			syoPoint = 0;
		else
			/* ポインタを+1 */
			syoPoint += 1;
	
		ps_JDATA->sec      = ps_gpt_JOHENFIL->s_jData[syoPoint].sec;
		ps_JDATA->uSec     = ps_gpt_JOHENFIL->s_jData[syoPoint].uSec;
		ps_JDATA->mKind    = ps_gpt_JOHENFIL->s_jData[syoPoint].mKind;
		ps_JDATA->mNumb    = ps_gpt_JOHENFIL->s_jData[syoPoint].mNumb;
		ps_JDATA->svstKind = ps_gpt_JOHENFIL->s_jData[syoPoint].svstKind;
		ps_JDATA->svstNumb = ps_gpt_JOHENFIL->s_jData[syoPoint].svstNumb;
		ps_JDATA->svstStat = ps_gpt_JOHENFIL->s_jData[syoPoint].svstStat;
		ps_JDATA->svstLink = ps_gpt_JOHENFIL->s_jData[syoPoint].svstLink;
		ps_JDATA->maskFlag = ps_gpt_JOHENFIL->s_jData[syoPoint].maskFlag;

		ps_JDATA++;
		msgCnt++;
		syoCnt++;
	}

	/* 状変蓄積ファイルのミューテックスをアンロック */
	//commfunc_mutex_unlock( PV_FILENO_JOHENFIL );	/* [20180117] DEL 呼び出し元がデバッガなので、mutexは使わない
	
	return( msgCnt );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 状変処理（ビットフィールド形式）
 *
 * 書式：
 * 		int	commfunc_sv_get(
 * 			int soKind,
 * 			int soNumb,
 * 			int maxSv,
 * 			unsigned char *psvStat,
 *          struct st_SVSTSFIL *ps_SVSTSFIL )
 * 機能：
 *		引数 soKind, soNumb （装置種別、装置番号）で指定された装置の装置状態を
 *		装置状態格納ファイル（SVSTSFIL.dat）から maxSv ビット分取得し、psvStat 
 *		で指定された配列に格納する。
 *		SVSTSFIL.datの格納位置は、管理ファイル（PVSYSINF.dat）から、装置種別と
 *		装置番号を元に算出する。
 *		引数 maxSv で指定されたビット数と装置に定義されているビット数が異なる
 *		場合は、値の小さい方を使用する。
 *		その他の引数は、ps_SVSTSFIL（装置状態格納ファイルのMMAPポインタ情報）
 *      を引数として処理する。
 * 戻り値：
 * 		0：正常処理 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		maxSv
 * 			ビットフィールド数
 * 		*psvStat
 * 			装置状態情報（ビットフィールド形式）の開始ポインタ
 *		*ps_SVSTSFIL
 *			装置状態保存ファイルMMAPポインタ
 */
int	commfunc_sv_get(
	int soKind,
	int soNumb,
	int maxSv,
	unsigned char *psvStat,
	struct st_SVSTSFIL *ps_SVSTSFIL )
{
	int					loopPoint;				/* ループカウンタ（ポイント） */
	int					maxLoop;				/* 最大処理数（ポイント） */
	int					svStLink;				/* 装置状態開始位置 */

	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_sv_get(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 指定された装置が実装されているか確認する */
	if( ps_gpt_PVSYSINF->s_machineInfo[ soKind ].machineCnt == 0 )
		return( -1 );

	/* 装置番号の確認 */
	if( ( soNumb < 1 ) || (ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxMachine < soNumb ) )
		return( -1 );

	/* 装置状態格納位置を取得する */
	svStLink = ps_gpt_PVSYSINF->s_machineInfo[ soKind ].svStLink + ((ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxSv - 1) / 8 + 1) * (soNumb - 1);

	/* 取得情報数を確認 */
	if( ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxSv < maxSv )
		maxSv = ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxSv;

	/* 最大処理ループ数を算出 */
	maxLoop = (maxSv - 1) / 8 + 1;

	/* 装置状態保持ファイルのミューテックスをロック */
	commfunc_mutex_lock(PV_FILENO_SVSTSFIL);

	/* 最大ループカウンタまたは最大処理機器数分繰り返す */
	for(loopPoint = 0 ; loopPoint < maxLoop ; loopPoint++ )
	{
		/* 最新の状態を装置状態ファイルから取得する */
		*(psvStat + loopPoint) = (ps_SVSTSFIL + svStLink + loopPoint)->svStat;
	}

	/* 装置状態保持ファイルのミューテックスをアンロック */
	commfunc_mutex_unlock(PV_FILENO_SVSTSFIL);

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 装置状態情報取得処理
 *
 * 書式：
 * 		int	commfunc_sv_stat_get(
 * 			int soKind,
 * 			int soNumb,
 * 			int svNumb,
 * 			int *psvStat,
 *          struct st_SVSTSFIL *ps_SVSTSFIL )
 * 機能：
 * 		引数 soKind（装置種別）、soNumb（装置番号）、svNumb（状態番号）の情報を
 *		元に、装置状態格納ファイル（SVSTSFIL.dat）に格納されている
 *		状態情報を取得し関数戻り値に設定する。
 * 		状態取得位置はsvStLink（比較開始位置）を元に行う。
 *		その他の引数は、ps_SVSTSFIL（装置状態格納ファイルのMMAPポインタ情報）
 * 戻り値：
 * 		0：復帰状態 / 1：発生状態 / -1：異常終了
 * 入力：
 * 		soKind
 * 			装置種別
 * 		soNumb
 * 			装置番号
 * 		svNumb
 * 			状態番号
 *		*ps_SVSTSFIL
 *			装置状態保存ファイルMMAPポインタ
 */
int	commfunc_sv_stat_get(
	int soKind,
	int soNumb,
	int svNumb,
	int *psvStat,
	struct st_SVSTSFIL *ps_SVSTSFIL )
{
	int					svStLink;				/* 装置状態開始位置 */
	int					stPoint;				/* 状態ポイント位置 */
	unsigned char		stBit;					/* 状変ビット位置 */
	unsigned char		stOld;					/* 装置状態（旧状態） */


	/* グローバルアクセス対象ファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_sv_get(): Error PVSYSINF file not open. (PID:%d)\n", getpid() );
		return( -1 );
	}

	/* 指定された装置が実装されているか確認する */
	if( ps_gpt_PVSYSINF->s_machineInfo[ soKind ].machineCnt == 0 )
		return( -1 );

	/* 装置番号の確認 */
	if( ( soNumb < 1 ) || (ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxMachine < soNumb ) )
		return( -1 );

	/* 状態番号を確認 */
	if( ( svNumb < 1 ) || ( ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxSv < svNumb ))
		return( -1 );

	/* 装置状態格納位置を取得する */
	svStLink = ps_gpt_PVSYSINF->s_machineInfo[ soKind ].svStLink + ((ps_gpt_PVSYSINF->s_machineInfo[ soKind ].maxSv - 1) / 8 + 1) * (soNumb - 1);

	/* 装置状態情報のポイントを算出 */
	stPoint = svStLink + (svNumb - 1) / 8;
	/* 装置状態情報のビット位置を算出 */
	stBit   = (unsigned char)0x80 >> ((svNumb - 1) % 8);

	/* 装置状態（旧状態）を取得 */
	stOld = (ps_SVSTSFIL + stPoint)->svStat;

	/* 装置状態取得 */
	if( ( stOld & stBit ) != 0 )
		*psvStat = 1;
	else
		*psvStat = 0;

	return( 0 );
}

