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

#ifndef DOLPHIN_MOBILITY_HELPER_H
#define DOLPHIN_MOBILITY_HELPER_H

#include "ns3/attribute.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/position-allocator.h"
#include "ns3/dolphin-mobility-model.h"

#include <vector>

namespace ns3
{

/**
 * \brief Helper class used to assign positions and mobility models to nodes.
 *
 * MobilityHelper::Install is the most important method here.
 */
class DolphinMobilityHelper
{
  public:
    DolphinMobilityHelper();

    ~DolphinMobilityHelper();

    /**
     * \tparam Ts \deduced Argument types
     * \param type the type of mobility model to use.
     * \param [in] args Name and AttributeValue pairs to set.
     *
     * Calls to MobilityHelper::Install will create an instance of a matching
     * mobility model for each node.
     */
    template <typename... Ts>
    void SetMobilityModel(std::string type, Ts&&... args);

    /**
     * \return a string which contains the TypeId of the currently-selected
     *          mobility model.
     */
    std::string GetMobilityModelType() const;

    void SetTime(double startT, double len);
    void SetSpeed(double s, Vector d);
    void SetSpeed(double s, Vector d, double sErr, double dErr);
    void SetCycle(double pCycle, double sCycle, double dCycle);

    void Install(Ptr<Node> node, bool isFirst);
    void Install(std::string nodeName, bool isFirst);
    void Install(NodeContainer container, bool isFirst);

    void SetPosition(Vector position, Vector2D pErr, Vector2D lErr, Ptr<Node> node);
    void SetPosition(Vector position, Ptr<Node> node);
    void SetPosition(Vector position, Vector2D pErr, Vector2D lErr, std::string nodeName);
    void SetPosition(Vector position, std::string nodeName);
    void SetPosition(std::vector<Vector> positions, std::vector<Vector2D> perrors, std::vector<Vector2D> lerrors, NodeContainer nodes);
    void SetPosition(std::vector<Vector> positions, NodeContainer nodes);

  private:
    /**
     * Output course change events from mobility model to output stream
     * \param stream output stream
     * \param mobility mobility model
     */
    //static void CourseChanged(Ptr<OutputStreamWrapper> stream, Ptr<const MobilityModel> mobility);
    void ScheduleChange(Ptr<Node> node, Ptr<DolphinMobilityModel> model);
    void ScheduleCycle(Ptr<Node> node, Ptr<DolphinMobilityModel> model);
    ObjectFactory m_mobility;                        //!< Object factory to create mobility objects
    double startTime, length;
    double speed;
    Vector direction;
    double speedErr, dirErr;
    double posCycle, speedCycle, dirCycle;
};

template <typename... Ts>
void
DolphinMobilityHelper::SetMobilityModel(std::string type, Ts&&... args)
{
    m_mobility.SetTypeId(type);
    m_mobility.Set(std::forward<Ts>(args)...);
}

} // namespace ns3

#endif /* DOLPHIN_MOBILITY_HELPER_H */
