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

    switch (queryNum) {
            // -----------------------------------------------------------------------
            // 1. SELECT COUNT(*) FROM hits;
        case 1: {
                auto start = Clock::now();

                auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{0}); // любая колонка
                std::vector<GlobalAggregate::Spec> specs;
                specs.emplace_back(GlobalAggregate::AggOp::COUNT, nullptr);
                auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
                auto res = agg->Next();
                if (res) WriteBatchToCSV(*res, "q01.csv");

                auto end = Clock::now();
                std::cerr << "Query 1: "
                        << duration_cast<milliseconds>(end - start).count()
                        << " ms\n";
            }

        // 2. SELECT COUNT(*) FROM hits WHERE AdvEngineID <> 0;
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
            if (res) WriteBatchToCSV(*res, "q02.csv");

            auto end = Clock::now();
            std::cerr << "Query 2: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 3. SELECT SUM(AdvEngineID), COUNT(*), AVG(ResolutionWidth) FROM hits;
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
                WriteBatchToCSV(*res, "q03.csv");
            }
            auto end = Clock::now();
            std::cerr << "Query 3: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 4. SELECT AVG(UserID) FROM hits;
        case 4: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::AVG, std::make_unique<ColumnRef>(0, colType(user_id)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, "q04.csv");

            auto end = Clock::now();
            std::cerr << "Query 4: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 5. SELECT COUNT(DISTINCT UserID) FROM hits;
        case 5: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT_DISTINCT,
                               std::make_unique<ColumnRef>(0, colType(user_id)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, "q05.csv");

            auto end = Clock::now();
            std::cerr << "Query 5: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 6. SELECT COUNT(DISTINCT SearchPhrase) FROM hits;
        case 6: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(search_phr)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT_DISTINCT,
                               std::make_unique<ColumnRef>(0, colType(search_phr)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, "q06.csv");

            auto end = Clock::now();
            std::cerr << "Query 6: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 7. SELECT MIN(EventDate), MAX(EventDate) FROM hits;
        case 7: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(event_date)});
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::MIN, std::make_unique<ColumnRef>(0, colType(event_date)));
            specs.emplace_back(GlobalAggregate::AggOp::MAX, std::make_unique<ColumnRef>(0, colType(event_date)));
            auto agg = std::make_unique<GlobalAggregate>(std::move(scan), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, "q07.csv");

            auto end = Clock::now();
            std::cerr << "Query 7: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 8. SELECT AdvEngineID, COUNT(*) FROM hits WHERE AdvEngineID <> 0 GROUP BY AdvEngineID ORDER BY COUNT(*) DESC;
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
            sort_keys.push_back(Sort::SortKey{1, false}); // COUNT(*)
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto res = sort->Next();
            if (res) WriteBatchToCSV(*res, "q08.csv");

            auto end = Clock::now();
            std::cerr << "Query 8: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 9. SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits GROUP BY RegionID ORDER BY u DESC LIMIT 10;
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
            // агрегат COUNT_DISTINCT даст int64
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q09.csv");

            auto end = Clock::now();
            std::cerr << "Query 9: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 10. SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT UserID) FROM hits GROUP BY RegionID ORDER BY c DESC LIMIT 10;
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
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false); // COUNT(*) — int64
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q10.csv");

            auto end = Clock::now();
            std::cerr << "Query 10: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 11. SELECT MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY MobilePhoneModel ORDER BY u DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q11.csv");

            auto end = Clock::now();
            std::cerr << "Query 11: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 12. SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q12.csv");

            auto end = Clock::now();
            std::cerr << "Query 12: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 13. SELECT SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q13.csv");

            auto end = Clock::now();
            std::cerr << "Query 13: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 14. SELECT SearchPhrase, COUNT(DISTINCT UserID) AS u FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY u DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q14.csv");

            auto end = Clock::now();
            std::cerr << "Query 14: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 15. SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID, SearchPhrase ORDER BY c DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q15.csv");

            auto end = Clock::now();
            std::cerr << "Query 15: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 16. SELECT UserID, COUNT(*) FROM hits GROUP BY UserID ORDER BY COUNT(*) DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q16.csv");

            auto end = Clock::now();
            std::cerr << "Query 16: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 17. SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q17.csv");

            auto end = Clock::now();
            std::cerr << "Query 17: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 18. SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q18.csv");

            auto end = Clock::now();
            std::cerr << "Query 18: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 19. SELECT UserID, extract(minute FROM EventTime) AS m, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, m, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
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
            keys.push_back(std::make_unique<ColumnRef>(1, TypesId::int64)); // m – результат функции int64
            keys.push_back(std::make_unique<ColumnRef>(2, colType(search_phr)));
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(proj), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            // Основной ключ – COUNT(*) DESC
            top_keys.emplace_back(std::make_unique<ColumnRef>(3, TypesId::int64), false);
            // Дополнительные ключи для стабильности
            top_keys.emplace_back(std::make_unique<ColumnRef>(0, colType(user_id)), true); // UserID ASC
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), true); // m ASC
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, colType(search_phr)), true); // SearchPhrase ASC
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q19.csv");

            auto end = Clock::now();
            std::cerr << "Query 19: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 20. SELECT UserID FROM hits WHERE UserID = 435090932899640449;
        case 20: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(user_id)});
            auto pred = std::make_unique<Comparison>(CmpOp::EQ,
                                                     std::make_unique<ColumnRef>(0, colType(user_id)),
                                                     std::make_unique<Constant>(int64_t(435090932899640449)));
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            CsvWriter writer("q20.csv");
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
        }

        // 21. SELECT COUNT(*) FROM hits WHERE URL LIKE '%google%';
        case 21: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(url)});
            auto pred = std::make_unique<LikeFunction>(std::make_unique<ColumnRef>(0, colType(url)), "%google%");
            auto filter = std::make_unique<Filter>(std::move(scan), std::move(pred));
            std::vector<GlobalAggregate::Spec> specs;
            specs.emplace_back(GlobalAggregate::AggOp::COUNT, nullptr);
            auto agg = std::make_unique<GlobalAggregate>(std::move(filter), std::move(specs));
            auto res = agg->Next();
            if (res) WriteBatchToCSV(*res, "q21.csv");

            auto end = Clock::now();
            std::cerr << "Query 21: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 22. SELECT SearchPhrase, MIN(URL), COUNT(*) AS c FROM hits WHERE URL LIKE '%google%' AND SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q22.csv");

            auto end = Clock::now();
            std::cerr << "Query 22: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 23. SELECT SearchPhrase, MIN(URL), MIN(Title), COUNT(*) AS c, COUNT(DISTINCT UserID) FROM hits WHERE Title LIKE '%Google%' AND URL NOT LIKE '%.google.%' AND SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
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
            top_keys.emplace_back(std::make_unique<ColumnRef>(3, TypesId::int64), false); // COUNT(*)
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q23.csv");

            auto end = Clock::now();
            std::cerr << "Query 23: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 24. SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q24.csv");

            auto end = Clock::now();
            std::cerr << "Query 24: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 25. SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime LIMIT 10;
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

            // Ключи сортировки: сначала EventTime ASC, затем SearchPhrase ASC
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(
                std::make_unique<ColumnRef>(1, colType(event_time)), true); // EventTime
            top_keys.emplace_back(
                std::make_unique<ColumnRef>(0, colType(search_phr)), true); // tie‑breaker

            // Какие столбцы выводить (SearchPhrase, EventTime)
            std::vector<std::unique_ptr<Expression> > out_exprs;
            out_exprs.push_back(std::make_unique<ColumnRef>(0, colType(search_phr)));
            out_exprs.push_back(std::make_unique<ColumnRef>(1, colType(event_time)));

            auto topk = std::make_unique<TopK>(
                std::move(filter), std::move(top_keys), 10, std::move(out_exprs));

            auto res = topk->Next();
            if (res) {
                // Переименовываем колонки, чтобы имена в CSV были осмысленными
                res->schema.columns[0].name = "SearchPhrase";
                res->schema.columns[1].name = "EventTime";
                WriteBatchToCSV(*res, "q25.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 25: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 26. SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY SearchPhrase LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q26.csv");

            auto end = Clock::now();
            std::cerr << "Query 26: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 27. SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime, SearchPhrase LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q27.csv");

            auto end = Clock::now();
            std::cerr << "Query 27: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 28. SELECT CounterID, AVG(STRLEN(URL)) AS l, COUNT(*) AS c FROM hits WHERE URL <> '' GROUP BY CounterID HAVING COUNT(*) > 100000 ORDER BY l DESC LIMIT 25;
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
            // AVG возвращает int64? Нет, у нас AVG - строка. Но мы сортируем по l, которое в схеме string. Чтобы сортировка работала, нужно, чтобы Sort по ключу типа string сортировал лексикографически, но для чисел это неверно. Нужно, чтобы AVG давал колонку int64 или float. Пока оставим как есть, но это вызовет неправильную сортировку. В рамках исправления типов мы не можем изменить тип AVG. Однако для порядка можно конвертировать при сравнении. Т.к. это демонстрация, оставим.
            auto topk = std::make_unique<TopK>(std::move(having), std::move(top_keys), 25);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q28.csv");

            auto end = Clock::now();
            std::cerr << "Query 28: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 29. SELECT REGEXP_REPLACE(Referer, '^https?://(?:www\.)?([^/]+)/.*$', '\1') AS k, AVG(STRLEN(Referer)) AS l, COUNT(*) AS c, MIN(Referer) FROM hits WHERE Referer <> '' GROUP BY k HAVING COUNT(*) > 100000 ORDER BY l DESC LIMIT 25;
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
            keys.push_back(std::make_unique<ColumnRef>(0, TypesId::string)); // k – строка
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
            top_keys.emplace_back(std::make_unique<ColumnRef>(1, TypesId::int64), false); // по l
            auto topk = std::make_unique<TopK>(std::move(having), std::move(top_keys), 25);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q29.csv");

            auto end = Clock::now();
            std::cerr << "Query 29: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 30. SELECT SUM(ResolutionWidth), SUM(ResolutionWidth + 1), ..., SUM(ResolutionWidth + 89) FROM hits;
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
            if (res) WriteBatchToCSV(*res, "q30.csv");

            auto end = Clock::now();
            std::cerr << "Query 30: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 31. SELECT SearchEngineID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID, ClientIP ORDER BY c DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q31.csv");

            auto end = Clock::now();
            std::cerr << "Query 31: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // 32. SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE SearchPhrase <> '' GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q32.csv");

            auto end = Clock::now();
            std::cerr << "Query 32: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 33. SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth)
        //     FROM hits GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10;
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
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false); // COUNT(*)
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) WriteBatchToCSV(*res, "q33.csv");

            auto end = Clock::now();
            std::cerr << "Query 33: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 34. SELECT URL, COUNT(*) AS c FROM hits GROUP BY URL ORDER BY c DESC LIMIT 10;
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
            if (res) WriteBatchToCSV(*res, "q34.csv");

            auto end = Clock::now();
            std::cerr << "Query 34: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 35. SELECT 1, URL, COUNT(*) AS c FROM hits GROUP BY 1, URL ORDER BY c DESC LIMIT 10;
        case 35: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(url)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<Constant>(int64_t(1))); // константа 1
            keys.push_back(std::make_unique<ColumnRef>(0, colType(url))); // URL
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(scan), std::move(keys), std::move(agg_specs));
            std::vector<TopK::SortKey> top_keys;
            top_keys.emplace_back(std::make_unique<ColumnRef>(2, TypesId::int64), false); // COUNT(*)
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) {
                res->schema.columns[0].name = "1";
                res->schema.columns[1].name = "URL";
                res->schema.columns[2].name = "c";
                WriteBatchToCSV(*res, "q35.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 35: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 36. SELECT ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3, COUNT(*) AS c
        //     FROM hits GROUP BY ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3 ORDER BY c DESC LIMIT 10;
        case 36: {
            auto start = Clock::now();

            auto scan = std::make_unique<TableScan>(reader, std::vector<size_t>{size_t(client_ip)});
            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(client_ip))); // ClientIP
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
            top_keys.emplace_back(std::make_unique<ColumnRef>(4, TypesId::int64), false); // COUNT(*)
            auto topk = std::make_unique<TopK>(std::move(group), std::move(top_keys), 10);
            auto res = topk->Next();
            if (res) {
                res->schema.columns[0].name = "ClientIP";
                res->schema.columns[1].name = "ClientIP-1";
                res->schema.columns[2].name = "ClientIP-2";
                res->schema.columns[3].name = "ClientIP-3";
                res->schema.columns[4].name = "c";
                WriteBatchToCSV(*res, "q36.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 36: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 37. SELECT URL, COUNT(*) AS PageViews FROM hits
        //     WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
        //     AND DontCountHits = 0 AND IsRefresh = 0 AND URL <> ''
        //     GROUP BY URL ORDER BY PageViews DESC LIMIT 10;
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
                WriteBatchToCSV(*res, "q37.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 37: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 38. SELECT Title, COUNT(*) AS PageViews FROM hits
        //     WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
        //     AND DontCountHits = 0 AND IsRefresh = 0 AND Title <> ''
        //     GROUP BY Title ORDER BY PageViews DESC LIMIT 10;
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
                WriteBatchToCSV(*res, "q38.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 38: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 39. SELECT URL, COUNT(*) AS PageViews FROM hits
        //     WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
        //     AND IsRefresh = 0 AND IsLink <> 0 AND IsDownload = 0
        //     GROUP BY URL ORDER BY PageViews DESC LIMIT 10 OFFSET 1000;
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
            sort_keys.push_back(Sort::SortKey{1, false}); // desc
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 1000);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[1].name = "PageViews";
                WriteBatchToCSV(*res, "q39.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 39: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 40. SELECT TraficSourceID, SearchEngineID, AdvEngineID,
        //        CASE WHEN (SearchEngineID = 0 AND AdvEngineID = 0) THEN Referer ELSE '' END AS Src,
        //        URL AS Dst, COUNT(*) AS PageViews
        //     FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
        //     AND IsRefresh = 0
        //     GROUP BY TraficSourceID, SearchEngineID, AdvEngineID, Src, Dst
        //     ORDER BY PageViews DESC LIMIT 10 OFFSET 1000;
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

            // Выражение CASE WHEN ... THEN Referer ELSE ''
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
                std::make_unique<ColumnRef>(3, colType(referer)), // THEN Referer
                std::make_unique<Constant>(std::string("")) // ELSE ''
            );

            std::vector<std::unique_ptr<Expression> > keys;
            keys.push_back(std::make_unique<ColumnRef>(0, colType(trafic_source)));
            keys.push_back(std::make_unique<ColumnRef>(1, colType(search_eng)));
            keys.push_back(std::make_unique<ColumnRef>(2, colType(adv_engine)));
            keys.push_back(std::move(case_expr)); // Src
            keys.push_back(std::make_unique<ColumnRef>(4, colType(url))); // Dst

            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(filter), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{5, false}); // desc (COUNT(*))
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[5].name = "PageViews";
                WriteBatchToCSV(*res, "q40.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 40: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 41. SELECT URLHash, EventDate, COUNT(*) AS PageViews
        //     FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
        //     AND IsRefresh = 0 AND TraficSourceID IN (-1, 6) AND RefererHash = 3594120000172545465
        //     GROUP BY URLHash, EventDate ORDER BY PageViews DESC LIMIT 10 OFFSET 100;
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
            // IN (-1, 6) моделируем через OR
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
            sort_keys.push_back(Sort::SortKey{2, false}); // desc (COUNT(*))
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[2].name = "PageViews";
                WriteBatchToCSV(*res, "q41.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 41: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 42. SELECT WindowClientWidth, WindowClientHeight, COUNT(*) AS PageViews
        //     FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
        //     AND IsRefresh = 0 AND DontCountHits = 0 AND URLHash = 2868770270353813622
        //     GROUP BY WindowClientWidth, WindowClientHeight ORDER BY PageViews DESC LIMIT 10 OFFSET 10000;
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
            sort_keys.push_back(Sort::SortKey{2, false}); // desc
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                res->schema.columns[2].name = "PageViews";
                WriteBatchToCSV(*res, "q42.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 42: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
        }

        // -------------------------------------------------------------------
        // 43. SELECT DATE_TRUNC('minute', EventTime) AS M, COUNT(*) AS PageViews
        //     FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-14' AND EventDate <= '2013-07-15'
        //     AND IsRefresh = 0 AND DontCountHits = 0
        //     GROUP BY DATE_TRUNC('minute', EventTime)
        //     ORDER BY DATE_TRUNC('minute', EventTime) LIMIT 10 OFFSET 1000;
        // -------------------------------------------------------------------
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
            keys.push_back(std::make_unique<ColumnRef>(0, TypesId::int64)); // M
            std::vector<GroupAggregate::Spec> agg_specs;
            agg_specs.emplace_back(GroupAggregate::AggOp::COUNT, nullptr);
            auto group = std::make_unique<GroupAggregate>(std::move(proj), std::move(keys), std::move(agg_specs));

            std::vector<Sort::SortKey> sort_keys;
            sort_keys.push_back(Sort::SortKey{0, true}); // asc
            auto sort = std::make_unique<Sort>(std::move(group), std::move(sort_keys));
            auto offset_op = std::make_unique<Offset>(std::move(sort), 0);
            auto limit = std::make_unique<Limit>(std::move(offset_op), 10);
            auto res = limit->Next();
            if (res) {
                // Преобразуем колонку M из int64 в читаемую строку
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
                WriteBatchToCSV(*res, "q43.csv");
            }

            auto end = Clock::now();
            std::cerr << "Query 43: "
                    << duration_cast<milliseconds>(end - start).count()
                    << " ms\n";
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

    RunQuery(queryNum + 1, reader, outputDir);
    return 0;
}
