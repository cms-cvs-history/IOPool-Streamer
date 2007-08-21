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
#include "IOPool/Streamer/interface/DumpTools.h"
#include "FWCore/Framework/interface/EventSelector.h"

using namespace boost::program_options;


// -----------------------------------------------



void convertTriggers(unsigned char c, std::vector<int> & results){
  /*
   * Util to convert an 8 bit word into 4x2 bit integer HLT Trigger Bits
   * c = AABBCCDD
   * results will get AA, BB, CC, DD converted to (0,1,2,3) and added to it.
   */

  for (int i = 3; i >= 0; --i) {
    int shift = 2*i;
    int bit1 = ((c >> (shift+1)) & 1);
    int bit2 = ((c >> shift) & 1);
    int trigVal = (2*bit1) + bit2;
   
    results.push_back(trigVal);
  }

}





int main(int argc, char* argv[]) {
  /*
   * Main program
   *
   */
  std::string const kProgramName = argv[0];

  /*
   * Command line setup and parsing
   */
  options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("in,i", value<std::string>(), "input file")
    ("out,o", value<std::string>(), "output file");

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
  
  if (!vm.count("out")) {
    std::cerr << "output not set.\n";
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
  std::string outf = vm["out"].as<std::string>(); 

  std::cout << "Input = " << in << std::endl;
  std::cout << "Output = " << outf << std::endl;
  
  
  /*
   * First read in the streamer index file
   * and get the Init Message
   *
   */
  StreamerInputIndexFile * indexFile = new StreamerInputIndexFile(in);
  const StartIndexRecord* startindx = indexFile->startMessage();
  const InitMsgView* start = startindx->getInit();

  /*
   * Extract the list of trigger names and the number of trigger bit entries 
   */
  std::vector<std::string> vhltnames;
  start->hltTriggerNames(vhltnames);
  int hltBitCount = start->get_hlt_bit_cnt();

  std::map<std::string, int> eventsPerTrigger;
  std::map<std::string, int> errorsPerTrigger;
  
  

  std::vector<std::string>::iterator iName;
  for (iName = vhltnames.begin(); iName != vhltnames.end(); iName++){
    eventsPerTrigger[*iName] = 0;
    errorsPerTrigger[*iName] = 0;
  }
  

  int runNumber = 0;
  int lumiNumber = 0;
  int eventCount = 0;

  /*
   * Process each event header in the index, by extracting the hlt trigger bits
   * HLT bits are 2 bits long
   *
   */
  for(indexRecIter it = indexFile->begin(), itEnd = indexFile->end(); it != itEnd; ++it) {
    
    const EventMsgView* iview = (*it)->getEventView();
    runNumber = iview->run();
    lumiNumber = iview->lumi();
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
      int triggerBit = triggerResults[i];
      if ( triggerBit == 1) 
	eventsPerTrigger[triggerName]++;
      if ( triggerBit == 3)
	errorsPerTrigger[triggerName]++;
    }
        
 
  } // End loop over event headers
  
  /*
   * Write summary for each path
   *
   */
  std::ofstream outputXML(outf.c_str(), std::ios::out );
  
  outputXML << "<StreamerIndex Run=\"" << runNumber <<"\" Lumi=\""
	    << lumiNumber << "\" TotalEvents=\"" << eventCount 
	    << "\" >\n";
  std::vector<std::string>::iterator iTrig;
  for (iTrig = vhltnames.begin(); iTrig != vhltnames.end(); iTrig++){
    int evCount = eventsPerTrigger[*iTrig];
    int errCount = errorsPerTrigger[*iTrig];
    
    if ( (evCount == 0) && (errCount == 0) ) 
      continue;
	
    outputXML << "  <TriggerPath Name=\"" << *iTrig <<"\">\n"
	      << "     <EventCount Value=\""<<eventsPerTrigger[*iTrig]<< "\"/>\n"
	      << "     <ErrorCount Value=\""<<errorsPerTrigger[*iTrig]<< "\"/>\n"
	      << "  </TriggerPath>\n";
    

  }
  outputXML << "</StreamerIndex>\n";
  outputXML.close();
  return 0;
}
  
