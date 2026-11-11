TARGET_PROC = emsystem

TARGET = ../obj/$(TARGET_PROC)

OBJ_DIR = ../obj/

SRCS = emsystem.c

OBJS = $(OBJ_DIR)emsystem.o

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

$(OBJ_DIR)emsystem.o : emsystem.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
