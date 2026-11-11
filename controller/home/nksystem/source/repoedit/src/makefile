TARGET_PROC = repoedit

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = repoedit.c

OBJS = $(OBJ_DIR)repoedit.o

LIBS = \
        ../../lib/obj/pvcomm.a

INCS = \
	../../inc/pvdefine.h \
	../../inc/pvfile.h \
	../../inc/pvcomm.h

CC := gcc
CFLAGS := -lpthread
RM := \rm
CP := cp -p

INC_PATH = \
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

$(OBJ_DIR)repoedit.o : repoedit.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
