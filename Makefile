# Define the compiler
CC = gcc

# Define compiler and linker flags
PYTHON_INCLUDE = "C:/Users/dnxjc/AppData/Local/Programs/Python/Python310/include"
LDFLAGS = -L"C:/Users/dnxjc/AppData/Local/Programs/Python/Python310/libs" -lpython310
NUMPY_INCLUDE = "C:/Users/dnxjc/AppData/Local/Programs/Python/Python310/Lib/site-packages/numpy/core/include"
CFLAGS = -I$(PYTHON_INCLUDE) -I$(NUMPY_INCLUDE) -Wall -g

# Define the output file names
EXE_TARGET = myprogram
PYD_TARGET = src/hll_module.pyd

# Define the source files for each target
EXE_SRCS = src/hll.c src/hll_example.c lib/murmur2.c
PYD_SRCS = src/py_hll_example.c src/hll.c src/hll_example.c lib/murmur2.c src/py_hyperanf.c

# Define the object files for each target
EXE_OBJS = $(EXE_SRCS:.c=.o)
PYD_OBJS = $(PYD_SRCS:.c=.o)

# Default target
all: $(EXE_TARGET) $(PYD_TARGET)

# Rule to build the executable
$(EXE_TARGET): $(EXE_OBJS)
	$(CC) $(EXE_OBJS) -o $(EXE_TARGET) $(LDFLAGS)

# Rule to build the shared library (.pyd) file for Python
$(PYD_TARGET): $(PYD_OBJS)
	$(CC) $(PYD_OBJS) -shared -o $(PYD_TARGET) $(LDFLAGS)

# Rule to compile .c files into .o object files
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

# Clean target to remove all built files
clean:
	if exist src\hll.o del /Q src\hll.o
	if exist src\hll_example.o del /Q src\hll_example.o
	if exist lib\murmur2.o del /Q lib\murmur2.o
	if exist src\py_hll_example.o del /Q src\py_hll_example.o
	if exist myprogram.exe del /Q myprogram.exe
	if exist hll_module.pyd del /Q hll_module.pyd