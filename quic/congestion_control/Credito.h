/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#pragma once

#include <quic/QuicException.h>
#include <quic/congestion_control/third_party/windowed_filter.h>
#include <quic/state/StateData.h>

#include <limits>

namespace quic {

using namespace std::chrono_literals;
constexpr std::chrono::microseconds kMinRTTWindowLength{10s};

class Credito : public CongestionController {
 public:
  explicit Credito(QuicConnectionStateBase& conn);
  void onRemoveBytesFromInflight(uint64_t) override;
  void onPacketSent(const OutstandingPacket& packet) override;
  void onPacketAckOrLoss(folly::Optional<AckEvent>, folly::Optional<LossEvent>)
      override;

  uint64_t getWritableBytes() const noexcept override;
  uint64_t getCongestionWindow() const noexcept override;
  void setAppIdle(bool, TimePoint) noexcept override;
  void setAppLimited() override;

  CongestionControlType type() const noexcept override;

  bool inSlowStart() const noexcept;

  uint64_t getBytesInFlight() const noexcept;

  bool isAppLimited() const noexcept override;

 private:
  void onPacketLoss(const LossEvent&);
  void onAckEvent(const AckEvent&);
  void onPacketAcked(const CongestionController::AckEvent::AckPacket&);

 private:
  QuicConnectionStateBase& conn_;
  folly::Optional<TimePoint> endOfRecovery_;

  uint64_t credits_;
  double mul_factor_;
  uint64_t skip_;

  uint64_t total_sent_;
  uint64_t total_acked_;


  WindowedFilter<
      std::chrono::microseconds,
      MinFilter<std::chrono::microseconds>,
      uint64_t,
      uint64_t>
      minRTTFilter_; // To get min RTT over 10 seconds

  WindowedFilter<
      std::chrono::microseconds,
      MinFilter<std::chrono::microseconds>,
      uint64_t,
      uint64_t>
      standingRTTFilter_; // To get min RTT over srtt/2
};
} // namespace quic
