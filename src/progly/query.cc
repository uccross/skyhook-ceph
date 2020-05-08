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
#include "query.h"
#include "../cls/tabular/cls_tabular_utils.h"


static std::string string_ncopy(const char* buffer, std::size_t buffer_size) {
  const char* copyupto = std::find(buffer, buffer + buffer_size, 0);
  return std::string(buffer, copyupto);
}

static std::mutex print_lock;

bool quiet;
bool use_cls;
std::string query;
bool use_index;
bool old_projection;
uint32_t index_batch_size;
uint64_t extra_row_cost;

std::vector<timing> timings;

// query parameters to be encoded into query_op struct
double extended_price;
int order_key;
int line_number;
int ship_date_low;
int ship_date_high;
double discount_low;
double discount_high;
double quantity;
std::string comment_regex;

// query_op params for flatbufs
bool qop_fastpath;
bool qop_index_read;
bool qop_mem_constrain;
int qop_index_type;
int qop_index2_type;
int qop_index_plan_type;
int qop_index_batch_size;
int qop_result_format;   // SkyFormatType enum
std::string qop_db_schema_name;
std::string qop_table_name;
std::string qop_data_schema;
std::string qop_query_schema;
std::string qop_index_schema;
std::string qop_index2_schema;
std::string qop_query_preds;
std::string qop_index_preds;
std::string qop_index2_preds;

// build index op params for flatbufs
bool idx_op_idx_unique;
bool idx_op_ignore_stopwords;
int idx_op_batch_size;
int idx_op_idx_type;
std::string idx_op_idx_schema;
std::string idx_op_text_delims;

// transform op params
int trans_op_format_type;

// Example op params
int expl_func_counter;
int expl_func_id;

// HEP op params
std::string qop_dataset_name;
std::string qop_file_name;
std::string qop_tree_name;

// other exec flags
bool runstats;
std::string project_cols;

// for debugging, prints full record header and metadata
bool print_verbose;

// final output format
int skyhook_output_format;  // SkyFormatType enum

// to convert strings <=> skyhook data structs
Tables::schema_vec sky_tbl_schema;
Tables::schema_vec sky_qry_schema;
Tables::schema_vec sky_idx_schema;
Tables::schema_vec sky_idx2_schema;
Tables::predicate_vec sky_qry_preds;
Tables::predicate_vec sky_idx_preds;
Tables::predicate_vec sky_idx2_preds;

 // these are all intialized in run-query
std::atomic<unsigned> result_count;
std::atomic<unsigned> rows_returned;
std::atomic<unsigned> nrows_processed;  // TODO: remove

// used for print csv
std::atomic<bool> print_header;
std::atomic<long long int> row_counter;
long long int row_limit;

// rename work_lock
int outstanding_ios;
std::vector<std::string> target_objects;
std::list<AioState*> ready_ios;

std::mutex dispatch_lock;
std::condition_variable dispatch_cond;

std::mutex work_lock;
std::condition_variable work_cond;

bool stop;

static void print_row(const char *row)
{
  if (quiet)
    return;

  print_lock.lock();

  const size_t order_key_field_offset = 0;
  size_t line_number_field_offset;
  if (old_projection && use_cls)
    line_number_field_offset = 4;
  else
    line_number_field_offset = 12;
  const size_t quantity_field_offset = 16;
  const size_t extended_price_field_offset = 24;
  const size_t discount_field_offset = 32;
  const size_t shipdate_field_offset = 50;
  const size_t comment_field_offset = 97;
  const size_t comment_field_length = 44;

  const double extended_price = *((const double *)(row + extended_price_field_offset));
  const int order_key = *((const int *)(row + order_key_field_offset));
  const int line_number = *((const int *)(row + line_number_field_offset));
  const int ship_date = *((const int *)(row + shipdate_field_offset));
  const double discount = *((const double *)(row + discount_field_offset));
  const double quantity = *((const double *)(row + quantity_field_offset));
  const std::string comment = string_ncopy(row + comment_field_offset,
      comment_field_length);

  if (old_projection) {
    std::cout << order_key <<
      "|" << line_number <<
      std::endl;
  } else {
    std::cout << extended_price <<
      "|" << order_key <<
      "|" << line_number <<
      "|" << ship_date <<
      "|" << discount <<
      "|" << quantity <<
      "|" << comment <<
      std::endl;
  }

  print_lock.unlock();
}


static void print_data(const char *dataptr,
                       const size_t datasz,
                       const int ds_format=SFT_FLATBUF_FLEX_ROW)
{

    // NOTE: quiet and print_verbose are exec flags in run-query
    if (quiet)
        return;

    // NOTE: print_header is atomic, and declared in query.h
    // used here to prevent duplicate printing of csv header at runtime
    // row_counter used to limit num rows returned in result (csv output)
    // print_lock prevents multiple worker threads from concurrent write output

    print_lock.lock();
    switch (ds_format) {

        case SFT_FLATBUF_FLEX_ROW:
            if (skyhook_output_format == SkyFormatType::SFT_PG_BINARY) {
                row_counter += Tables::printFlatbufFlexRowAsPGBinary(
                    dataptr,
                    datasz,
                    print_header,
                    print_verbose,
                    row_limit - row_counter);
            }
            else {
                row_counter += Tables::printFlatbufFlexRowAsCsv(
                    dataptr,
                    datasz,
                    print_header,
                    print_verbose,
                    row_limit - row_counter);
            }
            break;

        case SFT_ARROW:
            if (skyhook_output_format == SkyFormatType::SFT_PG_BINARY) {
                row_counter += Tables::printArrowbufRowAsPGBinary(
                    dataptr,
                    datasz,
                    print_header,
                    print_verbose,
                    row_limit - row_counter);
            }

            else if (skyhook_output_format == SkyFormatType::SFT_PYARROW_BINARY) {
                row_counter += Tables::printArrowbufRowAsPyArrowBinary(
                    dataptr,
                    datasz,
                    print_header,
                    print_verbose,
                    row_limit - row_counter
                );
            }

            else {
                row_counter += Tables::printArrowbufRowAsCsv(
                    dataptr,
                    datasz,
                    print_header,
                    print_verbose,
                    row_limit - row_counter);
            }
            break;

        case SFT_PYARROW_BINARY:
            row_counter += Tables::printArrowbufRowAsPyArrowBinary(
                dataptr,
                datasz,
                print_header,
                print_verbose,
                row_limit - row_counter);
            break;

        case SFT_JSON:
            if (skyhook_output_format == SkyFormatType::SFT_PG_BINARY) {
                std::cerr << "Print SFT_JSON: "
                          << "SFT_PG_BINARY not implemented" << std::endl;
                assert (Tables::SkyOutputBinaryNotImplemented==0);
            }
            row_counter += Tables::printJSONAsCsv(
                dataptr,
                datasz,
                print_header,
                print_verbose,
                row_limit - row_counter);
            break;

        case SFT_FLATBUF_UNION_ROW:
            if (skyhook_output_format == SkyFormatType::SFT_PG_BINARY) {
                std::cerr << "Print SFT_FLATBUF_UNION_ROW: "
                          << "SFT_PG_BINARY not implemented" << std::endl;
                assert (Tables::SkyOutputBinaryNotImplemented==0);
            }
            row_counter += Tables::printFlatbufFBURowAsCsv(
                dataptr,
                datasz,
                print_header,
                print_verbose,
                row_limit - row_counter);
            break;

        case SFT_FLATBUF_UNION_COL:
            if (skyhook_output_format == SkyFormatType::SFT_PG_BINARY) {
                std::cerr << "Print SFT_FLATBUF_UNION_COL: "
                          << "SFT_PG_BINARY not implemented" << std::endl;
                assert (Tables::SkyOutputBinaryNotImplemented==0);
            }
            row_counter += Tables::printFlatbufFBUColAsCsv(
                dataptr,
                datasz,
                print_header,
                print_verbose,
                row_limit - row_counter);
            break;

        case SFT_EXAMPLE_FORMAT:
            row_counter += Tables::printExampleFormatAsCsv(
                dataptr,
                datasz,
                print_header,
                print_verbose,
                row_limit - row_counter);
            break;

        case SFT_FLATBUF_CSV_ROW:
        case SFT_PG_TUPLE:
        case SFT_CSV:
        default:
            assert (Tables::TablesErrCodes::SkyFormatTypeNotRecognized==0);
    }
    print_header = false;
    print_lock.unlock();
}

/* NOTE: This function will be used by python driver  */
static void print_data(bufferlist out) {
    print_lock.lock();
    lockobj_info info;

    try {
        bufferlist::iterator it = out.begin();
        ::decode(info, it);
    } catch (const buffer::error &err) {
	    std::cout <<"ERROR: print_data: decoding inbl_lockobj_op failed";
        return;
    }
    std::cout << "Busy:" << info.table_busy << std::endl;
    print_lock.unlock();
}

static void worker_test_par(librados::IoCtx *ioctx, int i, uint64_t iters,
    bool test_par_read)
{
  std::stringstream ss;
  ss << "obj." << i;
  const std::string oid = ss.str();

  int ret = ioctx->create(oid, false);
  checkret(ret, 0);

  while (true) {
    ceph::bufferlist inbl, outbl;
    ::encode(iters, inbl);
    ::encode(test_par_read, inbl);
    ret = ioctx->exec(oid, "tabular", "test_par", inbl, outbl);
    checkret(ret, 0);
  }
}

void worker_build_index(librados::IoCtx *ioctx)
{
  while (true) {
    work_lock.lock();
    if (target_objects.empty()) {
      work_lock.unlock();
      break;
    }
    std::string oid = target_objects.back();
    target_objects.pop_back();
    std::cout << "building index... " << oid << std::endl;
    work_lock.unlock();

    ceph::bufferlist inbl, outbl;
    ::encode(index_batch_size, inbl);
    int ret = ioctx->exec(oid, "tabular", "build_index", inbl, outbl);
    checkret(ret, 0);
  }
  ioctx->close();
}

void worker_exec_build_sky_index_op(librados::IoCtx *ioctx, idx_op op)
{
  while (true) {
    work_lock.lock();
    if (target_objects.empty()) {
      work_lock.unlock();
      break;
    }
    std::string oid = target_objects.back();
    target_objects.pop_back();
    std::cout << "building index..." << " cols:" << op.idx_schema_str
              << " oid: " << oid << std::endl;
    work_lock.unlock();

    ceph::bufferlist inbl, outbl;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "exec_build_sky_index_op",
                          inbl, outbl);
    checkret(ret, 0);
  }
  ioctx->close();
}

void worker_transform_db_op(librados::IoCtx *ioctx, transform_op op)
{
  while (true) {
    work_lock.lock();
    if (target_objects.empty()) {
      work_lock.unlock();
      break;
    }
    std::string oid = target_objects.back();
    target_objects.pop_back();
    work_lock.unlock();

    ceph::bufferlist inbl, outbl;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "transform_db_op",
                          inbl, outbl);
    checkret(ret, 0);
  }
  ioctx->close();
}


void worker_exec_runstats_op(librados::IoCtx *ioctx, stats_op op)
{
  while (true) {
    work_lock.lock();
    if (target_objects.empty()) {
      work_lock.unlock();
      break;
    }
    std::string oid = target_objects.back();
    target_objects.pop_back();
    std::cout << "computing stats...table: " << op.table_name << " oid: "
              << oid << std::endl;
    work_lock.unlock();

    ceph::bufferlist inbl, outbl;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "exec_runstats_op", inbl, outbl);
    checkret(ret, 0);
  }
  ioctx->close();
}

void worker_lock_obj_init_op(librados::IoCtx *ioctx, lockobj_info op)
{
    std::string oid = op.table_group;

    ceph::bufferlist inbl, outbl;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "lock_obj_init_op",
                          inbl, outbl);
    checkret(ret, 0);
    //print_data(&outbl);
    std::cout << "Initialized lock object." << std::endl;
    ioctx->close();
}

void worker_lock_obj_create_op(librados::IoCtx *ioctx, lockobj_info op)
{
    std::string oid = op.table_group;

    ceph::bufferlist inbl, outbl;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "lock_obj_create_op",
                          inbl, outbl);
    checkret(ret, 0);
    //print_data(&outbl);
    std::cout << "Lock object created." << std::endl;
    ioctx->close();
}
void worker_lock_obj_free_op(librados::IoCtx *ioctx, lockobj_info op)
{

    std::string oid = op.table_group;
    ceph::bufferlist inbl, outbl;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "lock_obj_free_op",
                          inbl, outbl);
    checkret(ret, 0);
    print_data(outbl);
    ioctx->close();
}

/* NOTE: This is for debugging */
void worker_lock_obj_get_op(librados::IoCtx *ioctx, lockobj_info op)
{

    // Call get_lock_obj_query_op function
    ceph::bufferlist inbl, outbl;
    std::string oid = op.table_group;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "lock_obj_get_op",
                          inbl, outbl);

    checkret(ret, 0);
    print_data(outbl);

    ioctx->close();
}

void worker_lock_obj_acquire_op(librados::IoCtx *ioctx, lockobj_info op)
{

    // Call get_lock_obj_query_op function
    ceph::bufferlist inbl, outbl;
    std::string oid = op.table_group;
    ::encode(op, inbl);
    int ret = ioctx->exec(oid, "tabular", "lock_obj_acquire_op",
                          inbl, outbl);

    checkret(ret, 0);
    print_data(outbl);

    std::cout << "Lock object acquired." << std::endl;
    ioctx->close();
}
// busy loop work to simulate high cpu cost ops
volatile uint64_t __tabular_x;
static void add_extra_row_cost(uint64_t cost)
{
  for (uint64_t i = 0; i < cost; i++) {
    __tabular_x += i;
  }
}

void worker()
{
  std::unique_lock<std::mutex> lock(work_lock);
  while (true) {
    // wait for work, or done
    if (ready_ios.empty()) {
      if (stop)
        break;
      work_cond.wait(lock);
      continue;
    }

    // prepare result
    AioState *s = ready_ios.front();
    ready_ios.pop_front();

    // process result without lock. we own it now.
    lock.unlock();

    dispatch_lock.lock();
    outstanding_ios--;
    dispatch_lock.unlock();
    dispatch_cond.notify_one();

    struct timing times = s->times;
    uint64_t nrows_server_processed = 0;
    uint64_t eval2_start = getns();

    if (query == "flatbuf") {

        using namespace Tables;

        // standard librados read will return the raw object data (unprocessed)
        // cls read (execute) will return processed data from each obj.
        // in both cases, the results are wrapped as a sequence of bufferlists
        // currently each bufferlist is a skyhook flatbuf data structure.

        bufferlist wrapped_bls;   // to store the seq of bls.

        // first extract the top-level statistics encoded during cls processing
        if (use_cls) {
            try {
                ceph::bufferlist::iterator it = s->bl.begin();
                ::decode(times.read_ns, it);
                ::decode(times.eval_ns, it);
                ::decode(nrows_server_processed, it);
                ::decode(wrapped_bls, it);  // contains a seq of encoded bls.
            } catch (ceph::buffer::error&) {
                int decode_runquery_cls = 0;
                assert(decode_runquery_cls);
            }
            nrows_processed += nrows_server_processed;
        } else {
            wrapped_bls = s->bl;  // contains a seq of encoded bls.
        }
        delete s;  // we're done processing all of the bls contained within

        // decode and process each bl (contains 1 flatbuf) in a loop.
        ceph::bufferlist::iterator it = wrapped_bls.begin();
        while (it.get_remaining() > 0) {
            ceph::bufferlist bl;
            try {
                ::decode(bl, it);  // unpack the next data struct
            } catch (ceph::buffer::error&) {
                int decode_runquery_noncls = 0;
                assert(decode_runquery_noncls);
            }

            /*
            * NOTE:
            *
            * to manually test new formats you can append your new serialized
            * formatted data as a char* into a bl, then set optional args to
            * false and specify the format type such as this:
            * sky_meta meta = getSkyMeta(bl, false, SFT_FLATBUF_FLEX_ROW);
            *
            * which creates a new fbmeta from your new type of bl data.
            * then you can check the fields:
            * std::cout << "meta.blob_format:" << meta.blob_format << endl;
            */

            // default usage here assumes the fbmeta is already in the bl
            sky_meta meta = getSkyMeta(&bl);

            // this code block is only used for accounting (rows processed)
            switch (meta.blob_format) {

                case SFT_FLATBUF_FLEX_ROW:
                case SFT_ARROW:
                case SFT_JSON: {
                    sky_root root = Tables::getSkyRoot(meta.blob_data,
                                                       meta.blob_size,
                                                       meta.blob_format);
                    nrows_processed += root.nrows;
                    rows_returned += root.nrows;
                    break;
                }

                case SFT_FLATBUF_UNION_ROW:
                case SFT_FLATBUF_UNION_COL: {

                    // extract ptr to meta data blob
                    const char* blob_dataptr = reinterpret_cast<const char*>(meta.blob_data);
                    size_t blob_sz = meta.blob_size;

                    // extract bl_seq bufferlist
                    ceph::bufferlist bl_seq;
                    bl_seq.append(blob_dataptr, blob_sz);

                    // just need to grab the first bufferlist in the bl_seq
                    ceph::bufferlist::iterator it_bl_seq = bl_seq.begin();

                    // decode decrements get_remaining by moving itr
                    ceph::bufferlist bl;
                    ::decode(bl, it_bl_seq);
                    const char* dataptr = bl.c_str();
                    size_t datasz = bl.length();

                    // get the number of rows returned for accounting purposes
                    sky_root root = getSkyRoot(dataptr, datasz, SFT_FLATBUF_UNION_ROW);
                    rows_returned += root.nrows;
                    break;
                }
                case SFT_FLATBUF_CSV_ROW:
                case SFT_PG_TUPLE:
                case SFT_CSV:
                default:
                    assert (Tables::TablesErrCodes::SkyFormatTypeNotRecognized==0);
            }

            // check if cols to be projected or preds remaining to be applied
            // TODO: add any global aggs here.
            bool more_processing = false;
            if (!use_cls) {
                // TODO: remove pushed-down preds from sky_qry_preds then we
                // can remove project flag and just check size of preds here.
                if ((project_cols != PROJECT_DEFAULT) || (sky_qry_preds.size() > 0)) {
                    more_processing = true;
                }
            }

            // nothing left to do here, so we just print results
            if (!more_processing) {

                switch (meta.blob_format) {
                    case SFT_JSON:
                    case SFT_FLATBUF_FLEX_ROW:
                    case SFT_ARROW: {

                        sky_root root = \
                            Tables::getSkyRoot(meta.blob_data,
                                               meta.blob_size,
                                               meta.blob_format);

                        result_count += root.nrows;

                        print_data(meta.blob_data,
                                   meta.blob_size,
                                   meta.blob_format);
                        break;
                    }
                    case SFT_FLATBUF_UNION_ROW:
                    case SFT_FLATBUF_UNION_COL: {

                        // extract ptr to meta data blob
                        const char* blob_dataptr = \
                                reinterpret_cast<const char*>(meta.blob_data);
                        size_t blob_sz = meta.blob_size;

                        // extract bl_seq bufferlist
                        ceph::bufferlist bl_seq;
                        bl_seq.append(blob_dataptr, blob_sz);

                        // bl_seq for ROW format will only contain one bl
                        ceph::bufferlist::iterator it_bl_seq = bl_seq.begin();

                        bool first_iteration = true;
                        while (it_bl_seq.get_remaining() > 0) {

                            // decode decrements get_remaining by moving itr
                            ceph::bufferlist bl;
                            ::decode(bl, it_bl_seq);
                            const char* dataptr = bl.c_str();
                            size_t datasz = bl.length();

                            // get the number of rows returned for accounting
                            if (first_iteration) {
                                sky_root root = getSkyRoot(dataptr,
                                                           datasz,
                                                           SFT_FLATBUF_UNION_ROW);
                                result_count += root.nrows;
                                first_iteration = false;
                            }

                            // do the print
                            if (meta.blob_format == SFT_FLATBUF_UNION_ROW) {
                                print_data(dataptr,
                                           datasz,
                                           SFT_FLATBUF_UNION_ROW);
                            }
                            else if (meta.blob_format == SFT_FLATBUF_UNION_COL) {
                                print_data(dataptr,
                                           datasz,
                                           SFT_FLATBUF_UNION_COL);
                            }
                            else {
                                assert(Tables::TablesErrCodes::SkyFormatTypeNotRecognized==0);
                            }

                        } // while
                        break;
                    }

                    case SFT_FLATBUF_CSV_ROW:
                    case SFT_PG_TUPLE:
                    case SFT_CSV:
                    default:
                        assert (Tables::TablesErrCodes::SkyFormatTypeNotRecognized==0);
                }
            }
            else {

                // more processing to do such as min, sort, any other remaining preds.
                std::string errmsg;

                switch (meta.blob_format) {

                    case SFT_FLATBUF_FLEX_ROW: {
                        flatbuffers::FlatBufferBuilder flatbldr(1024); // pre-alloc
                        int ret = processSkyFb(flatbldr,
                                               sky_tbl_schema,
                                               sky_qry_schema,
                                               sky_qry_preds,
                                               meta.blob_data,
                                               meta.blob_size,
                                               errmsg);
                        if (ret != 0) {
                            int more_processing_failure = true;
                            std::cerr << "ERROR: query.cc: processing flatbuf: "
                                      << errmsg << "\n Tables::ErrCodes=" << ret
                                      << endl;
                            assert(more_processing_failure);
                        }

                        // TODO: we should be using uint8_t here
                        const char* processed_data = \
                            reinterpret_cast<const char*>(flatbldr.GetBufferPointer());
                        sky_root root = getSkyRoot(processed_data, 0);
                        result_count += root.nrows;
                        print_data(processed_data, 0, SFT_FLATBUF_FLEX_ROW);
                        break;
                    }

                    case SFT_ARROW: {
                        std::shared_ptr<arrow::Table> table;
                        int ret = processArrowCol(
                                      &table,
                                      sky_tbl_schema,
                                      sky_qry_schema,
                                      sky_qry_preds,
                                      meta.blob_data,
                                      meta.blob_size,
                                      errmsg);
                        if (ret != 0) {
                            int more_processing_failure = true;
                            std::cerr << "ERROR: query.cc: processing arrow: "
                                      << errmsg << "\n Tables::ErrCodes=" << ret
                                      << endl;
                            assert(more_processing_failure);
                        }
                        else {
                            std::shared_ptr<arrow::Buffer> buffer;
                            auto schema = table->schema();
                            auto metadata = schema->metadata();
                            result_count += std::stoi(metadata->value(METADATA_NUM_ROWS));
                            convert_arrow_to_buffer(table, &buffer);
                            print_data(buffer->ToString().c_str(), buffer->size(), SFT_ARROW);
                        }
                        break;
                    }

                    case SFT_JSON:  // TODO: call processJSON() here.
                        break;

                    case SFT_FLATBUF_UNION_ROW:
                    case SFT_FLATBUF_UNION_COL: {
                        flatbuffers::FlatBufferBuilder flatbldr(1024); // pre-alloc
                        int ret;

                        // extract ptr to meta data blob
                        const char* blob_dataptr = reinterpret_cast<const char*>(meta.blob_data);
                        size_t blob_sz = meta.blob_size;

                        // extract bl_seq bufferlist
                        ceph::bufferlist bl_seq;
                        bl_seq.append(blob_dataptr, blob_sz);

                        // bl_seq for ROW format will only contain one bl
                        ceph::bufferlist::iterator it_bl_seq = bl_seq.begin();

                        ceph::bufferlist bl;
                        ::decode(bl, it_bl_seq); // this decrements get_remaining by moving iterator
                        const char* dataptr = bl.c_str();
                        size_t datasz = bl.length();

                        if (meta.blob_format == SFT_FLATBUF_UNION_ROW) {
                            ret = processSkyFb_fbu_rows(
                                   flatbldr,
                                   sky_tbl_schema,
                                   sky_qry_schema,
                                   sky_qry_preds,
                                   dataptr,
                                   datasz,
                                   errmsg);
                        }
                        else if (meta.blob_format == SFT_FLATBUF_UNION_COL) {
                            ret = processSkyFb_fbu_cols(
                                    bl_seq,
                                    flatbldr,
                                    sky_tbl_schema,
                                    sky_qry_schema,
                                    sky_qry_preds,
                                    dataptr,
                                    datasz,
                                    errmsg);
                        }
                        else {
                            assert (Tables::TablesErrCodes::SkyFormatTypeNotRecognized==0);
                        }

                        if (ret != 0) {
                            int more_processing_failure = true;
                            std::cerr << "ERROR: query.cc: processing flatbuf: "
                                      << errmsg << "\n Tables::ErrCodes=" << ret
                                      << endl;
                            assert(more_processing_failure);
                        }

                        // TODO: we should be using uint8_t here
                        const char* processed_data = \
                            reinterpret_cast<const char*>(flatbldr.GetBufferPointer());
                        sky_root root = getSkyRoot(processed_data, 0);
                        result_count += root.nrows;
                        print_data(processed_data, 0, SFT_FLATBUF_FLEX_ROW);
                        break;
                    }
                    case SFT_FLATBUF_CSV_ROW:
                    case SFT_PG_TUPLE:
                    case SFT_CSV:
                    default:
                        assert (Tables::TablesErrCodes::SkyFormatTypeNotRecognized==0);
                }
            }
        } // endloop of processing sequence of encoded bls

    }
    else if (query == "example") {

        using namespace Tables;

        // to store the result from an object
        bufferlist bl;

        if (use_cls) {

            // result came from cls read, so we unpack our outbl info
            // added by the example cls method
            outbl_sample_info info;

            // decode the outbl from cls_tabular.cc example method to extract
            // results and metadata.
            // this worker has an AioState *s struct, declared above.
            try {
                ceph::bufferlist::iterator it = s->bl.begin();
                ::decode(info, it);
                ::decode(bl, it);
            } catch (ceph::buffer::error&) {
                int decode_examplequery_cls = 0;
                assert(decode_examplequery_cls);
            }
            times.read_ns = info.read_time_ns;
            times.eval_ns = info.eval_time_ns;
            rows_returned += info.rows_processed;
            result_count += info.rows_processed;
            cout << "count thus far... " <<rows_returned << std::endl;
        } else {

            // result came from standard read, no extra info to unpack
            // the outbl is just the actual data, and is stored in s.
            bl = s->bl;
        }

        print_data(bl.c_str(), bl.length(), SFT_EXAMPLE_FORMAT);

    }
    else if (query == "wasm") {

        using namespace Tables;

        // to store the result from an object
        bufferlist bl;

        if (use_cls) {

            // result came from cls read, so we unpack our outbl info
            // added by the example cls method
            wasm_outbl_sample_info info;

            // decode the outbl from cls_tabular.cc example method to extract
            // results and metadata.
            // this worker has an AioState *s struct, declared above.
            try {
                ceph::bufferlist::iterator it = s->bl.begin();
                ::decode(info, it);
                ::decode(bl, it);
            } catch (ceph::buffer::error&) {
                int decode_examplequery_cls = 0;
                assert(decode_examplequery_cls);
            }
            times.read_ns = info.read_time_ns;
            times.eval_ns = info.eval_time_ns;
            rows_returned += info.rows_processed;
            result_count += info.rows_processed;
            cout << "count thus far... " <<rows_returned << std::endl;
        } else {

            // result came from standard read, no extra info to unpack
            // the outbl is just the actual data, and is stored in s.
            bl = s->bl;
        }

        print_data(bl.c_str(), bl.length(), SFT_EXAMPLE_FORMAT);

    }
    else if (query == "hep") {

        using namespace Tables;

        // to store the result from an object.
        // the out bufferlist contains a cls_return_code and then
        // the actual result (if any) from the obj.
        bufferlist bl;
        int cls_result_code = 0;

        // check if object did not exist/had empty data partition.
        if (s->bl.length() > 0) {
            try {
                ceph::bufferlist::iterator it = s->bl.begin();
                ::decode(cls_result_code, it);
                ::decode(bl, it);
            } catch (ceph::buffer::error&) {
                int decode_hepquery_cls = 0;
                assert(decode_hepquery_cls);
            }

            // we do nothing for ClsResultCodeFalse, indicates no matching data
            // was returned from this object from the query, otherwise we output
            // result as as pyarrow binary
            if (cls_result_code == TablesErrCodes::ClsResultCodeTrue)
                print_data(bl.c_str(), bl.length(), SFT_PYARROW_BINARY);
        }
    }
    else {   // older processing code below

        // NOTE: these only used for older fixed size rows test dataset
        static const size_t order_key_field_offset = 0;
        static const size_t line_number_field_offset = 12;
        static const size_t quantity_field_offset = 16;
        static const size_t extended_price_field_offset = 24;
        static const size_t discount_field_offset = 32;
        static const size_t shipdate_field_offset = 50;
        static const size_t comment_field_offset = 97;
        static const size_t comment_field_length = 44;
        ceph::bufferlist bl;

        // if it was a cls read, first unpack some of the cls processing info
        if (use_cls) {
            try {
                ceph::bufferlist::iterator it = s->bl.begin();
                ::decode(times.read_ns, it);
                ::decode(times.eval_ns, it);
                ::decode(nrows_server_processed, it);
                ::decode(bl, it);
            } catch (ceph::buffer::error&) {
                int decode_runquery_cls = 0;
                assert(decode_runquery_cls);
            }
        } else {
            bl = s->bl;
        }

        // data is now all in bl
        delete s;

        // our older query processing code below...
        // apply the query
        size_t row_size;
        if (old_projection && use_cls)
          row_size = 8;
        else
          row_size = 141;
        const char *rows = bl.c_str();
        const size_t num_rows = bl.length() / row_size;
        rows_returned += num_rows;

        if (use_cls)
            nrows_processed += nrows_server_processed;
        else
            nrows_processed += num_rows;

        if (query == "a") {
          if (use_cls) {
            // if we are using cls then storage system returns the number of
            // matching rows rather than the actual rows. so we patch up the
            // results to the presentation of the results is correct.
            size_t matching_rows;
            ceph::bufferlist::iterator it = bl.begin();
            ::decode(matching_rows, it);
            result_count += matching_rows;
          } else {
            if (old_projection && use_cls) {
              result_count += num_rows;
            } else {
              for (size_t rid = 0; rid < num_rows; rid++) {
                const char *row = rows + rid * row_size;
                const char *vptr = row + extended_price_field_offset;
                const double val = *(const double*)vptr;
                if (val > extended_price) {
                  result_count++;
                  // when a predicate passes, add some extra work
                  add_extra_row_cost(extra_row_cost);
                }
              }
            }
          }
        }
        else if (query == "b") {
          if (old_projection && use_cls) {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              print_row(row);
              result_count++;
            }
          } else {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              const char *vptr = row + extended_price_field_offset;
              const double val = *(const double*)vptr;
              if (val > extended_price) {
                print_row(row);
                result_count++;
                add_extra_row_cost(extra_row_cost);
              }
            }
          }
        }
        else if (query == "c") {
          if (old_projection && use_cls) {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              print_row(row);
              result_count++;
            }
          } else {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              const char *vptr = row + extended_price_field_offset;
              const double val = *(const double*)vptr;
              if (val == extended_price) {
                print_row(row);
                result_count++;
                add_extra_row_cost(extra_row_cost);
              }
            }
          }
        }
        else if (query == "d") {
          if (old_projection && use_cls) {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              print_row(row);
              result_count++;
            }
          } else {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              const char *vptr = row + order_key_field_offset;
              const int order_key_val = *(const int*)vptr;
              if (order_key_val == order_key) {
                const char *vptr = row + line_number_field_offset;
                const int line_number_val = *(const int*)vptr;
                if (line_number_val == line_number) {
                  print_row(row);
                  result_count++;
                  add_extra_row_cost(extra_row_cost);
                }
              }
            }
          }
        }
        else if (query == "e") {
          if (old_projection && use_cls) {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              print_row(row);
              result_count++;
            }
          } else {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;

              const int shipdate_val = *((const int *)(row + shipdate_field_offset));
              if (shipdate_val >= ship_date_low && shipdate_val < ship_date_high) {
                const double discount_val = *((const double *)(row + discount_field_offset));
                if (discount_val > discount_low && discount_val < discount_high) {
                  const double quantity_val = *((const double *)(row + quantity_field_offset));
                  if (quantity_val < quantity) {
                    print_row(row);
                    result_count++;
                    add_extra_row_cost(extra_row_cost);
                  }
                }
              }
            }
          }
        }
        else if (query == "f") {
          if (old_projection && use_cls) {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              print_row(row);
              result_count++;
            }
          } else {
            RE2 re(comment_regex);
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              const char *cptr = row + comment_field_offset;
              const std::string comment_val = string_ncopy(cptr,
                  comment_field_length);
              if (RE2::PartialMatch(comment_val, re)) {
                print_row(row);
                result_count++;
                add_extra_row_cost(extra_row_cost);
              }
            }
          }
        }
        else if (query == "fastpath") {
            for (size_t rid = 0; rid < num_rows; rid++) {
              const char *row = rows + rid * row_size;
              print_row(row);
              result_count++;
            }
        }
        else {  // unrecognized query type
          assert(0);
        }
    }

    times.eval2_ns = getns() - eval2_start;

    lock.lock();
    timings.push_back(times);
  }
}

/*
 * 1. free up aio resources
 * 2. put io on work queue
 * 3. wake-up a worker
 */
void handle_cb(librados::completion_t cb, void *arg)
{
  AioState *s = (AioState*)arg;
  s->times.response = getns();

  // there might have been an error, although we can ignore obj not exists err.
  if (s->c->get_return_value()  < 0) {
    if (s->c->get_return_value() != -ENOENT) {
      // we can ignore ENOENT since skyhook generates reads for potentially
      // empty partitions due to partition name generator function.
        cerr << "handle_cb: s->c->get_return_value()="
             << std::to_string(s->c->get_return_value()) << endl;
        assert(s->c->get_return_value() >= 0);
    }
  }
  s->c->release();
  s->c = NULL;

  work_lock.lock();
  ready_ios.push_back(s);
  work_lock.unlock();

  work_cond.notify_one();
}
