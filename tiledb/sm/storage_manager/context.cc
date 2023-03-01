/**
 * @file   context.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017-2021 TileDB, Inc.
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
 * This file implements class Context.
 */

#include "tiledb/common/common.h"
#include "tiledb/common/unreachable.h"

#include "tiledb/common/logger.h"
#include "tiledb/sm/storage_manager/context.h"

using namespace tiledb::common;

namespace tiledb {
namespace sm {

/* ****************************** */
/*   CONSTRUCTORS & DESTRUCTORS   */
/* ****************************** */

// Constructor.  Note order of construction:  storage_manager depends on the
// preceding members to be initialized for its initialization.
Context::Context(const Config& config)
    : last_error_(nullopt)
    , logger_(make_shared<Logger>(
          HERE(),
          logger_prefix_ + std::to_string(++logger_id_),
          get_log_level(config),
          get_log_format(config)))
    , resources_(
          config,
          logger_,
          get_compute_thread_count(config),
          get_io_thread_count(config),
          // TODO: Remove `.StorageManager` from statistic names
          // We're sticking with `Context.StorageManager` here because
          // it is part of the public facing API.
          "Context.StorageManager")
    , storage_manager_{resources_, logger_, config} {
  check_logger_config(config);
}

/* ****************************** */
/*                API             */
/* ****************************** */

optional<std::string> Context::last_error() {
  std::lock_guard<std::mutex> lock(mtx_);
  return last_error_;
}

void Context::save_error(const Status& st) {
  std::lock_guard<std::mutex> lock(mtx_);
  last_error_ = st.to_string();
}

void Context::save_error(const StatusException& st) {
  std::lock_guard<std::mutex> lock(mtx_);
  last_error_ = st.what();
}

Status Context::get_config_thread_count(
    const Config& config, uint64_t& config_thread_count_ret) {
  // The "sm.num_async_threads", "sm.num_reader_threads",
  // "sm.num_tbb_threads", "sm.num_writer_threads", and "sm.num_vfs_threads"
  // have been removed. If they are set, we will log an error message.
  // To error on the side of maintaining high-performance for
  // existing users, we will take the maximum thread count
  // among all of these configurations and set them to the new
  // "sm.compute_concurrency_level" and "sm.io_concurrency_level".
  uint64_t config_thread_count{0};

  bool found{false};
  uint64_t num_async_threads{0};
  RETURN_NOT_OK(
      config.get<uint64_t>("sm.num_async_threads", &num_async_threads, &found));
  if (found) {
    config_thread_count = std::max(config_thread_count, num_async_threads);
    logger_->status_no_return_value(Status_StorageManagerError(
        "Config parameter \"sm.num_async_threads\" has been removed; use "
        "config parameter \"sm.compute_concurrency_level\"."));
  }

  uint64_t num_reader_threads{0};
  RETURN_NOT_OK(config.get<uint64_t>(
      "sm.num_reader_threads", &num_reader_threads, &found));
  if (found) {
    config_thread_count = std::max(config_thread_count, num_reader_threads);
    logger_->status_no_return_value(Status_StorageManagerError(
        "Config parameter \"sm.num_reader_threads\" has been removed; use "
        "config parameter \"sm.compute_concurrency_level\"."));
  }

  uint64_t num_writer_threads{0};
  RETURN_NOT_OK(config.get<uint64_t>(
      "sm.num_writer_threads", &num_writer_threads, &found));
  if (found) {
    config_thread_count = std::max(config_thread_count, num_writer_threads);
    logger_->status_no_return_value(Status_StorageManagerError(
        "Config parameter \"sm.num_writer_threads\" has been removed; use "
        "config parameter \"sm.compute_concurrency_level\"."));
  }

  uint64_t num_vfs_threads{0};
  RETURN_NOT_OK(
      config.get<uint64_t>("sm.num_vfs_threads", &num_vfs_threads, &found));
  if (found) {
    config_thread_count = std::max(config_thread_count, num_vfs_threads);
    logger_->status_no_return_value(Status_StorageManagerError(
        "Config parameter \"sm.num_vfs_threads\" has been removed; use "
        "config parameter \"sm.io_concurrency_level\"."));
  }

  // The "sm.num_tbb_threads" has been deprecated. Users may still be setting
  // this configuration parameter. In this scenario, we will override the
  // compute and io concurrency levels if the configured tbb threads are
  // greater.
  int num_tbb_threads{0};
  RETURN_NOT_OK(
      config.get<int>("sm.num_tbb_threads", &num_tbb_threads, &found));
  if (found) {
    config_thread_count =
        std::max(config_thread_count, static_cast<uint64_t>(num_tbb_threads));
    logger_->status_no_return_value(Status_StorageManagerError(
        "Config parameter \"sm.num_tbb_threads\" has been removed; use "
        "config parameter \"sm.io_concurrency_level\"."));
  }

  config_thread_count_ret = static_cast<size_t>(config_thread_count);

  return Status::Ok();
}

size_t Context::get_compute_thread_count(const Config& config) {
  uint64_t config_thread_count{0};
  if (!get_config_thread_count(config, config_thread_count).ok()) {
    throw std::logic_error("Cannot get compute thread count");
  }

  bool found{false};
  uint64_t compute_concurrency_level{0};
  if (!config
           .get<uint64_t>(
               "sm.compute_concurrency_level",
               &compute_concurrency_level,
               &found)
           .ok()) {
    throw std::logic_error("Cannot get compute concurrency level");
  }
  assert(found);

  return static_cast<size_t>(
      std::max(config_thread_count, compute_concurrency_level));
}

size_t Context::get_io_thread_count(const Config& config) {
  uint64_t config_thread_count{0};
  if (!get_config_thread_count(config, config_thread_count).ok()) {
    throw std::logic_error("Cannot get config thread count");
  }

  bool found = false;
  uint64_t io_concurrency_level{0};
  if (!config
           .get<uint64_t>(
               "sm.io_concurrency_level", &io_concurrency_level, &found)
           .ok()) {
    throw std::logic_error("Cannot get io concurrency level");
  }
  assert(found);

  io_concurrency_level = std::max(config_thread_count, io_concurrency_level);

  return static_cast<size_t>(
      std::max(config_thread_count, io_concurrency_level));
}

Logger::Format Context::get_log_format(const Config& config) noexcept {
  auto opt = config.get<std::string>("config.logging_format");
  if (!opt.has_value()) {
    return Logger::Format::DEFAULT;
  }

  Logger::Format format = Logger::Format::DEFAULT;
  if (!logger_format_from_string(opt.value(), &format).ok()) {
    return Logger::Format::DEFAULT;
  }

  return format;
}

Logger::Level Context::get_log_level(const Config& config) noexcept {
  uint64_t level = static_cast<uint64_t>(Logger::Level::ERR);
  uint64_t max_level = static_cast<uint64_t>(Logger::Level::TRACE);
  try {
    auto opt = config.get<uint64_t>("config.logging_level");
    level = std::min(max_level, opt.value());
    return static_cast<Logger::Level>(level);
  } catch (...) {
    return Logger::Level::ERR;
  }

  stdx::unreachable();
}

void Context::check_logger_config(const Config& config) noexcept {
  try {
    auto cfg_format = config.get<std::string>("config.logging_format");
    if (cfg_format.has_value()) {
      if (*cfg_format != "DEFAULT" && *cfg_format != "JSON") {
        logger_->error("Ignored invalid logging format: " + *cfg_format);
      }
    } else {
      logger_->error("Missing config setting: config.logging_format");
    }

    auto cfg_level = config.get<std::string>("config.logging_level");
    if (cfg_level.has_value()) {
      if (*cfg_level != "0" && *cfg_level != "1" && *cfg_level != "2" &&
          *cfg_level != "3" && *cfg_level != "4" && *cfg_level != "5") {
        logger_->error("Ignored invalid logging level: " + *cfg_level);
      }
    } else {
      logger_->error("Missing config setting: config.logging_level");
    }
  } catch (std::exception& exc) {
    logger_->error(std::string("Error checking logging config: ") + exc.what());
  } catch (...) {
    logger_->error("Unknown error while checking logging config.");
  }
}

}  // namespace sm
}  // namespace tiledb
