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

#include <cstdint>
#include <fstream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/absl/status/status_matchers.h"
#include "third_party/absl/status/statusor.h"
#include "third_party/zynamics/bindiff/config.h"
#include "third_party/zynamics/bindiff/match_colors.h"
#include "third_party/zynamics/binexport/testing.h"

namespace security::bindiff {
namespace {

using ::absl_testing::IsOk;
using ::security::binexport::GetTestTempPath;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::Ne;
using ::testing::Not;
using ::testing::SizeIs;

TEST(MatchColorsTest, ReturnsManualMatchColor) {
  uint32_t manual_color = GetMatchColor(kManualMatch);
  EXPECT_THAT(manual_color, Ne(0xffffffffu));
}

TEST(MatchColorsTest, ReturnsSimilarityRampColors) {
  uint32_t color_low = GetMatchColor(0.0);
  uint32_t color_mid = GetMatchColor(0.5);
  uint32_t color_high = GetMatchColor(1.0);
  EXPECT_THAT(color_low, Ne(0xffffffffu));
  EXPECT_THAT(color_mid, Ne(0xffffffffu));
  EXPECT_THAT(color_high, Ne(0xffffffffu));
}

TEST(MatchColorsTest, OutOfBoundsValuesFallbackToWhite) {
  EXPECT_THAT(GetMatchColor(-2.0), Eq(0xffffffffu));
  EXPECT_THAT(GetMatchColor(1.5), Eq(0xffffffffu));
}

TEST(ConfigTest, DefaultsAndProtoContainExpectedAlgorithmsAndThemes) {
  const Config& defaults = config::Defaults();
  EXPECT_THAT(defaults.function_matching(), SizeIs(Ge(10)));
  EXPECT_THAT(defaults.basic_block_matching(), SizeIs(Ge(10)));
  EXPECT_THAT(defaults.themes().contains("Google Material"), Eq(true));

  Config& proto = config::Proto();
  EXPECT_THAT(proto.function_matching(), SizeIs(Ge(10)));
}

TEST(ConfigTest, AsJsonStringAndLoadFromJsonRoundTrip) {
  const Config& defaults = config::Defaults();
  std::string json = config::AsJsonString(defaults);
  EXPECT_THAT(json, Not(IsEmpty()));

  absl::StatusOr<Config> loaded = config::LoadFromJson(json);
  ASSERT_THAT(loaded, IsOk());
  EXPECT_THAT(loaded->function_matching_size(),
              Eq(defaults.function_matching_size()));
}

TEST(ConfigTest, LoadFromFileHandlesValidAndMissingFiles) {
  EXPECT_THAT(config::LoadFromFile("/nonexistent/path/to/config.json").ok(),
              IsFalse());

  const std::string filename = GetTestTempPath("test_bindiff_config.json");
  {
    std::ofstream out(filename);
    out << config::AsJsonString(config::Defaults());
  }
  absl::StatusOr<Config> loaded = config::LoadFromFile(filename);
  EXPECT_THAT(loaded, IsOk());
}

TEST(ConfigTest, MergeIntoPreservesMatchingStepsWhenSourceStepsInvalid) {
  Config target = config::Defaults();
  int original_func_steps = target.function_matching_size();
  int original_bb_steps = target.basic_block_matching_size();

  Config custom;
  custom.set_directory("/custom/bindiff/dir");
  config::MergeInto(custom, target);

  EXPECT_THAT(target.directory(), Eq("/custom/bindiff/dir"));
  EXPECT_THAT(target.function_matching_size(), Eq(original_func_steps));
  EXPECT_THAT(target.basic_block_matching_size(), Eq(original_bb_steps));
}

TEST(ConfigTest, GetBinDiffDirOrDefaultReturnsConfiguredOrDefaultPath) {
  Config custom;
  custom.set_directory("/opt/custom/bindiff");
  EXPECT_THAT(config::GetBinDiffDirOrDefault(custom),
              Eq("/opt/custom/bindiff"));

  Config empty;
  EXPECT_THAT(config::GetBinDiffDirOrDefault(empty), Not(IsEmpty()));
}

}  // namespace
}  // namespace security::bindiff
