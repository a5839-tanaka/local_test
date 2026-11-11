/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 * -----------------------------------------------------------------------------
 * [20240701] 2024.07.01 機能改良
 * ・ログの時刻情報をマイクロ秒まで出力するように修正
 * [20230902] 2023.09.02
 * ・ログ出力ファイルの保持数をdefine定義に変更
 * ・指令データのログ出力を常に出力するように修正
 */

/*
 * バーチャルプラントシステム
 * -----------------------------------------------------------------------------
 * 共通関数：json関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・jsonデータを動的メモリに保持し、解析した情報を管理情報に保持する。
 * 		・jsonデータからタブ情報に一致する値を返却する
 * 		・jsonデータの動的メモリを解放する
 */

#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "pvdefine.h"
#include "pvfile.h"
#include "pvcomm.h"
#include "pvjson.h"

#if 0
#define			PV_DEBUG_MINIMUM
#define			PV_DEBUG_DATA_ANALIZE
#define			PV_DEBUG_VALUE_SEARCH
#endif

#define 		PV_JSON_CODE_SPACE			0x20			/* ' ' */
#define 		PV_JSON_CODE_DOUBLE_Q		0x22			/* "  */
#define 		PV_JSON_CODE_COLON			0x3A			/* : */
#define 		PV_JSON_CODE_PLUS			0x2B			/* - */
#define 		PV_JSON_CODE_COMMA			0x2C			/* , */
#define 		PV_JSON_CODE_MINUS			0x2D			/* - */
#define 		PV_JSON_CODE_DECIMAL_POINT	0x2E			/* . */
#define 		PV_JSON_CODE_BRACE_S		0x7B			/* { */
#define 		PV_JSON_CODE_BRACE_E		0x7D			/* } */
#define 		PV_JSON_CODE_SQUARE_S		0x5B			/* [ *//* 配列は非サポート */
#define 		PV_JSON_CODE_SQUARE_E		0x5D			/* ] *//* 配列は非サポート */
#define 		PV_JSON_CODE_LF				0x0A			/* <LF>(改行コード) */
#define 		PV_JSON_CODE_CR				0x0D			/* <CR>(Carriage Return：行頭復帰) */
#define 		PV_JSON_CODE_TAB			0x09			/* <\t>(タブ) */
#define 		PV_JSON_CODE_YEN			0x5C			/* \ */
#define 		PV_JSON_CODE_n				0x6E			/* n */
#define 		PV_JSON_CODE_r				0x72			/* r */

static char		errMsg[PV_MSG_SIZE_LOG];	/* エラーメッセージ */

int func_json_seq_tag( char *ps_data, struct st_tagStr * );
int func_json_conv_strTotime_t( char *, time_t * );
int func_json_get_days( int , int  );
int func_numb_format_check( char * );
int func_date_numb_check( char * );
int func_returnStrCheck( char * );

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解析処理
 *
 * 機能：
 *		jsonデータを動的メモリにコピーし、解析する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_parse( struct st_pvJsonParse *ps_jsonParse, int readSize, int payloadLen, char *pc_payload, char *pc_errMsg )
{
	int					retCode;				/* 処理結果 */

	/* 動的メモリが残っている場合 */
	if( ps_jsonParse->jsonDataPtr != NULL )
	{
		/* 動的メモリを解放する */
		commfunc_json_free( ps_jsonParse );
	}

	/* 管理情報を初期化する */
	memset(ps_jsonParse, 0, sizeof(struct st_pvJsonParse));


	/* 動的メモリを確保する */
	ps_jsonParse->jsonDataPtr = (char *)malloc( payloadLen );
	/* 初期化する */
	memset(ps_jsonParse->jsonDataPtr, 0, payloadLen);

	/* 解析処理 */
	if( readSize == 0 )
	{
		readSize = payloadLen;
	}
	retCode = commfunc_json_parse_sub( ps_jsonParse, readSize, pc_payload, pc_errMsg );

	int		loopCnt1;
	int		loopCnt2;
	for( loopCnt1 = 0; loopCnt1 < ps_jsonParse->dataNum; loopCnt1++ )
	{
		memset(errMsg, 0, sizeof(errMsg));
		for( loopCnt2 = 0; loopCnt2 < ps_jsonParse->dataInfo[loopCnt1].treeNum; loopCnt2++ )
		{
			if( loopCnt2 > 0 )
				sprintf(&errMsg[strlen(errMsg)], ".");
			
			if( ps_jsonParse->dataInfo[loopCnt1].treeName[loopCnt2] != NULL )
				sprintf(&errMsg[strlen(errMsg)], "%s", ps_jsonParse->dataInfo[loopCnt1].treeName[loopCnt2]);
			else
				sprintf(&errMsg[strlen(errMsg)], "null");

			sprintf(&errMsg[strlen(errMsg)], "[%d,%d]", ps_jsonParse->dataInfo[loopCnt1].arrayNumb[loopCnt2], ps_jsonParse->dataInfo[loopCnt1].groupNumb[loopCnt2]);

		}
		if( ps_jsonParse->dataInfo[loopCnt1].tagName != NULL )
			sprintf(&errMsg[strlen(errMsg)], ".%s", ps_jsonParse->dataInfo[loopCnt1].tagName);
		
#ifdef PV_DEBUG_MINIMUM
		printf("%s\n", errMsg);
#endif
	}

	/* 読み込みサイズがpayloadと異なる場合は、データ終端のエラーはエラーとしない */
	if( ( readSize != payloadLen ) &&
	    ( retCode == PV_JSON_ERR_DATAEND ) )
	{
		retCode = PV_JSON_RET_OK;
	}
	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解析処理
 *
 * 機能：
 *		jsonデータをタグ、ツリー階層、値に分け、格納する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_parse_sub( struct st_pvJsonParse *ps_jsonParse, int readSize, char *pc_payload, char *pc_errMsg )
{
	int									loopCnt1;
	int									treeCnt = 0;
	int									readPos=0;
	char								*pc_treePtr[PV_JSON_TREE_MAX];
	char								*pc_set_ptr;
	int									retCode;
	int									readCnt;
	int									dataType;
	char								*pc_tagPtr;
	char								*pc_valPtr;
	char								dataBuff[PV_JSON_DATALEN_MAX];
	struct st_pvJsonDataInfo			*ps_dataInfoPtr;
	int									arrayID[PV_JSON_TREE_MAX];
	int									groupID[PV_JSON_TREE_MAX];
	int									tagIdNumb=0;			/* 識別子 */
	int									valueType;

	memset( arrayID, 0, sizeof( arrayID ));
	memset( groupID, 0, sizeof( groupID ));

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("commfunc_json_parse_sub() : Start\n");
#endif

	/* 保持データの先頭アドレスを取得 */
	pc_set_ptr = ps_jsonParse->jsonDataPtr;

	/* 開始文字列({) を検索 */
	retCode = commfunc_json_parse_start( (pc_payload+readPos), ( readSize - readPos ), dataBuff, &readCnt, pc_errMsg );
	/* 読み込み位置を更新 */
	readPos += readCnt;

	if( retCode != PV_JSON_RET_OK )
	{
		return retCode;
	}

	while( 1 )
	{
		/* タグ名の取得 */
		memset(dataBuff, 0, PV_JSON_DATALEN_MAX);
		/* 配列だけど、'{'の中ならタグ検索 */
		if( ( treeCnt > 0 ) && ( arrayID[treeCnt-1] > 0 ) && groupID[treeCnt] == 0 )
			retCode = commfunc_json_parse_array_value( (pc_payload+readPos), ( readSize - readPos ), dataBuff, &dataType, &readCnt, &valueType, pc_errMsg );
		else
		retCode = commfunc_json_parse_tag( (pc_payload+readPos), ( readSize - readPos ), dataBuff, &dataType, &readCnt, pc_errMsg );

		if( retCode != PV_JSON_RET_OK )
		{
			break;
		}

		/* 読み込み位置を更新 */
		readPos += readCnt;
		/* タグの場合 */
		if( dataType == PV_JSON_TYPE_TAG )
		{
			/* 階層がマイナスの状態で、タグがある場合は、フォーマット異常 */
			if( treeCnt < 0 )
			{
				sprintf(errMsg, "allready json format end.");
				commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//				retCode = PV_JSON_ERR_OTHER;
				retCode = PV_JSON_ERR_FORMAT;
				break;
			}
			/* タグ名を取得 */
			memcpy( pc_set_ptr, dataBuff, strlen(dataBuff));
			/* タグ名のポインタを保持 */
			pc_tagPtr = pc_set_ptr;
			/* 次回書き込み位置を更新 */
			pc_set_ptr += (strlen(dataBuff) + 1);

			/* 値の取得 */
			memset(dataBuff, 0, PV_JSON_DATALEN_MAX);
			retCode = commfunc_json_parse_value( (pc_payload+readPos), ( readSize - readPos ), dataBuff, &dataType, &readCnt, &valueType, pc_errMsg );

			if( retCode != PV_JSON_RET_OK )
			{
				return retCode;
			}

			/* 読み込み位置を更新 */
			readPos += readCnt;
			/* 値の場合 */
			if( dataType == PV_JSON_TYPE_VALUE )
			{
				/* データ情報ポインタを取得 */
				ps_dataInfoPtr = &ps_jsonParse->dataInfo[ps_jsonParse->dataNum];

				/* データ数が管理情報の上限を到達した場合 */
				if( (ps_jsonParse->dataNum + 1) >= PV_JSON_DATA_MAX )
				{
					sprintf(errMsg, "data count is limit over(%d).", PV_JSON_DATA_MAX);
					commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
					printf("%s\n", errMsg);
#endif
					memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
					retCode = PV_JSON_ERR_DATAOVER;
					break;
				}

				/* 値を取得 */
				memcpy( pc_set_ptr, dataBuff, strlen(dataBuff));
				/* 値のポインタを保持 */
				pc_valPtr = pc_set_ptr;
				/* 次回書き込み位置を更新 */
				pc_set_ptr += (strlen(dataBuff) + 1);

				/*** データ情報を設定する ***/
				/* ツリー階層とツリー名を保持 */
				ps_dataInfoPtr->treeNum = treeCnt;
				for( loopCnt1 = 0; loopCnt1 < treeCnt; loopCnt1++ )
				{
					/* タグ数をカウントする */
					if( pc_treePtr[loopCnt1] != NULL )
					{
						ps_dataInfoPtr->tagNum++;
					}
					ps_dataInfoPtr->treeName[loopCnt1] = pc_treePtr[loopCnt1];
					ps_dataInfoPtr->arrayNumb[loopCnt1] = arrayID[loopCnt1];
					ps_dataInfoPtr->groupNumb[loopCnt1] = groupID[loopCnt1];
				}

				/* タグ名を保持 */
				ps_dataInfoPtr->tagName = pc_tagPtr;
				/* 値を保持 */
				ps_dataInfoPtr->value = pc_valPtr;
				/* 値種別を保持 */
				ps_dataInfoPtr->dataType = valueType;
				/* データ数をインクリメント */
				ps_jsonParse->dataNum++;
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("dataNum=%d, treeCnt=%d, %s.%s\n", ps_jsonParse->dataNum, ps_dataInfoPtr->treeNum, ps_dataInfoPtr->tagName, ps_dataInfoPtr->value);
#endif
			}
/* [20210813] Add St */
			/* 配列の開始の場合 */
			else if( dataType == PV_JSON_TYPE_ARRAY_S )
			{
				/* 階層が管理情報の上限を到達した場合 */
				if( (treeCnt + 1) >= PV_JSON_TREE_MAX )
				{
					sprintf(errMsg, "tree class count is limit over(%d).", PV_JSON_TREE_MAX);
					commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
					printf("%s\n", errMsg);
#endif
					memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
					retCode = PV_JSON_ERR_TREEOVER;
					break;
				}
				/* 階層名のポインタを保持 */
				pc_treePtr[treeCnt] = pc_tagPtr;
				/* 配列番号を初期化 */
				arrayID[treeCnt] = 1;
				groupID[treeCnt] = 0;

				/* 階層数をインクリメント */
				treeCnt++;
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("treeCnt=%d, arrayNumb[%d]\n", treeCnt, arrayID[treeCnt-1]);
#endif
			}
/* [20210813] Add Ed */
			/* 階層の開始の場合 */
			else if( dataType == PV_JSON_TYPE_TREE_S )
			{
				/* 階層が管理情報の上限を到達した場合 */
				if( (treeCnt + 1) >= PV_JSON_TREE_MAX )
				{
					sprintf(errMsg, "tree class count is limit over(%d).", PV_JSON_TREE_MAX);
					commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
					printf("%s\n", errMsg);
#endif
					memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
					retCode = PV_JSON_ERR_TREEOVER;
					break;
				}
				/* 階層名のポインタを保持 */
				pc_treePtr[treeCnt] = pc_tagPtr;
				/* タグ識別子をインクリメント */
				tagIdNumb++;
				/* 配列番号を初期化 */
				arrayID[treeCnt] = 0;
				groupID[treeCnt] = tagIdNumb;

				/* 階層数をインクリメント */
				treeCnt++;

#ifdef PV_DEBUG_DATA_ANALIZE
				printf("treeCnt=%d, %s\n", treeCnt, pc_tagPtr);
#endif
			}
		}
		/* 値の場合 */
		else if( dataType == PV_JSON_TYPE_VALUE )
		{
			/* 値を取得 */
			memcpy( pc_set_ptr, dataBuff, strlen(dataBuff));
			/* 値のポインタを保持 */
			pc_valPtr = pc_set_ptr;
			/* 次回書き込み位置を更新 */
			pc_set_ptr += (strlen(dataBuff) + 1);

			/* 現在のタブが配列じゃない場合は、フォーマット異常 */
			if( arrayID[treeCnt-1] == 0 )
			{
				sprintf(errMsg, "json format Error ( Not Arrray ).");
				commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
	#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
	#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
				retCode = PV_JSON_ERR_FORMAT;
				break;
			}
			
printf("ROOT1\n");
			/* データ情報ポインタを取得 */
			ps_dataInfoPtr = &ps_jsonParse->dataInfo[ps_jsonParse->dataNum];

printf("ROOT2\n");
			/* データ数が管理情報の上限を到達した場合 */
			if( (ps_jsonParse->dataNum + 1) >= PV_JSON_DATA_MAX )
			{
				sprintf(errMsg, "data count is limit over(%d).", PV_JSON_DATA_MAX);
				commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
				retCode = PV_JSON_ERR_DATAOVER;
				break;
			}
printf("ROOT3\n");

			/*** データ情報を設定する ***/
			/* ツリー階層とツリー名を保持 */
			ps_dataInfoPtr->treeNum = treeCnt;
			for( loopCnt1 = 0; loopCnt1 < treeCnt; loopCnt1++ )
			{
				/* タグ数をカウントする */
				if( pc_treePtr[loopCnt1] != NULL )
				{
					ps_dataInfoPtr->tagNum++;
				}
				ps_dataInfoPtr->treeName[loopCnt1] = pc_treePtr[loopCnt1];
				ps_dataInfoPtr->arrayNumb[loopCnt1] = arrayID[loopCnt1];
				ps_dataInfoPtr->groupNumb[loopCnt1] = groupID[loopCnt1];
			}
printf("ROOT2\n");
			/* タグ名はNULL */
			ps_dataInfoPtr->tagName = NULL;
			/* 値を保持 */
			ps_dataInfoPtr->value = pc_valPtr;
			/* 値種別を保持 */
			ps_dataInfoPtr->dataType = valueType;
			/* データ数をインクリメント */
			ps_jsonParse->dataNum++;
			/* 要素数をカウントアップ */
			arrayID[treeCnt-1]++;
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("dataNum=%d, treeCnt=%d[%d], %s\n", ps_jsonParse->dataNum, ps_dataInfoPtr->treeNum, arrayID[treeCnt-1]-1, ps_dataInfoPtr->value);
#endif
		}
		/* タグのない配列の開始の場合 */
		else if( dataType == PV_JSON_TYPE_ARRAY_S )
		{

			sprintf(errMsg, "json format Error ('[').");
			commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
			retCode = PV_JSON_ERR_FORMAT;
			break;
		}
		/* タグのない階層の場合 */
		else if( dataType == PV_JSON_TYPE_TREE_S )
		{
			/* 階層が管理情報の上限を到達した場合 */
			if( (treeCnt + 1) >= PV_JSON_TREE_MAX )
			{
				sprintf(errMsg, "tree class count is limit over(%d).", PV_JSON_TREE_MAX);
				commfunc_syslog_msgset( "commfunc_json_parse_sub()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
				retCode = PV_JSON_ERR_TREEOVER;
				break;
			}
			/* 階層名のポインタを保持 */
			pc_treePtr[treeCnt] = NULL;
			/* タグ識別子をインクリメント */
			tagIdNumb++;
			/* 配列番号を初期化 */
			groupID[treeCnt] = tagIdNumb;
			arrayID[treeCnt] = 0;

			/* 階層数をインクリメント */
			treeCnt++;

#ifdef PV_DEBUG_DATA_ANALIZE
			printf("treeCnt=%d, groupNumb[%d]=%d\n", treeCnt, treeCnt-1, groupID[treeCnt-1]);
#endif
		}
		/* 階層の終了の場合 */
		else if( dataType == PV_JSON_TYPE_TREE_E )
		{
			/* 階層数をデクリメント */
			treeCnt--;
			/* 階層がマイナス(データ終了しかない)の場合は終了 */
			if( treeCnt < 0 )
			{
				/* 解析終了 */
				retCode = PV_JSON_RET_OK;
				break;
			}
			if( arrayID[treeCnt-1] > 0 )
			{
				arrayID[treeCnt-1]++;
			}
			groupID[treeCnt] = 0;
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("treeCnt=%d\n", treeCnt);
#endif
		}
/* [20210813] Add St */
		/* 配列の終了の場合 */
		else if( dataType == PV_JSON_TYPE_ARRAY_E )
		{
			/* 配列番号を初期化 */
			groupID[treeCnt] = 0;
			arrayID[treeCnt] = 0;

			/* 階層数をデクリメント */
			treeCnt--;
			/* 階層がマイナス(データ終了しかない)の場合は終了 */
			if( treeCnt < 0 )
			{
				/* 解析終了 */
				retCode = PV_JSON_RET_OK;
				break;
			}
		}
/* [20210813] Add Ed */
	}
	
#ifdef PV_DEBUG_DATA_ANALIZE
	printf("commfunc_json_parse_sub() : End, retCode=%d\n", retCode);
#endif

	return retCode;

}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解析処理
 *
 * 機能：
 *		開始文字列 { を検索する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_parse_start( char *pc_dataptr, int dataSize, char *pc_dataBuff, int *pl_readCnt, char *pc_errMsg )
{
	int									loopCnt1;
	int			retCode;

#ifdef PV_DEBUG_DATA_ANALIZE
	memset(errMsg, 0, sizeof(errMsg));
	memcpy(errMsg, pc_dataptr, PV_JSON_DATALEN_MAX);
	printf("commfunc_json_parse_start() : Start : data=%s\n", errMsg);
#endif

	for( loopCnt1 = 0; loopCnt1 < dataSize; loopCnt1++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 ));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json start character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
		/* 改行文字列を検出した場合 */
		else if( retCode > 0 )
		{
			/* 対象文字をスキップする */
			loopCnt1+=retCode;
			continue;
		}
		/* '{'を検索 */
		if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_BRACE_S )
		{
			/* :の後から検索する為、インクリメント */
			loopCnt1++;
			break;
		}
		/* 空白 \t 改行 以外はフォーマット異常 */
		else if( ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_SPACE ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_LF ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_CR ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_TAB ))
		{
			sprintf(errMsg, "json start character is not '{'('%c').", *( pc_dataptr + loopCnt1 ));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//			return PV_JSON_ERR_OTHER;
			return PV_JSON_ERR_FORMAT;
		}
	}

	/* 読み込みデータサイズを設定 */
	*pl_readCnt = loopCnt1;

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("commfunc_json_parse_start() : End : readCnt=%d\n", *pl_readCnt);
#endif

	return PV_JSON_RET_OK;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解析処理
 *
 * 機能：
 *		タグ名称を検索する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_parse_tag( char *pc_dataptr, int dataSize, char *pc_dataBuff, int *pl_type, int *pl_readCnt, char *pc_errMsg )
{
	int			loopCnt1;
	int			loopCnt2;
	int			retCode;

#ifdef PV_DEBUG_DATA_ANALIZE
	memset(errMsg, 0, sizeof(errMsg));
	memcpy(errMsg, pc_dataptr, PV_JSON_DATALEN_MAX);
	printf("commfunc_json_parse_tag() : Start : data=%s\n", errMsg);
#endif

	for( loopCnt1 = 0; loopCnt1 < dataSize; loopCnt1++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 ));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json start character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
		/* 改行文字列を検出した場合 */
		else if( retCode > 0 )
		{
			/* 対象文字をスキップする */
			loopCnt1+=retCode;
			continue;
		}

		/* '"'を検索 */
		if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_DOUBLE_Q )
		{
			/* "の後から検索する為、インクリメント */
			loopCnt1++;
			*pl_type = PV_JSON_TYPE_TAG;
			break;
		}
		/* } はタグ終了 */
		else if(  *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_BRACE_E )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_TREE_E;
			sprintf(errMsg, "found '}'. tag end.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* ] は配列終了 */
		else if(  *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_SQUARE_E )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_ARRAY_E;
			sprintf(errMsg, "found '}'. tag end.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* { はタグ開始 */
		else if(  *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_BRACE_S )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_TREE_S;
			sprintf(errMsg, "found '}'. tag end.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* [ は配列開始 */
		else if(  *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_SQUARE_S )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_ARRAY_S;
			sprintf(errMsg, "found '}'. tag end.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* 空白 : { \n \t , 以外はフォーマット異常 */
		else if( ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_SPACE ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_COLON ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_LF ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_CR ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_TAB ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_COMMA ) )
		{
			sprintf(errMsg, "\"Tag\" start character is not '\"'('%c').", *( pc_dataptr + loopCnt1 ));
			commfunc_syslog_msgset( "commfunc_json_parse_tag()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//			return PV_JSON_ERR_TAG;
			return PV_JSON_ERR_FORMAT;
		}
	}

	/* ファイル終端の場合 */
	if( loopCnt1 >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}

	/* データ長上限の間に"を検索する */
	for( loopCnt2 = 0; loopCnt2 < PV_JSON_DATALEN_MAX; loopCnt2++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 + loopCnt2));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json Tag character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ), *( pc_dataptr + loopCnt1 + loopCnt2 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
		/* 改行文字列を検出した場合 */
		else if( retCode > 0 )
		{
			sprintf(errMsg, "json Tag character is returnStr NG. '('%c%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ), *( pc_dataptr + loopCnt1 + loopCnt2 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}

		if( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_DOUBLE_Q )
		{
			break;
		}
		// タブ、改行、は異常
		else if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ) ||
				 ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
				 ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) )
		{
			sprintf(errMsg, "json Tag character is Not Support '('%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
	}

	/* タグ名長の上限を超えている場合 */
	if( loopCnt2 >= PV_JSON_DATALEN_MAX )
	{
		sprintf(errMsg, "\"Tag\" string is over length(%d).", PV_JSON_DATALEN_MAX);
		commfunc_syslog_msgset( "commfunc_json_parse_tag()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_LENGTH;
	}
	/* データがない場合 */
	else if( loopCnt2 == 0 )
	{
		sprintf(errMsg, "\"Tag\" string is no data.");
		commfunc_syslog_msgset( "commfunc_json_parse_tag()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//		return PV_JSON_ERR_TAG;
		return PV_JSON_ERR_FORMAT;
	}
	/* データサイズを超えている場合 */
	else if( (loopCnt1 + loopCnt2) >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}

	/* タグ名取得を設定 */
	memcpy(pc_dataBuff, pc_dataptr+loopCnt1, loopCnt2);

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("tags found : Type[%d] %s\n", *pl_type, pc_dataBuff);
#endif

	/* 読み込みデータサイズを設定(終わりの"を含める) */
	*pl_readCnt = loopCnt1 + loopCnt2 +1;

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("commfunc_json_parse_tag() : End : readCnt=%d\n", *pl_readCnt);
#endif
	return PV_JSON_RET_OK;

}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解析処理
 *
 * 機能：
 *		値を検索する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_parse_value( char *pc_dataptr, int dataSize, char *pc_dataBuff, int *pl_type, int *pl_readCnt, int *pl_valueType, char *pc_errMsg )
{
	int			loopCnt1;
	int			loopCnt2;
	int			valueType;
	char		numbStr[PV_JSON_DATALEN_MAX];
	int			retCode;

#ifdef PV_DEBUG_DATA_ANALIZE
	memset(errMsg, 0, sizeof(errMsg));
	memcpy(errMsg, pc_dataptr, PV_JSON_DATALEN_MAX);
	printf("commfunc_json_parse_value() : Start : data=%s\n", errMsg);
#endif
	
	for( loopCnt1 = 0; loopCnt1 < dataSize; loopCnt1++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 ));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json start character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
		/* 改行文字列を検出した場合 */
		else if( retCode > 0 )
		{
			/* 対象文字をスキップする */
			loopCnt1+=retCode;
			continue;
		}

		/* ':'を検索 */
		if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_COLON )
		{
			/* :の後から検索する為、インクリメント */
			loopCnt1++;
			break;
		}
		/* 空白 \t 以外はフォーマット異常 */
		else if( ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_SPACE ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_LF ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_CR ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_TAB ))
		{
			sprintf(errMsg, "\"tag\" and \"value\" delimiter character is no ':'('%c').", *( pc_dataptr + loopCnt1 ));
			commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//			return PV_JSON_ERR_VALUE;
			return PV_JSON_ERR_FORMAT;
		}
	}

	/* ファイル終端の場合 */
	if( loopCnt1 >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}

	/* 値の開始となる文字を検索 */
	for( ; loopCnt1 < dataSize; loopCnt1++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 ));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json start character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
		/* 改行文字列を検出した場合 */
		else if( retCode > 0 )
		{
			/* 対象文字をスキップする */
			loopCnt1+=retCode;
			continue;
		}

		/* " を検出 */
		if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_DOUBLE_Q )
		{
			valueType = PV_JSON_VALUE_TYPE_STR;
			/* "の後から検索する為、インクリメント */
			loopCnt1++;
#ifdef PV_DEBUG_DATA_ANALIZE
			sprintf(errMsg, "value type is String.");
			printf("%s\n", errMsg);
#endif
			*pl_type = PV_JSON_TYPE_VALUE;
			break;
		}
		/* { を検出 */
		else if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_BRACE_S )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_TREE_S;
			sprintf(errMsg, "found '{'. tag start.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* [ を検出 */
		else if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_SQUARE_S )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_ARRAY_S;
			sprintf(errMsg, "found '['. tag start.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* 数字を検出 */
		else if( ( ( *( pc_dataptr + loopCnt1 ) >= 0x30 ) &&
				   ( *( pc_dataptr + loopCnt1 ) <= 0x39 ) ) ||
				 ( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_MINUS )  ||
				 ( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_PLUS ) )
		{
			valueType = PV_JSON_VALUE_TYPE_NUM;
			*pl_type = PV_JSON_TYPE_VALUE;
#ifdef PV_DEBUG_DATA_ANALIZE
			sprintf(errMsg, "value type is Number.");
			printf("%s\n", errMsg);
#endif
			break;
		}
		else if( ( *( pc_dataptr + loopCnt1 ) == 't' ) ||
				 ( *( pc_dataptr + loopCnt1 ) == 'n' ) ||
				 ( *( pc_dataptr + loopCnt1 ) == 'f' ))
		{
			valueType = PV_JSON_VALUE_TYPE_BOOL;
			*pl_type = PV_JSON_TYPE_VALUE;
#ifdef PV_DEBUG_DATA_ANALIZE
			sprintf(errMsg, "value type is bool[%s].", ( pc_dataptr + loopCnt1 ));
			printf("%s\n", errMsg);
#endif
			break;
		}
		/* 空白 \t 以外はフォーマット異常 */
		else if( ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_SPACE ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_LF ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_CR ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_TAB ))
		{
			sprintf(errMsg, "\"Value\" start character Error('%c').", *( pc_dataptr + loopCnt1 ));
			commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//			return PV_JSON_ERR_VALUE;
			return PV_JSON_ERR_FORMAT;
		}
	}

	/* 値種別を設定 */
	*pl_valueType = valueType;

	/* ファイル終端の場合 */
	if( loopCnt1 >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}


	/* データ長上限の間に"または,}を検索する */
	for( loopCnt2 = 0; loopCnt2 < PV_JSON_DATALEN_MAX; loopCnt2++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 + loopCnt2));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json Value character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ), *( pc_dataptr + loopCnt1 + loopCnt2 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}

		/* 文字列の場合 */
		if( valueType ==  PV_JSON_VALUE_TYPE_STR )
		{
			/* 改行文字列を検出した場合 */
			if( retCode > 0 )
			{
				sprintf(errMsg, "json String character is returnStr NG. '('%c%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ), *( pc_dataptr + loopCnt1 + loopCnt2 +1));
				commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
				return PV_JSON_ERR_FORMAT;
			}

			if( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_DOUBLE_Q )
			{
				break;
			}
			/* 改行 \t の場合は、フォーマット異常 */
			else if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ) ||
					 ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
					 ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) )
			{
				sprintf(errMsg, "\"Value\" character is illegal('%c').", *( pc_dataptr + loopCnt1 ));
				commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//				return PV_JSON_ERR_VALUE;
				return PV_JSON_ERR_FORMAT;
			}
		}
		/* bool型の場合 */
		else if( valueType == PV_JSON_VALUE_TYPE_BOOL )
		{
			/* 文字列の先頭の場合 */
			if( loopCnt2 == 0 )
			{
				if( strncmp( "true", (pc_dataptr + loopCnt1), 4 ) == 0 )
				{
					loopCnt2 += 3;
				}
				else if( strncmp( "null", (pc_dataptr + loopCnt1), 4 ) == 0 )
				{
					loopCnt2 += 3;
				}
				else if( strncmp( "false", (pc_dataptr + loopCnt1), 5 ) == 0 )
				{
					loopCnt2 += 4;
				}
				else
				{
					sprintf(errMsg, "\"Bool\" character is illegal('%s').", *( pc_dataptr + loopCnt1 ));
					commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
					printf("%s\n", errMsg);
#endif
					memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
					return PV_JSON_ERR_FORMAT;
				}
			}
			/* 改行文字列を検出した場合 */
			else if( retCode > 0 )
			{
				/* 対象文字をスキップし、終了と判断する*/
				loopCnt2+=retCode;
				break;
			}
			/* 空白 , { : \t を検出した場合、数値の終了と判断する */
			else if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_COMMA ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_SPACE ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_BRACE_E ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ))
			{
				break;
			}
			/* 上記以外の場合はフォーマット異常 */
			else
			{
				sprintf(errMsg, "\"Bool\" character is illegal('%s').", *( pc_dataptr + loopCnt1 ));
				commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
				return PV_JSON_ERR_FORMAT;
			}
		}
		/* 数値の場合 */
		else if( valueType ==  PV_JSON_VALUE_TYPE_NUM )
		{
			/* 改行文字列を検出した場合 */
			if( retCode > 0 )
			{
				/* 対象文字をスキップし、終了と判断する*/
				loopCnt1+=retCode;
				break;
			}
			/* 空白 , { : \t を検出した場合、数値の終了と判断する */
			if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_COMMA ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_SPACE ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_BRACE_E ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ))
			{
				break;
			}
			/* 数字、. 以外の場合はフォーマット異常 */
			else if( (( *( pc_dataptr + loopCnt1 + loopCnt2 ) < 0x30 ) ||
					  ( *( pc_dataptr + loopCnt1 + loopCnt2 ) > 0x39 )) &&
					 ( *( pc_dataptr + loopCnt1 + loopCnt2 ) != PV_JSON_CODE_MINUS )  &&
					 ( *( pc_dataptr + loopCnt1 + loopCnt2 ) != PV_JSON_CODE_PLUS ) &&
					 ( *( pc_dataptr + loopCnt1 + loopCnt2 ) != PV_JSON_CODE_DECIMAL_POINT ) )
			{
				sprintf(errMsg, "\"Value\" is not nummber character('%c'(0x%02X)).", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 ));
				commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//				return PV_JSON_ERR_VALUE;
				return PV_JSON_ERR_FORMAT;
			}
		}
	}

	/* 値長の上限を超えている場合 */
	if( loopCnt2 >= PV_JSON_DATALEN_MAX )
	{
		sprintf(errMsg, "\"Value\" end character is not found.");
		commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_LENGTH;
	}
	/* データがない場合 */
	else if( loopCnt2 == 0 )
	{
		sprintf(errMsg, "\"Value\" string is no data.");
		commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//		return PV_JSON_ERR_VALUE;
		return PV_JSON_ERR_FORMAT;
	}
	/* データサイズを超えている場合 */
	else if( (loopCnt1 + loopCnt2) >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}

	/* 種別が値の場合  */
	if( valueType ==  PV_JSON_VALUE_TYPE_NUM )
	{
		/* 値の文字列をコピー */
		memset(numbStr, 0, sizeof(numbStr));
		memcpy(numbStr, pc_dataptr+loopCnt1, loopCnt2);

		/* 数値チェック */
		if( func_numb_format_check(numbStr) != 0 )
		{
			sprintf(errMsg, "[%s] is not number.", numbStr);
			commfunc_syslog_msgset( "commfunc_json_parse_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
			return PV_JSON_ERR_FORMAT;
		}
	}
	
	

	/* タグ名取得を設定 */
	memcpy(pc_dataBuff, pc_dataptr+loopCnt1, loopCnt2);

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("value found : %s\n", pc_dataBuff);
#endif

	/* 文字列の場合 */
	if( valueType ==  PV_JSON_VALUE_TYPE_STR )
	{
		/* 読み込みデータサイズを設定(文字列の場合は、終わりの"を含める) */
		*pl_readCnt = loopCnt1 + loopCnt2 +1;
	}
	else
	{
		/* 読み込みデータサイズを設定 */
		*pl_readCnt = loopCnt1 + loopCnt2;
	}

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("commfunc_json_parse_value() : End : readCnt=%d\n", *pl_readCnt);
#endif
	return PV_JSON_RET_OK;

}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解析処理
 *
 * 機能：
 *		値を検索する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_parse_array_value( char *pc_dataptr, int dataSize, char *pc_dataBuff, int *pl_type, int *pl_readCnt, int *pl_valueType, char *pc_errMsg )
{
	int			loopCnt1;
	int			loopCnt2;
	int			valueType;
	char		numbStr[PV_JSON_DATALEN_MAX];
	int			retCode;

#ifdef PV_DEBUG_DATA_ANALIZE
	memset(errMsg, 0, sizeof(errMsg));
	memcpy(errMsg, pc_dataptr, PV_JSON_DATALEN_MAX);
	printf("commfunc_json_parse_array_value() : Start : data=%s\n", errMsg);
#endif
	
	/* 値の開始となる文字を検索 */
	for( loopCnt1 = 0; loopCnt1 < dataSize; loopCnt1++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 ));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json Value character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}
		/* 改行文字列を検出した場合 */
		else if( retCode > 0 )
		{
			/* 対象文字をスキップする */
			loopCnt1+=retCode;
			continue;
		}

		/* " を検出 */
		if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_DOUBLE_Q )
		{
			valueType = PV_JSON_VALUE_TYPE_STR;
			/* "の後から検索する為、インクリメント */
			loopCnt1++;
#ifdef PV_DEBUG_DATA_ANALIZE
			sprintf(errMsg, "value type is String.");
			printf("%s\n", errMsg);
#endif
			*pl_type = PV_JSON_TYPE_VALUE;
			break;
		}
		/* { を検出 */
		else if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_BRACE_S )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_TREE_S;
			sprintf(errMsg, "found '{'. tag start.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* ] を検出 */
		else if( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_SQUARE_E )
		{
			*pl_readCnt = loopCnt1+1;
			*pl_type = PV_JSON_TYPE_ARRAY_E;
			sprintf(errMsg, "found ']'. tag start.");
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			return PV_JSON_RET_OK;
		}
		/* 数字を検出 */
		else if( ( ( *( pc_dataptr + loopCnt1 ) >= 0x30 ) &&
				   ( *( pc_dataptr + loopCnt1 ) <= 0x39 ) ) ||
				 ( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_MINUS )  ||
				 ( *( pc_dataptr + loopCnt1 ) == PV_JSON_CODE_PLUS ) )
		{
			valueType = PV_JSON_VALUE_TYPE_NUM;
			*pl_type = PV_JSON_TYPE_VALUE;
#ifdef PV_DEBUG_DATA_ANALIZE
			sprintf(errMsg, "value type is Number.");
			printf("%s\n", errMsg);
#endif
			break;
		}
		else if( ( *( pc_dataptr + loopCnt1 ) == 't' ) ||
				 ( *( pc_dataptr + loopCnt1 ) == 'n' ) ||
				 ( *( pc_dataptr + loopCnt1 ) == 'f' ))
		{
			valueType = PV_JSON_VALUE_TYPE_BOOL;
			*pl_type = PV_JSON_TYPE_VALUE;
#ifdef PV_DEBUG_DATA_ANALIZE
			sprintf(errMsg, "value type is bool[%s].", ( pc_dataptr + loopCnt1 ));
			printf("%s\n", errMsg);
#endif
			break;
		}
		/* 空白 \t 以外はフォーマット異常 */
		else if( ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_SPACE ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_LF ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_CR ) &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_COMMA )  &&
				 ( *( pc_dataptr + loopCnt1 ) != PV_JSON_CODE_TAB ))
		{
			sprintf(errMsg, "\"Value\" start character Error('%c').", *( pc_dataptr + loopCnt1 ));
			commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
			printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//			return PV_JSON_ERR_VALUE;
			return PV_JSON_ERR_FORMAT;
		}
	}

	/* 値種別を設定 */
	*pl_valueType = valueType;

	/* ファイル終端の場合 */
	if( loopCnt1 >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}


	/* データ長上限の間に"または,}を検索する */
	for( loopCnt2 = 0; loopCnt2 < PV_JSON_DATALEN_MAX; loopCnt2++ )
	{
		/* 改行文字列チェック */
		retCode = func_returnStrCheck(( pc_dataptr + loopCnt1 + loopCnt2));
		/* 改行文字列が異常の場合 */
		if( retCode < 0 )
		{
			sprintf(errMsg, "json Value character is Not returnCode '('%c%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ), *( pc_dataptr + loopCnt1 + loopCnt2 +1));
			commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
			return PV_JSON_ERR_FORMAT;
		}

		/* 文字列の場合 */
		if( valueType ==  PV_JSON_VALUE_TYPE_STR )
		{
			/* 改行文字列を検出した場合 */
			if( retCode > 0 )
			{
				sprintf(errMsg, "json Value character is returnStr NG. '('%c%c').", *( pc_dataptr + loopCnt1 + loopCnt2 ), *( pc_dataptr + loopCnt1 + loopCnt2 +1));
				commfunc_syslog_msgset( "commfunc_json_parse_start()", errMsg );
				return PV_JSON_ERR_FORMAT;
			}

			if( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_DOUBLE_Q )
			{
				break;
			}
			/* 改行 \t の場合は、フォーマット異常 */
			else if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ) ||
					 ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
					 ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) )
			{
				sprintf(errMsg, "\"Value\" character is illegal('%c').", *( pc_dataptr + loopCnt1 ));
				commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//				return PV_JSON_ERR_VALUE;
				return PV_JSON_ERR_FORMAT;
			}
		}
		/* bool型の場合 */
		else if( valueType == PV_JSON_VALUE_TYPE_BOOL )
		{
			/* 文字列の先頭の場合 */
			if( loopCnt2 == 0 )
			{
				if( strncmp( "true", (pc_dataptr + loopCnt1), 4 ) == 0 )
				{
					loopCnt2 += 3;
				}
				else if( strncmp( "null", (pc_dataptr + loopCnt1), 4 ) == 0 )
				{
					loopCnt2 += 3;
				}
				else if( strncmp( "false", (pc_dataptr + loopCnt1), 5 ) == 0 )
				{
					loopCnt2 += 4;
				}
				else
				{
					sprintf(errMsg, "\"Bool\" character is illegal('%s').", *( pc_dataptr + loopCnt1 ));
					commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
					printf("%s\n", errMsg);
#endif
					memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
					return PV_JSON_ERR_FORMAT;
				}
			}
			/* 改行文字列を検出した場合 */
			else if( retCode > 0 )
			{
				/* 対象文字をスキップし、終了と判断する*/
				loopCnt2+=retCode;
				break;
			}
			/* 空白 , } ] : \t を検出した場合、数値の終了と判断する */
			else if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_COMMA ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_SPACE ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_BRACE_E ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_SQUARE_E ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
				( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ))
			{
				break;
			}
			/* 上記以外の場合はフォーマット異常 */
			else
			{
				sprintf(errMsg, "\"Bool\" character is illegal('%s').", *( pc_dataptr + loopCnt1 ));
				commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
				return PV_JSON_ERR_FORMAT;
			}
		}
		/* 数値の場合 */
		else if( valueType ==  PV_JSON_VALUE_TYPE_NUM )
		{
			/* 改行文字列を検出した場合 */
			if( retCode > 0 )
			{
				/* 対象文字をスキップし、終了と判断する*/
				loopCnt2+=retCode;
				break;
			}
			/* 空白 , } ] : \t を検出した場合、数値の終了と判断する */
			else if( ( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_COMMA ) ||
					( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_SPACE ) ||
					( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_BRACE_E ) ||
					( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_SQUARE_E ) ||
					( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_TAB ) ||
					( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_CR ) ||
					( *( pc_dataptr + loopCnt1 + loopCnt2) == PV_JSON_CODE_LF ))
			{
				break;
			}
			/* 数字、. 以外の場合はフォーマット異常 */
			else if( (( *( pc_dataptr + loopCnt1 + loopCnt2 ) < 0x30 ) ||
					  ( *( pc_dataptr + loopCnt1 + loopCnt2 ) > 0x39 )) &&
					 ( *( pc_dataptr + loopCnt1 + loopCnt2 ) != PV_JSON_CODE_MINUS )  &&
					 ( *( pc_dataptr + loopCnt1 + loopCnt2 ) != PV_JSON_CODE_PLUS ) &&
					 ( *( pc_dataptr + loopCnt1 + loopCnt2 ) != PV_JSON_CODE_DECIMAL_POINT ) )
			{
				sprintf(errMsg, "\"Value\" is not nummber character('%c'(0x%02X)).", *( pc_dataptr + loopCnt1 ), *( pc_dataptr + loopCnt1 ));
				commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
				printf("%s\n", errMsg);
#endif
				memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//				return PV_JSON_ERR_VALUE;
				return PV_JSON_ERR_FORMAT;
			}
			
		}
	}

	/* 値長の上限を超えている場合 */
	if( loopCnt2 >= PV_JSON_DATALEN_MAX )
	{
		sprintf(errMsg, "\"Value\" end character is not found.");
		commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_LENGTH;
	}
	/* データがない場合 */
	else if( loopCnt2 == 0 )
	{
		sprintf(errMsg, "\"Value\" string is no data.");
		commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
//		return PV_JSON_ERR_VALUE;
		return PV_JSON_ERR_FORMAT;
	}
	/* データサイズを超えている場合 */
	else if( (loopCnt1 + loopCnt2) >= dataSize )
	{
		sprintf(errMsg, "data end.");
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
		memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
		return PV_JSON_ERR_DATAEND;
	}

	/* 種別が値の場合  */
	if( valueType ==  PV_JSON_VALUE_TYPE_NUM )
	{
		/* 値の文字列をコピー */
		memset(numbStr, 0, sizeof(numbStr));
		memcpy(numbStr, pc_dataptr+loopCnt1, loopCnt2);

		/* 数値チェック */
		if( func_numb_format_check(numbStr) != 0 )
		{
			sprintf(errMsg, "[%s] is not number.", numbStr);
			commfunc_syslog_msgset( "commfunc_json_parse_array_value()", errMsg );
#ifdef PV_DEBUG_DATA_ANALIZE
		printf("%s\n", errMsg);
#endif
			memcpy(pc_errMsg, errMsg, PV_MSG_SIZE_LOG);
			return PV_JSON_ERR_FORMAT;
		}
	}
	
	

	/* タグ名取得を設定 */
	memcpy(pc_dataBuff, pc_dataptr+loopCnt1, loopCnt2);

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("value found : %s\n", pc_dataBuff);
#endif

	/* 文字列の場合 */
	if( valueType ==  PV_JSON_VALUE_TYPE_STR )
	{
		/* 読み込みデータサイズを設定(文字列の場合は、終わりの"を含める) */
		*pl_readCnt = loopCnt1 + loopCnt2 +1;
	}
	else
	{
		/* 読み込みデータサイズを設定 */
		*pl_readCnt = loopCnt1 + loopCnt2;
	}

#ifdef PV_DEBUG_DATA_ANALIZE
	printf("commfunc_json_parse_array_value() : End : readCnt=%d\n", *pl_readCnt);
#endif
	return PV_JSON_RET_OK;

}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ解放処理
 *
 * 機能：
 *		jsonデータを解放する
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_json_free( struct st_pvJsonParse *ps_jsonParse )
{
	/* 動的メモリが残っている場合 */
	if( ps_jsonParse->jsonDataPtr != NULL )
	{
		/* 動的メモリを解放する */
		free( ps_jsonParse->jsonDataPtr );
		ps_jsonParse->jsonDataPtr = NULL;
	}

	/* 管理情報を初期化する */
	memset(ps_jsonParse, 0, sizeof(struct st_pvJsonParse));

	return 0;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ検索処理
 *
 * 機能：
 *		解析したjsonデータに対して、引数(tagString)に該当するデータをvalueに格納する
 * 戻り値：
 * 	   >=1：該当データ数(最後に一致したタグの値を格納)
 * 		 0：該当データなし
 * 		-1：異常終了
 */
int	commfunc_json_getValue( struct st_pvJsonParse *ps_jsonParse, int *tagCount, char *tagString, char *value )
{
	struct st_tagStr		tagInfo;
	int						treeNum;
	int						loopCnt1;
	int						loopCnt2;
	int						retCode = PV_JSON_RET_NG;
	int						tag_count=0;

	/* 動的メモリが残っていない場合 */
	if( ps_jsonParse->jsonDataPtr == NULL )
	{
		return retCode;
	}

#ifdef PV_DEBUG_VALUE_SEARCH
	printf("commfunc_json_getValue() : Start : %s\n", tagString);
#endif

	/* 初期化 */
	memset(&tagInfo, 0, sizeof(tagInfo));

	/* タグ情報を取得 */
	treeNum = func_json_seq_tag( tagString, &tagInfo );

	/* 異常終了 */
	if( treeNum <= 0 )
	{
		return retCode;
	}

	for( loopCnt1 = 0; loopCnt1 < ps_jsonParse->dataNum; loopCnt1++ )
	{
		/* 階層が同じ場合 */
		if( ps_jsonParse->dataInfo[loopCnt1].treeNum == ( treeNum -1 ) )
		{
			for( loopCnt2 = 0; loopCnt2 < ps_jsonParse->dataInfo[loopCnt1].treeNum; loopCnt2++ )
			{
#ifdef PV_DEBUG_VALUE_SEARCH
				printf("compare tree : %s ? %s\n", ps_jsonParse->dataInfo[loopCnt1].treeName[loopCnt2], tagInfo.data[loopCnt2]);
#endif
				/* 階層のタグ名のチェック */
				if( strcmp( ps_jsonParse->dataInfo[loopCnt1].treeName[loopCnt2], tagInfo.data[loopCnt2]) != 0 )
				{
					break;
				}
			}

			/* 全階層のタグ名が一致していない場合、次の値を検索 */
			if( loopCnt2 != ps_jsonParse->dataInfo[loopCnt1].treeNum )
			{
				continue;
			}

#ifdef PV_DEBUG_VALUE_SEARCH
			printf("compare tag : %s ? %s\n", ps_jsonParse->dataInfo[loopCnt1].tagName, tagInfo.data[treeNum-1]);
#endif
			/* タグ名のチェック */
			if( strcmp( ps_jsonParse->dataInfo[loopCnt1].tagName, tagInfo.data[treeNum-1] ) != 0 )
			{
				continue;
			}

			/* 値をコピー */
			sprintf(value, "%s", ps_jsonParse->dataInfo[loopCnt1].value);
			tag_count++;
			retCode = PV_JSON_RET_OK;
		}
	}

#ifdef PV_DEBUG_MINIMUM
	if( retCode == PV_JSON_RET_OK )
		printf("tagCount(%d), %s : %s\n", tag_count, tagString, value);
#endif

#ifdef PV_DEBUG_VALUE_SEARCH
	printf("commfunc_json_getValue() : End\n");
#endif

	/* タグ一致数を設定 */
	*tagCount = tag_count;

	return retCode;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ検索処理
 *
 * 機能：
 *		解析したjsonデータに対して、引数(tagString)に該当するデータをvalueに格納する
 * 戻り値：
 * 	   >=1：該当データ数(最後に一致したタグの値を格納)
 * 		 0：該当データなし
 * 		-1：異常終了
 */
int	commfunc_json_getValue2( struct st_pvJsonParse *ps_jsonParse, int startPos, int groupId, char *tagString, struct st_jsonParseRsult * ps_jsonParseRsult)
{
	struct st_tagStr				tagInfo;
	int								treeNum=0;
	int								loopCnt1;
	int								loopCnt2;
	int								retCode = PV_JSON_RET_NG;
	int								tag_count=0;
	int								tagNum=0;
	int								tagCount=0;
	int								targetId=0;
	char							tagCheckStr[256];
	struct st_pvJsonDataInfo		*ps_pvJsonDataInfo;
	int								found;

	/* 動的メモリが残っていない場合 */
	if( ps_jsonParse->jsonDataPtr == NULL )
	{
		return retCode;
	}

#ifdef PV_DEBUG_VALUE_SEARCH
	printf("commfunc_json_getValue2() : Start : start[%d], group[%d], tagStr[%s]\n", startPos, groupId, tagString);
#endif

	/* 初期化 */
	memset(&tagInfo, 0, sizeof(tagInfo));

	/* タグ情報を取得 */
	treeNum = func_json_seq_tag( tagString, &tagInfo );

	/* 異常終了 */
	if( treeNum <= 0 )
	{
		return retCode;
	}

	/* データ数分ループ */
	for( loopCnt1 = startPos; loopCnt1 < ps_jsonParse->dataNum; loopCnt1++ )
	{
		found = 0;
		ps_pvJsonDataInfo = &ps_jsonParse->dataInfo[loopCnt1];
		tagNum = ps_pvJsonDataInfo->tagNum;
		if( ps_pvJsonDataInfo->tagName != NULL )
		{
			tagNum++;
		}
		/* タグの階層が同じ場合 */
		if( tagNum == treeNum )
		{
			memset(tagCheckStr, 0, sizeof(tagCheckStr));
			/* 検索タグを生成 */
			for( loopCnt2 = 0; loopCnt2 < ps_pvJsonDataInfo->treeNum; loopCnt2++ )
			{
				if( ps_pvJsonDataInfo->treeName[loopCnt2] != NULL )
				{
					if( tagCheckStr[0] != 0 )
					{
						strcat(tagCheckStr, ".");
					}
					sprintf(&tagCheckStr[strlen(tagCheckStr)], "%s", ps_pvJsonDataInfo->treeName[loopCnt2]);
				}

				if( ps_pvJsonDataInfo->groupNumb[loopCnt2] > 0 )
				{
					targetId = ps_pvJsonDataInfo->groupNumb[loopCnt2];

					if( ( groupId != 0 ) &&
						( groupId == targetId ) )
					{
						found = targetId;
					}
				}
			}
			if( ps_pvJsonDataInfo->tagName != NULL )
			{
				if( tagCheckStr[0] != 0 )
				{
					strcat(tagCheckStr, ".");
				}
				sprintf(&tagCheckStr[strlen(tagCheckStr)], "%s", ps_pvJsonDataInfo->tagName);
			}
			

			/* 階層のタグ名のチェック */
			if( strcmp( tagCheckStr, tagString) == 0 )
			{
				if( ( groupId == 0 ) || ( found > 0 ) )
				{
					sprintf(ps_jsonParseRsult->value, "%s", ps_pvJsonDataInfo->value);
					ps_jsonParseRsult->dataPos = loopCnt1;
					ps_jsonParseRsult->groupId = found;
					ps_jsonParseRsult->type = ps_pvJsonDataInfo->dataType;
					retCode = PV_JSON_RET_OK;
					break;
				}
			}
		}
	}

#ifdef PV_DEBUG_MINIMUM
	if( retCode == PV_JSON_RET_OK )
	{
		printf("commfunc_json_getValue2() : End : [%d] : groupId(%d), Type(%d) : value=%s\n", ps_jsonParseRsult->dataPos, ps_jsonParseRsult->groupId, ps_jsonParseRsult->type, ps_jsonParseRsult->value);
	}
	else
	{
		printf("commfunc_json_getValue2() : End : not found\n");
	}
#endif

#ifdef PV_DEBUG_VALUE_SEARCH
	//printf("commfunc_json_getValue() : End\n");
#endif

	return retCode;
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータ検索処理
 *
 * 機能：
 *		解析したjsonデータに対して、引数(tagString)に該当するデータをvalueに格納する
 * 戻り値：
 * 	   >=1：該当データ数(最後に一致したタグの値を格納)
 * 		 0：該当データなし
 * 		-1：異常終了
 */
int	commfunc_json_search( struct st_pvJsonParse *ps_jsonParse, int startPos, int groupId, char *tagString, char *value, struct st_jsonParseRsult *ps_jsonParseRsult )
{
	struct st_tagStr				tagInfo;
	int								treeNum=0;
	int								loopCnt1;
	int								loopCnt2;
	int								retCode = PV_JSON_RET_NG;
	int								tag_count=0;
	int								tagNum=0;
	int								tagCount=0;
	int								targetId=0;
	char							tagCheckStr[256];
	struct st_pvJsonDataInfo		*ps_pvJsonDataInfo;
	int								found;

	/* 動的メモリが残っていない場合 */
	if( ps_jsonParse->jsonDataPtr == NULL )
	{
		return retCode;
	}


	/* 初期化 */
	memset(&tagInfo, 0, sizeof(tagInfo));

	/* タグ情報を取得 */
	treeNum = func_json_seq_tag( tagString, &tagInfo );

#ifdef PV_DEBUG_VALUE_SEARCH
	printf("commfunc_json_search() : Start : start[%d], group[%d], tagStr[%s], value[%s], treeNum[%d]\n", startPos, groupId, tagString, value, treeNum);
#endif

	/* 異常終了 */
	if( treeNum <= 0 )
	{
		return retCode;
	}

	/* データ数分ループ */
	for( loopCnt1 = startPos; loopCnt1 < ps_jsonParse->dataNum; loopCnt1++ )
	{
		found = 0;
		ps_pvJsonDataInfo = &ps_jsonParse->dataInfo[loopCnt1];
		tagNum = ps_pvJsonDataInfo->tagNum;

		{
			memset(tagCheckStr, 0, sizeof(tagCheckStr));
				
			/* 検索タグを生成 */
			for( tagCount=0, loopCnt2 = 0; loopCnt2 < ps_pvJsonDataInfo->treeNum; loopCnt2++ )
			{
				if( tagCount == treeNum )
				{
					break;
				}

				if( ps_pvJsonDataInfo->treeName[loopCnt2] != NULL )
				{
					if( tagCheckStr[0] != 0 )
					{
						strcat(tagCheckStr, ".");
					}
					sprintf(&tagCheckStr[strlen(tagCheckStr)], "%s", ps_pvJsonDataInfo->treeName[loopCnt2]);
					tagCount++;
				}

				if( ps_pvJsonDataInfo->groupNumb[loopCnt2] > 0 )
				{
					targetId = ps_pvJsonDataInfo->groupNumb[loopCnt2];

					if( ( groupId != 0 ) &&
						( groupId == targetId ) )
					{
						found = targetId;
					}
				}
			}

			if( ( tagCount < treeNum ) &&
				( ps_pvJsonDataInfo->tagName != NULL ) )
			{
				if( tagCheckStr[0] != 0 )
				{
					strcat(tagCheckStr, ".");
				}
				sprintf(&tagCheckStr[strlen(tagCheckStr)], "%s", ps_pvJsonDataInfo->tagName);
			}
			

#ifdef PV_DEBUG_VALUE_SEARCH
//		printf("commfunc_json_search() : comp check[%s]\n", tagCheckStr );
#endif

			/* 階層のタグ名のチェック */
			if( strcmp( tagCheckStr, tagString) == 0 )
			{
#ifdef PV_DEBUG_VALUE_SEARCH
//		printf("commfunc_json_search() : value[%s], found[%d]\n", (value == NULL ? "NULL" : value), found );
#endif
				if( ( ( value == NULL ) ||
					  ( ( value != NULL ) &&
					    ( strcmp( ps_pvJsonDataInfo->value, value) == 0 ) ) ) &&
				    ( ( ( groupId == 0 ) || 
				        ( found > 0 ) ) ))
				{
					sprintf(ps_jsonParseRsult->value, "%s", ps_pvJsonDataInfo->value);
					ps_jsonParseRsult->dataPos = loopCnt1;
					ps_jsonParseRsult->groupId = targetId;
					ps_jsonParseRsult->type = ps_pvJsonDataInfo->dataType;
					retCode = PV_JSON_RET_OK;
					break;
				}
			}
		}
	}

#ifdef PV_DEBUG_MINIMUM
	if( retCode == PV_JSON_RET_OK )
	{
		printf("commfunc_json_search() : End : [%d] groupId(%d), Type(%d) : value=%s\n", ps_jsonParseRsult->dataPos, ps_jsonParseRsult->groupId, ps_jsonParseRsult->type, ps_jsonParseRsult->value);
	}
	else
	{
		printf("commfunc_json_search() : End : not found\n");
	}
#endif

#ifdef PV_DEBUG_VALUE_SEARCH
	//printf("commfunc_json_getValue() : End\n");
#endif

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * タグ名分割処理
 *
 * 機能：
 * 		引数 pc_data で指定された文字列をドットで区切り、ps_tagStr に格納する。
 * 戻り値：
 * 	0以上：ドットで区切られたデー多数を返す。
 * 	-1   ：タグ階層上限越え
 */
int func_json_seq_tag( char *pc_data, struct st_tagStr *ps_tagStr )
{
	char			*pt;
	int				Cnt;
	int				idx1 = 0;
	int				idx2 = 0;

	memset( ps_tagStr, 0, sizeof(struct st_tagStr) );

	pt = pc_data;
	for(Cnt = 0 ; (Cnt < 4096) && ((*pt != 0) && (*pt != '\n') && (*pt != '\r') ) ; Cnt++,pt++)
	{
		if( *pt == '.' )
		{
			ps_tagStr->data[idx1][idx2] = 0;
			idx2 = 0;
			/* タグの階層が管理情報の上限を超えている場合 */
			if( ( idx1 + 1 ) >= PV_JSON_TREE_MAX )
			{
				return PV_JSON_RET_NG;
			}
			idx1++;
			continue;
		}
		ps_tagStr->data[idx1][idx2] = *pt;
		idx2++;
	}
	ps_tagStr->data[idx1][idx2] = 0;
	if(idx2) idx1++;

	return( idx1 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻文字列チェック
 *
 * 機能：
 * 		引数 pc_numb で指定された文字列が全て数値かどうかをチェックする
 * 戻り値：
 * 	 0：全て数値
 * 	-1：数値以外
 */

int func_date_numb_check( char *pc_numb )
{
	int		loopCnt;
	int		retCode = 0;

	for( loopCnt = 0; loopCnt < strlen(pc_numb); loopCnt++ )
	{
		if( ( *( pc_numb + loopCnt ) < 0x30 ) ||
			( *( pc_numb + loopCnt ) > 0x39 ) )
		{
			retCode = -1;
			break;
		}
	}

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻文字列データ変換
 *
 * 機能：
 * 		引数 pc_timeStr で指定された時刻文字列をtime_t形式の値に変換する。
 * 		※時刻文字列はISO8601に準拠した文字列に限る
 * 		※TimeZoneは考慮しない。
 * 		※時間の表記も0～23とする
 * 		<例>  "2019-01-21T01:30:00Z"
 * 		または"2019-01-21 01:30:00"
 * 		または"2019/01/21 01:30:00"
 * 戻り値：
 * 	 0：正常終了
 * 	-1：時刻データ不備
 */
int func_json_conv_strTotime_t( char *p_timeStr, time_t *p_timeSec )
{
	struct tm		timeData;				/* 時刻情報 */
	char			timeStr[64];			/* 時刻情報文字列 */

	memset(&timeData, 0, sizeof(struct tm));
	memcpy(timeStr, p_timeStr, sizeof(timeStr));

	timeStr[19]=0;
	timeData.tm_sec  = atoi(&timeStr[17]);
	timeStr[16]=0;
	timeData.tm_min  = atoi(&timeStr[14]);
	timeStr[13]=0;
	timeData.tm_hour = atoi(&timeStr[11]);
	timeStr[10]=0;
	timeData.tm_mday = atoi(&timeStr[8]);
	timeStr[7]=0;
	timeData.tm_mon  = atoi(&timeStr[5]);
	timeStr[4]=0;
	timeData.tm_year = atoi(&timeStr[0]);

	if( ( (timeData.tm_year < 1970) || (timeData.tm_year  > 2100)) ||
	    ( (timeData.tm_mon < 1) || (timeData.tm_mon  > 12)) ||
	    ( (timeData.tm_mday < 1) || (timeData.tm_mday  > func_json_get_days(timeData.tm_year, timeData.tm_mon))) ||
	    ( (timeData.tm_hour < 0) || ( timeData.tm_hour > 23)) ||
	    ( (timeData.tm_min < 0) || ( timeData.tm_min > 59)) ||
	    ( (timeData.tm_sec < 0) || ( timeData.tm_sec > 59)) )
	{
		return PV_JSON_RET_NG;
	}

	timeData.tm_year -= 1900;
	timeData.tm_mon -= 1;

    *p_timeSec = mktime(&timeData);

	return PV_JSON_RET_OK;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 月日数取得
 *
 * 機能：
 * 		引数 year, monthで指定した年月に対する日数を返却する
 * 戻り値：
 * 	日数
 */
int func_json_get_days( int year, int month )
{
	/*							1  2  3  4  5  6  7  8  9  10 11 12 */
	char		month_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};


    // 西暦が400で割り切れる年はうるう年である
    // 400で割り切れない場合、西暦が100で割り切れる年はうるう年ではない
    // 100で割り切れない場合、西暦が4で割り切れる年はうるう年である
    // 4で割り切れない場合、うるう年ではない

	/* 西暦が400で割り切れる */
	if((year % 400 ) == 0 )
	{
		/* うるう年 */
		month_days[1]++;
	}
	else
	{
		/* 西暦が100で割り切れる */
	    if((year % 100 ) == 0 )
	    {
			/* うるう年ではない */
			;
		}
		/* 西暦が4で割り切れる年 */
		else if( (year % 4 ) == 0 )
		{
			/* うるう年 */
			month_days[1]++;
		}
	}

	return month_days[month-1];
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 日時文字列チェック
 *
 * 機能：
 * 		引数 pc_dateStrで指定した日時の文字列が正しいかチェックする
 * 戻り値：
 * 	 0：正常終了
 * 	-1：日時文字列ではない
 */
int func_date_format_check( char *pc_dateStr )
{
	struct tm		timeData;				/* 時刻情報 */
	char			timeStr[64];			/* 時刻情報文字列 */

	memset(&timeData, 0, sizeof(struct tm));
	memcpy(timeStr, pc_dateStr, sizeof(timeStr));

	timeStr[19]=0;
	if( func_date_numb_check( &timeStr[17] ) != 0 )
	{
		return PV_JSON_RET_NG;
	}
	timeData.tm_sec  = atoi(&timeStr[17]);

	timeStr[16]=0;
	if( func_date_numb_check( &timeStr[14] ) != 0 )
	{
		return PV_JSON_RET_NG;
	}
	timeData.tm_min  = atoi(&timeStr[14]);

	timeStr[13]=0;
	if( func_date_numb_check( &timeStr[11] ) != 0 )
	{
		return PV_JSON_RET_NG;
	}
	timeData.tm_hour = atoi(&timeStr[11]);

	timeStr[10]=0;
	if( func_date_numb_check( &timeStr[8] ) != 0 )
	{
		return PV_JSON_RET_NG;
	}
	timeData.tm_mday = atoi(&timeStr[8]);

	timeStr[7]=0;
	if( func_date_numb_check( &timeStr[5] ) != 0 )
	{
		return PV_JSON_RET_NG;
	}
	timeData.tm_mon  = atoi(&timeStr[5]);

	timeStr[4]=0;
	if( func_date_numb_check( &timeStr[0] ) != 0 )
	{
		return PV_JSON_RET_NG;
	}
	timeData.tm_year = atoi(&timeStr[0]);

	if( ( (timeData.tm_year < 1970) || (timeData.tm_year  > 2100)) ||
	    ( (timeData.tm_mon < 1) || (timeData.tm_mon  > 12)) ||
	    ( (timeData.tm_mday < 1) || (timeData.tm_mday  > func_json_get_days(timeData.tm_year, timeData.tm_mon))) ||
	    ( (timeData.tm_hour < 0) || ( timeData.tm_hour > 23)) ||
	    ( (timeData.tm_min < 0) || ( timeData.tm_min > 59)) ||
	    ( (timeData.tm_sec < 0) || ( timeData.tm_sec > 59)) )
	{
		return PV_JSON_RET_NG;
	}

	return PV_JSON_RET_OK;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 数値文字列チェック
 *
 * 機能：
 * 		引数 pc_nmbStrで指定した数値文字列が正しいかチェックする
 * 戻り値：
 * 	 0：正常終了
 * 	-1：数値以外
 */
int func_numb_format_check( char *pc_numbStr )
{
	int			loopCnt1;
	int			numbCnt=0;
	int			plusCnt=0;
	int			minusCnt=0;
	int			decpCnt=0;
	int			retCode = PV_JSON_RET_OK;

	for( loopCnt1 = 0; loopCnt1 < strlen(pc_numbStr); loopCnt1++ )
	{
		/* 数字の場合 */
		if( ( *( pc_numbStr + loopCnt1 ) >= 0x30 ) &&
			( *( pc_numbStr + loopCnt1 ) <= 0x39 ) )
		{
			numbCnt++;
		}
		/* -の場合 */
		else if( *( pc_numbStr + loopCnt1 ) == PV_JSON_CODE_MINUS )
		{
			/* 数字がでている場合、または、- + がでている場合 */
			if( ( numbCnt != 0 ) ||
				( plusCnt != 0 ) ||
				( minusCnt != 0 ))
			{
				retCode = PV_JSON_RET_NG;
				break;
			}
			minusCnt++;
		}
		/* +の場合 */
		else if( *( pc_numbStr + loopCnt1 ) == PV_JSON_CODE_PLUS )
		{
			/* 数字がでている場合、または、- + がでている場合 */
			if( ( numbCnt != 0 ) ||
				( plusCnt != 0 ) ||
				( minusCnt != 0 ))
			{
				retCode = PV_JSON_RET_NG;
				break;
			}
			plusCnt++;
		}
		/* .の場合 */
		else if( *( pc_numbStr + loopCnt1 ) == PV_JSON_CODE_DECIMAL_POINT )
		{
			/* 先頭文字、または、.がでている場合 */
			if( ( numbCnt == 0 ) ||
				( decpCnt != 0 ))
			{
				retCode = PV_JSON_RET_NG;
				break;
			}
			decpCnt++;
		}
		/* 空白以外の場合 */
		else if( *( pc_numbStr + loopCnt1 ) != PV_JSON_CODE_SPACE )
		{
			retCode = PV_JSON_RET_NG;
			break;
		}
	}

	/* 数字がない場合 */
	if( numbCnt == 0 )
	{
		retCode = PV_JSON_RET_NG;
	}

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * JSON 改行文字列チェック
 *
 * 機能：
 *		
 *
 * 入力：
 *
 * 出力：
 *
 */
int func_returnStrCheck( char *p_char )
{

	// '\'が見つかった場合
	if( *p_char == PV_JSON_CODE_YEN )
	{
		if( ( *(p_char+1) == PV_JSON_CODE_n ) ||
			( *(p_char+1) == PV_JSON_CODE_r ) )
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return 0;
	}
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * JSONログ出力処理
 *
 * 機能：
 *		
 *
 * 入力：
 *
 * 出力：
 *
 */
int commfunc_json_logout( char *pc_fileName, char *pc_payload )
{
	FILE					*fp;					/* ファイルポインタ */
	char					fileName1[512];			/* ファイル名 */
	char					fileName2[512];			/* ファイル名 */
	unsigned char			*pt1;					/* ポインタ */
	unsigned char			*pt2;					/* ポインタ */
	int						fd;						/* ファイルディスクリプタ */
	int						loopCnt1;				/* ループカウンタ */
	int						loopCnt2;				/* ループカウンタ */
	long					fileSize;				/* ファイルサイズ */
	struct tm				localTime;				/* 年月日時分秒 */
	struct timeval			nowTime;				/* 時刻情報 *//* [20240701] Chg */
	int						logMode = PV_LOG_OFF;	/* ログ出力 */

	/* グローバルアクセスファイル未オープンの場合 */
	if( ps_gpt_PVSYSINF == NULL )
	{
		printf("commfunc_json_logout(): Error PVSYSINF file not open. (fileName:%s)\n", pc_fileName );
		return( -1 );
	}

	if( strcmp( pc_fileName, PV_FNAME_AWSRCV_LOG ) == 0 )
	{
/* [20230902] Chg St */
		//logMode = ps_gpt_PVSYSINF->awsRcvLogFlag;
		logMode = PV_LOG_ON;
/* [20230902] Chg Ed */
	}
	else if( strcmp( pc_fileName, PV_FNAME_IOTCMN_LOG ) == 0 )
	{
		logMode = ps_gpt_PVSYSINF->iotSndLogFlag;
	}

	if( logMode == PV_LOG_ON )
	{
		/* 現在時刻を取得 */
		gettimeofday( &nowTime, NULL );					/* [20240701] Chg */

		/* 年月日を取得 */
		localtime_r( &nowTime.tv_sec, &localTime );		/* [20240701] Chg */

		/* 出力用ログファイル名の設定 */
		sprintf( fileName1, "%s%s.log", PV_PATH_LOG, pc_fileName );

		/* ファイルが存在しない場合 */
		if( access( fileName1, F_OK ) != 0 )
		{
			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH )) == -1 )
			{
				return( -1 );
			} 
			close( fd );
		}

		/* ログファイルをオープン */
		if( (fp = fopen( fileName1, "a" )) == NULL )
		{
			return( -1 );
		}

		/* ファイルサイズの取得 */
		fileSize = ftell( fp );

		/* ファイルサイズが１ＭＢｙｔｅを超過した場合 */
		if(fileSize > 1000000 )
		{ 
			/* ファイルを閉じる */
			fclose( fp );

			/* ログファイルの保存数分繰り返す */
			for(loopCnt1 = PV_LOG_STOCK_NUM ; loopCnt1 > 1 ; loopCnt1-- )	/* [20230902] */
			{
				/* ファイル名の設定 */
				sprintf( fileName1, "%s%s_%d.log", PV_PATH_LOG, pc_fileName, loopCnt1 - 1 );
				sprintf( fileName2, "%s%s_%d.log", PV_PATH_LOG, pc_fileName, loopCnt1 );
				/* 古いファイルの削除 */
				if( access( fileName2, F_OK ) == 0 )
					unlink( fileName2 );
				/* ローテーションの実施 */
				if( access( fileName1, F_OK ) == 0 )
					rename( fileName1, fileName2 );
			}
			/* ファイル名の設定 */
			sprintf( fileName1, "%s%s.log", PV_PATH_LOG, pc_fileName );
			sprintf( fileName2, "%s%s_1.log", PV_PATH_LOG, pc_fileName );
			/* 古いファイルの削除 */
			if( access( fileName2, F_OK ) == 0 )
				unlink( fileName2 );
			/* ローテーションの実施 */
			rename( fileName1, fileName2 );

			/* ファイルを生成 */
			if( (fd = open( fileName1, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH ) ) == -1 )
			{
				return( -1 );
			} 
			close( fd );

			/* ログファイルをオープン */
			if( (fp = fopen( fileName1, "a" )) == NULL )
			{
				return( -1 );
			}
		}

		/* メッセージ出力処理 *//* [20240701] Chg */
		fprintf( fp, "===== AWS SUBSCRIBE START [ %04d/%02d/%02d/ %02d:%02d:%02d.%06d ] =====\n",
			localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
			localTime.tm_hour, localTime.tm_min, localTime.tm_sec, nowTime.tv_usec );
		fprintf( fp, "%s\n", pc_payload );

		/* ファイルを閉じる */
		fclose( fp );
	}

	return( 0 );
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * jsonデータログ出力
 *
 * 機能：
 *		jsonデータをSYSログに出力する
 * 戻り値：
 * 		 0：正常処理
 */
int	commfunc_json_parse_logout( struct st_pvJsonParse *ps_jsonParse )
{
	int		retCode=0;				/* 処理結果 */
	int		loopCnt1;				/* ループカウンタ */
	int		loopCnt2;				/* ループカウンタ */

	/* 動的メモリが残っている場合 */
	if( ps_jsonParse->jsonDataPtr != NULL )
	{
		for( loopCnt1 = 0; loopCnt1 < ps_jsonParse->dataNum; loopCnt1++ )
		{
			memset(errMsg, 0, sizeof(errMsg));
			for( loopCnt2 = 0; loopCnt2 < ps_jsonParse->dataInfo[loopCnt1].treeNum; loopCnt2++ )
			{
				sprintf(&errMsg[strlen(errMsg)], "%s.", ps_jsonParse->dataInfo[loopCnt1].treeName[loopCnt2]);
			}
			sprintf(&errMsg[strlen(errMsg)], "%s=%s", ps_jsonParse->dataInfo[loopCnt1].tagName, ps_jsonParse->dataInfo[loopCnt1].value);
			commfunc_syslog_msgset( "commfunc_json_parse()", errMsg );
		}
	}

	return retCode;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 時刻文字列データ変換
 *
 * 機能：
 * 		引数 pc_timeStr で指定された時刻文字列をtimeval形式の値に変換する。
 * 		※時刻文字列はISO8601に準拠した文字列に限る
 * 		※TimeZoneは考慮しない。
 * 		※時間の表記も0～23とする
 * 		<例>  "2019-01-21T01:30:00"
 * 		または"2019-01-21 01:30:00.0"
 * 		または"2019/01/21 01:30:00.5"
 * 戻り値：
 * 	 0：正常終了
 * 	-1：時刻データ不備
 */
int func_json_conv_strTotimeval( char *p_timeStr, struct timeval *p_timeval )
{
	struct tm		timeData;				/* 時刻情報 */
	char			timeStr[64];			/* 時刻情報文字列 */
	suseconds_t		uSec;					/* マイクロ秒 */

	memset(&timeData, 0, sizeof(struct tm));
	memcpy(timeStr, p_timeStr, sizeof(timeStr));
	//012345678901234567890
	//YYYY-MM-DD hh:mm:ss.u
	timeStr[21]=0;
	uSec  = atoi(&timeStr[20]);
	timeStr[19]=0;
	timeData.tm_sec  = atoi(&timeStr[17]);
	timeStr[16]=0;
	timeData.tm_min  = atoi(&timeStr[14]);
	timeStr[13]=0;
	timeData.tm_hour = atoi(&timeStr[11]);
	timeStr[10]=0;
	timeData.tm_mday = atoi(&timeStr[8]);
	timeStr[7]=0;
	timeData.tm_mon  = atoi(&timeStr[5]);
	timeStr[4]=0;
	timeData.tm_year = atoi(&timeStr[0]);

	if( ( (timeData.tm_year < 1970) || (timeData.tm_year  > 2100)) ||
	    ( (timeData.tm_mon < 1) || (timeData.tm_mon  > 12)) ||
	    ( (timeData.tm_mday < 1) || (timeData.tm_mday  > func_json_get_days(timeData.tm_year, timeData.tm_mon))) ||
	    ( (timeData.tm_hour < 0) || ( timeData.tm_hour > 23)) ||
	    ( (timeData.tm_min < 0) || ( timeData.tm_min > 59)) ||
	    ( (timeData.tm_sec < 0) || ( timeData.tm_sec > 59)) ||
		( (uSec != 0) && ( uSec != 5)) )
	{
		return PV_JSON_RET_NG;
	}

	timeData.tm_year -= 1900;
	timeData.tm_mon -= 1;

	p_timeval->tv_sec = mktime(&timeData);
	p_timeval->tv_usec = uSec * 100000;

	return PV_JSON_RET_OK;
}

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 数値文字列チェック
 *
 * 機能：
 * 		引数 pc_nmbStrで指定した数値文字列が正しいかチェックする
 * 戻り値：
 * 	 0：正常終了
 * 	-1：数値以外
 */
int func_numb_conv( char *pc_numbStr, struct timeval *p_timeval)
{
	int			loopCnt1;
	int			retCode = PV_JSON_RET_OK;

	memset(p_timeval, 0, sizeof(struct timeval));

	for( loopCnt1 = 0; loopCnt1 < strlen(pc_numbStr); loopCnt1++ )
	{
		/* .の場合 */
		if( pc_numbStr[loopCnt1] == PV_JSON_CODE_DECIMAL_POINT )
		{
			pc_numbStr[loopCnt1] = 0;
			/* 小数第一位のみ有効なので、小数第二はクリアする */
			pc_numbStr[loopCnt1+2] = 0;
			p_timeval->tv_usec = atoi(&pc_numbStr[loopCnt1+1]) * 100000;
			break;
		}
	}
	p_timeval->tv_sec = atoi(pc_numbStr);

	return retCode;
}

