

static const char ERR_TIMEOUT = 231;      //"ERR_DHT_TIMEOUT";
static const char ERR_CRC = 232;           //"ERR_DHT_CRC";


#define DHT_PIN                 GPIO_NUM_5


#define DHT_INIT_TIME_US            18
#define DHT_WAIT_TIME_US            40
#define DHT_TOTAL_WAIT_LOW_US       80
#define DHT_TOTAL_WAIT_HIGH_US      80
#define DHT_DATA_TIMEOUT_US         500


#define DHT_SAMPLE_TIME_US      30


void dht_start_signal(void);
int get_humidity(void);
int get_temperature(void);
int get_temperature_humidity(int* temp, int* humdity);
int read_data_raw(int *data, int len);
int read_raw_byte(void);