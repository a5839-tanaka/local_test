TARGET_PROC = pvcomm.a

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = \
		comm_manage.c \
		comm_wait.c \
		comm_serial.c \
		comm_file.c \
		comm_johen.c \
		comm_bitsub.c \
		comm_pow10abs.c \
		comm_sys.c \
		comm_devdata.c \
		comm_pvctl.c \
		comm_json.c \
		comm_syslog.c \
		comm_ctl_johen.c \
		comm_pvsch.c

OBJS = \
		$(OBJ_DIR)comm_manage.o \
		$(OBJ_DIR)comm_wait.o \
		$(OBJ_DIR)comm_serial.o \
		$(OBJ_DIR)comm_file.o \
		$(OBJ_DIR)comm_johen.o \
		$(OBJ_DIR)comm_bitsub.o \
		$(OBJ_DIR)comm_pow10abs.o \
		$(OBJ_DIR)comm_sys.o \
		$(OBJ_DIR)comm_devdata.o \
		$(OBJ_DIR)comm_pvctl.o \
		$(OBJ_DIR)comm_json.o \
		$(OBJ_DIR)comm_syslog.o \
		$(OBJ_DIR)comm_ctl_johen.o \
		$(OBJ_DIR)comm_pvsch.o

INCS = \
		../../inc/pvdefine.h \
		../../inc/pvfile.h \
		../../inc/pvcomm.h

INCS2 = \
		../../inc/pvserial.h

INCS3 = \
		../../inc/pvdevdata.h

INCS4 = \
		../../inc/pvjson.h


CC := gcc
AR := ar
CFLAGS := 
RM := \rm
CP := \cp -p 

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(INCS) $(INCS2) $(INCS3) $(INCS4)
	@echo "NKSYSTEM COMMON LIBRARY ($(TARGET)) ARCHIVES"
	@echo "================================================================"
	@$(AR) -tv $@
	@echo "================================================================"

$(OBJ_DIR)comm_manage.o : comm_manage.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_wait.o : comm_wait.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_serial.o : comm_serial.c $(INCS) $(INCS2)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_file.o : comm_file.c  $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_johen.o : comm_johen.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_bitsub.o : comm_bitsub.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_pow10abs.o : comm_pow10abs.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_sys.o : comm_sys.c $(INCS) $(INCS3)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_devdata.o : comm_devdata.c $(INCS) $(INCS3)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_pvctl.o : comm_pvctl.c $(INCS) $(INCS4)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_json.o : comm_json.c $(INCS) $(INCS4)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_syslog.o : comm_syslog.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_ctl_johen.o : comm_ctl_johen.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

$(OBJ_DIR)comm_pvsch.o : comm_pvsch.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
