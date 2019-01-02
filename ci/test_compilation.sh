#!/bin/bash
set -ex

docker run -di --name test -v /home/travis/build/uccross/skyhook-ceph:/ceph cephbuilder/ceph:${RELEASE}
docker exec test /bin/bash -c "mkdir build && cd build && cmake -DWITH_RADOSGW=OFF .."
docker exec test /bin/bash -c "cd build && make -j2 ceph-osd osdc librados cls_tabular run-query ceph_test_skyhook_query"
# to run vstart, we additionally need: 
#   make j2 ceph-authtool ceph-mon monmaptool crushtool
#   cd /ceph/build/src/pybind && make -j2
