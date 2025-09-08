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
#include "dolphin-turning-helper.h"
#include "ns3/dolphin-mobility-helper.h"

#include "ns3/config.h"
#include "ns3/dolphin-mobility-model.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/names.h"
#include "ns3/pointer.h"
#include "ns3/position-allocator.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinTurningHelper");

DolphinTurningHelper::DolphinTurningHelper()
{
    speed = 10;
    m_helper.clear();
}

DolphinTurningHelper::~DolphinTurningHelper()
{
}

void
DolphinTurningHelper::Setspeed(double s)
{
    speed = s;
}

void
DolphinTurningHelper::Turning(Ptr<Node> node, Vector z)
{
    Ptr<DolphinMobilityModel> model = node->GetObject<DolphinMobilityModel>();
    Vector curPos = model->GetIdealPosition();
    Vector refPos = curPos - z;
    Vector newRefPos = Vector(-refPos.y, refPos.x, refPos.z);
    Vector route = newRefPos - refPos;
    double dis = route.GetLength(), t = dis / speed;
    if (t <= 1e-9)
        return;

    DolphinMobilityHelper *mobility = new DolphinMobilityHelper();
    mobility->SetMobilityModel("ns3::DolphinMobilityConstantVelocity");
    mobility->SetTime(0., t);
    mobility->SetSpeed(speed, Vector(route.x, route.y, route.z));
    mobility->Install(node, 0);
    m_helper.push_back(mobility);
}

void
DolphinTurningHelper::Turning(NodeContainer c, Vector z)
{
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Turning(*i, z);
    }
}

void
DolphinTurningHelper::Turning(NodeContainer c, Ptr<Node> node)
{
    Ptr<DolphinMobilityModel> model = node->GetObject<DolphinMobilityModel>();
    Vector z = model->GetIdealPosition();
    Turning(c, z);
}