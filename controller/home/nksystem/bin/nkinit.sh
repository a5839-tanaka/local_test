#!/bin/sh

#=============================================
# NKSYSTEM Execute Scripts
#---------------------------------------------
# 2019.01.18 1st release
#=============================================

#---------------------------------------------
# NKSYSTEM Base Path
#---------------------------------------------
NK_PATH_BASE=/home/nksystem

#---------------------------------------------
# NKSYSTEM File Path
#---------------------------------------------
NK_PATH_BIN=${NK_PATH_BASE}/bin
NK_PATH_CSV=${NK_PATH_BASE}/CSVDATA
NK_PATH_DAT=${NK_PATH_BASE}/ONLDATA
NK_PATH_DWL=${NK_PATH_BASE}/DWLDATA
NK_PATH_REC=${NK_PATH_BASE}/RECDATA

#---------------------------------------------
# NKSYSTEM Execute Program
#---------------------------------------------
NKSYSTEM_INIT=nkinit

#---------------------------------------------
# NKSYSTEM CSV-Data Update
#---------------------------------------------
NK_DATA_NEW=${NK_PATH_DWL}/DATA_UPDATE_NEW.tgz
NK_DATA_OLD=${NK_PATH_DWL}/DATA_UPDATE_OLD.tgz
NK_DATA_BCK=${NK_PATH_DWL}/DATA_BACKUP_OLD.tgz

#---------------------------------------------
# NKSYSTEM Process Update
#---------------------------------------------
NK_PROC_NEW=${NK_PATH_DWL}/PROC_UPDATE_NEW.tgz
NK_PROC_OLD=${NK_PATH_DWL}/PROC_UPDATE_OLD.tgz
NK_PROC_BCK=${NK_PATH_DWL}/PROC_BACKUP_OLD.tgz

# [20170921] Add
#---------------------------------------------
# NKSYSTEM Zabbix-Data Update
#---------------------------------------------
NK_PATH_ZABBIX_CONF=/etc/zabbix
NK_PATH_ZABBIX_PARAM=/etc/zabbix/zabbix_agentd.d

#---------------------------------------------
# STOP/REBOOT/SHUTDOWN/RECOVERY File
#---------------------------------------------
NK_STOP_FLG=${NK_PATH_DAT}/stop_flg
NK_REBOOT_FLG=${NK_PATH_DAT}/reboot_flg
NK_SHUTDOWN_FLG=${NK_PATH_DAT}/shutdown_flg
NK_RECOVERY_FLG=${NK_PATH_DAT}/recovery_flg
# [20140508] Add
NK_FALLBACK_FLG=${NK_PATH_DAT}/fallback_flg
NK_SILENTRB_FLG=${NK_PATH_DAT}/silentrb_flg

# [20250107] Add
#---------------------------------------------
# NKSYSTEM Rec-Data initFile
#---------------------------------------------
NK_RECDATA_INIT=${NK_PATH_REC}/recdata.tgz

#=============================================
# FUNCTION
#=============================================
func_csvdata_update()
{
	#--------------------------
	# CSV FILE UPDATE
	OLDPATH=`pwd`
	echo "  Backup *.csv Data -> (${NK_DATA_BCK})..."
	if [ $(find ${NK_PATH_DWL}/ -name "*UPDATECNF.csv" | grep -c "UPDATECNF.csv") -ne 0 ] ; then
		\tar czPpf ${NK_DATA_BCK} ${NK_PATH_CSV}/*.csv /root/.ssh/id_rsa.PVSERVER* ${NK_PATH_BASE}/.ssh/id_rsa.PVSERVER* ${NK_PATH_ZABBIX_CONF}/zabbix_agentd.conf ${NK_PATH_ZABBIX_PARAM}/userparameter_allsetting.conf ${NK_PATH_DWL}/*UPDATECNF.csv
	else
		\tar czPpf ${NK_DATA_BCK} ${NK_PATH_CSV}/*.csv /root/.ssh/id_rsa.PVSERVER* ${NK_PATH_BASE}/.ssh/id_rsa.PVSERVER* ${NK_PATH_ZABBIX_CONF}/zabbix_agentd.conf ${NK_PATH_ZABBIX_PARAM}/userparameter_allsetting.conf
	fi
	echo "  Old CSV Data Delete"
	\rm -f ${NK_PATH_CSV}/*.csv
	\rm -f ${NK_PATH_DWL}/*UPDATECNF.csv
	echo "  Update File (${NK_DATA_NEW}) Unpaking..."
	cd ${NK_PATH_CSV}
	\tar xzpPf ${NK_DATA_NEW} > /dev/null 2>&1
	cd ${OLDPATH}
	echo "  Update File Rename (${NK_DATA_NEW}) -> (${NK_DATA_OLD})"
	\mv -f ${NK_DATA_NEW} ${NK_DATA_OLD}	
	#--------------------------
	# SSH KEY FILE UPDATE
	if [ -f ${NK_PATH_CSV}/id_rsa.PVSERVER_1 ] || [ -f ${NK_PATH_CSV}/id_rsa.PVSERVER_2 ] ; then
		\cp ${NK_PATH_CSV}/id_rsa.PVSERVER_? /root/.ssh/
		\chown root:root /root/.ssh/id_rsa.PVSERVER_?
		\chmod 600 /root/.ssh/id_rsa.PVSERVER_?
		\cp ${NK_PATH_CSV}/id_rsa.PVSERVER_? ${NK_PATH_BASE}/.ssh/
		\chown nksystem:nksystem ${NK_PATH_BASE}/.ssh/id_rsa.PVSERVER_?
		\chmod 600 ${NK_PATH_BASE}/.ssh/id_rsa.PVSERVER_?
		\rm -f ${NK_PATH_CSV}/id_rsa.PVSERVER_?
	fi
	# [20170921] Add
	#--------------------------
	# ZABBIX CONFIG DATA FILE UPDATE
	REBOOTFLG=0
	if [ -f ${NK_PATH_CSV}/zabbix_agentd.conf ] ; then
		if [ ! -d ${NK_PATH_ZABBIX_CONF} ]; then
			mkdir -p ${NK_PATH_ZABBIX_CONF}
		fi
		echo "Move zabbix_agentd.conf -> ${NK_PATH_ZABBIX_CONF}"
		\mv -f ${NK_PATH_CSV}/zabbix_agentd.conf ${NK_PATH_ZABBIX_CONF}
		REBOOTFLG=1
	fi
	ls ${NK_PATH_CSV}/userparameter_allsetting.conf > /dev/null 2>&1
	if [ $? -eq 0 ] ; then
		if [ ! -d ${NK_PATH_ZABBIX_PARAM} ]; then
			mkdir -p ${NK_PATH_ZABBIX_PARAM}
		fi
		\rm -f ${NK_PATH_ZABBIX_PARAM}/*
		echo "Move userparameter_allsetting.conf -> ${NK_PATH_ZABBIX_PARAM}"
		\mv -f ${NK_PATH_CSV}/userparameter_allsetting.conf ${NK_PATH_ZABBIX_PARAM}
		REBOOTFLG=1
	fi
	if [ $REBOOTFLG -eq 1 ]; then
		echo "Zabbix agentd restart"
		\systemctl restart zabbix-agent
	fi
}

#=============================================
# FUNCTION
#=============================================
func_program_update()
{
	#--------------------------
	# PROGRAM FILE UPDATE
	OLDPATH=`pwd`
	echo "  Backup Program  -> (${NK_PROC_BCK})..."
	\tar czPpf ${NK_PROC_BCK} ${NK_PATH_BIN}/*
	echo "  Update File (${NK_PROC_NEW}) Unpaking..."
	cd /
	\tar xzpPf ${NK_PROC_NEW} > /dev/null 2>&1
	cd ${OLDPATH}
	echo "  Update File Rename (${NK_PROC_NEW}) -> (${NK_PROC_OLD})"
	\mv -f ${NK_PROC_NEW} ${NK_PROC_OLD}	
}

#=============================================
# START MESSAGES
#=============================================
echo "------------------------------------------"
echo "NKSYSTEM Startup Script (nkinit.sh) Start!"
echo "------------------------------------------"

#=============================================
# MULTIPLE START CANCEL
#=============================================
PVCHECKWD=`/usr/bin/pgrep -o nkmanage`
if [ -n "${PVCHECKWD}" ] ; then
	echo "NKSYSTEM Process Alrady Executed! [nkmanage PID:${PVCHECKWD}]"
	echo "NKSYSTEM Startup Script Stop!"
	exit 0
fi

#=============================================
# DELETE OLD BACKUP FILE (60 MIN BEFORE)
#=============================================
echo "NKSYSTEM Backup Data Check..."
CHECK_WD=`find ${NK_PATH_DWL} -name DATA_BACKUP_OLD.tgz -mmin +60`
if [ -n "${CHECK_WD}" ] ;then
	echo "  Delete Backupfile(DATA) [${CHECK_WD}]... (60min ago)"
	\mv -f ${CHECK_WD} ${CHECK_WD}.sav
fi
CHECK_WD=`find ${NK_PATH_DWL} -name PROC_BACKUP_OLD.tgz -mmin +60`
if [ -n "${CHECK_WD}" ] ;then
	echo "  Delete Backupfile(PROGRAM) [${CHECK_WD}]... (60min ago)"
	\mv -f ${CHECK_WD} ${CHECK_WD}.sav
fi

#=============================================
# RECOVERY CHECK
#=============================================
echo "NKSYSTEM Recovery Check..."
if [ -e ${NK_RECOVERY_FLG} ] ; then
	echo "  Recovery Flag Find!!"
	if [ -f ${NK_DATA_BCK} ] ; then
		echo "  Recovery File (${NK_DATA_BCK}) Exist!"
		echo "  Recovery File Unpaking..."
		\rm -f ${NK_PATH_CSV}/*.csv
		\tar xzpPf ${NK_DATA_BCK} > /dev/null 2>&1
		echo "  Delete Recovery File (${NK_DATA_BCK})"
		\rm -f ${NK_DATA_BCK}
		# [20140508] Add
		echo "  Set Fallback Flag !!"
		\touch ${NK_FALLBACK_FLG}
		\chmod 666 ${NK_FALLBACK_FLG}
		# [20161207] Add
		if [ -f ${NK_DATA_OLD} ] ; then
			echo "  Delete Update File (${NK_DATA_OLD})"
			\rm -f ${NK_DATA_OLD} > /dev/null 2>&1
		fi
		# [20161207] End
	else
		echo "  Recovery File (${NK_DATA_BCK}) Not Exist"
	fi
	if [ -f ${NK_PROC_BCK} ] ; then
		echo "  Recovery File (${NK_PROC_BCK}) Exist!"
		echo "  Recovery File Unpaking..."
		\tar xzpPf ${NK_PROC_BCK} > /dev/null 2>&1
		echo "  Delete Recovery File (${NK_PROC_BCK})"
		\rm -f ${NK_PROC_BCK}
		# [20140508] Add
		echo "  Set Fallback Flag !!"
		\touch ${NK_FALLBACK_FLG}
		\chmod 666 ${NK_FALLBACK_FLG}
		# [20161207] Add
		if [ -f ${NK_PROC_OLD} ] ; then
			echo "  Delete Update File (${NK_PROC_OLD})"
			\rm -f ${NK_PROC_OLD} > /dev/null 2>&1
		fi
		# [20161207] End
	else
		echo "  Recovery File (${NK_PROC_BCK}) Not Exist"
	fi
fi

#=============================================
# DATA UPDATE
#=============================================
echo "NKSYSTEM Data Update Check..."
if [ -f ${NK_DATA_NEW} ] ;then
	echo "  Update File(${NK_DATA_NEW}) Exist!"
	if [ -f ${NK_DATA_OLD} ] ; then
		echo "  Update File NEW-OLD Different Check..."
		\cmp ${NK_DATA_NEW} ${NK_DATA_OLD} > /dev/null 2>&1
		if [ $? -eq 0 ] ; then
			echo "  Update Needless (NEW=OLD)"
			echo "  Update File Delete."
			\rm -f ${NK_DATA_NEW} > /dev/null 2>&1
		else
			func_csvdata_update
		fi
	else
		func_csvdata_update
	fi
	# [20140508] Add
	if [ -e ${NK_FALLBACK_FLG} ] ; then
		echo "NKSYSTEM Fallback Flag Delete"
		\rm -f ${NK_FALLBACK_FLG}
	fi
else
	echo "  Update File (${NK_DATA_NEW}) Not Exist."
fi

#=============================================
# PROGRAM UPDATE
#=============================================
echo "NKSYSTEM Process Update Check..."
if [ -f ${NK_PROC_NEW} ] ;then
	echo "  Update File (${NK_PROC_NEW}) Exist!"
	if [ -f ${NK_PROC_OLD} ] ; then
		echo "  Update File NEW-OLD Different Check..."
		\cmp ${NK_PROC_NEW} ${NK_PROC_OLD} > /dev/null 2>&1
		if [ $? -eq 0 ] ; then
			echo "  Update Needless (NEW=OLD)"
			echo "  Update File Delete."
			\rm -f ${NK_PROC_NEW} > /dev/null 2>&1
		else
			func_program_update
		fi
	else
		func_program_update
	fi
	# [20140508] Add
	if [ -e ${NK_FALLBACK_FLG} ] ; then
		echo "NKSYSTEM Fallback Flag Delete"
		\rm -f ${NK_FALLBACK_FLG}
	fi
else
	echo "  Update File (${NK_PROC_NEW}) Not Exist."
fi

#=============================================
# FILE PARMISSION SETTING
#=============================================
echo "NKSYSTEM Data File Parmission Setting..."
chown nksystem:nksystem ${NK_PATH_CSV}/*.csv
chmod 644 ${NK_PATH_CSV}/*.csv

#=============================================
# DELETE FLAGS
#=============================================
if [ -e ${NK_STOP_FLG} ] ; then
	echo "NKSYSTEM Stop Flag Delete"
	\rm -f ${NK_STOP_FLG}
fi
if [ -e ${NK_REBOOT_FLG} ] ; then
	echo "NKSYSTEM Reboot Flag Delete"
	\rm -f ${NK_REBOOT_FLG}
fi
if [ -e ${NK_SHUTDOWN_FLG} ] ; then
	echo "NKSYSTEM Shutdown Flag Delete"
	\rm -f ${NK_SHUTDONW_FLG}
fi
if [ -e ${NK_RECOVERY_FLG} ] ; then
	echo "NKSYSTEM Recovery Flag Delete"
	\rm -f ${NK_RECOVERY_FLG}
fi
# [20140508] Add ([20140530] Comment out )
#if [ -e ${NK_SILENTRB_FLG} ] ; then
#	echo "NKSYSTEM Silent Reboot Flag Delete"
#	\rm -f ${NK_SILENTRB_FLG}
#fi

# [20250107] Add
#=============================================
# NKSYSTEM RECDATA INITIALIZE
#=============================================
if [ ! -e ${NK_RECDATA_INIT} ] ;then
	mkdir -p ${NK_PATH_REC}
	echo "NKSYSTEM RecDataFile Initialize..."
	touch ${NK_RECDATA_INIT}
fi
echo "NKSYSTEM RecDataFile Parmission Setting..."
chown nksystem:nksystem ${NK_PATH_REC}
chmod 755 ${NK_PATH_REC}
chown nksystem:nksystem ${NK_RECDATA_INIT}
chmod 644 ${NK_RECDATA_INIT}

#=============================================
# PERMISSION CONTROL
#=============================================
chmod 755 ${NK_PATH_BIN}/nkinit.sh
chmod 755 ${NK_PATH_BIN}/nkinit
chmod 755 ${NK_PATH_BIN}/nkmanage

#=============================================
# NKSYSTEM EXECUTE
#=============================================
echo "NKSYSTEM Process Execute!"
${NK_PATH_BIN}/${NKSYSTEM_INIT}

exit 0
