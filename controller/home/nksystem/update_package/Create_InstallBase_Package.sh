#!/bin/sh

# 統合コントローラの初期構築用ファイルを生成する。
#
# 統合コントローラの初期構築用ファイルは必ず / ディレクトリから始まる形式で作成する。
# Linux で普通に tar コマンドを使用すると、アーカイブ対象となるファイルの先頭の / が自動的に削除されてしまう。
# これを回避するため tar コマンドのオプションに -P をつける。
# ＜本ツールが生成する初期構築用ファイル＞
#  NKSYSTEM_ALL_YYYYMMDD_HH.tgz
# 実記に適用する場合は、サーバーの指定ディレクトリ上に 統合コントローラID_PROC_UPDATE.tgz の名称で配置する必要がある。
#----------------------------------------------------------------------------------
# 更新履歴
# 2019.01.18 新規作成
# 

echo "============================================================================="
echo "ＮＫ－ＳＹＳＴＥＭ 統合コントローラ"
echo "-----------------------------------------------------------------------------"
echo "初期構築用アーカイブファイル生成ツール"
echo "============================================================================="

if [ "zz${USER}" != "zzroot" ] ; then
	echo "ERROR: ルートユーザ以外はこのツールを起動できません"
	exit
fi

echo "----------------------------------------"
echo "初期構築用アーカイブ作成を開始しますか？"
echo "----------------------------------------"
echo -n "( y or n ) >>> "
read WORKDATA
if [ "zzy" != "zz${WORKDATA}" ] ; then
	echo "処理を中止します"
	exit
fi

# アプリケーションの停止
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
		test ${LOOPCNT} -lt 16 || break
	done
	\rm -f /home/nksystem/ONLDATA/stop_flg > /dev/null 2>&1
fi

# 現在時刻の取得
DATEINFO=`date +%Y%m%d_%H`

# アーカイブファイル名の決定
TARGETFILE="/home/nksystem/update_package/NKSYSTEM_ALL_AlmaLinux_MGE_${DATEINFO}.tgz"

# 現在のディレクトリの保存
OLDDIR=`pwd`

# CSVファイルの初期化
echo "・ＣＳＶファイルを初期化しています"
echo "  (/home/nksystem/CSVDATA/)"
\rm /home/nksystem/CSVDATA/*.csv > /dev/null 2>&1
cp -p /home/nksystem/CSVDATA/SAMPLE/COMLIINF.csv_00SYSTEM /home/nksystem/CSVDATA/COMLIINF.csv
cp -p /home/nksystem/CSVDATA/SAMPLE/PVSYSINF.csv_00SYSTEM /home/nksystem/CSVDATA/PVSYSINF.csv
cp -p /home/nksystem/CSVDATA/SAMPLE/PVSVRINF.csv_00SYSTEM /home/nksystem/CSVDATA/PVSVRINF.csv
cp -p /home/nksystem/CSVDATA/SAMPLE/STDATINF.csv_00SYSTEM /home/nksystem/CSVDATA/STDATINF.csv
cp -p /home/nksystem/CSVDATA/SAMPLE/SVDATINF.csv_00SYSTEM /home/nksystem/CSVDATA/SVDATINF.csv
cp -p /home/nksystem/CSVDATA/SAMPLE/TMDATINF.csv_00SYSTEM /home/nksystem/CSVDATA/TMDATINF.csv
cp -p /home/nksystem/CSVDATA/SAMPLE/CEMSPINF.csv_00SYSTEM /home/nksystem/CSVDATA/CEMSPINF.csv

# Zabbixファイルの初期化
echo "・Ｚａｂｂｉｘの設定ファイルを初期化しています"
echo "  (/usr/local/etc/zabbix_agentd.conf)"
cp -p /home/nksystem/CSVDATA/SAMPLE/zabbix_agentd.conf_00SYSTEM /etc/zabbix/zabbix_agentd.conf
cp -p /home/nksystem/CSVDATA/SAMPLE/userparameter_allsetting.conf_00SYSTEM /etc/zabbix/zabbix_agentd.d/userparameter_allsetting.conf

# emsystem設定ファイルの初期化
echo "・ｅｍｓｙｓｔｅｍの設定ファイルを初期化しています"
echo "  (/home/nksystem/sbin/emsystem.conf)"
cp -p /home/nksystem/CSVDATA/SAMPLE/emsystem.conf_00SYSTEM /home/nksystem/sbin/emsystem.conf


# SSH認証関連ファイルの削除
\rm /root/.ssh/known_hosts > /dev/null 2>&1
\rm /home/nksystem/.ssh/known_hosts > /dev/null 2>&1

# オンラインデータの初期化
echo "・オンラインデータ領域を初期しています"
echo "  (/home/nksystem/ONLDATA/)"
\rm /home/nksystem/ONLDATA/* > /dev/null 2>&1

# FTP送信データの初期化
echo "・ＦＴＰ送信用データ格納領域を初期化しています"
echo "  (/home/nksystem/FTPDATA/)"
\rm /home/nksystem/FTPDATA/* > /dev/null 2>&1

# アップデート用ファイルダウンロード領域の初期化
echo "・アップデート用ファイルダウンロード格納領域を初期化しています"
echo "  (/home/nksystem/DWLDATA/)"
\rm /home/nksystem/DWLDATA/* > /dev/null 2>&1

# ログ領域の初期化
echo "・ログ格納領域を初期化しています"
echo "  (/home/nksystem/log/)"
\rm /home/nksystem/log/* > /dev/null 2>&1

# アーカイブファイルの生成
echo "・初期構築用アーカイブファイルを生成しています"
# ルートディレクトリに移動
cd /
tar cvzPpf ${TARGETFILE} -X /home/nksystem/update_package/EXCLUDE_FILE_PV_STD.list \
/etc/udev/rules.d/50-serialport.rules \
/usr/lib/systemd/system/nksystem.service \
/usr/local/bin/nksystem_start.sh \
/usr/local/bin/nksystem_stop.sh \
/usr/local/bin/NKSYSTEM \
/usr/lib/systemd/system/nkembase.service \
/usr/local/bin/nkembase_start.sh \
/usr/local/bin/nkembase_stop.sh \
/usr/local/bin/NKEMBASE \
/etc/zabbix/zabbix_agentd.conf \
/etc/zabbix/zabbix_agentd.d \
/root/.ssh/ \
/home/nksystem/.ssh/ \
/home/nksystem/bin/* \
/home/nksystem/tool/* \
/home/nksystem/CSVDATA/* \
/home/nksystem/ONLDATA/ \
/home/nksystem/FTPDATA/ \
/home/nksystem/DWLDATA/ \
/home/nksystem/EXTDATA/* \
/home/nksystem/log/ \
/home/nksystem/sbin/ \
/home/nksystem/sONLDATA/ \
/home/nksystem/slog/


# 作業前のディレクトリに移動
cd ${OLDDIR}

# 終了メッセージ
echo "============================================================================="
echo "初期構築用アーカイブファイルの生成が終了致しました"
echo "-----------------------------------------------------------------------------"
echo "ファイル名：${TARGETFILE}"
echo "============================================================================="
