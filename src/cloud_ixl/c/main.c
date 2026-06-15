#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>


RouteDecision *request_route_decision(RouteId route_id){
    RouteDecision *decision;
    if(decision == NULL) {
        printf("El puntero de la deicsión es NULL. No es seguro usarlo.\n");
        return;
    }
    IXL_state state;
    cloud_ixl_state_init(&state);
    RouteDefinition *route = cloud_ixl_get_route_definition(route_id);
    for(size_t i = 0; i < MAX_SECTIONS_PER_ROUTE; i++){ 
        if(state.section[route->required_sections[i]] != 'FREE'){
            *decision = STOP;
            printf("Hay alguna sección ocupada. Instrucción: %p\n", *decision);
            return *decision;
        }
    }
    /*
    int free_count;
    free_count = 0;
    for(size_t i = 0; i < MAX_SECTIONS_PER_ROUTE; i++){ 
        if(state.section[route->required_sections[i]] == 'FREE'){
            free_count++;
        }
    }
    if (free_count == route->required_sections_count){
        *decision = GO; 
        printf("Todas las secciones están libre, procede. Instruccion: %p\n", *decision);
        return *decision;
    }
    if (free_count != route->required_sections_count){
        *decision = STOP;
        printf("Hay alguna sección ocupada. Instrucción: %p\n", *decision);
        return *decision;
    }
    */
 }