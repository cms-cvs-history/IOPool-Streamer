/** Sample code to Read Streammer and recover file without bad events
    This is meant to debug streamer data files.

   compares_bad:
       Compares the streamer header info for two events and return true
       if any header information that should be the same is different

  uncompressBuffer:
       Tries to uncompress the event data blob if it was compressed
       and return true if successful (or was not compressed)

  readfile:
       Reads a streamer file, dumps the headers for the INIT message
       and the first event, and then looks to see if there are any
       events with streamer header problems or uncompress problems
       optionally writes a streamer file without bad events

  main():

      Code entry point, comment the function call that you don't want to make.

 $Id: DiagStreamerFile.cpp,v 1.1 2008/10/31 23:44:18 hcheung Exp $

*/

#include <iostream>
#include "zlib.h"
#include "IOPool/Streamer/interface/MsgTools.h"
#include "IOPool/Streamer/interface/InitMessage.h"
#include "IOPool/Streamer/interface/EventMessage.h"
#include "IOPool/Streamer/interface/DumpTools.h"

#include "IOPool/Streamer/interface/StreamerInputFile.h"
#include "IOPool/Streamer/interface/StreamerOutputFile.h"

#include "FWCore/Utilities/interface/Exception.h"

bool compares_bad(const EventMsgView* eview1, const EventMsgView* eview2);
bool uncompressBuffer(unsigned char *inputBuffer,
                              unsigned int inputSize,
                              std::vector<unsigned char> &outputBuffer,
                              unsigned int expectedFullSize);
bool test_uncompress(const EventMsgView* eview, std::vector<unsigned char> &dest);
void readfile(std::string filename, std::string outfile);
void help();
void updateHLTStats(std::vector<uint8> const& packedHlt, uint32 hltcount, std::vector<uint32> &hltStats);

//==========================================================================
int main(int argc, char* argv[]){

  if (argc < 2) {
    std::cout << "No command line argument supplied\n";
    help();
    return 0;
  }

  std::string streamfile(argv[1]);
  std::string outfile("/dev/null");
  if(argc == 3) {
    outfile = argv[2];
  }

  readfile(streamfile, outfile);
  std::cout <<"\n\nDiagStreamerFile TEST DONE\n"<< std::endl;

  return 0;
}

//==========================================================================
void help() {
      std::cout << "Usage: DiagStreamerFile streamer_file_name" 
                << " [output_file_name]" << std::endl;
}

//==========================================================================
void readfile(std::string filename, std::string outfile) {

  int num_events(0);
  int num_badevents(0);
  int num_baduncompress(0);
  int num_goodevents(0);
  int num_duplevents(0);
  uint32 hltcount(0);
  std::vector<uint32> hltStats(0);
  std::vector<unsigned char> compress_buffer(7000000);
  std::map<uint32, uint32> seenEventMap;
  bool output(false);
  if(outfile != "/dev/null") {
    output = true;
  }
  StreamerOutputFile stream_output(outfile);
  try{
    // ----------- init
    StreamerInputFile stream_reader (filename);
    //if(output) StreamerOutputFile stream_output(outfile);

    std::cout << "Trying to Read The Init message from Streamer File: " << std::endl
         << filename << std::endl;
    const InitMsgView* init = stream_reader.startMessage();
    std::cout<<"\n\n-------------INIT Message---------------------"<< std::endl;
    std::cout<<"Dump the Init Message from Streamer:-"<< std::endl;
    dumpInitView(init);
    if(output) {
      stream_output.write(*init);
      hltcount = init->get_hlt_bit_cnt();
      //Initialize the HLT Stat vector with all ZEROs
      for(uint32 i = 0; i != hltcount; ++i)
        hltStats.push_back(0);
    }

    // ------- event
    std::cout<<"\n\n-------------EVENT Messages-------------------"<< std::endl;

    bool first_event(true);
    EventMsgView* firstEvtView(0);
    const EventMsgView* eview(0);
    seenEventMap.clear();
  
    while(stream_reader.next()) {
      eview = stream_reader.currentRecord();
      ++num_events;
      bool good_event(true);
      if(seenEventMap.find(eview->event()) == seenEventMap.end()) {
         seenEventMap.insert(std::make_pair(eview->event(), 1));
      } else {
         ++seenEventMap[eview->event()];
         ++num_duplevents;
         std::cout << "??????? duplicate event Id for count " << num_events
                    << " event number " << eview->event()
                    << " seen " << seenEventMap[eview->event()] << " times" << std::endl;
      }
      if(first_event) {
        std::cout<<"----------dumping first EVENT-----------"<< std::endl;
        dumpEventView(eview);
        first_event = false;
        std::vector<unsigned char> *savebuf(new std::vector<unsigned char>(0));
        unsigned char* src = (unsigned char*)eview->startAddress();
        unsigned int srcSize = eview->size();
        savebuf->resize(srcSize);
        std::copy(src, src+srcSize, &(*savebuf)[0]);
        firstEvtView = new EventMsgView(&(*savebuf)[0]);
        //firstEvtView = new EventMsgView((void*)eview->startAddress());
        if(!test_uncompress(firstEvtView, compress_buffer)) {
          std::cout << "uncompress error for count " << num_events 
                    << " event number " << firstEvtView->event() << std::endl;
          ++num_baduncompress;
          std::cout<<"----------dumping bad uncompress EVENT-----------"<< std::endl;
          dumpEventView(firstEvtView);
          good_event=false;
        }
      } else {
        if(compares_bad(firstEvtView, eview)) {
          std::cout << "Bad event at count " << num_events << " dumping event " << std::endl
                    << "----------dumping bad EVENT-----------"<< std::endl;
          dumpEventView(eview);
          ++num_badevents;
          good_event=false;
        }
        if(!test_uncompress(eview, compress_buffer)) {
          std::cout << "uncompress error for count " << num_events 
                    << " event number " << eview->event() << std::endl;
          ++num_baduncompress;
          std::cout<<"----------dumping bad uncompress EVENT-----------"<< std::endl;
          dumpEventView(eview);
          good_event=false;
        }
      }
      if(output && good_event) {
        ++num_goodevents;
        stream_output.write(*eview);
        //get the HLT Packed bytes
        std::vector<uint8> packedHlt;
        uint32 hlt_sz = 0;
        if (hltcount != 0) hlt_sz = 1 + ((hltcount-1)/4); 
        packedHlt.resize(hlt_sz);
        firstEvtView->hltTriggerBits(&packedHlt[0]);
        updateHLTStats(packedHlt, hltcount, hltStats);
      }
      if((num_events % 1000) == 0) {
        std::cout << "Read " << num_events << " events, and "
                  << num_badevents << " events with bad headers, and "
                  << num_baduncompress << " events with bad uncompress" << std::endl;
        if(output) std::cout << "Wrote " << num_goodevents << " good events " << std::endl;
      }
    }
    delete firstEvtView;
    std::cout << std::endl << "------------END--------------" << std::endl
              << "read " << num_events << " events" << std::endl
              << "and " << num_badevents << " events with bad headers" << std::endl
              << "and " << num_baduncompress << " events with bad uncompress" << std::endl
              << "and " << num_duplevents << " duplicated event Id" << std::endl;
    if(output) {
      uint32 dummyStatusCode = 1234;
      stream_output.writeEOF(dummyStatusCode, hltStats);
      std::cout << "Wrote " << num_goodevents << " good events " << std::endl;
    }

  }catch (cms::Exception& e){
     std::cerr << "Exception caught:  "
               << e.what() << std::endl
               << "After reading " << num_events << " events, and "
               << num_badevents << " events with bad headers" << std::endl
               << "and " << num_baduncompress << " events with bad uncompress"  << std::endl
               << "and " << num_duplevents << " duplicated event Id" << std::endl;
  }
}

//==========================================================================
bool compares_bad(const EventMsgView* eview1, const EventMsgView* eview2) {
  bool is_bad(false);
  if(eview1->code() != eview2->code()) {
    std::cout << "non-matching EVENT message code " << std::endl;
    is_bad = true;
  }
  if(eview1->protocolVersion() != eview2->protocolVersion()) {
    std::cout << "non-matching EVENT message protocol version" << std::endl;
    is_bad = true;
  }
  if(eview1->run() != eview2->run()) {
    std::cout << "non-matching run number " << std::endl;
    is_bad = true;
  }
  if(eview1->lumi() != eview2->lumi()) {
    std::cout << "non-matching lumi number" << std::endl;
    is_bad = true;
  }
  if(eview1->outModId() != eview2->outModId()) {
    std::cout << "non-matching output module id" << std::endl;
    is_bad = true;
  }
  if(eview1->hltCount() != eview2->hltCount()) {
    std::cout << "non-matching HLT count" << std::endl;
    is_bad = true;
  }
  if(eview1->l1Count() != eview2->l1Count()) {
    std::cout << "non-matching L1 count" << std::endl;
    is_bad = true;
  }
  return is_bad;
}

//==========================================================================
bool test_uncompress(const EventMsgView* eview, std::vector<unsigned char> &dest) {
  unsigned long origsize = eview->origDataSize();
  bool success = false;
  if(origsize != 0)
  {
    // compressed
    success = uncompressBuffer((unsigned char*)eview->eventData(),
                                   eview->eventLength(), dest, origsize);
  } else {
    // uncompressed anyway
    success = true;
  }
  return success;
}

//==========================================================================
bool uncompressBuffer(unsigned char *inputBuffer,
                              unsigned int inputSize,
                              std::vector<unsigned char> &outputBuffer,
                              unsigned int expectedFullSize)
  {
    unsigned long origSize = expectedFullSize;
    unsigned long uncompressedSize = expectedFullSize;
    outputBuffer.resize(origSize);
    int ret = uncompress(&outputBuffer[0], &uncompressedSize,
                         inputBuffer, inputSize);
    if(ret == Z_OK) {
        // check the length against original uncompressed length
        if(origSize != uncompressedSize) {
            std::cout << "Problem with uncompress, original size = "
                 << origSize << " uncompress size = " << uncompressedSize << std::endl;
            return false;
        }
    } else {
        std::cout << "Problem with uncompress, return value = "
             << ret << std::endl;
        return false;
    }
    return true;
}

//==========================================================================
void updateHLTStats(std::vector<uint8> const& packedHlt, uint32 hltcount, std::vector<uint32> &hltStats)
{
  unsigned int packInOneByte = 4;
  unsigned char testAgaint = 0x01;
  for(unsigned int i = 0; i != hltcount; ++i)
  {
    unsigned int whichByte = i/packInOneByte;
    unsigned int indxWithinByte = i % packInOneByte;
    if ((testAgaint << (2 * indxWithinByte)) & (packedHlt.at(whichByte))) {
        ++hltStats[i];
    }
  }
}

