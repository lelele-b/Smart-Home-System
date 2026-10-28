// index.js
// 获取应用实例
const app = getApp()

Page({
  data: {
    //需要修改的地方
    uid:"YOUR_BEMFA_UID",//巴法云私钥 UID，必须与单片机 WiFi.h 里一致
    ledtopic:"devicectrl006",//控制led的主题
    dhttopic:"sensordata004",//传输温湿度的主题
 
    device_status:"离线",// 显示led是否在线的字符串，默认离

    dataTime:"", //记录数据上传的时间

    client: null,//mqtt客户端，默认为空
    // ===================== 核心：图片状态配置 =====================
    imgGroup: [
        // 第0个图片：3状态
        {
            currentIndex: 0,
            stateList: [
              { src: "/utils/img/lightoff.png", msg: "led:0", text: "LED关" },
              { src: "/utils/img/lighton.png", msg: "led:1", text: "LED开" }
            ]
          },
          // BEEP 组：每个状态加上 text
          {
            currentIndex: 0,
            stateList: [
              { src: "/utils/img/jingbao0.png", msg: "beep:0", text: "正常" },
              { src: "/utils/img/jingbao1.png", msg: "beep:1", text: "报警" }
            ]
          },

          // RGB 组：每个状态加上 text
          {
            currentIndex: 0,
            stateList: [
              { src: "/utils/img/rgb_off.png", msg: "rgb:0", text: "关" },
              { src: "/utils/img/rgb_r.png", msg: "rgb:1", text: "红" },
              { src: "/utils/img/rgb_g.png", msg: "rgb:2", text: "绿" },
              { src: "/utils/img/rgb_b.png", msg: "rgb:3", text: "蓝" }
            ]
          },

          // RELAY 组：每个状态加上 text
          {
            currentIndex: 0,
            stateList: [
              { src: "/utils/img/relayoff.png", msg: "relay:0", text: "关" },
              { src: "/utils/img/relayon.png", msg: "relay:1", text: "开" }
            ]
          }
        ]

  },

//屏幕打开时执行的函数
  onLoad() {
    //检查设备是否在线
    this.getOnline()
    //检查设备是打开还是关闭
    this.getOnOff()
    //获取服务器上现在存储的dht11数据
    this.getdht11()

    //设置定时器，每3秒请求一下设备状态
    setInterval(() => this.getdht11(), 3000)

    //每 5 秒刷新一次在线状态（设备订阅的是 devicectrl006）
    setInterval(() => this.getOnline(), 5000)
   

  },
  //控制灯的函数1，小滑块点击后执行的函数
  onChange({ detail }){
    //detail是滑块的值，检查是打开还是关闭，并更换正确图标
    this.setData({ 
      checked: detail,
     });
     if(detail == true){//如果是打开操作
      this.LedSendMsg("1") //发送打开指令
      this.setData({ 
        ledicon: "/utils/img/jingbao1.png",//设置led图片为on
       });
     }else{
      this.LedSendMsg("0") //发送关闭指令
      this.setData({ 
        ledicon: "/utils/img/jingbao0.png",//设置led图片为off
       });
     }
  },
  //点击led图片执行的函数
  onChange2(){
    var that = this
      //如果点击前是打开状态，现在更换为关闭状态，并更换图标，完成状态切换
      if( that.data.checked == true){
        that.LedSendMsg("0")//发送关闭指令
        this.setData({ 
            ledicon: "/utils/img/lightoff.png",//设置led图片为off
            checked:false //设置led状态为false
         });
      }else{
        //如果点击前是关闭状态，现在更换为打开状态，并更换图标，完成状态切换
        that.LedSendMsg("1")//发送打开指令
        that.setData({ 
          ledicon: "/utils/img/lighton.png",//设置led图片为on
          checked:true//设置led状态为true
       });
      }
  },
  //请求设备状态,检查设备是否在线
  getOnline(){
    var that = this
     //api 接口详细说明见巴法云接入文档
    wx.request({
      url: 'https://api.bemfa.com/api/device/v1/status/?', //状态api接口，详见巴法云接入文档
      data: {
        uid: that.data.uid,
        topic: that.data.ledtopic,//查控制主题的在线状态（设备订阅的是这个主题）
      },
      header: {
        'content-type': "application/x-www-form-urlencoded"
      },
      success (res) {
        console.log(res.data)
        if(res.data.status === "online"){//如果在线
          that.setData({
            device_status:"在线"  //设置状态为在线
          })
        }else{                          //如果不在线
          that.setData({
            device_status:"离线"   //设置状态为离线
          })
        }
        console.log(that.data.device_status)
      }
    })    
  },
   //获取开关状态，检查设备是打开还是关闭
  getOnOff(){
    //api 接口详细说明见巴法云接入文档
    var that = this
    wx.request({
      url: 'https://api.bemfa.com/api/device/v1/data/1/get/', //状态api接口，详见巴法云接入文档
      data: {
        uid: that.data.uid,
        topic: that.data.dhttopic,
        num:1
      },
      header: {
        'content-type': "application/x-www-form-urlencoded"
      },
      success (res) {
        console.log(res.data.msg)
        if(res.data.msg == "on"){  //如果开关on
          that.setData({
            checked:true,
            ledOnOff:"打开",
            ledicon: "/utils/img/lighton.png",
          })
        }else{           //如果开关off
          that.setData({
            checked:false,
            ledOnOff:"关闭",
            ledicon: "/utils/img/jingbao0.png",
          })
        }
      }
    })    
  },
  getdht11(){
    //获取温湿度值，屏幕初始化时，未订阅收到温湿度时，先去主动获取值
    //api 接口详细说明见巴法云接入文档
    var that = this
    wx.request({
      url: 'https://api.bemfa.com/api/device/v1/data/1/get/', //状态api接口，详见巴法云接入文档
      data: {
        uid: that.data.uid,
        topic: that.data.dhttopic,
        num:1
      },
      header: {
        'content-type': "application/x-www-form-urlencoded"
      },
      success (res) {
       
        console.log(res)
        if(res.data.msg && res.data.msg.indexOf("#") != -1){//如果数据里包含#号，表示获取的是传感器值，因为单片机上传数据的时候用#号进行了包裹
          //如果有#号就进行字符串分割
          var all_data_arr = res.data.msg.split("#"); //分割数据，并把分割后的数据放到数组里。
          console.log(all_data_arr)//打印数组
          that.setData({ //数据赋值给变量
            wendu:all_data_arr[1],//赋值温度
            shidu:all_data_arr[2], //赋值湿度
            light:all_data_arr[3], //光照
            smoke:all_data_arr[4], //烟雾  
            ledflag:all_data_arr[5], //led_flag   
            beepflag:all_data_arr[6], //led_flag   
            rgbflag:all_data_arr[7], //led_flag   
            relayflag:all_data_arr[8], //led_flag                 
            dataTime:res.data.time
          })
            // ====================== 【关键】自动同步图片 ======================
          // 灯状态
          let ledStatus = all_data_arr[5] == "1" ? 1 : 0;
          that.setData({ [`imgGroup[0].currentIndex`]: ledStatus });

          // 蜂鸣器状态
          let beepStatus = all_data_arr[6] == "1" ? 1 : 0;
          that.setData({ [`imgGroup[1].currentIndex`]: beepStatus });

          // RGB 状态（0=关 1=红 2=绿 3=蓝）
          let rgbStatus = parseInt(all_data_arr[7]) || 0;
          if (rgbStatus > 3) { rgbStatus = 0; }
          that.setData({ [`imgGroup[2].currentIndex`]: rgbStatus });

          // 继电器状态
          let relayStatus = all_data_arr[8] == "1" ? 1 : 0;
          that.setData({ [`imgGroup[3].currentIndex`]: relayStatus });
        //   if(all_data_arr[3] != undefined){//判断是否上传了led状态
        //     if(all_data_arr[3] == "on"){//如果单片机处于打开状态
        //         that.setData({ //数据赋值给变量
        //           ledOnOff:"打开",//赋值led状态
        //         })
        //     }else{
        //       that.setData({ //数据赋值给变量
        //         ledOnOff:"关闭",//赋值led状态
        //       })
        //     }
        //   }

        }
      }
    })    
  }, 


  //发送开关数据
  LedSendMsg(msg){
    wx.request({
      url: 'https://api.bemfa.com/api/device/v1/data/1/push/get/?', //状态api接口，详见巴法云接入文档
      data: {
        uid: this.data.uid,
        topic: this.data.ledtopic,
        msg:msg
      },
      header: {
        'content-type': "application/x-www-form-urlencoded"
      },
      success (res) {
      }
    })   
  },
 // ===================== 1. 点击切换图片 + 发送消息 =====================
 changeImg(e) {
    const index = e.currentTarget.dataset.index;
    const item = this.data.imgGroup[index];
    const nextIndex = (item.currentIndex + 1) % item.stateList.length;

    this.setData({
      [`imgGroup[${index}].currentIndex`]: nextIndex
    }, () => {

          // 只发送当前图片的指令 → 互不干扰
    const sendMsg = item.stateList[nextIndex].msg;
      this.LedSendMsg(sendMsg);
    });
  },

})
