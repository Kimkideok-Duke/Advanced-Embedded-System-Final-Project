#!/bin/bash
insmod fpga_fnd_driver.ko
mknod /dev/fpga_fnd c 261 0
insmod fpga_text_lcd_driver.ko
mknod /dev/fpga_text_lcd c 263 0
insmod fpga_push_switch_driver.ko
mknod /dev/fpga_push_switch c 265 0
insmod fpga_dip_switch_driver.ko
mknod /dev/fpga_dip_switch c 266 0
insmod fpga_step_motor_driver.ko
mknod /dev/fpga_step_motor c 267 0
insmod fpga_buzzer_driver.ko
mknod /dev/fpga_buzzer c 264 0
insmod fpga_led_driver.ko
mknod /dev/fpga_led c 260 0
insmod fpga_dot_driver.ko
mknod /dev/fpga_dot c 262 0