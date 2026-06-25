#ifndef CLOUD_IXL_LOGIC_H
#define CLOUD_IXL_LOGIC_H


#include <stdbool.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_state.h>

RouteState get_route_state(const IXL_state *state, RouteId route_id);

RouteDecision request_route_decision(IXL_state *state, RouteId route_id);

bool release_route(IXL_state *state, RouteId route_id);



#endif
