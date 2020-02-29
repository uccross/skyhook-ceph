/*
* Copyright (C) 2018 The Regents of the University of California
* All Rights Reserved
*
* This library can redistribute it and/or modify under the terms
* of the GNU Lesser General Public License Version 2.1 as published
* by the Free Software Foundation.
*
*/

#include <bits/stdc++.h> 
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

  // ------------------------------ //
  //   SETUP THE DATA
  // ------------------------------ //
  // save a test fb_meta object
  std::cout << "-------------------------------------------------" << std::endl ;
  std::cout << "  SAVE A TEST OBJECT bl0 with contents of lineitem.10MB.objs.75Krows.csv" << std::endl ;

  // copy the csv file
  std::string cmd = "cp ../src/progly/katfiles/lineitem.10MB.objs.75Krows.csv ." ;
  std::cout << cmd << std::endl ;
  int sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // copy the schema file
  cmd = "cp ../src/progly/katfiles/lineitem_schema.txt ." ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // write into an fbx object file
  cmd = "bin/fbwriter --file_name lineitem.10MB.objs.75Krows.csv --schema_file_name lineitem_schema.txt --num_objs 1 --flush_rows 75000 --read_rows 75000 --csv_delim \"|\" --use_hashing false --rid_start_value 1 --table_name lineitem_10MB --input_oid 0 --obj_type SFT_FLATBUF_FLEX_ROW" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // put into src_fbx
  cmd = "bin/rados -p paper_exps put src_fbx fbmeta.Skyhook.v2.SFT_FLATBUF_FLEX_ROW.lineitem_10MB.0.1-1" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // check if rados df makes sense
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  CHECK IF RADOS DF MAKES SENSE" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "echo \"giving stats time to update...\" ; sleep 10 ; bin/rados df ;" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // check pool ls
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  CHECK POOL LS" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "echo \"giving stats time to update...\"; sleep 10; bin/rados -p paper_exps ls" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // ------------------------------ //
  //   ^^^^^SETUP THE DATA^^^^^
  // ------------------------------ //

  // ------------------------------ //
  //   TEST TRANSFORM2 INDIRECTLY
  // ------------------------------ //

  // read data from src_fbx
  std::cout << "-------------------------------------------------" << std::endl ;
  std::cout << "  READ CONTENTS OF src_fbx into inbl bufferlist" << std::endl ;
  librados::bufferlist inbl ;
  std::string src_fbx_oid = "src_fbx" ;
  auto num_bytes_read = ioctx.read( src_fbx_oid.c_str(), inbl, (size_t)0, (uint64_t)0 ) ;
  std::cout << "inbl num_bytes_read : " << num_bytes_read << std::endl ;
  std::cout << "inbl.length()       : " << inbl.length() << std::endl ;

  // call the method indirectly
  std::cout << "-------------------------------------------------" << std::endl ;
  std::cout << "  CALL TRANSFORM2 METHOD INDIRECTLY AS A REGISTERED METHOD" << std::endl ;
  librados::bufferlist outbl ;
  ret = ioctx.exec( "src_fbx", "tabular", "transform_db_op2", inbl, outbl);
  checkret(ret, 0);

  // write the outbl to ceph
  auto num_bytes_written = outbl.length() ;
  std::cout << "outbl num bytes written : " << num_bytes_written << std::endl ;
  ret = ioctx.write( "obj.0", outbl, num_bytes_written, 0 );

  // run select * query
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  RUN SELECT ALL QUERY ON TARGET" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "./bin/run-query --num-objs 1 --pool paper_exps --wthreads 36 --qdepth 36 --select \"*\" --limit 2;" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // ------------------------------ //
  //   ^^^TEST TRANSFORM2 INDIRECTLY^^^
  // ------------------------------ //

  // remove obj.0
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  REMOVE OBJ.0" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "bin/rados -p paper_exps rm obj.0 ;" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // check if rados df makes sense
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  CHECK IF RADOS DF MAKES SENSE" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "echo \"giving stats time to update...\" ; sleep 10 ; bin/rados df ;" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // check pool ls
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  CHECK POOL LS" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "echo \"giving stats time to update...\"; sleep 10; bin/rados -p paper_exps ls" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // read data from src_fbx
  std::cout << "-------------------------------------------------" << std::endl ;
  std::cout << "  READ CONTENTS OF src_fbx into inbl1 bufferlist" << std::endl ;
  librados::bufferlist inbl1 ;
  num_bytes_read = ioctx.read( src_fbx_oid.c_str(), inbl1, (size_t)0, (uint64_t)0 ) ;
  std::cout << "inbl1 num_bytes_read : " << num_bytes_read << std::endl ;
  std::cout << "inbl1.length()       : " << inbl1.length() << std::endl ;

  // copy contents of obj.src_fbx into obj.0
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  COPY CONTENTS OF obj.src_fbx INTO obj.0 WITH TRANSFORM" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  librados::ObjectWriteOperation op;
  op.copy_from2("src_fbx", ioctx, 0, librados::OP_FADVISE_COPYFROMAPPEND);
  ret = ioctx.operate("obj.0", &op);
  checkret(ret, 0);

  // check if rados df makes sense
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  CHECK IF RADOS DF MAKES SENSE" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "echo \"giving stats time to update...\" ; sleep 10 ; bin/rados df ;" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // check pool ls
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  CHECK POOL LS" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "echo \"giving stats time to update...\"; sleep 10; bin/rados -p paper_exps ls" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  // run select * query
  std::cout << "\n-------------------------------------------------" << std::endl ;
  std::cout << "  RUN SELECT ALL QUERY ON TARGET" << std::endl ;
  std::cout << "-------------------------------------------------" << std::endl ;
  cmd = "./bin/run-query --num-objs 1 --pool paper_exps --wthreads 36 --qdepth 36 --select \"*\" --limit 2;" ;
  std::cout << cmd << std::endl ;
  sys_ret = system( cmd.c_str() ) ;
  std::cout << "sys_ret = " << sys_ret << std::endl ;

  ioctx.close();
  return 0;
} //main
