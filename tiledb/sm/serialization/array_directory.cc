/**
 * @file   array_directory.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2022 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * This file declares serialization functions for Array Directory.
 */

// clang-format off
#ifdef TILEDB_SERIALIZATION
#include <capnp/compat/json.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include "tiledb/sm/serialization/capnp_utils.h"
#endif
// clang-format on

#include <string>

#include "tiledb/common/common.h"
#include "tiledb/sm/array/array_directory.h"
#include "tiledb/sm/enums/serialization_type.h"

using namespace tiledb::common;
using namespace tiledb::sm::stats;

namespace tiledb {
namespace sm {
namespace serialization {

#ifdef TILEDB_SERIALIZATION

void array_directory_to_capnp(
    const ArrayDirectory& array_directory,
    capnp::ArrayDirectory::Builder* array_directory_builder) {
  // set unfiltered fragment uris
  const auto& unfiltered_fragment_uris =
      array_directory.unfiltered_fragment_uris();
  if (!unfiltered_fragment_uris.empty()) {
    auto unfiltered_uris_builder =
        array_directory_builder->initUnfilteredFragmentUris(
            unfiltered_fragment_uris.size());
    for (size_t i = 0; i < unfiltered_fragment_uris.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(unfiltered_fragment_uris[i]);
      unfiltered_uris_builder.set(i, relative_uri);
    }
  }

  // set consolidated commit uris
  const auto& consolidated_commit_uris =
      array_directory.consolidated_commit_uris_set();
  if (!consolidated_commit_uris.empty()) {
    auto consolidated_commit_uris_builder =
        array_directory_builder->initConsolidatedCommitUris(
            consolidated_commit_uris.size());
    size_t i = 0;
    for (auto& uri : consolidated_commit_uris) {
      const auto& relative_uri = serialize_array_uri_to_relative(URI(uri));
      consolidated_commit_uris_builder.set(i++, relative_uri);
    }
  }

  // set array schema uris
  const auto& array_schema_uris = array_directory.array_schema_uris();
  if (!array_schema_uris.empty()) {
    auto array_schema_uris_builder =
        array_directory_builder->initArraySchemaUris(array_schema_uris.size());
    for (size_t i = 0; i < array_schema_uris.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(array_schema_uris[i]);
      array_schema_uris_builder.set(i, relative_uri);
    }
  }

  // set latest array schema uri
  if (!array_directory.latest_array_schema_uri().to_string().empty()) {
    const auto& relative_uri = serialize_array_uri_to_relative(
        array_directory.latest_array_schema_uri());
    array_directory_builder->setLatestArraySchemaUri(relative_uri);
  }

  // set array meta uris to vacuum
  const auto& array_meta_uris_to_vacuum =
      array_directory.array_meta_uris_to_vacuum();
  if (!array_meta_uris_to_vacuum.empty()) {
    auto array_meta_uris_to_vacuum_builder =
        array_directory_builder->initArrayMetaUrisToVacuum(
            array_meta_uris_to_vacuum.size());
    for (size_t i = 0; i < array_meta_uris_to_vacuum.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(array_meta_uris_to_vacuum[i]);
      array_meta_uris_to_vacuum_builder.set(i, relative_uri);
    }
  }

  // set array meta vac uris to vacuum
  const auto& array_meta_vac_uris_to_vacuum =
      array_directory.array_meta_vac_uris_to_vacuum();
  if (!array_meta_vac_uris_to_vacuum.empty()) {
    auto array_meta_vac_uris_to_vacuum_builder =
        array_directory_builder->initArrayMetaVacUrisToVacuum(
            array_meta_vac_uris_to_vacuum.size());
    for (size_t i = 0; i < array_meta_vac_uris_to_vacuum.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(array_meta_vac_uris_to_vacuum[i]);
      array_meta_vac_uris_to_vacuum_builder.set(i, relative_uri);
    }
  }

  // set commit uris to consolidate
  const auto& commit_uris_to_consolidate =
      array_directory.commit_uris_to_consolidate();
  if (!commit_uris_to_consolidate.empty()) {
    auto commit_uris_to_consolidate_builder =
        array_directory_builder->initCommitUrisToConsolidate(
            commit_uris_to_consolidate.size());
    for (size_t i = 0; i < commit_uris_to_consolidate.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(commit_uris_to_consolidate[i]);
      commit_uris_to_consolidate_builder.set(i, relative_uri);
    }
  }

  // set commit uris to vacuum
  const auto& commit_uris_to_vacuum = array_directory.commit_uris_to_vacuum();
  if (!commit_uris_to_vacuum.empty()) {
    auto commit_uris_to_vacuum_builder =
        array_directory_builder->initCommitUrisToVacuum(
            commit_uris_to_vacuum.size());
    for (size_t i = 0; i < commit_uris_to_vacuum.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(commit_uris_to_vacuum[i]);
      commit_uris_to_vacuum_builder.set(i, relative_uri);
    }
  }

  // set consolidated commit uris to vacuum
  const auto& consolidated_commit_uris_to_vacuum =
      array_directory.consolidated_commits_uris_to_vacuum();
  if (!consolidated_commit_uris_to_vacuum.empty()) {
    auto consolidated_commit_uris_to_vacuum_builder =
        array_directory_builder->initConsolidatedCommitUrisToVacuum(
            consolidated_commit_uris_to_vacuum.size());
    for (size_t i = 0; i < consolidated_commit_uris_to_vacuum.size(); i++) {
      const auto& relative_uri = serialize_array_uri_to_relative(
          consolidated_commit_uris_to_vacuum[i]);
      consolidated_commit_uris_to_vacuum_builder.set(i, relative_uri);
    }
  }

  // set array meta uris
  const auto& array_meta_uris = array_directory.array_meta_uris();
  if (!array_meta_uris.empty()) {
    auto array_meta_uris_builder =
        array_directory_builder->initArrayMetaUris(array_meta_uris.size());
    for (size_t i = 0; i < array_meta_uris.size(); i++) {
      auto timestamped_uri_builder = array_meta_uris_builder[i];
      const auto& relative_uri =
          serialize_array_uri_to_relative(array_meta_uris[i].uri_);
      timestamped_uri_builder.setUri(relative_uri);
      timestamped_uri_builder.setTimestampStart(
          array_meta_uris[i].timestamp_range_.first);
      timestamped_uri_builder.setTimestampEnd(
          array_meta_uris[i].timestamp_range_.second);
    }
  }

  // set fragment meta uris
  const auto& fragment_meta_uris = array_directory.fragment_meta_uris();
  if (!fragment_meta_uris.empty()) {
    auto fragment_meta_uris_builder =
        array_directory_builder->initFragmentMetaUris(
            fragment_meta_uris.size());
    for (size_t i = 0; i < fragment_meta_uris.size(); i++) {
      const auto& relative_uri =
          serialize_array_uri_to_relative(fragment_meta_uris[i]);
      fragment_meta_uris_builder.set(i, relative_uri);
    }
  }

  // set delete tiles location
  const auto& delete_and_update_tile_location =
      array_directory.delete_and_update_tiles_location();
  if (!delete_and_update_tile_location.empty()) {
    auto delete_and_update_tile_location_builder =
        array_directory_builder->initDeleteAndUpdateTileLocation(
            delete_and_update_tile_location.size());
    for (size_t i = 0; i < delete_and_update_tile_location.size(); i++) {
      auto del_tile_location_builder =
          delete_and_update_tile_location_builder[i];
      const auto& relative_uri = serialize_array_uri_to_relative(
          delete_and_update_tile_location[i].uri());
      del_tile_location_builder.setUri(relative_uri);
      del_tile_location_builder.setConditionMarker(
          delete_and_update_tile_location[i].condition_marker());
      del_tile_location_builder.setOffset(
          delete_and_update_tile_location[i].offset());
    }
  }

  // set timestamp start
  array_directory_builder->setTimestampStart(array_directory.timestamp_start());

  // set timestamp end
  array_directory_builder->setTimestampEnd(array_directory.timestamp_end());
}

shared_ptr<ArrayDirectory> array_directory_from_capnp(
    const capnp::ArrayDirectory::Reader& array_directory_reader,
    ContextResources& resources,
    const URI& array_uri) {
  auto array_directory =
      make_shared<ArrayDirectory>(HERE(), resources, array_uri);

  // Get unfiltered fragment uris
  if (array_directory_reader.hasUnfilteredFragmentUris()) {
    for (auto uri : array_directory_reader.getUnfilteredFragmentUris()) {
      array_directory->unfiltered_fragment_uris().reserve(
          array_directory_reader.getUnfilteredFragmentUris().size());
      array_directory->unfiltered_fragment_uris().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get consolidated commit uris
  if (array_directory_reader.hasConsolidatedCommitUris()) {
    for (auto uri : array_directory_reader.getConsolidatedCommitUris()) {
      array_directory->consolidated_commit_uris_set().reserve(
          array_directory_reader.getConsolidatedCommitUris().size());
      array_directory->consolidated_commit_uris_set().emplace(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri).to_string());
    }
  }

  // Get array schema uris
  if (array_directory_reader.hasArraySchemaUris()) {
    for (auto uri : array_directory_reader.getArraySchemaUris()) {
      array_directory->array_schema_uris().reserve(
          array_directory_reader.getArraySchemaUris().size());
      array_directory->array_schema_uris().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get latest array schema uri
  if (array_directory_reader.hasLatestArraySchemaUri()) {
    array_directory->latest_array_schema_uri() =
        URI(deserialize_array_uri_to_absolute(
            array_directory_reader.getLatestArraySchemaUri().cStr(),
            array_uri));
  }

  // Get array meta uris to vacuum
  if (array_directory_reader.hasArrayMetaUrisToVacuum()) {
    for (auto uri : array_directory_reader.getArrayMetaUrisToVacuum()) {
      array_directory->array_meta_uris_to_vacuum().reserve(
          array_directory_reader.getArrayMetaUrisToVacuum().size());
      array_directory->array_meta_uris_to_vacuum().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get array meta vac uris to vacuum
  if (array_directory_reader.hasArrayMetaVacUrisToVacuum()) {
    for (auto uri : array_directory_reader.getArrayMetaVacUrisToVacuum()) {
      array_directory->array_meta_vac_uris_to_vacuum().reserve(
          array_directory_reader.getArrayMetaVacUrisToVacuum().size());
      array_directory->array_meta_vac_uris_to_vacuum().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get commit uris to consolidate
  if (array_directory_reader.hasCommitUrisToConsolidate()) {
    for (auto uri : array_directory_reader.getCommitUrisToConsolidate()) {
      array_directory->commit_uris_to_consolidate().reserve(
          array_directory_reader.getCommitUrisToConsolidate().size());
      array_directory->commit_uris_to_consolidate().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get commit uris to vacuum
  if (array_directory_reader.hasCommitUrisToVacuum()) {
    for (auto uri : array_directory_reader.getCommitUrisToVacuum()) {
      array_directory->commit_uris_to_vacuum().reserve(
          array_directory_reader.getCommitUrisToVacuum().size());
      array_directory->commit_uris_to_vacuum().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get consolidated commit uris to vacuum
  if (array_directory_reader.hasConsolidatedCommitUrisToVacuum()) {
    for (auto uri :
         array_directory_reader.getConsolidatedCommitUrisToVacuum()) {
      array_directory->consolidated_commits_uris_to_vacuum().reserve(
          array_directory_reader.getConsolidatedCommitUrisToVacuum().size());
      array_directory->consolidated_commits_uris_to_vacuum().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get array meta uris
  if (array_directory_reader.hasArrayMetaUris()) {
    for (auto timestamp_reader : array_directory_reader.getArrayMetaUris()) {
      array_directory->array_meta_uris().reserve(
          array_directory_reader.getArrayMetaUris().size());
      array_directory->array_meta_uris().emplace_back(
          URI(deserialize_array_uri_to_absolute(
              timestamp_reader.getUri().cStr(), array_uri)),
          std::make_pair<uint64_t, uint64_t>(
              timestamp_reader.getTimestampStart(),
              timestamp_reader.getTimestampEnd()));
    }
  }

  // Get fragment meta uris
  if (array_directory_reader.hasFragmentMetaUris()) {
    for (auto uri : array_directory_reader.getFragmentMetaUris()) {
      array_directory->fragment_meta_uris().reserve(
          array_directory_reader.getFragmentMetaUris().size());
      array_directory->fragment_meta_uris().emplace_back(
          deserialize_array_uri_to_absolute(uri.cStr(), array_uri));
    }
  }

  // Get delete tiles location
  if (array_directory_reader.hasDeleteAndUpdateTileLocation()) {
    for (auto del_tile_reader :
         array_directory_reader.getDeleteAndUpdateTileLocation()) {
      array_directory->delete_and_update_tiles_location().reserve(
          array_directory_reader.getDeleteAndUpdateTileLocation().size());
      array_directory->delete_and_update_tiles_location().emplace_back(
          URI(deserialize_array_uri_to_absolute(
              del_tile_reader.getUri().cStr(), array_uri)),
          del_tile_reader.getConditionMarker(),
          del_tile_reader.getOffset());
    }
  }

  // Get timestamp start and end
  array_directory->timestamp_start() =
      array_directory_reader.getTimestampStart();
  array_directory->timestamp_end() = array_directory_reader.getTimestampEnd();

  // Set the array directory as loaded
  array_directory->loaded() = true;

  return array_directory;
}

#endif  // TILEDB_SERIALIZATION

}  // namespace serialization
}  // namespace sm
}  // namespace tiledb
