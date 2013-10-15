test:
	gcc -o test/main -g -Wall lib/json.c src/stream.c test/main.c -lhiredis -levent -lm

fpga:
	g++ -o fpga/main -g -Wall lib/json.c src/stream.c fpga/udpio.cc fpga/main.cc -lhiredis -levent

.PHONY: test fpga