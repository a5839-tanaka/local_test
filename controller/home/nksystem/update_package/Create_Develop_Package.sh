#!/bin/sh

# 統合コントローラの開発環境のアーカイブファイルを生成する。
# /homeにて、nksystemディレクトリのアーカイブファイルを作成する
# ＜本ツールが生成するアップデート用ファイル＞
#  NKSYSTEM_DEVELOP_YYYYMMDD_HH.tgz
#----------------------------------------------------------------------------------
# 更新履歴
# 

echo "============================================================================="
echo "ＮＫ－ＳＹＳＴＥＭ 統合コントローラ"
echo "-----------------------------------------------------------------------------"
echo "開発環境アーカイブファイル生成ツール"
echo "============================================================================="

if [ "zz${USER}" != "zzroot" ] ; then
	echo "ERROR: ルートユーザ以外はこのツールを起動できません"
	exit
fi

echo "--------------------------------------------"
echo "開発環境のアーカイブ作成を開始しますか？"
echo "--------------------------------------------"
echo -n "( y or n ) >>> "
read WORKDATA
if [ "zzy" != "zz${WORKDATA}" ] ; then
	echo "処理を中止します"
	exit
fi

# プリケーションの停止
#echo "・アプリケーション動作状態を確認しています"
#CHECKWD=`pgrep -o nkmanage`
#if [ -z ${CHECKWD} ] ; then
#	echo "・アプリケーションは動作していません"
#else
#	LOOPCNT=0
#	echo "・アプリケーションを停止しています"
#	/usr/bin/touch /home/nksystem/ONLDATA/stop_flg
#	/bin/chmod 666 /home/nksystem/ONLDATA/stop_flg
#	while [ 1 ]
#	do
#		sleep 10
#		test -e /home/nksystem/ONLDATA/stop_flg || break
#		LOOPCNT=`expr ${LOOPCNT} + 1`
#		test ${LOOPCNT} -lt 16 || break
#	done
#	\rm -f /home/nksystem/ONLDATA/stop_flg > /dev/null 2>&1
#fi

# 現在時刻の取得
DATEINFO=`date +%Y%m%d_%H`

# アップデート（アーカイブ）ファイル名の決定
TARGETFILE="/home/NKSYSTEM_DEVELOP_AlmaLinux_MGE_${DATEINFO}.tgz"

# 現在のディレクトリの保存
OLDDIR=`pwd`

# アーカイブファイルの生成
echo "・開発環境のアーカイブファイルを生成しています"
# ルートディレクトリに移動
cd /home
tar cvzPpf ${TARGETFILE} /etc/udev/rules.d/50-serialport.rules \
/usr/local/openssl/lib/lib* \
/usr/lib64/libssl.* \
/usr/lib64/libcrypto.* \
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
/etc/systemd/system/shutdown.target \
/etc/logrotate.d/recdata \
/root/.ssh/* \
/home/nksystem/.ssh/* \
/home/nksystem/source/* \
/home/nksystem/bin/* \
/home/nksystem/tool/* \
/home/nksystem/CSVDATA/* \
/home/nksystem/ONLDATA/* \
/home/nksystem/FTPDATA/* \
/home/nksystem/DWLDATA/* \
/home/nksystem/CERTDATA/* \
/home/nksystem/IOTDATA/* \
/home/nksystem/JSONDATA/* \
/home/nksystem/RECDATA/* \
/home/nksystem/log/* \
/home/nksystem/sbin/* \
/home/nksystem/sONLDATA/* \
/home/nksystem/slog/* \
/home/nksystem/update_package/* \
/home/nksystem/EXTDATA/*


# 作業前のディレクトリに移動
cd ${OLDDIR}

# 終了メッセージ
echo "============================================================================="
echo "開発環境のアーカイブファイルの生成が終了致しました"
echo "-----------------------------------------------------------------------------"
echo "ファイル名：${TARGETFILE}"
echo "============================================================================="
