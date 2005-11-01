
#include "IOPool/Streamer/interface/BufferArea.h"
#include "IOPool/Streamer/interface/HLTInfo.h"

namespace 
{
  // 250KB/fragment, allocate room for 20 fragments
  const int fragment_count = 20;
}

namespace stor 
{

  HLTInfo::HLTInfo():
    cmd_q_(edm::getEventBuffer(4,50)),
    evtbuf_q_(edm::getEventBuffer(4*fragment_count+4,100)),
    frag_q_(edm::getEventBuffer(4,200))
  {
  }

  HLTInfo::HLTInfo(const edm::ParameterSet& ps):
    cmd_q_(edm::getEventBuffer(4,50)),
    evtbuf_q_(edm::getEventBuffer(4*fragment_count+4,100)),
    frag_q_(edm::getEventBuffer(4,200))
  {
  }

  HLTInfo::HLTInfo(const edm::ProductRegistry& pr):
    prods_(pr),
    cmd_q_(edm::getEventBuffer(4,50)),
    evtbuf_q_(edm::getEventBuffer(4*fragment_count+4,100)),
    frag_q_(edm::getEventBuffer(4,200))
  {
  }

  HLTInfo::~HLTInfo()
  {
  }

  HLTInfo::HLTInfo(const HLTInfo&) { }
  const HLTInfo& HLTInfo::operator=(const HLTInfo&) { return *this; }

  void HLTInfo::mergeRegistry(edm::ProductRegistry& pr)
  {
    typedef edm::ProductRegistry::ProductList ProdList; 
    ProdList plist(prods_.productList());
    ProdList::iterator pi(plist.begin()),pe(plist.end());
    
    for(;pi!=pe;++pi)
      {
	pr.copyProduct(pi->second);
      }
  }
}

