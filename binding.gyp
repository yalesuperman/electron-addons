{
  "targets": [
    {
      "target_name": "addon",
      "sources": ["index.cpp"],
      "include_dirs": ["/usr/local/include"],
      "libraries": ["-L/usr/local/lib",  "-lavformat", "-lavutil", "-lavcodec",]
    }
  ]
}