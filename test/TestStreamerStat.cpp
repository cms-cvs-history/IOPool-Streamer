#include "IOPool/Streamer/interface/StreamerStatService.h"
#include <iostream>

using namespace std;

void writeStat()
  {
   //Write a Stat file
    edm::StreamerStatWriteService new_stat(1, "TESTStream", "TESTStream.dat", "TESTStream.ind", "TESTStream.stat");
    for (int i=0; i!=100 ; ++i)
       {
       new_stat.incrementEventCount();
       new_stat.advanceFileSize(i*10);
       }
    new_stat.writeStat();
    cout << "Written/Appened Stat file: TESTStream.stat" << endl;
  }

void readStat()
  {
   //Read a Stat file
   edm::StreamerStatReadService old_stat("TESTStream.stat");
   while (old_stat.next() )
       {
        edm::StatSummary currentRecord = old_stat.getRecord();
        cout<< "Stat Record : " <<
        currentRecord.run_ <<
        currentRecord.streamer_ <<
        currentRecord.dataFile_ <<
        currentRecord.indexFile_ <<
        currentRecord.fileSize_ <<
        currentRecord.eventCount_ <<
        currentRecord.startDate_ <<
        currentRecord.startTime_ <<
        currentRecord.endDate_ <<
        currentRecord.endTime_ << endl;  
       }
    }

int main()

   {
       writeStat();
       readStat();
   }

