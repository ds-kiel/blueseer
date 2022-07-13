/* 
We craft data samples from advertisments received during a BLE scan.
The data samples contain values for pre-defined features.
Each data sample is labeled with the corresponding environment and saved to the SD-card.

Secondly we predict the environment of a data sample using a pre-defined neural network.
The predicted environment and the confidence in the prediciton is saved to the SD-card.

All user interaction (to select environment and daytime) is happening via a display and 3 buttons.
*/

#include "module_config.h"

#ifdef WITH_CLASSIFICATION
    #include "blueseer.h"
    #include "blueseer_model.h"
#endif /* WITH_CLASSIFICATION */

#ifdef WITH_DISPLAY
	#include "display.h"
#endif /* WITH_DISPLAY */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#ifdef WITH_LOGGING
#include <sys/printk.h>
#endif /* WITH_LOGGING */
#include <sys/__assert.h>
#include <string.h>
#include <math.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/util.h>
#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <kernel.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>


#ifdef WITH_SD
#include <storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>
#endif /* WITH_SD */

#include <inttypes.h>

#define SECOND 1000

//scan
#define SCAN_COUNT 5
#define SCAN_TIME 3

const struct bt_le_scan_param scan_param = {
	.type = BT_HCI_LE_SCAN_ACTIVE,
	.options = BT_LE_SCAN_OPT_NONE,
	.interval = 0x0010,
	.window = 0x0010,
};

//data sample
#define DATA_LINE_LENGTH 23
#define DATA_LENGTH 115

// const char environments[][11] = {
// 	"home",	
// 	"office",
// 	"lecture",
// 	"shopping",
// 	"transport",
// 	"nature",
// 	"street",
// 	"mensa",
// 	"restaurant",
// 	"unknown"
// };
const char environments[][11] = {
	"transport", 
	"office",
	"shopping", 
	"street", 
	"home", 
	"restaurant", 
	"nature",
	"unknown"
};
#define ENVIRONMENT_COUNT 8

const char daytimes[][8] = { "morning", "noon", "aftern", "evening", "night" };
#define DAYTIME_COUNT 5

#ifdef WITH_SD
static char current_dir[50] = "";
#endif /* WITH_SD */

#ifdef WITH_SD
const char most_common_services[][5] = {
	"0af0", "1802", "180f", "1812", "1826", "2222", "ec88", "fd5a",
	"fd6f", "fdd2", "fddf", "fe03", "fe07", "fe0f", "fe61", "fe9f",
	"fea0", "feb9", "febe", "fee0", "ff0d", "ffc0", "ffe0",
};
#endif /* WITH_SD */

#define MOST_COMMON_SERVICES_COUNT 23

#ifdef WITH_SD
const char feature_names[] =
	"label, device_count, lost_devices, new_devices, different_services, services_count, txpower_count, tx_power_avg, min_txpower, max_txpower, man_packet_len_count, manufacturer_data_lengths_sum, manufacturer_data_len_avg, avg_received, min_received, max_received, avg_avg_rssi, min_avg_rssi, max_avg_rssi, min_rssi, max_rssi, avg_rssi_difference, avg_avg_difference_between_beacons, avg_difference_first_last";

// new names below
// const char feature_names[] =
// 	"label, device_count, lost_devices, new_devices, different_services, services_count, txpower_count, tx_power_avg, min_txpower, max_txpower, man_packet_len_count, manufacturer_data_lengths_sum, manufacturer_data_len_avg, avg_received, min_received, max_received, avg_avg_rssi, min_avg_rssi, max_avg_rssi, min_rssi, max_rssi, avg_rssi_difference, avg_avg_difference_between_adv, avg_difference_first_last";
#endif /* WITH_SD */

//how many samples are created/predicted until program terminates
#define N_SAMPLES 1800

//Limitations that max out SRAM
#define MAX_DEVICES 100 // 150 //max unique devices // todo: set max to 100 to save space?
#define MAX_ADVERTISEMENTS_RECEIVED 100 // 140 //max amount of advertisments from one device // todo: set to lower value?
#define MAX_DIFFERENT_TX_POWERS 30 //max unique txpowers received
#define MAX_DIFFERENT_MAN_PACKET_LEN 30 //max unique manufacturer packet lengths

//buttons
#ifdef WITH_BUTTONS
#define SWA_NODE DT_ALIAS(swa)
#define SWB_NODE DT_ALIAS(swb)
#define SWC_NODE DT_ALIAS(swc)

static const struct gpio_dt_spec buttonA = GPIO_DT_SPEC_GET_OR(SWA_NODE, gpios, { 0 });
static const struct gpio_dt_spec buttonB = GPIO_DT_SPEC_GET_OR(SWB_NODE, gpios, { 0 });
static const struct gpio_dt_spec buttonC = GPIO_DT_SPEC_GET_OR(SWC_NODE, gpios, { 0 });

static struct gpio_callback button_cb_dataA;
static struct gpio_callback button_cb_dataB;
static struct gpio_callback button_cb_dataC;
#endif /* WITH_BUTTONS */

//LEDs
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN0 DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS0 DT_GPIO_FLAGS(LED0_NODE, gpios)
#define LED1 DT_GPIO_LABEL(LED1_NODE, gpios)
#define PIN1 DT_GPIO_PIN(LED1_NODE, gpios)
#define FLAGS1 DT_GPIO_FLAGS(LED1_NODE, gpios)

const struct device *led0;
const struct device *led1;


#ifdef WITH_SD
	//SD-Card
	static FATFS fat_fs;
	/* mounting info */
	static struct fs_mount_t mp = {
		.type = FS_FATFS,
		.fs_data = &fat_fs,
	};
	const char *disk_mount_pt = "/SD:";
	const char *dataPath = "/ble_data";
	const char *evalPath = "/eval";
	bool sd_card_initialized = false;
#endif /* WITH_SD */

/*
data extracted from received BLE advertisments
*/
#ifdef WITH_PARSING
typedef struct RSS_stats {
	int8_t min, max;
	uint32_t time_first_adv, time_last_adv, sum_time_between_adv;
	int32_t sum;
	uint16_t count;
} RSS_stats;

//unique devices we receive at least one advertisment
static int device_count = 0;
static RSS_stats rss_statistics[MAX_DEVICES]; // rssi statistics for each device

static char devices[MAX_DEVICES][BT_ADDR_LE_STR_LEN]; //device ids

//TxPower and manufacturer data length
static int txPower[MAX_DIFFERENT_TX_POWERS][2];
static int manufacturer_data_len[MAX_DIFFERENT_MAN_PACKET_LEN][2];

//provided services
static int different_services = 0;
static int services_count = 0;
static char services[MOST_COMMON_SERVICES_COUNT][10]; //services UUIDs
static bool dev_services[MAX_DEVICES]
			[MOST_COMMON_SERVICES_COUNT]; //which devices provide which service

static int old_device_count = 0;
static char old_devices[MAX_DEVICES][BT_ADDR_LE_STR_LEN];

//final data sample that is written to SD-card in CSV file
static int data_sample[DATA_LENGTH];
#endif /* WITH_PARSING */

#ifdef WITH_SD
//number of data samples existing
static int data_file_count = 0;

//data sample string that is written to CSV file
static char data_str[3000];
#endif /* WITH_SD */

//measure time needed for processing and classification
static int time_points[6];

/*
to select current environment (to label data samples) and current time of the day (for evaluation)
*/

static int current_environment;
static bool environment_selected;

static int current_daytime;
static bool daytime_selected;

#ifdef WITH_CLASSIFICATION
//current classification (prediction and probability)
static classification current_classification;
#endif /* WITH_CLASSIFICATION */

//get index of device addr
//return -1 if addr not found
#ifdef WITH_PARSING
int getIndex(char *addr)
{
	for (int i = 0; i < device_count; i++) {
		if (!strcmp(devices[i], addr)) {
			return i;
		}
	}
	return -1;
}

//add new unique device (BLE adress)
void addDevice(char *addr)
{
	if (device_count < MAX_DEVICES) {
		strcpy(devices[device_count], addr);
		device_count++;
	}
}
#endif /* WITH_PARSING */

/*
obtain txpower, manufacturer data and service UUIDs from advertisment data
*/
#ifdef WITH_PARSING
static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = static_cast<bt_addr_le_t *>(user_data);
	char result[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, result, BT_ADDR_LE_STR_LEN);

	uint8_t txp = 0;
	uint8_t len = 0;

	switch (data->type) {
	case BT_DATA_TX_POWER:

		txp = data->data[0];

		for (int i = 0; i < MAX_DIFFERENT_TX_POWERS; i++) {
			if (txPower[i][0] == txp) {
				txPower[i][1]++;
				break;
			}
			if (txPower[i][1] == 0) {
				txPower[i][1]++;
				txPower[i][0] = txp;
				break;
			}
		}

		break;
	case BT_DATA_MANUFACTURER_DATA:
		len = data->data_len;
		for (int i = 0; i < MAX_DIFFERENT_MAN_PACKET_LEN; i++) {
			if (manufacturer_data_len[i][0] == len) {
				manufacturer_data_len[i][1]++;
				break;
			}
			if (manufacturer_data_len[i][1] == 0) {
				manufacturer_data_len[i][1]++;
				manufacturer_data_len[i][0] = len;
				break;
			}
		}
		break;

	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data->data_len % sizeof(uint16_t) != 0U) {
			#ifdef WITH_LOGGING
			printk("AD malformed\n");
			#endif /* WITH_LOGGING */
			return true;
		}

		for (int i = 0; i < data->data_len; i += sizeof(uint16_t)) {
			struct bt_uuid *uuid;
			uint16_t u16;

			memcpy(&u16, &data->data[i], sizeof(u16));

			struct bt_uuid_16 temp[] = { { .uuid = { BT_UUID_TYPE_16 },
						       .val = (sys_le16_to_cpu(u16)) } };

			uuid = ((struct bt_uuid *)(temp));

			char uuid_str[100];
			bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));

			for (int j = 0; j <= different_services; j++) {
				if (!strcmp(services[j], uuid_str)) {
					if (!dev_services[getIndex(result)][j]) {
						dev_services[getIndex(result)][j] = true;
						services_count++;
					}

					return false;
				}
				if (j == different_services) {
					strcpy(services[j], uuid_str);
					different_services++;
					dev_services[getIndex(result)][j] = true;
					services_count++;

					return false;
				}
			}
		}
	}
	return true;
}
#endif /* WITH_PARSING */

/*
callback method when new advertisment is received
*/
static void scan_cb(const bt_addr_le_t *addr, int8_t rss, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	#ifdef WITH_PARSING
	char result[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, result, BT_ADDR_LE_STR_LEN);

	int index = getIndex(result);

	uint32_t current_time = k_cycle_get_32();;
	if (index == -1) {
		if (device_count >= MAX_DEVICES) {
			return;
		}
		//new device
		addDevice(result);
		index = getIndex(result);

		rss_statistics[index].min = rss;
		rss_statistics[index].max = rss;
		rss_statistics[index].time_first_adv = current_time;
		rss_statistics[index].time_last_adv = current_time;
	}
	else {
		//known device
		if (rss < rss_statistics[index].min){
			rss_statistics[index].min = rss;
		}
		if (rss < rss_statistics[index].max){
			rss_statistics[index].max = rss;
		}
	}
	rss_statistics[index].sum += rss;
	rss_statistics[index].count++;

	rss_statistics[index].sum_time_between_adv += (current_time - rss_statistics[index].time_last_adv);
	rss_statistics[index].time_last_adv = current_time;

	bt_data_parse(buf, eir_found, (void *)addr);
	#endif /* WITH_PARSING */
}

#ifdef WITH_BUTTONS
	/*
	top button callback
	for switching between different environments and times of the day
	*/
	void buttonA_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
	{
		if (!environment_selected) {
			if (current_environment < ENVIRONMENT_COUNT - 1) {
				current_environment++;
			} else {
				current_environment = 0;
			}

		} else if (!daytime_selected) {
			if (current_daytime < DAYTIME_COUNT - 1) {
				current_daytime++;
			} else {
				current_daytime = 0;
			}
		}
	}

	/*
	center button callback
	confirm environment and time of the day selection
	*/
	void buttonB_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
	{
		if (!environment_selected) {
			environment_selected = true;

		} else if (!daytime_selected) {
			daytime_selected = true;
		}
	}

	/*
	bottom button callback
	for switching between different environments and times of the day
	*/
	void buttonC_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
	{
		if (!environment_selected) {
			if (current_environment > 0) {
				current_environment--;
			} else {
				current_environment = ENVIRONMENT_COUNT - 1;
			}

		} else if (!daytime_selected) {
			if (current_daytime > 0) {
				current_daytime--;
			} else {
				current_daytime = DAYTIME_COUNT - 1;
			}
		}
	}

	/*
	setup buttons
	*/
	void initButtons()
	{
		gpio_pin_configure_dt(&buttonA, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&buttonA, GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&button_cb_dataA, buttonA_pressed, BIT(buttonA.pin));
		gpio_add_callback(buttonA.port, &button_cb_dataA);

		gpio_pin_configure_dt(&buttonB, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&buttonB, GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&button_cb_dataB, buttonB_pressed, BIT(buttonB.pin));
		gpio_add_callback(buttonB.port, &button_cb_dataB);

		gpio_pin_configure_dt(&buttonC, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&buttonC, GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&button_cb_dataC, buttonC_pressed, BIT(buttonC.pin));
		gpio_add_callback(buttonC.port, &button_cb_dataC);
	}
#endif /* WITH_BUTTONS */

/*
setup LEDs
*/
void initLEDs()
{
	led0 = device_get_binding(LED0);
	led1 = device_get_binding(LED1);

	gpio_pin_configure(led0, PIN0, GPIO_OUTPUT_ACTIVE | FLAGS0);
	gpio_pin_configure(led1, PIN1, GPIO_OUTPUT_ACTIVE | FLAGS1);

	gpio_pin_set(led0, PIN0, (int)false);
	gpio_pin_set(led1, PIN1, (int)false);
}

void setLED0(bool on)
{
	gpio_pin_set(led0, PIN0, (int)on);
}
void setLED1(bool on)
{
	gpio_pin_set(led1, PIN1, (int)on);
}


#ifdef WITH_SD
	/*
	get number of files in dir at given path
	*/
	int get_file_count(const char *path)
	{
		int res;
		struct fs_dir_t dirp;
		static struct fs_dirent entry;

		fs_dir_t_init(&dirp);

		res = fs_opendir(&dirp, path);
		if (res) {
			#ifdef WITH_LOGGING
			printk("Error opening dir %s [%d]\n", path, res);
			#endif /* WITH_LOGGING */
			return -1;
		}

		int files = 0;
		for (;;) {
			res = fs_readdir(&dirp, &entry);

			if (res || entry.name[0] == 0) {
				break;
			}

			if (entry.type != FS_DIR_ENTRY_DIR) {
				files++;
			}
		}

		fs_closedir(&dirp);

		return files;
	}

	/*
	creates dir at given path if path exists
	*/
	int createDir(const char *path)
	{
		struct fs_dir_t dir_data;

		fs_dir_t_init(&dir_data);

		int dir_err = fs_opendir(&dir_data, path);
		//check if dir already exists at path
		if (dir_err) {
			fs_mkdir(path);
			#ifdef WITH_LOGGING
			printk("created dir: %s\n", path);
			#endif /* WITH_LOGGING */
		}
		fs_closedir(&dir_data);

		return get_file_count(path);
	}

	/*
	setup SD-card and create dirs to save data samples in
	*/
	void initSDCard()
	{
		do {
			static const char *disk_pdrv = "SD";
			uint64_t memory_size_mb;
			uint32_t block_count;
			uint32_t block_size;

			if (disk_access_init(disk_pdrv) != 0) {
				#ifdef WITH_LOGGING
				printk("Storage init ERROR!");
				#endif /* WITH_LOGGING */
				break;
			}

			if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
				#ifdef WITH_LOGGING
				printk("Unable to get sector count");
				#endif /* WITH_LOGGING */
				break;
			}
			#ifdef WITH_LOGGING
			printk("Block count %u", block_count);
			#endif /* WITH_LOGGING */

			if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
				#ifdef WITH_LOGGING
				printk("Unable to get sector size");
				#endif /* WITH_LOGGING */
				break;
			}
			#ifdef WITH_LOGGING
			printk("Sector size %u\n", block_size);
			#endif /* WITH_LOGGING */

			memory_size_mb = (uint64_t)block_count * block_size;
			#ifdef WITH_LOGGING
			printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
			#endif /* WITH_LOGGING */
		} while (0);

		mp.mnt_point = disk_mount_pt;

		int res = fs_mount(&mp);

		if (res == FR_OK) {
			#ifdef WITH_LOGGING
			printk("Disk mounted.\n");
			#endif /* WITH_LOGGING */

		} else {
			#ifdef WITH_LOGGING
			printk("Error mounting disk.\n");
			#endif /* WITH_LOGGING */
			return;
		}

		//create dirs for data samples, eval data and times of the day

		char data_path[50];
		strcpy(data_path, disk_mount_pt);
		strcat(data_path, dataPath);

		char eval_path[50];
		strcpy(eval_path, disk_mount_pt);
		strcat(eval_path, evalPath);

		#ifdef WITH_LOGGING
		printk("files in %s: %d\n", data_path, createDir(data_path));
		printk("files in %s: %d\n", eval_path, createDir(eval_path));
		#endif /* WITH_LOGGING */

		for (int i = 0; i < DAYTIME_COUNT; i++) {
			char dir_day_path[50];
			strcpy(dir_day_path, eval_path);
			strcat(dir_day_path, "/");
			strcat(dir_day_path, daytimes[i]);

			createDir(dir_day_path);

			char dir_day_data_path[50];
			strcpy(dir_day_data_path, data_path);
			strcat(dir_day_data_path, "/");
			strcat(dir_day_data_path, daytimes[i]);

			createDir(dir_day_data_path);
		}

		sd_card_initialized = true;
	}

	/*
	check if file at given path exists
	return >=0 if file exists
	*/
	int fileExists(struct fs_file_t *file, char *path)
	{
		fs_file_t_init(file);
		int rc;
		rc = fs_open(file, path, FS_O_RDWR);
		if (rc >= 0) {
			fs_close(file);
		}

		return rc;
	}

	/*
	open or create file at given path
	*/
	int openOrCreateFile(struct fs_file_t *file, char *path)
	{
		fs_file_t_init(file);
		int rc;
		rc = fs_open(file, path, FS_O_CREATE | FS_O_RDWR);
		#ifdef WITH_LOGGING
		if (rc < 0 && rc != -2) {
			//error while creating/opening file
			printk("FAIL: open %s: %d\n", path, rc);
		}
		#endif /* WITH_LOGGING */

		return rc;
	}

	/*
	search for file name that doesnt exist (by increasing index number)
	write data sample string (data_str) to CSV file
	*/
	void writeDataFile()
	{
		struct fs_file_t dataFile;

		int rc;

		char filePath[100];

		//create directory if new experiment started
		if (strcmp(current_dir, "") == 0){
			unsigned int dir_count = 0;
			while (1) {
				char current_env_str[20];
				strcpy(current_env_str, environments[current_environment]);

				char first[2];
				sprintf(first, "%c", current_env_str[0]);

				char second[2];
				sprintf(second, "%c", current_env_str[1]);

				char count_str[20];
				sprintf(count_str, "/%s%s%d", first, second, dir_count);

				char dataDirPath[50];
				strcpy(dataDirPath, disk_mount_pt);
				strcat(dataDirPath, dataPath);	
				strcat(dataDirPath, "/");
				strcat(dataDirPath, daytimes[current_daytime]);
				strcat(dataDirPath, count_str);

				#ifdef WITH_LOGGING
				printk("data dir path: %s\n", dataDirPath);
				#endif /* WITH_LOGGING */

				struct fs_dir_t dir_data;

				fs_dir_t_init(&dir_data);

				int dir_err_data = fs_opendir(&dir_data, dataDirPath);
				#ifdef WITH_CLASSIFICATION
				
					char evalDirPath[50];
					strcpy(evalDirPath, disk_mount_pt);
					strcat(evalDirPath, evalPath);	
					strcat(evalDirPath, "/");
					strcat(evalDirPath, daytimes[current_daytime]);
					strcat(evalDirPath, count_str);

					#ifdef WITH_LOGGING
					printk("eval dir path: %s\n", evalDirPath);
					#endif /* WITH_LOGGING */

					struct fs_dir_t dir_eval;

					fs_dir_t_init(&dir_eval);

					int dir_err_eval = fs_opendir(&dir_eval, evalDirPath);
					//check if dir already exists at path
					if (dir_err_data && dir_err_eval) {
						fs_mkdir(dataDirPath);
						fs_mkdir(evalDirPath);
						#ifdef WITH_LOGGING
						printk("created dir: %s\n", dataDirPath);
						printk("created dir: %s\n", evalDirPath);
						#endif /* WITH_LOGGING */
						strcpy(current_dir, count_str);
						fs_closedir(&dir_data);
						fs_closedir(&dir_eval);
						break;
					}
					fs_closedir(&dir_eval);

				#else
					//check if dir already exists at path
					if (dir_err_data) {
						fs_mkdir(dataDirPath);
						#ifdef WITH_LOGGING
						printk("created dir: %s\n", dataDirPath);
						#endif /* WITH_LOGGING */
						strcpy(current_dir, count_str);
						fs_closedir(&dir_data);
						break;
					}
				#endif /* WITH_CLASSIFICATION */
				fs_closedir(&dir_data);

				dir_count++;
			}
		}

		#if !defined(WITH_CLASSIFICATION) && defined(WITH_DISPLAY)
		setDisplayText("Saving...");
		#endif /* !WITH_CLASSIFICATION && WITH_DISPLAY */

		//find file name that doesnt exist
		while (1) {
			char current_env_str[20];
			strcpy(current_env_str, environments[current_environment]);

			char first[2];
			sprintf(first, "%c", current_env_str[0]);

			char second[2];
			sprintf(second, "%c", current_env_str[1]);

			char count_str[20];
			sprintf(count_str, "/%s%s%d.csv", first, second, data_file_count);

			char dataFilePath[50];
			strcpy(dataFilePath, disk_mount_pt);
			strcat(dataFilePath, dataPath);	
			strcat(dataFilePath, "/");
			strcat(dataFilePath, daytimes[current_daytime]);
			strcat(dataFilePath, "/");
			strcat(dataFilePath, current_dir);
			strcat(dataFilePath, count_str);

			#ifdef WITH_LOGGING
			printk("data file path: %s\n", dataFilePath);
			#endif /* WITH_LOGGING */

			rc = fileExists(&dataFile, dataFilePath);

			if (rc == -2) {
				rc = openOrCreateFile(&dataFile, dataFilePath);
				fs_seek(&dataFile, 0, FS_SEEK_SET);
				strcpy(filePath, dataFilePath);
				data_file_count++;
				break;
			}

			data_file_count++;
		}
		

		fs_write(&dataFile, data_str, strlen(data_str) * sizeof(char));
		fs_sync(&dataFile);

		fs_close(&dataFile);

		static struct fs_dirent entry;
		fs_stat(filePath, &entry);
		#ifdef WITH_LOGGING
		printk("created [FILE] %s (size = %zu)\n", entry.name, entry.size);
		#endif /* WITH_LOGGING */
		#if !defined(WITH_CLASSIFICATION) && defined(WITH_DISPLAY)
		setDisplayText("Scanning...");
		#endif /* !WITH_CLASSIFICATION && WITH_DISPLAY */
	}
#endif /* WITH_SD */

/*
reset all data variables after scan and update old devices
*/
#ifdef WITH_PARSING
void reset()
{
	for (int i = 0; i < device_count; i++) {
		rss_statistics[i].min = 0;
		rss_statistics[i].max = 0;
		rss_statistics[i].sum = 0;
		rss_statistics[i].count = 0;
		rss_statistics[i].time_first_adv = 0;
		rss_statistics[i].time_last_adv = 0;
		rss_statistics[i].sum_time_between_adv = 0;

		strcpy(old_devices[i], devices[i]);
		strcpy(devices[i], "");

		for (int j = 0; j < MOST_COMMON_SERVICES_COUNT; j++) {
			dev_services[i][j] = false;
		}
	}
	for (int i = 0; i < MOST_COMMON_SERVICES_COUNT; i++) {
		strcpy(services[i], "");
	}

	for (int k = 0; k < MAX_DIFFERENT_TX_POWERS; k++) {
		txPower[k][0] = 0;
		txPower[k][1] = 0;
	}

	for (int k = 0; k < MAX_DIFFERENT_MAN_PACKET_LEN; k++) {
		manufacturer_data_len[k][0] = 0;
		manufacturer_data_len[k][1] = 0;
	}
	old_device_count = device_count;
	device_count = 0;
	different_services = 0;
	services_count = 0;
}
#endif /* WITH_PARSING */

/*
First the user selects the current environment and time of the day
Secondly a BLE scan is performed while saving data from received BLE advertisments
Thirdly the raw data is processed to features
Lastly the data sample is crafted from the latest 5 scans and classified to one of the selected environments (printed on display)
The data sample and the prediction are saved on the SD card for further evaluation
*/
void main(void)
{
	#ifdef WITH_DISPLAY
	initDisplay();
	#endif /* WITH_DISPLAY */
	#ifdef WITH_BUTTONS
	initButtons();
	#endif /* WITH_BUTTONS */
	initLEDs();
	#ifdef WITH_SD
	initSDCard();
	#endif /* WITH_SD */

	#ifdef WITH_CLASSIFICATION
	//initialize neural network
	blueseer_setup();
	#endif /* WITH_CLASSIFICATION */

	//inital environment and daytime
	current_environment = 0;
	char current_env_str[11];
	strcpy(current_env_str, environments[current_environment]);

	current_daytime = 0;
	char current_daytime_str[8];
	strcpy(current_daytime_str, daytimes[current_daytime]);

	#ifndef WITH_BUTTONS
	//select office afternoon
	current_environment = 1;
	current_daytime = 2;
	environment_selected = true;
	daytime_selected = true;
	#endif /* WITH_BUTTONS */

	//first select current environment
	while (!environment_selected) {
		k_msleep(10);
		strcpy(current_env_str, environments[current_environment]);
		#ifdef WITH_DISPLAY
		setDisplayText(current_env_str);
		#endif /* WITH_DISPLAY */

		#ifdef WITH_LOGGING
		//printk("%s\n", current_env_str);
		#endif /* WITH_LOGGING */
	}

	//secondly select time of the day
	while (!daytime_selected) {
		k_msleep(10);
		strcpy(current_daytime_str, daytimes[current_daytime]);
		#ifdef WITH_DISPLAY
		setDisplayText(current_daytime_str);
		#endif /* WITH_DISPLAY */
		#ifdef WITH_LOGGING
		//printk("%s\n", current_daytime_str);
		#endif /* WITH_LOGGING */

	}

	#ifdef WITH_DISPLAY
	setDisplayText("Preparing...");
	#endif /* WITH_DISPLAY */

	//initialize bluetooth
	int err = bt_enable(NULL);
	#ifdef WITH_LOGGING
		if (err) {
			printk("Bluetooth init failed (err %d)\n", err);
		}
		printk("Bluetooth initialized\n");

		printk("\nScanning... \n");
	#endif /* WITH_LOGGING */

	#ifdef WITH_DISPLAY
	setDisplayText("Scanning...");
	#endif /* WITH_DISPLAY */

	//collect and detect samples
	for (int r = 0; r < N_SAMPLES + SCAN_COUNT; r++) {
		//start time
		#ifdef WITH_PARSING
		reset();
		

		//shift back the feature values of the 4 latest scans by one scan and make room for a new scan
		for (int i = DATA_LINE_LENGTH * 4 - 1; i >= 0; i--) {
			data_sample[i + DATA_LINE_LENGTH] = data_sample[i];
		}
		#endif /* WITH_PARSING */

		//perform BLE scan
		time_points[0] = k_cycle_get_32();
		err = bt_le_scan_start(&scan_param, scan_cb);
		if (err) {
			#ifdef WITH_LOGGING
			printk("Starting scanning failed (err %d)\n", err);
			#endif /* WITH_LOGGING */
			break;
		}

		k_msleep(SECOND * SCAN_TIME);

		err = bt_le_scan_stop();
		time_points[1] = k_cycle_get_32();


		

		if (err) {
			#ifdef WITH_LOGGING
			printk("Stopping scanning failed (err %d)\n", err);
			#endif /* WITH_LOGGING */
			break;
		}
		//BLE scan performed

		#ifdef WITH_LOGGING
			//for monotoring device count and services
			printk("\nDevices: %d; services: ", device_count);

			for (int i = 0; i < different_services; i++) {
				printk("%s, ", services[i]);
			}
			printk("\n");
		#endif /* WITH_LOGGING */

		/*
		process raw data received during the BLE scan to feature values 
		*/
		time_points[2] = k_cycle_get_32();
		// k_msleep(1000);
		#ifdef WITH_PARSING
		//compare to last scan: new devices, lost devices
		int new_device_count = 0;
		int lost_device_count = 0;

		for (int k = 0; k < device_count; k++) {
			for (int l = 0; l < old_device_count; l++) {
				if (!strcmp(devices[k], old_devices[l])) {
					break;
				}
				if (l == old_device_count - 1) {
					new_device_count++;
				}
			}
		}

		for (int k = 0; k < old_device_count; k++) {
			for (int l = 0; l < device_count; l++) {
				if (!strcmp(old_devices[k], devices[l])) {
					break;
				}
				if (l == device_count - 1) {
					lost_device_count++;
				}
			}
		}
		data_sample[0] = device_count;
		data_sample[1] = lost_device_count;
		data_sample[2] = new_device_count;

		//TxPower count, min, max, avg
		int txpower_count = 0;
		int txpower_avg = 0;

		int min_txpower = 200;
		int max_txpower = 0;

		for (int k = 0; k < MAX_DIFFERENT_TX_POWERS; k++) {
			if (txPower[k][1] != 0) {
				txpower_count += txPower[k][1];
				txpower_avg += (txPower[k][0] * txPower[k][1]);

				if (txPower[k][0] > max_txpower) {
					max_txpower = txPower[k][0];
				}

				if (txPower[k][0] < min_txpower) {
					min_txpower = txPower[k][0];
				}
			}
		}
		if (txpower_count != 0) {
			txpower_avg /= txpower_count;
		}

		//manufacturer packet length count, avg and sum
		int man_packet_len_count = 0;
		int man_packet_len_avg = 0;
		int man_packet_len_sum = 0;

		for (int k = 0; k < MAX_DIFFERENT_MAN_PACKET_LEN; k++) {
			if (manufacturer_data_len[k][1] != 0) {
				man_packet_len_count += manufacturer_data_len[k][1];
				man_packet_len_avg +=
					(manufacturer_data_len[k][0] * manufacturer_data_len[k][1]);

				man_packet_len_sum += manufacturer_data_len[k][0];
			}
		}
		if (man_packet_len_avg != 0) {
			man_packet_len_avg /= man_packet_len_count;
		}

		data_sample[3] = different_services;
		data_sample[4] = services_count;
		data_sample[5] = txpower_count;
		data_sample[6] = txpower_avg;
		data_sample[7] = min_txpower;
		data_sample[8] = max_txpower;
		data_sample[9] = man_packet_len_count;
		data_sample[10] = man_packet_len_sum;
		data_sample[11] = man_packet_len_avg;

		// RSSI feature values
		int avg_received = 0;
		int min_received = MAX_ADVERTISEMENTS_RECEIVED;
		int max_received = 0;

		int avg_avg_rssi = 0;
		int min_rssi = 0;
		int max_rssi = -100;
		int avg_rssi_difference = 0;

		int min_avg_rssi = 0;
		int max_avg_rssi = -100;

		//time difference (in CPU cycles) between advertisments
		int avg_avg_difference_between_adv = 0;
		int avg_difference_first_last = 0;

		for (int i = 0; i < device_count; i++) {
			if (i == 0){
				min_received = rss_statistics[0].count;
				max_received = rss_statistics[0].count;
			} else {
				if (rss_statistics[i].count < min_received){
					min_received = rss_statistics[i].count;
				}
				if (rss_statistics[i].count > max_received){
					max_received = rss_statistics[i].count;
				}
			}
			avg_received += rss_statistics[i].count;

			avg_rssi_difference += (rss_statistics[i].max - rss_statistics[i].min);

			if (rss_statistics[i].count != 0) {
				int current_avg = (rss_statistics[i].sum / rss_statistics[i].count);
				avg_avg_rssi += current_avg;

				if (current_avg > max_avg_rssi) {
					max_avg_rssi = current_avg;
				}

				if (current_avg < min_avg_rssi) {
					min_avg_rssi = current_avg;
				}
			}
			if (rss_statistics[i].count > 1) {
				avg_avg_difference_between_adv += (rss_statistics[i].sum_time_between_adv / (rss_statistics[i].count - 1));
				avg_difference_first_last += (rss_statistics[i].time_last_adv - rss_statistics[i].time_first_adv);
			}

			if (rss_statistics[i].min < min_rssi) {
				min_rssi = rss_statistics[i].min;
			}
			if (rss_statistics[i].max > max_rssi) {
				max_rssi = rss_statistics[i].max;
			}
		}
		if (device_count != 0) {
			avg_received = avg_received / device_count;
			avg_avg_rssi = avg_avg_rssi / device_count;
			avg_rssi_difference = avg_rssi_difference / device_count;
			avg_avg_difference_between_adv = avg_avg_difference_between_adv / device_count;
			avg_difference_first_last = avg_difference_first_last / device_count;
		}

		data_sample[12] = avg_received;
		data_sample[13] = min_received;
		data_sample[14] = max_received;
		data_sample[15] = avg_avg_rssi;
		data_sample[16] = min_avg_rssi;
		data_sample[17] = max_avg_rssi;
		data_sample[18] = min_rssi;
		data_sample[19] = max_rssi;
		data_sample[20] = avg_rssi_difference;
		data_sample[21] = avg_avg_difference_between_adv;
		data_sample[22] = avg_difference_first_last;

		// //provided services
		// for (int s = 0; s < MOST_COMMON_SERVICES_COUNT; s++) {
		// 	for (int t = 0; t < different_services; t++) {
		// 		if (!strcmp(most_common_services[s], services[t])) {
		// 			int s_count = 0;
		// 			for (int u = 0; u < device_count; u++) {
		// 				if (dev_services[u][t]) {
		// 					s_count++;
		// 				}
		// 			}
		// 			data_sample[23 + s] = s_count;
		// 			break;
		// 		}

		// 		if (t == different_services - 1) {
		// 			data_sample[23 + s] = 0;
		// 		}
		// 	}
		// }

		//timestamp after processing a scan
		#endif /* WITH_PARSING */
		// k_msleep(500);
		time_points[3] = k_cycle_get_32();

		//only if at least 5 scans were performed
		if (r > SCAN_COUNT - 1) {
			#ifdef WITH_CLASSIFICATION
				//classify data sample
				time_points[4] = k_cycle_get_32();
				// k_msleep(500);
				blueseer_infer(&data_sample[0], &current_classification);
				// k_msleep(1000);
				time_points[5] = k_cycle_get_32();
				int env_index = current_classification.index;
				int round_prob = (int)round(current_classification.probability * 100);
			#endif /* WITH_CLASSIFICATION */


			char current_env_str[20];
			strcpy(current_env_str, environments[current_environment]);

			char current_daytime_str[20];
			strcpy(current_daytime_str, daytimes[current_daytime]);

			#ifdef WITH_CLASSIFICATION

				#ifdef WITH_LOGGING
					printk("true environment: %s (index: %d)\n", current_env_str,
						current_environment);
					printk("predicted environment: %s (index: %d) (prob: %d%%)\n",
						available_env[env_index], env_index, round_prob);
				#endif /* WITH_LOGGING */

				#ifdef WITH_DISPLAY
					//show true and predicted environnment on the display
					char disp[50];
					strcpy(disp, "t: ");
					strcat(disp, current_env_str);
					strcat(disp, " (");
					strcat(disp, current_daytime_str);
					strcat(disp, ")");
					strcat(disp, "\n\np: ");
					strcat(disp, available_env[env_index]);
					char tmp_1[10];
					sprintf(tmp_1, " %d%%", round_prob);
					strcat(disp, tmp_1);
					setDisplayText(disp);
				#endif /* WITH_DISPLAY */

			#endif /* WITH_CLASSIFICATION */

			//if envrionment is unknown dont save data sample
			if (strcmp(current_env_str, "unknown")) {
				#ifdef WITH_CLASSIFICATION
					//turn on blue LED if prediction correct, otherwise turn on red LED
					if (!strcmp(current_env_str, available_env[env_index])) {
						setLED0(false);
						setLED1(true);

					} else {
						setLED0(true);
						setLED1(false);
					}
				#endif /* WITH_CLASSIFICATION */

				//convert data sample to string
				#ifdef WITH_SD
					//feature names
					strcpy(data_str, "");
					strcat(data_str, feature_names);
					for (int s = 0; s < MOST_COMMON_SERVICES_COUNT; s++) {
						strcat(data_str, ", ");
						strcat(data_str, most_common_services[s]);
					}

					strcat(data_str, ", time_point_1");
					strcat(data_str, ", time_point_2");
					strcat(data_str, ", time_point_3");
					strcat(data_str, "\n");
					strcat(data_str, current_env_str);

					//feature values
					for (int i = 0; i < DATA_LENGTH; i++) {
						if (i % DATA_LINE_LENGTH == 0 && i != 0) {
							for (int j = 1; j < 6; j+=2) {
								char time_point[20];
								sprintf(time_point, ", %d",
									time_points[j] - time_points[j-1]);
								strcat(data_str, time_point);
							}
							strcat(data_str, "\n");
							strcat(data_str, current_env_str);
						}
						char value[20];
						sprintf(value, ", %d", data_sample[i]);
						strcat(data_str, value);
					}

				
					//save data sample string to SD-card
					writeDataFile();

					#ifdef WITH_CLASSIFICATION
						//save predicted environment and probability to SD-card for later evaluation
						char env_path[50];
						strcpy(env_path, disk_mount_pt);
						strcat(env_path, evalPath);
						strcat(env_path, "/");
						strcat(env_path, current_daytime_str);
						strcat(env_path, current_dir);
						strcat(env_path, "/");

						char first[2];
						sprintf(first, "%c", current_env_str[0]);

						char second[2];
						sprintf(second, "%c", current_env_str[1]);

						char last[2];
						sprintf(last, "%c", current_env_str[strlen(current_env_str) - 1]);

						strcat(env_path, first);
						strcat(env_path, second);
						strcat(env_path, "_");
						strcat(env_path, last);
						strcat(env_path, ".txt");

						#ifdef WITH_LOGGING
						printk("env file: %s", env_path);
						#endif /* WITH_LOGGING */

						struct fs_file_t env_file;

						openOrCreateFile(&env_file, env_path);

						char temp[strlen(available_env[env_index]) + 4];
						strcpy(temp, available_env[env_index]);
						strcat(temp, tmp_1);
						strcat(temp, ", ");
						fs_seek(&env_file, 0, FS_SEEK_END);

						fs_write(&env_file, temp, strlen(temp) * sizeof(char));
						fs_sync(&env_file);

						fs_close(&env_file);
					#endif /* WITH_CLASSIFICATION */
				#endif /* WITH_SD */
			}
		}
	}

	#ifdef WITH_DISPLAY
	setDisplayText("done.");
	#endif /* WITH_DISPLAY */

	setLED0(true);
	setLED1(true);
	#ifdef WITH_SD
	fs_unmount(&mp);
	#endif /* WITH_SD */
	#ifdef WITH_LOGGING
	printk("finished\n");
	#endif /* WITH_LOGGING */
}
