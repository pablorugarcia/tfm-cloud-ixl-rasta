#include <cloud_ixl_types.h>
#include <cloud_ixl_state.h>

const char *route_state_to_string(RouteState state);
const char *point_state_to_string(PointState p_state);
const char *section_state_to_string(SectionState s_state);
const char *point_name_to_string(PointId p_id);
const char *section_name_to_string(SectionId s_id);
const char *decision_name_to_string(RouteDecision r_decision);