#include <algorithm>
#include <memory>
#include <iostream>
#include <queue>
#include <set>
//#include <chrono>

#include <alsa/asoundlib.h>

#include "dag.h"
#include "event_processors.h"


//using namespace std::chrono;

size_t ProcessorDAG::AddProcessor(std::unique_ptr<EventProcessor> processor) {
  size_t index = processors_.size();
  processor->init();
  // Checking for nullptr can be useful here.
  // Failure mode: calling Addprocessor(std::move(ptr)) twice with the
  // same unique_ptr.
  processors_.push_back(std::move(processor));
  parents_.emplace_back();
  children_.emplace_back();
  processed_events_.emplace_back();
  // Pre-allocate enough memory to cover most cases.
  processed_events_.back().reserve(5);
  return index;
}

size_t ProcessorDAG::AddProcessor(std::unique_ptr<EventProcessor> processor,
                                  const std::string& processor_name) {
  size_t index = AddProcessor(std::move(processor));
  name_to_index_[processor_name] = index;
  return index;
}

// Returns false if failure, true for success.
bool ProcessorDAG::AddConnection(size_t input, size_t output) {
  if (input == output) {
    std::cerr << "Cannot connect processor to itself\n";
    return false;
  }
  
  if (input >= processors_.size() || output >= processors_.size()) {
    return false;
  }
  if (input >= children_.size()) {
    std::cerr << "Invalid input index " << input << "\n";
    return false;
  }
  if (output >= parents_.size()) {
    std::cerr << "Invalid output index " << output << "\n";
    return false;
  }

  parents_[output].push_back(input);
  children_[input].push_back(output);
  return true;
}

bool ProcessorDAG::AddConnection(const std::string& input,
                                 const std::string& output) {
  size_t input_index = name_to_index_[input];
  size_t output_index = name_to_index_[output];
  return AddConnection(input_index, output_index);
}

void ProcessorDAG::ComputeEvaluationOrder(const std::vector<size_t>& outputs) {
  // Scan nodes from the inputs, compute maximum distance from the input.
  std::set<size_t> input_connected;
  std::vector<size_t> distance(processors_.size(), 0);
  {
    std::queue<size_t> to_explore;
    for (const size_t i: inputs_) {
      to_explore.push(i);
      distance[i] = 0;
      input_connected.insert(i);
    }
    
    while (!to_explore.empty()) {
      size_t current = to_explore.front();
      to_explore.pop();
      size_t d = distance[current];
      for (const size_t c: children_[current]) {
        to_explore.push(c);
        distance[c] = std::max(d+1, distance[c]);
        input_connected.insert(c);
      }
    }
  }

  // Scan nodes from the outputs
  std::set<size_t> output_connected;
  {
    std::queue<size_t> to_explore;
    for (const size_t i: outputs) {
      to_explore.push(i);
      output_connected.insert(i);
    }

    while (!to_explore.empty()) {
      size_t current = to_explore.front();
      to_explore.pop();
      for (const size_t c: parents_[current]) {
        to_explore.push(c);
        output_connected.insert(c);
      }
    }
  }

  // Keep only nodes that appear in both sets.
  evaluation_order_.clear();
  std::set_intersection(input_connected.begin(),input_connected.end(),
                        output_connected.begin(),output_connected.end(),
                        std::inserter(evaluation_order_, evaluation_order_.begin()));

  // Sort nodes by distance to an input.
  std::sort(evaluation_order_.begin(), evaluation_order_.end(),
            [&distance](size_t i, size_t j){ return distance[i] < distance[j];});
}


/* Postprocesses the processing graph.
   Mainly drops disconnected processors and computes evaluation order.
 */
bool ProcessorDAG::Finalize() {
  // DAG start and end nodes.
  inputs_.clear();
  std::vector<size_t> outputs;
  
  for (size_t i=0; i<processors_.size(); i++) {
    if (processors_[i]->HasInputs() && parents_[i].empty()) {
      std::cerr << "processor " << i << " disconnected (no inputs)\n";
    }
    if (processors_[i]->HasOutputs() && children_[i].empty()) {
      std::cerr << "processor " << i << " disconnected (no outputs)\n";
    }

    // Yes it's weird: HasOutputs() false means it's an output node.
    if (!processors_[i]->HasOutputs()) {
      outputs.push_back(i);
    }
    if (!processors_[i]->HasInputs()) {
      inputs_.push_back(i);
    }
  }

  // Topological sort of connected nodes, so that all nodes at a given
  // distance from the input are next to each other.
  // The result is stored in evaluation_order_.
  ComputeEvaluationOrder(outputs);
  finalized = true;
  return true;
}

bool ProcessorDAG::ProcessEvent(const snd_seq_event_t& ev) {
  if (!finalized) {
    std::cerr << "ProcessEvent called on a non-finalized graph.\n";
    return false;
  }
  
  // high_resolution_clock::time_point start_point = high_resolution_clock::now();
  for (const size_t processor_id : evaluation_order_) {
    processed_events_[processor_id].clear();

    // No parents for the processor: use the input event.
    if (parents_[processor_id].empty()) {
      auto events = processors_[processor_id]->ProcessEvent(ev);
      if (events->size() > 0) {
        processed_events_[processor_id].insert(
            processed_events_[processor_id].end(),
            events->begin(),
            events->end());
      }
      continue;
    }

    // When we have parents we call the processor on all events generated
    // by all parents.
    for(const size_t parent_id : parents_[processor_id]) {
      for(const snd_seq_event_t& event : processed_events_[parent_id]) {
        auto events = processors_[processor_id]->ProcessEvent(event);
        if (events->size() > 0) {
          processed_events_[processor_id].insert(processed_events_[processor_id].end(),
                                                 events->begin(),
                                                 events->end());
        }
      }
    }
  }
  // high_resolution_clock::time_point end_point = high_resolution_clock::now();
  // duration<double> time_span = duration_cast<duration<double>>(end_point - start_point);
  // std::cerr << "processing time: " << 1000*time_span.count() << " ms\n";
  
  return true;
} 
