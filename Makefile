CFLAGS=-Wall -Wextra -pedantic -g

lisp: main.c mpc.c mpc.h
        gcc  $(CFLAGS) -o lisp main.c mpc.c mpc.h