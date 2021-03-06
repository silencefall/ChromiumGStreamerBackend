// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FLAGS_H_
#define NET_QUIC_QUIC_FLAGS_H_

#include <stdint.h>

#include "net/base/net_export.h"

NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_time_loss_detection;
NET_EXPORT_PRIVATE extern bool FLAGS_use_early_return_when_verifying_chlo;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_bbr_congestion_control;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_bbr;
NET_EXPORT_PRIVATE extern int64_t FLAGS_quic_time_wait_list_seconds;
NET_EXPORT_PRIVATE extern int64_t FLAGS_quic_time_wait_list_max_connections;
NET_EXPORT_PRIVATE extern bool FLAGS_enable_quic_stateless_reject_support;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_always_log_bugs_for_tests;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_disable_hpack_dynamic_table;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_enable_multipath;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_require_handshake_confirmation;
NET_EXPORT_PRIVATE extern bool FLAGS_shift_quic_cubic_epoch_when_app_limited;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_measure_headers_hol_blocking_time;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_disable_pacing;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_never_write_unencrypted_data;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_require_fix;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_supports_push_promise;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_supports_push_promise;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_cede_correctly;
NET_EXPORT_PRIVATE extern bool
    FLAGS_quic_crypto_server_config_default_has_chacha20;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_reply_to_rej;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_no_lower_bw_resumption_limit;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_sslr_limit_reduction;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_auto_tune_receive_window;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_adaptive_loss_recovery;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_enable_autotune_by_default;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_loss_recovery_use_largest_acked;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_only_one_sending_alarm;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_hash_in_scup;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_detect_memory_corrpution;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_old_public_reset_packets;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_ignore_srbf;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_allow_noprr;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_disallow_multi_packet_chlo;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_use_optimized_write_path;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_dispatcher_creates_id;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_enable_chlo_policy;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_ignore_zero_length_frames;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_no_shlo_listener;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_always_write_queued_retransmissions;
NET_EXPORT_PRIVATE extern bool FLAGS_quic_rate_based_sending;

#endif  // NET_QUIC_QUIC_FLAGS_H_
