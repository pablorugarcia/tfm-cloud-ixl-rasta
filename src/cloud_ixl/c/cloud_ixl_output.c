#include <cloud_ixl_types.h>
#include <cloud_ixl_logic.h>

SignalAspect ls_request_command (RouteDecision decision){
    switch (decision)
    {
    case GO:
        return VIA_LIBRE ;
    
    case STOP:
        return PARADA;

    default:
        return PARADA;
    }
}