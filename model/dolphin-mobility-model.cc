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
#include "dolphin-mobility-model.h"
#include "ns3/random-variable-stream.h"

#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinMobilityModel");

Velocity::Velocity(): c_direction(Vector(1, 0, 0)), m_speed(0.)
{
    to_spherical();
}

Velocity::Velocity(double speed, Vector direction)
{
    SetSpeed_c(speed, direction);
}

void
Velocity::SetSpeed_c(double speed, Vector direction)
{
    double length = direction.GetLength();
    if (length == 0)
    {
        NS_FATAL_ERROR("The mobility model should have a non-zero direction");
    }
    m_speed = speed;
    c_direction = Vector(direction.x / length,
                         direction.y / length,
                         direction.z / length);
    to_spherical();
}

void
Velocity::SetSpeed_s(Vector direction)
{
    m_speed = direction.x;
    direction.x = 1;
    s_direction = direction;
    to_cartesian();
}

Vector
Velocity::GetSpeed_c() const
{
    return Vector(m_speed * c_direction.x,
                  m_speed * c_direction.y,
                  m_speed * c_direction.z);
}

Vector
Velocity::GetSpeed_s() const
{
    return Vector(m_speed * s_direction.x,
                  s_direction.y,
                  s_direction.z);
}

Vector
Velocity::GetDirection() const
{
    return c_direction;
}

void
Velocity::to_spherical()
{
    double x = c_direction.x,
           y = c_direction.y,
           z = c_direction.z,
           r = std::sqrt(x*x + y*y + z*z),
           theta = (r > 0)? std::acos(z / r): 0,
           phi = std::atan2(y, x);
    if (r == 0)
    {
        NS_FATAL_ERROR("The mobility model should have a non-zero direction");
    }
    s_direction = Vector(r, theta, phi);
}

void
Velocity::to_cartesian()
{
    double r = s_direction.x,
           theta = s_direction.y,
           phi = s_direction.z,
           x = r * std::sin(theta) * std::cos(phi),
           y = r * std::sin(theta) * std::sin(phi),
           z = r * std::cos(theta);
    if (r == 0)
    {
        NS_FATAL_ERROR("The mobility model should have a non-zero direction");
    }
    c_direction = Vector(x, y, z);
}

NS_OBJECT_ENSURE_REGISTERED(DolphinMobilityModel);

DolphinMobilityModel::DolphinMobilityModel()
{
}

DolphinMobilityModel::~DolphinMobilityModel()
{
}

TypeId
DolphinMobilityModel::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::DolphinMobilityModel")
        .SetParent<MobilityModel>()
        .AddAttribute(
            "IdealPos", 
            "The ideal current position of the mobility model.",
            TypeId::ATTR_GET,
            VectorValue(Vector(0, 0, 0)),
            MakeVectorAccessor(&DolphinMobilityModel::GetIdealPosition),
            MakeVectorChecker())
        .AddAttribute(
            "RealPos", 
            "The real current position of the mobility model.",
            TypeId::ATTR_GET,
            VectorValue(Vector(0, 0, 0)),
            MakeVectorAccessor(&DolphinMobilityModel::GetRealPosition),
            MakeVectorChecker())
        .AddAttribute(
            "LocPos", 
            "The located current position of the mobility model.",
            TypeId::ATTR_GET,
            VectorValue(Vector(0, 0, 0)),
            MakeVectorAccessor(&DolphinMobilityModel::GetLocatedPosition),
            MakeVectorChecker());
    return tid;
}

void
DolphinMobilityModel::SetNodeId(u_int32_t id)
{
    node_id = id;
}

Vector
DolphinMobilityModel::GetIdealPosition() const
{
    return DoGetIdealPosition();
}

Vector
DolphinMobilityModel::GetRealPosition() const
{
    return DoGetRealPosition();
}

Vector
DolphinMobilityModel::GetLocatedPosition() const
{
    return DoGetLocatedPosition();
}

Vector
DolphinMobilityModel::GeneratePosition(Vector pos, Vector2D Err) const
{
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    double max_err = Err.x + Err.y * mileage, pi = std::acos(-1);
    double theta = rand -> GetValue(0, pi),
           phi = rand -> GetValue(0, 2 * pi),
           len = rand -> GetValue(0, max_err),
           x = len * std::sin(theta) * std::cos(phi),
           y = len * std::sin(theta) * std::sin(phi),
           z = len * std::cos(theta);
    return pos + Vector(x, y, z);
}

void
DolphinMobilityModel::InitPosition(Vector pos, Vector2D pErr, Vector2D lErr)
{
    idealPos = pos;
    posErr = pErr;
    locErr = lErr;
    realPos = GeneratePosition(pos, pErr);
    locPos = GeneratePosition(realPos, lErr);
}

void
DolphinMobilityModel::SetPosition(Vector IdealPos, Vector RealPos, Vector LocPos) const
{
    idealPos = IdealPos;
    realPos = RealPos;
    locPos = LocPos;
}

double
DolphinMobilityModel::GetRealDistanceFrom(Vector other) const
{
    return DoGetRealDistanceFrom(other);
}

double
DolphinMobilityModel::GetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const
{
    return DoGetRealDistanceFrom(other);
}

Vector
DolphinMobilityModel::DoGetPosition() const
{
    return idealPos;
}

void
DolphinMobilityModel::DoSetPosition(const Vector &position)
{
    SetPosition(position, position, position);
}

Vector
DolphinMobilityModel::DoGetVelocity() const
{
    return Vector(0., 0., 0.);
}