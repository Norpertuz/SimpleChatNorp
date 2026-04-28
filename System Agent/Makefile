# nazwa programu
TARGET_LINUX = chat_agent
TARGET_WIN = chat_agent.exe

SRC = chat_agent.c

CC = gcc
CFLAGS = -Wall -O2
LIBS = -lcurl -lcjson

# wykrycie systemu
ifeq ($(OS),Windows_NT)
    TARGET = $(TARGET_WIN)
    EXE_EXT = .exe
    EXTRA_LIBS = -lws2_32
    RUN = $(TARGET)
else
    TARGET = $(TARGET_LINUX)
    EXE_EXT =
    EXTRA_LIBS =
    RUN = ./$(TARGET)
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS) $(EXTRA_LIBS)

run: $(TARGET)
	$(RUN)

clean:
	rm -f chat_agent chat_agent.exe *.o