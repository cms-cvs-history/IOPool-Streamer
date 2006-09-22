#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/ParameterSet/interface/ProcessDesc.h"
#include "boost/shared_ptr.hpp"

#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <iterator>

using namespace std;
using namespace edm;

class ParamSetWalker {
  
 public:
  explicit ParamSetWalker(const std::string& config) {
    try{
    
      ProcessDesc  pdesc(config.c_str()); 
      
      boost::shared_ptr<ParameterSet> procPset = pdesc.getProcessPSet();
      cout<<"Process PSet:"<<procPset->toString()<<endl;           
      
      //cout << "Module Label: " << procPset->getParameter<string>("@module_label")
      //     << std::endl;
      
     edm::ParameterSet allTrigPaths = procPset->getUntrackedParameter<edm::ParameterSet>("@trigger_paths");
     std::cout <<"Found  Trig Path :"<<allTrigPaths.toString()<<endl; 
     
     if (allTrigPaths.empty())
         throw cms::Exception("ParamSetWalker","ParamSetWalker") 
               << "No Trigger or End Path Found in the Config File" <<endl;
     
     std::vector<std::string> allEndPaths = allTrigPaths.getParameter<std::vector<std::string> >("@end_paths");

     if (allEndPaths.empty())
         throw cms::Exception("ParamSetWalker","ParamSetWalker") 
               << "No End Path Found in the Config File" <<endl;

     for(std::vector<std::string>::iterator it = allEndPaths.begin();
          it != allEndPaths.end();
          ++it)
        {
          std::cout <<"Found an end Path :"<<(*it)<<std::endl;
          //Lets try to get this PSet from the Process PSet
          vector<std::string> anEndPath = procPset->getParameter<vector<std::string> >((*it));
          for(std::vector<std::string>::iterator it = anEndPath.begin();
          it != anEndPath.end(); ++it) {  
              std::cout <<"Found a end Path PSet :"<<(*it)<<endl;
              //Lets Check this Module if its a EventStreamFileWriter type
              edm::ParameterSet aModInEndPathPset = procPset->getParameter<edm::ParameterSet>((*it));
              if (aModInEndPathPset.empty())
                    throw cms::Exception("ParamSetWalker","ParamSetWalker") 
                          << "Empty End Path Found in the Config File" <<endl;
              std::cout <<"This Module PSet is: "<<aModInEndPathPset.toString()<<endl;
              std::string mod_type = aModInEndPathPset.getParameter<string> ("@module_type");
              std::cout <<"Type of This Module is: "<<mod_type<<endl;
              if (mod_type == "EventStreamFileWriter") {
                 cout<<"FOUND WHAT WAS LOOKING FOR:::"<<endl;
                 std::string fileName = aModInEndPathPset.getParameter<string> ("fileName");
                 std::cout <<"Streamer File Name:"<<fileName<<endl; 
                 std::string indexFileName = aModInEndPathPset.getParameter<string> ("indexFileName");
                 std::cout <<"Index File Name:"<<indexFileName<<endl;

                 edm::ParameterSet selectEventsPSet = aModInEndPathPset.getUntrackedParameter<edm::ParameterSet>("SelectEvents");
                 if ( !selectEventsPSet.empty() ) {
                    std::cout <<"SelectEvents: "<<selectEventsPSet.toString()<<endl;
                 }
              }
          }
        }
     
    }catch (cms::Exception & e) {
      std::cerr << "cms::Exception: " << e.explainSelf() << std::endl;
      std::cerr << "std::Exception: " << e.what() << std::endl;
    }
    
    } //CTOR-end
  };
  
  
int main() {
  string test_config =   "process PROD  = { 
    untracked PSet options = { 
          untracked bool wantSummary=false 
          // untracked bool makeTriggerResults=true 
    } 
    source = EmptySource { untracked int32 maxEvents = 10 } 
    module m1 = TestWalkPSets { int32 ivalue = 10 } 
    module m2 = DoubleProducer { double dvalue = 3.3 } 
    module m3 = DoubleProducer { double dvalue = 3.3 } 
    module out = EventStreamFileWriter
        {
          int32 max_event_size = 7000000
          int32 max_queue_depth = 5
          bool use_compression = true
          int32 compression_level = 1

          string fileName = \"teststreamfile.dat\"
          string indexFileName = \"testindexfile.ind\"
          # untracked int32 numPerFile = 5
          untracked PSet SelectEvents = { vstring SelectEvents={\"p2\"}}
        }

    path p1 = { m1 } 
    endpath e1 = { m2 }
    endpath e2 = { m1, m3, out }
    
 } 
 ";
  
  ParamSetWalker psetWalket(test_config);

}
