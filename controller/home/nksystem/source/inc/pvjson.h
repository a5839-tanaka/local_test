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
 * バーチャルプラントシステム
 * -----------------------------------------------------------------------------
 * デバイス処理用データ関連定義
 *
 */


/*
 * json解析関連情報
 */
#define	PV_JSON_TYPE_TAG		0				/* キー種別(タグ) */
#define	PV_JSON_TYPE_TREE_S		1				/* キー種別(ツリー開始) */
#define	PV_JSON_TYPE_TREE_E		2				/* キー種別(ツリー終了) */
#define	PV_JSON_TYPE_VALUE		3				/* キー種別(値) */
/* [20210813] Add St */
#define	PV_JSON_TYPE_ARRAY_S	4				/* キー種別(配列開始) */
#define	PV_JSON_TYPE_ARRAY_E	5				/* キー種別(配列終了) */
/* [20210813] Add Ed */

#define	PV_JSON_VALUE_TYPE_NUM		1			/* メンバ(値) */
#define	PV_JSON_VALUE_TYPE_STR		2			/* メンバ(文字列) */
#define	PV_JSON_VALUE_TYPE_BOOL		3			/* メンバ(true/false) */

/*
 * json解析結果
 */
#define	PV_JSON_RET_OK			0				/* 正常終了 */
#define	PV_JSON_RET_NG			-1				/* 異常終了 */

#define	PV_JSON_ERR_FORMAT		-1				/* フォーマット異常 */
#define	PV_JSON_ERR_UNSUPPORTED	-2				/* 非サポート */
#define	PV_JSON_ERR_DATAEND		-3				/* データ終端 */
#define	PV_JSON_ERR_LENGTH		-4				/* データ長オーバー */
#define	PV_JSON_ERR_TREEOVER	-5				/* ツリー上限越え */
#define	PV_JSON_ERR_DATAOVER	-6				/* データ上限越え */
#define	PV_JSON_ERR_PARAM		-7				/* パラメータ異常 */
#define	PV_JSON_ERR_BUSY		-8				/* 処理中 */

/*
 * jsonデータ管理用
 */

/* jsonデータ情報 */
struct st_pvJsonDataInfo
{
	int									treeNum;						/* ツリー階層数 */
	int									tagNum;							/* タグ数 */
	int									dataType;						/* 1:数値,2:文字列,3:true/false */
	int									yobi;							/* 予備 */
	int 								arrayNumb[PV_JSON_TREE_MAX];	/* ツリー名称ポインタ */
	int 								groupNumb[PV_JSON_TREE_MAX];	/* ツリー名称ポインタ */
	char 								*treeName[PV_JSON_TREE_MAX];	/* ツリー名称ポインタ */
	char								*tagName;						/* タグ名称ポインタ */
	char								*value;							/* 値ポインタ */
};


/* jsonデータ解析情報 */
struct st_pvJsonParse
{
	int									dataNum;						/* jsonデータ数 */
	int									dummy1;
	struct st_pvJsonDataInfo			dataInfo[PV_JSON_DATA_MAX];		/* データ情報 */
	char								*jsonDataPtr;					/* データポインタ */
	char								dummy2[4];
};


/* jsonデータ解析結果 */
struct st_jsonParseRsult
{
	int									dataPos;						/* jsonデータ位置 */
	int									groupId;						/* グループID */
	int									type;							/* データ種別 */
	char								value[128];						/* 値(文字列 */
};


/* タグ名分割用構造体 */
struct st_tagStr {
	char								data[PV_JSON_TREE_MAX][PV_JSON_DATALEN_MAX];	/* 100データ（1データあたり128バイト） */
};

/* 関数定義 */
int	commfunc_json_parse(struct st_pvJsonParse *, int, int, char *, char * );
int	commfunc_json_parse_sub(struct st_pvJsonParse *, int, char *, char * );
int	commfunc_json_free(struct st_pvJsonParse *);
int	commfunc_json_data_get(struct st_pvJsonParse *, char *);
int	commfunc_json_parse_start( char *, int , char *, int *, char * );
int	commfunc_json_parse_tag( char *, int , char *, int *, int *, char * );
int	commfunc_json_parse_array_value( char *, int , char *, int *, int *, int *, char * );
int	commfunc_json_parse_value( char *, int , char *, int *, int *, int *, char * );
int	commfunc_json_getValue( struct st_pvJsonParse *, int *, char *, char * );
int	commfunc_json_getValue2( struct st_pvJsonParse *, int, int, char *, struct st_jsonParseRsult *);
int	commfunc_json_search( struct st_pvJsonParse *, int , int, char *, char *, struct st_jsonParseRsult *);
int commfunc_json_logout( char *, char * );
int	commfunc_json_parse_logout( struct st_pvJsonParse * );
int func_json_conv_strTotimeval( char *, struct timeval * );
int func_date_format_check( char * );
int func_numb_conv( char *, struct timeval *);
