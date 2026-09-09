// Copyright 2011-2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/zynamics/bindiff/writer.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/status/status.h"
#include "third_party/absl/status/status_matchers.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/zynamics/bindiff/call_graph.h"
#include "third_party/zynamics/bindiff/fixed_points.h"
#include "third_party/zynamics/bindiff/flow_graph.h"
#include "third_party/zynamics/bindiff/groundtruth_writer.h"
#include "third_party/zynamics/bindiff/instruction.h"
#include "third_party/zynamics/bindiff/reader.h"
#include "third_party/zynamics/bindiff/test_util.h"

namespace security::bindiff {
namespace {

using ::absl_testing::IsOk;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;

class WriterTest : public ::testing::Test {
 protected:
  CallGraph call_graph1_;
  CallGraph call_graph2_;
  FlowGraphs flow_graphs1_;
  FlowGraphs flow_graphs2_;
  FixedPoints fixed_points_;
};

class CountingNopWriter : public Writer {
 public:
  explicit CountingNopWriter(int* counter) : counter_(counter) {}

 private:
  absl::Status Write(const CallGraph& /*call_graph1*/,
                     const CallGraph& /*call_graph2*/,
                     const FlowGraphs& /*flow_graphs1*/,
                     const FlowGraphs& /*flow_graphs2*/,
                     const FixedPoints& /*fixed_points*/) override {
    ++*counter_;
    return absl::OkStatus();
  }

  int* counter_;
};

TEST_F(WriterTest, EmptyChainDoesNothing) {
  ChainWriter chain;
  EXPECT_THAT(chain.empty(), IsTrue());
  EXPECT_THAT(chain.Write(call_graph1_, call_graph2_, flow_graphs1_,
                          flow_graphs2_, fixed_points_),
              IsOk());
}

TEST_F(WriterTest, CanChainWriters) {
  ChainWriter chain;

  int count = 0;
  chain.Add(absl::make_unique<CountingNopWriter>(&count));
  chain.Add(absl::make_unique<CountingNopWriter>(&count));
  chain.Add(absl::make_unique<CountingNopWriter>(&count));
  EXPECT_THAT(chain.empty(), IsFalse());

  EXPECT_THAT(chain.Write(call_graph1_, call_graph2_, flow_graphs1_,
                          flow_graphs2_, fixed_points_),
              IsOk());
  EXPECT_THAT(count, Eq(3));
}

TEST_F(WriterTest, GroundtruthWriterWritesFixedPoints) {
  Instruction::Cache cache;
  auto primary =
      DiffBinaryBuilder()
          .AddFunctions(
              {FunctionBuilder(0x10000, "func_a")
                   .AddBasicBlocks({BasicBlockBuilder("entry").AddInstructions({
                       InstructionBuilder("ret"),
                   })})})
          .Build(cache);
  auto secondary =
      DiffBinaryBuilder()
          .AddFunctions(
              {FunctionBuilder(0x20000, "func_b")
                   .AddBasicBlocks({BasicBlockBuilder("entry").AddInstructions({
                       InstructionBuilder("ret"),
                   })})})
          .Build(cache);

  FixedPoints fixed_points;
  fixed_points.insert(FixedPoint(*primary->flow_graphs.begin(),
                                 *secondary->flow_graphs.begin(), "manual"));

  const std::string filename =
      absl::StrCat(::testing::TempDir(), "/groundtruth_fixed_points.txt");
  GroundtruthWriter writer(filename);
  EXPECT_THAT(
      writer.Write(primary->call_graph, secondary->call_graph,
                   primary->flow_graphs, secondary->flow_graphs, fixed_points),
      IsOk());

  std::ifstream in_file(filename);
  std::stringstream buffer;
  buffer << in_file.rdbuf();
  EXPECT_THAT(buffer.str(), Eq("00010000 00020000 func_a func_b\n"));
}

TEST_F(WriterTest, GroundtruthWriterWritesFixedPointInfos) {
  FixedPointInfos fixed_point_infos;
  FixedPointInfo info;
  info.primary = 0x10000;
  info.secondary = 0x20000;
  fixed_point_infos.insert(info);

  std::string name_a = "loaded_func_a";
  std::string name_b = "loaded_func_b";
  FlowGraphInfos primary_infos;
  primary_infos[0x10000].name = &name_a;
  FlowGraphInfos secondary_infos;
  secondary_infos[0x20000].name = &name_b;

  const std::string filename =
      absl::StrCat(::testing::TempDir(), "/groundtruth_infos.txt");
  GroundtruthWriter writer(filename, fixed_point_infos, primary_infos,
                           secondary_infos);
  EXPECT_THAT(writer.Write(call_graph1_, call_graph2_, flow_graphs1_,
                           flow_graphs2_, fixed_points_),
              IsOk());

  std::ifstream in_file(filename);
  std::stringstream buffer;
  buffer << in_file.rdbuf();
  EXPECT_THAT(buffer.str(),
              Eq("00010000 00020000 loaded_func_a loaded_func_b\n"));
}

}  // namespace
}  // namespace security::bindiff
