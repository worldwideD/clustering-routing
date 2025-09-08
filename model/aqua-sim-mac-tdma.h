/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2021 The City University of New York
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Dmitrii Dugaev <ddugaev@gradcenter.cuny.edu>
 */

#ifndef AQUA_SIM_MAC_TDMA_H
#define AQUA_SIM_MAC_TDMA_H

#include "aqua-sim-mac.h"
#include "aqua-sim-header.h"
#include "aqua-sim-header-mac.h"

namespace ns3 {

/**
 * \ingroup aqua-sim-ng
 *
 * \brief Classic TDMA MAC implementation
 */

class IntergeVector {
private:    
    friend std::ostream& operator<<(std::ostream& os, const IntergeVector& sl);
    friend std::istream& operator>>(std::istream& is, IntergeVector& sl);
public:
    // 默认构造函数
    IntergeVector() = default;

    // 构造函数，初始化vector
    explicit IntergeVector(size_t size) : data(size) {}

    // 列表初始化构造函数
    IntergeVector(std::initializer_list<int> init) : data(init) {}

    // 设置vector值的函数
    void setValues(const std::vector<int>& values) {
        data = values;
    }
    bool contains(int value) const {
        return std::find(data.begin(), data.end(), value) != data.end();
    }
    // 获取数组元素
    int& operator[](size_t index) {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }

    // 获取数组元素（常量版本）
    const int& operator[](size_t index) const {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }

    // 获取数组大小
    size_t getSize() const {
        return data.size();
    }

    // 打印数组内容
    void print() const {
        for (const int& value : data) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }
    std::vector<int> data;
};

ATTRIBUTE_HELPER_HEADER(IntergeVector);

std::ostream& operator<<(std::ostream& os, const IntergeVector& sl);
/**
 * Read IntergeVector from stream is.
 *
 * \param is The input stream.
 * \param sl The slot list to fill.
 * \return The stream.
 */
std::istream& operator>>(std::istream& is, IntergeVector& sl);

/**
 * \ingroup aqua-sim-ng
 *
 * \brief Classic TDMA MAC implementation
 */

class AquaSimTdmaMac : public AquaSimMac
{
public:

  typedef enum {
    IDLE, TX
  } TdmaState;

  AquaSimTdmaMac();
  int64_t AssignStreams (int64_t stream);

  static TypeId GetTypeId(void);

  // Handle a packet coming from channel
  virtual bool RecvProcess (Ptr<Packet>);
  // Handle a packet coming from upper layers
  virtual bool TxProcess (Ptr<Packet>);
  void SendPacketLOC ();
  void SendPacket (Ptr<Packet> packet);
  // TDMA methods
  void StartTdma();
  // schedule the transmission in the next available slot
  void ScheduleNextSlotTx(Ptr<Packet> packet);
  void LocPacketSend(void)override;
  static IntergeVector GetDefaultLists();
TracedCallback<Ptr<const Packet>,double,AquaSimAddress> tdma_mac_TxTrace;//数据包和当前时间
TracedCallback<Ptr<const Packet>,double,AquaSimAddress> tdma_mac_RxTrace;
TracedCallback<Ptr<const Packet>,double,AquaSimAddress> tdma_mac_LOSSTrace;

protected:
  virtual void DoDispose();

private:
  Ptr<UniformRandomVariable> m_rand;
  uint16_t m_packetSize;

  // TDMA-related variables
  TdmaState m_tdma_state;
  Time m_packet_start_ts = Seconds(0);
  std::list<Ptr<Packet>> m_send_queue;

  uint16_t m_tdma_slot_period;  // how many slots in 1 TDMA round
  Time m_tdma_slot_ms;
  Time m_tdma_dalay_ms;
  Time m_tdma_guard_interval_ms;
  Time m_tdma_total_ms;
  uint32_t m_tdma_slot_number;  // store the TDMA slot Tx number of this node
  uint32_t m_tdma_data_length;
  uint32_t m_tdma_data_length_now;
  uint32_t data_pac_length;
  uint32_t loc_pac_length;
  IntergeVector m_tdma_slot_list;
};  // class AquaSimTdmaMac

} // namespace ns3

#endif /* AQUA_SIM_MAC_TDMA_H */
