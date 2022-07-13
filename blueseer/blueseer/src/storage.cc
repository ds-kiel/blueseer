// #include "module_config.h"

// #ifdef WITH_SD
// #include <storage/disk_access.h>
// #include <fs/fs.h>
// #include <ff.h>
// #include <string.h>
// #ifdef WITH_DISPLAY
//     #include "display.h"
// #endif /* WITH_DISPLAY */

// 	//SD-Card
// 	static FATFS fat_fs;
// 	/* mounting info */
//     static struct fs_mount_t mp = {
// 		.type = FS_FATFS,
// 		.fs_data = &fat_fs,
// 	};
// 	const char *disk_mount_pt = "/SD:";
// 	const char *dataPath = "/ble_data";
// 	const char *evalPath = "/eval";
// 	bool sd_card_initialized = false;

//     /*
// 	get number of files in dir at given path
// 	*/
// 	int get_file_count(const char *path)
// 	{
// 		int res;
// 		struct fs_dir_t dirp;
// 		static struct fs_dirent entry;

// 		fs_dir_t_init(&dirp);

// 		res = fs_opendir(&dirp, path);
// 		if (res) {
// 			#ifdef WITH_LOGGING
// 			printk("Error opening dir %s [%d]\n", path, res);
// 			#endif /* WITH_LOGGING */
// 			return -1;
// 		}

// 		int files = 0;
// 		for (;;) {
// 			res = fs_readdir(&dirp, &entry);

// 			if (res || entry.name[0] == 0) {
// 				break;
// 			}

// 			if (entry.type != FS_DIR_ENTRY_DIR) {
// 				files++;
// 			}
// 		}

// 		fs_closedir(&dirp);

// 		return files;
// 	}

// 	/*
// 	creates dir at given path if path exists
// 	*/
// 	int createDir(const char *path)
// 	{
// 		struct fs_dir_t dir_data;

// 		fs_dir_t_init(&dir_data);

// 		int dir_err = fs_opendir(&dir_data, path);
// 		//check if dir already exists at path
// 		if (dir_err) {
// 			fs_mkdir(path);
// 			#ifdef WITH_LOGGING
// 			printk("created dir: %s\n", path);
// 			#endif /* WITH_LOGGING */
// 		}
// 		fs_closedir(&dir_data);

// 		return get_file_count(path);
// 	}

// 	/*
// 	setup SD-card and create dirs to save data samples in
// 	*/
// 	void initSDCard()
// 	{
// 		do {
// 			static const char *disk_pdrv = "SD";
// 			uint64_t memory_size_mb;
// 			uint32_t block_count;
// 			uint32_t block_size;

// 			if (disk_access_init(disk_pdrv) != 0) {
// 				#ifdef WITH_LOGGING
// 				printk("Storage init ERROR!");
// 				#endif /* WITH_LOGGING */
// 				break;
// 			}

// 			if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
// 				#ifdef WITH_LOGGING
// 				printk("Unable to get sector count");
// 				#endif /* WITH_LOGGING */
// 				break;
// 			}
// 			#ifdef WITH_LOGGING
// 			printk("Block count %u", block_count);
// 			#endif /* WITH_LOGGING */

// 			if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
// 				#ifdef WITH_LOGGING
// 				printk("Unable to get sector size");
// 				#endif /* WITH_LOGGING */
// 				break;
// 			}
// 			#ifdef WITH_LOGGING
// 			printk("Sector size %u\n", block_size);
// 			#endif /* WITH_LOGGING */

// 			memory_size_mb = (uint64_t)block_count * block_size;
// 			#ifdef WITH_LOGGING
// 			printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
// 			#endif /* WITH_LOGGING */
// 		} while (0);

// 		mp.mnt_point = disk_mount_pt;

// 		int res = fs_mount(&mp);

// 		if (res == FR_OK) {
// 			#ifdef WITH_LOGGING
// 			printk("Disk mounted.\n");
// 			#endif /* WITH_LOGGING */

// 		} else {
// 			#ifdef WITH_LOGGING
// 			printk("Error mounting disk.\n");
// 			#endif /* WITH_LOGGING */
// 			return;
// 		}

// 		//create dirs for data samples, eval data and times of the day

// 		char data_path[50];
// 		strcpy(data_path, disk_mount_pt);
// 		strcat(data_path, dataPath);

// 		char eval_path[50];
// 		strcpy(eval_path, disk_mount_pt);
// 		strcat(eval_path, evalPath);

// 		#ifdef WITH_LOGGING
// 		printk("files in %s: %d\n", data_path, createDir(data_path));
// 		printk("files in %s: %d\n", eval_path, createDir(eval_path));
// 		#endif /* WITH_LOGGING */

// 		for (int i = 0; i < DAYTIME_COUNT; i++) {
// 			char dir_day_path[50];
// 			strcpy(dir_day_path, eval_path);
// 			strcat(dir_day_path, "/");
// 			strcat(dir_day_path, daytimes[i]);

// 			createDir(dir_day_path);

// 			char dir_day_data_path[50];
// 			strcpy(dir_day_data_path, data_path);
// 			strcat(dir_day_data_path, "/");
// 			strcat(dir_day_data_path, daytimes[i]);

// 			createDir(dir_day_data_path);
// 		}

// 		sd_card_initialized = true;
// 	}

// 	/*
// 	check if file at given path exists
// 	return >=0 if file exists
// 	*/
// 	int fileExists(struct fs_file_t *file, char *path)
// 	{
// 		fs_file_t_init(file);
// 		int rc;
// 		rc = fs_open(file, path, FS_O_RDWR);
// 		if (rc >= 0) {
// 			fs_close(file);
// 		}

// 		return rc;
// 	}

// 	/*
// 	open or create file at given path
// 	*/
// 	int openOrCreateFile(struct fs_file_t *file, char *path)
// 	{
// 		fs_file_t_init(file);
// 		int rc;
// 		rc = fs_open(file, path, FS_O_CREATE | FS_O_RDWR);
// 		#ifdef WITH_LOGGING
// 		if (rc < 0 && rc != -2) {
// 			//error while creating/opening file
// 			printk("FAIL: open %s: %d\n", path, rc);
// 		}
// 		#endif /* WITH_LOGGING */

// 		return rc;
// 	}

// 	/*
// 	search for file name that doesnt exist (by increasing index number)
// 	write data sample string (data_str) to CSV file
// 	*/
// 	void writeDataFile()
// 	{
// 		struct fs_file_t dataFile;

// 		int rc;

// 		char filePath[100];

// 		//create directory if new experiment started
// 		if (strcmp(current_dir, "") == 0){
// 			unsigned int dir_count = 0;
// 			while (1) {
// 				char current_env_str[20];
// 				strcpy(current_env_str, environments[current_environment]);

// 				char first[2];
// 				sprintf(first, "%c", current_env_str[0]);

// 				char second[2];
// 				sprintf(second, "%c", current_env_str[1]);

// 				char count_str[20];
// 				sprintf(count_str, "/%s%s%d.csv", first, second, dir_count);

// 				char dataDirPath[50];
// 				strcpy(dataDirPath, disk_mount_pt);
// 				strcat(dataDirPath, dataPath);	
// 				strcat(dataDirPath, "/");
// 				strcat(dataDirPath, daytimes[current_daytime]);
// 				strcat(dataDirPath, count_str);

// 				#ifdef WITH_LOGGING
// 				printk("data dir path: %s\n", dataDirPath);
// 				#endif /* WITH_LOGGING */

// 				struct fs_dir_t dir_data;

// 				fs_dir_t_init(&dir_data);

// 				int dir_err_data = fs_opendir(&dir_data, dataDirPath);
// 				#ifdef WITH_CLASSIFICATION
				
// 					char evalDirPath[50];
// 					strcpy(evalDirPath, disk_mount_pt);
// 					strcat(evalDirPath, evalPath);	
// 					strcat(evalDirPath, "/");
// 					strcat(evalDirPath, daytimes[current_daytime]);
// 					strcat(evalDirPath, count_str);

// 					#ifdef WITH_LOGGING
// 					printk("eval dir path: %s\n", evalDirPath);
// 					#endif /* WITH_LOGGING */

// 					struct fs_dir_t dir_eval;

// 					fs_dir_t_init(&dir_eval);

// 					int dir_err_eval = fs_opendir(&dir_eval, evalDirPath);
// 					//check if dir already exists at path
// 					if (dir_err_data && dir_err_eval) {
// 						fs_mkdir(dataDirPath);
// 						fs_mkdir(evalDirPath);
// 						#ifdef WITH_LOGGING
// 						printk("created dir: %s\n", dataDirPath);
// 						printk("created dir: %s\n", evalDirPath);
// 						#endif /* WITH_LOGGING */
// 						strcpy(current_dir, count_str);
// 						fs_closedir(&dir_data);
// 						fs_closedir(&dir_eval);
// 						break;
// 					}
// 					fs_closedir(&dir_eval);

// 				#else
// 					//check if dir already exists at path
// 					if (dir_err_data) {
// 						fs_mkdir(dataDirPath);
// 						#ifdef WITH_LOGGING
// 						printk("created dir: %s\n", dataDirPath);
// 						#endif /* WITH_LOGGING */
// 						strcpy(current_dir, count_str);
// 						fs_closedir(&dir_data);
// 						break;
// 					}
// 				#endif /* WITH_CLASSIFICATION */
// 				fs_closedir(&dir_data);

// 				dir_count++;
// 			}
// 		}

// 		#if !defined(WITH_CLASSIFICATION) && defined(WITH_DISPLAY)
// 		setDisplayText("Saving...");
// 		#endif /* !WITH_CLASSIFICATION && WITH_DISPLAY */

// 		//find file name that doesnt exist
// 		while (1) {
// 			char current_env_str[20];
// 			strcpy(current_env_str, environments[current_environment]);

// 			char first[2];
// 			sprintf(first, "%c", current_env_str[0]);

// 			char second[2];
// 			sprintf(second, "%c", current_env_str[1]);

// 			char count_str[20];
// 			sprintf(count_str, "/%s%s%d.csv", first, second, data_file_count);

// 			char dataFilePath[50];
// 			strcpy(dataFilePath, disk_mount_pt);
// 			strcat(dataFilePath, dataPath);	
// 			strcat(dataFilePath, "/");
// 			strcat(dataFilePath, daytimes[current_daytime]);
// 			strcat(dataFilePath, "/");
// 			strcat(dataFilePath, current_dir);
// 			strcat(dataFilePath, count_str);

// 			#ifdef WITH_LOGGING
// 			printk("data file path: %s\n", dataFilePath);
// 			#endif /* WITH_LOGGING */

// 			rc = fileExists(&dataFile, dataFilePath);

// 			if (rc == -2) {
// 				rc = openOrCreateFile(&dataFile, dataFilePath);
// 				fs_seek(&dataFile, 0, FS_SEEK_SET);
// 				strcpy(filePath, dataFilePath);
// 				data_file_count++;
// 				break;
// 			}

// 			data_file_count++;
// 		}
		

// 		fs_write(&dataFile, data_str, strlen(data_str) * sizeof(char));
// 		fs_sync(&dataFile);

// 		fs_close(&dataFile);

// 		static struct fs_dirent entry;
// 		fs_stat(filePath, &entry);
// 		#ifdef WITH_LOGGING
// 		printk("created [FILE] %s (size = %zu)\n", entry.name, entry.size);
// 		#endif /* WITH_LOGGING */
// 		#if !defined(WITH_CLASSIFICATION) && defined(WITH_DISPLAY)
// 		setDisplayText("Scanning...");
// 		#endif /* !WITH_CLASSIFICATION && WITH_DISPLAY */
// 	}

//     void sd_unmount(void) {
//         fs_unmount(&mp);
//     }

//     void write_to_file(struct fs_file_t* env_file, char* data) {
//         fs_seek(env_file, 0, FS_SEEK_END);
//         fs_write(env_file, data, strlen(data) * sizeof(char));
//         fs_sync(env_file);
//         fs_close(env_file);
//     }

//     const char* get_disk_mount_pt() {
//         return disk_mount_pt;
//     }
//     const char* get_data_path() {
//         return dataPath;
//     }
//     const char* get_eval_path() {
//         return evalPath;
//     }


// #endif /* WITH_SD */