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
    GREEN,
    RED,
    YELLOW,
    GREEN_FLASHING,
    WHITE,
    BLUE,
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
