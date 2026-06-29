#include <cloud_ixl_input.h>
#include <stdio.h>

static char first_selection_char(const char *buffer){
    for(size_t i = 0; buffer[i] != '\0'; i++){
        if(buffer[i] != ' ' && buffer[i] != '\t' && buffer[i] != '\n'){
            return buffer[i];
        }
    }

    return '\0';
}

static int buffer_contains_char(const char *buffer, char lower, char upper){
    for(size_t i = 0; buffer[i] != '\0'; i++){
        if(buffer[i] == lower || buffer[i] == upper){
            return 1;
        }
    }

    return 0;
}

static RouteId read_route_id(void){
    char buffer[16];

    printf("Route [1/A/B=RUTA_AB, 2/C=RUTA_AC]: ");
    if(fgets(buffer, sizeof(buffer), stdin) == NULL){
        printf("No route input received. Defaulting to RUTA_AB.\n");
        return RUTA_AB;
    }

    switch(first_selection_char(buffer)){
        case '2':
        case 'c':
        case 'C':
            return RUTA_AC;

        case '1':
        case 'b':
        case 'B':
            return RUTA_AB;
        
        case 'a':
        case 'A':
        default:
            if(buffer_contains_char(buffer, 'c', 'C')){
                return RUTA_AC;
            }
            return RUTA_AB;
    }
}

static SignalAspect read_signal_aspect(void){
    char buffer[16];

    for(;;){
        printf(
            "Aspect [0=VIA_LIBRE, 1=PARADA, 2=ANUNCIO_PARADA, "
            "4=ANUNCIO_PRECAUCION, 5=REBASE, 6=PARADA_SELECTIVA_N2, "
            "7=REBASE_AUTORIZADO, 9=APAGADA]: "
        );

        if(fgets(buffer, sizeof(buffer), stdin) == NULL){
            printf("No aspect input received. Defaulting to PARADA.\n");
            return PARADA;
        }

        switch(first_selection_char(buffer)){
            case '0':
                return VIA_LIBRE;

            case '1':
                return PARADA;

            case '2':
                return ANUNCIO_PARADA;

            case '4':
                return ANUNCIO_PRECAUCION;

            case '5':
                return REBASE;

            case '6':
                return PARADA_SELECTIVA_N2;

            case '7':
                return REBASE_AUTORIZADO;

            case '9':
                return APAGADA;

            default:
                printf("Unsupported aspect for this lab command.\n");
                break;
        }
    }
}

RouteRequest receive_route_request(void){
    char buffer[16];
    RouteRequest r_request = {
        .command = ROUTE_COMMAND_REQUEST,
        .route_id = RUTA_AB,
        .aspect = PARADA,
    };

    printf("Command [r=request, l=release, a=aspect, q=quit]: ");
    if(fgets(buffer, sizeof(buffer), stdin) == NULL){
        r_request.command = ROUTE_COMMAND_QUIT;
        return r_request;
    }

    switch(first_selection_char(buffer)){
        case 'l':
        case 'L':
            r_request.command = ROUTE_COMMAND_RELEASE;
            break;

        case 'a':
        case 'A':
        case 's':
        case 'S':
            r_request.command = ROUTE_COMMAND_SIGNAL_ASPECT;
            r_request.aspect = read_signal_aspect();
            return r_request;

        case 'q':
        case 'Q':
            r_request.command = ROUTE_COMMAND_QUIT;
            return r_request;

        case 'r':
        case 'R':
        default:
            r_request.command = ROUTE_COMMAND_REQUEST;
            break;
    }

    r_request.route_id = read_route_id();

    return r_request;
}
