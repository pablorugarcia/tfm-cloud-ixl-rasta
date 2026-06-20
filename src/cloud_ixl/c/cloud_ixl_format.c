#include <stdio.h>
#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_input.h>
#include <cloud_ixl_format.h>

const char *route_state_to_string(RouteState state)
{
    switch (state) {
        case FREE:
            return "FREE";

        case WRONG_POINTS:
            return "WRONG_POINTS";

        case RESERVED:
            return "RESERVED";

        case OCCUPIED:
            return "OCCUPIED";

        case REQUESTED:
            return "REQUESTED";

        case INVALID:
            return "INVALID";

        default:
            return "UNKNOWN_ROUTE_STATE";
    }
}

const char *point_state_to_string(PointState p_state)
{
    switch (p_state) {

        case LEFT:
            return "LEFT";

        case RIGHT:
            return "RIGHT";

        case UNINTENDED:
            return "UNINTENDED";

        case MOVING:
            return "MOVING";

        case NO_END:
            return "NO_END";

        default:
            return "UNKNOWN_POINT_STATE";
    }
}

const char *section_state_to_string(SectionState s_state)
{
    switch (s_state) {

        case S_FREE:
            return "S_FREE";

        case S_OCCUPIED:
            return "S_OCCUPIED";

        case S_UNKNOWN:
            return "S_UNKNOWN";

        case S_FAILED:
            return "S_FAILED";

        default:
            return "UNKNOWN_SECTION_STATE";
    }
}

const char *point_name_to_string(PointId p_id)
{
    switch (p_id) {
        case P_01:
            return "P_01";
        default:
            return "UNKNOWN_POINT_ID";
    }
}

const char *section_name_to_string(SectionId s_id)
{
    switch (s_id) {
        case CV_01:
            return "CV_01";
        case CV_02:
            return "CV_02";
        case CV_03:
            return "CV_03";
        default:
            return "UNKNOWN_SECTION_ID";
    }
}

const char *decision_name_to_string(RouteDecision r_decision)
{
    switch (r_decision) {
        case STOP:
            return "STOP";
        case GO:
            return "GO";
        default:
            return "UNKNOWN_DECISION";
    }
}
