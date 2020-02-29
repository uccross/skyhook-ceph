/*
* Copyright (C) 2018 The Regents of the University of California
* All Rights Reserved
*
* This library can redistribute it and/or modify under the terms
* of the GNU Lesser General Public License Version 2.1 as published
* by the Free Software Foundation.
*
*/

#include <fstream>
#include <boost/program_options.hpp>
#include "include/rados/librados.hpp"
#include "cls/tabular/cls_tabular_utils.h"
#include "cls/tabular/cls_tabular.h"

namespace po = boost::program_options;

int main(int argc, char **argv)
{
  bool debug ;
  std::string pool;
  int start_oid;
  int end_oid;
  int merge_id;

  po::options_description gen_opts("General options");
  gen_opts.add_options()
    ("help,h", "show help message")
    ("debug",      po::value<bool>(&debug)->required(), "debug")
    ("pool",      po::value<std::string>(&pool)->required(), "pool")
    ("start-oid", po::value<int>(&start_oid)->required(),    "number for starting oid")
    ("end-oid",   po::value<int>(&end_oid)->required(),      "number for ending oid")
    ("merge-id",  po::value<int>(&merge_id)->required(),     "number id for merge object")
 ;

  po::options_description all_opts("Allowed options");
  all_opts.add(gen_opts);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, all_opts), vm);

  if (vm.count("help")) {
    std::cout << all_opts << std::endl;
    return 1;
  }

  po::notify(vm);

  // connect to rados
  librados::Rados cluster;
  cluster.init(NULL);
  cluster.conf_read_file(NULL);
  int ret = cluster.connect();
  checkret(ret, 0);

  // open pool
  librados::IoCtx ioctx;
  ret = cluster.ioctx_create(pool.c_str(), ioctx);
  checkret(ret, 0);

  for( int j=start_oid; j < end_oid; j++ ) {
    std::string target_objname = "obj.mergetarget."+std::to_string(merge_id) ;
    std::string src_objname = "obj."+std::to_string(j) ;
    if ( debug ) {
      std::cout << src_objname << std::endl ;
      std::cout << target_objname << std::endl ;
    }
    librados::ObjectWriteOperation op;
    op.copy_from2(src_objname, ioctx, 0, librados::OP_FADVISE_COPYFROMAPPEND);
    //op.copy_from(src_objname, ioctx, 0);
    ret = ioctx.operate(target_objname, &op);
    checkret(ret, 0);
  }

  ioctx.close();
  return 0;
}
