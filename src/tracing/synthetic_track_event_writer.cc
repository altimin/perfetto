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

#include <utility>

#include <set>

#include "perfetto/protozero/message_handle.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/protozero/root_message.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/protozero/scattered_stream_writer.h"
#include "perfetto/tracing/internal/track_event_internal.h"
#include "perfetto/tracing/track.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {

SyntheticTrackEventWriter::SyntheticTrackEventWriter() {
  buffer_ = std::make_unique<::protozero::ScatteredHeapBuffer>();
  stream_ = std::make_unique<::protozero::ScatteredStreamWriter>(buffer_.get());
  buffer_->set_writer(stream_.get());
  incremental_state_ = std::make_unique<internal::TrackEventIncrementalState>();
}

SyntheticTrackEventWriter::~SyntheticTrackEventWriter() = default;

void SyntheticTrackEventWriter::WriteTrackEventPacket(
    std::string_view name,
    const Track& track,
    Timestamp timestamp,
    protos::pbzero::TrackEvent::Type event_type,
    std::function<void(protos::pbzero::TrackDescriptor*)> track_serializer,
    std::function<void(EventContext)> args_writer) {
  // We need to write each packet as a field in a Trace message
  // Create a Trace message if this is the first packet
  if (!trace_) {
    trace_ = std::make_unique<
        ::protozero::RootMessage<::perfetto::protos::pbzero::Trace>>();
    trace_->Reset(stream_.get());
  }

  // Write track descriptor for non-default tracks that haven't been written yet
  if (track.uuid != 0 &&
      written_tracks_.find(track.uuid) == written_tracks_.end()) {
    auto track_packet = trace_->add_packet();
    track_serializer(track_packet->set_track_descriptor());
    written_tracks_.insert(track.uuid);
  }

  // Create a new trace packet as a repeated field in the Trace message
  auto packet = trace_->add_packet();

  // Set the timestamp
  packet->set_timestamp(timestamp);

  // Create the TrackEvent submessage
  auto* track_event = packet->set_track_event();

  // Set the track UUID if it's not the default track
  if (track.uuid != 0) {
    track_event->set_track_uuid(track.uuid);
  }

  // Set the event type
  track_event->set_type(event_type);

  // Set the event name (skip for end events)
  if (!name.empty()) {
    track_event->set_name(name.data(), name.size());
  }

  // Create an EventContext using the TracePacketHandle constructor
  if (args_writer) {
    EventContext::TracePacketHandle packet_handle(std::move(packet));
    EventContext ctx(std::move(packet_handle), incremental_state_.get(),
                     nullptr);
    args_writer(std::move(ctx));
  }
}

std::vector<uint8_t> SyntheticTrackEventWriter::GetSerializedTrace() const {
  // Finalize the trace message before returning
  if (trace_) {
    const_cast<SyntheticTrackEventWriter*>(this)->trace_->Finalize();
  }
  return buffer_->StitchSlices();
}

}  // namespace perfetto