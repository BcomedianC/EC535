ktimer : ktimer.o
	arm-linux-gnueabihf-gcc -static -o ktimer ktimer.o

ktimer.o : ktimer.c
	arm-linux-gnueabihf-gcc -static -c ktimer.c

clean :
	rm ktimer ktimer.o