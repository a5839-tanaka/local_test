/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * 改修履歴
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
0        1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
/*
 * MicroGridシステム
 * -----------------------------------------------------------------------------
 * ミューテックス状態確認ツール
 *
 * 収集装置内のプロセス間で、ファイル更新の排他制御に使用しているミューテックス
 * の状態を定期的に表示する
 * （初期値は５秒周期とし、＋／－キーで１～１０秒の範囲で設定可能とする）
 * [q]キーで終了
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <termio.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define PV_MAIN_MODULE

#include "pvdefine.h"
#include "pvfile.h"

int main()
{
	int					shmid;						/* 共有メモリＩＤ */
	int					loopCnt;					/* ループカウンタ */
	int					retCode = 0;				/* システムコール戻り値 */
	int					loopTime = 50;				/* 入力待ち時間 */
	char				inKeyData;					/* 入力内容 */
	char				readByte;					/* 入力バイト数 */
	time_t				nowTime;					/* 現在時刻 */
	struct tm			localTime;					/* 現在時刻 */
	struct termio		ttyBackup;					/* 端末設定保存用 */
	struct termio		ttyChange;					/* 端末設定変更用 */
	pthread_mutex_t		*mutex;						/* ミューテックス先頭ポインタ */

	/* 共有メモリ（ミューテックス用領域）のＩＤを取得 */
	shmid = shmget( PV_SHMKEY_MUTEX, PV_SHMSIZ_MUTEX, 0 );
	if( shmid == -1 )
	{
		printf("Error: (shmget error) 共有メモリのＩＤ取得に失敗しました (errno:%d)\n", errno );
		return( -1 );
	}

	/* 共有メモリを読込専用でアタッチ */
	mutex = (pthread_mutex_t *)shmat( shmid, NULL, SHM_RDONLY);
	if(mutex == (pthread_mutex_t *)-1)
	{
		printf("Error: (shmat error) 共有メモリのアタッチに失敗しました (errno:%d)\n", errno );
		return( -1 );
	}

	/* 現在の端末設定を保存 */
	ioctl( 0, TCGETA, &ttyBackup );
	/* 変更用設定にコピー */
	ttyChange = ttyBackup;

	/* ローカルエコーをやめる */
	ttyChange.c_lflag &= ~(ECHO | ICANON);

	while( 1 )
	{
		/* キー入力を５秒待つ */
		ttyChange.c_cc[VMIN] = 0;
		ttyChange.c_cc[VTIME] = loopTime;
		/* 設定変更を有効にする */
		ioctl( 0, TCSETAF, &ttyChange );

		retCode = printf("\n---------------------------------------------------------------------------\n");
		if( retCode < 0 ) break;
		nowTime = time( NULL );
		localtime_r( &nowTime, &localTime );
		printf("PV-SYSTEM MUTEX-STATUS LIST ( Shared Memory )         [%04d/%02d/%02d %02d:%02d:%02d]\n",
			localTime.tm_year + 1900,
			localTime.tm_mon + 1,
			localTime.tm_mday,
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec );
		printf("SHM-KEY:%d / SHM-ID:%d\n", PV_SHMKEY_MUTEX, shmid );
		for( loopCnt = 1 ; loopCnt <= PV_MAX_FILEINDEX ; loopCnt++ )
		{
			retCode = printf("File:%02d Mutex( Lock[%d] Count[%d] Owner[%5d] Kind[%3d] Nusers[%d] Spins[%d] )\n",
				loopCnt,
				(mutex + loopCnt - 1)->__data.__lock,
				(mutex + loopCnt - 1)->__data.__count,
				(mutex + loopCnt - 1)->__data.__owner,
				(mutex + loopCnt - 1)->__data.__kind,
				(mutex + loopCnt - 1)->__data.__nusers,
				(mutex + loopCnt - 1)->__data.__spins );
			if( retCode < 0 ) break;
		}
		printf("---------------------------------------------------------------------------\n");
		printf("Wait:%dsec (Wait:[-]/[+])   Exit:[q]", loopTime / 10);
		fflush( NULL );

		/* キー入力を待つ */
		readByte = read( 0, &inKeyData, 1 );

		/* 入力エラー */
		if( readByte < 0 ) break;

		if( readByte != 0 )
		{
			/* 終了キーの場合 */
			if( (inKeyData == 'q') || (inKeyData == 'Q') )
				break;
			if(inKeyData == '-')
			{
				if( loopTime >= 20 ) loopTime -= 10;
			}
			if(inKeyData == '+')
			{
				if( loopTime < 100 ) loopTime += 10;
			}
		}
	}

	/* 共有メモリをデタッチする */
	shmdt( mutex );

	/* 端末設定を元に戻す */
	ioctl( 0, TCSETAF, &ttyBackup );

	printf("\n");

	if( retCode < 0 )
		return( -1 );
	else
		return( 0 );
	
}


