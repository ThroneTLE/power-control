#ifndef LCD_TASK_H
#define LCD_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

void start_lcd_task(void *argument);
void lcd_task_entry(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* LCD_TASK_H */
