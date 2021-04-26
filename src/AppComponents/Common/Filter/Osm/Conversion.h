#pragma once

#include <AppComponents/Common/Types/Street/Highway.h>
#include <AppComponents/Common/Types/Street/TravelDirection.h>

#include <string>
#include <unordered_set>

namespace AppComponents::Common::Filter::Osm {

Types::Street::TravelDirection toTravelDirection(std::string const & oneway);

Types::Street::Highway toHighway(std::string const & highway);
std::string toOsmString(Types::Street::HighwayType highway);
std::string toHighwaySelectionSql(std::unordered_set<Types::Street::HighwayType> const & highways, std::string const & tableName);

}  // namespace AppComponents::Common::Filter::Osm
