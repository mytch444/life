all: life life2

life: life.c
	gcc -o life life.c -lX11 -lpthread

life2: life2.c
	gcc -o life2 life2.c -lX11 -lm

clean:
	rm life
	rm life2
