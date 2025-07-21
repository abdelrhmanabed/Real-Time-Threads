# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Output binaries
BINS = main generator calculator mover inspector1 inspector2 inspector3 opengl

# Default target: all
all: $(BINS)

# Compile main
main: main.c
	$(CC) $(CFLAGS) main.c -o main

# Compile generator
generator: generator.c
	$(CC) $(CFLAGS) generator.c -o generator

# Compile calculator
calculator: calculator.c
	$(CC) $(CFLAGS) calculator.c -o calculator

# Compile mover
mover: mover.c
	$(CC) $(CFLAGS) mover.c -o mover

# Compile inspector1
inspector1: inspector1.c
	$(CC) $(CFLAGS) inspector1.c -o inspector1

# Compile inspector2
inspector2: inspector2.c
	$(CC) $(CFLAGS) inspector2.c -o inspector2

# Compile inspector3
inspector3: inspector3.c
	$(CC) $(CFLAGS) inspector3.c -o inspector3

# Compile opengl
opengl: opengl.c
	$(CC) $(CFLAGS) opengl.c -o opengl -lGL -lGLU -lglut

# Run main with data.txt argument
run: main
	./main data.txt

# Clean up generated files
clean:
	rm -f $(BINS)

# Phony targets (not actual files)
.PHONY: all clean run
