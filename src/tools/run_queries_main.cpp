#include "src/structs/schema.h"
#include "src/structs/columnar_format.h"
#include "src/csv_io/csv_writer.h"
#include "src/operators/table_scan.h"
#include "src/operators/filter.h"
#include "src/operators/projection.h"
#include "src/operators/global_aggregate.h"
#include "src/operators/group_aggregate.h"
#include "src/operators/topk.h"
#include "src/operators/sort.h"
#include "src/operators/limit.h"
#include "src/operators/offset.h"
#include "src/expressions/column_ref.h"
#include "src/expressions/constant.h"
#include "src/expressions/comparison.h"
#include "src/expressions/like_function.h"
#include "src/expressions/strlen_function.h"
#include "src/expressions/extract_minute.h"
#include "src/expressions/regexp_replace.h"
#include "src/expressions/binary_arithmetic.h"
#include "src/expressions/logical.h"
#include "src/expressions/if_function.h"
#include "src/expressions/extract_hour.h"
#include "src/expressions/date_trunc.h"
#include "src/utils/date_utils.h"

#include <iostream>
#include <string>
#include <numeric>
#include <filesystem>


static void WriteBatchToCSV(const Batch &batch, const std::string &csv_path) {
    CsvWriter writer(csv_path);
    std::vector<std::string> header;
    for (const auto &col_def: batch.schema.columns)
        header.push_back(col_def.name);
    writer.WriteRow(header);
    for (size_t r = 0; r < batch.numRows; ++r) {
        std::vector<std::string> row;
        for (size_t c = 0; c < batch.columns.size(); ++c)
            row.push_back(batch.columns[c]->GetAsString(r));
        writer.WriteRow(row);
    }
}

static std::string getArg(int argc, char **argv, const std::string &key,
                          const std::string &def = "") {
    for (int i = 1; i < argc - 1; ++i)
        if (std::string(argv[i]) == key)
            return argv[i + 1];
    return def;
}

using Clock = std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

static void RunQuery(int queryNum,
                     ColumnarReader &reader,
                     const std::string &outputDir) {
    const Schema &schema = reader.GetSchema();
    const size_t ncols = schema.NumColumns();

    auto idx = [&](const std::string &name) -> int {
        int i = schema.FindColumn(name);
        if (i < 0) {
            std::cerr << "Column '" << name << "' not found!\n";
            exit(1);
        }
        return i;
    };
    auto colType = [&](int c) -> TypesId {
        return schema.columns[c].typeId;
    };

    char buf[32];
    std::snprintf(buf, sizeof(buf), "q%02d.csv", queryNum);
    std::string outputPath = outputDir + "/" + buf;

    const int adv_engine = idx("AdvEngineID");
    const int user_id = idx("UserID");
    const int search_phr = idx("SearchPhrase");
    const int event_date = idx("EventDate");
    const int event_time = idx("EventTime");
    const int region_id = idx("RegionID");
    const int res_width = idx("ResolutionWidth");
    const int mobile_phone = idx("MobilePhone");
    const int mobile_model = idx("MobilePhoneModel");
    const int search_eng = idx("SearchEngineID");
    const int url = idx("URL");
    const int title = idx("Title");
    const int referer = idx("Referer");
    const int counter_id = idx("CounterID");
    const int is_refresh = idx("IsRefresh");
    const int client_ip = idx("ClientIP");
    const int watch_id = idx("WatchID");
    const int dont_count_hits = idx("DontCountHits");
    const int is_link = idx("IsLink");
    const int is_download = idx("IsDownload");
    const int trafic_source = idx("TraficSourceID");
    const int url_hash = idx("URLHash");
    const int referer_hash = idx("RefererHash");
    const int window_client_w = idx("WindowClientWidth");
    const int window_client_h = idx("WindowClientHeight");

    switch (queryNum + 1) {
        case 1: {
                auto start = Clock::now();

                auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{0});
                std::vector<GlobalAggregate::Spec> specs;
                specs.emplace_back(GlobalAggregate::AggOp::COUNT, nullptr);
                auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
                auto res = agg->Next();
                if (res) WriteBatchToCSV(*res, outputPath);

                auto end = Clock::now();
                std::cerr << "Query 1: "
                        << duration_cast<milliseconds>(end - start).count()
                        << " ms\n";
                break;
            }

        case 2: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(adv_engine)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(adv_engine)),
                                                     std::make_unique<Constant>(int64_t(0)));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT, nullptr);
            auto agg = std::make_unique<GlobalAggregate>(std::move(filter), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 2: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 3: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(adv_engine), size_t(res_width)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::SUM, std::make_unique<ColumnRef>(0, colType(adv_engine)));
            specs.emplace_back(GlobalAggregate::AggOp::COUNT, nullptr);
            specs.emplace_back(GlobalAggregate::AggOp::AVG, std::make_unique<ColumnRef>(1, colType(res_width)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) {
                res->schema.columns[0].name = "SUM(AdvEngineID)";
                res->schema.columns[1].name = "COUNT(*)";
                res->schema.columns[2].name = "AVG(ResolutionWidth)";
                WriteBatchToCSV(*res, outputPath);
            }
            auto end = Clock::now();
            std::cerr << "Query 3: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 4: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::AVG, std::make_unique<ColumnRef>(0, colType(user_id)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 4: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 5: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT_DISTINCT,
                               std::make_unique<ColumnRef>(0, colType(user_id)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 5: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 6: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(search_phr)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT_DISTINCT,
                               std::make_unique<ColumnRef>(0, colType(search_phr)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 6: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 7: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(event_date)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::MIN, std::make_unique<ColumnRef>(0, colType(event_date)));
            specs.emplace_back(GlobalAggregate::AggOp::MAX, std::make_unique<ColumnRef>(0, colType(event_date)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 7: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 8: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(adv_engine)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(adv_engine)),
                                                     std::make_unique<Constant>(int64_t(0)));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(adv_engine)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{1, false});
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto res = sort->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 8: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 9: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(region_id), size_t(user_id)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(region_id)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT_DISTINCT,
                                   std::make_unique<ColumnRef>(1, colType(user_id)));
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 9: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 10: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(region_id), size_t(adv_engine), size_t(res_width),
                                                        size_t(user_id)
                                                    });

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(region_id)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::SUM, std::make_unique<ColumnRef>(1, colType(adv_engine)));
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            agg_specs.emplace_back(GroupAggregate::AggOp::AVG, std::make_unique<ColumnRef>(2, colType(res_width)));
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT_DISTINCT,
                                   std::make_unique<ColumnRef>(3, colType(user_id)));
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 10: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 11: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(mobile_model), size_t(user_id)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(mobile_model)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(mobile_model)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT_DISTINCT,
                                   std::make_unique<ColumnRef>(1, colType(user_id)));
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 11: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 12: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(mobile_phone), size_t(mobile_model), size_t(user_id)
                                                    });
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(1, colType(mobile_model)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(mobile_phone)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(mobile_model)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT_DISTINCT,
                                   std::make_unique<ColumnRef>(2, colType(user_id)));
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 12: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 13: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(search_phr)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 13: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 14: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(search_phr), size_t(user_id)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT_DISTINCT,
                                   std::make_unique<ColumnRef>(1, colType(user_id)));
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 14: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 15: {
            auto start = Clock::now();

            auto scan = std::make_unique<
                TableScan>(reader, std::vector<size_t>{size_t(search_eng), size_t(search_phr)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(1, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(search_eng)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 15: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 16: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(user_id)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 16: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 17: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id), size_t(search_phr)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(user_id)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 17: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 18: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id), size_t(search_phr)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(user_id)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            auto limit = std::make_unique<Limit>(std::move(group), 10);
            auto res = limit->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 18: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 19: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(user_id), size_t(event_time), size_t(search_phr)
                                                    });
            std::vector<std::unique_ptr<Expression> > proj_exprs;
            proj_exprs.push_back(std::make_unique<ColumnRef>(0, colType(user_id)));
            proj_exprs.push_back(std::make_unique<ExtractMinuteFunction>(
                std::make_unique<ColumnRef>(1, colType(event_time))));
            proj_exprs.push_back(std::make_unique<ColumnRef>(2, colType(search_phr)));
            std::vector<std::string> aliases = {"UserID", "m", "SearchPhrase"};
            auto proj = std::make_unique<Projection>(std::move(scan), std::move(proj_exprs), std::move(aliases));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(user_id)));
            keys.push_back(std::make_unique<ColumnRef>(1, TypesId::int64));
            keys.push_back(std::make_unique<ColumnRef>(2, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(proj), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(3, TypesId::int64), false);
            top_keys.emplace_back(std::make_unique<ColumnRef>(0, colType(user_id)), true);
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), true);
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, colType(search_phr)), true);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 19: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 20: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            auto pred = std::make_unique<Comparison>(CmpOp::EQ,
                                                     std::make_unique<ColumnRef>(0, colType(user_id)),
                                                     std::make_unique<Constant>(int64_t(435090932899640449)));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            CsvWriter writer(outputPath);
            writer.WriteRow({"UserID"});
            while (auto batch_opt = filter->Next()) {
                for (size_t r = 0; r < batch_opt->ActiveRows(); ++r) {
                    writer.WriteRow({batch_opt->GetValue(0, r)});
                }
            }

            auto end = Clock::now();
            std::cerr << "Query 20: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 21: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(url)});
            auto pred = std::make_unique<LikeFunction>(std::make_unique<ColumnRef>(0, colType(url)), "%google%");
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT, nullptr);
            auto agg = std::make_unique<GlobalAggregate>(std::move(filter), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 21: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 22: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(search_phr), size_t(url)});
            auto pred1 = std::make_unique<LikeFunction>(std::make_unique<ColumnRef>(1, colType(url)), "%google%");
            auto pred2 = std::make_unique<Comparison>(CmpOp::NE,
                                                      std::make_unique<ColumnRef>(0, colType(search_phr)),
                                                      std::make_unique<Constant>(std::string("")));
            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(pred1));
            and_ops.push_back(std::move(pred2));
            auto pred = std::make_unique<LogicalAnd>(std::move(and_ops));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::MIN, std::make_unique<ColumnRef>(1, colType(url)));
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 22: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 23: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(search_phr), size_t(url), size_t(title), size_t(user_id)
                                                    });
            auto pred1 = std::make_unique<LikeFunction>(std::make_unique<ColumnRef>(2, colType(title)), "%Google%");
            auto pred2 = std::make_unique<LogicalNot>(
                std::make_unique<LikeFunction>(std::make_unique<ColumnRef>(1, colType(url)), "%.google.%"));
            auto pred3 = std::make_unique<Comparison>(CmpOp::NE,
                                                      std::make_unique<ColumnRef>(0, colType(search_phr)),
                                                      std::make_unique<Constant>(std::string("")));
            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(pred1));
            and_ops.push_back(std::move(pred2));
            and_ops.push_back(std::move(pred3));
            auto combined = std::make_unique<LogicalAnd>(std::move(and_ops));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(combined));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::MIN, std::make_unique<ColumnRef>(1, colType(url)));
            agg_specs.emplace_back(GroupAggregate::AggOp::MIN, std::make_unique<ColumnRef>(2, colType(title)));
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT_DISTINCT,
                                   std::make_unique<ColumnRef>(3, colType(user_id)));
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(3, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 23: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 24: {
            auto start = Clock::now();

            std::vector<size_t> all_cols(ncols);
            std::iota(all_cols.begin(), all_cols.end(), 0);
            auto scan = std::make_unique<TableScan>(reader, all_cols);
            auto pred = std::make_unique<LikeFunction>(std::make_unique<ColumnRef>(url, colType(url)), "%google%");
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(event_time, colType(event_time)), true);
            auto topk = std::make_unique<TopK>(std::move(filter), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 24: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 25: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(
                reader,
                std::vector<size_t>{size_t(search_phr), size_t(event_time)});

            auto pred = std::make_unique<Comparison>(
                CmpOp::NE,
                std::make_unique<ColumnRef>(0, colType(search_phr)),
                std::make_unique<Constant>(std::string("")));

            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));

            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(
                std::make_unique<ColumnRef>(1, colType(event_time)), true);
            top_keys.emplace_back(
                std::make_unique<ColumnRef>(0, colType(search_phr)), true);

            std::vector<std::unique_ptr<Expression> > out_exprs;
            out_exprs.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            out_exprs.push_back(std::make_unique<ColumnRef>(1, colType(event_time)));

            auto topk = std::make_unique<TopK>(
                std::move(filter), std::move(top_keys), 10, std::move(out_exprs));

            auto res = topk->Next();
            if (res) {
                res->schema.columns[0].name = "SearchPhrase";
                res->schema.columns[1].name = "EventTime";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 25: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 26: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(search_phr)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(0, colType(search_phr)), true);
            auto topk = std::make_unique<TopK>(std::move(filter), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 26: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 27: {
            auto start = Clock::now();

            auto scan = std::make_unique<
                TableScan>(reader, std::vector<size_t>{size_t(search_phr), size_t(event_time)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, colType(event_time)), true);
            top_keys.emplace_back(std::make_unique<ColumnRef>(0, colType(search_phr)), true);
            std::vector<std::unique_ptr<Expression> > out_exprs;
            out_exprs.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            auto topk = std::make_unique<TopK>(std::move(filter), std::move(top_keys), 10, std::move(out_exprs));
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 27: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 28: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(counter_id), size_t(url)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(1, colType(url)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > proj_exprs;
            proj_exprs.push_back(std::make_unique<ColumnRef>(0, colType(counter_id)));
            proj_exprs.push_back(std::make_unique<StrLenFunction>(std::make_unique<ColumnRef>(1, colType(url))));
            auto proj = std::make_unique<Projection>(std::move(filter), std::move(proj_exprs),
                                                     std::vector<std::string>{"CounterID", "url_len"});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(counter_id)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::AVG, std::make_unique<ColumnRef>(1, TypesId::int64));
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(proj), std::move(keys), std::move(agg_specs));
            auto having_pred = std::make_unique<Comparison>(CmpOp::GT,
                                                            std::make_unique<ColumnRef>(2, TypesId::int64),
                                                            std::make_unique<Constant>(int64_t(100000)));
            auto having = std::make_unique<Filter>(std::move(group), std::move(having_pred));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(having), std::move(top_keys), 25);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 28: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 29: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(referer)});
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(0, colType(referer)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            auto regexp_expr = std::make_unique<RegexpReplaceFunction>(std::make_unique<ColumnRef>(0, colType(referer)),
                                                                       "",
                                                                       "\\1");
            auto len_expr = std::make_unique<StrLenFunction>(std::make_unique<ColumnRef>(0, colType(referer)));
            std::vector<std::unique_ptr<Expression> > proj_exprs;
            proj_exprs.push_back(std::move(regexp_expr));
            proj_exprs.push_back(std::move(len_expr));
            proj_exprs.push_back(std::make_unique<ColumnRef>(0, colType(referer)));
            auto proj = std::make_unique<Projection>(std::move(filter), std::move(proj_exprs),
                                                     std::vector<std::string>{"k", "len", "Referer"});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, TypesId::string));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::AVG, std::make_unique<ColumnRef>(1, TypesId::int64));
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            agg_specs.emplace_back(GroupAggregate::AggOp::MIN, std::make_unique<ColumnRef>(2, colType(referer)));
            auto group = std::make_unique<GroupAggregate>(std::move(proj), std::move(keys), std::move(agg_specs));
            auto having_pred = std::make_unique<Comparison>(CmpOp::GT,
                                                            std::make_unique<ColumnRef>(2, TypesId::int64),
                                                            std::make_unique<Constant>(int64_t(100000)));
            auto having = std::make_unique<Filter>(std::move(group), std::move(having_pred));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(having), std::move(top_keys), 25);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 29: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 30: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(res_width)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::SUM, std::make_unique<ColumnRef>(0, colType(res_width)));
            for (int i = 1; i <= 89; ++i) {
                auto add_expr = std::make_unique<BinaryArithmetic>(ArithOp::PLUS,
                                                                   std::make_unique<ColumnRef>(0, colType(res_width)),
                                                                   std::make_unique<Constant>(int64_t(i)));
                specs.emplace_back(GlobalAggregate::AggOp::SUM, std::move(add_expr));
            }
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 30: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 31: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(search_eng), size_t(client_ip), size_t(is_refresh),
                                                        size_t(res_width), size_t(search_phr)
                                                    });
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(4, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(search_eng)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(client_ip)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            agg_specs.emplace_back(GroupAggregate::AggOp::SUM, std::make_unique<ColumnRef>(2, colType(is_refresh)));
            agg_specs.emplace_back(GroupAggregate::AggOp::AVG, std::make_unique<ColumnRef>(3, colType(res_width)));
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 31: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 32: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(watch_id), size_t(client_ip), size_t(is_refresh),
                                                        size_t(res_width), size_t(search_phr)
                                                    });
            auto pred = std::make_unique<Comparison>(CmpOp::NE,
                                                     std::make_unique<ColumnRef>(4, colType(search_phr)),
                                                     std::make_unique<Constant>(std::string("")));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(watch_id)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(client_ip)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            agg_specs.emplace_back(GroupAggregate::AggOp::SUM, std::make_unique<ColumnRef>(2, colType(is_refresh)));
            agg_specs.emplace_back(GroupAggregate::AggOp::AVG, std::make_unique<ColumnRef>(3, colType(res_width)));
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 32: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 33: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(watch_id), size_t(client_ip), size_t(is_refresh),
                                                        size_t(res_width)
                                                    });
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(watch_id)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(client_ip)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            agg_specs.emplace_back(GroupAggregate::AggOp::SUM, std::make_unique<ColumnRef>(2, colType(is_refresh)));
            agg_specs.emplace_back(GroupAggregate::AggOp::AVG, std::make_unique<ColumnRef>(3, colType(res_width)));
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 33: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 34: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(url)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(url)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, outputPath);

            auto end = Clock::now();
            std::cerr << "Query 34: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 35: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(url)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<Constant>(int64_t(1)));
            keys.push_back(std::make_unique<ColumnRef>(0, colType(url)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) {
                res->schema.columns[0].name = "1";
                res->schema.columns[1].name = "URL";
                res->schema.columns[2].name = "c";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 35: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 36: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(client_ip)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(client_ip)));
            keys.push_back(std::make_unique<BinaryArithmetic>(ArithOp::MINUS,
                                                              std::make_unique<ColumnRef>(0, colType(client_ip)),
                                                              std::make_unique<Constant>(int64_t(1))));
            keys.push_back(std::make_unique<BinaryArithmetic>(ArithOp::MINUS,
                                                              std::make_unique<ColumnRef>(0, colType(client_ip)),
                                                              std::make_unique<Constant>(int64_t(2))));
            keys.push_back(std::make_unique<BinaryArithmetic>(ArithOp::MINUS,
                                                              std::make_unique<ColumnRef>(0, colType(client_ip)),
                                                              std::make_unique<Constant>(int64_t(3))));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(4, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) {
                res->schema.columns[0].name = "ClientIP";
                res->schema.columns[1].name = "ClientIP-1";
                res->schema.columns[2].name = "ClientIP-2";
                res->schema.columns[3].name = "ClientIP-3";
                res->schema.columns[4].name = "c";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 36: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 37: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(url), size_t(counter_id), size_t(event_date),
                                                        size_t(dont_count_hits), size_t(is_refresh)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(1, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-01")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-31")));
            auto cond_dontcount = std::make_unique<Comparison>(CmpOp::EQ,
                                                               std::make_unique<ColumnRef>(3, colType(dont_count_hits)),
                                                               std::make_unique<Constant>(int64_t(0)));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(4, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));
            auto cond_url = std::make_unique<Comparison>(CmpOp::NE,
                                                         std::make_unique<ColumnRef>(0, colType(url)),
                                                         std::make_unique<Constant>(std::string("")));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_dontcount));
            and_ops.push_back(std::move(cond_refresh));
            and_ops.push_back(std::move(cond_url));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(url)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) {
                res->schema.columns[1].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 37: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 38: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(title), size_t(counter_id), size_t(event_date),
                                                        size_t(dont_count_hits), size_t(is_refresh)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(1, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-01")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-31")));
            auto cond_dontcount = std::make_unique<Comparison>(CmpOp::EQ,
                                                               std::make_unique<ColumnRef>(3, colType(dont_count_hits)),
                                                               std::make_unique<Constant>(int64_t(0)));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(4, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));
            auto cond_title = std::make_unique<Comparison>(CmpOp::NE,
                                                           std::make_unique<ColumnRef>(0, colType(title)),
                                                           std::make_unique<Constant>(std::string("")));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_dontcount));
            and_ops.push_back(std::move(cond_refresh));
            and_ops.push_back(std::move(cond_title));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(title)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false);
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) {
                res->schema.columns[1].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 38: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 39: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(url), size_t(counter_id), size_t(event_date),
                                                        size_t(is_refresh), size_t(is_link), size_t(is_download)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(1, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-01")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-31")));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(3, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));
            auto cond_islink = std::make_unique<Comparison>(CmpOp::NE,
                                                            std::make_unique<ColumnRef>(4, colType(is_link)),
                                                            std::make_unique<Constant>(int64_t(0)));
            auto cond_download = std::make_unique<Comparison>(CmpOp::EQ,
                                                              std::make_unique<ColumnRef>(5, colType(is_download)),
                                                              std::make_unique<Constant>(int64_t(0)));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_refresh));
            and_ops.push_back(std::move(cond_islink));
            and_ops.push_back(std::move(cond_download));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(url)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{1, false});
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 1000);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[1].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 39: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 40: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(trafic_source), size_t(search_eng), size_t(adv_engine),
                                                        size_t(referer), size_t(url),
                                                        size_t(counter_id), size_t(event_date), size_t(is_refresh)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(5, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(6, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-01")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(6, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-31")));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(7, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_refresh));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > case_ops;
            case_ops.push_back(std::make_unique<Comparison>(CmpOp::EQ,
                                                            std::make_unique<ColumnRef>(1, colType(search_eng)),
                                                            std::make_unique<Constant>(int64_t(0))));
            case_ops.push_back(std::make_unique<Comparison>(CmpOp::EQ,
                                                            std::make_unique<ColumnRef>(2, colType(adv_engine)),
                                                            std::make_unique<Constant>(int64_t(0))));
            auto case_condition = std::make_unique<LogicalAnd>(std::move(case_ops));

            auto case_expr = std::make_unique<IfFunction>(
                std::move(case_condition),
                std::make_unique<ColumnRef>(3, colType(referer)),
                std::make_unique<Constant>(std::string(""))
            );

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(trafic_source)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(search_eng)));
            keys.push_back(std::make_unique<ColumnRef>(2, colType(adv_engine)));
            keys.push_back(std::move(case_expr));
            keys.push_back(std::make_unique<ColumnRef>(4, colType(url)));

            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{5, false});
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[5].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 40: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 41: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(url_hash), size_t(event_date),
                                                        size_t(counter_id), size_t(is_refresh),
                                                        size_t(trafic_source), size_t(referer_hash)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(2, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(1, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-01")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(1, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-31")));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(3, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));
            std::vector<std::unique_ptr<Expression> > in_ops;
            in_ops.push_back(std::make_unique<Comparison>(CmpOp::EQ,
                                                          std::make_unique<ColumnRef>(4, colType(trafic_source)),
                                                          std::make_unique<Constant>(int64_t(-1))));
            in_ops.push_back(std::make_unique<Comparison>(CmpOp::EQ,
                                                          std::make_unique<ColumnRef>(4, colType(trafic_source)),
                                                          std::make_unique<Constant>(int64_t(6))));
            auto cond_in = std::make_unique<LogicalOr>(std::move(in_ops));

            auto cond_hash = std::make_unique<Comparison>(CmpOp::EQ,
                                                          std::make_unique<ColumnRef>(5, colType(referer_hash)),
                                                          std::make_unique<Constant>(int64_t(3594120000172545465LL)));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_refresh));
            and_ops.push_back(std::move(cond_in));
            and_ops.push_back(std::move(cond_hash));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(url_hash)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(event_date)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{2, false});
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[2].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 41: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 42: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(window_client_w), size_t(window_client_h),
                                                        size_t(counter_id), size_t(event_date),
                                                        size_t(is_refresh), size_t(dont_count_hits), size_t(url_hash)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(2, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(3, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-01")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(3, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-31")));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(4, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));
            auto cond_dontcount = std::make_unique<Comparison>(CmpOp::EQ,
                                                               std::make_unique<ColumnRef>(5, colType(dont_count_hits)),
                                                               std::make_unique<Constant>(int64_t(0)));
            auto cond_hash = std::make_unique<Comparison>(CmpOp::EQ,
                                                          std::make_unique<ColumnRef>(6, colType(url_hash)),
                                                          std::make_unique<Constant>(int64_t(2868770270353813622LL)));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_refresh));
            and_ops.push_back(std::move(cond_dontcount));
            and_ops.push_back(std::move(cond_hash));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(window_client_w)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(window_client_h)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{2, false});
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[2].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 42: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        case 43: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{
                                                        size_t(event_time), size_t(counter_id), size_t(event_date),
                                                        size_t(is_refresh), size_t(dont_count_hits)
                                                    });

            auto cond_counter = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(1, colType(counter_id)),
                                                             std::make_unique<Constant>(int64_t(62)));
            auto cond_date_ge = std::make_unique<Comparison>(CmpOp::GE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-14")));
            auto cond_date_le = std::make_unique<Comparison>(CmpOp::LE,
                                                             std::make_unique<ColumnRef>(2, colType(event_date)),
                                                             std::make_unique<Constant>(std::string("2013-07-15")));
            auto cond_refresh = std::make_unique<Comparison>(CmpOp::EQ,
                                                             std::make_unique<ColumnRef>(3, colType(is_refresh)),
                                                             std::make_unique<Constant>(int64_t(0)));
            auto cond_dontcount = std::make_unique<Comparison>(CmpOp::EQ,
                                                               std::make_unique<ColumnRef>(4, colType(dont_count_hits)),
                                                               std::make_unique<Constant>(int64_t(0)));

            std::vector<std::unique_ptr<Expression> > and_ops;
            and_ops.push_back(std::move(cond_counter));
            and_ops.push_back(std::move(cond_date_ge));
            and_ops.push_back(std::move(cond_date_le));
            and_ops.push_back(std::move(cond_refresh));
            and_ops.push_back(std::move(cond_dontcount));
            auto filter = std::make_unique<Filter>(std::move(scan), std::make_unique<LogicalAnd>(std::move(and_ops)));

            std::vector<std::unique_ptr<Expression> > proj_exprs;
            proj_exprs.push_back(std::make_unique<DateTruncMinuteFunction>(
                std::make_unique<ColumnRef>(0, colType(event_time))));
            std::vector<std::string> aliases = {"M"};
            auto proj = std::make_unique<Projection>(std::move(filter), std::move(proj_exprs), std::move(aliases));

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, TypesId::int64));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(proj), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{0, true});
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                auto col_m = res->columns[0];
                auto new_col = Column::Create(TypesId::string);
                for (size_t i = 0; i < res->numRows; ++i) {
                    int64_t micros = std::stoll(col_m->GetAsString(i));
                    std::string formatted = ParseMicrosToTimestamp(micros);
                    new_col->PushFromString(formatted);
                }
                res->columns[0] = std::shared_ptr<Column>(new_col.release());
                res->schema.columns[0].typeId = TypesId::string;

                res->schema.columns[1].name = "PageViews";
                WriteBatchToCSV(*res, outputPath);
            }

            auto end = Clock::now();
            std::cerr << "Query 43: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
            break;
        }

        default:
            std::cerr << "Unknown query number: " << queryNum << "\n";
            break;
    }
}

int main(int argc, char **argv) {
    std::string inputCol = getArg(argc, argv, "--input");
    std::string schemaPath = getArg(argc, argv, "--schema");
    std::string outputDir = getArg(argc, argv, "--output_dir");
    std::string queriesStr = getArg(argc, argv, "--queries");

    if (inputCol.empty() || schemaPath.empty() ||
        outputDir.empty() || queriesStr.empty()) {
        std::cerr << "Usage: ngn-clickbench-run"
                << " --input <hits.col>"
                << " --schema <hits.schema>"
                << " --output_dir <dir>"
                << " --queries <N>\n";
        return 1;
    }

    std::filesystem::create_directories(outputDir);

    ColumnarReader reader(inputCol);

    const int queryNum = std::stoi(queriesStr);

    RunQuery(queryNum, reader, outputDir);
    return 0;
}
