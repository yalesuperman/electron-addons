{
  "targets": [
    {
      "target_name": "analyseMp4",
      "sources": ["src/analyse-mp4-h264/index.cpp"],
      "include_dirs": ["/usr/local/include"],
      "libraries": ["-L/usr/local/lib",  "-lavformat", "-lavutil", "-lavcodec",]
    },
    {
      "target_name": "editMedia",
      "sources": ["src/edit-media/index.cpp"],
      "include_dirs": ["/usr/local/include"],
      "libraries": ["-L/usr/local/lib",  "-lavformat", "-lavutil", "-lavcodec",]
    },
  ]
}