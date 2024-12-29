all: wait_random_busy

wait_random_busy: wait_random_busy.C
	g++ -Wall -o wait_random_busy wait_random_busy.C

build: wait_random_busy

clean:
	rm -f wait_random_busy success.txt
