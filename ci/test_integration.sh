#!/bin/bash
set -ex

export PATH=$PATH:/ceph/build/bin
export PATH=$PATH:/ceph/src/progly

docker_exec () {
  time docker exec deploy /bin/bash -c "$1"
}

# build plugin and query runner
docker run -di --name test -v /home/travis/build/uccross/skyhook-ceph:/ceph cephbuilder/ceph:${RELEASE}
docker exec test /bin/bash -c "mkdir build && cd build && cmake -DWITH_RADOSGW=OFF .." >> /dev/null 2>&1
docker exec test /bin/bash -c "cd build && make -j2 cls_tabular ceph_test_skyhook_query"

# install
docker run -dit --name deploy -v /home/travis/build/uccross/skyhook-ceph:/ceph cephbuilder/ceph:skyhook-test
docker_exec "cp -a /ceph/build/lib/libcls_tabular.so* /usr/lib/rados-classes/"

# run integration tests
docker_exec "micro-osd.sh test && cp test/ceph.conf ."
docker_exec "/ceph/build/bin/ceph_test_skyhook_query"
echo "DONE!"

