/*
  g++ -g -o FirmwareReceiver -std=c++11 -ISource -IJuceLibraryCode Source/FirmwareReceiver.cpp Source/sysex.c Source/crc32.c JuceLibraryCode/modules/juce_core/juce_core.cpp JuceLibraryCode/modules/juce_audio_basics/juce_audio_basics.cpp JuceLibraryCode/modules/juce_audio_devices/juce_audio_devices.cpp JuceLibraryCode/modules/juce_events/juce_events.cpp -lpthread -ldl -lX11 -lasound
*/
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include "JuceHeader.h"
#include "OpenWareMidiControl.h"
#include "crc32.h"
#include "sysex.h"
#include "MidiStatus.h"

#define MAX_SYSEX_PAYLOAD_SIZE (8*1024*1024)
#define NO_ERROR         0x00
#define HARDFAULT_ERROR  0x10
#define BUS_ERROR        0x20
#define MEM_ERROR        0x30
#define NMI_ERROR        0x40
#define USAGE_ERROR      0x50
#define PROGRAM_ERROR    0x60
#define CONFIG_ERROR     0x70
#define FLASH_ERROR      0x80
#define USB_ERROR        0x90
#define RUNTIME_ERROR    0xa0

static int8_t errorcode;

void error(int8_t code, const char* reason){
  printf("%s\n", reason);
  errorcode = -1;
  // exit(-1);
}

void setErrorStatus(int8_t err){
  errorcode = err;
}

#define USE_EXTERNAL_RAM
uint8_t rx_buffer[1024*1024];
#define _EXTRAM rx_buffer
#define MAX_SYSEX_FIRMWARE_SIZE (80*1024)

#include "FirmwareLoader.hpp"

#define MESSAGE_SIZE 8
#define DEFAULT_BLOCK_SIZE (248-MESSAGE_SIZE)
#define DEFAULT_BLOCK_DELAY 20 // wait in milliseconds between sysex messages

bool quiet = false;

class CommandLineException : public std::exception {
private:
  juce::String cause;
public:
  CommandLineException(juce::String c) : cause(c) {}
  juce::String getCause() const {
    return cause;
  }
  const char* what() const noexcept {
    return getCause().toUTF8();
  }
};

class FirmwareReceiver : public juce::MidiInputCallback {
private:
  bool running = false;
  bool verbose = false;
  juce::ScopedPointer<MidiInput> midiin;
  juce::ScopedPointer<File> fileout;
  juce::ScopedPointer<OutputStream> out;
  FirmwareLoader loader;
public:
  void listDevices(const StringArray& names){
    for(int i=0; i<names.size(); ++i)
      std::cout << i << ": " << names[i] << std::endl;
  }

  void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message){
    // if(verbose)
    //   std::cout << "rx message " << message.getRawDataSize() << " bytes." << std::endl;
    uint8_t* data = (uint8_t*)message.getRawData();
    uint16_t size = message.getRawDataSize();
    data += 1;
    size -= 2;
    if(size > 3 && 
       data[0] == MIDI_SYSEX_MANUFACTURER || 
       data[1] == MIDI_SYSEX_OWL_DEVICE) {
      if(data[2] == SYSEX_FIRMWARE_UPLOAD){
	
	int32_t ret = loader.handleFirmwareUpload(data, size);
	if(ret < 0){
	  std::cerr << "receive error: " << ret << std::endl;
	}else if(ret > 0){
	  std::cout << "receive complete: " << ret << " bytes. " << std::endl;
	  out->write(loader.getData(), loader.getSize());
	  out->flush();
	  if(verbose)
	    std::cout << "crc32: 0x" << std::hex << loader.getChecksum() << std::endl;
	  shutdown();
	}else{
	  std::cout << '.';
	}
      }
    }else{
      std::cout << "rx unknown or invalid SysEx message" << std::endl;
    }
  }

  void handlePartialSysexMessage(MidiInput *source, const uint8 *messageData, int numBytesSoFar, double timestamp){
    std::cout << "rx partial sysex " << numBytesSoFar << " bytes." << std::endl;
  }

  MidiInput* openMidiInput(const String& name){
    MidiInput* input = NULL;    
    StringArray inputs = MidiInput::getDevices();
    for(int i=0; i<inputs.size(); ++i){
      if(inputs[i].trim().matchesWildcard(name, true)){
	if(verbose)
	  std::cout << "opening MIDI input " << inputs[i] << std::endl;
	input = MidiInput::openDevice(i, this);
	break;
      }
    }
    return input;
  }

  void usage(){
    std::cerr << getApplicationName() << std::endl 
	      << "usage:" << std::endl
	      << "-h or --help\tprint this usage information and exit" << std::endl
	      << "-l or --list\tlist available MIDI ports and exit" << std::endl
	      << "-in DEVICE\tconnect to MIDI input DEVICE" << std::endl
	      << "-c DEVICE\tcreate MIDI input DEVICE" << std::endl
	      << "-save FILE\twrite data to FILE" << std::endl
	      << "-q or --quiet\treduce status output" << std::endl
	      << "-v or --verbose\tincrease status output" << std::endl
      ;
  }

  void configure(int argc, char* argv[]) {
    for(int i=1; i<argc; ++i){
      juce::String arg = juce::String(argv[i]);
      if(arg.compare("-h") == 0 || arg.compare("--help") == 0 ){
	usage();
	throw CommandLineException(juce::String::empty);
      }else if(arg.compare("-q") == 0 || arg.compare("--quiet") == 0 ){
	quiet = true;
      }else if(arg.compare("-v") == 0 || arg.compare("--verbose") == 0 ){
	verbose = true;
      }else if(arg.compare("-l") == 0 || arg.compare("--list") == 0){
	std::cout << "MIDI input devices:" << std::endl;
	listDevices(MidiInput::getDevices());
	std::cout << "MIDI outputput devices:" << std::endl;
	listDevices(MidiOutput::getDevices());
	throw CommandLineException(juce::String::empty);
      }else if(arg.compare("-in") == 0 && ++i < argc){
	juce::String name = juce::String(argv[i]);
	midiin = openMidiInput(name);
      }else if(arg.compare("-c") == 0 && ++i < argc){
	juce::String name = juce::String(argv[i]);
	midiin = MidiInput::createNewDevice(name, this);
      }else if(arg.compare("-save") == 0 && ++i < argc){
	juce::String name = juce::String(argv[i]);
	fileout = new juce::File(name);
	fileout->deleteFile();
	fileout->create();
      }else{
	usage();
	throw CommandLineException(juce::String::empty);
      }
    }
    if(midiin == NULL || fileout == NULL){
      usage();
      throw CommandLineException(juce::String::empty);
    }
  }

  void run(){
    running = true;
    if(!quiet){
      std::cout << "Receiving to file " << fileout->getFileName() << std::endl; 
      if(midiin != NULL)
	std::cout << "\tfrom MIDI input" << std::endl; 
      // if(filein != NULL)
      // 	std::cout << "\tfrom SysEx file " << filein->getFullPathName() << std::endl;       
    }
    if(fileout != NULL)
      out = fileout->createOutputStream();
    midiin->start();
    while(running);
    if(out != NULL)
      out->flush();
    if(midiin != NULL)
      midiin->stop();
  }

  uint32_t decodeInt(MemoryBlock& block){
    return 0;
    // uint8_t in[4];
    // uint8_t out[5];
    // in[3] = (uint8_t)data & 0xff;
    // in[2] = (uint8_t)(data >> 8) & 0xff;
    // in[1] = (uint8_t)(data >> 16) & 0xff;
    // in[0] = (uint8_t)(data >> 24) & 0xff;
    // int len = data_to_sysex(in, out, 4);
    // if(len != 5)
    //   throw CommandLineException("Error in sysex conversion"); 
    // block.append(out, len);
  }

  void shutdown(){
    running = false;
  }

  juce::String getApplicationName(){
    return "FirmwareReceiver";
  }
};

FirmwareReceiver* app = NULL;

#ifndef _WIN32
void sigfun(int sig){
  if(!quiet)
    std::cout << "shutting down" << std::endl;
  if(app != NULL)
    app->shutdown();
  (void)signal(SIGINT, SIG_DFL);
}
#endif

int main(int argc, char* argv[]) {
#ifndef _WIN32
  (void)signal(SIGINT, sigfun);
#endif
  int status = 0;
  app = new FirmwareReceiver();
  try{
    app->configure(argc, argv);
    app->run();
  }catch(const std::exception& exc){
    std::cerr << exc.what() << std::endl;
    status = -1;
  }
  delete app;
  return status;
}
