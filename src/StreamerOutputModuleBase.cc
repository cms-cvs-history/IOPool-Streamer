
#include "IOPool/Streamer/interface/StreamerOutputModuleBase.h"

#include "IOPool/Streamer/interface/InitMsgBuilder.h"
#include "IOPool/Streamer/interface/EventMsgBuilder.h"
#include "FWCore/RootAutoLibraryLoader/interface/RootAutoLibraryLoader.h"
#include "FWCore/Framework/interface/EventPrincipal.h"
#include "FWCore/Framework/interface/EventSelector.h"
#include "FWCore/ParameterSet/interface/Registry.h"
#include "FWCore/Utilities/interface/DebugMacros.h"
//#include "FWCore/Utilities/interface/Digest.h"
#include "FWCore/Utilities/interface/GetReleaseVersion.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/Provenance/interface/ParameterSetID.h"

#include <string>
#include "zlib.h"

namespace {
  //A utility function that packs bits from source into bytes, with 
  // packInOneByte as the numeber of bytes that are packed from source to dest.
  void printBits(unsigned char c) {
    for (int i = 7; i >= 0; --i) {
      int bit = ((c >> i) & 1);
      std::cout << " " << bit; 
    } 
  }   
    
  void packIntoString(std::vector<unsigned char> const& source,
                    std::vector<unsigned char>& package) {   
     unsigned int packInOneByte = 4; 
     unsigned int sizeOfPackage = 1+((source.size()-1)/packInOneByte); //Two bits per HLT
    
     package.resize(sizeOfPackage); 
     memset(&package[0], 0x00, sizeOfPackage);
 
     for (std::vector<unsigned char>::size_type i=0; i != source.size() ; ++i) { 
       unsigned int whichByte = i/packInOneByte;
       unsigned int indxWithinByte = i % packInOneByte;
       package[whichByte] = package[whichByte] | (source[i] << (indxWithinByte*2));
     }
    //for (unsigned int i=0; i !=package.size() ; ++i)
    //   printBits(package[i]);
    // std::cout << std::endl;

  }
}

namespace edm {
  StreamerOutputModuleBase::StreamerOutputModuleBase(ParameterSet const& ps) :
    OutputModule(ps),
    selections_(&keptProducts()[InEvent]),
    prod_reg_buf_(1000 * 1000),
    maxEventSize_(ps.getUntrackedParameter<int>("max_event_size", 7000000)),
    useCompression_(ps.getUntrackedParameter<bool>("use_compression", true)),
    compressionLevel_(ps.getUntrackedParameter<int>("compression_level", 1)),
    lumiSectionInterval_(ps.getUntrackedParameter<int>("lumiSection_interval", 0)), 
    serializer_(selections_),
    hltsize_(0),
    lumi_(0), 
    l1bit_(0),
    hltbits_(0),
    origSize_(0) // no compression as default value - we need this!
  {

    // test luminosity sections
    struct timeval now;
    struct timezone dummyTZ;
    gettimeofday(&now, &dummyTZ);
    timeInSecSinceUTC = static_cast<double>(now.tv_sec) + (static_cast<double>(now.tv_usec)/1000000.0);

    if(useCompression_ == true) {
      if(compressionLevel_ <= 0) {
        FDEBUG(9) << "Compression Level = " << compressionLevel_ 
                  << " no compression" << std::endl;
        compressionLevel_ = 0;
        useCompression_ = false;
      } else if(compressionLevel_ > 9) {
        FDEBUG(9) << "Compression Level = " << compressionLevel_ 
                  << " using max compression level 9" << std::endl;
        compressionLevel_ = 9;
      }
    }
    bufs_.resize(maxEventSize_);
    //loadExtraClasses();
    // do the line below instead of loadExtraClasses() to avoid Root errors
    RootAutoLibraryLoader::enable();

    // 25-Jan-2008, KAB - pull out the trigger selection request
    // which we need for the INIT message
    hltTriggerSelections_ = EventSelector::getEventSelectionVString(ps);
  }

  StreamerOutputModuleBase::~StreamerOutputModuleBase() {}

  void
  StreamerOutputModuleBase::beginRun(RunPrincipal const&) {
    start();
    std::auto_ptr<InitMsgBuilder>  init_message = serializeRegistry();
    doOutputHeader(*init_message);
  }
   
  void
  StreamerOutputModuleBase::endRun(RunPrincipal const&) {
    stop();
  }

  void
  StreamerOutputModuleBase::beginJob(EventSetup const&) {}

  void
  StreamerOutputModuleBase::endJob() {
    stop();  // for closing of files, notify storage manager, etc.
  }

  void
  StreamerOutputModuleBase::writeRun(RunPrincipal const&) {}

  void
  StreamerOutputModuleBase::writeLuminosityBlock(LuminosityBlockPrincipal const&) {}

  void
  StreamerOutputModuleBase::write(EventPrincipal const& e) {
    std::auto_ptr<EventMsgBuilder> msg = serializeEvent(e);
    doOutputEvent(*msg); // You can't use msg in StreamerOutputModuleBase after this point
  }

  std::auto_ptr<InitMsgBuilder>
  StreamerOutputModuleBase::serializeRegistry() {
    //Build the INIT Message
    //Following values are strictly DUMMY and will be replaced
    // once available with Utility function etc.
    uint32 run = 1;
    
    //Get the Process PSet ID  
    pset::Registry* reg = pset::Registry::instance();
    ParameterSetID toplevel = pset::getProcessParameterSetID(reg);

    //In case we need to print it 
    //  cms::Digest dig(toplevel.compactForm());
    //  cms::MD5Result r1 = dig.digest();
    //  std::string hexy = r1.toString();
    //  std::cout << "HEX Representation of Process PSetID: " << hexy << std::endl;  

    //Setting protocol version V
    Version v(5,(uint8*)toplevel.compactForm().c_str());

    Strings hltTriggerNames = getAllTriggerNames();
    hltsize_ = hltTriggerNames.size();
    // 18-Apr-2008, KAB: added a check for valid hltTriggerNames and
    // hltTriggerSelections lists.  This is a partial fix at best.
    // If getAllTriggerNames has returned an empty list, we are most
    // likely running offline in a cmsRun job that has no paths.  And,
    // it is likely that the job is trying to convert a ROOT file to
    // a streamer file.  In that case, it is not clear what trigger names
    // should be used.  Possibly ones from a previous process, but which
    // ones?  (and how would we get them?)
    // If there are no trigger names in an online job, isn't that a
    // mistake that should be flagged?
    if (hltsize_ > 0 && hltTriggerSelections_.size() > 0) {
      maskingSelector_.reset(new EventSelector(hltTriggerSelections_,
                                               hltTriggerNames));
    }

    //L1 stays dummy as of today
    Strings l1_names;  //3
    l1_names.push_back("t1");
    l1_names.push_back("t10");
    l1_names.push_back("t2");  

    //Setting the process name to HLT
    std::string processName = OutputModule::processName();

    std::auto_ptr<InitMsgBuilder> init_message(
        new InitMsgBuilder(&prod_reg_buf_[0], prod_reg_buf_.size(),
                           run, v, getReleaseVersion().c_str() , processName.c_str(),
                           description().moduleLabel().c_str(),
                           hltTriggerNames, hltTriggerSelections_, l1_names));

    // the translator already has the product registry (selections_),
    // so it just needs to serialize it to the init message.
    serializer_.serializeRegistry(*init_message);

    return init_message;
  }


  void
  StreamerOutputModuleBase::setHltMask(EventPrincipal const& e) {

    hltbits_.clear();  // If there was something left over from last event

    Handle<TriggerResults> const& prod = getTriggerResults(e);
    //Trig const& prod = getTrigMask(e);
    std::vector<unsigned char> vHltState; 
    
    if (prod.isValid()) {
      // 18-Apr-2008, KAB:  if we have a valid maskingSelector, use it
      // to mask the trigger results, otherwise simply use the TriggerResults
      // object in the event.
      if (maskingSelector_.get() != 0) {
        boost::shared_ptr<TriggerResults> maskedResults =
          maskingSelector_->maskTriggerResults(*prod);
        for(std::vector<unsigned char>::size_type i=0; i != hltsize_ ; ++i) {
          vHltState.push_back(((maskedResults->at(i)).state()));
        }
      }
      else {
        for(std::vector<unsigned char>::size_type i=0; i != hltsize_ ; ++i) {
          vHltState.push_back(((prod->at(i)).state()));
        }
      }
    } else {
     // We fill all Trigger bits to valid state.
     for(std::vector<unsigned char>::size_type i=0; i != hltsize_ ; ++i) {
           vHltState.push_back(hlt::Pass);
      }
    }
    
    //Pack into member hltbits_
    packIntoString(vHltState, hltbits_);

    //This is Just a printing code.
    //std::cout << "Size of hltbits:" << hltbits_.size() << std::endl;
    //for(unsigned int i=0; i != hltbits_.size() ; ++i) {
    //  printBits(hltbits_[i]);
    //}
    //std::cout << "\n";
  }

 
// test luminosity sections
  void
  StreamerOutputModuleBase::setLumiSection() {
    struct timeval now;
    struct timezone dummyTZ;
    gettimeofday(&now, &dummyTZ);
    double timeInSec = static_cast<double>(now.tv_sec) + (static_cast<double>(now.tv_usec)/1000000.0) - timeInSecSinceUTC;
    // what about overflows?
    if(lumiSectionInterval_ > 0) lumi_ = static_cast<uint32>(timeInSec/lumiSectionInterval_) + 1;
  }

  std::auto_ptr<EventMsgBuilder>
  StreamerOutputModuleBase::serializeEvent(EventPrincipal const& e) {
    //Lets Build the Event Message first 

    //Following is strictly DUMMY Data for L! Trig and will be replaced with actual
    // once figured out, there is no logic involved here.
    l1bit_.push_back(true);
    l1bit_.push_back(true);
    l1bit_.push_back(false);
    //End of dummy data

    setHltMask(e);

    if (lumiSectionInterval_ == 0) {
      lumi_ = e.luminosityBlock();
    } else {
      setLumiSection();
    }

    serializer_.serializeEvent(e, useCompression_, compressionLevel_);

    // resize bufs_ to reflect space used in serializer_ + header
    // I just added an overhead for header of 50000 for now
    unsigned int src_size = serializer_.currentSpaceUsed();
    unsigned int new_size = src_size + 50000;
    if(bufs_.size() < new_size) bufs_.resize(new_size);

    std::string moduleLabel = description().moduleLabel();
    uLong crc = crc32(0L, Z_NULL, 0);
    Bytef* buf = (Bytef*) moduleLabel.data();
    crc = crc32(crc, buf, moduleLabel.length());

    std::auto_ptr<EventMsgBuilder> 
      msg(new EventMsgBuilder(&bufs_[0], bufs_.size(),
			       e.id().run(), e.id().event(), lumi_,
                               (unsigned int) crc,
			       l1bit_, (uint8*)&hltbits_[0], hltsize_) );
    msg->setOrigDataSize(origSize_); // we need this set to zero

    // copy data into the destination message
    // an alternative is to have serializer only to the serialization
    // in serializeEvent, and then call a new member "getEventData" that
    // takes the compression arguments and a place to put the data.
    // This will require one less copy.  The only catch is that the
    // space provided in bufs_ should be at least the uncompressed 
    // size + overhead for header because we will not know the actual
    // compressed size.

    unsigned char* src = serializer_.bufferPointer();
    std::copy(src,src + src_size, msg->eventAddr());
    msg->setEventLength(src_size);
    if(useCompression_) msg->setOrigDataSize(serializer_.currentEventSize());

    l1bit_.clear();  //Clear up for the next event to come.
    return msg;
  }

} // end of namespace-edm