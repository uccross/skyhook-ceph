#include <iostream>
#include "query.h"
#include "test/librados/test.h"
#include "gtest/gtest.h"

using namespace librados;

// globals, taken from run-query
unsigned default_op_size = 1 << 22;
unsigned default_objsize = 14100000;
unsigned default_nobject = 10;
const size_t order_key_field_offset = 0;
const size_t line_number_field_offset = 12;
const size_t quantity_field_offset = 16;
const size_t extended_price_field_offset = 24;
const size_t discount_field_offset = 32;
const size_t shipdate_field_offset = 50;
const size_t comment_field_offset = 97;
const size_t comment_field_length = 44;

class SkyhookQuery : public ::testing::Test {
  protected:
    static void SetUpTestCase() {
      pool_name = get_temp_pool_name();
      ASSERT_EQ("", create_one_pool_pp(pool_name, rados));
      ASSERT_EQ(0, rados.ioctx_create(pool_name.c_str(), ioctx));

      // ingest data
      std::string dirname = "./data/";
      for (int i = 0; i < (int) default_nobject; i++) {

        // populate target objects, used by query.cc
        std::string oid = "obj000000" + std::to_string(i) + ".bin";
        target_objects.push_back(oid);

        // from rados tool
        std::cout << "  ... uploading " << (dirname + oid).c_str() << std::endl;
        int fd = open((dirname + oid).c_str(), O_RDONLY);
        ASSERT_GE(fd, 0);

        // how much should we read?
        int count = default_op_size;
        int offset = 0;
        while (count != 0) {
          bufferlist indata;
          count = indata.read_fd(fd, default_op_size);
          ASSERT_GE(count, 0);

          // write the data into rados
          int ret = 0;
          if (offset == 0)
            ret = ioctx.write_full(oid, indata);
          else
            ret = ioctx.write(oid, indata, count, offset);
          ASSERT_GE(ret, 0);

          offset += count;
        }
        close(fd);
      }

      unsigned int nobjects = 0;
      ObjectCursor c = ioctx.object_list_begin();
      ObjectCursor end = ioctx.object_list_end();
      std::cout << "  ... checking size and number of objects" << std::endl;
      while(!ioctx.object_list_is_end(c)) {
        std::vector<ObjectItem> result;
        int n = ioctx.object_list(c, end, default_nobject, {}, &result, &c);
        nobjects += n;

        for (int i = 0; i < n; i++) {
          auto oid = result[i].oid;
          uint64_t psize;
          time_t pmtime;
          int r = ioctx.stat(oid, &psize, &pmtime);
          ASSERT_GE(r, 0);
          ASSERT_EQ(default_objsize, psize);
        }
      }
      ASSERT_EQ(default_nobject, nobjects);
    }

    static void TearDownTestCase() {
      ioctx.close();
    }

    virtual void SetUp() {
      old_projection = false;
      extended_price = 0.0;
      order_key = 0.0;
      line_number = 0.0;
      ship_date_low = -9999;
      ship_date_high = -9999;
      discount_low = -9999.0;
      discount_high = -9999.0;
      quantity = 0.0;
      comment_regex = "";
      use_index = false;
      use_cls = false;
      old_projection = false;
      extra_row_cost = 0;
      quiet = true;

      stop = true;
      result_count = 0;
      rows_returned = 0;
    }

    static void execute() {
      // read data
      for (auto oid : target_objects) {
        AioState *s = new AioState;
        s->c = librados::Rados::aio_create_completion();

        if (use_cls) {
          test_op op;
          op.query = query;
          op.extended_price = extended_price;
          op.order_key = order_key;
          op.line_number = line_number;
          op.ship_date_low = ship_date_low;
          op.ship_date_high = ship_date_high;
          op.discount_low = discount_low;
          op.discount_high = discount_high;
          op.quantity = quantity;
          op.comment_regex = comment_regex;
          op.use_index = use_index;
          op.old_projection = old_projection;
          op.extra_row_cost = extra_row_cost;

          bufferlist inbl;
          ::encode(op, inbl);
          int ret = ioctx.aio_exec(oid, s->c,
            "tabular", "test_query_op", inbl, &s->bl);
          ASSERT_GE(ret, 0);
          s->c->wait_for_complete();
          ret = s->c->get_return_value();
          ASSERT_EQ(0, ret);
        } else {
          int ret = ioctx.aio_read(oid, s->c, &s->bl, 0, 0);
          ASSERT_GE(0, ret);

          s->c->wait_for_complete();
          ASSERT_GE(s->c->get_return_value(), 0);
          s->c->release();
        }
        ready_ios.push_back(s);
      }

      // process data
      worker();
    }

    static Rados rados;
    static IoCtx ioctx;
    static std::string pool_name;
};

Rados SkyhookQuery::rados;
IoCtx SkyhookQuery::ioctx;
std::string SkyhookQuery::pool_name;

/*
 *TEST QUERY B - NO CLS (returns all rows to client)
 *selectivity=1%
 *expect "total result row count: 10172 / 1000000; nrows_processed=1000000"
 *
 * $ run-query --extended-price 91400.0 --query b
 */
TEST_F(SkyhookQuery, QueryBNoCLSLowSelect) {
  query = "b";
  extended_price = 91400.0;

  execute();
  ASSERT_EQ((unsigned) 10172, result_count);
  ASSERT_EQ((unsigned) 1000000, rows_returned);
}

/*
 * TEST QUERY B - NO CLS (returns all rows to client)
 * selectivity=100%
 * expect "total result row count: 1000000 / 1000000; nrows_processed=1000000"
 *
 * $ run-query --extended-price 1.0 --query b
 */
TEST_F(SkyhookQuery, QueryBNoCLSHighSelect)
{
  query = "b";
  extended_price = 1.0;

  execute();
  ASSERT_EQ((unsigned) 1000000, result_count);
  ASSERT_EQ((unsigned) 1000000, rows_returned);
}

/*
 * TEST QUERY D - NO CLS (returns all rows to client)
 * selectivity=single row
 * expect "total result row count: 1 / 1000000; nrows_processed=1000000"
 *
 * $ run-query --query d --order-key 5 --line-number 3
 */
TEST_F(SkyhookQuery, QueryDNoCLSSingleRow)
{
  query = "d";
  order_key = 5;
  line_number = 3;

  execute();
  ASSERT_EQ((unsigned) 1, result_count);
  ASSERT_EQ((unsigned) 1000000, rows_returned);
}

/*
 * TEST QUERY B - WITH CLS (only returns matching rows to client)
 * selectivity=1%
 * expect "total result row count: 10172 / 10172; nrows_processed=1000000"
 *
 * $ run-query  --extended-price 91400.0 --query b --use-cls
 */
TEST_F(SkyhookQuery, QueryBCLSLowSelect)
{
  query = "b";
  extended_price = 91400.0;
  use_cls = true;

  execute();
  ASSERT_EQ((unsigned) 10172, result_count);
  ASSERT_EQ((unsigned) 10172, rows_returned);
}

/*
 * TEST QUERY D - WITH CLS (only returns matching rows to client)
 * selectivity=single row
 * expect "total result row count: 1 / 1; nrows_processed=1000000"
 *
 * run-query  --query d --order-key 5 --line-number 3 --use-cls
 */
TEST_F(SkyhookQuery, QueryDCLSSingleRow)
{
  query = "d";
  order_key = 5;
  line_number = 3;
  use_cls = true;

  execute();
  ASSERT_EQ((unsigned) 1, result_count);
  ASSERT_EQ((unsigned) 1, rows_returned);
}

/*
 * TEST QUERY D - WITH CLS BEFORE BUILD INDEX
 * selectivity=single row
 * expect total result row count: 0 / 0; nrows_processed=0"
 *
 * run-query  --query d --order-key 5 --line-number 3 --use-cls --use-index
 */
TEST_F(SkyhookQuery, QueryDCLSSingleRowNoIndex)
{
  query = "d";
  order_key = 5;
  line_number = 3;
  use_cls = true;
  use_index = true;

  execute();
  ASSERT_EQ((unsigned) 0, result_count);
  ASSERT_EQ((unsigned) 0, rows_returned);
}

/*
 * TEST QUERY D - WITH CLS AFTER BUILD INDEX
 * selectivity=single row
 * expect "total result row count: 1 / 1; nrows_processed=1000000"
 *
 * run-query --query d --build-index
 * run-query --query d --order-key 5 --line-number 3 --use-cls --use-index
 */
TEST_F(SkyhookQuery, QueryDCLSSingleRowIndex)
{
  query = "d";
  order_key = 5;
  line_number = 3;
  use_cls = true;
  use_index = true;
  index_batch_size = 1000;

  // build indices
  auto ioctx = new librados::IoCtx;
  ASSERT_EQ(0, rados.ioctx_create(pool_name.c_str(), *ioctx));
  worker_build_index(ioctx);

  // re-populate targets because build_index pops names off
  for (int i = 0; i < (int) default_nobject; i++)
    target_objects.push_back("obj000000" + std::to_string(i) + ".bin");

  execute();
  ASSERT_EQ((unsigned) 1, result_count);
  ASSERT_EQ((unsigned) 1, rows_returned);
}

/*
 * TEST SIMPLE FASTPATH QUERY WITH CLS
 * selectivity=100% (select * from T)
 * expect "total result row count: 1000000 / 1000000; nrows_processed=1000000"
 *
 * run-query  --query fastpath --use-cls
 */
TEST_F(SkyhookQuery, QueryFastpathCLS)
{
  query = "fastpath";
  use_cls = true;

  execute();
  ASSERT_EQ((unsigned) 1000000, result_count);
  ASSERT_EQ((unsigned) 1000000, rows_returned);
}
