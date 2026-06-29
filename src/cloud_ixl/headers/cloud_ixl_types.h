#ifndef IXL_TYPES
#define IXL_TYPES

typedef enum{
    S_FREE,
    S_OCCUPIED,
    S_UNKNOWN,
    S_FAILED
} SectionState;

typedef enum{
    LS_01,
    SIGNAL_COUNT,
} SignalId;

typedef enum{
    P_01,
    POINT_COUNT,
} PointId;

typedef enum {
    CV_01,
    CV_02,
    CV_03,
    SECTION_COUNT,
} SectionId;

typedef enum{
    RUTA_AB,
    RUTA_AC,
    ROUTE_COUNT,
} RouteId;

typedef enum{
    LEFT,
    RIGHT,
    UNINTENDED,
    MOVING, /* Not defined in EULYNX Eu.Doc.10*/
    NO_END,
} PointState;

typedef enum{
    VIA_LIBRE = 0,
    PARADA = 1,
    ANUNCIO_PARADA = 2,
    /*
     * Disabled until Pedro's OC accepts basic aspect 0x04.
     * VIA_LIBRE_CONDICIONAL = 3,
     */
    ANUNCIO_PRECAUCION = 4,
    REBASE = 5,
    PARADA_SELECTIVA_N2 = 6,
    REBASE_AUTORIZADO = 7,
    /*
     * Disabled until Pedro's OC accepts national aspect 0x04.
     * PARADA_SELECTIVA_N1 = 8,
     */
    APAGADA = 9
} SignalAspect;

typedef enum{
    FREE,
    WRONG_POINTS,
    RESERVED,
    OCCUPIED,
    REQUESTED,
    INVALID,
} RouteState;

typedef enum{
    STOP,
    GO,
} RouteDecision;

typedef enum{
    ROUTE_COMMAND_REQUEST,
    ROUTE_COMMAND_RELEASE,
    ROUTE_COMMAND_QUIT,
} RouteCommand;

typedef struct {
    RouteCommand command;
    RouteId route_id;
} RouteRequest;

#endif
