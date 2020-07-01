INCLUDES        = -I. -I/usr/include

LIBS		= libsocklib.a  \
			-ldl -lpthread -lm

COMPILE_FLAGS   = ${INCLUDES} -c
COMPILE         = gcc ${COMPILE_FLAGS}
LINK            = gcc -o

C_SRCS		= \
		producers.c \
		consumers.c \
		passivesock.c \
		connectsock.c \
		prodcon_server.c \
		status.c \
		status_value.c

SOURCE          = ${C_SRCS}

OBJS            = ${SOURCE:.c=.o}

EXEC		= producers consumers pcserver status status_value

.SUFFIXES       :       .o .c .h

all		:	library producers consumers pcserver status status_value

.c.o            :	${SOURCE}
			@echo "    Compiling $< . . .  "
			@${COMPILE} $<

library		:	passivesock.o connectsock.o
			ar rv libsocklib.a passivesock.o connectsock.o

pcserver	:	prodcon_server.o
			${LINK} $@ prodcon_server.o ${LIBS}

producers	:	producers.o
			${LINK} $@ producers.o ${LIBS}

consumers	:	consumers.o
			${LINK} $@ consumers.o ${LIBS}

status		: 	status.o
			${LINK} $@ status.o ${LIBS}

status_value		: 	status_value.o
			${LINK} $@ status_value.o ${LIBS}

clean           :
			@echo "    Cleaning ..."
			rm -f tags core *.out *.o *.lis *.a ${EXEC} libsocklib.a
