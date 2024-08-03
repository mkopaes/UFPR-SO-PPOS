####### PARÂMETROS
TARGET 		= program

# Opções do compilador
CC 			= gcc
FLAGS 		= -Wall
LIB_LINKS	= -lm -lrt

####### EXECUÇÕES
all: $(TARGET)

$(TARGET):
	$(CC) $(FLAGS) src/*.c disk/*.c *.c $(LIB_LINKS) -o $@

purge:
	@rm $(TARGET)