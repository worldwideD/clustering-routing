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

#ifndef DOLPHIN_MOBILITY_CONSTANT_VELOCITY_H
#define DOLPHIN_MOBILITY_CONSTANT_VELOCITY_H


#include <cmath>
#include <vector>
#include <stdexcept>

#include "dolphin-mobility-model.h"

namespace ns3
{
/**
 * \ingroup dolphin
 * 
 * \brief for mobility model with error
 */
class DolphinMobilityConstantVelocity: public DolphinMobilityModel
{
  private:
    mutable Velocity idealSpeed, realSpeed;
    double speedErr, dirErr;
    mutable double lastT;

    Vector DoGetIdealPosition() const;
    Vector DoGetRealPosition() const;
    Vector DoGetLocatedPosition() const;
    double DoGetRealDistanceFrom(Vector other) const;
    double DoGetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const;

  protected:
    // void SetIdealSpeed(double speed, Vector direction);
    // void SetRealSpeed(Vector RealSpeed);
    Vector DoGetPosition();
    Vector DoGetVelocity() const;

  public:
    DolphinMobilityConstantVelocity();
    ~DolphinMobilityConstantVelocity();
    static TypeId GetTypeId();

    void InitPosition(Vector Pos, Vector2D pErr, Vector2D lErr);
    void SetSpeed(double speed, Vector direction, double SpeedErr, double DirErr);
    
    Vector GetIdealSpeed() const;
    Vector GetRealSpeed() const;
    Vector GetDirection() const;

    void UpdatePosition() const;
    void UpdateSpeed() const;
    void UpdateDirection() const;

    void pause();
    void unpause();
};

} // namespace ns3

#endif /* DOLPHIN_MOBILITY_CONSTANT_VELOCITY_H */
