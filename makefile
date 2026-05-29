compiler = g++

# Archivos
SRC = optimal.cpp
OBJ = objects/optimal.o
TARGET = optimal

all: $(TARGET)

#Ejecutable
$(TARGET): $(OBJ)
	$(compiler) $(OBJ) -o $(TARGET) -lncurses

#objetos
$(OBJ): $(SRC)
	@mkdir -p objects
	$(compiler) -c $(SRC) -o $(OBJ)

clean:
	rm -rf objects/*.o $(TARGET)
