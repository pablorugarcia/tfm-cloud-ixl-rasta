FROM alpine:3.23.5
RUN apk update && apk add --no-cache cmake gcc make musl-dev linux-headers

# Copy the core sources required by the root CMake project.
COPY src/rasta /opt/rasta-c/src/rasta
COPY src/sci /opt/rasta-c/src/sci
COPY src/cloud_ixl /opt/rasta-c/src/cloud_ixl
COPY src/sci_ls_icd /opt/rasta-c/src/sci_ls_icd

# Copy only the optional examples and configurations used by this image.
COPY extra/examples /opt/rasta-c/extra/examples
COPY extra/config/examples /opt/rasta-c/extra/config/examples
COPY ./CMakeLists.txt /opt/rasta-c

# copy config files
COPY config /opt/rasta-c/config

# set build args as env variables during image build process
ARG SERVER_CH1
ARG SERVER_CH2
ARG CLIENT1_CH1
ARG CLIENT1_CH2
ARG CLIENT2_CH1
ARG CLIENT2_CH2

# replace IPs in remote example config files
RUN sed -i -e "s/10.0.0.100/$SERVER_CH1/g" /opt/rasta-c/extra/config/examples/rasta_server.cfg &&\
    sed -i -e "s/10.0.0.101/$SERVER_CH2/g" /opt/rasta-c/extra/config/examples/rasta_server.cfg &&\
    sed -i -e "s/10.0.0.200/$CLIENT1_CH1/g" /opt/rasta-c/extra/config/examples/rasta_client1.cfg &&\
    sed -i -e "s/10.0.0.201/$CLIENT1_CH2/g" /opt/rasta-c/extra/config/examples/rasta_client1.cfg &&\
    sed -i -e "s/10.0.0.250/$CLIENT2_CH1/g" /opt/rasta-c/extra/config/examples/rasta_client2.cfg &&\
    sed -i -e "s/10.0.0.251/$CLIENT2_CH2/g" /opt/rasta-c/extra/config/examples/rasta_client2.cfg

# set build args as env variables for the container
ENV SERVER_CH1=${SERVER_CH1} \
    SERVER_CH2=${SERVER_CH2}

# build librasta and examples
RUN cmake -S /opt/rasta-c -B /opt/rasta-c/build \
      -DBUILD_LOCAL_EXAMPLES=OFF \
      -DBUILD_REMOTE_EXAMPLES=ON \
      -DBUILD_SCIP_SUPPORT=ON \
      -DEXAMPLE_IP_OVERRIDE=ON && \
    cmake --build /opt/rasta-c/build
