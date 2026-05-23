#include "src/structs/schema.h"
#include "src/csv_io/csv_reader.h"
#include "src/structs/columnar_format.h"

#include <iostream>
#include <string>

static const size_t BATCH_SIZE = 65536;

static void CsvToColumnar(const std::string &schemaPath,
                           const std::string &dataPath,
                           const std::string &outputPath)
{
    Schema schema = Schema::LoadFromCSV(schemaPath);
    std::cerr << "Loaded schema: " << schema.NumColumns() << " columns\n";

    CsvReader reader(dataPath);
    ColumnarWriter writer(outputPath, schema);

    size_t totalRows = 0, rgCount = 0;

    while (true) {
        RowGroup rg;
        for (auto &cd : schema.columns)
            rg.columns.push_back(ColumnPtr(Column::Create(cd.typeId).release()));

        bool eof = false;
        for (size_t i = 0; i < BATCH_SIZE; i++) {
            auto row = reader.NextRow();
            if (!row) { eof = true; break; }

            auto &fields = *row;
            while (fields.size() < schema.NumColumns())
                fields.push_back("");

            for (size_t c = 0; c < schema.NumColumns(); c++) {
                std::string &field = fields[c];
                if (field.empty() && schema.columns[c].typeId != TypesId::string)
                    field = "0";
                rg.columns[c]->PushFromString(field);
            }
            rg.numRows++;
        }

        if (rg.numRows > 0) {
            writer.WriteRowGroup(rg);
            totalRows += rg.numRows;
            rgCount++;
        }
        if (eof) break;
    }

    writer.Finish();
    std::cerr << "Done: " << totalRows << " rows, "
              << rgCount << " row groups\n";
}

// Парсинг аргументов: --input X --schema Y --output Z
static std::string getArg(int argc, char **argv, const std::string &key)
{
    for (int i = 1; i < argc - 1; ++i)
        if (std::string(argv[i]) == key)
            return argv[i + 1];
    return "";
}

int main(int argc, char **argv)
{
    std::string input  = getArg(argc, argv, "--input");
    std::string schema = getArg(argc, argv, "--schema");
    std::string output = getArg(argc, argv, "--output");

    if (input.empty() || schema.empty() || output.empty()) {
        std::cerr << "Usage: csv_to_columnar"
                  << " --input <hits.csv>"
                  << " --schema <hits.schema>"
                  << " --output <hits.col>\n";
        return 1;
    }

    CsvToColumnar(schema, input, output);
    return 0;
}
