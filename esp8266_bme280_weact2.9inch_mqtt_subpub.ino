/**
* @file bme280.ino
* @author JayJayGas
* @brief sends output of bme280 via mqtt
* 
* Prints last value from subscribed topics
* every hour to an e-paper screen. GxEPD2 library using
* GxEPD2_290_C90c (model GDEM029C90 128x296, SSD1680)
* works with WeAct display. GxEPD2 uses
* Adafruit GFX Library for text and graphics. Hover mouse
* over a function and the library GxEPD2 indicates if the func
* is from Adafruit. Adafruit uses origin x,y 0,0 as top left
* of image.
*
* E-Paper details:
* 2.9'' WeACT E-paper red and black:
*       Note: Cannot do partial refresh!
*
* ESP8266 pinout: CS(SS)=15,SCL(SCK)=14,SDA(MOSI)=13,BUSY=16,RES(RST)=5,DC=4
* 
* Script Logic:
*   The best way to organise the logic for the E-paper, is to write functions to calculate the positions
*   and dimensions of objects relative to the screen. In this case it's ordered like:
*   Screen dimensions --> Box --> box quadrant --> text
*   This means the text is fixed to the box which is fixed to the screen dimensions. They now all move
*   together with whatever input (text) is put into the program (unless the text is too long theres no wrapping).
*   Once you calculate the dimensions/positions of objects, return the values as something (struct in this code)
*   and print all the objects out together in one function. In this case print_data() handles this.
*   
*   So to get that to work there is a function that turns the screen into a 2x2 grid (ret_grid_size()) and
*   another function that turns that screen grid into a smaller 2x2 grid (ret_grid_box_size()). This then allows
*   easy alignment of text into screen quadrants, and then further alignment into those quadrants with another
*   set of quadrants. Finally this gives you 2(2x2) grids to align things.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ESP8266WiFi.h>
///////////////////Modules///////////////////////////
#include <PubSubClient.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/Picopixel.h>
///////////////////Complex Defs///////////////////////
//Don't change these unless you know what you are doing.
//Please note: the addition of more sensors will require the
//GRID_ARRAY_SIZE to increase and therefore all global
//strings for this will need more headings to display properly.
#define ONE_HOUR (3.6E6)  //1 h in miliseconds
//change to increase/decrease grid array sizes for
//ret_grid_size() and ret_grid_box_size()
#define GRID_ARRAY_SIZE 4   
#define NUMBER_OF_SENSORS 3         //How many BME sensors are expected to get data from?
#define NUMBER_OF_DATA_POINTS_FROM_SENSORS 2  //How many data points from each BME sensors?
///////////////////Defs//////////////////////////////
//Change these for your purposes.
#define TOPIC_OUT ""       //topic for the message this unit will send to the broker.
#define TOPIC_IN ""        // topic for the message this unit will recieve from the broker.
#define TEXT_SPACING 5     //text paragraph spacing as pixels.
///////////////////Proto////////////////////////////
void* xmalloc(size_t size);
int setup_wifi(void);
int setup_mqtt(void);
int mqtt_callback(char* topic, byte* payload, unsigned int length);
int mqtt_publish(const char* topic, const char* string);
int esp_wifi_sleep(int sleep_time);
int _small_text_init(void);
int screen_setup(void);
struct shape_size background_square(void);
int print_screen_all();
struct shape_size ret_grid_size(uint16_t grid_width, uint16_t grid_height);
struct shape_size* ret_grid_box_size(struct shape_size grid);
struct shape_size background_square_coords(void);
struct shape_size* text_size(const char* string_array[], uint16_t array_size);
///////////////////Init/////////////////////////////
//2.9" WeAct ePaper (3 colour):
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(/*CS=*/15, /*DC=*/4, /*RES=*/5, /*BUSY=*/10));  // GDEM029C90 128x296, SSD1680
//and 4" WeAct ePaper (3 colour):
//GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT> display(GxEPD2_420c_GDEY042Z98(/*CS=*/15, /*DC=*/4, /*RES=*/5, /*BUSY=*/10));  // 400x300, SSD1683

WiFiClient espClient;
PubSubClient client(espClient);
///////////////////STRUCT/////////////////////////////
struct shape_size {//consistantly used for every object to store positional data.
  uint16_t height;
  uint16_t width;
  uint16_t x[GRID_ARRAY_SIZE];
  uint16_t y[GRID_ARRAY_SIZE];
};
///////////////////GLOB VAR/////////////////////////
const char* display_headings[NUMBER_OF_SENSORS + 1] = { //takes 1 string for each sensor + 1 string at the end for time.
                                        "|Bedroom|",
                                        "|Downstairs|",
                                        "|Upstairs|",
                                        "|Time|"};  //last heading will have no TEXT_data_display_headings printed underneath.
uint16_t display_headings_array_size = NUMBER_OF_SENSORS + 1;
const char* data_headings[NUMBER_OF_DATA_POINTS_FROM_SENSORS] =
                                            {"Temp: ",
                                            "Humidity: "};
uint16_t data_headings_array_size = NUMBER_OF_DATA_POINTS_FROM_SENSORS;
//mqtt + wifi setup
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* mqtt_user = "";
const char* mqtt_pass = "";
const char* clientID = "";
const int mqtt_port = 1883;
//////////////GLOB DATA VAR/////////////////////////
const char* data_array[NUMBER_OF_SENSORS*NUMBER_OF_DATA_POINTS_FROM_SENSORS+2]; // MQTT string stored here. See MQTT callback func.
///////////////////MAIN/////////////////////////////
void setup(void) {
  //screen setup
  display.init(115200, true, 50, false);
  setup_wifi();
  setup_mqtt();
  screen_setup();
  display.hibernate();
  //Function called when MQTT message recieved
  client.setCallback(mqtt_callback);
}

void loop(void) {
  //check if wifi/mqtt connected
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
    delay(10000);
    }
  if (!client.connected()) {
    setup_mqtt();
    delay(5000);
    client.loop();
  }

  //subscribe to data, send '0', get csv data back.
  if (client.connected()) {
    //subscribe to response
    client.subscribe(TOPIC_IN);
    delay(2000);
    //tell client you'd like data
    mqtt_publish(TOPIC_OUT, "0");
    //wait i*0.5 seconds, but keep client active by calling
    //client.loop() every 5 seconds while waiting.
    Serial.println("Listening for Response");
    for (int i=0; i<10; i++){
      client.loop();
      delay(1000);
    }
  }
}
////////////////////SETUP FUNCS//////////////////////////
/*****************************************************
* @brief wifi setup
* @note make sure you make the IP static when you get
        the printed local IP
*****************************************************/
int setup_wifi(void) {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed");
    return 1;
  } else {
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());  //set static at router!
    return 0;
  }
}

/*****************************************************
* @brief MQTT setup
* @return 0 or 1
*****************************************************/
int setup_mqtt(void) {
  //mqtt check if client exists (bool client.connect = true)
  client.setServer(mqtt_server, mqtt_port);
  if (client.connect(clientID, mqtt_user, mqtt_pass)) {
    Serial.println("MQTT broker connected");
    return 0;
  } else {
    Serial.println("MQTT broker failed to connect");
    return 1;
  }
}

/*****************************************************
* @brief MQTT publish string
* @param topic: The topic as a string
* @param string: The message as a string
* @return 0
*****************************************************/
int mqtt_publish(const char* topic, const char* string) {
  if (client.publish(topic, string)) {
    Serial.print("Data sent: ");
    Serial.println(string);
  }
  return 0;
}

/*****************************************************
* @brief MQTT callback
*
* Data from client is in the form of csv:
*   Temp, Humidity, ..., Hours:Minutes, Days:Month:Year
* The data string therefore looks like this for 3 sensors.
*   "15.87 *C,43.23%,17.98 *C,43.38%,16.46 *C,41.73%,12:29,26/06/25"
*
* Callback takes CSV data and puts it into array.
* This array is called into print_screen_all() which prints
* all the data to the screen
* 
* @param char* topic: topic
* @param byte* payload: message
* @param unsigned int length: length of data
* @return 0
*****************************************************/
int mqtt_callback(char* topic, byte* payload, unsigned int length) {
  //***STRING COLLECTION START****
  //collect data into a string
  char message[length];
  Serial.println("Data recieved");
  Serial.print(topic);
  Serial.print(",");
  Serial.print(" ");
  //convert bytes to string
  for (uint16_t i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';  //null terminate string
  Serial.println(message);     //print string
  //***STRING COLLECTION END****

  //save all data to an array for later. Use strtok to remove ",".
  /***DATA SEPERATION START****/
  char* ret;
  const char delimeter[2] = ",";
  ret = strtok(message, delimeter);
  if (!ret){Serial.println("bad or no data, Check if sensor data in CSV format.");}
  //assign each strtok ret to val_array.
  uint16_t i = 0;
  while (ret != NULL){
    data_array[i] = ret;
    Serial.print(data_array[i]); Serial.print(",");
    i++;
    ret = strtok(NULL, ",");      //next csv value
  }
  /***DATA SEPERATION END****/

  //Print all data to the screen and sleep for 1 hour.
  print_screen_all();
  display.hibernate();
  esp_wifi_sleep(ONE_HOUR);
  return 0;
}
////////////////////Custom Funcs////////////////////////
/*****************************************************
* @brief ESP sleep, followed by wifi/MQTT reconnect
* @note Pins D0 and RST must be connected for this function
*       to work.
* @return 0
*****************************************************/
int esp_wifi_sleep(int sleep_time) {
  Serial.println("Going to sleep for 1 hour.");
  WiFi.forceSleepBegin();
  delay(sleep_time);
  WiFi.forceSleepWake();
  setup_wifi();
  setup_mqtt();
  return 0;
}

/*****************************************************
* @brief Initialise text for ePaper
* @note Set font here. Make sure to add the header file
        for the font too.
* @return 0
*****************************************************/
int _small_text_init(void) {
  display.setRotation(1);
  display.setFont(&Picopixel);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  return 0;
}

/*****************************************************
* @brief Setup E-paper
* @return 0
*****************************************************/
int screen_setup(void) {
  //use char array pointer for text
  const char* lines[] = { "Display Active",
                          "Starting data collection",
                          "*************************" };
  //get number of items in array
  int num_lines = sizeof(lines) / sizeof(lines[0]);
  //coord vars
  int16_t tbx; int16_t tby;
  uint16_t tbw; uint16_t tbh;
  uint16_t x; uint16_t y;
  uint16_t y_prev;
  //display setup
  _small_text_init();
  //do while loop for text in lines array
  display.firstPage();
  do {
    //clear screen.
    display.fillScreen(GxEPD_WHITE);
    //print first line of text.
    display.getTextBounds(lines[0], 0, 0, &tbx, &tby, &tbw, &tbh);
    y = ((display.height() - tbh) / 2);
    x = ((display.width() - tbw) / 2);
    display.setCursor(x, y);
    display.print(lines[0]);
    //lines are now red.
    display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
    //iterate over array to print remaining lines.
    for (int i = 1; i < num_lines; i++) {
      //get text bounds/box
      display.getTextBounds(lines[i], 0, 0, &tbx, &tby, &tbw, &tbh);
      y_prev = y;
      y = y_prev + tbh + TEXT_SPACING;
      x = ((display.width() - tbw) / 2);
      display.setCursor(x, y);
      display.print(lines[i]);
    }
  } while (display.nextPage());
  return 0;
}
/*****************************************************
* @brief Return a shape in a square grid 
*        as as struct from imput params.
*
* Will take an x/y array, total grid width and height,
* then generate grid parameters in the shape_size struct.
* Change the GRID_ARRAY_SIZE definition to alter the grid
* sizing.
* Grid Numbering increasing in rows:
* |1 | 3|
* |2 | 4|
*
* @param x_array[GRID_ARRAY_SIZE]
* @param y_array[GRID_ARRAY_SIZE]
* @param grid_width
* @param grid_height
* @return struct shape_size
*****************************************************/
struct shape_size ret_grid_size(uint16_t grid_width,uint16_t grid_height){
  //init var
  uint16_t x; uint16_t y;
  uint16_t x_array[GRID_ARRAY_SIZE]; uint16_t y_array[GRID_ARRAY_SIZE];
  uint16_t row_col_num = GRID_ARRAY_SIZE/2; //row/col number
  uint16_t width = grid_width / row_col_num;
  uint16_t height = grid_height / row_col_num;

  //4x4 rectangle grid.
  //x or y = 100%height - (100%height/(number of rows/columns))
  //therefore when j/k=2 (2 columns) y = 50% of screen height
  uint16_t i = 0;  //array position
  do {
    for (uint16_t j = 0; j < row_col_num; j++) {  //col
      y = grid_height - (grid_height / (j + 1));
      for (uint16_t k = 0; k < row_col_num; k++) {  //row
        x = grid_width - (grid_width / (k + 1));
        x_array[i] = x;
        y_array[i] = y;
        Serial.print("grid coord: ");Serial.print(x_array[i]);
        Serial.print(y_array[i]);Serial.print("\n_____END_____\n");
        i++;
      }
    }
  } while (i < GRID_ARRAY_SIZE);

  struct shape_size size;
  size.height = height;
  size.width = width;
  for (uint16_t i = 0; i < GRID_ARRAY_SIZE; i++) {
    size.x[i] = x_array[i];
    size.y[i] = y_array[i];
  }
  return size;  //contains all dimensions of grid subsections
}

/*****************************************************
* @brief Use the grid struct to make a struct array of grids.
*
* Will take the struct from ret_grid_size() (or a manual shape struct) 
* and fsplit the shape into a grid.
* Useful for text alignment within the object you've got on a screen.
* e.g. take your shape shape_size struct, and then use ret_grid_box_size()
* to make a grid in the shape. You can then align text to the grid or other
* shapes by using the struct array returned.
*
* Grid Numbering example:
* |0 | 1|
* |2 | 3|
* Therefore text at the top left would be: struct shape_size shape[i].x[0]/y[0]
* 
* @param struct shape_size grid
* @return struct shape_size[GRID_ARRAY_SIZE]
*****************************************************/
struct shape_size* ret_grid_box_size(struct shape_size shape) {
  struct shape_size* ret_grid_box = (shape_size*)malloc(GRID_ARRAY_SIZE * sizeof(shape_size));
  //generate grid in as array of struct
  for (uint16_t i = 0; i < GRID_ARRAY_SIZE; i++) {
    for (uint16_t j = 0; j < GRID_ARRAY_SIZE; j++) {
      //if it works all the text will go to the top left corner...
      ret_grid_box[i].x[j] = shape.x[i] + ((shape.width / GRID_ARRAY_SIZE)*j);
      ret_grid_box[i].y[j] = shape.y[i] + ((shape.height / GRID_ARRAY_SIZE)*j);
      //printing
      Serial.print("grid coord: ");Serial.print(ret_grid_box[i].x[j]);Serial.print(",");
      Serial.print(ret_grid_box[i].y[j]);Serial.print("\n_____END_____\n");
    }
  }
  return ret_grid_box;
}

/*****************************************************
* @brief Return text sizing as a shape_size struct
* @param const char*: string array pointer
* @param uint16_t: string array size
* @return struct shape_size
*****************************************************/
struct shape_size* text_size(const char* string_array[], uint16_t array_size) {
  //create output struct
  struct shape_size* text_size = (shape_size*)malloc(array_size * sizeof(shape_size));
  //vars for loop
  int16_t tbx; int16_t tby;
  uint16_t tbw; uint16_t tbh;
  uint16_t x; uint16_t y;
  //init screen for display func
  display.setRotation(1);
  display.setFullWindow();
  //write data to struct
  for (int i = 0; i < array_size; i++) {
    //heading text
    display.getTextBounds(string_array[i], 0, 0, &tbx, &tby, &tbw, &tbh);
    text_size[i].width = tbw;
    text_size[i].height = tbh;
    text_size[i].x[0] = tbx;
    text_size[i].y[0] = tby;
  }
  return text_size;
}

////////////////////PRINT FUNCS////////////////////////

/*****************************************************
* @brief Return background square sizing as a shape_size struct
* @return struct shape_size
*****************************************************/
struct shape_size background_square(void) {
  //make a grid of squares 4x4
  uint16_t grid_height = display.height();
  uint16_t grid_width = display.width();
  struct shape_size square = ret_grid_size(grid_width, grid_height);
  return square;
}

/*****************************************************
* @brief prints everything
*
* Will print everything to the E-paper screen using the dimensions
* obtained above in shape_size structs. Uses global var text and data defined
* at top of script to determine what to print. 

* Will print the major headingsthen print sub headings under each major 
* heading excpet for the last heading in TEXT_display_headings which is left blank.
* The last heading in TEXT_display_headings is for time or constant data to make sure
* you know the screen is updating like it should.
*
* This function sucks and should be split into more functions.
* The for-while loop is scary.
* 
* @return 0 or 1
*****************************************************/
int print_screen_all() {
  //init funcs for display (must be run at top of a func!)
  display.setRotation(1);
  display.setFullWindow();
  
  //structs for objects to get dimensional data for easy access
  struct shape_size square_shape = background_square(); //2x2 background grid shape
  struct shape_size* square_shape_align_grid = ret_grid_box_size(square_shape); //text alignment for 'square_shape'
  struct shape_size* display_headings_size = text_size(display_headings,
                                                      display_headings_array_size);
  struct shape_size* data_headings_size = text_size(data_headings,
                                                    data_headings_array_size);
  struct shape_size* data_array_size  = text_size(data_array,
                                            (NUMBER_OF_SENSORS*NUMBER_OF_DATA_POINTS_FROM_SENSORS+2));
  
  //************SCREEN PRINTING START*****
  //note: This screen can't do a partial update so the whole
  //      screen is printed at once. If you screen can do a partial
  //      update you can make a new function, and do a partial update
  //      after this has been run the first time.
  display.firstPage();
  do {
    /**************INIT********************/
    uint16_t array_position = 0; //loop ticker for data_array
    //init text params
    uint16_t x; uint16_t y;
    uint16_t x_prev; uint16_t y_prev;
    _small_text_init();
    /************INIT END********************/
    /**********SHAPES/DRAWINGS**************/
    //print background box/grid
    for (int i = 0; i < GRID_ARRAY_SIZE; i++) {
      display.drawRoundRect(square_shape.x[i], square_shape.y[i], square_shape.width, square_shape.height, 10, GxEPD_BLACK);  //draw the square from the background() func
    }
    /********SHAPES/DRAWINGS END**********/
    /************HEADING TEXT************/
    //large for loop with int i to represent first grid.
    //This i represents box position from square_shape.
    //e.g. i=0 is the top left box of the screen.
    //see background_square() func for details.
    for (int i = 0; i < GRID_ARRAY_SIZE; i++) {
      x = square_shape_align_grid[i].x[0] + TEXT_SPACING;
      y = square_shape_align_grid[i].y[0] + (TEXT_SPACING*2) + (display_headings_size[i].height/2);
      y_prev = y; //save y
      x_prev = x; //save x
      display.setCursor(x, y);
      display.setTextColor(GxEPD_RED);
      display.print(display_headings[i]);
      /************HEADING TEXT END********************/
      /************DATA HEADING************************/
      //splitting grid for i into another grid j. see func 
      //Therefore when j=0 we are talking about the top left corner
      //of the box[i]. Both the TEXT_data_display_headings and data_array
      //are written here. So you get mini-titles with the data as well.
      for (int j = 0; j < data_headings_array_size; j++) {
        x = square_shape_align_grid[i].x[0] + TEXT_SPACING; //reset x after each loop
        if (i==(GRID_ARRAY_SIZE-1)){                      //This is for the last quadrant (time). No mini-title, only data.
          //data text (date data)
          y = y_prev + display_headings_size[i].height + (TEXT_SPACING*1.5);
          y_prev = y; //save y
          display.setCursor(x, y);
          display.setTextColor(GxEPD_BLACK);
          display.print(data_array[(NUMBER_OF_SENSORS*NUMBER_OF_DATA_POINTS_FROM_SENSORS+2)-2]);
          //data text (time data)
          y = y_prev + data_array_size[(NUMBER_OF_SENSORS*NUMBER_OF_DATA_POINTS_FROM_SENSORS+2)-2].height + (TEXT_SPACING*1.5);
          display.setCursor(x, y);
          display.setTextColor(GxEPD_BLACK);
          display.print(data_array[(NUMBER_OF_SENSORS*NUMBER_OF_DATA_POINTS_FROM_SENSORS+2)-1]);
          break; //End all loops
        }
        //if statement for slight adjustment of y text spacing if close to data_headings
        if (j==0){y = y_prev + display_headings_size[i].height + (TEXT_SPACING*1.5);}
        else{y = y_prev + (data_headings_size[j].height) + TEXT_SPACING;}
        y_prev = y; //save y
        x_prev = x; //save x
        display.setCursor(x, y);
        display.setTextColor(GxEPD_RED);
        display.print(data_headings[j]);
        /************DATA HEADING END****************/
        /************DATA TEXT START****************/
        //y already set
        x = x_prev + data_headings_size[j].width + (TEXT_SPACING);
        display.setCursor(x, y);
        display.setTextColor(GxEPD_BLACK);
        display.print(data_array[array_position]);
        array_position++; //go to next array position
        /************DATA TEXT END*****************/
      }
    }
  } while (display.nextPage());
  /****************SCREEN PRINTING END************/
  //malloc free any function with ret_grid_box_size() or text_size()
  //ret_grid_box_size() unnescarily uses malloc. Consider rewriting.
  free(square_shape_align_grid);  //ret_grid_box_size()
  free(data_array_size);  //text_size()
  free(display_headings_size);  //text_size()
  free(data_headings_size); //text_size()
  return 0;
}