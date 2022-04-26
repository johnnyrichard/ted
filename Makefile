ted: ted.c string_builder.o
		$(CC) ted.c string_builder.o -o ted -ggdb -Wall -Wextra -pedantic -std=c11
