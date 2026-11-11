TARGET_PROC = sysdebug

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = sysdebug.c

INCS = \
    ../../inc/pvdefine.h \
    ../../inc/pvserial.h \
    ../../inc/pvfile.h \
    ../../inc/pvudp.h \
    ../../inc/pvcomm.h \
	../inc/$(TARGET_PROC).h

OBJS = $(OBJ_DIR)sysdebug.o

LIBS = ../../lib/obj/pvcomm.a

CC := gcc
CFLAGS := -lpthread
RM := \rm
CP := cp -p

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(LIBS) $(INCS)
	@echo "======================================================"
	@echo "Target Link : $(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATH) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Target Chmod   : +s"
	@chmod +s $(TARGET)
	@echo "Target Copy : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)sysdebug.o : sysdebug.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
