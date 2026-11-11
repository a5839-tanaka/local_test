TARGET_PROC = platformcomm.a

TARGET = ./obj/$(TARGET_PROC)
OBJ_DIR = ./obj/

#mkdir -p $(DESTDIR)/include/mbedtls

PLATFORM_MBEDTLS_DIR = ./linux/mbedtls
PLATFORM_COMMON_DIR = ./linux/common
PLATFORM_PTHREAD_DIR = ./linux/pthread

IOT_INCLUDE_DIRS += -I ../include
IOT_INCLUDE_DIRS += -I $(PLATFORM_MBEDTLS_DIR)
IOT_INCLUDE_DIRS += -I $(PLATFORM_DIR)
IOT_INCLUDE_DIRS += -I $(PLATFORM_PTHREAD_DIR)

IOT_SRC_FILES += $(shell find ./ -name '*.c')
IOT_SRC_FILES += $(shell find $(PLATFORM_DIR)/ -name '*.c')
IOT_SRC_FILES += $(shell find $(PLATFORM_COMMON_DIR)/ -name '*.c')


#TLS - mbedtls
MBEDTLS_DIR = ../external_libs/mbedTLS
TLS_LIB_DIR = $(MBEDTLS_DIR)/library
TLS_INCLUDE_DIR = -I $(MBEDTLS_DIR)/include
EXTERNAL_LIBS += -L$(TLS_LIB_DIR)
LD_FLAG += -Wl,-rpath,$(TLS_LIB_DIR)
LD_FLAG += -ldl $(TLS_LIB_DIR)/libmbedtls.a $(TLS_LIB_DIR)/libmbedcrypto.a $(TLS_LIB_DIR)/libmbedx509.a -lpthread -DPVCTL_MSGLEVEL=PVCTL_MSGLEVEL_NONE

#Aggregate all include and src directories
INCLUDE_ALL_DIRS += $(IOT_INCLUDE_DIRS)
INCLUDE_ALL_DIRS += $(TLS_INCLUDE_DIR)
SRC_FILES += $(IOT_SRC_FILES)

# Logging level control
LOG_FLAGS += -DENABLE_IOT_DEBUG
LOG_FLAGS += -DENABLE_IOT_INFO
LOG_FLAGS += -DENABLE_IOT_WARN
LOG_FLAGS += -DENABLE_IOT_ERROR

COMPILER_FLAGS += $(LOG_FLAGS)
#If the processor is big endian uncomment the compiler flag
#COMPILER_FLAGS += -DREVERSED

#MBED_TLS_MAKE_CMD = $(MAKE) -C $(MBEDTLS_DIR)
MBED_TLS_MAKE_CMD = $(MAKE) no_test -C $(MBEDTLS_DIR)

PRE_MAKE_CMD = $(MBED_TLS_MAKE_CMD)
MAKE_CMD = $(CC) $(SRC_FILES) $(COMPILER_FLAGS) -o $(TARGET) $(LD_FLAG) $(EXTERNAL_LIBS) $(INCLUDE_ALL_DIRS)

all:
$(PRE_MAKE_CMD)
$(TARGET) : $(OBJS) $(INCS) $(INCS2) $(INCS3)
	@echo "NKSYSTEM COMMON LIBRARY ($(TARGET)) ARCHIVES"
	@echo "================================================================"
	@$(AR) -tv $@
	@echo "================================================================"



	@echo "Target Chmod   : +s"
	@chmod +s $(TARGET)
	@echo "Target Copy to : ../../../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../../../bin/$(TARGET_PROC)

clean:
	@echo "Remove : $(TARGET)"
	@$(RM) -f $(TARGET)
#	rm -f $(APP_DIR)/$(APP_NAME)
#	$(MBED_TLS_MAKE_CMD) clean



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
		comm_syslog.c

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
		$(OBJ_DIR)comm_syslog.o
INCS = \
		../../inc/pvdefine.h \
		../../inc/pvfile.h \
		../../inc/pvcomm.h

INCS2 = \
		../../inc/pvserial.h

INCS3 = \
		../../inc/pvdevdata.h


CC := gcc
AR := ar
CFLAGS := 
RM := \rm
CP := \cp -p 

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(INCS) $(INCS2) $(INCS3)
	@echo "NKSYSTEM COMMON LIBRARY ($(TARGET)) ARCHIVES"
	@echo "================================================================"
	@$(AR) -tv $@
	@echo "================================================================"

$(OBJ_DIR)comm_manage.o : comm_manage.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_wait.o : comm_wait.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_serial.o : comm_serial.c $(INCS) $(INCS2)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_file.o : comm_file.c  $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_johen.o : comm_johen.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_bitsub.o : comm_bitsub.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_pow10abs.o : comm_pow10abs.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_sys.o : comm_sys.c $(INCS) $(INCS3)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_devdata.o : comm_devdata.c $(INCS) $(INCS3)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_pvctl.o : comm_pvctl.c $(INCS) $(INCS4)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)comm_syslog.o : comm_syslog.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
