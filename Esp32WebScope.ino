#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Ticker.h>
#include <driver/i2s.h>
#include "driver/dac.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ADC_CHANNEL   ADC1_CHANNEL_7     // GPIO36
#define NUM_SAMPLES   1024             // number of samples in  buffer
#define samplePerCycle 64
double DRAM_ATTR freq = 5.0;
double freq_old = 5.0;
double uMaxValue = 3.3;//峰峰值
double offSetValue = 1.65;//偏置电压
int duty = 50;//占空比%
int waveFlag =1;//波形种类
int scalTimes = 8;
int waveIndex = 0;//波形发送数据标志位
int sampleRate = 8000;
int sampleRate_old = 8000;
uint8_t waveTab[samplePerCycle]={0};  //储存波形数据
int waveTab1[samplePerCycle] = {0};
float adcBuff[NUM_SAMPLES]={0};
uint16_t i2s_read_buff[NUM_SAMPLES*2];
const int capacity = JSON_ARRAY_SIZE(1) + 128*JSON_OBJECT_SIZE(1);
DynamicJsonDocument adcJson(capacity);
DynamicJsonDocument root(256);
//websocket
const char* ssid = "CMCC4109"; //Enter SSID
const char* password = "31393139"; //Enter Password

hw_timer_t * timerDac = NULL;            //声明一个定时器
Ticker timer1;
WebServer server;
WebSocketsServer webSocket = WebSocketsServer(81);
char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="zh-CN">
<head> 
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>波形发生器</title>
  <style>
    *{margin: 0;padding: 0;}
  </style>
</head>
<body onload="javascript:init()">
  <div>
    <p>coming soon~</p>
  </div>  
</body>
<script>
  var webSocket;
  function init(){
    webSocket = new WebSocket('ws://'+window.location.hostname +':81/');
    webSocket.onmessage = function(event){
    var data = JSON.parse(event.data);
    console.log(data);
  }
}
</script>
</html>
)=====";

void InitI2S (){
    i2s_config_t i2s_config = {
        //I2S with ADC 
        .mode = (i2s_mode_t) (I2S_MODE_MASTER
                            | I2S_MODE_RX
                            | I2S_MODE_ADC_BUILT_IN),
        .sample_rate            = (8000),                                                      
        .bits_per_sample        = I2S_BITS_PER_SAMPLE_16BIT,                            
        .channel_format         = I2S_CHANNEL_FMT_ONLY_RIGHT,                                
        .communication_format   = I2S_COMM_FORMAT_I2S_MSB,                            
        .intr_alloc_flags       = 0,                                                    
        .dma_buf_count          = 4,                                                          
        .dma_buf_len            = NUM_SAMPLES,                                                  
        .use_apll               = false,                                                        
    };
 
    adc1_config_channel_atten (ADC_CHANNEL, ADC_ATTEN_11db);
    adc1_config_width (ADC_WIDTH_12Bit);
    i2s_driver_install (I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_adc_mode (ADC_UNIT_1, ADC_CHANNEL);
    i2s_adc_enable (I2S_NUM_0);
    
}
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  //websocket 事件相应函数
  if(type==WStype_TEXT){
    deserializeJson(root, payload);
    freq = root["freq"];
    waveFlag = root["waveType"];
    duty = root["duty"];
    uMaxValue = root["uMaxValue"];
    offSetValue = root["biasVoltage"];
    scalTimes = root["scaleTimes"];
    sampleRate = root["sampleRate"];
    wave_gen(waveFlag);
    Serial.printf("get text:%f %d %d %f %f %d\n",freq, waveFlag,duty,uMaxValue,offSetValue,scalTimes);
    Serial.printf("%f\n",(1000/freq)/64);
    if(freq != freq_old){
      //timer.detach();
      //timer.attach_ms((1000/freq)/64,dacGenWave);
      updateTimer();
      freq_old = freq;
    }
    if(sampleRate != sampleRate_old ){
      i2s_set_sample_rates(I2S_NUM_0, sampleRate);
      sampleRate_old = sampleRate;
    }
    
  }
}

void IRAM_ATTR onTimer() {            //中断函数
  if(waveIndex>samplePerCycle){waveIndex = 0;}
  dac_output_voltage(DAC_CHANNEL_1,waveTab[waveIndex]);
  waveIndex++;
}
void timerDacInit(){
  dac_output_enable(DAC_CHANNEL_1);
  timerDac = timerBegin(0, 80, true);                //初始化
  timerAttachInterrupt(timerDac, &onTimer, true);    //调用中断函数
  Serial.println("here ok");
  timerAlarmWrite(timerDac, 1000000, true);        //timerBegin的参数二 80位80MHZ，这里为10  意思为10us
  Serial.println("here ok");
  timerAlarmEnable(timerDac);                //定时器使能
  Serial.println("here ok");
  //timerDetachInterrupt(timerDac);            //关闭定时器
}
void updateTimer()
{
  timerAlarmDisable(timerDac);              //先关闭定时器
  uint64_t dacTime = (1000000 / samplePerCycle) / freq; //波形周期,微秒
  /* *设置闹钟每秒调用onTimer函数1 tick为1us => 1秒为1000000us * /
  / *重复闹钟（第三个参数）*/
  timerAlarmWrite(timerDac, dacTime, true);
  /* 启动警报*/
  timerAlarmEnable(timerDac);
}
void InitWeb(){
  // Connect to wifi
  WiFi.begin(ssid, password);
  // Wait some time to connect to wifi
  for(int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++) {
      Serial.print(".");
      delay(1000);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   //You can get IP address assigned to ESP
  server.on("/",[](){
   server.send_P(200,"text/html",webpage); 
  });
  server.begin();
  //websockets next
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
void wave_gen(int index)
{
  
  if (index==1)
  {
    double sineValue = 0.0;
    for (int i = 0; i < samplePerCycle; i++)
    {
      sineValue = sin(((2*PI)/samplePerCycle)*i) * (uMaxValue/2) + offSetValue ;
      waveTab1[i] = (((int)(sineValue*255/3.3)));
    }
    Serial.printf("波形表重设成功，当前为正弦波\n");
  }
  else if(index==2)
  {
    float x = samplePerCycle * ((float)duty / 100.0);
    int x1 = (int)x;
    for (int i = 0; i < samplePerCycle; i++)
    {
      if ( i < x)
      {
        waveTab1[i] = (int)(255 * (uMaxValue/2 + offSetValue)/3.3)  ;
      }
      else{
        waveTab1[i] = (int)(255 * (-(uMaxValue/2) + offSetValue)/3.3);
      }
    }
    Serial.printf("波形表重设成功，当前为方波,占空比:%d\n",duty);
  }
  else if (index==3)//锯齿波
  {
    for (int i = 0; i < samplePerCycle; i++)
    {
        float x = (-samplePerCycle/2+i)/(samplePerCycle/2);
        waveTab1[i] = (int)(255*(x*(uMaxValue/2) + offSetValue)/3.3);
    }
    Serial.println("波形表重设成功，当前为锯齿波");
  }
  for (int i = 0; i < samplePerCycle; i++){
    if(waveTab1[i]>255){
      waveTab1[i]=255;
    }
    if(waveTab1[i]<0){
      waveTab1[i]=0;
    }
    waveTab[i]= (uint8_t) waveTab1[i];
  }
}
int GetAdcData(float* po_AdcValues)
{
    //uint16_t *i2s_read_buff = malloc(sizeof(uint16_t) * NUM_SAMPLES*2);
    //uint16_t i2s_read_buff[NUM_SAMPLES*2];
    size_t num_bytes_read =0;
 
    i2s_read( I2S_NUM_0, (void*) i2s_read_buff,  NUM_SAMPLES * 2* sizeof (uint16_t), &num_bytes_read, portMAX_DELAY);
 
    int NumSamps =  num_bytes_read/(2*sizeof (uint16_t));
 
    for (int i=0; i< NumSamps; i++)
    {   //convert 12 bits to voltage
        po_AdcValues[i] = 3.3* ( (float) (i2s_read_buff[i*2]& 0x0FFF)) /0x0FFF;
    }
    //free(i2s_read_buff);
    return NumSamps; //return  number of the read samples
}
void adcTask(void *arg)
{
  GetAdcData(adcBuff);
  while (1)
  {
      GetAdcData(adcBuff);
  }
}
void dacGenWave()
{
  if(waveIndex>samplePerCycle){waveIndex = 0;}
  dacWrite(25,waveTab[waveIndex]);
  waveIndex++;
}
void getData(){
  digitalWrite(2,LOW);
  char p[1500];
  for(int i=0;i<128;i++){
    adcJson["adcBuff"][i] = adcBuff[i*scalTimes];
  }
  //const char* p = adcJson["adcBuff"];
  serializeJson(adcJson, p);
  webSocket.broadcastTXT(p,strlen(p));
  //free(p);
  digitalWrite(2,HIGH);
}
void setup()
{
  Serial.begin(115200);
  pinMode(2,OUTPUT);
  wave_gen(waveFlag);
  InitI2S();
  Serial.println("i2s init success");
  //timer.attach_ms(2,dacGenWave);
  timerDacInit();
  Serial.println("timerDac init success");
  xTaskCreatePinnedToCore(adcTask,"adcTask",4096*4,NULL,1,NULL,0);
  Serial.println("adc init success");
  InitWeb();
  Serial.println("web init success");
  timer1.attach_ms(100,getData);
  Serial.println("web init success1");
  digitalWrite(2,HIGH);

}

void loop()
{
  webSocket.loop();
  server.handleClient();
}
