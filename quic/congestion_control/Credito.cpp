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
      ssthresh_(std::numeric_limits<uint32_t>::max()),
      cwndBytes_(conn.transportSettings.initCwndInMss * conn.udpSendPacketLen),
      credits_(conn.transportSettings.initCwndInMss * conn.udpSendPacketLen) {
  cwndBytes_ = boundedCwnd(
      cwndBytes_,
      conn_.udpSendPacketLen,
      conn_.transportSettings.maxCwndInMss,
      conn_.transportSettings.minCwndInMss);
  credits_ = boundedCwnd(
      cwndBytes_,
      conn_.udpSendPacketLen,
      conn_.transportSettings.maxCwndInMss,
      conn_.transportSettings.minCwndInMss);
}

void Credito::onRemoveBytesFromInflight(uint64_t bytes) {
  subtractAndCheckUnderflow(conn_.lossState.inflightBytes, bytes);
  return;
}

void Credito::onPacketSent(const OutstandingPacket& packet) {
  addAndCheckOverflow(conn_.lossState.inflightBytes, packet.encodedSize);

  subtractAndCheckUnderflow(credits_, kDefaultUDPSendPacketLen);
}

void Credito::onAckEvent(const AckEvent& ack) {
  subtractAndCheckUnderflow(conn_.lossState.inflightBytes, ack.ackedBytes);

  for (const auto& packet : ack.ackedPackets) {
    addAndCheckOverflow(credits_, packet.encodedSize);
  }
}

void Credito::onPacketAckOrLoss(
    folly::Optional<AckEvent> ackEvent,
    folly::Optional<LossEvent> lossEvent) {
  if (lossEvent) {
    subtractAndCheckUnderflow(conn_.lossState.inflightBytes, loss.lostBytes);
  }
  if (ackEvent && ackEvent->largestAckedPacket.has_value()) {
    onAckEvent(*ackEvent);
  }
}

uint64_t Credito::getWritableBytes() const noexcept {
  return credits_;
}

uint64_t Credito::getCongestionWindow() const noexcept {
  return 0;
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
