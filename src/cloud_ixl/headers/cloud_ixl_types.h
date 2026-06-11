typedef enum{
    FREE,
    OCCUPIED,
    UNKNOWN,
    FAILED
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
    YELLOW_FLASHING,
} SignalAspect;

typedef enum{
    FREE,
    RESERVED,
    OCCUPIED,
    REQUESTED,
} RouteState;

typedef enum{
    STOP,
    GO,
} RouteDecision;

