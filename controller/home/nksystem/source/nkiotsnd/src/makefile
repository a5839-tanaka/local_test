TARGET_PROC = nkiotsnd

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = nkiotsnd.c

OBJS = $(OBJ_DIR)nkiotsnd.o

LIBS = \
	../../external_lib_aws/obj/awscomm.a \
	../../external_lib_mbedTLS/library/libmbedtls.a \
	../../external_lib_mbedTLS/library/libmbedcrypto.a \
	../../external_lib_mbedTLS/library/libmbedx509.a \
	../../lib/obj/pvcomm.a

INCS = \
	../../external_lib_aws/include/aws_iot_config.h \
	../../external_lib_aws/include/aws_iot_log.h \
	../../external_lib_aws/include/aws_iot_version.h \
	../../external_lib_aws/include/aws_iot_mqtt_client_interface.h \
	../../inc/pvjson.h \
	../../inc/pvdefine.h \
	../../inc/pvfile.h \
	../../inc/pvserial.h \
	../../inc/pvcomm.h

CC := gcc
CFLAGS := -lpthread
RM := \rm
CP := cp -p

INC_PATH = \
	-I../../external_lib_mbedTLS/include \
	-I../../external_lib_aws/platform/linux/mbedtls \
	-I../../external_lib_aws/platform/linux/common \
	-I../../external_lib_aws/platform/linux/pthread \
	-I../../external_lib_aws/include \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(LIBS) $(INCS)
	@echo "======================================================"
	@echo "Target Linking : $(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATH) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Target Chmod   : +s"
	@chmod +s $(TARGET)
	@echo "Target Copy to : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)nkiotsnd.o : nkiotsnd.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
