#include <AppComponents/Common/GeometricGraphBuilder.h>

#include <Core/Common/Geometry/Helper.h>

#include <boost/function_output_iterator.hpp>

#include <optional>

namespace AppComponents::Common {

GeometricGraphBuilder::GeometricGraphBuilder(Core::Graph::Graph & graph, double maxDistanceInMeters) : graph_{graph}, maxDistanceInMeters_{maxDistanceInMeters}
{
}

void GeometricGraphBuilder::operator()(Core::Common::Geometry::LineString const & lineString)
{
    auto sourceNode = getOrCreateNode(lineString.front());
    auto targetNode = getOrCreateNode(lineString.back());
    graph_.addEdge(sourceNode, targetNode);
}

Core::Graph::Node GeometricGraphBuilder::getOrCreateNode(Core::Common::Geometry::Point point)
{
    unsigned const k_nearest = 1;
    auto result = std::optional<Value>{};
    auto inserter = [&](Value const & value)
    {
        if (Core::Common::Geometry::geoDistance(value.first, point) <= maxDistanceInMeters_)
        {
            result = value;
        }
    };
    geoIndex_.query(boost::geometry::index::nearest(point, k_nearest), boost::make_function_output_iterator(inserter));

    if (not result)
    {
        result = Value{point, graph_.createNode()};
    }
    return result->second;
}

}  // namespace AppComponents::Common
