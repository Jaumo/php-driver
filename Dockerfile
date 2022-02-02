ARG PHP_VERSION=7.4.16
FROM php:$PHP_VERSION-cli-buster

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL C.UTF-8

ENV BASE_DEPS curl wget git lsof unzip apt-transport-https ca-certificates dirmngr gnupg software-properties-common sudo htop python-pip vim gdb
RUN apt-get update && apt-get install -y $BASE_DEPS

# Install OpenJDK8 and fix the man directory so it does not fail
RUN mkdir -p /usr/share/man/man1
RUN wget -qO - https://adoptopenjdk.jfrog.io/adoptopenjdk/api/gpg/key/public | apt-key add -
RUN add-apt-repository --yes https://adoptopenjdk.jfrog.io/adoptopenjdk/deb/
RUN apt-get update && apt-get install -y adoptopenjdk-8-hotspot

# Install Cassandra for integration tests incl. ccm for cluster management
RUN wget -q -O - https://www.apache.org/dist/cassandra/KEYS | apt-key add -
RUN sh -c 'echo "deb https://www.apache.org/dist/cassandra/debian 311x main" > /etc/apt/sources.list.d/cassandra.list'
RUN apt-get update && apt-get install -y cassandra
RUN pip install ccm

# Install composer for PHP dependency management
COPY --from=composer:2 /usr/bin/composer /usr/bin/composer
RUN composer self-update

# Install libcassandra
ARG CASSANDRA_CPP_VERSION=2.16.0
ENV DEP_CASSANDRA zlib1g libicu63 libuv1 libgmp10 uuid libssl1.1
ENV DEP_CASSANDRA_DEV zlib1g-dev libicu-dev g++ make cmake libuv1-dev libgmp-dev uuid-dev libssl-dev

RUN apt-get install -y $DEP_CASSANDRA $DEP_CASSANDRA_DEV
RUN cd /tmp && git clone https://github.com/datastax/cpp-driver cpp-driver && cd cpp-driver \
    && git checkout $CASSANDRA_CPP_VERSION \
    && mkdir build && cd build && cmake .. && make -j$(nproc) && make install && ldconfig




