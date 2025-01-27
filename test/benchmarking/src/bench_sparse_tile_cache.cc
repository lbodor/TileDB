/**
 * @file   bench_sparse_tile_cache.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2018-2021 TileDB, Inc.
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
 * Benchmark compressed sparse 2D read performance from the tile cache.
 */

#include <tiledb/tiledb>

#include "benchmark.h"

using namespace tiledb;

class Benchmark : public BenchmarkBase {
 public:
  Benchmark()
      : BenchmarkBase() {
    // Set the max tile cache size to 10GB -- this is more
    // than enough to guarantee that the tile cach can
    // hold all of our tiles.
    Config config;
    config["sm.tile_cache_size"] = "10000000000";
    ctx_ = std::unique_ptr<Context>(new Context(config));
  }

 protected:
  virtual void setup() {
    ArraySchema schema(*ctx_, TILEDB_SPARSE);
    Domain domain(*ctx_);
    domain.add_dimension(Dimension::create<uint32_t>(
        *ctx_,
        "d1",
        {{1, std::numeric_limits<uint32_t>::max() - tile_rows}},
        tile_rows));
    domain.add_dimension(Dimension::create<uint32_t>(
        *ctx_,
        "d2",
        {{1, std::numeric_limits<uint32_t>::max() - tile_cols}},
        tile_cols));
    schema.set_domain(domain);
    FilterList filters(*ctx_);
    filters.add_filter({*ctx_, TILEDB_FILTER_BYTESHUFFLE})
        .add_filter({*ctx_, TILEDB_FILTER_LZ4});
    schema.add_attribute(Attribute::create<int32_t>(*ctx_, "a", filters));
    Array::create(array_uri_, schema);

    // RNG coords are expensive to generate. Just make the data "sparse"
    // by skipping a few cells between each nonempty cell.
    const unsigned skip = 2;
    for (uint32_t i = 1; i < max_row; i += skip) {
      for (uint32_t j = 1; j < max_col; j += skip) {
        coords_.push_back(i);
        coords_.push_back(j);
      }
    }

    data_.resize(coords_.size() / 2);
    for (uint64_t i = 0; i < data_.size(); i++)
      data_[i] = i;

    // Write the array one time.
    Array write_array(*ctx_, array_uri_, TILEDB_WRITE);
    Query write_query(*ctx_, write_array);
    write_query.set_layout(TILEDB_UNORDERED)
        .set_data_buffer("a", data_)
        .set_coordinates(coords_);
    write_query.submit();
    write_array.close();
  }

  virtual void teardown() {
    VFS vfs(*ctx_);
    if (vfs.is_dir(array_uri_))
      vfs.remove_dir(array_uri_);
  }

  virtual void pre_run() {
    Array array(*ctx_, array_uri_, TILEDB_READ);
    auto non_empty = array.non_empty_domain<uint32_t>();
    subarray_ = {
        non_empty[0].second.first,
        non_empty[0].second.second,
        non_empty[1].second.first,
        non_empty[1].second.second};

    // Read the array one time, populating the entire tile cache.
    Array read_array(*ctx_, array_uri_, TILEDB_READ);
    Query read_query(*ctx_, read_array);
    data_.resize(read_query.est_result_size("a"));
    coords_.resize(read_query.est_result_size("TILEDB_COORDS"));
    read_query.set_subarray(subarray_)
        .set_layout(TILEDB_ROW_MAJOR)
        .set_data_buffer("a", data_)
        .set_coordinates(coords_);
    read_query.submit();
    read_array.close();
  }

  virtual void run() {
    // Read the entire array multiple times. We have already populated
    // the tile cache in pre_run().
    for (int i = 0; i < 5; ++i) {
      Array array(*ctx_, array_uri_, TILEDB_READ);
      Query query(*ctx_, array);
      query.set_subarray(subarray_)
          .set_layout(TILEDB_ROW_MAJOR)
          .set_data_buffer("a", data_)
          .set_coordinates(coords_);
      query.submit();
      array.close();
    }
  }

 private:
  const std::string array_uri_ = "bench_array";
  const unsigned tile_rows = 50, tile_cols = 50;
  const unsigned max_row = 5000, max_col = 5000;

  std::unique_ptr<Context> ctx_;
  std::vector<int> data_;
  std::vector<uint32_t> subarray_, coords_;
};

int main(int argc, char** argv) {
  Benchmark bench;
  return bench.main(argc, argv);
}
