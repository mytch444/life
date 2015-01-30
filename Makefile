EXEC=life

all: life

life: life.c
	gcc -o ${EXEC} life.c -lX11 -lpthread

clean:
	rm ${EXEC}
