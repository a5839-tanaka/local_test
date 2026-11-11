TARGET_PROC = filemake

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = \
		filemake.c \
		csv_cep.c \
		file_pvsysinf.c \
		file_comliinf.c \
		file_tmdatinf.c \
		file_svdatinf.c \
		file_stdatinf.c \
		file_pvsvrinf.c \
		file_tmagginf.c \
		file_svagginf.c \
		file_tmmoninf.c \
		file_cemspinf.c \
		file_demndinf.c \
		file_rcvpwinf.c \
		file_ovrchinf.c \
		file_mdctlinf.c \
		file_batteinf.c \
		file_powcninf.c \
		file_mltmtinf.c \
		file_remioinf.c \
		file_plcfainf.c \
		file_rtctlinf.c \
		file_demndfil.c \
		file_tmstsfil.c \
		file_ststsfil.c \
		file_svstsfil.c \
		file_johenfil.c \
		file_tmstofil.c \
		file_tmrecfil.c \
		file_svrecfil.c \
		file_rprecfil.c \
		file_pvctlfil.c \
		file_emctlfil.c \
		file_ctljhfil.c \
		file_svaggfil.c \
		file_pwplninf.c \
		file_pwplnfil.c \
		file_pwplnfil.c \
		file_pgconinf.c \
		file_schctlfil.c \
		file_iotcnfinf.c \
		file_fcrstinf.c \
		file_fcrrpinf.c 

OBJS = \
		$(OBJ_DIR)filemake.o \
		$(OBJ_DIR)csv_sep.o \
		$(OBJ_DIR)file_pvsysinf.o \
		$(OBJ_DIR)file_comliinf.o \
		$(OBJ_DIR)file_tmdatinf.o \
		$(OBJ_DIR)file_svdatinf.o \
		$(OBJ_DIR)file_stdatinf.o \
		$(OBJ_DIR)file_pvsvrinf.o \
		$(OBJ_DIR)file_tmagginf.o \
		$(OBJ_DIR)file_svagginf.o \
		$(OBJ_DIR)file_tmmoninf.o \
		$(OBJ_DIR)file_cemspinf.o \
		$(OBJ_DIR)file_demndinf.o \
		$(OBJ_DIR)file_rcvpwinf.o \
		$(OBJ_DIR)file_ovrchinf.o \
		$(OBJ_DIR)file_mdctlinf.o \
		$(OBJ_DIR)file_batteinf.o \
		$(OBJ_DIR)file_powcninf.o \
		$(OBJ_DIR)file_mltmtinf.o \
		$(OBJ_DIR)file_remioinf.o \
		$(OBJ_DIR)file_plcfainf.o \
		$(OBJ_DIR)file_rtctlinf.o \
		$(OBJ_DIR)file_demndfil.o \
		$(OBJ_DIR)file_tmstsfil.o \
		$(OBJ_DIR)file_ststsfil.o \
		$(OBJ_DIR)file_svstsfil.o \
		$(OBJ_DIR)file_johenfil.o \
		$(OBJ_DIR)file_tmstofil.o \
		$(OBJ_DIR)file_tmrecfil.o \
		$(OBJ_DIR)file_svrecfil.o \
		$(OBJ_DIR)file_rprecfil.o \
		$(OBJ_DIR)file_pvctlfil.o \
		$(OBJ_DIR)file_emctlfil.o \
		$(OBJ_DIR)file_ctljhfil.o \
		$(OBJ_DIR)file_svaggfil.o \
		$(OBJ_DIR)file_pwplninf.o \
		$(OBJ_DIR)file_pwplnfil.o \
		$(OBJ_DIR)file_pgconinf.o \
		$(OBJ_DIR)file_schctlfil.o \
		$(OBJ_DIR)file_iotcnfinf.o \
		$(OBJ_DIR)file_fcrstinf.o \
		$(OBJ_DIR)file_fcrrpinf.o

LIBS = ../../lib/obj/pvcomm.a

INCS = \
		../inc/filemake.h \
		../../inc/pvdefine.h \
		../../inc/pvfile.h \
		../../inc/pvcomm.h \
		../inc/$(TARGET_PROC).h

INCS2 = \
		../../inc/pvevent.h

CC := gcc
CFLAGS := -lpthread
RM := \rm
CP := \cp -p 

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(LIBS) $(INCS) $(INCS2)
	@echo "======================================================"
	@echo "Target Linking : $(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATH) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Target Chmod   : a+s"
	@chmod a+s $(TARGET)
	@echo "Target Copy to : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PORC)
	@echo "======================================================"

$(OBJ_DIR)filemake.o : filemake.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)csv_sep.o : csv_sep.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@


$(OBJ_DIR)file_pvsysinf.o : file_pvsysinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_comliinf.o : file_comliinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_tmdatinf.o : file_tmdatinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_svdatinf.o : file_svdatinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_stdatinf.o : file_stdatinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_pvsvrinf.o : file_pvsvrinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_tmagginf.o : file_tmagginf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_svagginf.o : file_svagginf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_tmmoninf.o : file_tmmoninf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_cemspinf.o : file_cemspinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_demndinf.o : file_demndinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_rcvpwinf.o : file_rcvpwinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_ovrchinf.o : file_ovrchinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_mdctlinf.o : file_mdctlinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_batteinf.o : file_batteinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_powcninf.o : file_powcninf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_mltmtinf.o : file_mltmtinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_remioinf.o : file_remioinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_plcfainf.o : file_plcfainf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_rtctlinf.o : file_rtctlinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_demndfil.o : file_demndfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_tmstsfil.o : file_tmstsfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_ststsfil.o : file_ststsfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_svstsfil.o : file_svstsfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_johenfil.o : file_johenfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_tmstofil.o : file_tmstofil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_tmrecfil.o : file_tmrecfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_svrecfil.o : file_svrecfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_rprecfil.o : file_rprecfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_pvctlfil.o : file_pvctlfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_emctlfil.o : file_emctlfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_ctljhfil.o : file_ctljhfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_svaggfil.o : file_svaggfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_pwplninf.o : file_pwplninf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_pwplnfil.o : file_pwplnfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_pgconinf.o : file_pgconinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_schctlfil.o : file_schctlfil.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_iotcnfinf.o : file_iotcnfinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_fcrstinf.o : file_fcrstinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

$(OBJ_DIR)file_fcrrpinf.o : file_fcrrpinf.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TERGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
