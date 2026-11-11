#!/bin/sh

# 統合コントローラのオンラインデータ初期化を行う
#
# 統合コントローラのアプリケーションを停止し、下記のディレクトリを初期化する。
# ・ONLDATA・・・全ファイルを削除する
# ・IOTDATA・・・全ファイルを削除する
# ・JSONDATA・・・全ファイルを削除する
# ・FTPDATA・・・全ファイルを削除する
# ・log・・・・・全ファイルを削除する
#
# CSVデータの初期化は行いません。


echo "============================================================================="
echo "ＮＫ－ＳＹＳＴＥＭ 統合コントローラ"
echo "-----------------------------------------------------------------------------"
echo "オンライン（状態）データ初期化ツール"
echo ""
echo "統合コントローラのアプリケーションを停止した後、下記のデータを初期化します。"
echo "・統合コントローラが保持している状態データ（ONLDATA 以下の *.dat ファイル）"
echo "・モニタリングサーバー宛の未送信ファイル（FTPDATA 以下の *.DAT ファイル）"
echo "・システムログファイル（log 以下の *.log ファイル"
echo ""
echo "参考：CSVDATA 以下の *.csv ファイルの内容は初期化しません"
echo "============================================================================="

if [ "zz${USER}" != "zzroot" ] ; then
	echo "ERROR: root ユーザ以外はこのツールを起動できません"
	exit
fi

echo "--------------------------"
echo "初期化処理を開始しますか？"
echo "--------------------------"
echo -n "( y or n ) >>> "
read WORKDATA
if [ "zzy" != "zz${WORKDATA}" ]
then
	echo "処理を中止します"
	exit
fi

# 現在のディレクトリの保存
OLDDIR=`pwd`


# プリケーションの停止
echo "・アプリケーション動作状態を確認しています"
CHECKWD=`pgrep -o nkmanage`
if [ -z ${CHECKWD} ] ; then
	echo "・アプリケーションは動作していません"
else
	LOOPCNT=0
	echo "・アプリケーションを停止しています"
	/usr/bin/touch /home/nksystem/ONLDATA/stop_flg
	/bin/chmod 666 /home/nksystem/ONLDATA/stop_flg
	while [ 1 ]
	do
		sleep 10
		test -e /home/nksystem/ONLDATA/stop_flg || break
		LOOPCNT=`expr ${LOOPCNT} + 1`
		test ${LOOPCNT} -lt 18 || break;
	done
	\rm -f /home/nksystem/ONLDATA/stop_flg > /dev/null 2>&1
fi

# オンラインデータの初期化
echo "・オンラインデータ領域を初期しています"
echo "  (/home/nksystem/ONLDATA/)"
\rm -f /home/nksystem/ONLDATA/* > /dev/null 2>&1

# FTP送信データの初期化
echo "・ＦＴＰ送信用データ格納領域を初期化しています"
echo "  (/home/nksystem/FTPDATA/)"
\rm -rf /home/nksystem/FTPDATA/* > /dev/null 2>&1

# IOT送信データの初期化
echo "・ＩＯＴ送信用データ格納領域を初期化しています"
echo "  (/home/nksystem/IOTDATA/)"
\rm -rf /home/nksystem/IOTDATA/* > /dev/null 2>&1
echo "  (/home/nksystem/JSONDATA/)"
\rm -rf /home/nksystem/JSONDATA/* > /dev/null 2>&1

# 100ms蓄積データの初期化
echo "・100ms蓄積データ格納領域を初期化しています"
echo "  (/home/nksystem/RECDATA/)"
\rm -rf /home/nksystem/RECDATA/* > /dev/null 2>&1

# ログ領域の初期化
echo "・ログ格納領域を初期化しています"
echo "  (/home/nksystem/log/)"
\rm -f /home/nksystem/log/* > /dev/null 2>&1


# SSH認証関連ファイルの削除
\rm /root/.ssh/known_hosts > /dev/null 2>&1
\rm /home/nksystem/.ssh/known_hosts > /dev/null 2>&1

echo ""
echo "============================================================================="
echo "データ送信ロックを設定しますか？"
echo "-----------------------------------------------------------------------------"
echo "データ送信ロックを設定すると、統合コントローラからモニタリングサーバに送信する"
echo "情報が空のファイル（統合コントローラＩＤと終端情報のみで、装置状態・計測情報・"
echo "状変情報を収録しないファイル）を生成します。"
echo "ロック設定で出荷した場合は、最初の現地接続確認試験時の異常情報や調整中計測情"
echo "報がモニタリングサーバに通知される事態を回避できます。"
echo "ロック設定は【モニタリングサーバから遠隔で解除可能】です。"
echo "============================================================================="
echo ""
echo "--------------------------------"
echo "データ送信ロックを設定しますか？"
echo "--------------------------------"
echo -n "( y or n ) >>> "
read WORKDATA
if [ "zzy" = "zz${WORKDATA}" ]
then
	echo "-------------------------------------------------"
	echo "データ送信ロックを設定します"
	echo "Create file : /home/nksystem/ONLDATA/dtsdlock_flg"
	echo "-------------------------------------------------"
	\touch /home/nksystem/ONLDATA/dtsdlock_flg
	\chown nksystem:nksystem /home/nksystem/ONLDATA/dtsdlock_flg
	\chmod 666 /home/nksystem/ONLDATA/dtsdlock_flg
fi

# 作業前のディレクトリに移動
cd ${OLDDIR}

# 終了メッセージ
echo "============================================================================="
echo "データの初期化が終了致しました"
echo "============================================================================="
