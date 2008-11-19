#include "IOPool/Streamer/interface/StreamerInputSource.h"
#include "DataFormats/Provenance/interface/ProcessConfiguration.h" 

#include "IOPool/Streamer/interface/EventMessage.h"
#include "IOPool/Streamer/interface/InitMessage.h"
#include "IOPool/Streamer/interface/ClassFiller.h"

#include "FWCore/Framework/interface/EventPrincipal.h"
#include "FWCore/Framework/interface/FileBlock.h"
#include "DataFormats/Provenance/interface/BranchDescription.h"
#include "DataFormats/Provenance/interface/EventEntryDescription.h"
#include "DataFormats/Provenance/interface/ProductProvenance.h"
#include "DataFormats/Provenance/interface/EntryDescriptionRegistry.h"
#include "DataFormats/Provenance/interface/EventAuxiliary.h"
#include "DataFormats/Provenance/interface/LuminosityBlockAuxiliary.h"
#include "DataFormats/Provenance/interface/RunAuxiliary.h"

#include "zlib.h"

#include "DataFormats/Common/interface/RefCoreStreamer.h"
#include "FWCore/Utilities/interface/WrappedClassName.h"
#include "DataFormats/Common/interface/EDProduct.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Framework/interface/LuminosityBlockPrincipal.h"
#include "FWCore/Framework/interface/RunPrincipal.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/Registry.h"
#include "FWCore/Utilities/interface/ThreadSafeRegistry.h"

#include "DataFormats/Provenance/interface/ModuleDescriptionRegistry.h"
#include "DataFormats/Provenance/interface/BranchIDListRegistry.h"
#include "DataFormats/Provenance/interface/ProductRegistry.h"
#include "DataFormats/Provenance/interface/ProcessHistoryRegistry.h"
#include "FWCore/Utilities/interface/DebugMacros.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <string>
#include <iostream>
#include <set>

namespace edm {
  namespace {
    const int init_size = 1024*1024;
  }

  std::string StreamerInputSource::processName_;
  unsigned int StreamerInputSource::protocolVersion_;


  StreamerInputSource::StreamerInputSource(
                    ParameterSet const& pset,
                    InputSourceDescription const& desc):
    InputSource(pset, desc),
    newRun_(true),
    newLumi_(true),
    ep_(),
    tc_(getTClass(typeid(SendEvent))),
    dest_(init_size),
    xbuf_(TBuffer::kRead, init_size),
    runEndingFlag_(false),
    productGetter_()
  {
  }

  StreamerInputSource::~StreamerInputSource() {}

  // ---------------------------------------
  boost::shared_ptr<FileBlock>
  StreamerInputSource::readFile_() {
    return boost::shared_ptr<FileBlock>(new FileBlock);
  }

  void
  StreamerInputSource::mergeIntoRegistry(SendJobHeader const& header,
			 ProductRegistry& reg, bool subsequent) {

    SendDescs const& descs = header.descs();

    FDEBUG(6) << "mergeIntoRegistry: Product List: " << std::endl;

    if (subsequent) {
      ProductRegistry pReg;
      pReg.updateFromInput(descs);
      std::string mergeInfo = reg.merge(pReg, std::string(), BranchDescription::Permissive);
      if (!mergeInfo.empty()) {
        throw cms::Exception("MismatchedInput","RootInputFileSequence::previousEvent()") << mergeInfo;
      }
      BranchIDListHelper::updateFromInput(header.branchIDLists(), std::string());
      BranchIDListHelper::updateFromInput(header.parameterSetIDLists(), std::string());
    } else {
      declareStreamers(descs);
      buildClassCache(descs);
      loadExtraClasses();
      reg.updateFromInput(descs);
      BranchIDListHelper::updateFromInput(header.branchIDLists(), std::string());
      BranchIDListHelper::updateFromInput(header.parameterSetIDLists(), std::string());
    }
  }

  void
  StreamerInputSource::declareStreamers(SendDescs const& descs) {
    SendDescs::const_iterator i(descs.begin()), e(descs.end());

    for(; i != e; ++i) {
        //pi->init();
        std::string const real_name = wrappedClassName(i->className());
        FDEBUG(6) << "declare: " << real_name << std::endl;
        loadCap(real_name);
    }
  }


  void
  StreamerInputSource::buildClassCache(SendDescs const& descs) { 
    SendDescs::const_iterator i(descs.begin()), e(descs.end());

    for(; i != e; ++i) {
        //pi->init();
        std::string const real_name = wrappedClassName(i->className());
        FDEBUG(6) << "BuildReadData: " << real_name << std::endl;
        doBuildRealData(real_name);
    }
  }

  void
  StreamerInputSource::saveTriggerNames(InitMsgView const* header) {

    ParameterSet trigger_pset;
    std::vector<std::string> paths;
    header->hltTriggerNames(paths);
    trigger_pset.addParameter<Strings>("@trigger_paths", paths);
    pset::Registry* psetRegistry = pset::Registry::instance();
    psetRegistry->insertMapped(trigger_pset);
  }

  boost::shared_ptr<RunPrincipal>
  StreamerInputSource::readRun_() {
    assert(newRun_);
    assert(runPrincipal());
    newRun_ = false;
    return runPrincipal();
  }

  boost::shared_ptr<LuminosityBlockPrincipal>
  StreamerInputSource::readLuminosityBlock_() {
    assert(!newRun_);
    assert(newLumi_);
    assert(luminosityBlockPrincipal());
    newLumi_ = false;
    return luminosityBlockPrincipal();
  }

  std::auto_ptr<EventPrincipal>
  StreamerInputSource::readEvent_() {
    assert(!newRun_);
    assert(!newLumi_);
    assert(ep_.get() != 0);
    // This copy resets ep_.
    return ep_;
  }

  InputSource::ItemType 
  StreamerInputSource::getNextItemType() {
    if (runEndingFlag_) {
      return IsStop;
    }
    if(newRun_ && runPrincipal()) {
      return IsRun;
    }
    if(newLumi_ && luminosityBlockPrincipal()) {
      return IsLumi;
    }
    if (ep_.get() != 0) {
      return IsEvent;
    }
    ep_ = read();
    if (ep_.get() == 0) {
      return IsStop;
    } else {
      runEndingFlag_ = false;
    }
    if(newRun_) {
      return IsRun;
    } else if(newLumi_) {
      return IsLumi;
    }
    return IsEvent;
  }

  /**
   * Deserializes the specified init message into a SendJobHeader object
   * (which is related to the product registry).
   */
  std::auto_ptr<SendJobHeader>
  StreamerInputSource::deserializeRegistry(InitMsgView const& initView)
  {
    if(initView.code() != Header::INIT)
      throw cms::Exception("StreamTranslation","Registry deserialization error")
        << "received wrong message type: expected INIT, got "
        << initView.code() << "\n";

    //Get the process name and store if for Protocol version 4 and above.
    if (initView.protocolVersion() > 3) {

         processName_ = initView.processName();
         protocolVersion_ = initView.protocolVersion();

         FDEBUG(10) << "StreamerInputSource::deserializeRegistry processName = "<< processName_<< std::endl;
         FDEBUG(10) << "StreamerInputSource::deserializeRegistry protocolVersion_= "<< protocolVersion_<< std::endl;
    }
    
    TClass* desc = getTClass(typeid(SendJobHeader));

    RootBuffer xbuf(TBuffer::kRead, initView.descLength(),
                 (char*)initView.descData(),kFALSE);
    RootDebug tracer(10,10);
    std::auto_ptr<SendJobHeader> sd((SendJobHeader*)xbuf.ReadObjectAny(desc));

    if(sd.get()==0) {
        throw cms::Exception("StreamTranslation","Registry deserialization error")
          << "Could not read the initial product registry list\n";
    }

    return sd;  
  }

  /**
   * Deserializes the specified init message into a SendJobHeader object
   * and merges registries.
   */
  void
  StreamerInputSource::deserializeAndMergeWithRegistry(InitMsgView const& initView, bool subsequent)
  {
     std::auto_ptr<SendJobHeader> sd = deserializeRegistry(initView);
     mergeIntoRegistry(*sd, productRegistryUpdate(), subsequent);
     ModuleDescriptionRegistry::instance()->insertCollection(sd->moduleDescriptionMap());
     SendJobHeader::ParameterSetMap const & psetMap = sd->processParameterSet();
     pset::Registry& psetRegistry = *pset::Registry::instance();
     for (SendJobHeader::ParameterSetMap::const_iterator i = psetMap.begin(), iEnd = psetMap.end(); i != iEnd; ++i) {
       psetRegistry.insertMapped(ParameterSet(i->second.pset_));
     }
  }

  /**
   * Deserializes the specified event message into an EventPrincipal object.
   */
  std::auto_ptr<EventPrincipal>
  StreamerInputSource::deserializeEvent(EventMsgView const& eventView)
  {
    if(eventView.code() != Header::EVENT)
      throw cms::Exception("StreamTranslation","Event deserialization error")
        << "received wrong message type: expected EVENT, got "
        << eventView.code() << "\n";
    FDEBUG(9) << "Decode event: "
         << eventView.event() << " "
         << eventView.run() << " "
         << eventView.size() << " "
         << eventView.eventLength() << " "
         << eventView.eventData()
         << std::endl;
    EventSourceSentry(*this);
    // uncompress if we need to
    // 78 was a dummy value (for no uncompressed) - should be 0 for uncompressed
    // need to get rid of this when 090 MTCC streamers are gotten rid of
    unsigned long origsize = eventView.origDataSize();
    unsigned long dest_size; //(should be >= eventView.origDataSize() )

    if(origsize != 78 && origsize != 0)
    {
      // compressed
      dest_size = uncompressBuffer((unsigned char*)eventView.eventData(),
                                   eventView.eventLength(), dest_, origsize);
    }
    else // not compressed
    {
      // we need to copy anyway the buffer as we are using dest in xbuf
      dest_size = eventView.eventLength();
      dest_.resize(dest_size);
      unsigned char* pos = (unsigned char*) &dest_[0];
      unsigned char* from = (unsigned char*) eventView.eventData();
      std::copy(from,from+dest_size,pos);
    }
    //TBuffer xbuf(TBuffer::kRead, dest_size,
    //             (char*) &dest[0],kFALSE);
    //TBuffer xbuf(TBuffer::kRead, eventView.eventLength(),
    //             (char*) eventView.eventData(),kFALSE);
    xbuf_.Reset();
    xbuf_.SetBuffer(&dest_[0],dest_size,kFALSE);
    RootDebug tracer(10,10);

    setRefCoreStreamer(&productGetter_);
    std::auto_ptr<SendEvent> sd((SendEvent*)xbuf_.ReadObjectAny(tc_));
    setRefCoreStreamer();

    if(sd.get()==0) {
        throw cms::Exception("StreamTranslation","Event deserialization error")
          << "got a null event from input stream\n";
    }
    ProcessHistoryRegistry::instance()->insertMapped(sd->processHistory());

    FDEBUG(5) << "Got event: " << sd->aux().id() << " " << sd->products().size() << std::endl;
    if(!runPrincipal() || runPrincipal()->run() != sd->aux().run()) {
	newRun_ = newLumi_ = true;
	RunAuxiliary runAux(sd->aux().run(), sd->aux().time(), Timestamp::invalidTimestamp());
	setRunPrincipal(boost::shared_ptr<RunPrincipal>(
          new RunPrincipal(runAux,
			   productRegistry(),
			   processConfiguration())));
        resetLuminosityBlockPrincipal();
    }
    if(!luminosityBlockPrincipal() || luminosityBlockPrincipal()->luminosityBlock() != eventView.lumi()) {
      
      LuminosityBlockAuxiliary lumiAux(runPrincipal()->run(), eventView.lumi(), sd->aux().time(), Timestamp::invalidTimestamp());
      setLuminosityBlockPrincipal(boost::shared_ptr<LuminosityBlockPrincipal>(
        new LuminosityBlockPrincipal(lumiAux,
				     productRegistry(),
				     processConfiguration())));
      newLumi_ = true;
    }

    boost::shared_ptr<History> history (new History(sd->history()));
    std::auto_ptr<EventPrincipal> ep(new EventPrincipal(sd->aux(),
                                                   productRegistry(),
                                                   processConfiguration(),
                                                   history));
    productGetter_.setEventPrincipal(ep.get());

    // no process name list handling

    SendProds & sps = sd->products();
    for(SendProds::iterator spi = sps.begin(), spe = sps.end(); spi != spe; ++spi) {
        FDEBUG(10) << "check prodpair" << std::endl;
        if(spi->desc() == 0)
          throw cms::Exception("StreamTranslation","Empty Provenance");
        FDEBUG(5) << "Prov:"
             << " " << spi->desc()->className()
             << " " << spi->desc()->productInstanceName()
             << " " << spi->desc()->branchID()
             << std::endl;

	ConstBranchDescription branchDesc(*spi->desc());
	// This ProductProvenance constructor inserts into the entry description registry
        boost::shared_ptr<ProductProvenance> eventEntryDesc(
	     new ProductProvenance(spi->branchID(),
				spi->status(),
				*spi->parents()));

	ep->branchMapperPtr()->insert(*eventEntryDesc);
        if(spi->prod() != 0) {
          std::auto_ptr<EDProduct> aprod(const_cast<EDProduct*>(spi->prod()));
          FDEBUG(10) << "addgroup next " << spi->branchID() << std::endl;
          ep->addGroup(aprod, branchDesc, eventEntryDesc);
          FDEBUG(10) << "addgroup done" << std::endl;
        } else {
          FDEBUG(10) << "addgroup empty next " << spi->branchID() << std::endl;
          ep->addGroup(branchDesc, eventEntryDesc);
          FDEBUG(10) << "addgroup empty done" << std::endl;
        }
        spi->clear();
    }

    FDEBUG(10) << "Size = " << ep->size() << std::endl;

    return ep;     
  }

  /**
   * Uncompresses the data in the specified input buffer into the
   * specified output buffer.  The inputSize should be set to the size
   * of the compressed data in the inputBuffer.  The expectedFullSize should
   * be set to the original size of the data (before compression).
   * Returns the actual size of the uncompressed data.
   * Errors are reported by throwing exceptions.
   */
  unsigned int
  StreamerInputSource::uncompressBuffer(unsigned char *inputBuffer,
				       unsigned int inputSize,
				       std::vector<unsigned char> &outputBuffer,
				       unsigned int expectedFullSize)
  {
    unsigned long origSize = expectedFullSize;
    unsigned long uncompressedSize = expectedFullSize;
    FDEBUG(1) << "Uncompress: original size = " << origSize
              << ", compressed size = " << inputSize
              << std::endl;
    outputBuffer.resize(origSize);
    int ret = uncompress(&outputBuffer[0], &uncompressedSize,
                         inputBuffer, inputSize); // do not need compression level
    //std::cout << "unCompress Return value: " << ret << " Okay = " << Z_OK << std::endl;
    if(ret == Z_OK) {
        // check the length against original uncompressed length
        FDEBUG(10) << " original size = " << origSize << " final size = " 
                   << uncompressedSize << std::endl;
        if(origSize != uncompressedSize) {
            std::cerr << "deserializeEvent: Problem with uncompress, original size = "
                 << origSize << " uncompress size = " << uncompressedSize << std::endl;
            // we throw an error and return without event! null pointer
            throw cms::Exception("StreamDeserialization","Uncompression error")
              << "mismatch event lengths should be" << origSize << " got "
              << uncompressedSize << "\n";
        }
    } else {
        // we throw an error and return without event! null pointer
        std::cerr << "deserializeEvent: Problem with uncompress, return value = "
             << ret << std::endl;
        throw cms::Exception("StreamDeserialization","Uncompression error")
            << "Error code = " << ret << "\n ";
    }

    return (unsigned int) uncompressedSize;
  }

  void StreamerInputSource::resetAfterEndRun()
  {
     // called from an online streamer source to reset after a stop command
     // so an enable command will work
     assert(ep_.get() == 0);
     reset();
     runEndingFlag_ = false;
  }

  void StreamerInputSource::setRun(RunNumber_t) 
  {
     // Need to define a dummy setRun here or else the InputSource::setRun is called
     // if we have a source inheriting from this and wants to define a setRun method
     throw edm::Exception(edm::errors::LogicError)
     << "StreamerInputSource::setRun()\n"
     << "Run number cannot be modified for this type of Input Source\n"
     << "Contact a Storage Manager Developer\n";
  }

  StreamerInputSource::ProductGetter::ProductGetter() : eventPrincipal_(0) {}

  StreamerInputSource::ProductGetter::~ProductGetter() {}

  EDProduct const*
  StreamerInputSource::ProductGetter::getIt(edm::ProductID const& id) const {
    return eventPrincipal_ ? eventPrincipal_->getIt(id) : 0;
  }

  void
  StreamerInputSource::ProductGetter::setEventPrincipal(EventPrincipal *ep) {
    eventPrincipal_ = ep;
  }
}
