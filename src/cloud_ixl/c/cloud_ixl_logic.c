#include <stdio.h>
#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_input.h>

RouteState get_route_state(const IXL_state *state, RouteId route_id){
    RouteState r_state;
    const RouteDefinition *route = cloud_ixl_get_route_definition(route_id);
    if(route == NULL) {
        printf("El puntero de la ruta es NULL. No es seguro usarlo.\n");
        return INVALID;
    }
    for(size_t i = 0; i < route->required_sections_count; i++){ 
        if(state->section[route->required_sections[i]] != S_FREE){
            r_state = OCCUPIED;
            printf("Hay alguna sección %d.\n", r_state);
            return r_state;
        }
    }
    for(size_t i = 0; i < route->required_points_count; i++){ 
        if(state->point[route->required_points[i]] != route->required_point_states[i]){
            r_state = WRONG_POINTS;
            printf("Las secciones están libres pero la aguja %d está en posición incompatible: %d \n", route->required_points[i], route->required_point_states[i]);
            return r_state;
        }
        r_state = FREE;
    }
    printf("Todas las secciones están %d y las agujas están correctas. \n", r_state);
    return r_state;
}

RouteDecision request_route_decision(const IXL_state *state, RouteId route_id){
    RouteState r_state = get_route_state(state, route_id);
    if (r_state == FREE){
        r_state = RESERVED; /* TODO: no funciona*/
        return GO;
    }
     
    return STOP;
}