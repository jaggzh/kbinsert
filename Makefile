all: kbinsert

kbinsert: kbinsert.c
	gcc -o kbinsert kbinsert.c

vi:
	vim README.md Makefile kbinsert.c
