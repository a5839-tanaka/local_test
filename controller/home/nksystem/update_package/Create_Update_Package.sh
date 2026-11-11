#!/bin/sh

# 統合コントローラのプログラムアップデート用ファイルを生成する。
#
# 統合コントローラのアップデート用ファイルは必ず / ディレクトリから始まる形式で作成する。
# Linux で普通に tar コマンドを使用すると、アーカイブ対象となるファイルの先頭の / が自動的に削除されてしまう。
# これを回避するため tar コマンドのオプションに -P をつける。
# ＜本ツールが生成するアップデート用ファイル＞
#  PROC_UPDATE_YYYYMMDD_HH.tgz
# 実記に適用する場合は、サーバーの指定ディレクトリ上に 統合コントローラID_PROC_UPDATE.tgz の名称で配置する必要がある。
#----------------------------------------------------------------------------------
# 更新履歴
# 2019.01.18 新規作成
# 2025.10.31 プログラム以外で変更の可能性があるファイルを削除（開発環境で異なる可能性がある為）
#            /root/.ssh/id_rsa.PVSERVER
#            /home/nksystem/.ssh/id_rsa.PVSERVER
# 2025.10.31 ソフトの起動に関係するファイルを削除
#            /etc/udev/rules.d/50-serialport.rules
#            /usr/lib/systemd/system/nksystem.service
#            /usr/local/bin/nksystem_start.sh
#            /usr/local/bin/nksystem_stop.sh
#            /usr/local/bin/NKSYSTEM
#

echo "============================================================================="
echo "ＮＫ－ＳＹＳＴＥＭ 統合コントローラ"
echo "-----------------------------------------------------------------------------"
echo "プログラムアップデート用アーカイブファイル生成ツール"
echo "============================================================================="

if [ "zz${USER}" != "zzroot" ] ; then
	echo "ERROR: ルートユーザ以外はこのツールを起動できません"
	exit
fi

echo "--------------------------------------------"
echo "アップデート用アーカイブ作成を開始しますか？"
echo "--------------------------------------------"
echo -n "( y or n ) >>> "
read WORKDATA
if [ "zzy" != "zz${WORKDATA}" ] ; then
	echo "処理を中止します"
	exit
fi

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
		test ${LOOPCNT} -lt 16 || break
	done
	\rm -f /home/nksystem/ONLDATA/stop_flg > /dev/null 2>&1
fi

# 現在時刻の取得
DATEINFO=`date +%Y%m%d_%H`

# アップデート（アーカイブ）ファイル名の決定
TARGETFILE="/home/nksystem/update_package/PROC_UPDATE_AlmaLinux_MGE_${DATEINFO}.tgz"

# 現在のディレクトリの保存
OLDDIR=`pwd`

# SSH認証関連ファイルの削除
\rm /root/.ssh/known_hosts > /dev/null 2>&1
\rm /home/nksystem/.ssh/known_hosts > /dev/null 2>&1

# アーカイブファイルの生成
echo "・アップデート用アーカイブファイルを生成しています"
# ルートディレクトリに移動
# 水平展開が必要になったものを随時追加する
cd /
tar cvzPpf ${TARGETFILE}  -X /home/nksystem/update_package/EXCLUDE_FILE_PV_STD.list \
/usr/local/openssl/lib/lib* \
/usr/lib64/libssl.* \
/usr/lib64/libcrypto.* \
/root/.ssh/config \
/root/.ssh/id_rsa.PVCLIENT \
/home/nksystem/.ssh/authorized_keys \
/home/nksystem/.ssh/config \
/home/nksystem/.ssh/id_rsa.PVCLIENT \
/home/nksystem/bin/* \
/home/nksystem/tool/*

# 作業前のディレクトリに移動
cd ${OLDDIR}

# 終了メッセージ
echo "============================================================================="
echo "アップデート用アーカイブファイルの生成が終了致しました"
echo "-----------------------------------------------------------------------------"
echo "ファイル名：${TARGETFILE}"
echo "============================================================================="
