#ifndef Stream_DQM_Serializer_h
#define Stream_DQM_Serializer_h

/**
 * StreamDQMSerializer.h
 *
 * Utility class for translating DQM objects (monitor elements)
 * into streamer message objects.
 */

#include "IOPool/Streamer/interface/DQMEventMsgBuilder.h"
#include "TBuffer.h"
#include <vector>

namespace edm
{

  class StreamDQMSerializer
  {

  public:

    StreamDQMSerializer();

    int serializeDQMEvent(DQMEvent::TObjectTable& toTable,
                          bool use_compression, int compression_level);

    // This object always caches the results of the last event 
    // serialization operation.  You get access to the data using the
    // following member functions.

    unsigned char* bufferPointer() const { return ptr_; }
    unsigned int currentSpaceUsed() const { return curr_space_used_; }
    unsigned int currentEventSize() const { return curr_event_size_; }

  private:

    // helps to keep the data in this class exception safe
    struct Arr
    {
      explicit Arr(int sz); // allocate
      ~Arr(); // free

      char* ptr_;
    };

    // Arr data_;
    std::vector<unsigned char> comp_buf_; // space for compressed data
    unsigned int curr_event_size_;
    unsigned int curr_space_used_; // less than curr_event_size_ if compressed
    TBuffer rootbuf_;
    unsigned char* ptr_; // set to the place where the last event stored

  };

}

#endif