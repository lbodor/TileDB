/**
 * @file   unit-cppapi-consolidation-with-timestamps.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2022 TileDB Inc.
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
 * Tests the CPP API consolidation with timestamps.
 */

#include "catch.hpp"
#include "test/src/helpers.h"
#include "tiledb/sm/array/array_directory.h"
#include "tiledb/sm/c_api/tiledb_struct_def.h"
#include "tiledb/sm/cpp_api/tiledb"
#include "tiledb/sm/query/sparse_global_order_reader.h"

using namespace tiledb;
using namespace tiledb::test;

/** Tests for C API consolidation with timestamps. */
struct ConsolidationWithTimestampsFx {
  // Constants
  const char* SPARSE_ARRAY_NAME = "test_consolidate_sparse_array";
  const char* SPARSE_ARRAY_FRAG_DIR =
      "test_consolidate_sparse_array/__fragments";

  // TileDB context
  Context ctx_;
  VFS vfs_;
  sm::StorageManager* sm_;

  // Constructors/destructors
  ConsolidationWithTimestampsFx();
  ~ConsolidationWithTimestampsFx();

  // Functions
  void set_legacy();
  void create_sparse_array(bool allows_dups = false);
  void create_sparse_array_v11();
  void write_sparse(
      std::vector<int> a1,
      std::vector<uint64_t> dim1,
      std::vector<uint64_t> dim2,
      uint64_t timestamp);
  void write_sparse_v11(uint64_t timestamp);
  void consolidate_sparse();
  void check_timestamps_file(std::vector<uint64_t> expected);
  void read_sparse(
      std::vector<int>& a1,
      std::vector<uint64_t>& dim1,
      std::vector<uint64_t>& dim2,
      std::string& stats,
      tiledb_layout_t layout,
      uint64_t timestamp);
  void reopen_sparse(
      std::vector<int>& a1,
      std::vector<uint64_t>& dim1,
      std::vector<uint64_t>& dim2,
      std::string& stats,
      tiledb_layout_t layout,
      uint64_t start_time,
      uint64_t end_time);

  void remove_sparse_array();
  void remove_array(const std::string& array_name);
  bool is_array(const std::string& array_name);
};

ConsolidationWithTimestampsFx::ConsolidationWithTimestampsFx()
    : vfs_(ctx_) {
  Config config;
  config.set("sm.consolidation.with_timestamps", "true");
  config.set("sm.consolidation.buffer_size", "1000");
  ctx_ = Context(config);
  sm_ = ctx_.ptr().get()->ctx_->storage_manager();
  vfs_ = VFS(ctx_);
}

ConsolidationWithTimestampsFx::~ConsolidationWithTimestampsFx() {
}

void ConsolidationWithTimestampsFx::set_legacy() {
  Config config;
  config.set("sm.consolidation.with_timestamps", "true");
  config.set("sm.consolidation.buffer_size", "1000");
  config.set("sm.query.sparse_global_order.reader", "legacy");
  config.set("sm.query.sparse_unordered_with_dups.reader", "legacy");

  ctx_ = Context(config);
  sm_ = ctx_.ptr().get()->ctx_->storage_manager();
  vfs_ = VFS(ctx_);
}

void ConsolidationWithTimestampsFx::create_sparse_array(bool allows_dups) {
  // Create dimensions
  auto d1 = Dimension::create<uint64_t>(ctx_, "d1", {{1, 4}}, 2);
  auto d2 = Dimension::create<uint64_t>(ctx_, "d2", {{1, 4}}, 2);

  // Create domain
  Domain domain(ctx_);
  domain.add_dimension(d1);
  domain.add_dimension(d2);

  // Create attributes
  auto a1 = Attribute::create<int32_t>(ctx_, "a1");

  // Create array schmea
  ArraySchema schema(ctx_, TILEDB_SPARSE);
  schema.set_domain(domain);
  schema.set_capacity(20);
  schema.add_attributes(a1);

  if (allows_dups) {
    schema.set_allows_dups(true);
  }

  // Set up filters
  Filter filter(ctx_, TILEDB_FILTER_NONE);
  FilterList filter_list(ctx_);
  filter_list.add_filter(filter);
  schema.set_coords_filter_list(filter_list);

  Array::create(SPARSE_ARRAY_NAME, schema);
}

void ConsolidationWithTimestampsFx::create_sparse_array_v11() {
  // Get the v11 sparse array.
  std::string v11_arrays_dir =
      std::string(TILEDB_TEST_INPUTS_DIR) + "/arrays/sparse_array_v11";
  REQUIRE(
      tiledb_vfs_copy_dir(
          ctx_.ptr().get(),
          vfs_.ptr().get(),
          v11_arrays_dir.c_str(),
          SPARSE_ARRAY_NAME) == TILEDB_OK);
}

void ConsolidationWithTimestampsFx::write_sparse(
    std::vector<int> a1,
    std::vector<uint64_t> dim1,
    std::vector<uint64_t> dim2,
    uint64_t timestamp) {
  // Open array
  Array array(ctx_, SPARSE_ARRAY_NAME, TILEDB_WRITE, timestamp);

  // Create query
  Query query(ctx_, array, TILEDB_WRITE);
  query.set_layout(TILEDB_GLOBAL_ORDER);
  query.set_data_buffer("a1", a1);
  query.set_data_buffer("d1", dim1);
  query.set_data_buffer("d2", dim2);

  // Submit/finalize the query
  query.submit();
  query.finalize();

  // Close array
  array.close();
}

void ConsolidationWithTimestampsFx::write_sparse_v11(uint64_t timestamp) {
  // Prepare cell buffers
  std::vector<int> buffer_a1{0, 1, 2, 3};
  std::vector<uint64_t> buffer_a2{0, 1, 3, 6};
  std::string buffer_var_a2("abbcccdddd");
  std::vector<float> buffer_a3{0.1f, 0.2f, 1.1f, 1.2f, 2.1f, 2.2f, 3.1f, 3.2f};
  std::vector<uint64_t> buffer_coords_dim1{1, 1, 1, 2};
  std::vector<uint64_t> buffer_coords_dim2{1, 2, 4, 3};

  // Open array
  Array array(ctx_, SPARSE_ARRAY_NAME, TILEDB_WRITE, timestamp);

  // Create query
  Query query(ctx_, array, TILEDB_WRITE);
  query.set_layout(TILEDB_GLOBAL_ORDER);
  query.set_data_buffer("a1", buffer_a1);
  query.set_data_buffer(
      "a2", (void*)buffer_var_a2.c_str(), buffer_var_a2.size());
  query.set_offsets_buffer("a2", buffer_a2);
  query.set_data_buffer("a3", buffer_a3);
  query.set_data_buffer("d1", buffer_coords_dim1);
  query.set_data_buffer("d2", buffer_coords_dim2);

  // Submit/finalize the query
  query.submit();
  query.finalize();

  // Close array
  array.close();
}

void ConsolidationWithTimestampsFx::consolidate_sparse() {
  auto config = ctx_.config();
  Array::consolidate(ctx_, SPARSE_ARRAY_NAME, &config);
}

void ConsolidationWithTimestampsFx::check_timestamps_file(
    std::vector<uint64_t> expected) {
  // Find consolidated fragment URI.
  std::string consolidated_fragment_uri;
  auto uris = vfs_.ls(SPARSE_ARRAY_FRAG_DIR);
  for (auto& uri : uris) {
    if (uri.find("__1_2_") != std::string::npos) {
      consolidated_fragment_uri = uri;
      break;
    }
  }

  auto timestamps_file = consolidated_fragment_uri + "/t.tdb";

  VFS::filebuf buf(vfs_);
  buf.open(timestamps_file, std::ios::in);
  std::istream is(&buf);
  REQUIRE(is.good());

  uint64_t num_tiles;
  is.read((char*)&num_tiles, sizeof(uint64_t));
  REQUIRE(num_tiles == 1);

  uint32_t filtered_size;
  is.read((char*)&filtered_size, sizeof(uint32_t));
  REQUIRE(filtered_size == expected.size() * sizeof(uint64_t));

  uint32_t unfiltered_size;
  is.read((char*)&unfiltered_size, sizeof(uint32_t));
  REQUIRE(unfiltered_size == expected.size() * sizeof(uint64_t));

  uint32_t md_size;
  is.read((char*)&md_size, sizeof(uint32_t));
  REQUIRE(md_size == 0);

  std::vector<uint64_t> written(unfiltered_size / sizeof(uint64_t));
  is.read((char*)written.data(), unfiltered_size);

  for (uint64_t i = 0; i < expected.size(); i++) {
    if (expected[i] == std::numeric_limits<uint64_t>::max()) {
      CHECK((written[i] == 1 || written[i] == 2));
    } else {
      CHECK(expected[i] == written[i]);
    }
  }
}

void ConsolidationWithTimestampsFx::read_sparse(
    std::vector<int>& a1,
    std::vector<uint64_t>& dim1,
    std::vector<uint64_t>& dim2,
    std::string& stats,
    tiledb_layout_t layout,
    uint64_t timestamp) {
  // Open array
  Array array(ctx_, SPARSE_ARRAY_NAME, TILEDB_READ, timestamp);

  // Create query
  Query query(ctx_, array, TILEDB_READ);
  query.set_layout(layout);
  query.set_data_buffer("a1", a1);
  query.set_data_buffer("d1", dim1);
  query.set_data_buffer("d2", dim2);

  // Submit/finalize the query
  query.submit();
  CHECK(query.query_status() == Query::Status::COMPLETE);

  // Get the query stats.
  stats = query.stats();

  // Close array
  array.close();
}

void ConsolidationWithTimestampsFx::reopen_sparse(
    std::vector<int>& a1,
    std::vector<uint64_t>& dim1,
    std::vector<uint64_t>& dim2,
    std::string& stats,
    tiledb_layout_t layout,
    uint64_t start_time,
    uint64_t end_time) {
  // Re-open array
  Array array(ctx_, SPARSE_ARRAY_NAME, TILEDB_READ);
  array.set_open_timestamp_start(start_time);
  array.set_open_timestamp_end(end_time);
  array.reopen();

  // Create query
  Query query(ctx_, array, TILEDB_READ);
  query.set_layout(layout);
  query.set_data_buffer("a1", a1);
  query.set_data_buffer("d1", dim1);
  query.set_data_buffer("d2", dim2);

  // Submit/finalize the query
  query.submit();
  CHECK(query.query_status() == Query::Status::COMPLETE);

  // Get the query stats.
  stats = query.stats();

  // Close array
  array.close();
}

void ConsolidationWithTimestampsFx::remove_array(
    const std::string& array_name) {
  if (!is_array(array_name))
    return;

  vfs_.remove_dir(array_name);
}

void ConsolidationWithTimestampsFx::remove_sparse_array() {
  remove_array(SPARSE_ARRAY_NAME);
}

bool ConsolidationWithTimestampsFx::is_array(const std::string& array_name) {
  return vfs_.is_dir(array_name);
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps",
    "[cppapi][consolidation-with-timestamps][write-check]") {
  remove_sparse_array();
  create_sparse_array();

  // Write first fragment.
  write_sparse(
      {0, 1, 2, 3, 4, 5, 6, 7},
      {1, 1, 1, 2, 3, 4, 3, 3},
      {1, 2, 4, 3, 1, 2, 3, 4},
      1);

  // Write second fragment.
  write_sparse({8, 9, 10, 11}, {2, 2, 3, 3}, {2, 3, 2, 3}, 2);

  // Consolidate.
  consolidate_sparse();

  // Check t.tdb file.
  const uint64_t X = std::numeric_limits<uint64_t>::max();
  check_timestamps_file({1, 1, 2, 1, X, X, 1, 2, 1, X, X, 1});

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, check directory contents",
    "[cppapi][consolidation-with-timestamps][sparse-unordered-with-dups]") {
  remove_sparse_array();
  create_sparse_array(true);

  // Write first fragment.
  write_sparse({0, 1, 2, 3}, {1, 1, 1, 2}, {1, 2, 4, 3}, 1);

  // Write second fragment.
  write_sparse({8, 9, 10, 11}, {2, 2, 3, 3}, {2, 3, 2, 3}, 3);

  // Consolidate.
  consolidate_sparse();

  sm::URI array_uri(SPARSE_ARRAY_NAME);
  ThreadPool tp(2);
  // Partial coverage of lower timestamp
  sm::ArrayDirectory array_dir(sm_->vfs(), &tp, array_uri, 0, 2, true);
  // Check that only the consolidated fragment is visible
  auto fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  auto ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 1);
  CHECK(ts_range.second == 3);

  // Partial coverage of upper timestamp
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 2, 10, true);
  // Check that only the consolidated fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 1);
  CHECK(ts_range.second == 3);

  // Full coverage
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 0, 5, true);
  // Check that only the consolidated fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 1);
  CHECK(ts_range.second == 3);

  // Boundary case
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 3, 5, true);
  // Check that only the consolidated fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 1);
  CHECK(ts_range.second == 3);

  // No coverage - later read
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 4, 5, true);
  // Check that no fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 0);

  // No coverage - earlier read
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 0, 0, true);
  // Check that no fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 0);

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, global read",
    "[cppapi][consolidation-with-timestamps][global-read]") {
  remove_sparse_array();
  create_sparse_array();

  // Write first fragment.
  write_sparse(
      {0, 1, 2, 3, 4, 5, 6, 7},
      {1, 1, 1, 2, 3, 4, 3, 3},
      {1, 2, 4, 3, 1, 2, 3, 4},
      1);

  // Write second fragment.
  write_sparse({8, 9, 10, 11}, {2, 2, 3, 3}, {2, 3, 2, 3}, 2);

  // Consolidate.
  consolidate_sparse();

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  uint64_t buffer_size = 10;
  if (legacy) {
    buffer_size = 100;
    set_legacy();
  }

  std::string stats;
  std::vector<int> a1(buffer_size);
  std::vector<uint64_t> dim1(buffer_size);
  std::vector<uint64_t> dim2(buffer_size);
  read_sparse(a1, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 3);

  std::vector<int> c_a1 = {0, 1, 8, 2, 9, 4, 10, 5, 11, 7};
  std::vector<uint64_t> c_dim1 = {1, 1, 2, 1, 2, 3, 3, 4, 3, 3};
  std::vector<uint64_t> c_dim2 = {1, 2, 2, 4, 3, 1, 2, 2, 3, 4};
  CHECK(!memcmp(c_a1.data(), a1.data(), c_a1.size() * sizeof(int)));
  CHECK(!memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
  CHECK(!memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));

  remove_sparse_array();
}

#ifndef _WIN32
TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, check directory contents of "
    "old array",
    "[cppapi][consolidation-with-timestamps][sparse-unordered-with-dups]") {
  remove_sparse_array();
  create_sparse_array_v11();
  // Write first fragment.
  write_sparse_v11(1);

  // Write second fragment.
  write_sparse_v11(3);

  // Consolidate.
  consolidate_sparse();

  sm::URI array_uri(SPARSE_ARRAY_NAME);
  ThreadPool tp(2);

  // Partial coverage of lower timestamp
  sm::ArrayDirectory array_dir(sm_->vfs(), &tp, array_uri, 0, 2, true);
  // Check that only the first fragment is visible on an old array
  auto fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  auto ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 1);
  CHECK(ts_range.second == 1);

  // Partial coverage of upper timestamp
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 2, 10, true);
  // Check that only the second fragment is visible on an old array
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 3);
  CHECK(ts_range.second == 3);

  // Full coverage
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 0, 5, true);
  // Check that only the consolidated fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 1);
  CHECK(ts_range.second == 3);

  // Boundary case
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 3, 5, true);
  // Check that only the second fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 1);
  ts_range = fragments[0].timestamp_range_;
  CHECK(ts_range.first == 3);
  CHECK(ts_range.second == 3);

  // No coverage - later read
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 4, 5, true);
  // Check that no fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 0);

  // No coverage - earlier read
  array_dir = sm::ArrayDirectory(sm_->vfs(), &tp, array_uri, 0, 0, true);
  // Check that no fragment is visible
  fragments = array_dir.fragment_uris();
  CHECK(fragments.size() == 0);

  remove_sparse_array();
}
#endif

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, global read, all cells same "
    "coords",
    "[cppapi][consolidation-with-timestamps][global-read][same-coords]") {
  remove_sparse_array();
  create_sparse_array();

  // Write fragments.
  for (uint64_t i = 0; i < 50; i++) {
    std::vector<int> a(1);
    a[0] = i + 1;
    write_sparse(a, {1}, {1}, i + 1);
  }

  // Consolidate.
  consolidate_sparse();

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  uint64_t buffer_size = 1;
  if (legacy) {
    set_legacy();
    buffer_size = 100;
  }

  std::string stats;
  std::vector<int> a1(buffer_size);
  std::vector<uint64_t> dim1(buffer_size);
  std::vector<uint64_t> dim2(buffer_size);
  read_sparse(a1, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 51);

  CHECK(a1[0] == 50);
  CHECK(dim1[0] == 1);
  CHECK(dim2[0] == 1);

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, global read, same coords "
    "across tiles",
    "[cppapi][consolidation-with-timestamps][global-read][across-tiles]") {
  remove_sparse_array();
  create_sparse_array();

  // Write fragments.
  // We write 8 cells per fragments for 6 fragments. Then it gets consolidated
  // into one. So we'll get in order 6xcell1, 6xcell2... total 48 cells. Tile
  // capacity is 20 so we'll end up with 3 tiles. First break in the tiles will
  // be in the middle of cell3, second will be in the middle of the cells7.
  for (uint64_t i = 0; i < 6; i++) {
    write_sparse(
        {1, 2, 3, 4, 5, 6, 7, 8},
        {1, 1, 2, 2, 1, 1, 2, 2},
        {1, 2, 1, 2, 3, 4, 3, 4},
        i + 1);
  }

  // Consolidate.
  consolidate_sparse();

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  uint64_t buffer_size = 8;
  if (legacy) {
    set_legacy();
    buffer_size = 100;
  }

  std::string stats;
  std::vector<int> a1(buffer_size);
  std::vector<uint64_t> dim1(buffer_size);
  std::vector<uint64_t> dim2(buffer_size);
  read_sparse(a1, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 7);

  std::vector<int> c_a1 = {1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<uint64_t> c_dim1 = {1, 1, 2, 2, 1, 1, 2, 2};
  std::vector<uint64_t> c_dim2 = {1, 2, 1, 2, 3, 4, 3, 4};
  CHECK(!memcmp(c_a1.data(), a1.data(), c_a1.size() * sizeof(int)));
  CHECK(!memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
  CHECK(!memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, global read, all cells same "
    "coords, with memory budget",
    "[cppapi][consolidation-with-timestamps][global-read][same-coords][mem-"
    "budget]") {
  remove_sparse_array();
  create_sparse_array();

  // Write fragments.
  for (uint64_t i = 0; i < 50; i++) {
    std::vector<int> a(1);
    a[0] = i + 1;
    write_sparse(a, {1}, {1}, i + 1);
  }

  // Consolidate.
  consolidate_sparse();

  // Will only allow to load two tiles out of 3.
  Config cfg;
  cfg.set("sm.mem.total_budget", "10000");
  cfg.set("sm.mem.reader.sparse_global_order.ratio_coords", "0.35");
  ctx_ = Context(cfg);

  std::string stats;
  std::vector<int> a1(1);
  std::vector<uint64_t> dim1(1);
  std::vector<uint64_t> dim2(1);
  read_sparse(a1, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 51);

  CHECK(a1[0] == 50);
  CHECK(dim1[0] == 1);
  CHECK(dim2[0] == 1);

  // Make sure there was an internal loop on the reader.
  CHECK(
      stats.find("\"Context.StorageManager.Query.Reader.loop_num\": 2") !=
      std::string::npos);

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, global read, same cells "
    "across tiles, with memory budget",
    "[cppapi][consolidation-with-timestamps][global-read][across-tiles][mem-"
    "budget]") {
  remove_sparse_array();
  create_sparse_array();

  // Write fragments.
  // We write 8 cells per fragment for 6 fragments. Then it gets consolidated
  // into one. So we'll get in order 6xcell1, 6xcell2... total 48 cells. Tile
  // capacity is 20 so we'll end up with 3 tiles. First break in the tiles will
  // be in the middle of cell3, second will be in the middle of the cells7.
  for (uint64_t i = 0; i < 6; i++) {
    write_sparse(
        {1, 2, 3, 4, 5, 6, 7, 8},
        {1, 1, 2, 2, 1, 1, 2, 2},
        {1, 2, 1, 2, 3, 4, 3, 4},
        i + 1);
  }

  // Consolidate.
  consolidate_sparse();

  // Will only allow to load two tiles out of 3.
  Config cfg;
  cfg.set("sm.mem.total_budget", "10000");
  cfg.set("sm.mem.reader.sparse_global_order.ratio_coords", "0.35");
  ctx_ = Context(cfg);

  std::string stats;
  std::vector<int> a1(9);
  std::vector<uint64_t> dim1(9);
  std::vector<uint64_t> dim2(9);
  read_sparse(a1, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 7);

  std::vector<int> c_a1 = {1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<uint64_t> c_dim1 = {1, 1, 2, 2, 1, 1, 2, 2};
  std::vector<uint64_t> c_dim2 = {1, 2, 1, 2, 3, 4, 3, 4};
  CHECK(!memcmp(c_a1.data(), a1.data(), c_a1.size() * sizeof(int)));
  CHECK(!memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
  CHECK(!memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));

  // Make sure there was an internal loop on the reader.
  CHECK(
      stats.find("\"Context.StorageManager.Query.Reader.loop_num\": 2") !=
      std::string::npos);

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, partial read with dups",
    "[cppapi][consolidation-with-timestamps][partial-read][dups]") {
  remove_sparse_array();
  // enable duplicates
  create_sparse_array(true);

  // Write first fragment.
  write_sparse({0, 1, 2, 3}, {1, 1, 1, 2}, {1, 2, 4, 3}, 1);

  // Write second fragment.
  write_sparse({8, 9, 10, 11}, {2, 2, 3, 3}, {2, 3, 2, 3}, 3);

  // Consolidate.
  consolidate_sparse();

  tiledb_layout_t layout = GENERATE(TILEDB_UNORDERED, TILEDB_GLOBAL_ORDER);

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  if (legacy) {
    set_legacy();
  }

  std::string stats;
  std::vector<int> a(10);
  std::vector<uint64_t> dim1(10);
  std::vector<uint64_t> dim2(10);
  SECTION("Read after all writes") {
    // Read after both writes - should see everything
    read_sparse(a, dim1, dim2, stats, layout, 4);

    std::vector<int> c_a_opt1 = {0, 1, 8, 2, 3, 9, 10, 11};
    std::vector<int> c_a_opt2 = {0, 1, 8, 2, 9, 3, 10, 11};
    std::vector<uint64_t> c_dim1 = {1, 1, 2, 1, 2, 2, 3, 3};
    std::vector<uint64_t> c_dim2 = {1, 2, 2, 4, 3, 3, 2, 3};
    CHECK(
        (!memcmp(c_a_opt1.data(), a.data(), c_a_opt1.size() * sizeof(int)) ||
         !memcmp(c_a_opt2.data(), a.data(), c_a_opt2.size() * sizeof(int))));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  SECTION("Read between the 2 writes") {
    // Should see only first write
    read_sparse(a, dim1, dim2, stats, layout, 2);

    std::vector<int> c_a = {0, 1, 2, 3};
    std::vector<uint64_t> c_dim1 = {1, 1, 1, 2};
    std::vector<uint64_t> c_dim2 = {1, 2, 4, 3};
    CHECK(!memcmp(c_a.data(), a.data(), c_a.size() * sizeof(int)));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, partial read without dups",
    "[cppapi][consolidation-with-timestamps][partial-read][no-dups]") {
  remove_sparse_array();
  // no duplicates
  create_sparse_array();

  // Write first fragment.
  write_sparse({0, 1, 2, 3}, {1, 1, 1, 2}, {1, 2, 4, 3}, 1);

  // Write second fragment.
  write_sparse({8, 9, 10, 11}, {2, 2, 3, 3}, {2, 3, 2, 3}, 3);

  // Consolidate.
  consolidate_sparse();

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  if (legacy) {
    set_legacy();
  }

  std::string stats;
  std::vector<int> a(10);
  std::vector<uint64_t> dim1(10);
  std::vector<uint64_t> dim2(10);
  SECTION("Read after all writes") {
    read_sparse(a, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 4);

    std::vector<int> c_a = {0, 1, 8, 2, 9, 10, 11};
    std::vector<uint64_t> c_dim1 = {1, 1, 2, 1, 2, 3, 3};
    std::vector<uint64_t> c_dim2 = {1, 2, 2, 4, 3, 2, 3};
    CHECK(!memcmp(c_a.data(), a.data(), sizeof(c_a)));
    CHECK(!memcmp(c_dim1.data(), dim1.data(), sizeof(c_dim1)));
    CHECK(!memcmp(c_dim2.data(), dim2.data(), sizeof(c_dim2)));
  }

  SECTION("Read between the 2 writes") {
    read_sparse(a, dim1, dim2, stats, TILEDB_GLOBAL_ORDER, 2);

    std::vector<int> c_a = {0, 1, 2, 3};
    std::vector<uint64_t> c_dim1 = {1, 1, 1, 2};
    std::vector<uint64_t> c_dim2 = {1, 2, 4, 3};
    CHECK(!memcmp(c_a.data(), a.data(), c_a.size() * sizeof(int)));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, full and partial read with "
    "dups",
    "[cppapi][consolidation-with-timestamps][partial-read][dups]") {
  remove_sparse_array();
  // enable duplicates
  create_sparse_array(true);

  // Write first fragment.
  write_sparse({0, 1, 2, 3}, {1, 1, 1, 2}, {1, 2, 4, 3}, 1);
  // Write second fragment.
  write_sparse({4, 5, 6, 7}, {2, 2, 3, 3}, {2, 4, 2, 3}, 3);

  // Consolidate.
  consolidate_sparse();

  // Write third fragment.
  write_sparse({8, 9, 10, 11}, {2, 1, 3, 4}, {1, 3, 1, 1}, 4);
  // Write fourth fragment.
  write_sparse({12, 13, 14, 15}, {4, 3, 3, 4}, {2, 3, 4, 4}, 6);

  // Consolidate.
  consolidate_sparse();

  tiledb_layout_t layout = GENERATE(TILEDB_UNORDERED, TILEDB_GLOBAL_ORDER);

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  if (legacy) {
    set_legacy();
  }

  std::string stats;
  // TBD: why 15 in user buffer not enough?
  std::vector<int> a(16);
  std::vector<uint64_t> dim1(16);
  std::vector<uint64_t> dim2(16);

  SECTION("Read after all writes") {
    // Read after both writes - should see everything
    read_sparse(a, dim1, dim2, stats, layout, 7);
    std::vector<int> c_a_opt1 = {
        0, 1, 8, 4, 9, 2, 3, 5, 10, 6, 11, 12, 7, 13, 14, 15};
    std::vector<int> c_a_opt2 = {
        0, 1, 8, 4, 9, 2, 3, 5, 10, 6, 11, 12, 13, 7, 14, 15};
    std::vector<uint64_t> c_dim1 = {
        1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4, 3, 3, 3, 4};
    std::vector<uint64_t> c_dim2 = {
        1, 2, 1, 2, 3, 4, 3, 4, 1, 2, 1, 2, 3, 3, 4, 4};
    CHECK(
        (!memcmp(c_a_opt1.data(), a.data(), c_a_opt1.size() * sizeof(int)) ||
         !memcmp(c_a_opt2.data(), a.data(), c_a_opt2.size() * sizeof(int))));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  // Read with full coverage on first 2 consolidated, partial coverage on second
  // 2 consolidated
  SECTION("Read between the 2 writes") {
    read_sparse(a, dim1, dim2, stats, layout, 5);

    std::vector<int> c_a = {0, 1, 8, 4, 9, 2, 3, 5, 10, 6, 11, 7};
    std::vector<uint64_t> c_dim1 = {1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 3};
    std::vector<uint64_t> c_dim2 = {1, 2, 1, 2, 3, 4, 3, 4, 1, 2, 1, 3};
    CHECK(!memcmp(c_a.data(), a.data(), c_a.size() * sizeof(int)));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }
  remove_sparse_array();
}

TEST_CASE_METHOD(
    ConsolidationWithTimestampsFx,
    "CPP API: Test consolidation with timestamps, reopen",
    "[cppapi][consolidation-with-timestamps][partial-read][dups]") {
  remove_sparse_array();
  // enable duplicates
  create_sparse_array(true);

  // Write first fragment.
  write_sparse({0, 1, 2, 3}, {1, 1, 1, 2}, {1, 2, 4, 3}, 1);
  // Write second fragment.
  write_sparse({4, 5, 6, 7}, {2, 2, 3, 3}, {2, 3, 2, 3}, 3);
  // Write third  fragment.
  write_sparse({8, 9, 10, 11}, {1, 2, 3, 4}, {3, 4, 1, 1}, 5);

  // Consolidate.
  consolidate_sparse();

  tiledb_layout_t layout = GENERATE(TILEDB_UNORDERED, TILEDB_GLOBAL_ORDER);

  // Test read for both refactored and legacy.
  bool legacy = GENERATE(true, false);
  if (legacy) {
    set_legacy();
  }

  std::string stats;
  std::vector<int> a(16);
  std::vector<uint64_t> dim1(16);
  std::vector<uint64_t> dim2(16);

  SECTION("Read in the middle") {
    // Read between 2 and 4
    reopen_sparse(a, dim1, dim2, stats, layout, 2, 4);

    // Expect to read only what was written at time 3
    std::vector<int> c_a = {4, 5, 6, 7};
    std::vector<uint64_t> c_dim1 = {2, 2, 3, 3};
    std::vector<uint64_t> c_dim2 = {2, 3, 2, 3};
    CHECK(!memcmp(c_a.data(), a.data(), c_a.size() * sizeof(int)));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  SECTION("Read the last 2 writes only") {
    reopen_sparse(a, dim1, dim2, stats, layout, 2, 6);

    // Expect to read what the last 2 writes wrote
    std::vector<int> c_a = {4, 8, 5, 9, 10, 6, 11, 7};
    std::vector<uint64_t> c_dim1 = {2, 1, 2, 2, 3, 3, 4, 3};
    std::vector<uint64_t> c_dim2 = {2, 3, 3, 4, 1, 2, 1, 3};
    CHECK(!memcmp(c_a.data(), a.data(), c_a.size() * sizeof(int)));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  SECTION("Read the first 2 writes only") {
    reopen_sparse(a, dim1, dim2, stats, layout, 0, 4);

    // Expect to read what the first 2 writes wrote
    std::vector<int> c_a_opt1 = {0, 1, 4, 2, 5, 3, 6, 7};
    std::vector<int> c_a_opt2 = {0, 1, 4, 2, 3, 5, 6, 7};
    std::vector<uint64_t> c_dim1 = {1, 1, 2, 1, 2, 2, 3, 3};
    std::vector<uint64_t> c_dim2 = {1, 2, 2, 4, 3, 3, 2, 3};
    CHECK(
        (!memcmp(c_a_opt1.data(), a.data(), c_a_opt1.size() * sizeof(int)) ||
         !memcmp(c_a_opt2.data(), a.data(), c_a_opt2.size() * sizeof(int))));
    CHECK(
        !memcmp(c_dim1.data(), dim1.data(), c_dim1.size() * sizeof(uint64_t)));
    CHECK(
        !memcmp(c_dim2.data(), dim2.data(), c_dim2.size() * sizeof(uint64_t)));
  }

  remove_sparse_array();
}