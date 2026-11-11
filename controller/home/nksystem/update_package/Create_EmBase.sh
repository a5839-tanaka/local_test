#!/bin/sh

# EMBASEのプログラムアップデート用ファイルを生成する。
#----------------------------------------------------------------------------------
# 更新履歴
# 

echo "============================================================================="
echo "ＮＫ－ＥＭＢＡＳＥ"
echo "-----------------------------------------------------------------------------"
echo "プログラムアップデート用アーカイブファイル生成ツール"
echo "============================================================================="

if [ "zz${USER}" != "zzroot" ] ; then
	echo "ERROR: ルートユーザ以外はこのツールを起動できません"
	exit
fi

# 現在のディレクトリの保存
OLDDIR=`pwd`


# 現在時刻の取得
DATEINFO=`date +%Y%m%d_%H`

# アップデート（アーカイブ）ファイル名の決定
TARGETFILE="/home/nksystem/update_package/NKEMBASE_UPDATE_AlmaLinux_MGE_${DATEINFO}.tgz"

# 現在のディレクトリの保存
OLDDIR=`pwd`

# アーカイブファイルの生成
echo "・EMBASEのアーカイブファイルを生成しています"
# ルートディレクトリに移動
cd /home
tar cvzPpf ${TARGETFILE}   -X /home/nksystem/update_package/EXCLUDE_FILE_PV_STD.list \
/usr/lib/systemd/system/nksystem.service \
/usr/local/bin/nksystem_start.sh \
/usr/local/bin/nksystem_stop.sh \
/usr/local/bin/NKSYSTEM \
/usr/lib/systemd/system/nkembase.service \
/usr/local/bin/nkembase_start.sh \
/usr/local/bin/nkembase_stop.sh \
/usr/local/bin/NKEMBASE \
/home/nksystem/sbin/*

# 作業前のディレクトリに移動
cd ${OLDDIR}

# 終了メッセージ
echo "============================================================================="
echo "アップデート用アーカイブファイルの生成が終了致しました"
echo "-----------------------------------------------------------------------------"
echo "ファイル名：${TARGETFILE}"
echo "============================================================================="
