all: main.c
	gcc -g -Wall -o PushToJack main.c -ljack

clean:
	$(RM) PushToJack
