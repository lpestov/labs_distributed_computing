CXX := c++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -pthread

.PHONY: all clean

all: task1_mandelbrot task2_nbody

task1_mandelbrot: task1_mandelbrot.cpp timer.h
	$(CXX) $(CXXFLAGS) task1_mandelbrot.cpp -o task1_mandelbrot

task2_nbody: task2_nbody.cpp timer.h
	$(CXX) $(CXXFLAGS) task2_nbody.cpp -o task2_nbody

clean:
	rm -f task1_mandelbrot task2_nbody mandelbrot.csv nbody.csv
