/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/tracing/synthetic_track_event_writer.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "perfetto/tracing/track.h"
#include "perfetto/tracing/track_event.h"
#include "protos/perfetto/trace/interned_data/interned_data.gen.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "protos/perfetto/trace/track_event/debug_annotation.gen.h"
#include "protos/perfetto/trace/track_event/track_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/track_event.gen.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

// Helper function to build packet descriptions from a parsed trace
std::vector<std::string> GetPacketDescriptions(
    const protos::gen::Trace& parsed_trace) {
  std::vector<std::string> packet_descriptions;
  std::map<uint64_t, std::string> interned_debug_annotation_names;

  for (const auto& packet : parsed_trace.packet()) {
    // Update interned data
    if (packet.has_interned_data()) {
      for (const auto& name : packet.interned_data().debug_annotation_names()) {
        interned_debug_annotation_names[name.iid()] = name.name();
      }
    }

    if (packet.has_track_event()) {
      const auto& track_event = packet.track_event();

      // Build packet description
      std::string description;

      // Add event type
      switch (track_event.type()) {
        case protos::gen::TrackEvent::TYPE_INSTANT:
          description = "instant";
          break;
        case protos::gen::TrackEvent::TYPE_SLICE_BEGIN:
          description = "begin";
          break;
        case protos::gen::TrackEvent::TYPE_SLICE_END:
          description = "end";
          break;
        default:
          description = "unknown";
          break;
      }

      // Add name if present
      if (track_event.has_name()) {
        description += " name=" + track_event.name();
      }

      // Add timestamp
      if (packet.has_timestamp()) {
        description += " ts=" + std::to_string(packet.timestamp());
      }

      // Add track ID
      uint64_t track_uuid = 0;
      if (track_event.has_track_uuid()) {
        track_uuid = track_event.track_uuid();
      }
      description += " track_id=" + std::to_string(track_uuid);

      // Add arguments if present
      if (track_event.debug_annotations_size() > 0) {
        description += " args=(";
        bool first = true;
        for (const auto& annotation : track_event.debug_annotations()) {
          if (!first) {
            description += ", ";
          }
          first = false;

          // Get annotation name
          std::string ann_name;
          if (annotation.has_name_iid()) {
            auto it =
                interned_debug_annotation_names.find(annotation.name_iid());
            if (it != interned_debug_annotation_names.end()) {
              ann_name = it->second;
            }
          } else if (annotation.has_name()) {
            ann_name = annotation.name();
          }

          description += ann_name + "=";

          // Get annotation value
          if (annotation.has_int_value()) {
            description += std::to_string(annotation.int_value());
          } else if (annotation.has_string_value()) {
            description += annotation.string_value();
          }
        }
        description += ")";
      }

      packet_descriptions.push_back(description);
    }
  }

  return packet_descriptions;
}

TEST(SyntheticTrackEventWriterTest, BasicFunctionality) {
  SyntheticTrackEventWriter writer;

  // Create tracks
  Track default_track;
  NamedTrack track1("MyFirstTrack");
  NamedTrack track2("MySecondTrack");

  // Write a simple instant event on default track
  writer.instant("TestEvent", default_track, 1000);

  // Write an instant event with arguments on a named track
  writer.instant("EventWithArgs", track1, 2000, "arg1", 123, "arg2",
                 "hello world");

  // Write an event with a lambda for complex arguments on another named track
  writer.instant("EventWithLambda", track2, 3000, [](EventContext ctx) {
    ctx.AddDebugAnnotation("custom_arg", 456);
  });

  // Get the serialized trace
  auto trace_data = writer.GetSerializedTrace();
  EXPECT_GT(trace_data.size(), 0u);

  // Parse the trace to verify contents
  protos::gen::Trace parsed_trace;
  EXPECT_TRUE(
      parsed_trace.ParseFromArray(trace_data.data(), trace_data.size()));

  // Collect track descriptors
  std::vector<std::string> track_descriptors;

  for (const auto& packet : parsed_trace.packet()) {
    // Assert that incremental state is not cleared
    EXPECT_FALSE(packet.sequence_flags() &
                 protos::gen::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

    // Check for track descriptors
    if (packet.has_track_descriptor()) {
      const auto& track_desc = packet.track_descriptor();
      std::string desc = "track";
      if (track_desc.has_uuid()) {
        desc += " uuid=" + std::to_string(track_desc.uuid());
      }
      // Check both name and static_name fields
      if (track_desc.has_name()) {
        desc += " name=" + track_desc.name();
      } else if (track_desc.has_static_name()) {
        desc += " name=" + track_desc.static_name();
      } else {
        desc += " name=";  // Show empty name if not present
      }
      track_descriptors.push_back(desc);
    }
  }

  // Get packet descriptions using the helper function
  std::vector<std::string> packet_descriptions =
      GetPacketDescriptions(parsed_trace);

  // Verify track descriptors are written for custom tracks
  EXPECT_THAT(
      track_descriptors,
      ::testing::ElementsAre(
          "track uuid=" + std::to_string(track1.uuid) + " name=MyFirstTrack",
          "track uuid=" + std::to_string(track2.uuid) + " name=MySecondTrack"));

  // Verify packet descriptions
  EXPECT_THAT(packet_descriptions,
              ::testing::ElementsAre(
                  "instant name=TestEvent ts=1000 track_id=" +
                      std::to_string(default_track.uuid),
                  "instant name=EventWithArgs ts=2000 track_id=" +
                      std::to_string(track1.uuid) +
                      " args=(arg1=123, arg2=hello world)",
                  "instant name=EventWithLambda ts=3000 track_id=" +
                      std::to_string(track2.uuid) + " args=(custom_arg=456)"));
}

TEST(SyntheticTrackEventWriterTest, BeginEndEvents) {
  SyntheticTrackEventWriter writer;

  // Create a track for the slice
  NamedTrack slice_track("SliceTrack");

  // Write begin and end events
  writer.begin("MySlice", slice_track, 1000, "begin_arg", 42);
  writer.end(slice_track, 2000, "end_arg", "done");

  // Get the serialized trace
  auto trace_data = writer.GetSerializedTrace();
  EXPECT_GT(trace_data.size(), 0u);

  // Parse the trace to verify contents
  protos::gen::Trace parsed_trace;
  EXPECT_TRUE(
      parsed_trace.ParseFromArray(trace_data.data(), trace_data.size()));

  // Get packet descriptions using the helper function
  std::vector<std::string> packet_descriptions =
      GetPacketDescriptions(parsed_trace);

  // Verify packet descriptions
  EXPECT_THAT(packet_descriptions,
              ::testing::ElementsAre(
                  "begin name=MySlice ts=1000 track_id=" +
                      std::to_string(slice_track.uuid) + " args=(begin_arg=42)",
                  "end ts=2000 track_id=" + std::to_string(slice_track.uuid) +
                      " args=(end_arg=done)"));
}

}  // namespace
}  // namespace perfetto