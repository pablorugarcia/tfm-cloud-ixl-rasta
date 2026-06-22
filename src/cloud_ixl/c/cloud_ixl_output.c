#include <cloud_ixl_types.h>
#include <cloud_ixl_logic.h>

SignalAspect ls_request_command (RouteDecision decision){
    switch (decision)
    {
    case GO:
        return GREEN ;
    
    case STOP:
        return RED;

    default:
        return RED;
    }
}