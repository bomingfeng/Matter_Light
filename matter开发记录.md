sudo modprobe ch341

sudo chmod 777 /dev/ttyUSB0

idf.py -p /dev/ttyUSB0 erase_flash

idf.py -p /dev/ttyUSB0 flash 

idf.py -p /dev/ttyUSB0 monitor

idf.py -p /dev/ttyUSB0 erase_flash flash monitor

idf.py fullclean

idf.py clean

idf.py set-target esp32s3

idf.py build


合宇USB下载
idf.py -p /dev/ttyACM0 flash 


chip-tool interactive start

pairing ble-wifi 0x7283 A412  A41296956 20202021 3840
pairing ble-wifi 0x7283 A412  A41296956 20202021 3841

#define CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR 0xF01 //3840 只能修改这个
esp-matter/connectedhomeip/connectedhomeip/src/include/platform/CHIPDeviceConfig.h


(matter esp wifi connect A412 A41296956

pairing onnetwork 0x7283 20202021

pairing code-wifi 0x7283 A412 A41296956 34970112332(蓝牙）)


onoff toggle 0x7283 0x1

onoff on 0x7283 0x1

levelcontrol move-to-level 10 0 0 0 0x7283 0x1

colorcontrol move-to-saturation 200 0 0 0 0x7283 0x1

colorcontrol move-to-hue 150 0 0 0 0 0x7283 0x1



0x7283<->(29315);	0x5164<->(20836)
灯   accesscontrol write acl '[{"privilege": 5, "authMode": 2, "subjects": [ 112233,   29315    ], "targets": null}]' 0x5164	0x0
										       开关Node_ID			    灯Node_ID			

开关  binding write binding '[{"node": 20836,    "endpoint":1, "cluster":6}]' 0x7283	 0x1
				            灯Node_ID				    开关Node_ID  灯endpoint



已安装使用皇龙湾玄关射灯："node": 8888(0x22B8),    灯"endpoint":1, "cluster":6		开关"endpoint":2, "cluster":3
pairing onnetwork 0x22B8 20202021
accesscontrol write acl '[{"privilege":5,"authMode":2,"subjects":[112233,6666],"targets":null}]' 0x22B8 0x0
binding write binding '[{"node":6666,"endpoint":2,"cluster":6}]' 0x22B8 0x2

已安装使用皇龙湾大门门禁：本址"node": 6666(0x1A0A),   灯"endpoint":2, "cluster":6	开关"endpoint":1, "cluster":3	
pairing onnetwork 0x1A0A 20202021	            
accesscontrol write acl '[{"privilege":5,"authMode":2,"subjects":[112233,8888],"targets":null}]' 0x1A0A 0x0
binding write binding '[{"node":8888,"endpoint":1,"cluster":6}]' 0x1A0A 0x1
			            
项目目录下的CMakeLists.txt把：set(ZAP_GENERATED_PATH ${CMAKE_CURRENT_LIST_DIR}/../main/zap-generated)


namespace color_temperature_light {
typedef struct config {
    cluster::identify::config_t identify;
    cluster::groups::config_t groups;
    cluster::scenes::config_t scenes;
    cluster::on_off::config_t on_off;			//->1;2 
    cluster::level_control::config_t level_control;	
    cluster::color_control::config_t color_control;
} config_t;

namespace cluster {
namespace on_off {
typedef struct config {
    uint16_t cluster_revision;
    bool on_off;					//->1
    feature::lighting::config_t lighting;		//->2
    config() : cluster_revision(4), on_off(false) {}
} config_t;

cluster_t *create(endpoint_t *endpoint, config_t *config, uint8_t flags, uint32_t features);
} /* on_off */
}

namespace feature {
namespace lighting {

typedef struct config {
    bool global_scene_control;
    nullable<uint16_t> on_time;
    nullable<uint16_t> off_wait_time;
    nullable<uint8_t> start_up_on_off;			//->2
    config() : global_scene_control(1), on_time(0), off_wait_time(0), start_up_on_off(0) {}
} config_t;

uint32_t get_id();
esp_err_t add(cluster_t *cluster, config_t *config);

} /* lighting */
} /* feature */
} /* on_off */


esp-matter/connectedhomeip/connectedhomeip/third_party/mbed-mcu-boot/repo/boot/espressif/hal/esp-idf/examples/protocols/http_server/file_serving/main/file_server.c
A41296956


