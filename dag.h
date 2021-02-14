#ifndef _DAG_H_
#define _DAG_H_

#include <memory>
#include <unordered_map> 
#include <alsa/asoundlib.h>
#include "event_processors.h"

/* Class used to store the DAG of processors */
class ProcessorDAG {
 public:
  // Adds a processor to the DAG.
  size_t AddProcessor(std::unique_ptr<EventProcessor> processor);
  // Same as above, with a human-friendly name associated to the processor.
  // This user-friendly name has to be unique across the DAG and can be used
  // in AddConnection. Using a string makes the lua config more user-friendly
  // and provides an entry point to a processor at runtime too.
  size_t AddProcessor(std::unique_ptr<EventProcessor> processor,
                      const std::string& processor_name);

  // Adds a connection between two processors.
  // Use indices returned by AddProcessor() as input.
  bool AddConnection(size_t input, size_t output);
  // Adds a connection between two processors referred to by names.
  // Only names provided through AddProcessor can be used here:
  bool AddConnection(const std::string& input,
                     const std::string& output);

  // Call this when all processors and connections have been added.
  // It does all the precomputation and optimization to speed up
  // evaluation.
  bool Finalize();
  bool IsFinalized() { return finalized; }

  // Sends a incoming event through the processing graph. Output is
  // performed by the graph itself.
  bool ProcessEvent(const snd_seq_event_t& ev);
  
  // Returns the order in which processors will be run. For testing purposes.
  const std::vector<size_t>& GetEvaluationOrder() {
    return evaluation_order_;
  }
  
 private:
  void ComputeEvaluationOrder(const std::vector<size_t>& outputs);
  
  bool finalized = false;
  std::vector<std::unique_ptr<EventProcessor>> processors_;
  
  // The DAG structure.
  std::vector<size_t> inputs_;  // TODO: Get rid of this
  std::vector<std::vector<size_t>> children_;
  std::vector<std::vector<size_t>> parents_; 
  
  // Order in which processors must be called.
  // Elements in here that have no parents are the inputs.
  std::vector<size_t> evaluation_order_;

  // List of events generated during a single call to
  // ProcessorDAG::ProcessEvent for each processor.
  std::vector<std::vector<snd_seq_event_t>> processed_events_;

  // Mapping from processor name to index.
  std::unordered_map<std::string, size_t> name_to_index_;
};
#endif
