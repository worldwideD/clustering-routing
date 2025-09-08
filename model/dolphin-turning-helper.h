/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#ifndef DOLPHIN_TURNING_HELPER_H
#define DOLPHIN_TURNING_HELPER_H

#include "ns3/attribute.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/position-allocator.h"
#include "ns3/dolphin-mobility-model.h"
#include "ns3/dolphin-mobility-helper.h"

#include <vector>

namespace ns3
{

/**
 * \brief Helper class used to make turning for nodes.
 *
 */
class DolphinTurningHelper
{
  public:
    DolphinTurningHelper();

    ~DolphinTurningHelper();

    void Setspeed(double s);
    void Turning(Ptr<Node> node, Vector z);
    void Turning(NodeContainer c, Vector z);
    void Turning(NodeContainer c, Ptr<Node> node);

  private:
    double speed;
    std::vector<DolphinMobilityHelper *> m_helper;
};

} // namespace ns3

#endif /* DOLPHIN_TURNING_HELPER_H */
