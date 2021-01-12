all: project4

project4: project4.cpp
	g++ -std=c++14 -o project4 project4.cpp

clean:
	rm -rf project4
