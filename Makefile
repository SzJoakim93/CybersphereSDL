#OBJS specifies which files to compile as part of the project
OBJS = src/main.c \
	src/exe.c \
	src/assets.c \
	src/gfx.c \
	src/effects.c \
	src/video.c \
	src/game.c \
	src/ball.c \
	src/brick.c \
	src/bonus.c \
	src/player.c \
	src/screens.c \
	src/menu.c \
	src/sound.c \
	src/third_party/nuked_opl3/opl3.c

#CC specifies which compiler we're using
CC = gcc

#COMPILER_FLAGS specifies the additional compilation options we're using
# -w suppresses all warnings
COMPILER_FLAGS = -w -g -std=c99 -DUSE_SDL=1

#LINKER_FLAGS specifies the libraries we're linking against

ifeq ($(OS),Windows_NT)
	LINKER_FLAGS = -mwindows -lMingw32 -lSDLmain -lSDL
	RESOURCE = bin/icon.o
else
	LINKER_FLAGS = -lSDL
endif

# Directory to be created
OUT_DIR := bin

#Create directory if it doesn't exist
$(OUT_DIR):
	@echo "Checking for directory: $(OUT_DIR)"
	@mkdir -p $(OUT_DIR)
	@echo "Directory ready: $(OUT_DIR)"

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = Cybersphere-SDL

#This is the target that compiles our executable
.DEFAULT_GOAL := all
.PHONY: all
all : $(OUT_DIR) $(OBJS) $(RESOURCE)
	$(CC) $(OBJS) $(RESOURCE) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OUT_DIR)/$(OBJ_NAME)

ifeq ($(OS),Windows_NT)
bin/icon.o: src/icon.rc src/icon.ico | $(OUT_DIR)
	windres -i $< -I src -O coff -o $@
endif