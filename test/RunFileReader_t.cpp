/*----------------------------------------------------------------------

 $Id: RunFileReader_t.cpp,v 1.5 2007/01/22 21:52:24 wmtan Exp $

----------------------------------------------------------------------*/  

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "DataFormats/Common/interface/ProductRegistry.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "IOPool/Streamer/interface/Utilities.h"
#include "IOPool/Streamer/interface/TestFileReader.h"
#include "IOPool/Streamer/interface/HLTInfo.h"
#include "IOPool/Streamer/interface/ClassFiller.h"

#include "boost/shared_ptr.hpp"
#include "boost/bind.hpp"

#include "FWCore/PluginManager/interface/PluginManager.h"

using namespace std;

class Drain
{
 public:
  Drain();
  ~Drain();

  edm::EventBuffer& getQueue() { return q_; }

  void start();
  void join() { me_->join(); }

 private:
  static void run(Drain*);
  void readData();

  int count_;
  edm::EventBuffer q_;

  boost::shared_ptr<boost::thread> me_;
};

Drain::Drain():count_(),q_(sizeof(stor::FragEntry),2*3)
{
}

Drain::~Drain()
{
}

void Drain::start()
{
  me_.reset(new boost::thread(boost::bind(Drain::run,this)));
}

void Drain::run(Drain* d)
{
  cout << "Drain::run " << (void*)d << endl;
  d->readData();
}

void Drain::readData()
{
  while(1)
    {
      edm::EventBuffer::ConsumerBuffer b(q_);
      if(b.size()==0) break;

	  // cout << "Drain: " << (void*)this << " " << (void*)&q_ << endl;
      // cout << "Drain: woke up " << b.size() << endl;

      stor::FragEntry* v = (stor::FragEntry*)b.buffer();
      // cout << "Drain: cast frag " << b.buffer() << " " << v->buffer_size_ << endl;
      char* p = (char*)v->buffer_object_;
      // cout << "Drain: get frag " << (void*)p << endl;
      delete [] p;
      // cout << "Drain: delete frag " << b.size() << endl;
      ++count_;
    }

  cout << "Drain: got " << count_ << " events" << endl;
}

// -----------------------------------------------

class Main
{
 public:
  Main(const vector<string>& file_names);
  ~Main();
  
  int run();

 private:

  // disallow the following
  Main(const Main&) {} 
  Main& operator=(const Main&) { return *this; }

  vector<string> names_;
  edm::ProductRegistry prods_;
  Drain drain_;
  typedef boost::shared_ptr<edmtestp::TestFileReader> ReaderPtr;
  typedef vector<ReaderPtr> Readers;
  Readers readers_;
};

// ----------- implementation --------------

Main::~Main() { }


Main::Main(const vector<string>& file_names):
  names_(file_names),
  prods_(edm::getRegFromFile(file_names[0])),
  drain_()
{
  cout << "ctor of Main" << endl;
  // jbk - the next line should not be needed
  // edm::declareStreamers(prods_);
  vector<string>::iterator it(names_.begin()),en(names_.end());
  for(; it != en; ++it) {
      ReaderPtr p(new edmtestp::TestFileReader(*it,
					       drain_.getQueue(),
					       prods_));
      readers_.push_back(p);
  }
  edm::loadExtraClasses();
}

int Main::run()
{
  cout << "starting the drain" << endl;
  drain_.start();

  cout << "started the drain" << endl;
  // sleep(10);

  // start file readers
  Readers::iterator it(readers_.begin()),en(readers_.end());
  for(; it != en; ++it) {
      (*it)->start();
  }
  // wait for all file readers to complete
  for(it = readers_.begin(); it != en; ++it) {
      (*it)->join();
  }

  // send done to the drain
  edm::EventBuffer::ProducerBuffer b(drain_.getQueue());
  b.commit();

  drain_.join();
  return 0;
}

int main(int argc, char* argv[])
{
  // pull options out of command line
  if(argc < 2) {
      cout << "Usage: " << argv[0] << " "
	   << "file1 file2 ... fileN"
	   << endl;
      return 0;
      //throw cms::Exception("config") << "Bad command line arguments\n";
  }

  seal::PluginManager::get()->initialise();
  
  vector<string> file_names;
  
  for(int i = 1; i < argc; ++i) {
      cout << argv[i] << endl;
      file_names.push_back(argv[i]);
  }
  
  try {
    edm::loadExtraClasses();
    cout << "Done loading extra classes" << endl;
    Main m(file_names);
    m.run();
  }
  catch(cms::Exception& e) {
      cerr << "Caught an exception:\n" << e.what() << endl;
      throw;
  }
  catch(...) {
      cerr << "Caught unknown exception\n" << endl;
  }

  cout << "Main is done!" << endl;
  return 0;
}

