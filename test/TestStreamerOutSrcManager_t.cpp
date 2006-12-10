/** Example code shows you to write Streamer files.
    Using StreamerOutSrvcManager

    All values are dummy here, The Init message contains different 
    values from what Event Header contains, this is only 
    for the demonstration, obviously.

    Change total number of written events using,

     #define NO_OF_EVENTS 10

    Crank it up to 10000 if you like to scrol your screen ;-).

Disclaimer: Most of the code here is randomly written during
               testing various parts, its not a supported testing code.
               Changes can and will be made, when and if required.

>>>>>>>>>>>
Mind that config lines has no affect beside they provide OutputModules 
for StreamerOutSrvcManager

Theses lines below actauly controls Selection of Path in output files

  //uint8 hltbits[] = "U";  //( 0 1 0 1 0 1 0 1)  select all paths, and BOTH files will have NO_OF_EVENTS
  uint8 hltbits[] = "1";  //( 0 0 0 0 0 0 0 1)  select just p1, one file will have NO_OF_EVENTS, other will have none.


*/

#include <iostream>
#include "IOPool/Streamer/interface/MsgTools.h"
#include "IOPool/Streamer/interface/EventMsgBuilder.h"
#include "IOPool/Streamer/interface/InitMsgBuilder.h"
#include "IOPool/Streamer/interface/InitMessage.h"
#include "IOPool/Streamer/interface/EventMessage.h"

#include "IOPool/Streamer/interface/StreamerOutSrvcManager.h"
#include "FWCore/Utilities/interface/Exception.h"

#define NO_OF_EVENTS 10

using namespace std;

typedef vector<uint8> Buffer;

int main()
{
  try { 
  string config = "process PROD  ="
     "{"
        "source = EmptySource"
        "{"
          "untracked int32 maxEvents = 200"
        "}"
        "module m1 = StreamThingProducer"
        "{"
         "int32 array_size = 2 # 20000"
         "int32 instance_count=5"
        "}"
        "module a1 = StreamThingAnalyzer { string product_to_get='m1' }"

        "module pre1 = Prescaler { int32 prescaleFactor = 5 }"
        "module pre2 = Prescaler { int32 prescaleFactor = 2 }"
        "module pre3 = Prescaler { int32 prescaleFactor = 4 }"

        "module out1 = EventStreamFileWriter"
        "{"
          "int32 max_event_size = 7000000"
          "int32 max_queue_depth = 5"
          "bool use_compression = true"
          "int32 compression_level = 1"
          "string fileName = 'teststreamfile1.dat'"
          "string indexFileName = 'testindexfile1.ind'"
          "# untracked int32 numPerFile = 5"
          "untracked PSet SelectEvents = { vstring SelectEvents={'p1'}}"
        "}"

        "module out2 = EventStreamFileWriter"
        "{"
          "int32 max_event_size = 7000000"
          "int32 max_queue_depth = 5"
          "bool use_compression = true"
          "int32 compression_level = 1"
          "string fileName = 'teststreamfile2.dat'"
          "string indexFileName = 'testindexfile2.ind'"
          "# untracked int32 numPerFile = 5"
          "untracked PSet SelectEvents = { vstring SelectEvents={'p2'}}"
        "}"

        "path p1 = { m1, a1}"
        "path p2 = { m1, pre1 }"
        "path p3 = { m1, pre2 }"
        "path p4 = { m1, pre3 }"

        "#path p1 = { m1, a1 }"

        "endpath e1 = { out1 }"
        "endpath e2 = { out2 }"
     "}";

  //Instantiate StreamerOutSrvcManager and process config
  edm::StreamerOutSrvcManager outMgr(config);


  Buffer buf(1024);

  // ----------- init

  char psetid[] = "1234567890123456";
  char test_value[] = "This is a test, This is a";
  char test_value_event[] = "This is a test Event, This is a";
  Strings hlt_names;
  Strings l1_names;

  hlt_names.push_back("p1");  hlt_names.push_back("p2");
  hlt_names.push_back("p3");  hlt_names.push_back("p4");
    
  l1_names.push_back("t10");  l1_names.push_back("t11");
  l1_names.push_back("t12");  

  char reltag[]="CMSSW_0_6_0_pre45";

  InitMsgBuilder init(&buf[0],buf.size(),12,
                      Version(3,(const uint8*)psetid),
                      (const char*)reltag,
                      hlt_names,l1_names);

  init.setDescLength(sizeof(test_value));
  std::copy(&test_value[0],&test_value[0]+sizeof(test_value),
            init.dataAddress());
  
  //Lets make a view on this message
  InitMsgView init_message(&buf[0]); 
  //Do a dumpInit here if you need to see the event.    

  //Start the Streamer file
  cout<<"Trying to Write a Streamer file"<<endl; 
  unsigned long maxFileSize=1000000;
  uint32 runNum = 0;
  double highWaterMark=1.0;
  std::string path="/cmsonline/home/anzar/StorageManagerTest/out";
  std::string mpath="/cmsonline/home/anzar/StorageManagerTest/out";
  outMgr.manageInitMsg("teststreamer", runNum, maxFileSize, highWaterMark,
                  path, mpath, "fileCatalog.txt", 0, init_message);

  // ------- event

  std::vector<bool> l1bit(3);
  //uint8 hltbits[] = "U";  //( 0 1 0 1 0 1 0 1)  select all paths
  uint8 hltbits[] = "1";  //( 0 0 0 0 0 0 0 1)  select just p1 
  //const int hltsize = (sizeof(hltbits)-1)*4;
  const int hltsize = 4;  /** I am interested in 4 bits only */

  l1bit[0]=true;  
  l1bit[1]=true;  
  l1bit[2]=false;  

  //Lets Build 10 Events ad then Write them into Streamer/Index file.
  
  for (uint32 eventId = 2000; eventId != 2000+NO_OF_EVENTS; ++eventId) {
    EventMsgBuilder emb(&buf[0],buf.size(),45,eventId,2,
                      l1bit,hltbits,hltsize);            
    emb.setReserved(78);
    emb.setEventLength(sizeof(test_value_event));
    std::copy(&test_value_event[0],&test_value_event[0]+sizeof(test_value_event),
             emb.eventAddr());

    //Covert this to an EventView
    EventMsgView eview(&buf[0]);
 
    //Lets write this to our streamer file.
    outMgr.manageEventMsg(eview);

  }

  //Write the EOF Record Both at the end of Streamer file.
  outMgr.stop();
 } catch (cms::Exception & e) {
       std::cerr << "cms::Exception: " << e.explainSelf() << std::endl;
       std::cerr << "std::Exception: " << e.what() << std::endl;
     }
  return 0;
}


