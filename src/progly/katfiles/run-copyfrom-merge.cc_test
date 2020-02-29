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
  std::string pool;
  std::string obj_prefix;

  po::options_description gen_opts("General options");
  gen_opts.add_options()
    ("help,h", "show help message")
    ("pool", po::value<std::string>(&pool)->required(), "pool")
    //("obj-prefix", po::value<std::string>(&obj_prefix)->required(), "object prefix")
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

  // save a test object
  librados::bufferlist bl0;
  bl0.append( "ABC" );
  bufferlist::iterator p = bl0.begin();
  size_t num_bytes_written = p.get_remaining() ;
  std::cout << "num bytes written : " << num_bytes_written << std::endl ;
  ret = ioctx.write( "bl0", bl0, num_bytes_written, 0 );
  checkret(ret, 0);

  // copy contents of bl0 into bl1
  // this works bc bl1=ABC
  librados::ObjectWriteOperation op;
  op.copy_from2("bl0", ioctx, 0, librados::OP_FADVISE_COPYFROMAPPEND);
  //op.copy_from("bl0", ioctx, 0);
  ret = ioctx.operate("bl1", &op);
  checkret(ret, 0);

  // read bl_seq
  librados::bufferlist bl_seq ;
  std::string oid1 = "bl1" ;
  int num_bytes_read = ioctx.read( oid1.c_str(), bl_seq, (size_t)0, (uint64_t)0 ) ;
  std::cout << "num_bytes_read : " << num_bytes_read << std::endl ;
  std::cout << bl_seq.c_str() << std::endl ;
  std::cout << "blah" << std::endl;

  // save a test object
  librados::bufferlist bl2;
  bl2.append( "1234" );
  p = bl2.begin();
  num_bytes_written = p.get_remaining() ;
  std::cout << "num bytes written : " << num_bytes_written << std::endl ;
  ret = ioctx.write( "bl2", bl2, num_bytes_written, 0 );
  checkret(ret, 0);

  // copy contents of bl2 into bl1
  // this fails bc bl1 is wiped.
  // should be ABC123
  librados::ObjectWriteOperation op2;
  op2.copy_from2("bl2", ioctx, 0, librados::OP_FADVISE_COPYFROMAPPEND);
  //op2.copy_from("bl2", ioctx, 0) ;
  ret = ioctx.operate("bl1", &op2);
  checkret(ret, 0);

  // read bl_seq
  bl_seq.clear() ;
  num_bytes_read = ioctx.read( oid1.c_str(), bl_seq, (size_t)0, (uint64_t)0 ) ;
  std::cout << "num_bytes_read : " << num_bytes_read << std::endl ;
  std::cout << bl_seq.c_str() << std::endl ;
  std::cout << "blah" << std::endl;

  // call a cls method without using a registered func.
  //int an_int = 0;
  //std::cout << "before: " << std::to_string(an_int) << std::endl;
  //ret = Tables::stub_method(an_int);
  //std::cout << "after:  " << std::to_string(an_int) << std::endl;

  // call a cls method using a registered func.
  //librados::bufferlist bl2;
  //bl2.append( "DEF" );
  //p = bl2.begin();
  //num_bytes_written = p.get_remaining() ;
  //std::cout << "num bytes written : " << num_bytes_written << std::endl ;
  //ret = ioctx.write( "bl2", bl2, num_bytes_written, 0 );
  //checkret(ret, 0);

  //kat_op kop_in;
  //kat_op kop_out;
  //kop_in.input_int = 8;
  //ceph::bufferlist inbl, outbl;
  //::encode(kop_in, inbl);
  //ret = ioctx.exec("bl2", "tabular", "stub_op",
  //                 inbl, outbl);
  //checkret(ret, 0);
  //std::cout << "ret = " << ret << std::endl;
  // doesn't work if you're using cxx_replace???
  //::decode(kop_out, outbl);
  //std::cout << "kop_out.input_int = " << kop_out.input_int << std::endl;

  ioctx.close();
  return 0;
}


//int main(int argc, char **argv)
//{
//  std::string pool;
//  unsigned num_objs;
//  int wthreads;
//  int qdepth;
//
//  po::options_description gen_opts("General options");
//  gen_opts.add_options()
//    ("help,h", "show help message")
//    ("pool", po::value<std::string>(&pool)->required(), "pool")
//    ("num-objs", po::value<unsigned>(&num_objs)->required(), "num objects")
//    ("wthreads", po::value<int>(&wthreads)->default_value(1), "num threads")
//    ("qdepth", po::value<int>(&qdepth)->default_value(1), "queue depth")
// ;
//
//  po::options_description all_opts("Allowed options");
//  all_opts.add(gen_opts);
//
//  po::variables_map vm;
//  po::store(po::parse_command_line(argc, argv, all_opts), vm);
//
//  if (vm.count("help")) {
//    std::cout << all_opts << std::endl;
//    return 1;
//  }
//
//  po::notify(vm);
//
//  assert(num_objs > 0);
//  assert(wthreads > 0);
//  assert(qdepth > 0);
//
//  // connect to rados
//  librados::Rados cluster;
//  cluster.init(NULL);
//  cluster.conf_read_file(NULL);
//  int ret = cluster.connect();
//  checkret(ret, 0);
//
//  // open pool
//  librados::IoCtx ioctx;
//  ret = cluster.ioctx_create(pool.c_str(), ioctx);
//  checkret(ret, 0);
//
//  // save a test object
//  librados::bufferlist bl0;
//  bl0.append( "ABC" );
//  bufferlist::iterator p = bl0.begin();
//  size_t num_bytes_written = p.get_remaining() ;
//  std::cout << "num bytes written : " << num_bytes_written << std::endl ;
//  ret = ioctx.write( "bl0", bl0, num_bytes_written, 0 );
//  checkret(ret, 0);
//
//  // copy contents of bl0 into bl1
//  librados::ObjectWriteOperation op;
//  op.copy_from("bl0", ioctx, 0);
//  ret = ioctx.operate("bl1", &op);
//  checkret(ret, 0);
//
//  std::cout << "blah" << std::endl;
//
//  // call a cls method without using a registered func.
//  int an_int = 0;
//  std::cout << "before: " << std::to_string(an_int) << std::endl;
//  ret = Tables::stub_method(an_int);
//  std::cout << "after:  " << std::to_string(an_int) << std::endl;
//
//  // call a cls method using a registered func.
//  librados::bufferlist bl2;
//  bl2.append( "DEF" );
//  p = bl2.begin();
//  num_bytes_written = p.get_remaining() ;
//  std::cout << "num bytes written : " << num_bytes_written << std::endl ;
//  ret = ioctx.write( "bl2", bl2, num_bytes_written, 0 );
//  checkret(ret, 0);
//
//  kat_op kop_in;
//  kat_op kop_out;
//  kop_in.input_int = 8;
//  ceph::bufferlist inbl, outbl;
//  ::encode(kop_in, inbl);
//  ret = ioctx.exec("bl2", "tabular", "stub_op",
//                   inbl, outbl);
//  checkret(ret, 0);
//  std::cout << "ret = " << ret << std::endl;
//  // doesn't work if you're using cxx_replace???
//  //::decode(kop_out, outbl);
//  //std::cout << "kop_out.input_int = " << kop_out.input_int << std::endl;
//
//  ioctx.close();
//  return 0;
//}
