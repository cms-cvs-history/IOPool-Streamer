#ifndef STREAMER_TESTEVENTSTREAMFILEREADER_H
#define STREAMER_TESTEVENTSTREAMFILEREADER_H

#include "IOPool/Streamer/interface/EventBuffer.h"
#include "IOPool/Streamer/interface/Utilities.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/ProductRegistry.h"

#include "boost/shared_ptr.hpp"
#include "boost/thread/thread.hpp"

#include <vector>
#include <memory>
#include <string>
#include <fstream>

namespace edmtestp
{
  class TestFileReader
  {
  public:
    TestFileReader(const std::string& filename,edm::EventBuffer& to,
		   edm::ProductRegistry& prods);
    virtual ~TestFileReader();

    void start();
    void join();

  private:  
    void readEvents();
    static void run(TestFileReader*);

    std::string filename_;
    std::ifstream ist_;
    edm::EventReader reader_;
    edm::EventBuffer* to_;
    boost::shared_ptr<boost::thread> me_;
  };

}

#endif

