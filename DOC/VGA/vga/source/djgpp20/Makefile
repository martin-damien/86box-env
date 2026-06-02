# Makefile for DJGPP 2.0

CC = gcc
FLAGS = -g

all: pixel.exe lines.exe rect.exe circle.exe bitmap.exe palette.exe mouse.exe unchain.exe modes.exe

pixel.exe: pixel.c
	$(CC) $(FLAGS) pixel.c -o pixel.exe

lines.exe: lines.c
	$(CC) $(FLAGS) lines.c -o lines.exe

rect.exe: rect.c
	$(CC) $(FLAGS) rect.c -o rect.exe

circle.exe: circle.c
	$(CC) $(FLAGS) circle.c -o circle.exe

bitmap.exe: bitmap.c
	$(CC) $(FLAGS) bitmap.c -o bitmap.exe

palette.exe: palette.c
	$(CC) $(FLAGS) palette.c -o palette.exe

mouse.exe: mouse.c
	$(CC) $(FLAGS) mouse.c -o mouse.exe

unchain.exe: unchain.c
	$(CC) $(FLAGS) unchain.c -o unchain.exe

modes.exe: modes.c
	$(CC) $(FLAGS) modes.c -o modes.exe

