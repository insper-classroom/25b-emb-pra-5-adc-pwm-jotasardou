#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pins.h"
#include "ssd1306.h"

ssd1306_t disp;

QueueHandle_t xQueueADC;

typedef struct adc {
    int axis;
    int val;
} adc_t;

void x_task(void *p){
    // Vamos, primeiro, aplicar a leitura dos dados que vem do ADC
    adc_init();
    adc_gpio_init(28);

    int buf[5] = {0};      // buffer circular
    int idx = 0;           // posição atual para sobrescrever (0..4)
    int count = 0;         // quantas amostras já temos (até 5)
    int sum = 0;           // soma das amostras no buffer 

    while(1){
        // Por enquanto considerando o result como simplesmente o dado vindo do ADC sem conversão alguma
        uint16_t result;
        int avg;
        int filtro;
        adc_t result_data;
        

        adc_select_input(2);
        result = adc_read();

        // Config para a criação da média móvel:
        if (count < 5) {
            // ainda enchendo a janela
            buf[idx] = result;
            sum += result;
            idx = (idx + 1) % 5;
            count++;
            avg = sum / 5;
            // printf("%d\n", avg);
        }
        else {
            // janela cheia: remove o velho, coloca o novo, atualiza soma
            sum -= buf[idx];      // subtrai elemento que sai
            buf[idx] = result;    // insere novo elemento
            sum += result;        // adiciona novo à soma
            idx = (idx + 1) % 5;  // move índice circular
            avg = sum / 5;
            // printf("%d\n", avg);
        }

        // Mudando a escala da leitura analógica:
        filtro = (avg-2047)/8;
        
        if (filtro > -30 && filtro < 30){
            filtro = 0;
        }
    
        if (filtro == 255)
            filtro = 254;

        if (filtro == -255)
            filtro = -254;


        // printf("%d \n", filtro);

        result_data.axis = 0;
        result_data.val = filtro;

  

        if(filtro != 0)
            xQueueSend(xQueueADC, &result_data, 0);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void y_task(void *p){
    
    adc_init();
    adc_gpio_init(27);

    int buf[5] = {0};      
    int idx = 0;           
    int count = 0;         
    int sum = 0;           

    while(1){
        uint16_t result;
        int avg;
        int filtro;
        adc_t result_data;
        
        adc_select_input(1);
        result = adc_read();

        if (count < 5) {
            buf[idx] = result;
            sum += result;
            idx = (idx + 1) % 5;
            count++;
            avg = sum / 5;
            // printf("%d\n", avg);
        }
        else {
            sum -= buf[idx];
            buf[idx] = result;    
            sum += result;        
            idx = (idx + 1) % 5;  
            avg = sum / 5;
            // printf("%d\n", avg);
        }

        filtro = (avg-2047)/8;
        
        if (filtro > -30 && filtro < 30){
            filtro = 0;
        }

        if (filtro == 255)
            filtro = 254;

        if (filtro == -255)
            filtro = -254;

      //  printf("%d \n", filtro);

        result_data.axis = 1;
        result_data.val = filtro;
        if(filtro != 0)
            xQueueSend(xQueueADC, &result_data, 0);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void uart_task(void *p){
    while(1){
        //enviar os dados pela uart no formato: 00000000 01001101 00000011 11111111 (845 enviado pelo x)
        // aqui eu preciso definir se ve
        uint8_t AXIS;
        adc_t dado;

        if (xQueueReceive(xQueueADC, &dado, portMAX_DELAY)){
            
            // Setando o fim do envio (eop = -1)
            uint8_t EOP = 0xFF;

            // Fazendo o casting para 16bits:
            int16_t val = (int16_t)dado.val;

            // Pegando os bits menos significativos
            int8_t LSB =  (int8_t) val;

            // Pegando os bits mais significativos
            int8_t MSB = (int8_t)(val >> 8);
            
            if (LSB == -1 || MSB)
                LSB = -2;
            if (MSB == -1)
                MSB = -2;

            // Mandando pacote
            uart_putc_raw(uart0, dado.axis);
            uart_putc_raw(uart0, LSB);
            uart_putc_raw(uart0, MSB);
            uart_putc_raw(uart0, EOP);


        }
    }
}

int main() {
    stdio_init_all();

    xQueueADC = xQueueCreate(32, sizeof(adc_t)); 
    xTaskCreate(x_task, "x_task", 8192, NULL, 1, NULL);
    xTaskCreate(y_task, "y_task", 8192, NULL, 1, NULL);
    xTaskCreate(uart_task, "uart_task", 8192, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true){
    }
}

// void y_task(void *p){
// //aplicar a média móvel
// }

// int main() {
//     stdio_init_all();
//     adc_init();

//     xQueueADC = xQueueCreate(32, sizeof(adc_t)); 
//     xTaskCreate(x_task, "x_task", 8192, NULL, 1, NULL);
//     xTaskCreate(y_task, "y_task", 8192, NULL, 1, NULL);
//     xTaskCreate(uart_task, "uart_task", 8192, NULL, 1, NULL);

//     vTaskStartScheduler();

//     while (true){
//     }
// }

// == funcoes de inicializacao ===
// void btn_callback(uint gpio, uint32_t events) {
//     if (events & GPIO_IRQ_EDGE_FALL) xQueueSendFromISR(xQueueBtn, &gpio, 0);
// }

// void oled_display_init(void) {
//     i2c_init(i2c1, 400000);
//     gpio_set_function(2, GPIO_FUNC_I2C);
//     gpio_set_function(3, GPIO_FUNC_I2C);
//     gpio_pull_up(2);
//     gpio_pull_up(3);

//     disp.external_vcc = false;
//     ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
//     ssd1306_clear(&disp);
//     ssd1306_show(&disp);
// }

// void btns_init(void) {
//     gpio_init(BTN_PIN_R);
//     gpio_set_dir(BTN_PIN_R, GPIO_IN);
//     gpio_pull_up(BTN_PIN_R);

//     gpio_init(BTN_PIN_G);
//     gpio_set_dir(BTN_PIN_G, GPIO_IN);
//     gpio_pull_up(BTN_PIN_G);

//     gpio_init(BTN_PIN_B);
//     gpio_set_dir(BTN_PIN_B, GPIO_IN);
//     gpio_pull_up(BTN_PIN_B);

//     gpio_set_irq_enabled_with_callback(BTN_PIN_R,
//                                        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
//                                        true, &btn_callback);
//     gpio_set_irq_enabled(BTN_PIN_G, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
//                          true);
//     gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
//                          true);
// }

// void led_rgb_init(void) {
//     gpio_init(LED_PIN_R);
//     gpio_set_dir(LED_PIN_R, GPIO_OUT);
//     gpio_put(LED_PIN_R, 1);

//     gpio_init(LED_PIN_G);
//     gpio_set_dir(LED_PIN_G, GPIO_OUT);
//     gpio_put(LED_PIN_G, 1);

//     gpio_init(LED_PIN_B);
//     gpio_set_dir(LED_PIN_B, GPIO_OUT);
//     gpio_put(LED_PIN_B, 1);
// }

// void task_1(void *p) {
//     btns_init();
//     led_rgb_init();
//     oled_display_init();

//     uint btn_data;

//     while (1) {
//         if (xQueueReceive(xQueueBtn, &btn_data, pdMS_TO_TICKS(2000))) {
//             printf("btn: %d \n", btn_data);

//             switch (btn_data) {
//                 case BTN_PIN_B:
//                     gpio_put(LED_PIN_B, 0);
//                     ssd1306_draw_string(&disp, 8, 0, 2, "BLUE");
//                     ssd1306_show(&disp);
//                     break;
//                 case BTN_PIN_G:
//                     gpio_put(LED_PIN_G, 0);
//                     ssd1306_draw_string(&disp, 8, 24, 2, "GREEN");
//                     ssd1306_show(&disp);
//                     break;
//                 case BTN_PIN_R:
//                     gpio_put(LED_PIN_R, 0);

//                     ssd1306_draw_string(&disp, 8, 48, 2, "RED");
//                     ssd1306_show(&disp);
//                     break;
//                 default:
//                     // Handle other buttons if needed
//                     break;
//             }
//         } else {
//             ssd1306_clear(&disp);
//             ssd1306_show(&disp);
//             gpio_put(LED_PIN_R, 1);
//             gpio_put(LED_PIN_G, 1);
//             gpio_put(LED_PIN_B, 1);
//         }
//     }
// }

// int main() {
//     stdio_init_all();

//     xQueueBtn = xQueueCreate(32, sizeof(uint));
//     xTaskCreate(task_1, "Task 1", 8192, NULL, 1, NULL);

//     vTaskStartScheduler();

//     while (true);
// }
