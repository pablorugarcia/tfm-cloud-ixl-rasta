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

RouteRequest receive_route_request(void){
    char buffer[16];
    RouteRequest r_request = {
        .command = ROUTE_COMMAND_REQUEST,
        .route_id = RUTA_AB,
    };

    printf("Command [r=request, l=release, q=quit]: ");
    if(fgets(buffer, sizeof(buffer), stdin) == NULL){
        r_request.command = ROUTE_COMMAND_QUIT;
        return r_request;
    }

    switch(first_selection_char(buffer)){
        case 'l':
        case 'L':
            r_request.command = ROUTE_COMMAND_RELEASE;
            break;

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
