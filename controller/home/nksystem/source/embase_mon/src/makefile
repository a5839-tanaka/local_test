TARGET_PROC = embase_mon

TARGET = ../obj/$(TARGET_PROC)

OBJ_DIR = ../obj/

SRCS = embase_mon.c

OBJS = $(OBJ_DIR)embase_mon.o

LIBS = ../../embaselib/obj/embaselib.a

INCS = \
	../../inc/embase.h \
	../inc/$(TARGET_PROC).h

CC := gcc
CFLAGS := -lpthread -lrt
RM := \rm
CP := cp -p

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(LIBS) $(INCS)
	@echo "======================================================"
	@echo "Target Linking : $(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATH) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Target Copy to : ../../../sbin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../sbin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)embase_mon.o : embase_mon.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
