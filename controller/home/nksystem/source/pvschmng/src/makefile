TARGET_PROC = pvschmng

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = pvschmng.c

OBJS = $(OBJ_DIR)pvschmng.o

LIBS = \
	../../lib/obj/pvcomm.a \
	/usr/lib64/libssl.so \
	/usr/lib64/libcrypto.so

INCS = \
	../inc/pvschmng.h \
	../../inc/pvdevdata.h \
	../../inc/pvdefine.h \
	../../inc/pvfile.h \
	../../inc/pvcomm.h

CC := gcc
#CFLAGS := -lpthread -DPVSCH_MSGLEVEL=PVSCH_MSGLEVEL_LOW
CFLAGS := -lpthread -DPVSCH_MSGLEVEL=PVSCH_MSGLEVEL_NONE
CFLAGS += -D PVCTL_DO_VAL_REF
#CFLAGS += -D DMD_DEBUG_DATA_SET
#CFLAGS += -D PV_CTL_VIRTL_SV
RM := \rm
CP := cp -p

INC_PATH = \
	-I../inc \
	-I../../inc \
	-I/usr/local/openssl/include

$(TARGET) : $(OBJS) $(LIBS) $(INCS)
	@echo "======================================================"
	@echo "Target Linking : $(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATH) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Target Copy to : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)pvschmng.o : pvschmng.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
