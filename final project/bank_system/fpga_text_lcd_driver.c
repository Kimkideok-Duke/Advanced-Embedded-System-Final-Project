#include "../include/fpga_driver.h"

static int text_lcd_port_usage = 0;
static unsigned char *iom_fpga_text_lcd_addr;     // 가상 주소를 저장할 변수

// prototypes
static ssize_t iom_text_lcd_write(struct file *file, const char *buf, size_t count, loff_t *f_pos);
static int iom_text_lcd_open(struct inode *inode, struct file *file);
static int iom_text_lcd_release(struct inode *inode, struct file *file);
struct file_operations iom_text_lcd_fops = {
	.owner	= 	THIS_MODULE,
	.open	=	iom_text_lcd_open,
	.write	=	iom_text_lcd_write,
	.release   =	iom_text_lcd_release
};

static int iom_text_lcd_open(struct inode *inode, struct file *file) {
	if (text_lcd_port_usage)
		return -EBUSY; 

	text_lcd_port_usage = 1;
	return 0;
}

static int iom_text_lcd_release(struct inode *inode, struct file *file) {
	text_lcd_port_usage = 0;
	return 0;
}

static ssize_t iom_text_lcd_write(struct file *file, const char *buf, size_t count, loff_t *f_pos) {
	unsigned char value[IOM_TEXT_LCD_MAX_BUF+1];
	unsigned short _s_value;
	int i;

	if (count > IOM_TEXT_LCD_MAX_BUF) count = IOM_TEXT_LCD_MAX_BUF;
	if (copy_from_user(value, buf, count)) return -EFAULT;

	value[count] = 0;
	printk("Text LCD driver: write(), length=%d / string=%s\n", count, value);
	for (i = 0; i < count; i += 2) {
		_s_value = (value[i] & 0xFF) << 8 | (value[i+1] & 0xFF);
		outw(_s_value, (unsigned int)iom_fpga_text_lcd_addr + i);
	}
	return count;
}

// __init : <linux/init.h>, this function is called for OS initialization only
int __init iom_text_lcd_init(void) {
	int result;
	result = register_chrdev(IOM_TEXT_LCD_MAJOR, IOM_TEXT_LCD_NAME, &iom_text_lcd_fops);

	if (result < 0) {
		printk(KERN_WARNING "Can't get any major number\n");
		return result;
	}
	
	// 물리 주소를 가상 주소에 mapping한다.
	iom_fpga_text_lcd_addr = ioremap(IOM_TEXT_LCD_ADDRESS, 0x20);
	printk("init module %s, major number: %d\n", IOM_TEXT_LCD_NAME, IOM_TEXT_LCD_MAJOR);
	return 0;
}



void __exit iom_text_lcd_exit(void) {
	iounmap(iom_fpga_text_lcd_addr);
	unregister_chrdev(IOM_TEXT_LCD_MAJOR, IOM_TEXT_LCD_NAME);
}

module_init(iom_text_lcd_init);
module_exit(iom_text_lcd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your name");
MODULE_DESCRIPTION("FPGA TEXT_LCD driver");

