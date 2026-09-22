// Called from the main sketch; no extra web server or dashboard framework.
void radioDownload(const String &path,const char *type) {
  if(!g_sdReady) {server.send(503,"text/plain","SD unavailable");return;}
  File f=SD.open(path,FILE_READ);
  if(!f) {server.send(404,"text/plain","No capture or log yet");return;}
  server.sendHeader("Content-Disposition","attachment; filename="+path.substring(path.lastIndexOf('/')+1));
  server.streamFile(f,type);
}
void registerRadioRoutes() {
  server.on("/api/radio",HTTP_GET,[](){server.send(200,"application/json",radio.json());});
  server.on("/api/radio/devices",HTTP_GET,[](){server.send(200,"application/json",radio.rowsJson());});
  server.on("/api/radio",HTTP_POST,[](){
    bool ok=radio.configure(server.arg("mode"),server.arg("ble")=="1",server.arg("capture")=="1",
      server.arg("watch"),server.arg("target"),server.arg("channel").toInt());
    server.send(ok?200:400,"application/json",ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"Invalid settings or storage failure\"}");
  });
  server.on("/api/radio/log",[](){radioDownload(radio.logPath(),"application/x-ndjson");});
  server.on("/api/radio/pcap",[](){radioDownload(radio.pcapPath(),"application/vnd.tcpdump.pcap");});
  server.on("/api/radio/ble",[](){radioDownload(radio.blePath(),"application/x-ndjson");});
  server.on("/api/radio/clear",HTTP_POST,[](){radio.clearLive();server.send(200,"application/json","{\"ok\":true}");});
  server.on("/api/radio/files",[](){
    String json="[";int count=0;
    if(g_sdReady) {
      File dir=SD.open("/radio");
      for(File f=dir.openNextFile();f && count<100;f=dir.openNextFile()) {
        if(f.isDirectory())continue;
        if(count++)json+=',';
        String name=f.name();name=name.substring(name.lastIndexOf('/')+1);
        json+="{\"name\":"+jsonQuote(name)+",\"size\":"+String(f.size())+"}";
      }
    }
    server.send(200,"application/json",json+"]");
  });
  server.on("/api/radio/file",[](){
    String name=server.arg("name");
    if(name.indexOf('/')>=0 || name.indexOf('\\')>=0 || name.indexOf("..")>=0 ||
       (!name.endsWith(".jsonl") && !name.endsWith(".pcap"))) {server.send(400,"text/plain","Invalid name");return;}
    radioDownload("/radio/"+name,name.endsWith(".pcap")?"application/vnd.tcpdump.pcap":"application/x-ndjson");
  });
}
void serviceRadioSerial() {
  static String command,output;
  static size_t position=0;
  if(position<output.length()) {
    size_t remaining=output.length()-position;
    size_t n=min(remaining,size_t(max(0,Serial.availableForWrite())));
    if(n)position+=Serial.write((const uint8_t*)output.c_str()+position,n);
    return;
  }
  output="";position=0;
  for(int i=0;i<64 && Serial.available();++i) {
    char c=Serial.read();
    if(c=='\r')continue;
    if(c=='\n') {
      if(command=="CMD:STATUS")output=radio.json()+"\n";
      else if(command=="CMD:HEALTH")output=statusJson()+"\n";
      else if(command=="CMD:VERSION")output="{\"firmware\":\"Flock Noir\",\"version\":\"" FLOCK_NOIR_VERSION "\"}\n";
      else if(command=="CMD:DUMP_LIVE")output=radio.rowsJson()+"\n";
      else if(command=="CMD:CLEAR_LIVE") {radio.clearLive();output="{\"ok\":true}\n";}
      else output="{\"error\":\"Unknown command; hold BOOT 1.5s for dashboard\"}\n";
      command="";break;
    }
    if(command.length()<80)command+=c;else command="";
  }
}
