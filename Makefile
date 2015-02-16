# Paths 
BUILDROOT = .
TARGET = FirmwareSender
BUILD = $(BUILDROOT)/Build
SOURCE = $(BUILDROOT)/Source
BIN = $(BUILDROOT)/bin
JUCE = /home/mars/devel/juce

# Tools
CC  = gcc
CXX = g++
LD  = g++

# Flags
CFLAGS   = -g
CFLAGS  += -I$(SOURCE) -I$(JUCE)/
CXXFLAGS = -std=c++11 $(CFLAGS) 
CFLAGS  += -std=gnu99
LDLIBS   = -lm -lpthread -ldl -lX11 -lasound

C_SRC    = sysex.c crc32.c
CPP_SRC  = FirmwareSender.cpp 
CPP_SRC += juce_core.cpp juce_audio_basics.cpp juce_audio_devices.cpp juce_events.cpp

OBJS = $(C_SRC:%.c=Build/%.o) $(CPP_SRC:%.cpp=Build/%.o)

# Set up search path
vpath %.cpp $(JUCE)/modules/juce_core
vpath %.cpp $(JUCE)/modules/juce_audio_basics
vpath %.cpp $(JUCE)/modules/juce_audio_devices
vpath %.cpp $(JUCE)/modules/juce_events
vpath %.cpp $(SOURCE)
vpath %.c   $(SOURCE)

all: $(TARGET)

# Build executable 
$(TARGET) : $(OBJS)
	$(LD) $(LDFLAGS) -o $(BIN)/$@ $(OBJS) $(LDLIBS)

# compile and generate dependency info
$(BUILD)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
	$(CC) -MM -MT"$@" $(CFLAGS) $< > $(@:.o=.d)

$(BUILD)/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
	$(CXX) -MM -MT"$@" $(CXXFLAGS) $< > $(@:.o=.d)

$(BUILD)/%.o: %.s
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.s: %.c
	$(CC) -S $(CFLAGS) $< -o $@

$(BUILD)/%.s: %.cpp
	$(CXX) -S $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(BUILD)/*.d $(TARGET) $(OBJS:.o=.s) 

map : $(OBJS) $(LDSCRIPT) $(TANN_LIB)
	$(LD) $(LDFLAGS) -Wl,-Map=$(ELF:.elf=.map) $(OBJS) $(LDLIBS)

# pull in dependencies
-include $(OBJS:.o=.d)
