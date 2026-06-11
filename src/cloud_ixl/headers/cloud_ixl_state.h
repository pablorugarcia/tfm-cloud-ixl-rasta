#ifndef CLOUD_IXL_STATE_H
#define CLOUD_IXL_STATE_H

#include <cloud_ixl_types.h>

typedef struct{
    SectionState section[SIGNAL_COUNT];
    PointState point[POINT_COUNT];
    SignalAspect signal[SIGNAL_COUNT];
    RouteState route[ROUTE_COUNT];
} IXL_state;

void cloud_ixl_state_init(IXL_state *state);

#endif
