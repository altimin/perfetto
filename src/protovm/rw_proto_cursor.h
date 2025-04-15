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

#ifndef SRC_PROTOVM_RW_PROTO_CURSOR_H_
#define SRC_PROTOVM_RW_PROTO_CURSOR_H_

#include "perfetto/protozero/field.h"

#include "src/protovm/allocator.h"
#include "src/protovm/error_handling.h"
#include "src/protovm/node.h"

namespace perfetto {
namespace protovm {

class RwProtoCursor {
 public:
  class RepeatedFieldIterator {
   public:
    RepeatedFieldIterator();
    explicit RepeatedFieldIterator(Allocator& allocator,
                                   IntrusiveMap::Iterator it);
    RepeatedFieldIterator& operator++();
    RwProtoCursor operator*();
    explicit operator bool() const;

   private:
    Allocator* allocator_;
    IntrusiveMap::Iterator it_;
  };

  RwProtoCursor();
  explicit RwProtoCursor(Node* node, Allocator* allocator);
  StatusOr<bool> HasField(uint32_t field_id);
  StatusOr<void> EnterField(uint32_t field_id);
  StatusOr<void> EnterRepeatedFieldByIndex(uint32_t field_id, uint32_t index);
  StatusOr<RepeatedFieldIterator> IterateRepeatedField(uint32_t field_id);
  StatusOr<void> EnterRepeatedFieldByKey(uint32_t field_id,
                                         uint32_t map_key_field_id,
                                         uint64_t key);
  StatusOr<Scalar> GetScalar() const;
  StatusOr<void> SetBytes(protozero::ConstBytes data);
  StatusOr<void> SetScalar(Scalar scalar);
  StatusOr<void> Merge(protozero::ConstBytes data);
  StatusOr<void> Delete();

 private:
  StatusOr<void> ConvertToMessageIfNeeded(Node& node);
  StatusOr<UniquePtr<Node>> CreateNodeFromField(protozero::Field field);
  StatusOr<void> ConvertToMappedRepeatedFieldIfNeeded(
      Node& node,
      uint32_t map_key_field_id);
  StatusOr<void> ConvertToIndexedRepeatedFieldIfNeeded(Node& node);
  StatusOr<IntrusiveMap::Iterator> FindOrCreateMessageField(Node& node,
                                                            uint32_t field_id);
  StatusOr<IntrusiveMap::Iterator> FindOrCreateIndexedRepeatedField(
      Node& node,
      uint32_t index);
  StatusOr<IntrusiveMap::Iterator> FindOrCreateMappedRepeatedField(
      Node& node,
      uint64_t key);
  StatusOr<IntrusiveMap::Iterator> MapInsert(IntrusiveMap& map,
                                             uint64_t key,
                                             UniquePtr<Node> map_value);
  StatusOr<uint64_t> ReadScalarField(Node& node, uint32_t field_id);

  Node* node_;
  std::pair<IntrusiveMap*, Node::MapNode*> holding_map_and_node_ = {nullptr,
                                                                    nullptr};
  Allocator* allocator_;
};

}  // namespace protovm
}  // namespace perfetto

#endif  // SRC_PROTOVM_RW_PROTO_CURSOR_H_
