#ifndef DISPLAY_H_
#define DISPLAY_H_

esp_err_t display_init(void);
bool display_active(void);
void display_clear();
void display_show_status(const char *messages[], size_t message_count);
void display_show_logo();
void display_show_graph(const double hashrate[], int array_length, int count, int index);

#endif /* DISPLAY_H_ */