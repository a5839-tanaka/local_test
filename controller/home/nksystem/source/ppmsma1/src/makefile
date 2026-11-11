TARGET_PROC = ppmsma1

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = ppmsma1.c

OBJS = $(OBJ_DIR)ppmsma1.o

LIBS = ../../lib/obj/pvcomm.a

INCS = \
		../../inc/pvdefine.h \
		../../inc/pvfile.h \
		../../inc/pvserial.h \
		../../inc/pvcomm.h \
		../inc/$(TARGET_PROC).h

CC := gcc
CFLAGS := -lpthread -lm
RM := \rm
CP := cp -p

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(LIBS) $(INCS)
	@echo "======================================================"
	@echo "Target Linking : $(TARGET)"
	@$(CC) $(CFLAGS) $(INC_PATH) -o $(TARGET) $(OBJS) $(LIBS)
	@echo "Target Chgrp   : dialout"
	@chgrp dialout $(TARGET)
	@echo "Target Chmod   : a+s"
	@chmod a+s $(TARGET)
	@echo "Target Copy to : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)ppmsma1.o : ppmsma1.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
