#include <AppComponents/Common/Filter/Routing/Comparators.h>
#include <AppComponents/Common/Filter/Routing/Helper.h>
#include <AppComponents/Common/Filter/Routing/PiecewiseRouter.h>
#include <AppComponents/Common/Filter/Routing/SkipRouter.h>

#include <Core/Common/Time/Helper.h>  // TODO: only for debugging

#include <amblog/global.h>

namespace AppComponents::Common::Filter::Routing {

PiecewiseRouter::PiecewiseRouter(SkipRouter const & router, Types::Routing::SamplingPointList const & samplingPointList, Types::Track::TimeList const & timeList)
  : router_(router), samplingPointList_(samplingPointList), timeList_(timeList)
{
}

bool PiecewiseRouter::operator()(Types::Routing::RouteList & routeList, Types::Routing::RoutingStatistic & routingStatistic)
{
    APP_LOG_LOCATION("PiecewiseRouter");

    // TODO: only for debugging
    auto getTime = [&](size_t const samplingPoint) { return Core::Common::Time::toString(timeList_[samplingPointList_[samplingPoint].trackIndex], "%H:%M:%S"); };

    size_t sourceSamplingPointIndex = 0;
    while (!samplingPointList_.empty() && sourceSamplingPointIndex < samplingPointList_.size() - 1)
    {
        router_(sourceSamplingPointIndex, samplingPointList_.size() - 1, routeList, routingStatistic);

        if (!routeList.empty() && routeList.back()->target.samplingPoint.index > sourceSamplingPointIndex)
        {
            APP_LOG(debug) << "routed " << getTime(sourceSamplingPointIndex) << " -> " << getTime(routeList.back()->target.samplingPoint.index);
            sourceSamplingPointIndex = routeList.back()->target.samplingPoint.index + 1;
        }
        else
            sourceSamplingPointIndex++;
    }

    APP_LOG(noise) << routeList.size() << " edges routed";

    return true;
}

}  // namespace AppComponents::Common::Filter::Routing