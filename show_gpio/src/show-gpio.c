#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "show-gpio.h"
#include "mmio.h"
#include "utils.h"

#define XSTR(s)	STR(s)
#define STR(s)	#s

#define	GPIO_OE			0x0000
#define GPIO_IN			0x0004
#define GPIO_OUT		0x0008
#define GPIO_SET		0x000C
#define GPIO_CLEAR		0x0010
#define GPIO_INT GPIO		0x0014
#define GPIO_INT_TYPE		0x0018 
#define GPIO_INT_POLARITY	0x001C
#define GPIO_INT_PENDING	0x0020
#define GPIO_INT_MASK		0x0024
#define GPIO_IN_ETH_SWITCH_LED	0x0028
#define GPIO_OUT_FUNCTION0	0x002C
#define GPIO_OUT_FUNCTION1	0x0030
#define GPIO_OUT_FUNCTION2	0x0034
#define GPIO_OUT_FUNCTION3	0x0038
#define GPIO_OUT_FUNCTION4	0x003C
#define GPIO_IN_ENABLE0		0x0044
#define GPIO_IN_ENABLE1		0x0048
#define GPIO_IN_ENABLE2		0x004C
#define GPIO_IN_ENABLE3		0x0050
#define GPIO_IN_ENABLE4		0x0054
#define GPIO_IN_ENABLE9		0x0068
#define GPIO_FUNCTION		0x006C

#define RES01			 1
#define RES02			 2
#define RES03			 3
#define SLIC_DATA_OUT		 4	
#define SLIC_PCM_FS SLIC	 5
#define SLIC_PCM_CLK		 6
#define SPI_CS_1		 7
#define SPI_CS_2		 8
#define SPI_CS_0		 9
#define SPI_CLK			10
#define SPI_MOSI		11
#define I2S_CLK			12
#define I2S_WS			13
#define I2S_SD			14
#define I2S_MCK			15
#define CLK_OBS0		16
#define CLK_OBS1		17
#define CLK_OBS2		18
#define CLK_OBS3		19
#define CLK_OBS04		20
#define CLK_OBS5		21
#define CLK_OBS6		22
#define CLK_OBS7		23
#define UART0_SOUT		24
#define SPDIF_OUT		25
#define LED_ACTN_0		26
#define LED_ACTN_1		27
#define LED_ACTN_2		28
#define LED_ACTN_3		29
#define LED_ACTN_4		30
#define LED_COLN_0		31
#define LED_COLN_1		32
#define LED_COLN_2		33
#define LED_COLN_3		34
#define LED_COLN_4		35
#define LED_DUPLEXN_0		36
#define LED_DUPLEXN_1		37
#define LED_DUPLEXN_2		38
#define LED_DUPLEXN_3		39
#define LED_DUPLEXN_4		40
#define LED_LINK_0		41
#define LED_LINK_1		42
#define LED_LINK_2		43
#define LED_LINK_3		44
#define LED_LINK_4		45
#define ATT_LED			46
#define PWR_LED			47
#define TX_FRAME		48
#define RX_CLEAR_EXTERNAL	49
#define LED_NETWORK_EN		50
#define LED_POWER_EN		51
#define RES_52			52
#define RES_71			71
#define WMAC_GLUE_WOW		72	
#define BT_ANT			73
#define RX_CLEAR_EXTENSION	74
#define RES_75			75
#define RES_76			76
#define RES_77			77
#define ETH_TX			78
#define UART1_TD		79
#define UART1_RTS		80
#define RES_81			81
#define RES_82			82
#define RES_83			83
#define DDR_DQ_OE		84
#define RES_85			85
#define RES_86			86
#define USB_SUSPEND		87
#define RES_88			88
#define RES_91			91

#define GPIO_MAX		22

int main(int argc, char **argv) {
    uint32_t gpio, gpio_output_function, gpio_output_function_reg;
    uint32_t index, gpio_input_function_reg, gpio_input_pin;
    uint32_t gpio_data[32];
    struct mmio io;
    char gpio_direction;
    char *gpio_input[GPIO_MAX];

    bzero(gpio_input, sizeof(gpio_input));
    if (mmio_map(&io, 0x18040000, 0x80))
        die_errno("mmio_map() failed");

    for (index=0; index<0x20; index++) {
        gpio_data[index]=mmio_readl(&io, index<<2);
    }
    mmio_unmap(&io);

    for (index=0; index<20; index++) {
        gpio_input_function_reg = gpio_data[(GPIO_IN_ENABLE0 + index)>>2];
        gpio_input_pin = (gpio_input_function_reg >> ((index & 0x03) << 3)) & 0xff ;
        if (gpio_input_pin) {
            gpio_input[gpio_input_pin]=gpio_input_function_description[index];
	}
    }    
        
    for (gpio=0; gpio <= GPIO_MAX; gpio++) {
	if (gpio_data[GPIO_OE] & (1<<gpio)) {
            gpio_direction = 'I';
	} else {
            gpio_direction = 'O';
        }
        
	if ((gpio <= 3 ) & !(gpio_data[GPIO_FUNCTION>>2] && 1<<1)) {
	    if (gpio == 0 ) printf("GPIO %02d %c TCK \n", gpio, gpio_direction);
	    if (gpio == 1 ) printf("GPIO %02d %c TDI \n", gpio, gpio_direction);
	    if (gpio == 2 ) printf("GPIO %02d %c TDO \n", gpio, gpio_direction);
	    if (gpio == 3 ) printf("GPIO %02d %c TMS \n", gpio, gpio_direction);
	} else {
            switch(gpio_direction) {
		case 'O':
                    gpio_output_function_reg = gpio_data[(gpio+GPIO_OUT_FUNCTION0)>>2];
                    gpio_output_function = ((gpio_output_function_reg >> ((gpio & 0x03) << 3) & 0xff )) ;
                    
                    printf("%12s GPIO %02d %c %s\n", wr_841n_v8[gpio], gpio, gpio_direction, gpio_output_function_description[gpio_output_function] );
                    
                    break;
                case 'I':
                    if (gpio_input[gpio]) {
                        printf("%12s GPIO %02d %c %s\n", wr_841n_v8[gpio], gpio, gpio_direction, gpio_input[gpio]);
                    } else {
                        printf("%12s GPIO %02d %c GPIO\n", wr_841n_v8[gpio], gpio, gpio_direction);
                    } 
                    break;
            }
	}
    }
    return 0;
}