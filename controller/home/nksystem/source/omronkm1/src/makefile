TARGET_PROC = omronkm1

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = omronkm1.c

OBJS = $(OBJ_DIR)omronkm1.o

LIBS = ../../lib/obj/pvcomm.a

INCS = \
	../inc/omronkm1.h \
	../../inc/pvdefine.h \
	../../inc/pvfile.h \
	../../inc/pvserial.h \
	../../inc/pvcomm.h \
	../inc/$(TARGET_PROC).h

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
	@echo "Target chgrp   : dialout"
	@chgrp dialout $(TARGET)
	@echo "Target chmod   : a+s"
	@chmod a+s $(TARGET)
	@echo "Target Copy to : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)omronkm1.o : omronkm1.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJES)"
	@$(RM) -f $(TARGET) $(OBJS)
