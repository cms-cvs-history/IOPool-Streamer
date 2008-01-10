#include <exception>
#include <iostream>
#include <fstream>
#include <iosfwd>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <boost/program_options.hpp>
#include "IOPool/Streamer/interface/StreamerInputIndexFile.h"
#include "IOPool/Streamer/interface/EventMessage.h"
#include "IOPool/Streamer/interface/InitMessage.h"

using namespace boost::program_options;


// -----------------------------------------------



void convertTriggers(unsigned char c, std::vector<int> & results){
  /*
   * Util to convert an 8 bit word into 4x2 bit integer HLT Trigger Bits
   * c = AABBCCDD
   * results will get AA, BB, CC, DD converted to (0,1,2,3) and added to it.
   */

  for (int i = 3; i >= 0; --i) {
    const int shift = 2*i;
    const int bit1 = ((c >> (shift+1)) & 1);
    const int bit2 = ((c >> shift) & 1);
    const int trigVal = (2*bit1) + bit2;
   
    results.push_back(trigVal);
  }

}





int main(int argc, char* argv[]) {
  /*
   * Main program
   *
   */
  std::string kProgramName = argv[0];

  /*
   * Command line setup and parsing
   */
  options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("in,i", value<std::string>(), "input file");

  positional_options_description p;

  variables_map vm;

  try
    {
      store(command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
    }
  catch (boost::program_options::error const& x)
    {
      std::cerr << "Option parsing failure:\n"
		<< x.what() << '\n'
		<< "Try 'edmStreamerIndex -h' for help.\n";
      return 1;
    }

  notify(vm);    

  if (vm.count("help")) {
    std::cerr << desc << "\n";
    return 1;
  }
  
  if (!vm.count("in")) {
    std::cerr << "input not set.\n";
    return 1;
  }
  
  /*
   * Input Streamer Index file and output XML file
   */
  std::string in = vm["in"].as<std::string>(); 

  
  /*
   * First read in the streamer index file
   * and get the Init Message
   *
   */
  StreamerInputIndexFile* const indexFile = new StreamerInputIndexFile(in);
  const StartIndexRecord* const startindx = indexFile->startMessage();
  const InitMsgView* const start = startindx->getInit();

  /*
   * Extract the list of trigger names and the number of trigger bit entries 
   */
  std::vector<std::string> vhltnames;
  start->hltTriggerNames(vhltnames);
  int hltBitCount = start->get_hlt_bit_cnt();

  std::map<std::string, int> eventsPerTrigger;
  std::map<std::string, int> errorsPerTrigger;

  for (std::vector<std::string>::const_iterator it = vhltnames.begin(), itEnd = vhltnames.end(); it != itEnd; ++it) {
    eventsPerTrigger[*it] = 0;
    errorsPerTrigger[*it] = 0;
  }

  // int runNumber = 0;
  // int lumiNumber = 0;
  int eventCount = 0;

  /*
   * Process each event header in the index, by extracting the hlt trigger bits
   * HLT bits are 2 bits long
   *
   */
  for(indexRecIter it = indexFile->begin(), itEnd = indexFile->end(); it != itEnd; ++it) {
    
    const EventMsgView* const iview = (*it)->getEventView();
    // runNumber = iview->run();
    // lumiNumber = iview->lumi();
    eventCount++;
    /*
     * Extract the trigger bits
     */
    std::vector<unsigned char> hlt_out;
    hlt_out.resize(1 + (iview->hltCount()-1)/4);
    iview->hltTriggerBits(&hlt_out[0]);
    
    /*
     * Convert the trigger bits into 2 bit integers
     *
     */
    std::vector<int> triggerResults;
    for(int i=(hlt_out.size()-1); i != -1 ; --i) {
      convertTriggers(hlt_out[i], triggerResults);
    }
    
    std::reverse(triggerResults.begin(), triggerResults.end());
    /*
     * Walk through the trigger paths and trigger bits and record the states in the
     * maps available
     *
     */
    for (int i=0; i < hltBitCount; i++){
      std::string triggerName = vhltnames[i];
      const int triggerBit = triggerResults[i];
      if ( triggerBit == 1) 
	eventsPerTrigger[triggerName]++;
      if ( triggerBit == 3)
	errorsPerTrigger[triggerName]++;
    }
        
 
  } // End loop over event headers


  /*
   * Print out summary
   *
   */

  for (std::vector<std::string>::const_iterator it = vhltnames.begin(), itEnd = vhltnames.end(); it != itEnd; ++it) {

    const int eventCount = eventsPerTrigger[*it];
    const int errorCount = errorsPerTrigger[*it];

    if ( (eventCount == 0) && (errorCount == 0) )
      continue;

    std::cout << ":" << *it << "," << eventCount << "," << errorCount;
  }

  return 0;
}
  
