TARGET_PROC = nkmanage

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = nkmanage.c

OBJS = $(OBJ_DIR)nkmanage.o

LIBS = ../../lib/obj/pvcomm.a \
	../../embaselib/obj/embaselib.a

INCS = \
	../../inc/pvdefine.h \
	../../inc/pvfile.h \
	../../inc/pvserial.h \
	../../inc/pvcomm.h \
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
	@echo "Target Copy to : ../../../bin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../bin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)nkmanage.o : nkmanage.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
