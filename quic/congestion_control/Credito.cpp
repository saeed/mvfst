/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <quic/congestion_control/Credito.h>
#include <quic/congestion_control/CongestionControlFunctions.h>
#include <quic/logging/QLoggerConstants.h>

namespace quic {

constexpr int kRenoLossReductionFactorShift = 1;

Credito::Credito(QuicConnectionStateBase& conn)
    : conn_(conn),
      credits_(conn.transportSettings.initCwndInMss * conn.udpSendPacketLen) {
  credits_ = boundedCwnd(
      credits_,
      conn_.udpSendPacketLen,
      conn_.transportSettings.maxCwndInMss,
      conn_.transportSettings.minCwndInMss);
  mul_factor_ = 1.01;
  skip_ = 5;
}

void Credito::onRemoveBytesFromInflight(uint64_t bytes) {
  subtractAndCheckUnderflow(conn_.lossState.inflightBytes, bytes);
  return;
}

void Credito::onPacketSent(const OutstandingPacket& packet) {
//  LOG(INFO) << "CREDITS " << credits_;
  addAndCheckOverflow(conn_.lossState.inflightBytes, packet.encodedSize);

  subtractAndCheckUnderflow(credits_, kDefaultUDPSendPacketLen);

  credits_ = boundedCwnd(
      credits_,
      conn_.udpSendPacketLen,
      conn_.transportSettings.maxCwndInMss,
      conn_.transportSettings.minCwndInMss);
}

void Credito::onAckEvent(const AckEvent& ack) {
  subtractAndCheckUnderflow(conn_.lossState.inflightBytes, ack.ackedBytes);
  uint64_t __add = kDefaultUDPSendPacketLen * mul_factor_;
  for (const auto& packet : ack.ackedPackets) {
    if (mul_factor_ <= 1 && skip_) {
      skip_--;
    } else {
      addAndCheckOverflow(credits_, __add);
    }
  }

  credits_ = boundedCwnd(
      credits_,
      conn_.udpSendPacketLen,
      conn_.transportSettings.maxCwndInMss,
      conn_.transportSettings.minCwndInMss);
}

void Credito::onPacketAckOrLoss(
    folly::Optional<AckEvent> ackEvent,
    folly::Optional<LossEvent> lossEvent) {
  if (lossEvent) {
    subtractAndCheckUnderflow(conn_.lossState.inflightBytes, lossEvent->lostBytes);
//    if (mul_factor_ > 1)
//	mul_factor_ -= 0.005;
//    mul_factor_ = 1;
  }
  if (ackEvent && ackEvent->largestAckedPacket.has_value()) {
    onAckEvent(*ackEvent);
  }
}

uint64_t Credito::getWritableBytes() const noexcept {
  return credits_;
}

uint64_t Credito::getCongestionWindow() const noexcept {
  return credits_;
}

bool Credito::inSlowStart() const noexcept {
  return false;
}

CongestionControlType Credito::type() const noexcept {
  return CongestionControlType::Credito;
}

uint64_t Credito::getBytesInFlight() const noexcept {
  return conn_.lossState.inflightBytes;
}

void Credito::setAppIdle(bool, TimePoint) noexcept { /* unsupported */
}

void Credito::setAppLimited() { /* unsupported */
}

bool Credito::isAppLimited() const noexcept {
  return false; // unsupported
}

} // namespace quic
