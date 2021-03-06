/*
* SD_FileUtils.c
*
* Created: 10/21/2016 2:30:59 PM
*  Author: tim.anderson
*/
#include "SD_FileUtils/SD_FileUtils.h"
#include "DS1302/DS1302.h"


static void SD_FileCreateWithHeader(const struct LoggerConfig *loggerConf);
static bool CheckAndFixLastFileLineIntegrity(const uint16_t expectedValues);


#ifdef SD_FILE_UTILS_UNIT_TEST
#define SD_DEBUG_FILE "0:debug.txt"
static FRESULT SD_UnitTestRemoveFile(void);
static FRESULT SD_UnitTestCreateDebugFile(FIL *file);
static FRESULT SD_UnitTestReadFile(FIL *file);
static int SD_UnitTestLoadAndCheckDebugFile(const char *addresses, uint8_t numAddresses, const char *interval, bool defer);
static void SD_UnitTestHeaderCreate(struct LoggerConfig *config, const char *headerFileName);
static bool SD_UnitDataFileIntegrityCheck(void);
#endif

/*SdCardInit
Initializes the sd mmc connection, and attempts to mount the fileSystem.
system will be mounted into first arg, fatFilesys.
second argument, fileResult, will show the result state of the attempted mount.
*/
void SdCardInit(FATFS *fatFileSys)
{
	//TODO: add checking for replugging card
	FRESULT res;
	sd_mmc_init();
	Ctrl_status checkStatus;
	do{
		checkStatus = sd_mmc_test_unit_ready(0);
	} while (checkStatus != CTRL_GOOD);
	
	memset(fatFileSys, 0, sizeof(FATFS));
	res = f_mount(SD_VOLUME_NUMBER, fatFileSys);
	if(res !=  FR_OK){
		checkStatus = CTRL_FAIL;
	}
}

/* tryReadTimeFile
attempts to read the time file, whose filename is defined in boardconf.h.
File should be in format hh,mm,ss,mm,dd,yyyy
comma is suggested, but any non-numeric, non +- non \n, character should work as a separator.

returns false if file didn't exist, or there was an error.
*/
void TryReadTimeFile(void){
	struct Ds1302DateTime dateTime;

	FIL fileObj;
	//see if we can talk to the SD card
	FRESULT status = f_open(&fileObj, SD_TIME_FILENAME, FA_READ | FA_OPEN_EXISTING);
	if(status == FR_OK){
		//reads until \n in found, or until buff is filled
		UINT numBytesRead;
		const uint8_t timeBufferLen = 17;
		char buf[timeBufferLen];
		f_read(&fileObj, buf, timeBufferLen, &numBytesRead);
		f_close(&fileObj);
		if(numBytesRead >= timeBufferLen){
			
			//convert two digit strings to values.
			dateTime.hours = (buf[1]- '0') + ((buf[0]- '0')*10);
			dateTime.minutes = (buf[4]- '0') + ((buf[3]- '0')*10);
			dateTime.seconds = (buf[7]- '0') + ((buf[6]- '0')*10);
			dateTime.month = (buf[10]- '0') + ((buf[9]- '0')*10);
			dateTime.date = (buf[13]- '0') + ((buf[12]- '0')*10);
			dateTime.year = (buf[16]- '0') + ((buf[15]- '0')*10);
			
			//set the time
			Ds1302SetDateTime(&dateTime);
		}
	}
}

/* SD_CreateWithHeaderIfMissing
Function determines if the datalog file exists. If it doesn't,
the function calls to SD_FileCreatWithHeader
return true if header was created or file already existed.*/
void SD_CreateWithHeaderIfMissing(const struct LoggerConfig *loggerConfig)
{
	FIL file;
	FRESULT statsResult = f_open(&file, SD_DATALOG_FILENAME, FA_READ | FA_OPEN_EXISTING);
	f_close(&file);
	if (statsResult == FR_NO_FILE){
		SD_FileCreateWithHeader(loggerConfig);
	}
}


static void SD_FileCreateWithHeader(const struct LoggerConfig *loggerConf)
{
	//enable the bod. if we're in a brown out state currently, don't even try to create the header,
	//it would risk SD card corruption.
	bod_enable(BOD_BOD33);
	bool brownoutState = bod_is_detected(BOD_BOD33);
	bod_disable(BOD_BOD33);
	if(brownoutState){
		return;
	}
	FIL file;
	f_open(&file, SD_DATALOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE);
	//create the header for the data file
	f_puts("Date,Time,TcBefore1,TcBefore2,TcBefore3,TcBefore4,TcAfter1,TcAfter2,TcAfter3,TcAfter4,Dht1Temp,Dht1Rh,Dht2Temp,Dht2Rh,Dend1,Dend2", &file);
	char sdiColumnHeader[11] = ",SDI12_A.00";
	const uint8_t sdiHeaderAddressIndex = 7;
	const uint8_t sdiHeaderOnesValueIndex = 10;
	
	for(uint8_t sdiCounter = 0; sdiCounter < loggerConf->numSdiSensors; sdiCounter++){
		//reset the value index in the header
		sdiColumnHeader[sdiHeaderOnesValueIndex] = '0';
		sdiColumnHeader[sdiHeaderOnesValueIndex - 1] = '0';
		//slot the address character into the header.
		sdiColumnHeader[sdiHeaderAddressIndex] = loggerConf->SDI12_SensorAddresses[sdiCounter];
		
		//set the value indexes, and write to the data file.
		for(uint8_t valueCounter = 0;valueCounter < loggerConf->SDI12_SensorNumValues[sdiCounter]; valueCounter++){
			//we want to show the values on the header 1 indexed.
			uint8_t oneIndexedValueCounter = valueCounter + 1;
			//set the ones, then the tens.
			sdiColumnHeader[sdiHeaderOnesValueIndex] = '0' + (oneIndexedValueCounter % 10);
			sdiColumnHeader[sdiHeaderOnesValueIndex - 1] = '0' + (oneIndexedValueCounter / 10);
			f_puts(sdiColumnHeader, &file);
		}
	}
	f_close(&file);
}

/*readConfigFile
Reads the Configuration file, and stores the configuration in the given struct.
Config file is formated as defined below:

0000
DT
ABC...
9,9,9...

first line:
0000 is the number of minutes between readings that the sensor will sleep

second line:
DT
first character may be letter i or d, and specifies if the sensor takes the reading immediately on waking up(i), or defers logging until after the sleep interval(d).
this character is not case sensitive.
second character specifies the type of thermocouples used. Acceptable characters are any of the following: BEJKNRST .
This character is not case sensitive.

Third Line
ABC.. specifies the SDI12 addresses of the sensors. Thus, if logger is connected to 3 sensors, with addresses 0,7, and B, line 2 may read
07B

Fourth line:
9,9,9,... specifies the SDI12 number of values for each of the sdi sensors addressed in the Third line.
*/
void ReadConfigFile(struct LoggerConfig *config){
	//set config defaults
	config->loggingInterval = 3600; //1 hour
	config->logImmediately = false;
	config->numSdiSensors = 0;
	config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_K;
	
	FIL fileObj;
	char intervalBuffer[5];
	char flagBuffer[4];
	const uint16_t numValuesBufferSize = SDI12_MAX_SUPPORTED_SENSORS * 4;
	char numValuesBuffer[numValuesBufferSize];
	memset(intervalBuffer, 0, 5 * sizeof(char));
	memset(flagBuffer, 0, 4 * sizeof(char));
	memset(config->SDI12_SensorAddresses, 0, sizeof(char) * (SDI12_MAX_SUPPORTED_SENSORS + 1));
	
	if(f_open(&fileObj, SD_CONFIG_FILENAME, FA_READ) == FR_OK){
		f_gets(config->SDI12_SensorAddresses, SDI12_MAX_SUPPORTED_SENSORS, &fileObj);
		f_gets(numValuesBuffer, numValuesBufferSize, &fileObj);
		f_gets(intervalBuffer, 6, &fileObj);
		f_gets(flagBuffer, 4, &fileObj);
		f_close(&fileObj);
		
		char *ptrToNumValuesBuffer = &(numValuesBuffer[0]);
		//count up the number of sdi sensors, and convert the numValues into ints for the config
		config->numSdiSensors = 0;
		while(config->SDI12_SensorAddresses[config->numSdiSensors] >= '0'){//the only way the sdi sensor can be valid is if it's ASCII value is over '0'
			config->SDI12_SensorNumValues[config->numSdiSensors] =  strtol(ptrToNumValuesBuffer, &ptrToNumValuesBuffer, 10);
			ptrToNumValuesBuffer++;	//go past the comma, or into  the line feed (value 10)
			config->numSdiSensors++;
		}
		
		//convert the interval to an integer to store in our config struct.
		char *ptrToIntervalBuffer = &(intervalBuffer[0]);
		config->loggingInterval = strtol(ptrToIntervalBuffer, &ptrToIntervalBuffer, 10);
		
		if(flagBuffer[0] == 'd' || flagBuffer[0] == 'D'){
			config->logImmediately = false;
		}
		else{
			config->logImmediately = true;
		}
		switch(flagBuffer[1]){
			case 'b': case 'B':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_B; break;
			case 'e': case 'E':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_E; break;
			case 'j': case 'J':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_J; break;
			case 'k': case 'K':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_K; break;
			case 'n': case 'N':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_N; break;
			case 'r': case 'R':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_R; break;
			case 's': case 'S':
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_S; break;
			//T is the only one left, so set that as default
			default:
			config->thermocoupleType = MAX31856_THERMOCOUPLE_TYPE_T;
		}
	}
}

/*SD_CheckIntegrity
If the logger has a reason to think that the previous log was interrupted, this function may be called.
Function determines how many values should be in each line, and passes that information to CheckAndFixLastFileLineIntegrity
to potentiallyf fix integrity issues.*/
bool SD_CheckIntegrity(const struct LoggerConfig *loggerConfig){
	//count up the number of SDI values we're expecting
	uint16_t expectedValues = 14; //start with 14 values, we'll add more for the SDI12s
	uint8_t sdiIndex = loggerConfig->numSdiSensors;
	while(sdiIndex){
		expectedValues += loggerConfig->SDI12_SensorNumValues[--sdiIndex];
	}
	return CheckAndFixLastFileLineIntegrity(expectedValues);
}

/*CheckAndFixLastFileLineIntegrity
takes a number of values expected to be in each line of the data log file.
If the last line has fewer than this many values, function will write NANs until there are as many values as needed.*/
static bool CheckAndFixLastFileLineIntegrity(const uint16_t expectedValues)
{
	//open the data file, and start checking.
	FIL file;
	f_open(&file, SD_DATALOG_FILENAME, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
	char buffer[256];
	
	uint16_t numCommasFound = 0;
	do{
		f_gets(buffer, 256, &file);
		for(uint16_t bufferIndex = 0; bufferIndex < 256; bufferIndex++){
			
			if(buffer[bufferIndex] == '\n'){
				numCommasFound = 0;
				break;
			}
			
			else if(buffer[bufferIndex] == 0){
				//if we got a 0, AND it's the end of the file without a newline, the datafile is probably invalid.
				//try to fix it.
				if(f_eof(&file)){
					//add enough NaNs, and end the line.
					while(expectedValues > ++numCommasFound)	f_puts(",NaN", &file);
					f_putc('\n', &file);
					f_close(&file);
					return false;
				}
				//if it's not the end of the file, but we encountered a 0, that means that we ran out of buffer.
				//we're not going to clear numCommasFound, but loop again to grab another buffer.
				else {
					break;
				}
			}
			
			else if(buffer[bufferIndex] == ','){
				numCommasFound++;
			}
		}
	}while(!f_eof(&file));
	
	f_close(&file);
	return true;
}

#ifdef SD_FILE_UTILS_UNIT_TEST
int8_t SD_UnitTest(void)
{
	FATFS fatfs;
	SdCardInit(&fatfs);
	
	SD_UnitDataFileIntegrityCheck();
	return 0;
}

static void SD_UnitTestHeaderCreate(struct LoggerConfig *config, const char *headerFileName){
	f_unlink(SD_DATALOG_FILENAME);
	FIL file;
	f_open(&file,headerFileName, FA_CREATE_ALWAYS | FA_WRITE);
	
	SD_FileCreateWithHeader(config);
	f_close(&file);
}

static int SD_UnitTestLoadAndCheckDebugFile(const char *addresses, uint8_t numAddresses, const char *interval, bool defer){
	FIL file;
	FRESULT result;
	result = f_open(&file,SD_CONFIG_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
	if(result != FR_OK){
		return -1;
	}
	
	f_puts(addresses, &file);
	f_putc('\n', &file);
	f_puts(interval, &file);
	f_putc('\n', &file);
	if(defer){
		f_puts("d", &file);
	}
	else{
		f_puts("i", &file);
	}
	f_putc('\n',&file);
	f_close(&file);

	struct LoggerConfig config;
	ReadConfigFile(&config);
	
	if(config.numSdiSensors != numAddresses){
		return -5;
	}
	for(int x = 0; x < numAddresses;x++){
		if(config.SDI12_SensorAddresses[x] != addresses[x]){
			return -6;
		}
		if(config.SDI12_SensorNumValues[x] != 0){
			return -7;
		}
	}
	int intParse = atoi(interval);
	if(config.loggingInterval != intParse){
		return -8;
	}
	if(config.logImmediately == defer){
		return -9;
	}
	return 1;
}

static FRESULT SD_UnitTestCreateDebugFile(FIL *file){
	FRESULT res;
	do{
		res = f_open(file, SD_DEBUG_FILE, FA_CREATE_ALWAYS | FA_WRITE);
	}while(res == FR_NOT_READY);
	if(res != FR_OK){
		return res;
	}
	const char *message = "debug message\n w space start\n\n!@#%%^*(%^";
	size_t messageLen = strlen(message);
	uint16_t charsWritten = 0;
	charsWritten = f_puts("debug message\n w space start\n\n!@#%%^*(%^", file);
	f_sync(file);
	if(charsWritten < messageLen - 1){
		return res;
	}
	res = f_close(file);
	if(res != FR_OK){
		return res;
	}
	
	return res;
}

static FRESULT SD_UnitTestReadFile(FIL *file){
	FRESULT res = f_open(file, SD_DEBUG_FILE,FA_READ);
	if(res != FR_OK){
		return res;
	}
	char buffer[256];
	memset(buffer, 0, 256);
	f_gets(&buffer[0],256, file);
	uint8_t len = 0;
	while(buffer[len] != 0){
		len++;
	}
	if(len != 14){
		return FR_TOO_MANY_OPEN_FILES;
	}
	
	memset(buffer, 0, 256);
	f_gets(&buffer[0], 256, file);
	len = 0;
	while(buffer[len] != 0){
		len++;
	}
	if(len != 15){
		return FR_TOO_MANY_OPEN_FILES;
	}
	
	memset(buffer, 0, 256);
	f_gets(&buffer[0], 256, file);
	len = 0;
	while(buffer[len] != 0){
		len++;
	}
	if(len != 1)
	{
		return FR_TOO_MANY_OPEN_FILES;
	}
	
	memset(buffer, 0, 256);
	f_gets(&buffer[0], 256, file);
	len = 0;
	while(buffer[len] != 0){
		len++;
	}
	if(len != 10){
		return FR_TOO_MANY_OPEN_FILES;
	}
	
	res = f_close(file);
	if(res != FR_OK){
		return res;
	}
	
	return res;
}

static FRESULT SD_UnitTestRemoveFile(void){
	FILINFO info;
	FRESULT res = f_stat(SD_DEBUG_FILE, &info);
	if(res == FR_NO_FILE){
		return res;
	}
	
	res = f_unlink(SD_DEBUG_FILE);
	if(res == FR_OK){
		return res;
	}
	
	
	res = f_stat(SD_DEBUG_FILE, &info);
	if(res != FR_NO_FILE){
		return res;
	}
	return res;
}

static bool SD_UnitDataFileIntegrityCheck(void){
	const char *dataFileMessage = "Date,Time,TcPort1,TcPort2,TcPort3,TcPort4,Dht1Temp,Dht1Rh,Dht2Temp,Dht2Rh\n1,2,3,4,5,6,7,8,9,10";
	FIL file;
	f_open(&file, SD_DATALOG_FILENAME, FA_WRITE| FA_CREATE_ALWAYS);
	f_puts(dataFileMessage, &file);
	f_close(&file);
	f_open(&file, SD_DATALOG_FILENAME, FA_READ | FA_WRITE);
	bool success = CheckAndFixLastFileLineIntegrity(10);
	f_puts("checked",&file);
	f_close(&file);
	return success;
}
#endif