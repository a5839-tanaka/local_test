TARGET_PROC = embaselib.a

TARGET = ../obj/$(TARGET_PROC)
OBJ_DIR = ../obj/

SRCS = \
		embase.c

OBJS = \
		$(OBJ_DIR)embase.o 

INCS = \
		../../inc/embase.h

CC := gcc
AR := ar
CFLAGS := 
RM := \rm
CP := \cp -p 

INC_PATH = \
	-I../inc \
	-I../../inc

$(TARGET) : $(OBJS) $(INCS)
	@echo "PVSYSTEM COMMON LIBRARY ($(TARGET)) ARCHIVES"
	@echo "================================================================"
	@$(AR) -tv $@
	@echo "================================================================"

$(OBJ_DIR)embase.o : embase.c $(INCS)
	@echo "Compile : $<"
	@$(CC) $(CFLAGS) $(INC_PATH) -c $< -o $@
	@$(AR) -rusv $(TARGET) $@

clean :
	@echo "Remove : $(TARGET) $(OBJS)"
	@$(RM) -f $(TARGET) $(OBJS)

