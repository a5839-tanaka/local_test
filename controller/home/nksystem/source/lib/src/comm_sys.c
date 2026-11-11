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
 * 共通関数：システム関連関数
 *
 * 機能：
 * 		以下の機能を提供する。
 *      ・プロセスＩＤ、スレッドＩＤを取得する関数
 *		・実ユーザID、実グループIDをそれぞれ実効ユーザID、実効グループIDに変更
 */

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pvdefine.h"
#include "pvfile.h"

void commfunc_pidtid_get(int *, int *);
void commfunc_ugid_change();
int	commfunc_machine_type();	/* */
int commfunc_gpio_check();		/* */
int commfunc_gpio_ctl( int );	/* */
int commfunc_get_soft_ver( char *);

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセスＩＤ、スレッドＩＤ取得
 *
 * 機能：
 *		引数 pidNumb および tidNumb に、プロセスＩＤとスレッドＩＤを設定する
 * 戻り値：
 * 		なし
 */
void commfunc_pidtid_get(int *pidNumb, int *tidNumb )
{
	*pidNumb = getpid();
	*tidNumb = syscall( SYS_gettid );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * プロセスの実ユーザＩＤ、実グループＩＤ変更
 *
 * 機能：
 *		プロセスの実ユーザIDと実グループIDをそれぞれ、実効ユーザIDと実効グルー
 *		プIDに変更する
 * 戻り値：
 *		なし
 */
void commfunc_ugid_change()
{
	int						retCode;			/* 戻り値 */
	pid_t					pidNumb;			/* プロセスID */
	pid_t					tidNumb;			/* スレッドID */
	pid_t					rUID,eUID,sUID;		/* 実・実効・保存ユーザID */
	pid_t					rGID,eGID,sGID;		/* 実・実効・保存グループID */

	/* 実・実効・保存ユーザIDを取得 */
	if( (retCode = getresuid( &rUID, &eUID, &sUID )) != 0 )
	{
		commfunc_pidtid_get( &pidNumb, &tidNumb );
		printf( "commfunc_ugid_change(): Error getresuid (errno:%d) (pid/tid:%d/%d)\n", errno, pidNumb, tidNumb );
		return;
	}

	/* 実・実効・保存グループIDを取得 */
	if( (retCode = getresgid( &rGID, &eGID, &sGID )) != 0 )
	{
		commfunc_pidtid_get( &pidNumb, &tidNumb );
		printf("commfunc_ugid_change(): Error getresgid (errno:%d) (pid/tid:%d/%d)\n", errno, pidNumb, tidNumb );
		return;
	}

	/* 実ユーザIDを実効ユーザIDと同じ値に設定 */
	if( (retCode = setresuid( eUID, -1, -1 )) != 0 )
	{
		commfunc_pidtid_get( &pidNumb, &tidNumb );
		printf("commfunc_ugid_change(): Error setresuid ruid(%d -> %d) (errno:%d) (pid/tid:%d/%d)\n",
			rUID, eUID, errno, pidNumb, tidNumb );
		return;
	}

	/* 実グループIDを実効グループIDと同じ値に設定 */
	if( (retCode = setresgid( eGID, -1, -1 )) != 0 )
	{
		commfunc_pidtid_get( &pidNumb, &tidNumb );
		printf("commfunc_ugid_change(): Error setresgid rgid(%d -> %d) (errno:%d) (pid/tid:%d/%d)\n",
			rGID, eGID, errno, pidNumb, tidNumb );
		return;
	}

	return;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 動作環境取得
 *
 * 機能：
 *		動作環境を確認する
 * 戻り値：
 *		0(PV_MTYPE_OTHER)     : 該当無し
 *		2(PV_MTYPE_OPSA_DEB6) : OpenBlocksA6(Debian6)環境
 *		3(PV_MTYPE_OPSA_DEB7) : OpenBlocksA6(Debian7)環境
 */
int commfunc_machine_type()
{
	char					chkStr[256];		/* 確認用文字列 */
	int						fd;					/* ファイルディスクリプタ */
	int						retCode;			/* 戻り値 */

	/* 戻り値を標準環境に設定 */
	retCode = PV_MTYPE_OTHER;

	/* OpenBlocks固有のファイルが存在する場合 */
	if( access( "/etc/openblocks-release", R_OK ) == 0 )
	{
		/* ファイルをオープン */
		if( (fd = open( "/etc/openblocks-release", O_RDONLY )) != -1 )
		{
			/* 先頭２バイトを読み込む */
			read( fd, chkStr, 2);
			/* ファイルクローズ */
			close( fd );

			/* カーネルのメジャーバージョンが2の場合 */
			if( (chkStr[0] == '2') && (chkStr[1] == '.') )
			{
				/* OpenBlocks(Debian6)を設定 */
				retCode = PV_MTYPE_OPSA_DEB6;
			}
			/* カーネルのメジャーバージョンが3の場合 */
			else if( (chkStr[0] == '3') && (chkStr[1] == '.') )
			{
				/* OpenBlocks(Debian7)を設定 */
				retCode = PV_MTYPE_OPSA_DEB7;
			}
		}
	}
	return( retCode );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＧＰＩＯ使用可否チェック
 *
 * 機能：
 *		ＧＰＩＯによるシャットダウン釦操作や動作状態のＬＥＤ表示を行うため、Ｇ
 *		ＰＩＯが使用可能か確認する。
 *		ＧＰＩＯが使用可能かつ初期化未実装の場合はＧＰＩＯの初期化を行う。
 * 戻り値：
 *		 0 : ＧＰＩＯ使用可能
 *		-1 : ＧＰＩＯ使用不可
 */
int commfunc_gpio_check()
{
//	char					chkStr[256];		/* 確認用文字列 */
//	int						retCode;			/* system関数戻り値 */
//	int						mType;				/* 装置タイプ */
//
//	/* GPIO対象装置か確認 */
//	mType = commfunc_machine_type();
//	/* OpenBlocksA6の場合 */
//	if( ( mType == PV_MTYPE_OPSA_DEB6 ) || ( mType == PV_MTYPE_OPSA_DEB7 ))
//	{
//		/* GPIO操作用スクリプトファイルのパス設定 */
//		sprintf( chkStr, "%s%s", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//
//		/* GPIO操作用スクリプトが存在するか確認 */
//		if( access( chkStr, R_OK | X_OK ) != 0 )
//		{
//			/* 存在しないまたはアクセス不能な場合 */
//			return( -1 );
//		}
//		/* GPIO操作用スクリプト（確認コマンド）の生成 */
//		sprintf( chkStr, "%s%s CHECK", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		/* GPIO操作用スクリプトの実行 */
//		if( ( retCode = system( chkStr ) ) == -1 )
//		{
//			/* シェルの実行に失敗した場合 */
//			return ( -1 );
//		}
//		/* 戻り値が0以外（GPIOの初期化が行われていない場合） */
//		if( WEXITSTATUS( retCode ) != 0 )
//		{
//			/* GPIO操作用スクリプト（初期化コマンド）の生成 */
//			sprintf( chkStr, "%s%s INIT", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//			/* GPIO操作用スクリプトの実行 */
//			if( ( retCode = system( chkStr ) ) == -1 )
//			{
//				/* シェルの実行に失敗した場合 */
//				return ( -1 );
//			}
//			/* 戻り値が0以外（GPIOの初期化が失敗した場合） */
//			if( WEXITSTATUS( retCode ) != 0 ) return ( -1 );
//		}
//	}
//	else
//	{
//		return( -1 );
//	}
	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ＧＰＩＯ操作
 *
 * 機能：
 *		引数 gpioCommand の値により下記の処理を行う
 *		PV_GPIO_CMD_SHUT_BT_CHK:
 *			シャットダウン釦が有効かつＯＮ状態の場合は３秒後にもう一度状態を確
 *		    認し、ＯＮの場合は 1 を返す。それ以外の場合は 0 を返す。
 *		PV_GPIO_CMD_SHUT_ST:
 *          シャットダウン処理中ＬＥＤの点灯を行う。成功は 1 、失敗は 0 を返す。
 *		PV_GPIO_CMD_PROG_ST:
 *          プログラム動作中ＬＥＤの点灯を行う。成功は 1 、失敗は 0 を返す。
 *		PV_GPIO_CMD_PROG_ED:
 *          プログラム動作中ＬＥＤの消灯を行う。成功は 1 、失敗は 0 を返す。
 *		PV_GPIO_CMD_ERROR_ON:
 *          エラーＬＥＤの点灯を行う。成功は 1 、失敗は 0 を返す。
 *		PV_GPIO_CMD_ERROR_OFF:
 *          エラーＬＥＤの消灯を行う。成功は 1 、失敗は 0 を返す。
 * 戻り値：
 *		0 : ＬＥＤ操作失敗／シャットダウン釦ＯＦＦまたは無効
 *		1 : ＬＥＤ操作成功／シャットダウン釦ＯＮ
 */
int commfunc_gpio_ctl( int gpioCommand )
{
//	char					errMsg[256];		/* ログ用文字列 */
//	char					chkStr[256];		/* 確認用文字列 */
//	int						retCode;			/* system関数戻り値 */
//
//	/* シャットダウン釦状態の確認 */
//	if( gpioCommand == PV_GPIO_CMD_SHUT_BT_CHK )
//	{
//		/* GPIO操作用スクリプト（シャットダウン釦状態確認コマンド）の生成 */
//		sprintf( chkStr, "%s%s SHUT_BT_CHK", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		/* GPIO操作用スクリプトの実行 */
//		if( ( retCode = system( chkStr ) ) == -1 )
//		{
//			/* スクリプトの実行に失敗した場合 */
//			return( 0 );
//		}
//		/* シェルスクリプトの戻り値を確認 */
//		if( WEXITSTATUS( retCode ) != 99 )
//		{
//			/* シャットダウン釦が無効の場合 */
//			return( 0 );
//		}
//
//		sprintf( errMsg, "Shutdown Button Status --- ON !! (3sec After Re-confirmation)" );
//		commfunc_syslog_msgset( "commfunc_gpio_ctl()", errMsg );
//
//		printf("commfunc_gpio_ctl() : SHUTDOWN BUTTON --- ON (3sec After Re-confirmation)\n");
//
//		/* シャットダウン釦が有効（ＯＮ）の場合は、３秒後にもう一度チェックする */
//		sleep( 3 );
//		/* GPIO操作用スクリプトの実行 */
//		if( ( retCode = system( chkStr ) ) == -1 )
//		{
//			/* スクリプトの実行に失敗した場合 */
//			return( 0 );
//		}
//		/* シェルスクリプトの戻り値を確認 */
//		if( WEXITSTATUS( retCode ) != 99 )
//		{
//			sprintf( errMsg, "Shutdown Button Status --- OFF !! (Result of Re-confirmation)" );
//			commfunc_syslog_msgset( "commfunc_gpio_ctl()", errMsg );
//
//			printf("commfunc_gpio_ctl() : SHUTDOWN BUTTON --- OFF !! (Result of Re-confirmation)\n");
//
//			/* シャットダウン釦が無効の場合 */
//			return( 0 );
//		}
//
//		sprintf( errMsg, "Shutdown Button Status --- ON !! (Result of Re-confirmation)" );
//		commfunc_syslog_msgset( "commfunc_gpio_ctl()", errMsg );
//
//		printf("commfunc_gpio_ctl() : SHUTDOWN BUTTON --- ON !! (Result of Re-confirmation)\n");
//	}
//	else
//	{
//		/* シャットダウン処理開始ＬＥＤ点灯 */
//		if ( gpioCommand == PV_GPIO_CMD_SHUT_ST )
//		{
//			/* GPIO操作用スクリプトの生成 */
//			sprintf( chkStr, "%s%s SHUT_ST", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		}
//		/* プログラム動作中ＬＥＤ点灯 */
//		else if ( gpioCommand == PV_GPIO_CMD_PROG_ST )
//		{
//			/* GPIO操作用スクリプトの生成 */
//			sprintf( chkStr, "%s%s PROG_ST", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		}
//		/* プログラム動作中ＬＥＤ消灯 */
//		else if ( gpioCommand == PV_GPIO_CMD_PROG_ED )
//		{
//			/* GPIO操作用スクリプトの生成 */
//			sprintf( chkStr, "%s%s PROG_ED", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		}
//		/* エラーＬＥＤ点灯 */
//		else if ( gpioCommand == PV_GPIO_CMD_ERROR_ON )
//		{
//			/* GPIO操作用スクリプトの生成 */
//			sprintf( chkStr, "%s%s ERROR_ON", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		}
//		/* エラーＬＥＤ消灯 */
//		else if ( gpioCommand == PV_GPIO_CMD_ERROR_OFF )
//		{
//			/* GPIO操作用スクリプトの生成 */
//			sprintf( chkStr, "%s%s ERROR_OFF", PV_PATH_EXE, PV_FNAME_GPIO_SCRIPT );
//		}
//		else
//		{
//			/* 該当コマンド無しの場合 */
//			return( 0 );
//		}
//
//		/* GPIO操作用スクリプトの実行 */
//		if( ( retCode = system( chkStr ) ) == -1 )
//		{
//			/* スクリプトの実行に失敗した場合 */
//			return( 0 );
//		}
//		/* シェルスクリプトの戻り値を確認 */
//		if( WEXITSTATUS( retCode ) != 0 )
//		{
//			/* 操作が失敗した場合 */
//			return( 0 );
//		}
//	}
	/* 操作成功またはシャットダウン釦ＯＮ */
	return( 1 );
}
/* [20141204] Add Ed */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ソフトウェアバージョン取得
 *
 * 機能：
 *		README.txtのソフトウェアバージョン(日付)を文字列で格納する
 * 戻り値：
 *		 0 : 正常
 *		-1 : 異常(ファイル読み込み異常など)
 */
int commfunc_get_soft_ver( char * verStr)
{
	FILE					*fp;				/* ファイルポインタ */
	char					filePath[256];		/* ファイルパス */
	char					readBuff[128];		/* ファイル名読み込みバッファ */
	int						retCode = -1;		/* 戻り値 */

	/* README.txtファイルのパス設定 */
	sprintf( filePath, "%sREADME.txt", PV_PATH_EXE );

	/* ファイルが存在するか確認 */
	if( access( filePath, R_OK ) != 0 )
	{
		/* 存在しないまたはアクセス不能な場合 */
		return retCode;
	}

	/* Tファイルオープン */
	if( (fp = fopen(filePath, "r")) != NULL)
	{
		/* DATファイルの終端まで繰り返す */
		if( fgets( readBuff, sizeof(readBuff), fp) != NULL )
		{
			readBuff[strlen(readBuff)-1] = 0;
			sprintf( verStr, "%s", readBuff);
			retCode = 0;
		}

		/* ファイルをクローズ */
		fclose( fp );
	}

	return retCode;
}
