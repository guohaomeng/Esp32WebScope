var chartDom = document.getElementById('main');
var myChart = echarts.init(chartDom);
var option;
var start = true;

function startWave(){
    start=true;
}
function stopWave(){
    start=false;
}
function generateData() {
  let data = [];
  for (let i = 0; i <= 127; i ++) {
      data.push([i, dataAdc.adcBuff[i]]);
  }
  return data;
}
function refreshData(){
    //刷新数据
    if(start===true){
        var option1 = myChart.getOption();
        option1.series[0].data = generateData();
        option1.xAxis[0].name = time_div2;
        myChart.setOption(option1); 
    }
}
option = {
  animation: false,
  grid: {
      top: 40,
      left: 50,
      right: 80,
      bottom: 50
  },
  tooltip: {
    trigger : 'axis'
  },
  xAxis: {
      name: '3.65ms/Div',
      min: 0,
      max: 128,
      type: 'value',
      splitLine: {
          show: true
      },
      minorTick: {
          show: true
      },
      minorSplitLine: {
          show: true
      }
  },
  yAxis: {
      name: '电压/V',
      min: -1,
      max: 4,
      minorTick: {
          show: true
      },
      minorSplitLine: {
          show: true
      }
  },
  dataZoom: [{
      show: true,
      type: 'inside',
      filterMode: 'none',
      xAxisIndex: [0],
      startValue: -20,
      endValue: 20
  }, {
      show: true,
      type: 'inside',
      filterMode: 'none',
      yAxisIndex: [0],
      startValue: 0,
      endValue: 100,
      minSpan: 100
  }],
  series: [
      {
          type: 'line',
          showSymbol: false, //是否显示每个点
          clip: true,
          data: generateData()
      }
  ]
};

myChart.setOption(option);
init();