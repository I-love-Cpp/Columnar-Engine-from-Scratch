#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "ReaderAndWriter/ColumnReader.h"
#include "ReaderAndWriter/ColumnWriter.h"
#include "ReaderAndWriter/Utils.h"

void CreateTestData() {
    std::ofstream s("schema.csv");
    s << "id,int64\nname,string\nage,int64\ncomment,string";
    s.close();
    std::ofstream d("data.csv");
    for (int i = 0; i < 250; i++) {
        d << i << ",User" << i << "," << (20 + i % 50) << "," << "\"Some comment " << i << "\"" << "\n";
    }
    d.close();
}

int main() {
    try {
        CreateTestData();
        ColumnWriter writer("output.columnar", "schema.csv", 4);
        std::ifstream csv("data.csv");
        std::string line;
        int row_num = 0;
        while (std::getline(csv, line)) {
            if (line.empty()) continue;
            writer.AddRow(SplitCSV(line));
            row_num++;
        }
        writer.Close();
        ColumnarReader reader("output.columnar");
        reader.ToCSV("restored.csv");
    } catch (const std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
