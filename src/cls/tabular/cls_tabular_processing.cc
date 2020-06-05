/*
* Copyright (C) 2018 The Regents of the University of California
* All Rights Reserved
*
* This library can redistribute it and/or modify under the terms
* of the GNU Lesser General Public License Version 2.1 as published
* by the Free Software Foundation.
*
*/

#include "cls_tabular_processing.h"


namespace Tables {


/*
 * Function: processSkyFb
 * Description: Process the input flatbuffer table rowwise for the corresponding
 *              query and encapsulate the output in an output arrow table.
 * @param[out] builder     : Ouput flatbuffer builder containing result set.
 * @param[in] tbl_schema   : Schema of an input table
 * @param[in] query_schema : Schema of an query
 * @param[in] preds        : Predicates for the query
 * @param[in] dataptr      : Input table in the form of char array
 * @param[in] datasz       : Size of char array
 * @param[out] errmsg      : Error message
 * @param[out] row_nums    : Specified rows to be processed
 *
 * Return Value: error code
 */
int processSkyFb(
    flatbuffers::FlatBufferBuilder& flatbldr,
    schema_vec& data_schema,
    schema_vec& query_schema,
    predicate_vec& preds,
    const char* dataptr,
    const size_t datasz,
    std::string& errmsg,
    const std::vector<uint32_t>& row_nums)
{
    int errcode = 0;
    delete_vector dead_rows;
    std::vector<flatbuffers::Offset<Tables::Record>> offs;
    sky_root root = getSkyRoot(dataptr, datasz, SFT_FLATBUF_FLEX_ROW);

    // identify the max col idx, to prevent flexbuf vector oob error
    int col_idx_max = -1;
    for (auto it=data_schema.begin(); it!=data_schema.end(); ++it) {
        if (it->idx > col_idx_max)
            col_idx_max = it->idx;
    }

    bool project_all = std::equal(data_schema.begin(), data_schema.end(),
                                  query_schema.begin(), compareColInfo);

    // build the flexbuf with computed aggregates, aggs are computed for
    // each row that passes, and added to flexbuf after loop below.
    bool encode_aggs = false;
    if (hasAggPreds(preds)) encode_aggs = true;
    bool encode_rows = !encode_aggs;

    // determines if we process specific rows or all rows, since
    // row_nums vector is optional parameter - default process all rows.
    bool process_all_rows = true;
    uint32_t nrows = root.nrows;
    if (!row_nums.empty()) {
        process_all_rows = false;  // process specified row numbers only
        nrows = row_nums.size();
    }

    // 1. check the preds for passing
    // 2a. accumulate agg preds (return flexbuf built after all rows) or
    // 2b. build the return flatbuf inline below from each row's projection
    for (uint32_t i = 0; i < nrows; i++) {

        // process row i or the specified row number
        uint32_t rnum = 0;
        if (process_all_rows) rnum = i;
        else rnum = row_nums[i];
        if (rnum > root.nrows) {
            errmsg += "ERROR: rnum(" + std::to_string(rnum) +
                      ") > root.nrows(" + to_string(root.nrows) + ")";
            return RowIndexOOB;
        }

         // skip dead rows.
        if (root.delete_vec[rnum] == 1) continue;

        // get a skyhook record struct
        sky_rec rec = getSkyRec(static_cast<row_offs>(root.data_vec)->Get(rnum));

        // apply predicates to this record
        if (!preds.empty()) {
            bool pass = applyPredicates(preds, rec);
            if (!pass) continue;  // skip non matching rows.
        }

        // note: agg preds are accumlated in the predicate itself during
        // applyPredicates above, then later added to result fb outside
        // of this loop (i.e., they are not encoded into the result fb yet)
        // thus we can skip the below encoding of rows into the result fb
        // and just continue accumulating agg preds in this processing loop.
        if (!encode_rows) continue;

        if (project_all) {
            // TODO:  just pass through row table offset to new data_vec
            // (which is also type offs), do not rebuild row table and flexbuf
        }

        // build the return projection for this row.
        auto row = rec.data.AsVector();
        flexbuffers::Builder *flexbldr = new flexbuffers::Builder();
        flatbuffers::Offset<flatbuffers::Vector<unsigned char>> datavec;

        flexbldr->Vector([&]() {

            // iter over the query schema, locating it within the data schema
            for (auto it=query_schema.begin();
                      it!=query_schema.end() && !errcode; ++it) {
                col_info col = *it;
                if (col.idx < AGG_COL_LAST or col.idx > col_idx_max) {
                    errcode = TablesErrCodes::RequestedColIndexOOB;
                    errmsg.append("ERROR processSkyFb(): table=" +
                            root.table_name + "; rid=" +
                            std::to_string(rec.RID) + " col.idx=" +
                            std::to_string(col.idx) + " OOB.");

                } else {

                    switch(col.type) {  // encode data val into flexbuf

                        case SDT_INT8:
                            flexbldr->Add(row[col.idx].AsInt8());
                            break;
                        case SDT_INT16:
                            flexbldr->Add(row[col.idx].AsInt16());
                            break;
                        case SDT_INT32:
                            flexbldr->Add(row[col.idx].AsInt32());
                            break;
                        case SDT_INT64:
                            flexbldr->Add(row[col.idx].AsInt64());
                            break;
                        case SDT_UINT8:
                            flexbldr->Add(row[col.idx].AsUInt8());
                            break;
                        case SDT_UINT16:
                            flexbldr->Add(row[col.idx].AsUInt16());
                            break;
                        case SDT_UINT32:
                            flexbldr->Add(row[col.idx].AsUInt32());
                            break;
                        case SDT_UINT64:
                            flexbldr->Add(row[col.idx].AsUInt64());
                            break;
                        case SDT_CHAR:
                            flexbldr->Add(row[col.idx].AsInt8());
                            break;
                        case SDT_UCHAR:
                            flexbldr->Add(row[col.idx].AsUInt8());
                            break;
                        case SDT_BOOL:
                            flexbldr->Add(row[col.idx].AsBool());
                            break;
                        case SDT_FLOAT:
                            flexbldr->Add(row[col.idx].AsFloat());
                            break;
                        case SDT_DOUBLE:
                            flexbldr->Add(row[col.idx].AsDouble());
                            break;
                        case SDT_DATE:
                            flexbldr->Add(row[col.idx].AsString().str());
                            break;
                        case SDT_STRING:
                            flexbldr->Add(row[col.idx].AsString().str());
                            break;
                        default: {
                            errcode = TablesErrCodes::UnsupportedSkyDataType;
                            errmsg.append("ERROR processSkyFb(): table=" +
                                    root.table_name + "; rid=" +
                                    std::to_string(rec.RID) + " col.type=" +
                                    std::to_string(col.type) +
                                    " UnsupportedSkyDataType.");
                        }
                    }
                }
            }
        });

        // finalize the row's projected data within our flexbuf
        flexbldr->Finish();

        // build the return ROW flatbuf that contains the flexbuf data
        auto row_data = flatbldr.CreateVector(flexbldr->GetBuffer());
        delete flexbldr;

        // TODO: update nullbits
        auto nullbits = flatbldr.CreateVector(rec.nullbits);
        flatbuffers::Offset<Tables::Record> row_off = \
                Tables::CreateRecord(flatbldr, rec.RID, nullbits, row_data);

        // Continue building the ROOT flatbuf's dead vector and rowOffsets vec
        dead_rows.push_back(0);
        offs.push_back(row_off);
    }

    // here we build the return flatbuf result with agg values that were
    // accumulated above in applyPredicates (agg predicates do not return
    // true false but update their internal values each time processed
    if (encode_aggs) { //  encode accumulated agg pred val into return flexbuf
        PredicateBase* pb;
        flexbuffers::Builder *flexbldr = new flexbuffers::Builder();
        flexbldr->Vector([&]() {
            for (auto itp = preds.begin(); itp != preds.end(); ++itp) {

                // assumes preds appear in same order as return schema
                if (!(*itp)->isGlobalAgg()) continue;
                pb = *itp;
                switch(pb->colType()) {  // encode agg data val into flexbuf
                    case SDT_INT64: {
                        TypedPredicate<int64_t>* p = \
                                dynamic_cast<TypedPredicate<int64_t>*>(pb);
                        int64_t agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_UINT32: {
                        TypedPredicate<uint32_t>* p = \
                                dynamic_cast<TypedPredicate<uint32_t>*>(pb);
                        uint32_t agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_UINT64: {
                        TypedPredicate<uint64_t>* p = \
                                dynamic_cast<TypedPredicate<uint64_t>*>(pb);
                        uint64_t agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_FLOAT: {
                        TypedPredicate<float>* p = \
                                dynamic_cast<TypedPredicate<float>*>(pb);
                        float agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_DOUBLE: {
                        TypedPredicate<double>* p = \
                                dynamic_cast<TypedPredicate<double>*>(pb);
                        double agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    default:  assert(UnsupportedAggDataType==0);
                }
            }
        });
        // finalize the row's projected data within our flexbuf
        flexbldr->Finish();

        // build the return ROW flatbuf that contains the flexbuf data
        auto row_data = flatbldr.CreateVector(flexbldr->GetBuffer());
        delete flexbldr;

        // assume no nullbits in the agg results. ?
        nullbits_vector nb(2,0);
        auto nullbits = flatbldr.CreateVector(nb);
        int RID = -1;  // agg recs only, since these are derived data
        flatbuffers::Offset<Tables::Record> row_off = \
            Tables::CreateRecord(flatbldr, RID, nullbits, row_data);

        // Continue building the ROOT flatbuf's dead vector and rowOffsets vec
        dead_rows.push_back(0);
        offs.push_back(row_off);
    }

    // now build the return ROOT flatbuf wrapper
    std::string query_schema_str;
    for (auto it = query_schema.begin(); it != query_schema.end(); ++it) {
        query_schema_str.append(it->toString() + "\n");
    }

    auto return_data_schema = flatbldr.CreateString(query_schema_str);
    auto db_schema_name = flatbldr.CreateString(root.db_schema_name);
    auto table_name = flatbldr.CreateString(root.table_name);
    auto delete_v = flatbldr.CreateVector(dead_rows);
    auto rows_v = flatbldr.CreateVector(offs);

    auto table = CreateTable(
        flatbldr,
        root.data_format_type,
        root.skyhook_version,
        root.data_structure_version,
        root.data_schema_version,
        return_data_schema,
        db_schema_name,
        table_name,
        delete_v,
        rows_v,
        offs.size());

    // NOTE: the fb may be incomplete/empty, but must finish() else internal
    // fb lib assert finished() fails, hence we must always return a valid fb
    // and catch any ret error code upstream
    flatbldr.Finish(table);

    return errcode;
}


/*
 * Function: processArrowCol
 * Description: Process the input arrow table columnwise for the corresponding
 *              query and encapsulate the output in an output arrow table.
 * @param[out] table       :  Ouput arrow table containing result set.
 * @param[in] tbl_schema   : Schema of an input table
 * @param[in] query_schema : Schema of an query
 * @param[in] preds        : Predicates for the query
 * @param[in] dataptr      : Input table in the form of char array
 * @param[in] datasz       : Size of char array
 * @param[out] errmsg      : Error message
 * @param[out] row_nums    : Specified rows to be processed
 *
 * Return Value: error code
 */
int processArrowCol(
        std::shared_ptr<arrow::Table>* table,
        schema_vec& tbl_schema,
        schema_vec& query_schema,
        predicate_vec& preds,
        const char* dataptr,
        const size_t datasz,
        std::string& errmsg,
        const std::vector<uint32_t>& row_nums)
{
   int errcode = 0;
    int processed_rows = 0;
    int num_cols = std::distance(tbl_schema.begin(), tbl_schema.end());
    auto pool = arrow::default_memory_pool();
    std::vector<arrow::ArrayBuilder *> builder_list;
    std::vector<std::shared_ptr<arrow::Array>> array_list;
    std::vector<std::shared_ptr<arrow::Field>> output_tbl_fields_vec;
    std::shared_ptr<arrow::Buffer> buffer =                             \
        arrow::MutableBuffer::Wrap(reinterpret_cast<uint8_t*>(const_cast<char*>(dataptr)), datasz);
    std::shared_ptr<arrow::Table> input_table, temp_table;
    std::vector<uint32_t> result_rows;

    // Get input table from dataptr
    extract_arrow_from_buffer(&input_table, buffer);

    auto schema = input_table->schema();
    auto metadata = schema->metadata();
    uint32_t nrows = atoi(metadata->value(METADATA_NUM_ROWS).c_str());

    // TODO: should we verify these are the same as nrows
    // int64_t nrows_from_api = input_table->num_rows();

    // Get number of rows to be processed
    if (!row_nums.empty()) {
        result_rows = row_nums;
    }

    // identify the max col idx, to prevent flexbuf vector oob error
    int col_idx_max = -1;
    for (auto it = tbl_schema.begin(); it != tbl_schema.end(); ++it) {
        if (it->idx > col_idx_max)
            col_idx_max = it->idx;
    }

    // Apply predicates to all the columns and get the rows which
    // satifies the condition
    if (!preds.empty()) {
        // Iterate through each column in print the data inside it
        for (auto it = tbl_schema.begin(); it != tbl_schema.end(); ++it) {
            col_info col = *it;

            applyPredicatesArrowCol(preds,
                                    input_table->column(col.idx)->chunk(0),
                                    col.idx,
                                    result_rows);
        }
        nrows = result_rows.size();
    }

    // At this point we have rows which satisfied the required predicates.
    // Now create the output arrow table from input table.

    // Iterate through query schema vector to get the details of columns i.e name and type.
    // Also, get the builder arrays required for each data type
    for (auto it = query_schema.begin(); it != query_schema.end() && !errcode; ++it) {
        col_info col = *it;

        // Create the array builders for respective datatypes. Use these array
        // builders to store data to array vectors. These array vectors holds the
        // actual column values. Also, add the details of column (Name and Datatype)
        switch(col.type) {

            case SDT_BOOL: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::BooleanBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::boolean()));
                break;
            }
            case SDT_INT8: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int8()));
                break;
            }
            case SDT_INT16: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int16Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int16()));
                break;
            }
            case SDT_INT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int32Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int32()));
                break;
            }
            case SDT_INT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int64Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int64()));
                break;
            }
            case SDT_UINT8: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint8()));
                break;
            }
            case SDT_UINT16: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt16Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint16()));
                break;
            }
            case SDT_UINT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt32Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint32()));
                break;
            }
            case SDT_UINT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt64Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint64()));
                break;
            }
            case SDT_FLOAT: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::FloatBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::float32()));
                break;
            }
            case SDT_DOUBLE: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::DoubleBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::float64()));
                break;
            }
            case SDT_CHAR: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int8()));
                break;
            }
            case SDT_UCHAR: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint8()));
                break;
            }
            case SDT_DATE:
            case SDT_STRING: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::StringBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::utf8()));
                break;
            }
            case SDT_JAGGEDARRAY_BOOL: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::BooleanBuilder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::boolean())));
                break;
            }
            case SDT_JAGGEDARRAY_INT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::Int32Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::int32())));
                break;
            }
             case SDT_JAGGEDARRAY_UINT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::UInt32Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::uint32())));
                break;
            }
            case SDT_JAGGEDARRAY_INT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::Int64Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::int64())));
                break;
            }
            case SDT_JAGGEDARRAY_UINT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::UInt64Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::uint64())));
                break;
            }
            case SDT_JAGGEDARRAY_FLOAT: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::FloatBuilder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::float32())));
                break;
            }
            case SDT_JAGGEDARRAY_DOUBLE: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::DoubleBuilder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::float64())));
                break;
            }
            default: {
                errcode = TablesErrCodes::UnsupportedSkyDataType;
                errmsg.append("ERROR processArrow()");
                return errcode;
            }
        }
    }

    // Copy values from input table rows to the output table rows
    for (uint32_t i = 0; i < nrows; i++) {

        uint32_t rnum = i;
        if (!preds.empty())
            rnum = result_rows[i];

        // skip dead rows.
        auto delvec_chunk = input_table->column(ARROW_DELVEC_INDEX(num_cols))->chunk(0);
        if (std::static_pointer_cast<arrow::BooleanArray>(delvec_chunk)->Value(rnum) == true) continue;

        processed_rows++;

        // iter over the query schema and add the values from input table
        // to output table
        for (auto it = query_schema.begin(); it != query_schema.end() && !errcode; ++it) {
            col_info col = *it;
            auto builder = builder_list[std::distance(query_schema.begin(), it)];

            auto processing_chunk = input_table->column(col.idx)->chunk(0);

            if (col.idx < AGG_COL_LAST or col.idx > col_idx_max) {
                errcode = TablesErrCodes::RequestedColIndexOOB;
                errmsg.append("ERROR processArrowCol()");
                return errcode;
            } else {
                if (col.nullable) {  // check nullbit
                    if (processing_chunk->IsNull(rnum)) {
                        builder->AppendNull();
                        continue;
                    }
                }

                // Append data from input tbale to the respective data type builders
                switch(col.type) {

                    case SDT_BOOL:
                        static_cast<arrow::BooleanBuilder *>(builder)->Append(std::static_pointer_cast<arrow::BooleanArray>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_INT8:
                        static_cast<arrow::Int8Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int8Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_INT16:
                        static_cast<arrow::Int16Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int16Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_INT32:
                        static_cast<arrow::Int32Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int32Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_INT64:
                        static_cast<arrow::Int64Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int64Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_UINT8:
                        static_cast<arrow::UInt8Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt8Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_UINT16:
                        static_cast<arrow::UInt16Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt16Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_UINT32:
                        static_cast<arrow::UInt32Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt32Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_UINT64:
                        static_cast<arrow::UInt64Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt64Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_FLOAT:
                        static_cast<arrow::FloatBuilder *>(builder)->Append(std::static_pointer_cast<arrow::FloatArray>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_DOUBLE:
                        static_cast<arrow::DoubleBuilder *>(builder)->Append(std::static_pointer_cast<arrow::DoubleArray>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_CHAR:
                        static_cast<arrow::Int8Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int8Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_UCHAR:
                        static_cast<arrow::UInt8Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt8Array>(processing_chunk)->Value(rnum));
                        break;
                    case SDT_DATE:
                    case SDT_STRING:
                        static_cast<arrow::StringBuilder *>(builder)->Append(std::static_pointer_cast<arrow::StringArray>(processing_chunk)->GetString(rnum));
                        break;
                    case SDT_JAGGEDARRAY_FLOAT: {
                        // advance to the start of a new list row
                        static_cast<arrow::ListBuilder *>(builder)->Append();

                        // extract list builder as int32 builder
                        // arrow::ListBuilder* lb = static_cast<arrow::ListBuilder *>(builder);
                        // arrow::Int32Builder* ib = static_cast<arrow::Int32Builder *>(lb->value_builder());

                        // TODO: extract prev values and append to ib
                        // ib->AppendValues(vector.data(), vector.size());
                        break;
                    }
                    default: {
                        errcode = TablesErrCodes::UnsupportedSkyDataType;
                        errmsg.append("ERROR processArrow()");
                        return errcode;
                    }
                }
            }
        }
    }

    // Finalize the chunks holding the data
    for (auto it = builder_list.begin(); it != builder_list.end(); ++it) {
        auto builder = *it;
        std::shared_ptr<arrow::Array> chunk;
        builder->Finish(&chunk);
        array_list.push_back(chunk);
        delete builder;
    }

    // Add skyhook metadata to arrow metadata.
    std::shared_ptr<arrow::KeyValueMetadata> output_tbl_metadata (new arrow::KeyValueMetadata);
    output_tbl_metadata->Append(ToString(METADATA_SKYHOOK_VERSION),
                                metadata->value(METADATA_SKYHOOK_VERSION));
    output_tbl_metadata->Append(ToString(METADATA_DATA_SCHEMA_VERSION),
                                metadata->value(METADATA_DATA_SCHEMA_VERSION));
    output_tbl_metadata->Append(ToString(METADATA_DATA_STRUCTURE_VERSION),
                                metadata->value(METADATA_DATA_STRUCTURE_VERSION));
    output_tbl_metadata->Append(ToString(METADATA_DATA_FORMAT_TYPE),
                                metadata->value(METADATA_DATA_FORMAT_TYPE));
    output_tbl_metadata->Append(ToString(METADATA_DATA_SCHEMA),
                                schemaToString(query_schema));
    output_tbl_metadata->Append(ToString(METADATA_DB_SCHEMA),
                                metadata->value(METADATA_DB_SCHEMA));
    output_tbl_metadata->Append(ToString(METADATA_TABLE_NAME),
                                metadata->value(METADATA_TABLE_NAME));
    output_tbl_metadata->Append(ToString(METADATA_NUM_ROWS),
                                std::to_string(processed_rows));

    // Generate schema from schema vector and add the metadata
    schema = std::make_shared<arrow::Schema>(output_tbl_fields_vec, output_tbl_metadata);

    // Finally, create a arrow table from schema and array vector
    *table = arrow::Table::Make(schema, array_list);

    return errcode;

}


/*
 * Function: processArrow
 * Description: Process the input arrow table rowwise for the corresponding input
 *              query and encapsulate the output in an output arrow table.
 * @param[out] table       : Ouput arrow table
 * @param[in] tbl_schema   : Schema of an input table
 * @param[in] query_schema : Schema of an query
 * @param[in] preds        : Predicates for the query
 * @param[in] dataptr      : Input table in the form of char array
 * @param[in] datasz       : Size of char array
 * @param[out] errmsg      : Error message
 * @param[out] row_nums    : Specified rows to be processed
 *
 * Return Value: error code
 */
int processArrow(
    std::shared_ptr<arrow::Table>* table,
    schema_vec& tbl_schema,
    schema_vec& query_schema,
    predicate_vec& preds,
    const char* dataptr,
    const size_t datasz,
    std::string& errmsg,
    const std::vector<uint32_t>& row_nums)
{
    int errcode = 0;
    int processed_rows = 0;
    int num_cols = std::distance(tbl_schema.begin(), tbl_schema.end());
    auto pool = arrow::default_memory_pool();
    std::vector<arrow::ArrayBuilder *> builder_list;
    std::vector<std::shared_ptr<arrow::Array>> array_list;
    std::vector<std::shared_ptr<arrow::Field>> output_tbl_fields_vec;
    std::shared_ptr<arrow::Buffer> buffer = arrow::MutableBuffer::Wrap(reinterpret_cast<uint8_t*>(const_cast<char*>(dataptr)), datasz);
    std::shared_ptr<arrow::Table> input_table, temp_table;

    // Get input table from dataptr
    extract_arrow_from_buffer(&input_table, buffer);

    auto schema = input_table->schema();
    auto metadata = schema->metadata();

    // Get number of rows to be processed
    bool process_all_rows = true;
    uint32_t nrows = atoi(metadata->value(METADATA_NUM_ROWS).c_str());
    if (!row_nums.empty()) {
        process_all_rows = false;  // process specified row numbers only
        nrows = row_nums.size();
    }

    // identify the max col idx, to prevent flexbuf vector oob error
    int col_idx_max = -1;
    for (auto it = tbl_schema.begin(); it != tbl_schema.end(); ++it) {
        if (it->idx > col_idx_max)
            col_idx_max = it->idx;
    }

    // Iterate through query schema vector to get the details of columns i.e name and type.
    // Also, get the builder arrays required for each data type
    for (auto it = query_schema.begin(); it != query_schema.end() && !errcode; ++it) {
        col_info col = *it;

        // Create the array builders for respective datatypes. Use these array
        // builders to store data to array vectors. These array vectors holds the
        // actual column values. Also, add the details of column (Name and Datatype)
        switch(col.type) {

            case SDT_BOOL: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::BooleanBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::boolean()));
                break;
            }
            case SDT_INT8: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int8()));
                break;
            }
            case SDT_INT16: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int16Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int16()));
                break;
            }
            case SDT_INT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int32Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int32()));
                break;
            }
            case SDT_INT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int64Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int64()));
                break;
            }
            case SDT_UINT8: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint8()));
                break;
            }
            case SDT_UINT16: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt16Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint16()));
                break;
            }
            case SDT_UINT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt32Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint32()));
                break;
            }
            case SDT_UINT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt64Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint64()));
                break;
            }
            case SDT_FLOAT: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::FloatBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::float32()));
                break;
            }
            case SDT_DOUBLE: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::DoubleBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::float64()));
                break;
            }
            case SDT_CHAR: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::Int8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::int8()));
                break;
            }
            case SDT_UCHAR: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::UInt8Builder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::uint8()));
                break;
            }
            case SDT_DATE:
            case SDT_STRING: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::StringBuilder(pool));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::utf8()));
                break;
            }
            case SDT_JAGGEDARRAY_BOOL: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::BooleanBuilder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::boolean())));
                break;
            }
            case SDT_JAGGEDARRAY_INT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::Int32Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::int32())));
                break;
            }
             case SDT_JAGGEDARRAY_UINT32: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::UInt32Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::uint32())));
                break;
            }
            case SDT_JAGGEDARRAY_INT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::Int64Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::int64())));
                break;
            }
            case SDT_JAGGEDARRAY_UINT64: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::UInt64Builder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::uint64())));
                break;
            }
            case SDT_JAGGEDARRAY_FLOAT: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::FloatBuilder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::float32())));
                break;
            }
            case SDT_JAGGEDARRAY_DOUBLE: {
                auto ptr = std::unique_ptr<arrow::ArrayBuilder>(new arrow::ListBuilder(pool,std::make_shared<arrow::DoubleBuilder>(pool)));
                builder_list.emplace_back(ptr.get());
                ptr.release();
                output_tbl_fields_vec.push_back(arrow::field(col.name, arrow::list(arrow::float64())));
                break;
            }
            default: {
                errcode = TablesErrCodes::UnsupportedSkyDataType;
                errmsg.append("ERROR processArrow()");
                return errcode;
            }
        }
    }

    for (uint32_t i = 0; i < nrows; i++) {

        // process row i or the specified row number
        uint32_t rnum = 0;
        if (process_all_rows) rnum = i;
        else rnum = row_nums[i];
        if (rnum > nrows) {
            errmsg += "ERROR: rnum(" + std::to_string(rnum) +
                      ") > nrows(" + to_string(nrows) + ")";
            return RowIndexOOB;
        }

        // skip dead rows.
        auto delvec_chunk = input_table->column(ARROW_DELVEC_INDEX(num_cols))->chunk(0);
        if (std::static_pointer_cast<arrow::BooleanArray>(delvec_chunk)->Value(i) == true) continue;

        // Apply predicates
        if (!preds.empty()) {
            bool pass = applyPredicatesArrow(preds, input_table, i);
            if (!pass) continue;  // skip non matching rows.
        }
        processed_rows++;

        // iter over the query schema and add the values from input table
        // to output table
        for (auto it = query_schema.begin(); it != query_schema.end() && !errcode; ++it) {
            col_info col = *it;
            auto builder = builder_list[std::distance(query_schema.begin(), it)];

            auto processing_chunk = input_table->column(col.idx)->chunk(0);

            if (col.idx < AGG_COL_LAST or col.idx > col_idx_max) {
                errcode = TablesErrCodes::RequestedColIndexOOB;
                errmsg.append("ERROR processArrow()");
                return errcode;
            } else {
                if (col.nullable) {  // check nullbit
                    if (processing_chunk->IsNull(i)) {
                        builder->AppendNull();
                        continue;
                    }
                }

                // Append data from input tbale to the respective data type builders
                switch(col.type) {

                case SDT_BOOL:
                    static_cast<arrow::BooleanBuilder *>(builder)->Append(std::static_pointer_cast<arrow::BooleanArray>(processing_chunk)->Value(i));
                    break;
                case SDT_INT8:
                    static_cast<arrow::Int8Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int8Array>(processing_chunk)->Value(i));
                    break;
                case SDT_INT16:
                    static_cast<arrow::Int16Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int16Array>(processing_chunk)->Value(i));
                    break;
                case SDT_INT32:
                    static_cast<arrow::Int32Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int32Array>(processing_chunk)->Value(i));
                    break;
                case SDT_INT64:
                    static_cast<arrow::Int64Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int64Array>(processing_chunk)->Value(i));
                    break;
                case SDT_UINT8:
                    static_cast<arrow::UInt8Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt8Array>(processing_chunk)->Value(i));
                    break;
                case SDT_UINT16:
                    static_cast<arrow::UInt16Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt16Array>(processing_chunk)->Value(i));
                    break;
                case SDT_UINT32:
                    static_cast<arrow::UInt32Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt32Array>(processing_chunk)->Value(i));
                    break;
                case SDT_UINT64:
                    static_cast<arrow::UInt64Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt64Array>(processing_chunk)->Value(i));
                    break;
                case SDT_FLOAT:
                    static_cast<arrow::FloatBuilder *>(builder)->Append(std::static_pointer_cast<arrow::FloatArray>(processing_chunk)->Value(i));
                    break;
                case SDT_DOUBLE:
                    static_cast<arrow::DoubleBuilder *>(builder)->Append(std::static_pointer_cast<arrow::DoubleArray>(processing_chunk)->Value(i));
                    break;
                case SDT_CHAR:
                    static_cast<arrow::Int8Builder *>(builder)->Append(std::static_pointer_cast<arrow::Int8Array>(processing_chunk)->Value(i));
                    break;
                case SDT_UCHAR:
                    static_cast<arrow::UInt8Builder *>(builder)->Append(std::static_pointer_cast<arrow::UInt8Array>(processing_chunk)->Value(i));
                    break;
                case SDT_DATE:
                case SDT_STRING:
                    static_cast<arrow::StringBuilder *>(builder)->Append(std::static_pointer_cast<arrow::StringArray>(processing_chunk)->GetString(i));
                    break;
                default: {
                    errcode = TablesErrCodes::UnsupportedSkyDataType;
                    errmsg.append("ERROR processArrow()");
                    return errcode;
                }
                }
            }
        }
    }

    // Finalize the chunks holding the data
    for (auto it = builder_list.begin(); it != builder_list.end(); ++it) {
        auto builder = *it;
        std::shared_ptr<arrow::Array> chunk;
        builder->Finish(&chunk);
        array_list.push_back(chunk);
        delete builder;
    }

    std::shared_ptr<arrow::KeyValueMetadata> output_tbl_metadata (new arrow::KeyValueMetadata);
    // Add skyhook metadata to arrow metadata.
    output_tbl_metadata->Append(ToString(METADATA_SKYHOOK_VERSION),
                          metadata->value(METADATA_SKYHOOK_VERSION));
    output_tbl_metadata->Append(ToString(METADATA_DATA_SCHEMA_VERSION),
                          metadata->value(METADATA_DATA_SCHEMA_VERSION));
    output_tbl_metadata->Append(ToString(METADATA_DATA_STRUCTURE_VERSION),
                          metadata->value(METADATA_DATA_STRUCTURE_VERSION));
    output_tbl_metadata->Append(ToString(METADATA_DATA_FORMAT_TYPE),
                          metadata->value(METADATA_DATA_FORMAT_TYPE));
    output_tbl_metadata->Append(ToString(METADATA_DATA_SCHEMA),
                          schemaToString(query_schema));
    output_tbl_metadata->Append(ToString(METADATA_DB_SCHEMA),
                          metadata->value(METADATA_DB_SCHEMA));
    output_tbl_metadata->Append(ToString(METADATA_TABLE_NAME),
                          metadata->value(METADATA_TABLE_NAME));
    output_tbl_metadata->Append(ToString(METADATA_NUM_ROWS),
                          std::to_string(processed_rows));

    // Generate schema from schema vector and add the metadata
    schema = std::make_shared<arrow::Schema>(output_tbl_fields_vec, output_tbl_metadata);

    // Finally, create a arrow table from schema and array vector
    *table = arrow::Table::Make(schema, array_list);
    return errcode;
}



/*
 * For wasm execution testing.
 * Method is otherwise identical to processSkyFb, except signature is only
 * int or char for wasm compatibility.  We convert these back to their typical
 * types then proceed with same flatbuffer processing code.
 * Just before returning we also copy the local string error message back into
 * the caller's char* ptr.
 */
int processSkyFbWASM(
        char* _flatbldr,
        int _flatbldr_len,
        char* _data_schema,
        int _data_schema_len,
        char* _query_schema,
        int _query_schema_len,
        char* _preds,
        int _preds_len,
        char* _fb,
        int _fb_size,
        char* _errmsg,
        int _errmsg_len,
        int* _row_nums,
        int _row_nums_size)
{

    // this is the result builder
    flatbuffers::FlatBufferBuilder *flatbldr = (flatbuffers::FlatBufferBuilder*) _flatbldr;

    // query params strings to skyhook types.
    schema_vec data_schema = schemaFromString(std::string(_data_schema, _data_schema_len));
    schema_vec query_schema = schemaFromString(std::string(_query_schema, _query_schema_len));
    predicate_vec preds = predsFromString(data_schema, std::string(_preds, _preds_len));

    // this is the in-mem data (as a flatbuffer)
    const char* fb = _fb;
    int fb_size = _fb_size;

    // local var, need to copy back into our errmsg ptr before returning
    // append messages with errmsg.append();
    // but only max  _errmsg_len will be copied back into caller's ptr
    std::string errmsg;

    // row_nums array represents those found by an index, if any.
    std::vector<uint32_t> row_nums(&_row_nums[0], &_row_nums[_row_nums_size]);

    int errcode = 0;
    delete_vector dead_rows;
    std::vector<flatbuffers::Offset<Tables::Record>> offs;
    sky_root root = getSkyRoot(fb, fb_size, SFT_FLATBUF_FLEX_ROW);

    // identify the max col idx, to prevent flexbuf vector oob error
    int col_idx_max = -1;
    for (auto it=data_schema.begin(); it!=data_schema.end(); ++it) {
        if (it->idx > col_idx_max)
            col_idx_max = it->idx;
    }

    bool project_all = std::equal(data_schema.begin(), data_schema.end(),
                                  query_schema.begin(), compareColInfo);

    // build the flexbuf with computed aggregates, aggs are computed for
    // each row that passes, and added to flexbuf after loop below.
    bool encode_aggs = false;
    if (hasAggPreds(preds)) encode_aggs = true;
    bool encode_rows = !encode_aggs;

    // determines if we process specific rows (from index lookup) or all rows
    bool process_all_rows = false;
    uint32_t nrows = 0;
    if (row_nums.empty()) {         // default is empty, so process them all
        process_all_rows = true;
        nrows = root.nrows;
    }
    else {
        process_all_rows = false;  // process only specific rows
        nrows = row_nums.size();
    }

    // 1. check the preds for passing
    // 2a. accumulate agg preds (return flexbuf built after all rows) or
    // 2b. build the return flatbuf inline below from each row's projection
    for (uint32_t i = 0; i < nrows; i++) {

        // process row i or the specified row number
        uint32_t rnum = 0;
        if (process_all_rows) rnum = i;
        else rnum = row_nums[i];

        if (rnum > root.nrows) {
            errmsg += "ERROR: rnum(" + std::to_string(rnum) +
                      ") > root.nrows(" + to_string(root.nrows) + ")";
            //  copy finite len errmsg into caller's char ptr
            size_t len = _errmsg_len;
            if(errmsg.length()+1 < len)
                len = errmsg.length()+1;
            strncpy(_errmsg, errmsg.c_str(), len);
            return RowIndexOOB;
        }

         // skip dead rows.
        if (root.delete_vec[rnum] == 1) continue;

        // get a skyhook record struct
        sky_rec rec = getSkyRec(static_cast<row_offs>(root.data_vec)->Get(rnum));

        // apply predicates to this record
        if (!preds.empty()) {
            bool pass = applyPredicates(preds, rec);
            if (!pass) continue;  // skip non matching rows.
        }

        // note: agg preds are accumlated in the predicate itself during
        // applyPredicates above, then later added to result fb outside
        // of this loop (i.e., they are not encoded into the result fb yet)
        // thus we can skip the below encoding of rows into the result fb
        // and just continue accumulating agg preds in this processing loop.
        if (!encode_rows) continue;

        if (project_all) {
            // TODO:  just pass through row table offset to new data_vec
            // (which is also type offs), do not rebuild row table and flexbuf
        }

        // build the return projection for this row.
        auto row = rec.data.AsVector();
        flexbuffers::Builder *flexbldr = new flexbuffers::Builder();
        flatbuffers::Offset<flatbuffers::Vector<unsigned char>> datavec;

        flexbldr->Vector([&]() {

            // iter over the query schema, locating it within the data schema
            for (auto it=query_schema.begin();
                      it!=query_schema.end() && !errcode; ++it) {
                col_info col = *it;
                if (col.idx < AGG_COL_LAST or col.idx > col_idx_max) {
                    errcode = TablesErrCodes::RequestedColIndexOOB;
                    errmsg.append("ERROR processSkyFb(): table=" +
                            root.table_name + "; rid=" +
                            std::to_string(rec.RID) + " col.idx=" +
                            std::to_string(col.idx) + " OOB.");

                } else {

                    switch(col.type) {  // encode data val into flexbuf

                        case SDT_INT8:
                            flexbldr->Add(row[col.idx].AsInt8());
                            break;
                        case SDT_INT16:
                            flexbldr->Add(row[col.idx].AsInt16());
                            break;
                        case SDT_INT32:
                            flexbldr->Add(row[col.idx].AsInt32());
                            break;
                        case SDT_INT64:
                            flexbldr->Add(row[col.idx].AsInt64());
                            break;
                        case SDT_UINT8:
                            flexbldr->Add(row[col.idx].AsUInt8());
                            break;
                        case SDT_UINT16:
                            flexbldr->Add(row[col.idx].AsUInt16());
                            break;
                        case SDT_UINT32:
                            flexbldr->Add(row[col.idx].AsUInt32());
                            break;
                        case SDT_UINT64:
                            flexbldr->Add(row[col.idx].AsUInt64());
                            break;
                        case SDT_CHAR:
                            flexbldr->Add(row[col.idx].AsInt8());
                            break;
                        case SDT_UCHAR:
                            flexbldr->Add(row[col.idx].AsUInt8());
                            break;
                        case SDT_BOOL:
                            flexbldr->Add(row[col.idx].AsBool());
                            break;
                        case SDT_FLOAT:
                            flexbldr->Add(row[col.idx].AsFloat());
                            break;
                        case SDT_DOUBLE:
                            flexbldr->Add(row[col.idx].AsDouble());
                            break;
                        case SDT_DATE:
                            flexbldr->Add(row[col.idx].AsString().str());
                            break;
                        case SDT_STRING:
                            flexbldr->Add(row[col.idx].AsString().str());
                            break;
                        default: {
                            errcode = TablesErrCodes::UnsupportedSkyDataType;
                            errmsg.append("ERROR processSkyFb(): table=" +
                                    root.table_name + "; rid=" +
                                    std::to_string(rec.RID) + " col.type=" +
                                    std::to_string(col.type) +
                                    " UnsupportedSkyDataType.");
                        }
                    }
                }
            }
        });

        // finalize the row's projected data within our flexbuf
        flexbldr->Finish();

        // build the return ROW flatbuf that contains the flexbuf data
        auto row_data = flatbldr->CreateVector(flexbldr->GetBuffer());
        delete flexbldr;

        // TODO: update nullbits
        auto nullbits = flatbldr->CreateVector(rec.nullbits);
        flatbuffers::Offset<Tables::Record> row_off = \
                Tables::CreateRecord(*flatbldr, rec.RID, nullbits, row_data);

        // Continue building the ROOT flatbuf's dead vector and rowOffsets vec
        dead_rows.push_back(0);
        offs.push_back(row_off);
    }

    // here we build the return flatbuf result with agg values that were
    // accumulated above in applyPredicates (agg predicates do not return
    // true false but update their internal values each time processed
    if (encode_aggs) { //  encode accumulated agg pred val into return flexbuf
        PredicateBase* pb;
        flexbuffers::Builder *flexbldr = new flexbuffers::Builder();
        flexbldr->Vector([&]() {
            for (auto itp = preds.begin(); itp != preds.end(); ++itp) {

                // assumes preds appear in same order as return schema
                if (!(*itp)->isGlobalAgg()) continue;
                pb = *itp;
                switch(pb->colType()) {  // encode agg data val into flexbuf
                    case SDT_INT64: {
                        TypedPredicate<int64_t>* p = \
                                dynamic_cast<TypedPredicate<int64_t>*>(pb);
                        int64_t agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_UINT32: {
                        TypedPredicate<uint32_t>* p = \
                                dynamic_cast<TypedPredicate<uint32_t>*>(pb);
                        uint32_t agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_UINT64: {
                        TypedPredicate<uint64_t>* p = \
                                dynamic_cast<TypedPredicate<uint64_t>*>(pb);
                        uint64_t agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_FLOAT: {
                        TypedPredicate<float>* p = \
                                dynamic_cast<TypedPredicate<float>*>(pb);
                        float agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    case SDT_DOUBLE: {
                        TypedPredicate<double>* p = \
                                dynamic_cast<TypedPredicate<double>*>(pb);
                        double agg_val = p->Val();
                        flexbldr->Add(agg_val);
                        p->updateAgg(0);  // reset accumulated add val
                        break;
                    }
                    default:  assert(UnsupportedAggDataType==0);
                }
            }
        });
        // finalize the row's projected data within our flexbuf
        flexbldr->Finish();

        // build the return ROW flatbuf that contains the flexbuf data
        auto row_data = flatbldr->CreateVector(flexbldr->GetBuffer());
        delete flexbldr;

        // assume no nullbits in the agg results. ?
        nullbits_vector nb(2,0);
        auto nullbits = flatbldr->CreateVector(nb);
        int RID = -1;  // agg recs only, since these are derived data
        flatbuffers::Offset<Tables::Record> row_off = \
            Tables::CreateRecord(*flatbldr, RID, nullbits, row_data);

        // Continue building the ROOT flatbuf's dead vector and rowOffsets vec
        dead_rows.push_back(0);
        offs.push_back(row_off);
    }

    // now build the return ROOT flatbuf wrapper
    std::string query_schema_str;
    for (auto it = query_schema.begin(); it != query_schema.end(); ++it) {
        query_schema_str.append(it->toString() + "\n");
    }

    auto return_data_schema = flatbldr->CreateString(query_schema_str);
    auto db_schema_name = flatbldr->CreateString(root.db_schema_name);
    auto table_name = flatbldr->CreateString(root.table_name);
    auto delete_v = flatbldr->CreateVector(dead_rows);
    auto rows_v = flatbldr->CreateVector(offs);

    auto table = CreateTable(
        *flatbldr,
        root.data_format_type,
        root.skyhook_version,
        root.data_structure_version,
        root.data_schema_version,
        return_data_schema,
        db_schema_name,
        table_name,
        delete_v,
        rows_v,
        offs.size());

    // NOTE: the fb may be incomplete/empty, but must finish() else internal
    // fb lib assert finished() fails, hence we must always return a valid fb
    // and catch any ret error code upstream
    flatbldr->Finish(table);

    // copy finite len errmsg into caller's char ptr
    size_t len = _errmsg_len;
    if(errmsg.length()+1 < len)
        len = errmsg.length()+1;
    strncpy(_errmsg, errmsg.c_str(), len);

    return errcode;
}


}  // end namepace Tables

