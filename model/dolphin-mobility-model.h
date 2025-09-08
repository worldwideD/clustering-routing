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

#ifndef DOLPHIN_MOBILITY_MODEL_H
#define DOLPHIN_MOBILITY_MODEL_H


#include <cmath>
#include <vector>
#include <stdexcept>

#include "ns3/vector.h"
#include "ns3/mobility-model.h"
#include "ns3/timer.h"

// Dolphin Mobility Model

namespace ns3
{

/**
 * \ingroup dolphin
 * 
 * \brief Speed helper for mobility model
 */
class Velocity
{
  private:
    double m_speed;
    Vector c_direction, s_direction;
    void to_spherical();
    void to_cartesian();

  public:
    Velocity();
    Velocity(double speed, Vector direction);

    void SetSpeed_c(double speed, Vector direction);
    void SetSpeed_s(Vector direction);
    Vector GetDirection() const;
    Vector GetSpeed_c() const;
    Vector GetSpeed_s() const;
};

/**
 * \ingroup dolphin
 * 
 * \brief for mobility model with error
 */
class DolphinMobilityModel: public MobilityModel
{
  protected:
    mutable Vector idealPos, realPos, locPos;
    Vector2D posErr, locErr;
    bool paused;
    u_int32_t node_id;
    mutable double mileage;
    Vector GeneratePosition(Vector pos, Vector2D err) const;
  
  private:
    virtual Vector DoGetPosition() const;
    virtual void DoSetPosition(const Vector &position);
    virtual Vector DoGetVelocity() const;
    virtual Vector DoGetIdealPosition() const = 0;
    virtual Vector DoGetRealPosition() const = 0;
    virtual Vector DoGetLocatedPosition() const = 0;
    virtual double DoGetRealDistanceFrom(Vector other) const = 0;
    virtual double DoGetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const = 0;

  public:
    DolphinMobilityModel();
    ~DolphinMobilityModel();
    static TypeId GetTypeId();

    void SetNodeId(u_int32_t id);
    virtual Vector GetIdealPosition() const;
    virtual Vector GetRealPosition() const;
    virtual Vector GetLocatedPosition() const;
    virtual Vector GetIdealSpeed() const = 0;
    virtual Vector GetRealSpeed() const = 0;
    virtual Vector GetDirection() const = 0;
    virtual void InitPosition(Vector Pos, Vector2D pErr, Vector2D lErr);
    void SetPosition(Vector idealPos, Vector realPos, Vector locPos) const;
    virtual double GetRealDistanceFrom(Vector other) const;
    virtual double GetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const;

    virtual void SetSpeed(double speed, Vector direction, double SpeedErr, double DirErr) = 0;
    virtual void UpdatePosition() const = 0;
    virtual void UpdateSpeed() const = 0;
    virtual void UpdateDirection() const = 0;
    virtual void pause() = 0;
    virtual void unpause() = 0;
};

} // namespace ns3

#endif /* DOLPHIN_MOBILITY_MODEL_H */
