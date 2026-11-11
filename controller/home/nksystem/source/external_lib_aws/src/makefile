TARGET_PROC = awscomm.a

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj

AWS_DIR = .
PLATFORM_MBEDTLS_DIR = ../platform/linux/mbedtls
PLATFORM_COMMON_DIR = ../platform/linux/common
PLATFORM_PTHREAD_DIR = ../platform/linux/pthread

SRC_FILES = \
		$(PLATFORM_MBEDTLS_DIR)/network_mbedtls_wrapper.c \
		$(PLATFORM_COMMON_DIR)/timer.c \
		$(PLATFORM_PTHREAD_DIR)/threads_pthread_wrapper.c \
		$(AWS_DIR)/aws_iot_mqtt_client_yield.c \
		$(AWS_DIR)/aws_iot_mqtt_client_unsubscribe.c \
		$(AWS_DIR)/aws_iot_mqtt_client_subscribe.c \
		$(AWS_DIR)/aws_iot_mqtt_client_publish.c \
		$(AWS_DIR)/aws_iot_mqtt_client_connect.c \
		$(AWS_DIR)/aws_iot_mqtt_client_common_internal.c \
		$(AWS_DIR)/aws_iot_mqtt_client.c

OBJS = \
		$(OBJ_DIR)/network_mbedtls_wrapper.o \
		$(OBJ_DIR)/timer.o \
		$(OBJ_DIR)/threads_pthread_wrapper.o \
		$(OBJ_DIR)/aws_iot_mqtt_client_yield.o \
		$(OBJ_DIR)/aws_iot_mqtt_client_unsubscribe.o \
		$(OBJ_DIR)/aws_iot_mqtt_client_subscribe.o \
		$(OBJ_DIR)/aws_iot_mqtt_client_publish.o \
		$(OBJ_DIR)/aws_iot_mqtt_client_connect.o \
		$(OBJ_DIR)/aws_iot_mqtt_client_common_internal.o \
		$(OBJ_DIR)/aws_iot_mqtt_client.o

INCS += $(shell find ../include/ -name '*.h')
INCS += $(shell find $(PLATFORM_MBEDTLS_DIR)/ -name '*.h')
INCS += $(shell find $(PLATFORM_COMMON_DIR)/ -name '*.h')
#INCS += $(shell find $(PLATFORM_PTHREAD_DIR)/ -name '*.h')

INC_PATH += -I ../include
INC_PATH += -I $(PLATFORM_MBEDTLS_DIR)
INC_PATH += -I $(PLATFORM_COMMON_DIR)
INC_PATH += -I $(PLATFORM_PTHREAD_DIR)


#TLS - mbedtls
MBEDTLS_DIR = ../../external_lib_mbedTLS/
TLS_INCLUDE_DIR = -I $(MBEDTLS_DIR)/include
LD_FLAG :=

#Aggregate all include and src directories
INC_PATH += $(TLS_INCLUDE_DIR)

# Logging level control
#LOG_FLAGS += -DENABLE_IOT_DEBUG
#LOG_FLAGS += -DENABLE_IOT_INFO
#LOG_FLAGS += -DENABLE_IOT_WARN
#LOG_FLAGS += -DENABLE_IOT_ERROR

COMPILER_FLAGS += $(LOG_FLAGS)
#If the processor is big endian uncomment the compiler flag
#COMPILER_FLAGS += -DREVERSED

CC := gcc
AR := ar
CFLAGS := -lpthread
RM := \rm
CP := \cp -p 


$(TARGET) : $(OBJS) $(INCS)
	@echo "AWS COMMON LIBRARY ($(TARGET)) ARCHIVES"
	@echo "================================================================"
	@$(AR) -tv $@
	@echo "================================================================"

$(OBJ_DIR)/network_mbedtls_wrapper.o : $(PLATFORM_MBEDTLS_DIR)/network_mbedtls_wrapper.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/timer.o : $(PLATFORM_COMMON_DIR)/timer.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/threads_pthread_wrapper.o : $(PLATFORM_PTHREAD_DIR)/threads_pthread_wrapper.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client_yield.o : aws_iot_mqtt_client_yield.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client_unsubscribe.o : aws_iot_mqtt_client_unsubscribe.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client_subscribe.o : aws_iot_mqtt_client_subscribe.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client_publish.o : aws_iot_mqtt_client_publish.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client_connect.o : aws_iot_mqtt_client_connect.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client_common_internal.o : aws_iot_mqtt_client_common_internal.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

$(OBJ_DIR)/aws_iot_mqtt_client.o : aws_iot_mqtt_client.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) $(LD_FLAG) -c $< -o $@
	@$(AR) -rv $(TARGET) $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
