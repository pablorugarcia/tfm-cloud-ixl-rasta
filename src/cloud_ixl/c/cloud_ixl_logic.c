#include <stdio.h>
#include <stdbool.h>
#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_format.h>

static bool route_state_blocks(RouteState state){
    return state == RESERVED || state == REQUESTED || state == OCCUPIED;
}

static bool routes_share_section(const RouteDefinition *left, const RouteDefinition *right){
    for(size_t left_index = 0; left_index < left->required_sections_count; left_index++){
        for(size_t right_index = 0; right_index < right->required_sections_count; right_index++){
            if(left->required_sections[left_index] == right->required_sections[right_index]){
                return true;
            }
        }
    }

    return false;
}

static RouteState get_conflicting_route_state(
    const IXL_state *state,
    RouteId route_id,
    const RouteDefinition *route
){
    for(size_t other_index = 0; other_index < ROUTE_COUNT; other_index++){
        RouteId other_route_id = (RouteId)other_index;
        const RouteDefinition *other_route;

        if(other_route_id == route_id){
            continue;
        }

        if(!route_state_blocks(state->route[other_route_id])){
            continue;
        }

        other_route = cloud_ixl_get_route_definition(other_route_id);
        if(other_route != NULL && routes_share_section(route, other_route)){
            printf(
                "La ruta %d está %s y comparte secciones con la ruta %d.\n",
                (int)other_route_id,
                route_state_to_string(state->route[other_route_id]),
                (int)route_id
            );
            return RESERVED;
        }
    }

    return FREE;
}

RouteState get_route_state(const IXL_state *state, RouteId route_id){
    const RouteDefinition *route = cloud_ixl_get_route_definition(route_id);
    RouteState conflict_state;

    if(state == NULL) {
        printf("El puntero del estado del enclavamiento es NULL. No es seguro usarlo.\n");
        return INVALID;
    }

    if(route == NULL) {
        printf("El puntero de la ruta es NULL. No es seguro usarlo.\n");
        return INVALID;
    }

    if(route_state_blocks(state->route[route_id])){
        printf(
            "La ruta %d ya está %s.\n",
            (int)route_id,
            route_state_to_string(state->route[route_id])
        );
        return state->route[route_id];
    }

    for(size_t i = 0; i < route->required_sections_count; i++){ 
        if(state->section[route->required_sections[i]] != S_FREE){
            printf("La sección %s está %s.\n", section_name_to_string(route->required_sections[i]), section_state_to_string(state->section[route->required_sections[i]]));
            return OCCUPIED;
        }
    }

    conflict_state = get_conflicting_route_state(state, route_id, route);
    if(conflict_state != FREE){
        return conflict_state;
    }

    for(size_t i = 0; i < route->required_points_count; i++){ 
        if(state->point[route->required_points[i]] != route->required_point_states[i]){
            printf("Las secciones están libres pero la aguja %s está en posición incompatible: %s \n", point_name_to_string(route->required_points[i]), point_state_to_string(state->point[route->required_points[i]]));
            return WRONG_POINTS;
        }
    }

    printf("Todas las secciones están %s y las agujas están correctas. \n", route_state_to_string(FREE));
    return FREE;
}

RouteDecision request_route_decision(IXL_state *state, RouteId route_id){
    RouteState r_state = get_route_state(state, route_id);
    if (r_state == FREE){
        state->route[route_id] = RESERVED;
        printf("La ruta %d queda %s.\n", (int)route_id, route_state_to_string(state->route[route_id]));
        return GO;
    }
     
    return STOP;
}

bool release_route(IXL_state *state, RouteId route_id){
    const RouteDefinition *route = cloud_ixl_get_route_definition(route_id);

    if(state == NULL) {
        printf("El puntero del estado del enclavamiento es NULL. No es seguro usarlo.\n");
        return false;
    }

    if(route == NULL) {
        printf("El puntero de la ruta es NULL. No es seguro usarlo.\n");
        return false;
    }

    if(state->route[route_id] == FREE){
        printf("La ruta %d ya está %s.\n", (int)route_id, route_state_to_string(FREE));
        return false;
    }

    state->route[route_id] = FREE;
    printf("La ruta %d queda %s.\n", (int)route_id, route_state_to_string(state->route[route_id]));
    return true;
}
