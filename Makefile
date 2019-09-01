all: s2

s2:
	gcc -shared -fPIC s2.c -o s2.so
