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
#include "dolphin-mobility-helper.h"

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

NS_LOG_COMPONENT_DEFINE("DolphinMobilityHelper");

DolphinMobilityHelper::DolphinMobilityHelper()
{
    m_mobility.SetTypeId("ns3::DolphinMobilityConstantVelocity");
    length = startTime = 0.;
    speed = 0;
    direction = Vector(1., 0., 0.);
    speedErr = dirErr = 0.;
    posCycle = speedCycle = dirCycle = 0.;
}

DolphinMobilityHelper::~DolphinMobilityHelper()
{
}

std::string
DolphinMobilityHelper::GetMobilityModelType() const
{
    return m_mobility.GetTypeId().GetName();
}

void
DolphinMobilityHelper::SetTime(double startT, double len)
{
    startTime = startT;
    length = len;
}

void
DolphinMobilityHelper::SetSpeed(double s, Vector d)
{
    speed = s;
    direction = d;
}

void
DolphinMobilityHelper::SetSpeed(double s, Vector d, double sErr, double dErr)
{
    speed = s;
    direction = d;
    speedErr = sErr;
    dirErr = dErr;
}

void
DolphinMobilityHelper::SetCycle(double pCycle, double sCycle, double dCycle)
{
    posCycle = pCycle;
    speedCycle = sCycle;
    dirCycle = dCycle;
}

void
DolphinMobilityHelper::ScheduleCycle(Ptr<Node> node, Ptr<DolphinMobilityModel> model)
{
    double curT;
    if (posCycle > 0)
    {
        for (curT = posCycle; curT < length; curT += posCycle)
        {
            Simulator::Schedule(Seconds(curT), MakeCallback(&DolphinMobilityModel::UpdatePosition, model));
        }
    }
    if (speedCycle > 0)
    {
        for (curT = speedCycle; curT < length; curT += speedCycle)
        {
            Simulator::Schedule(Seconds(curT), MakeCallback(&DolphinMobilityModel::UpdateSpeed, model));
        }
    }
    if (dirCycle > 0)
    {
        for (curT = dirCycle; curT < length; curT += dirCycle)
        {
            Simulator::Schedule(Seconds(curT), MakeCallback(&DolphinMobilityModel::UpdateDirection, model));
        }
    }
    Simulator::Schedule(Seconds(length), MakeCallback(&DolphinMobilityModel::pause, model));
}

void
DolphinMobilityHelper::ScheduleChange(Ptr<Node> node, Ptr<DolphinMobilityModel> model)
{
    Ptr<Object> object = node;
    Ptr<DolphinMobilityModel> m_model = object->GetObject<DolphinMobilityModel>();
    if (!m_model)
    {
        m_model = model;
        object->AggregateObject(m_model);
    }
    m_model->SetSpeed(speed, direction, speedErr, dirErr);
    m_model->SetNodeId(node->GetId() + 1);
    m_model->unpause();
    ScheduleCycle(node, m_model);
}

void
DolphinMobilityHelper::Install(Ptr<Node> node, bool isFirst)
{
    Time startT = Seconds(startTime);
    Ptr<Object> object = node;
    Ptr<DolphinMobilityModel> model = object->GetObject<DolphinMobilityModel>(), newModel = m_mobility.Create()->GetObject<DolphinMobilityModel>();
    if (!newModel)
    {
        NS_FATAL_ERROR("The requested mobility model is not a mobility model: \""
                        << m_mobility.GetTypeId().GetName() << "\"");
    }
    if (isFirst) // first model
    {
        model = newModel;
        object->AggregateObject(newModel);
        model->SetSpeed(speed, direction, speedErr, dirErr);
        model->SetNodeId(node->GetId() + 1);
        Simulator::Schedule(startT, MakeCallback(&DolphinMobilityModel::unpause, model));
        Simulator::Schedule(startT, &DolphinMobilityHelper::ScheduleCycle, this, node, model);
    }
    else  // schedule model to change
    {
        Simulator::Schedule(startT, &DolphinMobilityHelper::ScheduleChange, this, node, newModel);
    }
}

void
DolphinMobilityHelper::Install(std::string nodeName, bool isFirst)
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    Install(node, isFirst);
}

void
DolphinMobilityHelper::Install(NodeContainer c, bool isFirst)
{
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Install(*i, isFirst);
    }
}

void
DolphinMobilityHelper::SetPosition(Vector position, Vector2D pErr, Vector2D lErr, Ptr<Node> node)
{
    Ptr<Object> object = node;
    Ptr<DolphinMobilityModel> model = object->GetObject<DolphinMobilityModel>();
    if (!model)
    {
        //NS_FATAL_ERROR("Install position for a node without mobility model: \""
        //                << object->G << "\"");
        NS_FATAL_ERROR("Install position for a node without mobility model");
    }
    model->InitPosition(position, pErr, lErr);
}

void
DolphinMobilityHelper::SetPosition(Vector position, Ptr<Node> node)
{
    SetPosition(position, Vector2D(0., 0.), Vector2D(0., 0.), node);
}

void
DolphinMobilityHelper::SetPosition(Vector position, Vector2D pErr, Vector2D lErr, std::string nodeName)
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    SetPosition(position, pErr, lErr, node);
}

void
DolphinMobilityHelper::SetPosition(Vector position, std::string nodeName)
{
    SetPosition(position, Vector2D(0., 0.), Vector2D(0., 0.), nodeName);
}

void
DolphinMobilityHelper::SetPosition(std::vector<Vector> positions, std::vector<Vector2D> perrors, std::vector<Vector2D> lerrors, NodeContainer c)
{
    u_int32_t p = 0, e = 0;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i, ++p, ++e)
    {
        if (p >= positions.size())
        {
            NS_FATAL_ERROR("Not enough positions to allocate");
        }
        if (e >= perrors.size() || e >= lerrors.size())
        {
            NS_FATAL_ERROR("Not enough positions errors to allocate");
        }
        SetPosition(positions[p], perrors[e], lerrors[e], *i);
    }
}

void
DolphinMobilityHelper::SetPosition(std::vector<Vector> positions, NodeContainer c)
{
    u_int32_t p = 0;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i, ++p)
    {
        if (p >= positions.size())
        {
            NS_FATAL_ERROR("Not enough positions to allocate");
        }
        SetPosition(positions[p], *i);
    }
}