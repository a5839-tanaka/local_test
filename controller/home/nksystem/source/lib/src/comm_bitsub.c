/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * 共通関数：ビットステータス関数
 *
 * 機能：
 * 		以下の機能を提供する。
 * 		・バイト配列の領域に対して、指定されたビット位置のデータを操作する
 */


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ビット状態設定
 *
 * 機能：
 *		引数 startAddr で指定されたバイト配列のアドレスから bitNumb 番目のビット
 *		状態を setStatus (0または1）にセットする
 *		先頭ビットは 0 として処理を行う
 * 戻り値：
 * 		 0：正常処理
 * 		-1：異常終了
 */
int	commfunc_bitset(unsigned char *startAddr, int bitNumb, int setStatus )
{
	int					offsetByte;				/* オフセット（バイト位置） */
	int					offsetBit;				/* オフセット（ビット位置） */
	unsigned char		bitMask;

	/* オフセット位置の取得 */
	offsetByte = bitNumb / 8;
	offsetBit  = bitNumb % 8;

	/* マスク情報の取得 */
	bitMask = (unsigned char)0x80 >> offsetBit;

	/* ビット状態の設定 */
	if( setStatus == 0 )
	{
		*(startAddr + offsetByte) &= ~bitMask;
	}
	else
	{
		*(startAddr + offsetByte) |= bitMask;
	}

	return( 0 );
}


/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * ビット状態取得
 *
 * 機能：
 *		引数 startAddr で指定されたバイト配列のアドレスから bitNumb 番目のビット
 *		状態を取得する
 *		先頭ビットは 0 として処理を行う
 * 戻り値：
 * 		 0：状態０
 * 		 1：状態１
 */
int	commfunc_bitget(unsigned char *startAddr, int bitNumb )
{
	int					offsetByte;				/* オフセット（バイト位置） */
	int					offsetBit;				/* オフセット（ビット位置） */
	unsigned char		bitMask;

	/* オフセット位置の取得 */
	offsetByte = bitNumb / 8;
	offsetBit  = bitNumb % 8;

	/* マスク情報の取得 */
	bitMask = (unsigned char)0x80 >> offsetBit;

	/* ビット状態の取得 */
	if( ( *(startAddr + offsetByte) & bitMask ) != 0 )
		return( 1 );
	else
		return( 0 );
}

