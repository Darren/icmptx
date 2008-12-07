flags=-g3 -Wall -ansi -pedantic

all: icmptx

icmptx: it.o icmptx.o tun_dev.o
	gcc $(flags) -o icmptx icmptx.o it.o tun_dev.o

it.o: it.c driver.h
	gcc $(flags) -c it.c

icmptx.o: icmptx.c driver.h
	gcc $(flags) -c icmptx.c

tun_dev.o: tun_dev.c vtun.h llist.h
	gcc $(flags) -c tun_dev.c

clean:
	rm -f tun_dev.o it.o icmptx.o icmptx