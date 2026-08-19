cmake_minimum_required(VERSION 3.6.2)
project(rasta C)

set(CMAKE_C_STANDARD 11)
include(CTest)

SET(USE_OPENSSL false CACHE BOOL "Use OpenSSL MD4 implementation")

# Optional targets: none of these are required for the main Cloud IXL -> OC connection. They remain available for laboratory diagnostics and examples, but are excluded from the default TFM build.
option(BUILD_OC_SIMULATOR "Build the local Object Controller simulator" OFF)
option(BUILD_SCIP_SUPPORT "Build optional SCI-P support for point controllers" OFF)
option(BUILD_LOCAL_EXAMPLES "Build the optional localhost examples" OFF)
option(BUILD_REMOTE_EXAMPLES "Build the optional network examples" OFF)
option(EXAMPLE_IP_OVERRIDE "Use IPs from environment variables in RaSTA/SCI examples" OFF)

# Only used by the optional `cmake --install` packaging rule below. It does not participate in the Cloud IXL -> OC runtime path.
include(GNUInstallDirs)

include_directories(
        src/rasta/headers
        src/sci/headers)

set(EXECUTABLE_OUTPUT_PATH bin/exe/examples)
set(LIBRARY_OUTPUT_PATH bin/lib)

# Shared object for RaSTA and SCI protocols
add_library(rasta SHARED
        # RaSTA headers
        src/rasta/headers/config.h
        src/rasta/headers/dictionary.h
        src/rasta/headers/fifo.h
        src/rasta/headers/logging.h
        src/rasta/headers/rasta_new.h
        src/rasta/headers/rasta_red_multiplexer.h
        src/rasta/headers/rastacrc.h
        src/rasta/headers/rastadeferqueue.h
        src/rasta/headers/rastafactory.h
        src/rasta/headers/rastahandle.h
        src/rasta/headers/rastalist.h
        src/rasta/headers/rastamd4.h
        src/rasta/headers/rastamodule.h
        src/rasta/headers/rastaredundancy_new.h
        src/rasta/headers/rastautil.h
        src/rasta/headers/rmemory.h
        src/rasta/headers/udp.h
        src/rasta/headers/rastablake2.h
        src/rasta/headers/rastasiphash24.h
        src/rasta/headers/rastahashing.h
        # SCI headers
        src/sci/headers/hashmap.h
        src/sci/headers/sci.h
        src/sci/headers/sci_telegram_factory.h
        src/sci/headers/scils.h
        # RaSTA sources
        src/rasta/c/config.c
        src/rasta/c/dictionary.c
        src/rasta/c/fifo.c
        src/rasta/c/logging.c
        src/rasta/c/rasta_new.c
        src/rasta/c/rasta_red_multiplexer.c
        src/rasta/c/rastacrc.c
        src/rasta/c/rastadeferqueue.c
        src/rasta/c/rastafactory.c
        src/rasta/c/rastahandle.c
        src/rasta/c/rastalist.c
        src/rasta/c/rastamd4.c
        src/rasta/c/rastamodule.c
        src/rasta/c/rastaredundancy_new.c
        src/rasta/c/rastautil.c
        src/rasta/c/rmemory.c
        src/rasta/c/udp.c
        src/sci/c/hashmap.c
        src/rasta/c/rastablake2.c
        src/rasta/c/rastasiphash24.c
        src/rasta/c/rastahashing.c
        # SCI sources
        src/sci/c/sci.c
        src/sci/c/sci_telegram_factory.c
        src/sci/c/scils.c
        )

# OPTIONAL: SCI-P belongs to point controllers and is not part of the
# Cloud IXL -> Light Signal OC path. Keep it available for the point examples.
if(BUILD_SCIP_SUPPORT)
    target_sources(rasta PRIVATE
            extra/protocols/scip/c/scip.c
            extra/protocols/scip/c/scip_telegram_factory.c
    )

    target_include_directories(rasta PUBLIC
            extra/protocols/scip/headers
    )
endif()

# Link system libraries for librasta
target_link_libraries(rasta rt pthread)

# if USE_OPENSSL parameter is passed to cmake or if architecture is ARM -> use openssl md4 implementation
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" OR ${USE_OPENSSL})
    message("Using OpenSSL MD4 implementation (only standard IV)")

    # define flag to use openssl in rastamd4
    target_compile_definitions(rasta PUBLIC USE_OPENSSL)
    # link libcrypto
    target_link_libraries(rasta crypto)
else()
    message("Using rasta-c MD4 implementation")
endif()

# OPTIONAL: packaging/install rule for librasta. It is not executed by a normal configure or build and is not required to run cloud_ixl.
install(TARGETS rasta
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Enable optimization for RaSTA
target_compile_options(rasta PRIVATE -O3 -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(rastadeferqueue_regression_test
            tests/rasta/c/rastadeferqueue_regression_test.c)
    target_link_libraries(rastadeferqueue_regression_test PRIVATE rasta)
    target_compile_options(rastadeferqueue_regression_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME rastadeferqueue_regression
             COMMAND rastadeferqueue_regression_test)
endif()

# ----- Cloud IXL part for Masters' Thesis -----
add_library(sci_ls_icd STATIC
        src/sci_ls_icd/c/sci_ls_icd.c
)

target_include_directories(sci_ls_icd PUBLIC
        src/sci_ls_icd/headers
)

target_link_libraries(sci_ls_icd PUBLIC rasta)

target_compile_options(sci_ls_icd PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
)

add_executable(cloud_ixl
        src/cloud_ixl/c/main.c
        src/cloud_ixl/c/cloud_ixl_format.c
        src/cloud_ixl/c/cloud_ixl_input.c
        src/cloud_ixl/c/cloud_ixl_logic.c
        src/cloud_ixl/c/cloud_ixl_output.c
        src/cloud_ixl/c/cloud_ixl_routes.c
        src/cloud_ixl/c/cloud_ixl_scils_adapter.c
        src/cloud_ixl/c/cloud_ixl_state.c
)

target_include_directories(cloud_ixl PRIVATE
        src/cloud_ixl/headers
)

target_link_libraries(cloud_ixl rasta sci_ls_icd)

target_compile_options(cloud_ixl PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
)

configure_file(config/rasta_client1_lab.cfg rasta_client1_lab.cfg COPYONLY)

# OPTIONAL: useful for a self-contained local test, but not used when Cloud IXL
# connects by IP to Pedro's real or separately executed Object Controller.
if(BUILD_OC_SIMULATOR)
    add_executable(oc_simulator
            src/oc_simulator/c/main.c
    )

    target_link_libraries(oc_simulator rasta sci_ls_icd)

    target_compile_options(oc_simulator PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
    )
endif()
# ----- End of Master Thesis Part -----

# OPTIONAL: standalone localhost examples from extra/examples/. They do not
# participate in the Cloud IXL -> OC execution path. The legacy SCI-LS
# examples use the retired 7/9-byte aspect API and are not build targets.
if(BUILD_LOCAL_EXAMPLES)
	# SCI-P example: only available when optional point support is enabled.
	if(BUILD_SCIP_SUPPORT)
		add_executable(scip_example_local
			    extra/examples/localhost/c/scip.c)
		target_link_libraries(scip_example_local rasta)
	endif()


	# rasta_example_new on localhost
	add_executable(rasta_example_local
		    extra/examples/localhost/c/rasta.c)
	target_link_libraries(rasta_example_local rasta)
	
	# Copy RaSTA configs to build directory
	configure_file(config/rasta_server_local.cfg rasta_server_local.cfg COPYONLY)
	configure_file(config/rasta_client1_local.cfg rasta_client1_local.cfg COPYONLY)
	configure_file(extra/config/examples/rasta_client2_local.cfg rasta_client2_local.cfg COPYONLY)
endif(BUILD_LOCAL_EXAMPLES)

# OPTIONAL: standalone network examples from extra/examples/. They demonstrate
# RaSTA/SCI in isolation and are not the Cloud IXL executable. The legacy
# SCI-LS example is not a build target for the same reason noted above.
if(BUILD_REMOTE_EXAMPLES)
	# SCI-P example: only available when optional point support is enabled.
	if(BUILD_SCIP_SUPPORT)
		add_executable(scip_example
			    extra/examples/scip_remote/c/main.c)
		target_link_libraries(scip_example rasta)
	endif()


	# rasta_example_new
	add_executable(rasta_example
		    extra/examples/rasta_remote/c/main.c)
	target_link_libraries(rasta_example rasta)
	
	# Copy RaSTA configs to build directory
	configure_file(extra/config/examples/rasta_server.cfg rasta_server.cfg COPYONLY)
	configure_file(extra/config/examples/rasta_client1.cfg rasta_client1.cfg COPYONLY)
	configure_file(extra/config/examples/rasta_client2.cfg rasta_client2.cfg COPYONLY)

	if (EXAMPLE_IP_OVERRIDE)
		add_compile_definitions(EXAMPLE_IP_OVERRIDE)
	endif(EXAMPLE_IP_OVERRIDE)
endif(BUILD_REMOTE_EXAMPLES)
