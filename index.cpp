#include <node.h>
#include <string>
#include "analyse-mp4/analyse-mp4.cpp"
using namespace v8;
using namespace std;

char* ConvertToLocalChar(v8::Isolate* isolate, v8::Local<v8::Value> value);

void analyseMp4(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate *isolate = args.GetIsolate();


  char *args1 = ConvertToLocalChar(isolate, args[0]);
  AnalyseMp4 analyseInstance = AnalyseMp4(args1);
  cout << args1 << " args1" << endl;

  analyseInstance.analyse();
  Local<Value> result = String::NewFromUtf8(isolate, analyseInstance.getAnalyseData().c_str()).ToLocalChecked();

  args.GetReturnValue().Set(result);
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "analyseMp4", analyseMp4);
}

char* ConvertToLocalChar(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (!value->IsString()) {
    // Handle error: the value is not a string.
    return nullptr;
  }

  v8::Local<v8::String> str = value.As<v8::String>();
  v8::String::Utf8Value utf8Value(isolate, str);
  
  // strdup will allocate memory that needs to be free'd later.
  return strdup(*utf8Value);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize);