#include <stdio.h>
#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_input.h>
#include <cloud_ixl_logic.h>
#include <cloud_ixl_format.h>
#include <cloud_ixl_output.h>

int main(void){
    IXL_state state;
    cloud_ixl_state_init(&state);
    RouteRequest r_request = receive_route_request();
    RouteDecision decision = request_route_decision(&state, r_request.route_id);
    SignalAspect aspect = ls_request_command(decision); 
    printf("Decision: %s\n", decision_name_to_string(decision));

    return 0;
}