TARGET_PROC = eminit

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = eminit.c

OBJS = $(OBJ_DIR)eminit.o

LIBS = 

INCS = \
	../../inc/embase.h

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
	@echo "Target Copy to : ../../../sbin/$(TARGET_PROC)"
	@$(CP) -p $(TARGET) ../../../sbin/$(TARGET_PROC)
	@echo "======================================================"

$(OBJ_DIR)eminit.o : eminit.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)
