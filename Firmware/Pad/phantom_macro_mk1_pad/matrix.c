#include "matrix.h"
#include "quantum.h"
#include "hal.h"
#include "wait.h"

//this stub so firmware can compile, too be fixes when i get the board
int16_t analogReadPin(pin_t pin){
    return 2048;
}

//rotary encoder
#define ENCODER_SW_PORT GPIOB
#define ENCODER_SW_PIN 11

//left mux
#define L_MUX_S0_PORT GPIOB
#define L_MUX_S0_PIN 9
#define L_MUX_S1_PORT GPIOB
#define L_MUX_S1_PIN 6
#define L_MUX_S2_PORT GPIOA
#define L_MUX_S2_PIN 6
#define L_MUX_S3_PORT GPIOA
#define L_MUX_S3_PIN 7
#define L_MUX_OUT_PIN A0

//right mux
#define R_MUX_S0_PORT GPIOB
#define R_MUX_S0_PIN 12
#define R_MUX_S1_PORT GPIOB
#define R_MUX_S1_PIN 13
#define R_MUX_S2_PORT GPIOB
#define R_MUX_S2_PIN 14
#define R_MUX_S3_PORT GPIOB
#define R_MUX_S3_PIN 15
#define R_MUX_OUT_PIN A4

//analog
#define HE_ACTUATION_HIGH 2600 //south
#define HE_ACTUATION_LOW 1500 //north
#define HE_RELEASE_HIGH 2400
#define HE_RELEASE_LOW 1700


//matrix mapping
matrix_row_t matrix[MATRIX_ROWS];

const uint8_t he_matrix_map[16][2] = {

    //left mux channel
    {1, 1}, //I8->SW_5
    {2, 1}, //I9->SW_9
    {3, 1}, //I10->SW_12
    {4, 0}, //I11->SW_15
    {0, 0}, //I12->SW_1
    {1, 0}, //I13->SW_4
    {2, 0}, //I14->SW_8
    {3, 0}, //I15->SW_11

    //right mux channel
    {1, 3}, //I0->SW_7
    {3, 3}, //I1->SW_14
    {0, 2}, //I2->SW_3
    {1, 2}, //I3->SW_6
    {2, 2}, //I4->SW_10
    {3, 2}, //I5->SW_13
    {4, 2}, //I6->SW_16
    {0, 1}  //I7->SW_2
};


// initialise mux
void matrix_init(void){
    palSetPadMode(ENCODER_SW_PORT, ENCODER_SW_PIN, PAL_MODE_INPUT_PULLUP);

    //left mux control pins
    palSetPadMode(L_MUX_S0_PORT, L_MUX_S0_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(L_MUX_S1_PORT, L_MUX_S1_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(L_MUX_S2_PORT, L_MUX_S2_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(L_MUX_S3_PORT, L_MUX_S3_PIN, PAL_MODE_OUTPUT_PUSHPULL);

    //right mux control pins
    palSetPadMode(R_MUX_S0_PORT, R_MUX_S0_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(R_MUX_S1_PORT, R_MUX_S1_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(R_MUX_S2_PORT, R_MUX_S2_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(R_MUX_S3_PORT, R_MUX_S3_PIN, PAL_MODE_OUTPUT_PUSHPULL);

    //disable left uart on boot
    palSetPadMode(GPIOA, 5, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(GPIOB, 5, PAL_MODE_OUTPUT_PUSHPULL);
    palWritePad(GPIOA, 5, PAL_LOW);
    palWritePad(GPIOB, 5, PAL_HIGH);
    //disable right uart on boot
    palSetPadMode(GPIOA, 15, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPadMode(GPIOA, 13, PAL_MODE_OUTPUT_PUSHPULL);
    palWritePad(GPIOA, 15, PAL_LOW);
    palWritePad(GPIOA, 13, PAL_HIGH);

    //pogo detect pins
    palSetPadMode(GPIOC, 13, PAL_MODE_INPUT_PULLDOWN); //left
    palSetPadMode(GPIOA, 8, PAL_MODE_INPUT_PULLDOWN); //right

    //zero out
    for (uint8_t i = 0; i < MATRIX_ROWS; i++){
        matrix[i] = 0;
    }

}




// matrix scan loop
uint8_t matrix_scan(void){
    bool changed = false;

    //scan both mux same time
    for (uint8_t i = 0; i < 8; i++){

        //left
        palWritePad(L_MUX_S0_PORT, L_MUX_S0_PIN, (i & 1));
        palWritePad(L_MUX_S1_PORT, L_MUX_S1_PIN, (i & 2));
        palWritePad(L_MUX_S2_PORT, L_MUX_S2_PIN, (i & 4));
        palWritePad(L_MUX_S3_PORT, L_MUX_S3_PIN, PAL_HIGH);

        //right
        palWritePad(R_MUX_S0_PORT, R_MUX_S0_PIN, (i & 1));
        palWritePad(R_MUX_S1_PORT, R_MUX_S1_PIN, (i & 2));
        palWritePad(R_MUX_S2_PORT, R_MUX_S2_PIN, (i & 4));
        palWritePad(R_MUX_S3_PORT, R_MUX_S3_PIN, PAL_LOW);

        wait_us(2);

        //read adc
        uint16_t left_val = analogReadPin(L_MUX_OUT_PIN);
        uint16_t right_val = analogReadPin(R_MUX_OUT_PIN);

        //process left
        uint8_t l_row = he_matrix_map[i][0];
        uint8_t l_col = he_matrix_map[i][1];
        if (left_val > HE_ACTUATION_HIGH || left_val < HE_ACTUATION_LOW){
            if (!(matrix[l_row] & (1 << l_col))){
                matrix[l_row] |= (1 << l_col);
                changed = true;
            }
        } else if (left_val < HE_RELEASE_HIGH && left_val > HE_RELEASE_LOW){
            if (matrix[l_row] & (1 << l_col)){
                matrix[l_row] &= ~(1 << l_col);
                changed = true;
            }
        }

        //process right
        uint8_t r_row = he_matrix_map[i + 8][0];
        uint8_t r_col = he_matrix_map[i + 8][1];
        if (right_val > HE_ACTUATION_HIGH || right_val < HE_ACTUATION_LOW){
            if (!(matrix[r_row] & (1 << r_col))){
                matrix[r_row] |= (1 << r_col);
                changed = true;
            }
        } else if (right_val < HE_RELEASE_HIGH && right_val > HE_RELEASE_LOW){
            if (matrix[r_row] & (1 << r_col)){
                matrix[r_row] &= ~(1 << r_col);
                changed = true;
            }
        }
    }


    //process encoder button
    if (palReadPad(ENCODER_SW_PORT, ENCODER_SW_PIN) == PAL_LOW){
        if (!(matrix[0]&(1<<3))){
            matrix[0] |= (1<<3);
            changed = true;
        }
    } else {
        if (matrix[0]&(1<<3)){
            matrix[0]&= ~(1<<3);
            changed = true;
        }
    }
    return changed ? 1:0;
}








//custom matrix function
matrix_row_t matrix_get_row(uint8_t row){
    return matrix[row];
}

void matrix_print(void){
    //required for CUSTOM_MATRIX
    //left empty unless debug
}

//hardware adc stub
//stm32g4 dual adc need mcuconf.h ovveride

