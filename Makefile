test:
	gcc -o test/main -g -Wall lib/json.c src/stream.c test/main.c -lhiredis -levent -lm

.PHONY: test