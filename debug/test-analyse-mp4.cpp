// #include <nlohmann/json.hpp>
// #include <iostream>
// #include <iomanip>
#include "../analyse-mp4/analyse-mp4.cpp"


// using json = nlohmann::json;

int main(int argc, char * argv[])
{
  // json j;
  // j["p1"] = 123;
  // j["p2"] = { {"aaa", 11}, {"bbb", 222} };
  // // std::cout << std::setw(4) << json::meta() << std::endl;
  // std::cout << std::setw(4) << j << std::endl;
  if (argc < 2) {
    std::cout << "请输入需要打开的媒体路径" << std::endl;
    return -1;
  }

  std::cout << "argv[1]" << argv[1] << std::endl;  

  AnalyseMp4 test = AnalyseMp4(argv[1]);
  test.analyse();
  std::cout << test.getAnalyseData() << std::endl;
}