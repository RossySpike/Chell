make:
	gcc -O0 -g main.c -o main.debug.out && /bin/cp main.debug.out test/
debug:
	valgrind -s --leak-check=full --show-leak-kinds=all ./main.debug.out
gdb:
	make
	gdb -tui ./main.debug.out
