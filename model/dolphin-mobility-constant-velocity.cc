/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Connecticut
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
 * Author: Robert Martin <robert.martin@engr.uconn.edu>
 */

#include "ns3/log.h"
#include "ns3/double.h"
#include "dolphin-mobility-constant-velocity.h"
#include "ns3/random-variable-stream.h"

#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinMobilityConstantVelocity");

NS_OBJECT_ENSURE_REGISTERED(DolphinMobilityConstantVelocity);

DolphinMobilityConstantVelocity::DolphinMobilityConstantVelocity()
{
    idealSpeed.SetSpeed_c(0, Vector(1, 0, 0));
    realSpeed.SetSpeed_c(0, Vector(1, 0, 0));
    paused = 1;
}

DolphinMobilityConstantVelocity::~DolphinMobilityConstantVelocity()
{
}

TypeId
DolphinMobilityConstantVelocity::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinMobilityConstantVelocity")
        .SetParent<DolphinMobilityModel>()
        .AddConstructor<DolphinMobilityConstantVelocity>()
        .AddAttribute(
            "IdealSpeed",
            "The ideal current speed of the mobility model",
            TypeId::ATTR_GET,
            VectorValue(Vector(0, 0, 0)),
            MakeVectorAccessor(&DolphinMobilityConstantVelocity::GetIdealSpeed),
            MakeVectorChecker())
        .AddAttribute(
            "RealSpeed",
            "The real current speed of the mobility model",
            TypeId::ATTR_GET,
            VectorValue(Vector(0, 0, 0)),
            MakeVectorAccessor(&DolphinMobilityConstantVelocity::GetRealSpeed),
            MakeVectorChecker());
        
    return tid;
}

// void
// DolphinMobilityConstantVelocity::SetIdealSpeed(Vector IdealSpeed)
// {
//     idealSpeed.SetSpeed_c(IdealSpeed);
// }

// void
// DolphinMobilityConstantVelocity::SetRealSpeed(Vector RealSpeed)
// {
//     realSpeed.SetSpeed_c(RealSpeed);
// }

Vector
DolphinMobilityConstantVelocity::GetIdealSpeed() const
{
    if (paused)
        return Vector(0., 0., 0.);
    return idealSpeed.GetSpeed_c();
}

Vector
DolphinMobilityConstantVelocity::GetRealSpeed() const
{
    if (paused)
        return Vector(0., 0., 0.);
    return realSpeed.GetSpeed_c();
}

Vector
DolphinMobilityConstantVelocity::GetDirection() const
{
    return realSpeed.GetDirection();
}

void 
DolphinMobilityConstantVelocity::InitPosition(Vector Pos, Vector2D pErr, Vector2D lErr)
{
    DolphinMobilityModel::InitPosition(Pos, pErr, lErr);
    lastT = Simulator::Now().ToDouble(Time::S);
}

void
DolphinMobilityConstantVelocity::UpdatePosition() const
{
    if (paused)
    {
        return;
    }
    double period = Simulator::Now().ToDouble(Time::S) - lastT;
    if (period == 0)
    {
        return;
    }
    Vector last_idealpos = idealPos,//, last_realpos = realPos, 
            idealsp = GetIdealSpeed()/*, realsp = GetRealSpeed()*/;
    Vector ipos = Vector(last_idealpos.x + period * idealsp.x, 
                         last_idealpos.y + period * idealsp.y,
                         last_idealpos.z + period * idealsp.z);
    mileage += period * idealsp.GetLength();
    Vector rPos = GeneratePosition(ipos, posErr),
           lPos = GeneratePosition(rPos, locErr);
    SetPosition(ipos, rPos, lPos);
    // SetPosition(Vector(
    //     last_idealpos.x + period * idealsp.x,
    //     last_idealpos.y + period * idealsp.y,
    //     last_idealpos.z + period * idealsp.z),
    //     Vector(last_realpos.x + period * realsp.x,
    //     last_realpos.y + period * realsp.y,
    //     last_realpos.z + period * realsp.z
    // ));
    lastT += period;
}

void
DolphinMobilityConstantVelocity::UpdateSpeed() const
{
    UpdatePosition();

    double speed = idealSpeed.GetSpeed_s().x;
    Vector rv = realSpeed.GetSpeed_s();
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();

    realSpeed.SetSpeed_s(Vector(speed * rand -> GetValue(1 - speedErr, 1 + speedErr), 
                                rv.y,
                                rv.z));
}

void
DolphinMobilityConstantVelocity::UpdateDirection() const
{
    UpdatePosition();

    Vector iv = idealSpeed.GetSpeed_s();
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    double pi = std::acos(-1), theta, phi, threshold = std::cos(dirErr * pi / 180);
    if (dirErr > 0.)
    {
        while (1)
        {
            // 随机生成新方向，  可以证明极角的差一定在误差角度范围内
            theta = iv.y + rand -> GetValue(-dirErr, dirErr) * pi / 180,
            phi = iv.z + rand -> GetValue(0, 2 * pi);

            // 判断夹角是否在误差范围内
            if (threshold <= std::cos(iv.y) * std::cos(theta) + std::sin(iv.y) * std::sin(theta) * std::cos(iv.z - phi))
                break;
        }
        if (theta < 0)
            theta += pi;
        else if (theta > pi)
            theta -= pi;
        
        if (phi < -pi)
            phi += 2*pi;
        else if (phi > pi)
            phi -= 2*pi;
    }else
    {
        theta = iv.y;
        phi = iv.z;
    }

    realSpeed.SetSpeed_s(Vector(1, theta, phi));
}

void
DolphinMobilityConstantVelocity::SetSpeed(double speed, Vector direction, double SpeedErr, double DirErr)
{
    UpdatePosition();
    speedErr = SpeedErr;
    dirErr = DirErr;
    idealSpeed.SetSpeed_c(speed, direction);
    UpdateDirection();
    // UpdatePosition();
}

Vector
DolphinMobilityConstantVelocity::DoGetIdealPosition() const
{
    UpdatePosition();
    return idealPos;
}

Vector
DolphinMobilityConstantVelocity::DoGetRealPosition() const
{
    UpdatePosition();
    return realPos;
}

Vector
DolphinMobilityConstantVelocity::DoGetLocatedPosition() const
{
    UpdatePosition();
    return locPos;
}

double
DolphinMobilityConstantVelocity::DoGetRealDistanceFrom(Vector other) const
{
    UpdatePosition();
    Vector m_pos = GetRealPosition();
    return CalculateDistance(other, m_pos);
}

double
DolphinMobilityConstantVelocity::DoGetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const
{
    UpdatePosition();
    Vector o_pos = other->GetRealPosition();
    return GetRealDistanceFrom(o_pos);
}

Vector
DolphinMobilityConstantVelocity::DoGetPosition()
{
    return GetIdealPosition();
}

Vector
DolphinMobilityConstantVelocity::DoGetVelocity() const
{
    return GetIdealSpeed();
}

void
DolphinMobilityConstantVelocity::pause()
{
    if (paused)
        return;
    UpdatePosition();
    NS_LOG_UNCOND("At Time " << lastT 
                << " Node " << node_id
                << " stop moving at position: (" << GetIdealPosition() 
                << "),  real position: (" << GetRealPosition() << ")");
    paused = 1;
}

void
DolphinMobilityConstantVelocity::unpause()
{
    if (!paused)
        return;
    paused = 0;
    lastT = Simulator::Now().ToDouble(Time::S);
    NS_LOG_UNCOND("At Time " << lastT 
                << " Node " << node_id
                << " start moving from position: (" << GetIdealPosition() 
                << "),  real position: (" << GetRealPosition() << ")");
}