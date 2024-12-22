all: catalyst

catalyst: catalyst.C
	g++ -Wall -o catalyst catalyst.C
