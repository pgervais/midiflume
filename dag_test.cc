#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "third_party/catch.hpp"

#include "event_processors.h"
#include "dag.h"

TEST_CASE("Empty") {
  ProcessorDAG dag;
  REQUIRE(dag.Finalize());
  REQUIRE(dag.IsFinalized());
}

TEST_CASE("Input-Output Forward") {
  ProcessorDAG dag;

  // Input and output are inserted in the order they must be run.
  auto midi_input = std::make_unique<MidiInput>("blah", nullptr);
  size_t input_index = dag.AddProcessor(std::move(midi_input));

  auto midi_output = std::make_unique<MidiOutput>("blah", nullptr);
  size_t output_index = dag.AddProcessor(std::move(midi_output));
  
  REQUIRE(dag.AddConnection(input_index, output_index));
  REQUIRE(dag.Finalize());
  REQUIRE(dag.IsFinalized());

  REQUIRE_THAT(dag.GetEvaluationOrder(),
               Catch::Equals(std::vector<size_t>({input_index, output_index})));
  
}
TEST_CASE("Input-Output Reversed") {
  ProcessorDAG dag;

  // Input and output are inserted in the opposite order they must be run.
  auto midi_output = std::make_unique<MidiOutput>("blah", nullptr);
  size_t output_index = dag.AddProcessor(std::move(midi_output));
  
  auto midi_input = std::make_unique<MidiInput>("blah", nullptr);
  size_t input_index = dag.AddProcessor(std::move(midi_input));

  REQUIRE(dag.AddConnection(input_index, output_index));
  REQUIRE(dag.Finalize());
  REQUIRE(dag.IsFinalized());

  REQUIRE_THAT(dag.GetEvaluationOrder(),
               Catch::Equals(std::vector<size_t>({input_index, output_index})));
  
}

TEST_CASE("Topological sort") {
  ProcessorDAG dag;

  size_t output_index = dag.AddProcessor(std::make_unique<MidiOutput>("blah", nullptr));  
  size_t input_index = dag.AddProcessor(std::make_unique<MidiInput>("blah", nullptr));
  size_t filter1_index = dag.AddProcessor(std::make_unique<NoteSelector>(0,64,0,127));
  size_t filter2_index = dag.AddProcessor(std::make_unique<NoteSelector>(65,127,0,127));
  
  REQUIRE(dag.AddConnection(input_index, filter1_index));
  REQUIRE(dag.AddConnection(input_index, filter2_index));
  REQUIRE(dag.AddConnection(filter1_index, output_index));
  REQUIRE(dag.AddConnection(filter2_index, output_index));
  REQUIRE(dag.Finalize());
  REQUIRE(dag.IsFinalized());

  REQUIRE_THAT(dag.GetEvaluationOrder(),
               Catch::Equals(std::vector<size_t>({input_index, filter1_index,
                                                  filter2_index, output_index})));
}

TEST_CASE("No self connection") {
  ProcessorDAG dag;

  size_t filter1_index = dag.AddProcessor(std::make_unique<NoteSelector>(0,64,0,127));
   
  REQUIRE(!dag.AddConnection(filter1_index, filter1_index));
}

