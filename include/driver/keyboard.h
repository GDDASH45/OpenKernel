#ifndef KEYBOARD_H
#define KEYBOARD_H

int init_keyboard(void);
void keyboard_irq_handler(void);
int keyboard_try_get_char(char *character);
char keyboard_wait_and_get_char(void);

#endif