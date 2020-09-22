NAME = ssu_shell
SRCS = ssu_shell.c
OBJS = $(SRCS:.c=.o)
LDFLAGS=-lncurses

OUTPUT=$(NAME)


all: $(NAME)

CC=gcc -g
lCC=cc -g

.PHONY = all clean fclean re

$(NAME):$(OBJS)
	$(lCC) -o $@ $< $(LDFLAGS)


clean:
	rm -rf $(OBJS)

fclean:
	rm -rf $(NAME)

re: fclean all
