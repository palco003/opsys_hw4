testmem: mem.o libmem.so
	gcc testmem.c -lmem -L. -o testmem

mem.o: mem.c
	gcc -c -fpic -c mem.c -o mem.o

libmem.so: mem.o
	gcc -shared -o libmem.so mem.o

.PHONY: clean
clean:
	rm -f mem.o libmem.so testmem
