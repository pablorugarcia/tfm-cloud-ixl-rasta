#include <stdio.h>
#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>

void cloud_ixl_state_init(IXL_state *state){
    if(state == NULL) {
        printf("El puntero del estado del enclavamiento es NULL. No es seguro usarlo.\n");
        return;
    }
    /*TODO: mejorar el orden de esto*/
    for(size_t i = 0; i < SIGNAL_COUNT; i++){
        state->signal[i] = PARADA;
    }
    for(size_t i = 0; i < POINT_COUNT; i++){
        state->point[i] = LEFT;
    }
    for(size_t i = 0; i < ROUTE_COUNT; i++){
        state->route[i] = FREE;
    }
    for(size_t i = 0; i < SECTION_COUNT; i++){
        state->section[i] = S_FREE;
    }
}

