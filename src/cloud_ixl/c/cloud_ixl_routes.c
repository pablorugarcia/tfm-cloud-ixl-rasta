#include <cloud_ixl_routes.h>
#include <cloud_ixl_types.h>

static const RouteDefinition route_definitions[] = {
    {
        .id = RUTA_AB,
        .required_sections = {
            CV_01,
            CV_02,
        },
        .required_sections_count = 2,
        .required_points = {
            P_01,
        },
        .required_point_states = {
            LEFT,
        },
        .required_points_count = 1,
    },
    {
        .id = RUTA_AC,
        .required_sections = {
            CV_01,
            CV_03,
        },
        .required_sections_count = 2,
        .required_points = {
            P_01,
        },
        .required_point_states = {
            RIGHT,
        },
        .required_points_count = 1,
    }
};

const RouteDefinition *cloud_ixl_get_route_definition(RouteId route_id){
    for(size_t i = 0; i < ROUTE_COUNT; i++){
        if(route_definitions[i].id == route_id){
            return &route_definitions[i];
        } 
    }
    return NULL;
}
