FROM ubuntu:bionic

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install -y curl gnupg && \
    curl https://dist.apache.org/repos/dist/dev/arrow/KEYS | apt-key add - && \
    printf "deb [arch=amd64] https://dl.bintray.com/apache/arrow/ubuntu/ bionic main\ndeb-src https://dl.bintray.com/apache/arrow/ubuntu/ bionic main\n" | tee /etc/apt/sources.list.d/apache-arrow.list && \
    apt update && \
    apt install -y --no-install-suggests \
      ceph-mon \
      ceph-osd \
      apt-transport-https \
      gnupg \
      libarrow-dev \
      libparquet-dev && \
    rm -rf /var/lib/apt/lists/* /var/tmp/*

COPY build/bin/run-query build/bin/sky_tabular_flatflex_writer build/bin/ceph_test_skyhook_query /usr/bin/

# copy our cls library to correct path as below
COPY build/lib/libcls_tabular.so.1.0.0 /usr/lib/x86_64-linux-gnu/rados-classes/libcls_tabular.so
