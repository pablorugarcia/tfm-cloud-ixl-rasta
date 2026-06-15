#ifndef CLOUD_IXL_ROUTES_H
#define CLOUD_IXL_ROUTES_H

#include <cloud_ixl_types.h>
#include <stddef.h>
#define MAX_SECTIONS_PER_ROUTE 8
#define MAX_POINTS_PER_ROUTE 8

typedef struct {
    RouteId id;
    SectionId required_sections[MAX_SECTIONS_PER_ROUTE];
    size_t required_sections_count;
    PointId required_points[MAX_POINTS_PER_ROUTE];
    PointState required_point_states[MAX_POINTS_PER_ROUTE];
    size_t required_points_count;

} RouteDefinition;

const RouteDefinition* cloud_ixl_get_route_definition(RouteId route_id);

#endif
