#include "ns3/log.h"
#include "ns3/double.h"
#include "dolphin-mobility-random-waypoint.h"
#include "ns3/random-variable-stream.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinMobilityRandomWayPoint");

NS_OBJECT_ENSURE_REGISTERED(DolphinMobilityRandomWayPoint);

DolphinMobilityRandomWayPoint::DolphinMobilityRandomWayPoint()
{
    Speed.SetSpeed_c(0, Vector(1, 0, 0));
    paused = 1;
}

DolphinMobilityRandomWayPoint::~DolphinMobilityRandomWayPoint()
{
}

TypeId
DolphinMobilityRandomWayPoint::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinMobilityRandomWayPoint")
        .SetParent<DolphinMobilityModel>()
        .AddConstructor<DolphinMobilityRandomWayPoint>()
        .AddAttribute("MaxX", "Set the max x-axis.",
            DoubleValue(100.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::maxX),
            MakeDoubleChecker<double>())
        .AddAttribute("MinX", "Set the min x-axis.",
            DoubleValue(0.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::minX),
            MakeDoubleChecker<double>())
        .AddAttribute("MaxY", "Set the max y-axis.",
            DoubleValue(100.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::maxY),
            MakeDoubleChecker<double>())
        .AddAttribute("MinY", "Set the min y-axis.",
            DoubleValue(0.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::minY),
            MakeDoubleChecker<double>())
        .AddAttribute("MaxZ", "Set the max z-axis.",
            DoubleValue(100.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::maxZ),
            MakeDoubleChecker<double>())
        .AddAttribute("MinZ", "Set the min z-axis.",
            DoubleValue(0.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::minZ),
            MakeDoubleChecker<double>())
        .AddAttribute("MaxSpeed", "Set the max speed.",
            DoubleValue(5.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::maxSpeed),
            MakeDoubleChecker<double>())
        .AddAttribute("MinSpeed", "Set the min speed.",
            DoubleValue(1.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::minSpeed),
            MakeDoubleChecker<double>())
        .AddAttribute("MaxPauseTime", "Set the max pause time.",
            DoubleValue(0.0),
            MakeDoubleAccessor(&DolphinMobilityRandomWayPoint::maxPause),
            MakeDoubleChecker<double>())
        ;
    return tid;
}

Vector
DolphinMobilityRandomWayPoint::GetIdealSpeed() const
{
    if (paused)
        return Vector(0., 0., 0.);
    return Speed.GetSpeed_c();
}

Vector
DolphinMobilityRandomWayPoint::GetRealSpeed() const
{
    if (paused)
        return Vector(0., 0., 0.);
    return Speed.GetSpeed_c();
}

Vector
DolphinMobilityRandomWayPoint::GetDirection() const
{
    return Speed.GetDirection();
}

void 
DolphinMobilityRandomWayPoint::InitPosition(Vector Pos, Vector2D pErr, Vector2D lErr)
{
    DolphinMobilityModel::InitPosition(Pos, pErr, lErr);
    lastT = Simulator::Now().ToDouble(Time::S);
}

void
DolphinMobilityRandomWayPoint::UpdatePosition() const
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
    lastT += period;
}

void
DolphinMobilityRandomWayPoint::SetSpeed(double speed, Vector direction, double SpeedErr, double DirErr)
{
    dirErr = DirErr;
    Speed.SetSpeed_c(speed, direction);
}

void
DolphinMobilityRandomWayPoint::UpdateSpeed() const
{
    
}

void
DolphinMobilityRandomWayPoint::UpdateDirection() const
{
    
}

Vector
DolphinMobilityRandomWayPoint::DoGetIdealPosition() const
{
    UpdatePosition();
    return idealPos;
}

Vector
DolphinMobilityRandomWayPoint::DoGetRealPosition() const
{
    UpdatePosition();
    return realPos;
}

Vector
DolphinMobilityRandomWayPoint::DoGetLocatedPosition() const
{
    UpdatePosition();
    return locPos;
}

double
DolphinMobilityRandomWayPoint::DoGetRealDistanceFrom(Vector other) const
{
    UpdatePosition();
    Vector m_pos = GetRealPosition();
    return CalculateDistance(other, m_pos);
}

double
DolphinMobilityRandomWayPoint::DoGetRealDistanceFrom(Ptr<DolphinMobilityModel> other) const
{
    UpdatePosition();
    Vector o_pos = other->GetRealPosition();
    return GetRealDistanceFrom(o_pos);
}

Vector
DolphinMobilityRandomWayPoint::DoGetPosition()
{
    return GetIdealPosition();
}

Vector
DolphinMobilityRandomWayPoint::DoGetVelocity() const
{
    return GetIdealSpeed();
}

void
DolphinMobilityRandomWayPoint::pause()
{
    if (paused)
        return;
    UpdatePosition();
    paused = 1;
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    Time pauseTime = Seconds(rand->GetValue(0, maxPause));
    NS_LOG_DEBUG("At Time " << lastT 
        << " Node " << node_id
        << " stop moving at position: (" << GetIdealPosition() 
        << "),  pause time is: " << pauseTime);
    m_event.Cancel();
    m_event = Simulator::Schedule(pauseTime, &DolphinMobilityRandomWayPoint::unpause, this);
}

void
DolphinMobilityRandomWayPoint::unpause()
{
    if (!paused)
        return;
    paused = 0;
    lastT = Simulator::Now().ToDouble(Time::S);
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    Vector m_current = idealPos;
    Vector destination = Vector(rand->GetValue(minX, maxX),
                                rand->GetValue(minY, maxY),
                                rand->GetValue(minZ, maxZ));
    double speed = rand->GetValue(minSpeed, maxSpeed);
    while (speed <= 0)
        speed = rand->GetValue(minSpeed, maxSpeed);
    double dx = (destination.x - m_current.x);
    double dy = (destination.y - m_current.y);
    double dz = (destination.z - m_current.z);
    SetSpeed(speed, Vector(dx, dy, dz), 0., dirErr);
    Time travelDelay = Seconds(CalculateDistance(destination, m_current) / speed);
    NS_LOG_UNCOND("At Time " << lastT 
        << " Node " << node_id
        << " start moving from position: (" << idealPos
        << "), the next waypoint is: (" << destination 
        << "),  speed: " << speed << "");
    m_event.Cancel();
    m_event =
        Simulator::Schedule(travelDelay, &DolphinMobilityRandomWayPoint::pause, this);
}