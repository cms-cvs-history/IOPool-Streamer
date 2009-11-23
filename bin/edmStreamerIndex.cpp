#include <exception>
#include <iostream>
#include <fstream>
#include <iosfwd>
#include <iomanip>
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


void convertTriggers(unsigned char c, std::vector<int> & results) {
  /*
   * Util to convert an 8 bit word into 4x2 bit integer HLT Trigger Bits
   * c = AABBCCDD
   * results will get AA, BB, CC, DD converted to (0,1,2,3) and added to it.
   */

  for (int i = 0; i < 4; ++i) {
    int const shift = 2*i;
    int const bit1 = ((c >> (shift+1)) & 1);
    int const bit2 = ((c >> shift) & 1);
    int const trigVal = (2*bit1) + bit2;

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
  edm::StreamerInputIndexFile indexFile(in);
  StartIndexRecord const* const startindx = indexFile.startMessage();
  InitMsgView const* const start = startindx->getInit();

  /*
   * Extract the list of trigger names and the number of trigger bit entries
   */
  std::vector<std::string> vhltnames;
  start->hltTriggerNames(vhltnames);
  unsigned int const hltBitCount = start->get_hlt_bit_cnt();

  std::map<std::string, std::vector<unsigned int> > eventsPerTrigger;
  std::map<std::string, std::vector<unsigned int> > errorsPerTrigger;

  /*
   * Process each event header in the index, by extracting the hlt trigger bits
   * HLT bits are 2 bits long
   *
   */
  unsigned int eventCount = 0;
  for(indexRecIter it = indexFile.begin(), itEnd = indexFile.end(); it != itEnd; ++it) {

    EventMsgView const* const iview = (*it)->getEventView();

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
    for (std::vector<unsigned char>::const_iterator it = hlt_out.begin(), itEnd = hlt_out.end(); it != itEnd; ++it) {
      convertTriggers(*it, triggerResults);
    }

    /*
     * Walk through the trigger paths and trigger bits and record the states in the
     * maps available
     *
     */
    for (unsigned int i=0; i < hltBitCount; i++){
      std::string triggerName = vhltnames[i];
      int const triggerBit = triggerResults[i];
      if (triggerBit == 1)
	eventsPerTrigger[triggerName].push_back(eventCount);
      if (triggerBit == 3)
	errorsPerTrigger[triggerName].push_back(eventCount);
    }

    eventCount++;

  } // End loop over event headers


  /*
   * Print out summary
   *
   */

  for (std::vector<std::string>::const_iterator it = vhltnames.begin(), itEnd = vhltnames.end(); it != itEnd; ++it) {

    std::vector<unsigned int> events = eventsPerTrigger[*it];
    std::vector<unsigned int> errors = errorsPerTrigger[*it];

    // print trigger path name
    std::cout << ":" << *it;

//     unsigned int eventCount = 0;
//     for (std::vector<unsigned int>::const_iterator it = events.begin(), itEnd = events.end(); it != itEnd; ++it) {
//       ++eventCount;
//     }

//     unsigned int errorCount = 0;
//     for (std::vector<unsigned int>::const_iterator it = errors.begin(), itEnd = errors.end(); it != itEnd; ++it) {
//       ++errorCount;
//     }

//     std::cout << "," << eventCount << "," << errorCount;

    // how many integers do we need ?
    unsigned int intCount = eventCount / (sizeof(unsigned int) * 8);
    if ((eventCount % (sizeof(unsigned int)*8) ) != 0 )
      ++intCount;

    // prepare and initialize integers
    std::vector<unsigned int> eventBitPattern;
    eventBitPattern.resize(intCount);

    // fill integers, use their bits
    for (std::vector<unsigned int>::const_iterator it = events.begin(), itEnd = events.end(); it != itEnd; ++it) {
      eventBitPattern[ *it / (sizeof(unsigned int) * 8) ] |= (1 << *it % (sizeof(unsigned int)*8) );
    }

    // print integers
    for (std::vector<unsigned int>::const_iterator it = eventBitPattern.begin(), itEnd = eventBitPattern.end(); it != itEnd; ++it) {
      std::cout << "," << std::hex << *it;
    }

    std::cout << std::endl;

  }

  return 0;
}

