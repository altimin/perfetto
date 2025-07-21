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

#ifndef INCLUDE_PERFETTO_TRACING_SYNTHETIC_TRACK_EVENT_WRITER_H_
#define INCLUDE_PERFETTO_TRACING_SYNTHETIC_TRACK_EVENT_WRITER_H_

#include <functional>
#include <memory>
#include <set>
#include <string_view>
#include <vector>

#include "perfetto/base/export.h"
#include "perfetto/tracing/event_context.h"
#include "perfetto/tracing/internal/write_track_event_args.h"
#include "perfetto/tracing/track.h"

namespace protozero {
class ScatteredHeapBuffer;
class ScatteredStreamWriter;
template <typename T>
class RootMessage;
}  // namespace protozero

namespace perfetto {
namespace protos {
namespace pbzero {
class Trace;
}  // namespace pbzero
}  // namespace protos

namespace internal {
struct TrackEventIncrementalState;
}  // namespace internal

class PERFETTO_EXPORT_COMPONENT SyntheticTrackEventWriter {
 public:
  using Timestamp = uint64_t;

  SyntheticTrackEventWriter();
  ~SyntheticTrackEventWriter();

  SyntheticTrackEventWriter(const SyntheticTrackEventWriter&) = delete;
  SyntheticTrackEventWriter& operator=(const SyntheticTrackEventWriter&) =
      delete;

  template <typename TrackType, typename... Args>
  void instant(std::string_view name,
               TrackType&& track,
               Timestamp timestamp,
               Args&&... args);

  template <typename TrackType, typename... Args>
  void begin(std::string_view name,
             TrackType&& track,
             Timestamp timestamp,
             Args&&... args);

  template <typename TrackType, typename... Args>
  void end(TrackType&& track,
           Timestamp timestamp,
           Args&&... args);

  // Get the written trace data
  std::vector<uint8_t> GetSerializedTrace() const;

 private:
  void WriteTrackEventPacket(
      std::string_view name,
      const Track& track,
      Timestamp timestamp,
      protos::pbzero::TrackEvent::Type event_type,
      std::function<void(protos::pbzero::TrackDescriptor*)> track_serializer,
      std::function<void(EventContext)> args_writer);

  std::unique_ptr<::protozero::ScatteredHeapBuffer> buffer_;
  std::unique_ptr<::protozero::ScatteredStreamWriter> stream_;
  std::unique_ptr<internal::TrackEventIncrementalState> incremental_state_;
  std::unique_ptr<::protozero::RootMessage<::perfetto::protos::pbzero::Trace>>
      trace_;
  std::set<uint64_t> written_tracks_;
};

// Template implementation
template <typename TrackType, typename... Args>
void SyntheticTrackEventWriter::instant(std::string_view name,
                                        TrackType&& track,
                                        Timestamp timestamp,
                                        Args&&... args) {
  WriteTrackEventPacket(
      name, track, timestamp,
      protos::pbzero::TrackEvent::TYPE_INSTANT,
      [&track](protos::pbzero::TrackDescriptor* desc) {
        track.Serialize(desc);
      },
      [&](EventContext ctx) {
        internal::WriteTrackEventArgs(std::move(ctx),
                                      std::forward<Args>(args)...);
      });
}

template <typename TrackType, typename... Args>
void SyntheticTrackEventWriter::begin(std::string_view name,
                                      TrackType&& track,
                                      Timestamp timestamp,
                                      Args&&... args) {
  WriteTrackEventPacket(
      name, track, timestamp,
      protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN,
      [&track](protos::pbzero::TrackDescriptor* desc) {
        track.Serialize(desc);
      },
      [&](EventContext ctx) {
        internal::WriteTrackEventArgs(std::move(ctx),
                                      std::forward<Args>(args)...);
      });
}

template <typename TrackType, typename... Args>
void SyntheticTrackEventWriter::end(TrackType&& track,
                                    Timestamp timestamp,
                                    Args&&... args) {
  WriteTrackEventPacket(
      "", track, timestamp,  // Empty name for end events
      protos::pbzero::TrackEvent::TYPE_SLICE_END,
      [&track](protos::pbzero::TrackDescriptor* desc) {
        track.Serialize(desc);
      },
      [&](EventContext ctx) {
        internal::WriteTrackEventArgs(std::move(ctx),
                                      std::forward<Args>(args)...);
      });
}

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_SYNTHETIC_TRACK_EVENT_WRITER_H_