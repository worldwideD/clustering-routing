#ifndef DOLPHIN_MOBILITY_RANDOM_WAYPOINT_H
#define DOLPHIN_MOBILITY_RANDOM_WAYPOINT_H
 
 
#include <cmath>
#include <vector>
#include <stdexcept>

#include "dolphin-mobility-model.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
 
namespace ns3
{
/**
 * \ingroup dolphin
 * 
 * \brief for rwp mobility model with error
 */
class DolphinMobilityRandomWayPoint: public DolphinMobilityModel
{
  private:
    mutable Velocity Speed;
    double dirErr;
    mutable double lastT;
    Vector dest;                       // destination location
    double minX, maxX, minY, maxY, minZ, maxZ;  // location range
    double minSpeed, maxSpeed;         // speed range
    double maxPause;                   // max pause time
    EventId m_event;                   //!< event ID of next scheduled event

    Vector DoGetIdealPosition() const;
    Vector DoGetRealPosition() const;
    Vector DoGetLocatedPosition() const;
    double DoGetRealDistanceFrom(Vector other) const;
    double DoGetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const;

  protected:
    Vector DoGetPosition();
    Vector DoGetVelocity() const;

  public:
    DolphinMobilityRandomWayPoint();
    ~DolphinMobilityRandomWayPoint();
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

#endif /* DOLPHIN_MOBILITY_RANDOM_WAYPOINT_H */
 