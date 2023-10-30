all: kbinsert kbinsertx

kbinsert: kbinsert.c
	gcc -o kbinsert kbinsert.c

kbinsertx: kbinsert.c
	gcc -DGUI_SUPPORT -o kbinsertx kbinsert.c

debug:
	gcc -ggdb3 -o kbinsert kbinsert.c

run_debug: debug
	gdb ./kbinsert

vi:
	vim README.md Makefile kbinsert.c
